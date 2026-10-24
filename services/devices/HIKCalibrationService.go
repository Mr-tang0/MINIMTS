package devices

import (
	"MINIMTS/services/devices/hik"
	"encoding/base64"
	"fmt"
	"image"
	"strings"
	"sync"

	"github.com/wailsapp/wails/v3/pkg/application"
	"gocv.io/x/gocv"
)

// CornerPoint 棋盘格角点坐标
type CornerPoint struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
}

// FindChessboardResult 棋盘格角点识别结果
type FindChessboardResult struct {
	Success bool          `json:"success"`
	Image   string        `json:"image"`
	Corners []CornerPoint `json:"corners"`
	Error   string        `json:"error"`
}

// AddCalibrationCornersResult 相机标定角点累计结果
type AddCalibrationCornersResult struct {
	Success bool   `json:"success"`
	Count   int    `json:"count"`
	Error   string `json:"error"`
}

// HIKCalibrationService 负责相机图像的标定。
//
// 它从 HIKCameraService 读取最新一帧原始图像，进行棋盘格识别，
// 计算相机标定（内参去畸变）或位姿标定（透视）变换矩阵，
// 并通过变换钩子把变换结果应用回相机的实时图像流。
type HIKCalibrationService struct {
	camera      *HIKCameraService
	calibration *hik.Calibration

	// ── 棋盘格参数 ──
	patternMu  sync.RWMutex
	rows       int
	cols       int
	squareSize float64

	// ── 标定变换矩阵 ──
	transformMu        sync.RWMutex
	cameraTransformMat gocv.Mat
	poseTransformMat   gocv.Mat
	hasCameraTransform bool
	hasPoseTransform   bool
	cameraCornersList  []gocv.Mat
}

// NewHIKCalibration 创建标定服务，并把它注册为相机服务的变换钩子
func NewHIKCalibration(camera *HIKCameraService) *HIKCalibrationService {
	service := &HIKCalibrationService{
		camera:      camera,
		calibration: hik.NewCalibration(),
		rows:        7,
		cols:        5,
		squareSize:  25,
	}
	camera.setTransformHook(service.applyTransforms)
	return service
}

// ────────────── 棋盘格参数与识别 ──────────────

// SetCalibrationPattern 设置棋盘格行列数与方格边长
func (s *HIKCalibrationService) SetCalibrationPattern(rows, cols int, squareSize float64) error {
	if rows <= 0 || cols <= 0 || squareSize <= 0 {
		return fmt.Errorf("棋盘格参数无效")
	}
	s.patternMu.Lock()
	s.rows = rows
	s.cols = cols
	s.squareSize = squareSize
	s.patternMu.Unlock()
	return nil
}

func (s *HIKCalibrationService) getCalibrationPattern() (int, int, float64) {
	s.patternMu.RLock()
	defer s.patternMu.RUnlock()
	return s.rows, s.cols, s.squareSize
}

// FindChessboardCorners 在最新一帧原始图像中识别棋盘格角点
func (s *HIKCalibrationService) FindChessboardCorners(rows, cols int) *FindChessboardResult {
	data, width, height, pixelType := s.camera.latestRawFrameSnapshot()
	if len(data) == 0 {
		return &FindChessboardResult{Success: false, Error: "未获取到相机图像"}
	}

	var img gocv.Mat
	var err error
	if hik.IsRaw8Bit(pixelType) {
		raw, err := gocv.NewMatFromBytes(height, width, gocv.MatTypeCV8U, data)
		if err != nil || raw.Empty() {
			return &FindChessboardResult{Success: false, Error: fmt.Sprintf("图像解析失败: %v", err)}
		}
		if code := bayerToBGRCode(pixelType); code >= 0 {
			bgr := gocv.NewMat()
			gocv.CvtColor(raw, &bgr, code)
			raw.Close()
			img = bgr
		} else {
			img = raw // Mono8 灰度可直接找棋盘格
		}
	} else {
		img, err = gocv.IMDecode(data, gocv.IMReadColor)
	}
	if err != nil || img.Empty() {
		return &FindChessboardResult{Success: false, Error: fmt.Sprintf("图像解码失败: %v", err)}
	}
	defer img.Close()

	corners, err := s.calibration.FindChessboardCorners(img, image.Pt(cols, rows))
	if err != nil {
		return &FindChessboardResult{Success: false, Error: err.Error()}
	}
	defer corners.Close()

	buf, err := encodeJPEG(img)
	if err != nil {
		return &FindChessboardResult{Success: false, Error: fmt.Sprintf("图像编码失败: %v", err)}
	}

	cornerPoints := make([]CornerPoint, corners.Total())
	pt2fVec := gocv.NewPoint2fVectorFromMat(corners)
	defer pt2fVec.Close()
	for i := 0; i < pt2fVec.Size(); i++ {
		pt := pt2fVec.At(i)
		cornerPoints[i] = CornerPoint{X: float64(pt.X), Y: float64(pt.Y)}
	}

	return &FindChessboardResult{
		Success: true,
		Image:   "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(buf),
		Corners: cornerPoints,
	}
}

