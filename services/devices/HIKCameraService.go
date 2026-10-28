package devices

import (
	"MINIMTS/services/devices/hik"
	"encoding/base64"
	"fmt"
	"image"
	"sync"
	"time"

	"github.com/wailsapp/wails/v3/pkg/application"
	"github.com/wailsapp/wails/v3/pkg/events"
	"gocv.io/x/gocv"
)

// HIKCameraService 负责相机的连接/断开、相机窗口、图像采集与相机参数调节。
//
// 采集链路：相机回调 -> 标定变换钩子（HIKCalibrationService）-> ROI 跟踪钩子
// （HIKExtensometerService，快路径同步执行）；JPEG 编码与 hik_camera_frame
// 事件推送在独立协程中执行，不阻塞采集回调。
// 各服务之间通过钩子解耦，相机服务本身不关心标定与引伸计的实现细节。
type HIKCameraService struct {
	// ── 相机设备与窗口 ──
	camera *hik.Camera
	window *application.WebviewWindow

	// ── 采集生命周期 ──
	stopCh   chan struct{}
	stopOnce sync.Once

	// ── 图像帧缓存 ──
	frameMu     sync.RWMutex // 保护图像帧缓存的读写访问
	latestFrame []byte       // 显示用 JPEG（未叠加 ROI）
	frameWidth  int          // 图像帧宽度
	frameHeight int          // 图像帧高度

	rawFrame     []byte // 原始帧（Mono8 原始像素或 JPEG，取决于 rawPixelType）
	rawWidth     int    // 原始帧宽度
	rawHeight    int    // 原始帧高度
	rawPixelType uint32 // 原始帧像素格式

	// ── 外部服务钩子 ──
	hookMu        sync.RWMutex
	transformHook func(gocv.Mat) (gocv.Mat, error) // 标定变换
	trackHook     func(*gocv.Mat)                  // ROI 跟踪（采集回调内同步执行）
	drawHook      func(*gocv.Mat)                  // ROI 叠加绘制（显示编码协程内执行）

	// ── 显示编码通道 ──
	displayCh chan *displayJob
}

// displayJob 显示编码任务：data 为回调数据副本（8位原始像素或 JPEG），仅显示协程使用
type displayJob struct {
	frameId   uint64
	data      []byte
	width     int
	height    int
	pixelType uint32
}

func NewHIKCamera() *HIKCameraService {
	service := &HIKCameraService{
		camera:    hik.NewCamera(),
		stopCh:    make(chan struct{}),
		displayCh: make(chan *displayJob, 1),
	}

	// 设置图像回调
	service.camera.SetImageCallback(service.onImageReceived)
	// 初始化相机 SDK
	service.camera.Init()
	// 启动显示编码协程
	go service.displayWorker()

	return service
}

// ────────────── 相机设备 ──────────────

func (s *HIKCameraService) GetHIKCameraDevices() []string {
	devices, err := s.camera.GetCameraDevices()
	if err != nil {
		fmt.Println(err)
		return nil
	}
	return devices
}

// OpenHIKCamera 打开 HIK 相机
func (s *HIKCameraService) OpenHIKCamera(name string) error {
	return s.camera.OpenCamera(name)
}

// CloseHIKCamera 关闭 HIK 相机
func (s *HIKCameraService) CloseHIKCamera() error {
	return s.camera.CloseCamera()
}

// IsHIKCameraConnected 检查 HIK 相机是否已连接
func (s *HIKCameraService) IsHIKCameraConnected() bool {
	return s.camera.IsOpened()
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

	s.window.OnWindowEvent(events.Common.WindowClosing, func(event *application.WindowEvent) {
		fmt.Println("camera window closed")
		s.window = nil
	})
	return nil
}

func (s *HIKCameraService) CallExit() {
	_ = s.CloseHIKCamera()
	s.Stop()
}

// Stop 停止所有后台协程（应用退出时调用）
func (s *HIKCameraService) Stop() {
	s.stopOnce.Do(func() {
		close(s.stopCh)
	})
}

// ────────────── 帧采集与分发 ──────────────

// setTransformHook 注册标定变换钩子（由 HIKCalibrationService 在构造时调用）
func (s *HIKCameraService) setTransformHook(hook func(gocv.Mat) (gocv.Mat, error)) {
	s.hookMu.Lock()
	defer s.hookMu.Unlock()
	s.transformHook = hook
}

