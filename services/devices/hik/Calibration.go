package hik

import (
	"fmt"
	"image"
	"math"

	"gocv.io/x/gocv"
)

// Calibration provides stateless chessboard calibration helpers.
type Calibration struct{}

func NewCalibration() *Calibration {
	return &Calibration{}
}

// FindChessboardCorners detects chessboard corners and refines them to sub-pixel accuracy.
func (c *Calibration) FindChessboardCorners(img gocv.Mat, patternSize image.Point) (gocv.Mat, error) {
	if img.Empty() {
		return gocv.Mat{}, fmt.Errorf("输入图像为空")
	}

	gray := gocv.NewMat()
	defer gray.Close()
	if img.Channels() > 1 {
		gocv.CvtColor(img, &gray, gocv.ColorBGRToGray)
	} else {
		img.CopyTo(&gray)
	}

	corners := gocv.NewMat()
	found := gocv.FindChessboardCornersSB(gray, patternSize, &corners,
		gocv.CalibCBExhaustive|gocv.CalibCBAccuracy)
	if !found {
		corners.Close()
		corners = gocv.NewMat()
		found = gocv.FindChessboardCorners(gray, patternSize, &corners,
			gocv.CalibCBAdaptiveThresh|gocv.CalibCBNormalizeImage|gocv.CalibCBFastCheck)
	}
	if !found {
		corners.Close()
		return gocv.Mat{}, fmt.Errorf("未检测到棋盘格角点，请确认棋盘格完整可见")
	}

	criteria := gocv.NewTermCriteria(gocv.Count|gocv.EPS, 30, 0.001)
	if err := gocv.CornerSubPix(gray, &corners, image.Pt(11, 11), image.Pt(-1, -1), criteria); err != nil {
		corners.Close()
		return gocv.Mat{}, fmt.Errorf("亚像素优化失败: %v", err)
	}

	return corners, nil
}

// ComputeCameraCalibration computes a camera intrinsic matrix from multiple chessboard poses.
func (c *Calibration) ComputeCameraCalibration(cornersList []gocv.Mat, patternSize image.Point, squareSize float64) (gocv.Mat, error) {
	if len(cornersList) == 0 {
		return gocv.Mat{}, fmt.Errorf("角点列表为空")
	}
	if patternSize.X <= 0 || patternSize.Y <= 0 {
		return gocv.Mat{}, fmt.Errorf("棋盘格内角点数量无效")
	}
	if squareSize <= 0 {
		return gocv.Mat{}, fmt.Errorf("棋盘格单格尺寸无效")
	}

	expected := patternSize.X * patternSize.Y
	imagePoints := gocv.NewPoints2fVector()
	defer imagePoints.Close()
	objectPoints := gocv.NewPoints3fVector()
	defer objectPoints.Close()

	imageSize := image.Point{}
	for i, corners := range cornersList {
		if corners.Empty() {
			return gocv.Mat{}, fmt.Errorf("第 %d 组角点数据为空", i+1)
		}

		pt2fVec := gocv.NewPoint2fVectorFromMat(corners)
		if pt2fVec.Size() != expected {
			size := pt2fVec.Size()
			pt2fVec.Close()
			return gocv.Mat{}, fmt.Errorf("第 %d 组角点数量为 %d，期望 %d", i+1, size, expected)
		}

		for j := 0; j < pt2fVec.Size(); j++ {
			pt := pt2fVec.At(j)
			imageSize.X = max(imageSize.X, int(math.Ceil(float64(pt.X)))+1)
			imageSize.Y = max(imageSize.Y, int(math.Ceil(float64(pt.Y)))+1)
		}

		imagePoints.Append(pt2fVec)
		pt2fVec.Close()

		objPoints := calibrationObjectPoints(patternSize, squareSize)
		objectPoints.Append(objPoints)
		objPoints.Close()
	}

	if len(cornersList) < 3 {
		return gocv.Mat{}, fmt.Errorf("至少需要 3 组不同姿态的角点")
	}
	if imageSize.X <= 0 || imageSize.Y <= 0 {
		return gocv.Mat{}, fmt.Errorf("无法从角点估算图像尺寸")
	}

	k := gocv.NewMat()
	d := gocv.NewMat()
	r := gocv.NewMat()
	t := gocv.NewMat()
	defer d.Close()
	defer r.Close()
	defer t.Close()

	gocv.CalibrateCamera(objectPoints, imagePoints, imageSize, &k, &d, &r, &t, 0)
	if k.Empty() {
		k.Close()
		return gocv.Mat{}, fmt.Errorf("相机标定失败")
	}

	return k, nil
}

