package devices

import (
	"MINIMTS/services/devices/hik"
	"fmt"
	"image"
	"image/color"
	"math"
	"sync"
	"time"

	"github.com/wailsapp/wails/v3/pkg/application"
	"gocv.io/x/gocv"
)

// ExtensometerResult 引伸计计算结果：两个 ROI 的中心坐标与欧几里得距离（像素）
type ExtensometerResult struct {
	AX float64 `json:"ax"`
	AY float64 `json:"ay"`
	BX float64 `json:"bx"`
	BY float64 `json:"by"`

	Distance float64 `json:"distance"`
}

// HIKExtensometerService 负责视频引伸计的 ROI 绘制与方向线绘制。
//
// 它作为相机服务的叠加钩子运行：每帧对 ROI A/B 执行 DIC 跟踪，
// 调整方向线角度，计算两个 ROI 中心点的欧几里得距离，并通过
// hik_extensometer_result 事件把 A/B 坐标与距离发射给 MINIMTSService。
// 应变由 MINIMTSService 根据距离变化计算。
type HIKExtensometerService struct {
	camera *HIKCameraService

	// ── ROI 标记 ──
	roiMu sync.Mutex
	roiA  *hik.ExtensometerService
	roiB  *hik.ExtensometerService

	// ── 方向线 ──
	directionLine   hik.LineDirection
	directionLineMu sync.Mutex

	// ── 方向线自适应追踪状态 ──
	dirLineReady bool    // 初始垂距是否已计算
	perpDistA    float64 // ROI A 中心到方向线的初始垂距
	perpDistB    float64 // ROI B 中心到方向线的初始垂距

	// ── 比例标定 ──
	resolutionRatio float64
	resolutionMu    sync.RWMutex

	// ── DIC 预处理参数（系统设置实时修改） ──
	dicParamMu sync.Mutex
	dicMethod  string  // none/gauss/bilateral/nlm/highpass
	dicSharpen float64 // 锐化强度

	// ── 噪声底测量 ──
	noiseMu         sync.Mutex
	noiseCollecting bool
	noiseSamples    []float64
	noiseTarget     int
	noiseDone       chan struct{}
}

// NoiseFloorResult 噪声底测量结果
type NoiseFloorResult struct {
	Count int     `json:"count"` // 有效样本数
	Mean  float64 `json:"mean"`  // 距离均值（像素）
	Std   float64 `json:"std"`   // 距离标准差（像素）——噪声底
	Min   float64 `json:"min"`   // 最小值（像素）
	Max   float64 `json:"max"`   // 最大值（像素）
	Ratio float64 `json:"ratio"` // 比例标定系数（>0 时 Std/Ratio 为毫米噪声）
}

// NewHIKExtensometer 创建引伸计服务，并把它注册为相机服务的跟踪与绘制钩子
func NewHIKExtensometer(camera *HIKCameraService) *HIKExtensometerService {
	service := &HIKExtensometerService{camera: camera}
	camera.setTrackHook(service.processFrame)
	camera.setDrawHook(service.drawOverlay)
	return service
}

// ────────────── 选择器窗口 ──────────────

// OpenROISelector 打开 ROI 选择器窗口
// 输入：ROI 标记（A 或 B）
func (s *HIKExtensometerService) OpenROISelector(label string) error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}
	if err := s.camera.ensureLatestFrame(); err != nil {
		return err
	}

	app.Window.NewWithOptions(application.WebviewWindowOptions{
		Title:            "选择标记 " + label,
		Width:            1200,
		Height:           850,
		BackgroundColour: application.NewRGB(15, 23, 42),
		URL:              "/#/calibration?label=" + label,
	})
	return nil
}

// OpenDirectionSelector 打开拉伸方向线选择器窗口
func (s *HIKExtensometerService) OpenDirectionSelector() error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}
	if err := s.camera.ensureLatestFrame(); err != nil {
		return err
	}

	app.Window.NewWithOptions(application.WebviewWindowOptions{
		Title:            "绘制方向",
		Width:            1200,
		Height:           850,
		BackgroundColour: application.NewRGB(15, 23, 42),
		URL:              "/#/calibration?mode=line",
	})
	return nil
}

