package services

import (
	"MINIMTS/services/hik"
	"encoding/base64"
	"fmt"
	"image"
	"image/color"
	"math"
	"sync"
	"time"

	"github.com/wailsapp/wails/v3/pkg/application"
	"github.com/wailsapp/wails/v3/pkg/events"
	"gocv.io/x/gocv"
)

type HIKCameraService struct {
	// ── 相机设备与窗口 ──
	camera *hik.Camera
	window *application.WebviewWindow

	// ── 采集生命周期 ──
	stopCh   chan struct{}
	stopOnce sync.Once

	// ── 图像帧缓存 ──
	frameMu     sync.RWMutex
	latestFrame []byte // 显示用 JPEG
	frameWidth  int
	frameHeight int
	rawFrame    []byte // 原始帧，用于标定
	rawWidth    int
	rawHeight   int

	// ── 预览节流 ──
	previewMu       sync.Mutex
	lastPreviewEmit time.Time
	previewInterval time.Duration

	// ── ROI 标记 ──
	roiMu sync.Mutex
	roiA  *hik.ExtensometerService
	roiB  *hik.ExtensometerService

	// ── 方向线 ──
	directionLine   hik.LineDirection
	directionLineMu sync.Mutex

	// ── 方向线自适应追踪状态 ──
	dirLineReady      bool    // 初始垂距是否已计算
	perpDistA         float64 // ROI A 中心到方向线的初始垂距
	perpDistB         float64 // ROI B 中心到方向线的初始垂距
	initialAngleRad   float64 // 方向线法向的初始角度
	angleChangeDeg    float64 // 累计角度变化（度）
	initialProjDist   float64 // 方向线上 A/B 投影点的初始距离
	initialCenterDist float64 // ROI A/B 中心距离初始值

	// ── 比例标定 ──
	resolutionRatio float64
	resolutionMu    sync.RWMutex

	// ── 相机标定与位姿标定 ──
	calibration            *hik.Calibration
	calibrationTransformMu sync.RWMutex
	cameraTransformMat     gocv.Mat
	poseTransformMat       gocv.Mat
	hasCameraTransform     bool
	hasPoseTransform       bool
	cameraCornersList      []gocv.Mat

	// ── 棋盘格参数 ──
	calibrationRows       int
	calibrationCols       int
	calibrationSquareSize float64
	calibrationFlow       string // "camera" 或 "pose"
	calibrationPatternMu  sync.RWMutex
}

func NewHIKCamera() *HIKCameraService {
	service := &HIKCameraService{
		camera:                hik.NewCamera(),
		previewInterval:       100 * time.Millisecond,
		stopCh:                make(chan struct{}),
		calibration:           hik.NewCalibration(),
		calibrationRows:       7,
		calibrationCols:       5,
		calibrationSquareSize: 25,
		calibrationFlow:       "camera",
	}

	// 设置图像回调 - camera.go 采集到图像后会调用此回调
	service.camera.SetImageCallback(service.onImageReceived)
	// 初始化相机 SDK
	service.camera.Init()

	return service
}

func (s *HIKCameraService) CallExit() {
	_ = s.CloseHIKCamera()
	s.Stop()
}

func (s *HIKCameraService) GetHIKCameraDevices() []string {
	devices, err := s.camera.GetCameraDevices()
	if err != nil {
		fmt.Println(err)
		return nil
	}
	return devices
}

func (s *HIKCameraService) IsHIKCameraConnected() bool {
	return s.camera.IsOpened()
}

// OpenHIKCamera 打开 HIK 相机
func (s *HIKCameraService) OpenHIKCamera(name string) error {
	return s.camera.OpenCamera(name)
}

// CloseHIKCamera 关闭 HIK 相机
func (s *HIKCameraService) CloseHIKCamera() error {
	return s.camera.CloseCamera()
}

// CallCameraWindow 唤起相机监控窗口（独立界面，路由 /#/camera）
func (s *HIKCameraService) CallCameraWindow() error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}

	if s.window != nil {
		fmt.Println("camera window already showing")
		return nil
	}

	s.window = app.Window.NewWithOptions(application.WebviewWindowOptions{
		Title:            "相机",
		Width:            1280,
		Height:           860,
		BackgroundColour: application.NewRGB(15, 23, 42),
		URL:              "/#/camera",
	})

	// 用户点击窗口右上角 X 或按 Alt+F4 关闭时，WebView 会被系统直接销毁，
	// 前端 onUnmounted 无法可靠通知后端，需在 Go 侧监听原生关闭事件以清理引用，
	// 否则 s.window 会一直指向已销毁窗口，导致 CallCameraWindow 无法再次打开窗口
	s.window.OnWindowEvent(events.Common.WindowClosing, func(event *application.WindowEvent) {
		fmt.Println("camera window closed")
		s.window = nil
	})
	return nil
}