// setTrackHook 注册 ROI 跟踪钩子（由 HIKExtensometerService 在构造时调用）
func (s *HIKCameraService) setTrackHook(hook func(*gocv.Mat)) {
	s.hookMu.Lock()
	defer s.hookMu.Unlock()
	s.trackHook = hook
}

// setDrawHook 注册叠加绘制钩子（由 HIKExtensometerService 在构造时调用）
func (s *HIKCameraService) setDrawHook(hook func(*gocv.Mat)) {
	s.hookMu.Lock()
	defer s.hookMu.Unlock()
	s.drawHook = hook
}

// bayerToGrayCode PFNC Bayer 格式 → OpenCV Bayer→灰度 转换码
// 注意：OpenCV 的 Bayer 命名与 PFNC 相差 180°（PFNC BayerRG 对应 OpenCV BayerBG）
func bayerToGrayCode(pixelType uint32) gocv.ColorConversionCode {
	switch pixelType {
	case hik.PixelTypeGvspBayerRG8:
		return gocv.ColorBayerBGToGRAY
	case hik.PixelTypeGvspBayerGB8:
		return gocv.ColorBayerGRToGRAY
	case hik.PixelTypeGvspBayerGR8:
		return gocv.ColorBayerGBToGRAY
	case hik.PixelTypeGvspBayerBG8:
		return gocv.ColorBayerRGToGRAY
	}
	return -1
}

// bayerToBGRCode PFNC Bayer 格式 → OpenCV Bayer→BGR 转换码（命名差异同上）
func bayerToBGRCode(pixelType uint32) gocv.ColorConversionCode {
	switch pixelType {
	case hik.PixelTypeGvspBayerRG8:
		return gocv.ColorBayerBGToBGR
	case hik.PixelTypeGvspBayerGB8:
		return gocv.ColorBayerGRToBGR
	case hik.PixelTypeGvspBayerGR8:
		return gocv.ColorBayerGBToBGR
	case hik.PixelTypeGvspBayerBG8:
		return gocv.ColorBayerRGToBGR
	}
	return -1
}

// onImageReceived 图像数据回调（由 camera.go 调用）
var lastPixelTypeLog uint32 // 像素格式变化打印

func (s *HIKCameraService) onImageReceived(data []byte, frameId uint64, width, height int, pixelType uint32) {
	defer func() {
		if r := recover(); r != nil {
			fmt.Printf("onImageReceived panic recovered: %v\n", r)
		}
	}()

	if pixelType != lastPixelTypeLog {
		lastPixelTypeLog = pixelType
		switch {
		case pixelType == hik.PixelTypeGvspMono8:
			fmt.Println("[相机像素格式] Mono8 原始灰度直传")
		case hik.IsRaw8Bit(pixelType):
			fmt.Printf("[相机像素格式] 0x%08x Bayer 原始直传（灰度供DIC，BGR供显示）\n", pixelType)
		default:
			fmt.Printf("[相机像素格式] 0x%08x（非8位原始格式，走JPEG压缩链路）\n", pixelType)
		}
	}

	select {
	case <-s.stopCh:
		return
	default:
	}

	app := application.Get()
	if app == nil {
		return
	}

	// 解码（ROI 跟踪必需）：8 位原始格式直接包装像素跳过 JPEG 解码；
	// Bayer 彩色格式转灰度供 DIC 使用，Mono8 直接作为灰度帧
	var rawMat gocv.Mat
	var err error
	if hik.IsRaw8Bit(pixelType) {
		rawMat, err = gocv.NewMatFromBytes(height, width, gocv.MatTypeCV8U, data)
	} else {
		rawMat, err = gocv.IMDecode(data, gocv.IMReadColor)
	}
	if err != nil || rawMat.Empty() {
		return
	}

	var frameMat gocv.Mat
	if code := bayerToGrayCode(pixelType); code >= 0 {
		gray := gocv.NewMat()
		gocv.CvtColor(rawMat, &gray, code)
		rawMat.Close()
		frameMat = gray
	} else {
		frameMat = rawMat
	}
	defer frameMat.Close()

	// 保存原始图像，供标定使用
	s.storeLatestRawFrame(data, width, height, pixelType)

	// 应用标定变换
	displayMat, err := s.applyTransformHook(frameMat)
	if err != nil || displayMat.Empty() {
		closeMatQuietly(&displayMat)
		displayMat = frameMat.Clone()
	}

	// ROI 跟踪快路径（DIC + 结果发射），不含任何编码绘制
	s.applyTrackHook(&displayMat)
	displayMat.Close()

	// 显示编码转入独立协程；通道满时丢弃旧帧保最新
	job := &displayJob{frameId: frameId, data: data, width: width, height: height, pixelType: pixelType}
	select {
	case s.displayCh <- job:
	default:
		select {
		case old := <-s.displayCh:
			_ = old // 丢弃旧帧（数据为字节切片，无需释放 Mat）
		default:
		}
		s.displayCh <- job
	}
}