// ────────────── 标定窗口 ──────────────

// OpenCameraCalibration 打开相机标定（棋盘格角点调整）窗口
func (s *HIKCalibrationService) OpenCameraCalibration() error {
	return s.openCalibrationWindow("棋盘格角点调整", "calibration_corners")
}

// OpenPoseCalibration 打开位姿标定窗口
func (s *HIKCalibrationService) OpenPoseCalibration() error {
	return s.openCalibrationWindow("位姿标定", "pose_calibration")
}

// openCalibrationWindow 以最新一帧棋盘格画面打开标定窗口
func (s *HIKCalibrationService) openCalibrationWindow(title, mode string) error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}
	if err := s.camera.ensureLatestFrame(); err != nil {
		return err
	}

	rows, cols, squareSize := s.getCalibrationPattern()
	app.Window.NewWithOptions(application.WebviewWindowOptions{
		Title:            title,
		Width:            1200,
		Height:           850,
		BackgroundColour: application.NewRGB(15, 23, 42),
		URL: fmt.Sprintf("/#/calibration?mode=%s&rows=%d&cols=%d&squareSize=%g",
			mode, rows, cols, squareSize),
	})
	return nil
}

// ────────────── 标定计算 ──────────────

// AddCalibrationCorners 累加一帧棋盘格角点，累计 3 帧以上后计算相机标定矩阵
func (s *HIKCalibrationService) AddCalibrationCorners(corners []CornerPoint, rows, cols int, squareSize float64) *AddCalibrationCornersResult {
	if len(corners) == 0 {
		return &AddCalibrationCornersResult{Success: false, Error: "角点数据为空"}
	}

	cornersMat := cornersToMat(corners)
	defer closeMatQuietly(&cornersMat)

	s.transformMu.Lock()
	s.cameraCornersList = append(s.cameraCornersList, cornersMat.Clone())
	cornersList := make([]gocv.Mat, len(s.cameraCornersList))
	for i := range s.cameraCornersList {
		cornersList[i] = s.cameraCornersList[i].Clone()
	}
	count := len(s.cameraCornersList)
	s.transformMu.Unlock()
	defer func() {
		for i := range cornersList {
			closeMatQuietly(&cornersList[i])
		}
	}()

	if count >= 3 {
		cameraMat, err := s.calibration.ComputeCameraCalibration(cornersList, image.Pt(cols, rows), squareSize)
		if err != nil {
			return &AddCalibrationCornersResult{Success: false, Count: count, Error: err.Error()}
		}
		s.setCameraTransform(cameraMat)
		closeMatQuietly(&cameraMat)
	}

	return &AddCalibrationCornersResult{Success: true, Count: count}
}

// ClearCameraCalibration 清除相机标定矩阵与已采集的角点
func (s *HIKCalibrationService) ClearCameraCalibration() error {
	s.transformMu.Lock()
	defer s.transformMu.Unlock()

	if s.hasCameraTransform {
		closeMatQuietly(&s.cameraTransformMat)
	}
	s.hasCameraTransform = false
	for i := range s.cameraCornersList {
		closeMatQuietly(&s.cameraCornersList[i])
	}
	s.cameraCornersList = nil
	return nil
}

// AddPoseCalibration 根据棋盘格角点计算位姿（透视）变换矩阵
func (s *HIKCalibrationService) AddPoseCalibration(corners []CornerPoint, rows, cols int, squareSize float64) error {
	if len(corners) == 0 {
		return fmt.Errorf("角点数据为空")
	}

	cornersMat := cornersToMat(corners)
	defer closeMatQuietly(&cornersMat)

	pose, err := s.calibration.ComputePose(cornersMat, image.Pt(cols, rows), squareSize)
	if err != nil {
		return err
	}
	defer closeMatQuietly(&pose)

	s.setPoseTransform(pose)
	return nil
}