// OpenCalibrationSelector 打开比例标定选择器窗口
func (s *HIKExtensometerService) OpenCalibrationSelector() error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}
	if err := s.camera.ensureLatestFrame(); err != nil {
		return err
	}

	app.Window.NewWithOptions(application.WebviewWindowOptions{
		Title:            "比例标定",
		Width:            1200,
		Height:           850,
		BackgroundColour: application.NewRGB(15, 23, 42),
		URL:              "/#/calibration?mode=calibration",
	})
	return nil
}

// ────────────── ROI 设置 ──────────────

// SetROI 设置指定标记（A 或 B）的感兴趣区域
func (s *HIKExtensometerService) SetROI(label string, rect hik.ROIRect) error {
	if rect.Width <= 0 || rect.Height <= 0 {
		return fmt.Errorf("ROI 区域无效")
	}

	latestFrame, _, _ := s.camera.latestDisplayFrameSnapshot()
	if len(latestFrame) == 0 {
		return fmt.Errorf("暂无相机图像")
	}

	mat, err := gocv.IMDecode(latestFrame, gocv.IMReadColor)
	if err != nil || mat.Empty() {
		return fmt.Errorf("解析相机图像失败")
	}
	defer closeMatQuietly(&mat)

	tracker := hik.NewExtensometerService()
	s.applyDICParamsToTracker(tracker) // 模板提取前先套用当前预处理方法
	tracker.SetOrignalImg(mat)
	tracker.SetROI(hik.Rect2f{
		X:      rect.X,
		Y:      rect.Y,
		Width:  rect.Width,
		Height: rect.Height,
	})

	s.roiMu.Lock()
	defer s.roiMu.Unlock()
	switch label {
	case "A":
		s.roiA = tracker
	case "B":
		s.roiB = tracker
	default:
		return fmt.Errorf("未知 ROI 标记: %s", label)
	}

	s.updateTrackerAxesLocked()

	// ROI 变更后，方向线的初始状态需要重新计算
	s.resetDirectionLineTracking()

	if app := application.Get(); app != nil {
		app.Event.Emit("hik_roi_selected", map[string]any{
			"label": label,
			"roi":   rect,
		})
	}
	return nil
}

// ────────────── 方向线 ──────────────

// SetDirectionLineFromConfig 从配置文件设置方向线段
func (s *HIKExtensometerService) SetDirectionLineFromConfig(line hik.LineDirection) {
	s.directionLineMu.Lock()
	s.directionLine = line
	s.dirLineReady = false // 需要重新计算初始垂距
	s.directionLineMu.Unlock()
}

// SetDirectionLine 设置方向线段并发射选择事件
func (s *HIKExtensometerService) SetDirectionLine(line hik.LineDirection) error {
	dx := line.X2 - line.X1
	dy := line.Y2 - line.Y1
	if math.Sqrt(dx*dx+dy*dy) < 5 {
		return fmt.Errorf("直线太短，请重试")
	}

	s.directionLineMu.Lock()
	s.directionLine = line
	s.resetDirectionLineTrackingLocked()
	s.directionLineMu.Unlock()

	if app := application.Get(); app != nil {
		app.Event.Emit("hik_direction_selected", map[string]any{
			"line": line,
		})
	}
	return nil
}

// GetDirectionLine 获取当前方向线段，长度不足时返回 false
func (s *HIKExtensometerService) GetDirectionLine() (hik.LineDirection, bool) {
	s.directionLineMu.Lock()
	defer s.directionLineMu.Unlock()

	dx := s.directionLine.X2 - s.directionLine.X1
	dy := s.directionLine.Y2 - s.directionLine.Y1
	if math.Sqrt(dx*dx+dy*dy) < 5 {
		return hik.LineDirection{}, false
	}
	return s.directionLine, true
}

// ────────────── 比例标定 ──────────────

// SetResolutionRatio 设置比例标定系数（像素长度 / 实际长度）
func (s *HIKExtensometerService) SetResolutionRatio(pixelLength, realLength float64) error {
	if pixelLength <= 0 || realLength <= 0 {
		return fmt.Errorf("像素长度和实际距离必须大于0")
	}
	s.resolutionMu.Lock()
	s.resolutionRatio = pixelLength / realLength
	s.resolutionMu.Unlock()
	return nil
}