// 显示链路减负参数：预览无交互，限帧率 + 降采样可大幅降低 CPU 占用；
// DIC 追踪走采集回调快路径，不受显示节流影响
const (
	minDisplayInterval = 100 * time.Millisecond // 显示帧率上限（10fps，丢弃间隔内的帧）
	cacheMinInterval   = time.Second            // 全分辨率缓存帧最小更新间隔（供选择器/标定取静态快照）
	displayMaxWidth    = 1280                   // 预览帧最大宽度（超出则降采样）
)

// displayWorker 显示编码协程：JPEG 编码与前端推送不占用采集回调线程
func (s *HIKCameraService) displayWorker() {
	var lastDisplay, lastCache time.Time
	for {
		select {
		case <-s.stopCh:
			return
		case job := <-s.displayCh:
			// 帧率节流：未到间隔直接丢弃，通道容量 1 自动保最新
			now := time.Now()
			if !lastDisplay.IsZero() && now.Sub(lastDisplay) < minDisplayInterval {
				continue
			}
			lastDisplay = now

			// 原始帧 → BGR 显示帧；JPEG 数据直接解码
			var mat gocv.Mat
			if hik.IsRaw8Bit(job.pixelType) {
				raw, err := gocv.NewMatFromBytes(job.height, job.width, gocv.MatTypeCV8U, job.data)
				if err != nil || raw.Empty() {
					continue
				}
				bgr := gocv.NewMat()
				if code := bayerToBGRCode(job.pixelType); code >= 0 {
					gocv.CvtColor(raw, &bgr, code)
				} else {
					gocv.CvtColor(raw, &bgr, gocv.ColorGrayToBGR)
				}
				raw.Close()
				mat = bgr
			} else {
				decoded, err := gocv.IMDecode(job.data, gocv.IMReadColor)
				if err != nil || decoded.Empty() {
					continue
				}
				mat = decoded
			}

			// 应用标定变换
			displayMat, err := s.applyTransformHook(mat)
			mat.Close()
			if err != nil || displayMat.Empty() {
				closeMatQuietly(&displayMat)
				continue
			}

			// 降频缓存无叠加的全分辨率图像，供 ROI/方向线选择器与标定界面取帧
			// （缓存帧直接喂 DIC 模板，必须保持全分辨率；每秒编码一次开销可忽略）
			if lastCache.IsZero() || now.Sub(lastCache) >= cacheMinInterval {
				if frame, err := encodeJPEG(displayMat); err == nil {
					s.storeLatestFrame(frame, displayMat.Cols(), displayMat.Rows())
				}
				lastCache = now
			}

			// 叠加跟踪 ROI 与方向线（全图坐标，降采样前绘制）
			s.applyDrawHook(&displayMat)

			// 显示帧降采样：降低 JPEG 编码与前端解码开销
			if displayMat.Cols() > displayMaxWidth {
				scale := float64(displayMaxWidth) / float64(displayMat.Cols())
				small := gocv.NewMat()
				gocv.Resize(displayMat, &small, image.Pt(displayMaxWidth, int(float64(displayMat.Rows())*scale)), 0, 0, gocv.InterpolationArea)
				displayMat.Close()
				displayMat = small
			}

			out, err := encodeJPEG(displayMat)
			displayMat.Close()

			if err != nil {
				continue
			}

			app := application.Get()
			if app == nil {
				continue
			}
			app.Event.Emit("hik_camera_frame", map[string]any{
				"frameId": job.frameId,
				"image":   "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(out),
			})
		}
	}
}

// applyTransformHook 调用标定变换钩子，未注册时直接返回原图副本
func (s *HIKCameraService) applyTransformHook(img gocv.Mat) (gocv.Mat, error) {
	s.hookMu.RLock()
	hook := s.transformHook
	s.hookMu.RUnlock()

	if hook == nil {
		return img.Clone(), nil
	}
	return hook(img)
}