// ComputePose computes a perspective transform matrix from one chessboard pose.
func (c *Calibration) ComputePose(corners gocv.Mat, patternSize image.Point, squareSize float64) (gocv.Mat, error) {
	if corners.Empty() {
		return gocv.Mat{}, fmt.Errorf("角点数据为空")
	}

	expected := patternSize.X * patternSize.Y
	pt2fVec := gocv.NewPoint2fVectorFromMat(corners)
	defer pt2fVec.Close()
	if pt2fVec.Size() < expected || expected < 4 {
		return gocv.Mat{}, fmt.Errorf("角点数量不足")
	}

	points := make([]gocv.Point2f, pt2fVec.Size())
	for i := 0; i < pt2fVec.Size(); i++ {
		points[i] = pt2fVec.At(i)
	}

	tl, tr, br, bl := visualQuadCorners(
		points[0],
		points[patternSize.X-1],
		points[expected-1],
		points[expected-patternSize.X],
	)

	topLen := distance2f(tl, tr)
	bottomLen := distance2f(bl, br)
	leftLen := distance2f(tl, bl)
	rightLen := distance2f(tr, br)

	targetWidth := averagePositive(topLen, bottomLen)
	targetHeight := averagePositive(leftLen, rightLen)
	if targetWidth < 1 {
		targetWidth = 1
	}
	if targetHeight < 1 {
		targetHeight = 1
	}

	centerX := (float64(tl.X) + float64(tr.X) + float64(br.X) + float64(bl.X)) / 4
	centerY := (float64(tl.Y) + float64(tr.Y) + float64(br.Y) + float64(bl.Y)) / 4
	halfW := targetWidth / 2
	halfH := targetHeight / 2

	srcVec := gocv.NewPointVectorFromPoints([]image.Point{
		image.Pt(int(math.Round(float64(tl.X))), int(math.Round(float64(tl.Y)))),
		image.Pt(int(math.Round(float64(tr.X))), int(math.Round(float64(tr.Y)))),
		image.Pt(int(math.Round(float64(br.X))), int(math.Round(float64(br.Y)))),
		image.Pt(int(math.Round(float64(bl.X))), int(math.Round(float64(bl.Y)))),
	})
	defer srcVec.Close()

	dstVec := gocv.NewPointVectorFromPoints([]image.Point{
		image.Pt(int(math.Round(centerX-halfW)), int(math.Round(centerY-halfH))),
		image.Pt(int(math.Round(centerX+halfW)), int(math.Round(centerY-halfH))),
		image.Pt(int(math.Round(centerX+halfW)), int(math.Round(centerY+halfH))),
		image.Pt(int(math.Round(centerX-halfW)), int(math.Round(centerY+halfH))),
	})
	defer dstVec.Close()

	return gocv.GetPerspectiveTransform(srcVec, dstVec), nil
}

// CorrectImage applies a perspective transform matrix to an image.
func (c *Calibration) CorrectImage(img gocv.Mat, transform gocv.Mat) (gocv.Mat, error) {
	if img.Empty() {
		return gocv.Mat{}, fmt.Errorf("输入图像为空")
	}
	if transform.Empty() {
		return gocv.Mat{}, fmt.Errorf("变换矩阵为空")
	}

	result := gocv.NewMat()
	if err := gocv.WarpPerspective(img, &result, transform, image.Pt(img.Cols(), img.Rows())); err != nil {
		result.Close()
		return gocv.Mat{}, err
	}
	return result, nil
}

func calibrationObjectPoints(patternSize image.Point, squareSize float64) gocv.Point3fVector {
	objPoints := gocv.NewPoint3fVector()
	for y := 0; y < patternSize.Y; y++ {
		for x := 0; x < patternSize.X; x++ {
			objPoints.Append(gocv.Point3f{
				X: float32(x) * float32(squareSize),
				Y: float32(y) * float32(squareSize),
				Z: 0,
			})
		}
	}
	return objPoints
}

func visualQuadCorners(corners ...gocv.Point2f) (gocv.Point2f, gocv.Point2f, gocv.Point2f, gocv.Point2f) {
	tl := corners[0]
	tr := corners[0]
	br := corners[0]
	bl := corners[0]

	minSum := float64(corners[0].X + corners[0].Y)
	maxSum := minSum
	minDiff := float64(corners[0].X - corners[0].Y)
	maxDiff := minDiff

	for _, pt := range corners[1:] {
		sum := float64(pt.X + pt.Y)
		diff := float64(pt.X - pt.Y)
		if sum < minSum {
			minSum = sum
			tl = pt
		}
		if sum > maxSum {
			maxSum = sum
			br = pt
		}
		if diff > maxDiff {
			maxDiff = diff
			tr = pt
		}
		if diff < minDiff {
			minDiff = diff
			bl = pt
		}
	}

	return tl, tr, br, bl
}

func averagePositive(values ...float64) float64 {
	var sum float64
	var count float64
	for _, value := range values {
		if value > 0 {
			sum += value
			count++
		}
	}
	if count == 0 {
		return 0
	}
	return sum / count
}

func distance2f(a, b gocv.Point2f) float64 {
	dx := float64(a.X - b.X)
	dy := float64(a.Y - b.Y)
	return math.Sqrt(dx*dx + dy*dy)
}