// GetResolutionRatio 获取比例标定系数
func (s *HIKExtensometerService) GetResolutionRatio() float64 {
	s.resolutionMu.RLock()
	defer s.resolutionMu.RUnlock()
	return s.resolutionRatio
}

// ────────────── 结果计算与信号发射 ──────────────

// ComputeROIResult 计算两个 ROI 的中心坐标与欧几里得距离
func (s *HIKExtensometerService) ComputeROIResult() (*ExtensometerResult, error) {
	s.roiMu.Lock()
	roiA := s.roiA
	roiB := s.roiB
	s.roiMu.Unlock()

	if roiA == nil || roiB == nil {
		return nil, fmt.Errorf("请先完成 ROI A 和 ROI B 的绘制")
	}

	ra := roiA.CurrentROI()
	rb := roiB.CurrentROI()

	result := &ExtensometerResult{
		AX: ra.X + ra.Width/2,
		AY: ra.Y + ra.Height/2,
		BX: rb.X + rb.Width/2,
		BY: rb.Y + rb.Height/2,
	}
	result.Distance = math.Hypot(result.BX-result.AX, result.BY-result.AY)

	return result, nil
}

// emitROIResult 计算并发射引伸计结果信号
func (s *HIKExtensometerService) emitROIResult() {
	result, err := s.ComputeROIResult()
	if err != nil {
		return
	}
	s.collectNoiseSample(result.Distance)
	if app := application.Get(); app != nil {
		app.Event.Emit("hik_extensometer_result", *result)
	}
}

// ────────────── DIC 预处理参数 ──────────────

// SetDICPreprocess 设置 DIC 预处理方法与锐化强度，立即作用于后续所有帧。
// method: none/gauss/bilateral/nlm/highpass
// 注意：修改方法后建议重新框选 ROI，使参考模板与实时帧的预处理保持一致。
func (s *HIKExtensometerService) SetDICPreprocess(method string, sharpen float64) error {
	valid := map[string]bool{
		hik.DICPreprocessNone:      true,
		hik.DICPreprocessGauss:     true,
		hik.DICPreprocessBilateral: true,
		hik.DICPreprocessNLM:       true,
		hik.DICPreprocessHighPass:  true,
	}
	if !valid[method] {
		return fmt.Errorf("未知预处理方法: %s", method)
	}
	if sharpen < 0 {
		sharpen = 0
	}

	s.dicParamMu.Lock()
	s.dicMethod = method
	s.dicSharpen = sharpen
	s.dicParamMu.Unlock()

	// 已存在的 tracker 立即生效
	s.roiMu.Lock()
	trackers := []*hik.ExtensometerService{s.roiA, s.roiB}
	s.roiMu.Unlock()
	for _, t := range trackers {
		if t != nil {
			s.applyDICParamsToTracker(t)
		}
	}

	if app := application.Get(); app != nil {
		app.Event.Emit("hik_dic_preprocess_changed", map[string]any{
			"method":  method,
			"sharpen": sharpen,
		})
	}
	return nil
}

// GetDICPreprocess 获取当前预处理设置
func (s *HIKExtensometerService) GetDICPreprocess() map[string]any {
	s.dicParamMu.Lock()
	defer s.dicParamMu.Unlock()
	return map[string]any{
		"method":  s.dicMethod,
		"sharpen": s.dicSharpen,
	}
}

// applyDICParamsToTracker 将当前预处理设置套用到 tracker（其余参数保持默认）
func (s *HIKExtensometerService) applyDICParamsToTracker(tracker *hik.ExtensometerService) {
	s.dicParamMu.Lock()
	method, sharpen := s.dicMethod, s.dicSharpen
	s.dicParamMu.Unlock()

	param := hik.DefaultDICParam()
	param.PreprocessMethod = method
	param.SharpenAmount = sharpen
	tracker.SetDICParam(param)
}

// ────────────── 噪声底测量 ──────────────

