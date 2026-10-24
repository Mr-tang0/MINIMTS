package services

import (
	"encoding/base64"
	"fmt"
	"image"

	"github.com/wailsapp/wails/v3/pkg/application"
	"gocv.io/x/gocv"
)

type CornerPoint struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
}

type FindChessboardResult struct {
	Success bool          `json:"success"`
	Image   string        `json:"image"`
	Corners []CornerPoint `json:"corners"`
	Error   string        `json:"error"`
}

func (s *HIKCameraService) SetCalibrationPattern(rows, cols int, squareSize float64) error {
	if rows <= 0 || cols <= 0 || squareSize <= 0 {
		return fmt.Errorf("棋盘格参数无效")
	}
	s.calibrationPatternMu.Lock()
	s.calibrationRows = rows
	s.calibrationCols = cols
	s.calibrationSquareSize = squareSize
	s.calibrationPatternMu.Unlock()
	return nil
}

func (s *HIKCameraService) getCalibrationPattern() (int, int, float64) {
	s.calibrationPatternMu.RLock()
	defer s.calibrationPatternMu.RUnlock()
	return s.calibrationRows, s.calibrationCols, s.calibrationSquareSize
}

func (s *HIKCameraService) getCalibrationFlow() string {
	s.calibrationPatternMu.RLock()
	defer s.calibrationPatternMu.RUnlock()
	return s.calibrationFlow
}

func (s *HIKCameraService) FindChessboardCorners(rows, cols int) *FindChessboardResult {
	s.frameMu.RLock()
	data := s.rawFrame
	s.frameMu.RUnlock()

	if len(data) == 0 {
		return &FindChessboardResult{Success: false, Error: "未获取到相机图像"}
	}

	img, err := gocv.IMDecode(data, gocv.IMReadColor)
	if err != nil || img.Empty() {
		return &FindChessboardResult{Success: false, Error: fmt.Sprintf("图像解码失败: %v", err)}
	}
	defer img.Close()

	corners, err := s.calibration.FindChessboardCorners(img, image.Pt(cols, rows))
	if err != nil {
		return &FindChessboardResult{Success: false, Error: err.Error()}
	}
	defer corners.Close()

	buf, err := gocv.IMEncode(gocv.JPEGFileExt, img)
	if err != nil {
		return &FindChessboardResult{Success: false, Error: fmt.Sprintf("图像编码失败: %v", err)}
	}
	defer buf.Close()

	cornerPoints := make([]CornerPoint, corners.Total())
	pt2fVec := gocv.NewPoint2fVectorFromMat(corners)
	defer pt2fVec.Close()
	for i := 0; i < pt2fVec.Size(); i++ {
		pt := pt2fVec.At(i)
		cornerPoints[i] = CornerPoint{X: float64(pt.X), Y: float64(pt.Y)}
	}

	return &FindChessboardResult{
		Success: true,
		Image:   "data:image/jpeg;base64," + base64.StdEncoding.EncodeToString(buf.GetBytes()),
		Corners: cornerPoints,
	}
}

func (s *HIKCameraService) OpenCameraCalibration() error {
	s.calibrationPatternMu.Lock()
	s.calibrationFlow = "camera"
	s.calibrationPatternMu.Unlock()
	return s.openCalibrationWindow("棋盘格角点调整", "calibration_corners")
}

func (s *HIKCameraService) OpenPoseCalibration() error {
	s.calibrationPatternMu.Lock()
	s.calibrationFlow = "pose"
	s.calibrationPatternMu.Unlock()
	return s.openCalibrationWindow("位姿标定", "pose_calibration")
}

func (s *HIKCameraService) openCalibrationWindow(title, mode string) error {
	app := application.Get()
	if app == nil {
		return fmt.Errorf("application not initialized")
	}

	if _, err := s.GetLatestFrameForROI(); err != nil {
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

type AddCalibrationCornersResult struct {
	Success bool   `json:"success"`
	Count   int    `json:"count"`
	Error   string `json:"error"`
}

func (s *HIKCameraService) AddCalibrationCorners(corners []CornerPoint, rows, cols int, squareSize float64) *AddCalibrationCornersResult {
	if len(corners) == 0 {
		return &AddCalibrationCornersResult{Success: false, Error: "角点数据为空"}
	}

	cornersMat := cornersToMat(corners)
	defer cornersMat.Close()

	s.calibrationTransformMu.Lock()
	s.cameraCornersList = append(s.cameraCornersList, cornersMat.Clone())
	cornersList := make([]gocv.Mat, len(s.cameraCornersList))
	for i := range s.cameraCornersList {
		cornersList[i] = s.cameraCornersList[i].Clone()
	}
	count := len(s.cameraCornersList)
	s.calibrationTransformMu.Unlock()
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
		cameraMat.Close()
	}

	return &AddCalibrationCornersResult{Success: true, Count: count}
}

func (s *HIKCameraService) ClearCameraCalibration() error {
	s.clearCameraCalibration()
	return nil
}

func (s *HIKCameraService) AddPoseCalibration(corners []CornerPoint, rows, cols int, squareSize float64) error {
	if len(corners) == 0 {
		return fmt.Errorf("角点数据为空")
	}

	cornersMat := cornersToMat(corners)
	defer cornersMat.Close()

	pose, err := s.calibration.ComputePose(cornersMat, image.Pt(cols, rows), squareSize)
	if err != nil {
		return err
	}
	defer pose.Close()

	s.setPoseTransform(pose)
	return nil
}

func (s *HIKCameraService) ClearPoseCalibration() error {
	s.clearPoseTransform()
	return nil
}

func (s *HIKCameraService) getRawImageSize() image.Point {
	s.frameMu.RLock()
	defer s.frameMu.RUnlock()
	return image.Pt(s.rawWidth, s.rawHeight)
}

func cornersToMat(corners []CornerPoint) gocv.Mat {
	pt2fSlice := make([]gocv.Point2f, len(corners))
	for i, corner := range corners {
		pt2fSlice[i] = gocv.Point2f{X: float32(corner.X), Y: float32(corner.Y)}
	}

	ptVec := gocv.NewPoint2fVectorFromPoints(pt2fSlice)
	defer ptVec.Close()
	return gocv.NewMatFromPoint2fVector(ptVec, true)
}