// applyTrackHook 调用 ROI 跟踪钩子，未注册时不做任何处理
func (s *HIKCameraService) applyTrackHook(img *gocv.Mat) {
	s.hookMu.RLock()
	hook := s.trackHook
	s.hookMu.RUnlock()

	if hook != nil {
		hook(img)
	}
}

// applyDrawHook 调用叠加绘制钩子，未注册时不做任何处理
func (s *HIKCameraService) applyDrawHook(img *gocv.Mat) {
	s.hookMu.RLock()
	hook := s.drawHook
	s.hookMu.RUnlock()

	if hook != nil {
		hook(img)
	}
}

// ────────────── 帧缓存 ──────────────

func (s *HIKCameraService) storeLatestFrame(data []byte, width, height int) {
	s.frameMu.Lock()
	defer s.frameMu.Unlock()

	s.latestFrame = make([]byte, len(data))
	copy(s.latestFrame, data)
	s.frameWidth = width
	s.frameHeight = height
}

func (s *HIKCameraService) storeLatestRawFrame(data []byte, width, height int, pixelType uint32) {
	s.frameMu.Lock()
	defer s.frameMu.Unlock()

	s.rawFrame = make([]byte, len(data))
	copy(s.rawFrame, data)
	s.rawWidth = width
	s.rawHeight = height
	s.rawPixelType = pixelType
}

// latestDisplayFrameSnapshot 返回当前显示帧副本（未叠加 ROI）
func (s *HIKCameraService) latestDisplayFrameSnapshot() ([]byte, int, int) {
	s.frameMu.RLock()
	defer s.frameMu.RUnlock()

	data := make([]byte, len(s.latestFrame))
	copy(data, s.latestFrame)
	return data, s.frameWidth, s.frameHeight
}

// latestRawFrameSnapshot 返回当前原始帧副本（Mono8 原始像素或 JPEG，见 pixelType）
func (s *HIKCameraService) latestRawFrameSnapshot() ([]byte, int, int, uint32) {
	s.frameMu.RLock()
	defer s.frameMu.RUnlock()

	data := make([]byte, len(s.rawFrame))
	copy(data, s.rawFrame)
	return data, s.rawWidth, s.rawHeight, s.rawPixelType
}

// ensureLatestFrame 校验是否已采集到图像
func (s *HIKCameraService) ensureLatestFrame() error {
	s.frameMu.RLock()
	defer s.frameMu.RUnlock()

	if len(s.latestFrame) == 0 {
		return fmt.Errorf("暂无相机图像，请先打开相机并等待图像流")
	}
	return nil
}

// GetLatestFrameForROI 获取最新一帧显示图像（base64），供前端选择器界面使用
func (s *HIKCameraService) GetLatestFrameForROI() (map[string]any, error) {
	data, width, height := s.latestDisplayFrameSnapshot()
	if len(data) == 0 {
		return nil, fmt.Errorf("暂无相机图像，请先打开相机并等待图像流")
	}

	return map[string]any{
		"image":  "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(data),
		"width":  width,
		"height": height,
	}, nil
}

// encodeJPEG 将 Mat 编码为 JPEG 字节
func encodeJPEG(img gocv.Mat) ([]byte, error) {
	buf, err := gocv.IMEncode(gocv.JPEGFileExt, img)
	if err != nil {
		return nil, err
	}
	defer buf.Close()

	out := make([]byte, len(buf.GetBytes()))
	copy(out, buf.GetBytes())
	return out, nil
}

// ────────────── 相机参数读取 ──────────────

// cameraModeToString 将相机模式值转换为字符串
func cameraModeToString(value uint32) string {
	switch value {
	case 1:
		return "once_auto"
	case 2:
		return "continuous_auto"
	default:
		return "manual"
	}
}

// cameraModeToValue 将相机模式字符串转换为值
func cameraModeToValue(mode string) uint32 {
	switch mode {
	case "once_auto":
		return 1
	case "continuous_auto":
		return 2
	default:
		return 0
	}
}