// onImageReceived 图像数据回调（由 camera.go 调用）
func (s *HIKCameraService) onImageReceived(data []byte, frameId uint64) {
	defer func() {
		if r := recover(); r != nil {
			fmt.Printf("onImageReceived panic recovered: %v\n", r)
		}
	}()

	select {
	case <-s.stopCh:
		return
	default:
	}

	app := application.Get()
	if app == nil {
		return
	}
	display := s.processFrame(data)

	if len(display) != 0 {
		app.Event.Emit("hik_camera_frame", map[string]any{
			"frameId": frameId,
			"image":   "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(display),
		})
	}
}

func (s *HIKCameraService) processFrame(data []byte) []byte {
	rawMat, err := gocv.IMDecode(data, gocv.IMReadColor)
	if err != nil || rawMat.Empty() {
		return data
	}
	defer rawMat.Close()
	// 处理标定：待实现

	//保存原始图像
	s.storeLatestRawFrame(data, rawMat.Cols(), rawMat.Rows())

	// 应用标定变换
	displayMat, err := s.applyCalibrationTransforms(rawMat)
	if err != nil {
		displayMat = rawMat.Clone()
	}
	defer displayMat.Close()

	// 保存处理后的图像
	latestBuf, err := gocv.IMEncode(gocv.JPEGFileExt, displayMat)
	if err == nil {
		latest := make([]byte, len(latestBuf.GetBytes()))
		copy(latest, latestBuf.GetBytes())
		s.storeLatestFrame(latest, displayMat.Cols(), displayMat.Rows())
		latestBuf.Close()
	}

	// 处理ROI
	if s.roiA != nil || s.roiB != nil {
		s.updateTrackedROIs(displayMat) // 更新跟踪ROI
		s.adjustDirectionLine()         // 调整方向线
		s.drawTrackedROIs(&displayMat)  // 绘制跟踪ROI
	}

	//返回处理后的图像
	buf, err := gocv.IMEncode(gocv.JPEGFileExt, displayMat)
	if err != nil {
		return data
	}
	defer buf.Close()

	out := make([]byte, len(buf.GetBytes()))
	copy(out, buf.GetBytes())
	return out
}

func (s *HIKCameraService) applyCalibrationTransforms(img gocv.Mat) (gocv.Mat, error) {
	if img.Empty() {
		return gocv.Mat{}, fmt.Errorf("输入图像为空")
	}

	cameraMat, poseMat := s.cloneCalibrationTransforms()
	defer closeMatQuietly(&cameraMat)
	defer closeMatQuietly(&poseMat)

	result := img.Clone()
	calibration := s.calibration
	if calibration == nil {
		calibration = hik.NewCalibration()
	}
	if !cameraMat.Empty() {
		corrected, err := calibration.CorrectImage(result, cameraMat)
		result.Close()
		if err != nil {
			return gocv.Mat{}, err
		}
		result = corrected
	}
	if !poseMat.Empty() {
		corrected, err := calibration.CorrectImage(result, poseMat)
		result.Close()
		if err != nil {
			return gocv.Mat{}, err
		}
		result = corrected
	}
	return result, nil
}

func (s *HIKCameraService) cloneCalibrationTransforms() (gocv.Mat, gocv.Mat) {
	s.calibrationTransformMu.RLock()
	defer s.calibrationTransformMu.RUnlock()

	cameraMat := gocv.NewMat()
	if s.hasCameraTransform {
		cameraMat = s.cameraTransformMat.Clone()
	}

	poseMat := gocv.NewMat()
	if s.hasPoseTransform {
		poseMat = s.poseTransformMat.Clone()
	}

	return cameraMat, poseMat
}