// ClearPoseCalibration 清除位姿标定矩阵
func (s *HIKCalibrationService) ClearPoseCalibration() error {
	s.transformMu.Lock()
	defer s.transformMu.Unlock()

	if s.hasPoseTransform {
		closeMatQuietly(&s.poseTransformMat)
	}
	s.hasPoseTransform = false
	return nil
}

// ────────────── 反投影变换 ──────────────

// applyTransforms 对图像依次应用相机标定与位姿标定变换（相机服务的变换钩子）
func (s *HIKCalibrationService) applyTransforms(img gocv.Mat) (gocv.Mat, error) {
	return s.transformImage(img)
}

// transformImage 对输入图像应用标定/位姿变换后返回，未标定时返回原图副本
func (s *HIKCalibrationService) transformImage(img gocv.Mat) (gocv.Mat, error) {
	if img.Empty() {
		return gocv.Mat{}, fmt.Errorf("输入图像为空")
	}

	cameraMat, poseMat := s.cloneCalibrationTransforms()
	defer closeMatQuietly(&cameraMat)
	defer closeMatQuietly(&poseMat)

	result := img.Clone()
	if !cameraMat.Empty() {
		corrected, err := s.calibration.CorrectImage(result, cameraMat)
		closeMatQuietly(&result)
		if err != nil {
			return gocv.Mat{}, err
		}
		result = corrected
	}
	if !poseMat.Empty() {
		corrected, err := s.calibration.CorrectImage(result, poseMat)
		closeMatQuietly(&result)
		if err != nil {
			return gocv.Mat{}, err
		}
		result = corrected
	}
	return result, nil
}

// TransformImageBase64 对输入的 base64 图片应用反投影变换后返回 base64 图片
func (s *HIKCalibrationService) TransformImageBase64(imageData string) (string, error) {
	payload := imageData
	if strings.HasPrefix(payload, "data:") {
		if idx := strings.Index(payload, ","); idx >= 0 {
			payload = payload[idx+1:]
		}
	}

	raw, err := base64.StdEncoding.DecodeString(strings.TrimSpace(payload))
	if err != nil {
		return "", fmt.Errorf("图片解码失败: %w", err)
	}

	img, err := gocv.IMDecode(raw, gocv.IMReadColor)
	if err != nil || img.Empty() {
		return "", fmt.Errorf("图片解析失败")
	}
	defer closeMatQuietly(&img)

	transformed, err := s.transformImage(img)
	if err != nil {
		return "", err
	}
	defer closeMatQuietly(&transformed)

	out, err := encodeJPEG(transformed)
	if err != nil {
		return "", fmt.Errorf("图片编码失败: %w", err)
	}
	return "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(out), nil
}

// ────────────── 变换矩阵存取 ──────────────

func (s *HIKCalibrationService) cloneCalibrationTransforms() (gocv.Mat, gocv.Mat) {
	s.transformMu.RLock()
	defer s.transformMu.RUnlock()

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

func (s *HIKCalibrationService) setCameraTransform(mat gocv.Mat) {
	s.transformMu.Lock()
	defer s.transformMu.Unlock()

	if s.hasCameraTransform {
		closeMatQuietly(&s.cameraTransformMat)
	}
	s.hasCameraTransform = false
	if !mat.Empty() {
		s.cameraTransformMat = mat.Clone()
		s.hasCameraTransform = true
	}
}

func (s *HIKCalibrationService) setPoseTransform(mat gocv.Mat) {
	s.transformMu.Lock()
	defer s.transformMu.Unlock()

	if s.hasPoseTransform {
		closeMatQuietly(&s.poseTransformMat)
	}
	s.hasPoseTransform = false
	if !mat.Empty() {
		s.poseTransformMat = mat.Clone()
		s.hasPoseTransform = true
	}
}

// closeMatQuietly 安全释放 Mat，并把变量重置为空 Mat
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

// cornersToMat 将角点切片转换为 Mat
func cornersToMat(corners []CornerPoint) gocv.Mat {
	pt2fSlice := make([]gocv.Point2f, len(corners))
	for i, corner := range corners {
		pt2fSlice[i] = gocv.Point2f{X: float32(corner.X), Y: float32(corner.Y)}
	}

	ptVec := gocv.NewPoint2fVectorFromPoints(pt2fSlice)
	defer ptVec.Close()
	return gocv.NewMatFromPoint2fVector(ptVec, true)
}