// GetCameraParams 获取相机参数
func (s *HIKCameraService) GetCameraParams() hik.HIKCameraParams {
	var p hik.HIKCameraParams

	if value, _, err := s.camera.GetEnumValue("ExposureAuto"); err == nil {
		fmt.Println("GetExposureAuto:", cameraModeToString(value))
		p.ExposureAuto = value
	}
	if value, _, _, err := s.camera.GetFloatValue("ExposureTime"); err == nil {
		fmt.Println("GetExposureTime:", value)
		p.ExposureTime = value
	}
	if value, _, err := s.camera.GetEnumValue("GainAuto"); err == nil {
		fmt.Println("GetGainAuto:", cameraModeToString(value))
		p.GainAuto = value
	}
	if value, _, _, err := s.camera.GetFloatValue("Gain"); err == nil {
		fmt.Println("GetGain:", value)
		p.Gain = value
	}
	if value, _, _, err := s.camera.GetFloatValue("DigitalShift"); err == nil {
		fmt.Println("GetDigitalGain:", value)
		p.DigitalGain = value
		p.DigitalGainEnable = value > 0
	}
	if value, err := s.camera.GetBoolValue("DigitalShiftEnable"); err == nil {
		fmt.Println("GetDigitalGainEnable:", value)
		p.DigitalGainEnable = value
	}
	if value, _, _, err := s.camera.GetFloatValue("ResultingFrameRate"); err == nil {
		fmt.Println("GetFrameRate:", value)
		p.FrameRate = value
	}
	if value, err := s.camera.GetBoolValue("GammaEnable"); err == nil {
		fmt.Println("GetGammaEnable:", value)
		p.GammaEnable = value
	}
	if value, _, _, err := s.camera.GetFloatValue("Gamma"); err == nil {
		fmt.Println("GetGamma:", value)
		p.Gamma = value
	}
	if value, _, err := s.camera.GetEnumValue("BalanceWhiteAuto"); err == nil {
		fmt.Println("GetBalanceWhiteAuto:", cameraModeToString(value))
		p.BalanceWhiteAuto = value
	}
	if err := s.camera.SetEnumValue("BalanceRatioSelector", 0); err == nil {
		fmt.Println("SetBalanceRatioSelector:", 0)
		if value, _, _, _, err := s.camera.GetIntValue("BalanceRatio"); err == nil {
			fmt.Println("GetBalanceWhite:", value)
			p.BalanceWhite = value
		}
	}

	return p
}

// ────────────── 相机参数设置 ──────────────

func (s *HIKCameraService) SetExposureAuto(mode string) error {
	return s.camera.SetEnumValue("ExposureAuto", cameraModeToValue(mode))
}

func (s *HIKCameraService) SetExposureTime(value float64) error {
	fmt.Println("SetExposureTime:", value)
	return s.camera.SetFloatValue("ExposureTime", value)
}

func (s *HIKCameraService) SetGainAuto(mode string) error {
	fmt.Println("SetGainAuto:", cameraModeToString(cameraModeToValue(mode)))
	return s.camera.SetEnumValue("GainAuto", cameraModeToValue(mode))
}

func (s *HIKCameraService) SetGain(value float64) error {
	fmt.Println("SetGain:", value)
	return s.camera.SetFloatValue("Gain", value)
}

func (s *HIKCameraService) SetDigitalGainEnabled(enabled bool) error {
	return s.camera.SetBoolValue("DigitalShiftEnable", enabled)
}

func (s *HIKCameraService) SetDigitalGain(value float64) error {
	fmt.Println("SetDigitalGain:", value)
	return s.camera.SetFloatValue("DigitalShift", value)
}

func (s *HIKCameraService) SetGammaEnabled(enabled bool) error {
	return s.camera.SetBoolValue("GammaEnable", enabled)
}

func (s *HIKCameraService) SetGamma(value float64) error {
	return s.camera.SetFloatValue("Gamma", value)
}

func (s *HIKCameraService) SetBalanceWhiteAuto(mode string) error {
	return s.camera.SetEnumValue("BalanceWhiteAuto", cameraModeToValue(mode))
}

func (s *HIKCameraService) SetBalanceWhiteAutoByString(value string) error {
	return s.camera.SetEnumValueByString("BalanceWhiteAuto", value)
}

func (s *HIKCameraService) SetBalanceRatio(selector string, value int64) error {
	if err := s.camera.SetEnumValueByString("BalanceRatioSelector", selector); err != nil {
		return err
	}
	return s.camera.SetIntValue("BalanceRatio", value)
}

func (s *HIKCameraService) SetWhiteBalanceRed(value int64) error {
	return s.SetBalanceRatio("Red", value)
}

func (s *HIKCameraService) SetWhiteBalanceGreen(value int64) error {
	return s.SetBalanceRatio("Green", value)
}

func (s *HIKCameraService) SetWhiteBalanceBlue(value int64) error {
	return s.SetBalanceRatio("Blue", value)
}