func (s *HIKCameraService) setCameraTransform(mat gocv.Mat) {
	s.calibrationTransformMu.Lock()
	defer s.calibrationTransformMu.Unlock()

	if s.hasCameraTransform {
		closeMatQuietly(&s.cameraTransformMat)
	}
	s.hasCameraTransform = false
	if !mat.Empty() {
		s.cameraTransformMat = mat.Clone()
		s.hasCameraTransform = true
	}
}

func (s *HIKCameraService) setPoseTransform(mat gocv.Mat) {
	s.calibrationTransformMu.Lock()
	defer s.calibrationTransformMu.Unlock()

	if s.hasPoseTransform {
		closeMatQuietly(&s.poseTransformMat)
	}
	s.hasPoseTransform = false
	if !mat.Empty() {
		s.poseTransformMat = mat.Clone()
		s.hasPoseTransform = true
	}
}

func (s *HIKCameraService) clearCameraTransform() {
	s.calibrationTransformMu.Lock()
	defer s.calibrationTransformMu.Unlock()

	if s.hasCameraTransform {
		closeMatQuietly(&s.cameraTransformMat)
	}
	s.hasCameraTransform = false
}

func (s *HIKCameraService) clearCameraCalibration() {
	s.calibrationTransformMu.Lock()
	defer s.calibrationTransformMu.Unlock()

	if s.hasCameraTransform {
		closeMatQuietly(&s.cameraTransformMat)
	}
	s.hasCameraTransform = false
	for i := range s.cameraCornersList {
		closeMatQuietly(&s.cameraCornersList[i])
	}
	s.cameraCornersList = nil
}

func (s *HIKCameraService) clearPoseTransform() {
	s.calibrationTransformMu.Lock()
	defer s.calibrationTransformMu.Unlock()

	if s.hasPoseTransform {
		closeMatQuietly(&s.poseTransformMat)
	}
	s.hasPoseTransform = false
}

func (s *HIKCameraService) clearCalibrationTransforms() {
	s.calibrationTransformMu.Lock()
	defer s.calibrationTransformMu.Unlock()

	if s.hasCameraTransform {
		closeMatQuietly(&s.cameraTransformMat)
	}
	if s.hasPoseTransform {
		closeMatQuietly(&s.poseTransformMat)
	}
	s.hasCameraTransform = false
	s.hasPoseTransform = false
}

func closeMatQuietly(mat *gocv.Mat) {
	if mat == nil {
		return
	}
	defer func() {
		*mat = gocv.NewMat()
	}()
	func() {
		defer func() {
			_ = recover()
		}()
		mat.Close()
	}()
}

func (s *HIKCameraService) updateTrackedROIs(frame gocv.Mat) {
	s.roiMu.Lock()
	roiA := s.roiA
	roiB := s.roiB
	s.roiMu.Unlock()
	if roiA == nil || roiB == nil {
		return
	}

	grayFrame := gocv.NewMat()
	defer grayFrame.Close()
	if frame.Channels() > 1 {
		gocv.CvtColor(frame, &grayFrame, gocv.ColorBGRToGray)
	} else {
		frame.CopyTo(&grayFrame)
	}

	preparedFrame := roiA.PreprocessGrayForDIC(grayFrame)
	defer preparedFrame.Close()

	var wg sync.WaitGroup
	runTracker := func(tracker *hik.ExtensometerService) {
		defer wg.Done()
		_, _ = tracker.RunDICPreparedGrayMat(preparedFrame)
	}

	wg.Add(1)
	go runTracker(roiA)
	wg.Add(1)
	go runTracker(roiB)
	wg.Wait()
}