// MeasureNoiseFloor 噪声底测量：静止状态下连续采集指定帧数的 ROI 距离值，
// 输出均值/标准差/极值。标准差即位移噪声底，用于评估预处理方法的效果。
// 同步阻塞直至采满或超时（60 秒）。
func (s *HIKExtensometerService) MeasureNoiseFloor(sampleCount int) (*NoiseFloorResult, error) {
	if sampleCount < 50 {
		sampleCount = 50
	}
	if sampleCount > 2000 {
		sampleCount = 2000
	}

	s.noiseMu.Lock()
	if s.noiseCollecting {
		s.noiseMu.Unlock()
		return nil, fmt.Errorf("测量进行中，请稍候")
	}
	s.noiseCollecting = true
	s.noiseSamples = make([]float64, 0, sampleCount)
	s.noiseTarget = sampleCount
	done := make(chan struct{})
	s.noiseDone = done
	s.noiseMu.Unlock()

	defer func() {
		s.noiseMu.Lock()
		s.noiseCollecting = false
		s.noiseDone = nil
		s.noiseMu.Unlock()
	}()

	select {
	case <-done:
	case <-time.After(60 * time.Second):
		return nil, fmt.Errorf("测量超时，请确认相机在采集且 ROI A/B 已框选")
	}

	s.noiseMu.Lock()
	samples := s.noiseSamples
	s.noiseSamples = nil
	s.noiseMu.Unlock()
	if len(samples) == 0 {
		return nil, fmt.Errorf("未采集到数据")
	}

	var sum float64
	min, max := samples[0], samples[0]
	for _, v := range samples {
		sum += v
		if v < min {
			min = v
		}
		if v > max {
			max = v
		}
	}
	mean := sum / float64(len(samples))

	var varSum float64
	for _, v := range samples {
		d := v - mean
		varSum += d * d
	}
	std := math.Sqrt(varSum / float64(len(samples)))

	return &NoiseFloorResult{
		Count: len(samples),
		Mean:  mean,
		Std:   std,
		Min:   min,
		Max:   max,
		Ratio: s.GetResolutionRatio(),
	}, nil
}

// collectNoiseSample 噪声底采样（每帧结果发射时调用）
func (s *HIKExtensometerService) collectNoiseSample(distance float64) {
	s.noiseMu.Lock()
	defer s.noiseMu.Unlock()
	if !s.noiseCollecting || s.noiseDone == nil {
		return
	}
	s.noiseSamples = append(s.noiseSamples, distance)
	if len(s.noiseSamples) >= s.noiseTarget {
		close(s.noiseDone)
		s.noiseDone = nil
	}
}

// ────────────── 相机叠加钩子 ──────────────

// rect2fToImageRect 浮点 ROI 转整型图像区域（向外取整保证覆盖）
func rect2fToImageRect(r hik.Rect2f) image.Rectangle {
	x0 := int(math.Floor(r.X))
	y0 := int(math.Floor(r.Y))
	x1 := int(math.Ceil(r.X + r.Width))
	y1 := int(math.Ceil(r.Y + r.Height))
	return image.Rect(x0, y0, x1, y1)
}

// processFrame 每帧执行 ROI 跟踪、方向线调整与结果发射（快路径，不含绘制编码）
func (s *HIKExtensometerService) processFrame(frame *gocv.Mat) {
	s.roiMu.Lock()
	hasROI := s.roiA != nil || s.roiB != nil
	s.roiMu.Unlock()
	if !hasROI {
		return
	}

	s.updateTrackedROIs(*frame)

	s.adjustDirectionLine()
	s.emitROIResult()
}

// drawOverlay 绘制 ROI 跟踪框与方向线（仅显示编码协程调用，不占用采集回调线程）
func (s *HIKExtensometerService) drawOverlay(frame *gocv.Mat) {
	s.roiMu.Lock()
	hasROI := s.roiA != nil || s.roiB != nil
	s.roiMu.Unlock()
	if !hasROI {
		return
	}

	s.drawTrackedROIs(frame)
}