func (s *HIKCameraService) drawTrackedROIs(frame *gocv.Mat) {
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

func (s *HIKCameraService) storeLatestFrame(data []byte, width, height int) {
	s.frameMu.Lock()
	defer s.frameMu.Unlock()

	s.latestFrame = make([]byte, len(data))
	copy(s.latestFrame, data)
	s.frameWidth = width
	s.frameHeight = height
}

func (s *HIKCameraService) storeLatestRawFrame(data []byte, width, height int) {
	s.frameMu.Lock()
	defer s.frameMu.Unlock()

	s.rawFrame = make([]byte, len(data))
	copy(s.rawFrame, data)
	s.rawWidth = width
	s.rawHeight = height
}

func (s *HIKCameraService) GetLatestFrameForROI() (map[string]any, error) {
	s.frameMu.RLock()
	defer s.frameMu.RUnlock()

	if len(s.latestFrame) == 0 {
		return nil, fmt.Errorf("暂无相机图像，请先打开相机并等待图像流")
	}

	return map[string]any{
		"image":  "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(s.latestFrame),
		"width":  s.frameWidth,
		"height": s.frameHeight,
	}, nil
}

// OpenROISelector 打开 ROI 选择器
// 输入：ROI 标记（A 或 B）
// 输出：无
func (s *HIKCameraService) OpenROISelector(label string) error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}

	_, err := s.GetLatestFrameForROI()
	if err != nil {
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

func (s *HIKCameraService) SetROI(label string, rect hik.ROIRect) error {
	if rect.Width <= 0 || rect.Height <= 0 {
		return fmt.Errorf("ROI 区域无效")
	}
	s.frameMu.RLock()
	latestFrame := make([]byte, len(s.latestFrame))
	copy(latestFrame, s.latestFrame)
	s.frameMu.RUnlock()
	if len(latestFrame) == 0 {
		return fmt.Errorf("暂无相机图像")
	}

	mat, err := gocv.IMDecode(latestFrame, gocv.IMReadColor)
	if err != nil || mat.Empty() {
		return fmt.Errorf("解析相机图像失败")
	}
	defer mat.Close()

	tracker := hik.NewExtensometerService()
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

// OpenDirectionSelector 打开方向线选择器
// 输入：无
// 输出：无
func (s *HIKCameraService) OpenDirectionSelector() error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}

	_, err := s.GetLatestFrameForROI()
	if err != nil {
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

// SetDirectionLineFromConfig 从配置文件设置方向线段
func (s *HIKCameraService) SetDirectionLineFromConfig(line hik.LineDirection) {
	s.directionLineMu.Lock()
	s.directionLine = line
	s.dirLineReady = false // 需要重新计算初始垂距
	s.directionLineMu.Unlock()
}

// SetDirectionLine 设置方向线段
func (s *HIKCameraService) SetDirectionLine(line hik.LineDirection) error {
	dx := line.X2 - line.X1
	dy := line.Y2 - line.Y1
	length := math.Sqrt(dx*dx + dy*dy)
	if length < 5 {
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

// GetDirectionLine 获取方向线段
func (s *HIKCameraService) GetDirectionLine() (hik.LineDirection, bool) {
	s.directionLineMu.Lock()
	defer s.directionLineMu.Unlock()

	dx := s.directionLine.X2 - s.directionLine.X1
	dy := s.directionLine.Y2 - s.directionLine.Y1
	length := math.Sqrt(dx*dx + dy*dy)
	if length < 5 {
		return hik.LineDirection{}, false
	}
	return s.directionLine, true
}

// OpenCalibrationSelector 打开比例标定选择器
// 输入：无
// 输出：无
func (s *HIKCameraService) OpenCalibrationSelector() error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}

	_, err := s.GetLatestFrameForROI()
	if err != nil {
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

// SetResolutionRatio 设置比例标定系数
func (s *HIKCameraService) SetResolutionRatio(pixelLength, realLength float64) error {
	if pixelLength <= 0 || realLength <= 0 {
		return fmt.Errorf("像素长度和实际距离必须大于0")
	}
	s.resolutionMu.Lock()
	s.resolutionRatio = pixelLength / realLength
	s.resolutionMu.Unlock()
	return nil
}

// GetResolutionRatio 获取比例标定系数
func (s *HIKCameraService) GetResolutionRatio() float64 {
	s.resolutionMu.RLock()
	defer s.resolutionMu.RUnlock()
	return s.resolutionRatio
}

// resetDirectionLineTracking 重置方向线的初始追踪状态，下次 adjustDirectionLine 会重新计算。
func (s *HIKCameraService) resetDirectionLineTracking() {
	s.directionLineMu.Lock()
	defer s.directionLineMu.Unlock()
	s.resetDirectionLineTrackingLocked()
}

// resetDirectionLineTrackingLocked 无锁版本，调用方需持有 directionLineMu
func (s *HIKCameraService) resetDirectionLineTrackingLocked() {
	s.dirLineReady = false
	s.initialCenterDist = 0
	s.initialProjDist = 0
	s.angleChangeDeg = 0
}

// adjustDirectionLine 根据 ROI A/B 的最新追踪位置，动态调整方向线角度。
//
// 核心原理：两个标记点中心到方向线的垂距在拉伸过程中应保持恒定。
// 每帧根据新的标记点位置，求解满足垂距恒定的新直线方程，
// 从而获取方向线的实时偏转角度（反映拉伸角度的变化）。
func (s *HIKCameraService) adjustDirectionLine() {
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
		// 首次：记录初始有符号垂距和方向线角度
		s.perpDistA = a0*ax + b0*ay + c0
		s.perpDistB = a0*bx + b0*by + c0
		s.initialAngleRad = math.Atan2(a0, -b0) // 方向线的方向角（非法向角）
		s.initialCenterDist = math.Sqrt((bx-ax)*(bx-ax) + (by-ay)*(by-ay))

		// 计算初始投影点距离
		pax := ax - a0*(a0*ax+b0*ay+c0)
		pay := ay - b0*(a0*ax+b0*ay+c0)
		pbx := bx - a0*(a0*bx+b0*by+c0)
		pby := by - b0*(a0*bx+b0*by+c0)
		s.initialProjDist = math.Sqrt((pbx-pax)*(pbx-pax) + (pby-pay)*(pby-pay))
		// fmt.Printf("[video-debug] init centerDist=%.6f projDist=%.6f\n", s.initialCenterDist, s.initialProjDist)

		s.dirLineReady = true
		return
	}

	// 后续帧：计算保持垂距不变的新方向线
	Δx := ax - bx
	Δy := ay - by
	R := math.Sqrt(Δx*Δx + Δy*Δy)
	if R < 1 {
		return
	}
	φ := math.Atan2(Δy, Δx)
	Δd := s.perpDistA - s.perpDistB

	ratio := Δd / R
	if ratio < -1 || ratio > 1 {
		return // 标记点间距过小，无法求解，保持当前方向线
	}

	// 两个候选法向角
	α := math.Acos(ratio)
	θ1 := φ + α
	θ2 := φ - α

	// 选与初始法向角更接近的解
	θ0 := math.Atan2(b0, a0)
	if math.Abs(angleDiff(θ1, θ0)) > math.Abs(angleDiff(θ2, θ0)) {
		θ1 = θ2
	}
	a := math.Cos(θ1)
	b := math.Sin(θ1)
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

	// 计算方向线的角度变化
	currentAngle := math.Atan2(a, -b)
	θdiff := currentAngle - s.initialAngleRad
	// 归一化到 [-π/2, π/2]
	for θdiff > math.Pi/2 {
		θdiff -= math.Pi
	}
	for θdiff < -math.Pi/2 {
		θdiff += math.Pi
	}
	s.angleChangeDeg = θdiff * 180 / math.Pi

	// 计算当前投影距离并推送视频数据
	currentProjDist := math.Sqrt((projBx-projAx)*(projBx-projAx) + (projBy-projAy)*(projBy-projAy))
	if s.initialProjDist > 0 {
		videoDisp := currentProjDist - s.initialProjDist
		videoStrain := videoDisp / s.initialProjDist
		ratio := s.GetResolutionRatio()
		if ratio > 0 {
			videoDisp = videoDisp / ratio
		}
		videoDisp = math.Round(videoDisp*10000) / 10000
		videoStrain = math.Round(videoStrain*1000000) / 1000000
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
func (s *HIKCameraService) updateTrackerAxesLocked() {
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
	ux := float64(dx) / float64(totalLen)
	uy := float64(dy) / float64(totalLen)

	steps := int(totalLen) / dashLen
	if steps < 1 {
		steps = 1
	}
	for i := 0; i < steps; i++ {
		if i%2 == 0 {
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
}

// projectPointToLine 将点 (px, py) 垂线投影到方向线 (x1,y1)-(x2,y2) 上，返回投影点坐标
func projectPointToLine(px, py float64, line hik.LineDirection) (float64, float64) {
	dx := line.X2 - line.X1
	dy := line.Y2 - line.Y1
	den := dx*dx + dy*dy
	if den < 1e-10 {
		return line.X1, line.Y1
	}
	t := ((px-line.X1)*dx + (py-line.Y1)*dy) / den
	return line.X1 + t*dx, line.Y1 + t*dy
}

// Stop 停止所有后台协程（应用退出时调用）
func (s *HIKCameraService) Stop() {
	s.stopOnce.Do(func() {
		close(s.stopCh)
	})
}