func (s *HIKExtensometerService) updateTrackedROIs(frame gocv.Mat) {
	s.roiMu.Lock()
	roiA := s.roiA
	roiB := s.roiB
	s.roiMu.Unlock()
	if roiA == nil || roiB == nil {
		return
	}

	// 仅对两个 ROI 覆盖区域（含搜索余量）做预处理，跳过全图模糊
	regions := []image.Rectangle{
		rect2fToImageRect(roiA.ROIF),
		rect2fToImageRect(roiB.ROIF),
	}
	preparedFrame := roiA.PreprocessRegionsForDIC(frame, regions)
	defer preparedFrame.Close()

	// 全图字节只提取一次，两个 ROI 共享（省去重复全图拷贝）
	preparedBytes := preparedFrame.ToBytes()

	var wg sync.WaitGroup
	runTracker := func(tracker *hik.ExtensometerService) {
		defer wg.Done()
		_, _ = tracker.RunDICPreparedGrayMatShared(preparedFrame, preparedBytes)
	}

	wg.Add(2)
	go runTracker(roiA)
	go runTracker(roiB)
	wg.Wait()
}

func (s *HIKExtensometerService) drawTrackedROIs(frame *gocv.Mat) {
	s.roiMu.Lock()
	defer s.roiMu.Unlock()

	drawROI := func(label string, tracker *hik.ExtensometerService, c color.RGBA) {
		if tracker == nil {
			return
		}
		roi := tracker.CurrentROI()

		rect := image.Rect(int(roi.X), int(roi.Y), int(roi.X+roi.Width), int(roi.Y+roi.Height))
		_ = gocv.Rectangle(frame, rect, c, 2)
		_ = gocv.PutText(frame, label, image.Pt(rect.Max.X+6, rect.Min.Y+18), gocv.FontHersheySimplex, 0.7, c, 2)
	}

	drawROI("A", s.roiA, color.RGBA{R: 34, G: 197, B: 94, A: 255})
	drawROI("B", s.roiB, color.RGBA{R: 59, G: 130, B: 246, A: 255})

	// 绘制方向线（虚线）
	dirLine, hasDirLine := s.GetDirectionLine()
	if hasDirLine {
		drawDashedLine(frame, image.Pt(int(dirLine.X1), int(dirLine.Y1)), image.Pt(int(dirLine.X2), int(dirLine.Y2)), color.RGBA{R: 6, G: 182, B: 212, A: 255}, 2, 12)
	}
}

// ────────────── 方向线自适应追踪 ──────────────

// resetDirectionLineTracking 重置方向线的初始追踪状态，下次 adjustDirectionLine 会重新计算。
func (s *HIKExtensometerService) resetDirectionLineTracking() {
	s.directionLineMu.Lock()
	defer s.directionLineMu.Unlock()
	s.resetDirectionLineTrackingLocked()
}

// resetDirectionLineTrackingLocked 无锁版本，调用方需持有 directionLineMu
func (s *HIKExtensometerService) resetDirectionLineTrackingLocked() {
	s.dirLineReady = false
}

// adjustDirectionLine 根据 ROI A/B 的最新追踪位置，动态调整方向线角度。
//
// 核心原理：两个标记点中心到方向线的垂距在拉伸过程中应保持恒定。
// 每帧根据新的标记点位置，求解满足垂距恒定的新直线方程，
// 从而获取方向线的实时偏转角度（反映拉伸角度的变化）。
func (s *HIKExtensometerService) adjustDirectionLine() {
	s.roiMu.Lock()
	roiA := s.roiA
	roiB := s.roiB
	s.roiMu.Unlock()
	if roiA == nil || roiB == nil {
		return
	}

	s.directionLineMu.Lock()
	defer s.directionLineMu.Unlock()

	// 获取当前 ROI 中心
	ra := roiA.CurrentROI()
	rb := roiB.CurrentROI()
	ax := ra.X + ra.Width/2
	ay := ra.Y + ra.Height/2
	bx := rb.X + rb.Width/2
	by := rb.Y + rb.Height/2

	// 当前存储的方向线端点
	x1, y1 := s.directionLine.X1, s.directionLine.Y1
	x2, y2 := s.directionLine.X2, s.directionLine.Y2
	ldx := x2 - x1
	ldy := y2 - y1
	lineLen := math.Sqrt(ldx*ldx + ldy*ldy)
	if lineLen < 1 {
		return
	}

	// 方向线的单位法向量 (a,b)，直线方程 a·x + b·y + c = 0
	a0 := -ldy / lineLen
	b0 := ldx / lineLen
	c0 := -(a0*x1 + b0*y1)

	if !s.dirLineReady {
		// 首次：记录初始有符号垂距
		s.perpDistA = a0*ax + b0*ay + c0
		s.perpDistB = a0*bx + b0*by + c0
		s.dirLineReady = true
		return
	}

	// 后续帧：计算保持垂距不变的新方向线
	dx := ax - bx
	dy := ay - by
	radius := math.Sqrt(dx*dx + dy*dy)
	if radius < 1 {
		return
	}
	phi := math.Atan2(dy, dx)
	deltaDist := s.perpDistA - s.perpDistB

	ratio := deltaDist / radius
	if ratio < -1 || ratio > 1 {
		return // 标记点间距过小，无法求解，保持当前方向线
	}

	// 两个候选法向角，选与初始法向角更接近的解
	alpha := math.Acos(ratio)
	theta1 := phi + alpha
	theta2 := phi - alpha
	theta0 := math.Atan2(b0, a0)
	if math.Abs(angleDiff(theta1, theta0)) > math.Abs(angleDiff(theta2, theta0)) {
		theta1 = theta2
	}

	a := math.Cos(theta1)
	b := math.Sin(theta1)
	c := s.perpDistA - (a*ax + b*ay)

	// 将 A、B 投影到新直线上作为新端点
	projAx := ax - a*(a*ax+b*ay+c)
	projAy := ay - b*(a*ax+b*ay+c)
	projBx := bx - a*(a*bx+b*by+c)
	projBy := by - b*(a*bx+b*by+c)

	s.directionLine = hik.LineDirection{
		X1: projAx, Y1: projAy,
		X2: projBx, Y2: projBy,
	}
}

// angleDiff 计算角度差（弧度），结果归一化到 [-π, π]
func angleDiff(a, b float64) float64 {
	d := a - b
	for d > math.Pi {
		d -= 2 * math.Pi
	}
	for d < -math.Pi {
		d += 2 * math.Pi
	}
	return d
}

// updateTrackerAxesLocked 更新 ROI A/B 的追踪轴，保持垂直或水平追踪
func (s *HIKExtensometerService) updateTrackerAxesLocked() {
	if s.roiA == nil || s.roiB == nil {
		return
	}
	axis := roiPairTrackingAxis(s.roiA.CurrentROI(), s.roiB.CurrentROI())
	s.roiA.SetTrackingAxis(axis)
	s.roiB.SetTrackingAxis(axis)
}

func roiPairTrackingAxis(roiA, roiB hik.Rect2f) hik.TrackingAxis {
	centerAX := roiA.X + roiA.Width/2
	centerAY := roiA.Y + roiA.Height/2
	centerBX := roiB.X + roiB.Width/2
	centerBY := roiB.Y + roiB.Height/2
	if math.Abs(centerBX-centerAX) >= math.Abs(centerBY-centerAY) {
		return hik.TrackingAxisVertical
	}
	return hik.TrackingAxisHorizontal
}

// drawDashedLine 在图像上绘制虚线（gocv 本身不支持虚线样式，手动分段绘制）
func drawDashedLine(img *gocv.Mat, pt1, pt2 image.Point, c color.RGBA, thickness int, dashLen int) {
	dx := pt2.X - pt1.X
	dy := pt2.Y - pt1.Y
	totalLen := math.Sqrt(float64(dx*dx + dy*dy))
	if totalLen < 1 {
		return
	}
	ux := float64(dx) / totalLen
	uy := float64(dy) / totalLen

	steps := int(totalLen) / dashLen
	if steps < 1 {
		steps = 1
	}
	for i := 0; i < steps; i++ {
		if i%2 != 0 {
			continue
		}
		startX := pt1.X + int(ux*float64(i*dashLen))
		startY := pt1.Y + int(uy*float64(i*dashLen))
		endX := pt1.X + int(ux*float64((i+1)*dashLen))
		endY := pt1.Y + int(uy*float64((i+1)*dashLen))
		if endX > pt2.X && ux > 0 {
			endX = pt2.X
		}
		if endY > pt2.Y && uy > 0 {
			endY = pt2.Y
		}
		_ = gocv.Line(img, image.Pt(startX, startY), image.Pt(endX, endY), c, thickness)
	}
}
