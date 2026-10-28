package hik

import (
	"fmt"
	"image"
	"math"
	"sort"
	"sync"

	"gocv.io/x/gocv"
)

// Rect2f 浮点矩形
type Rect2f struct {
	X      float64
	Y      float64
	Width  float64
	Height float64
}

// TrackingAxis 追踪轴类型
type TrackingAxis int

const (
	TrackingAxisVertical TrackingAxis = iota
	TrackingAxisHorizontal
)

// DICFrameResult 单帧 DIC 分析结果（多点融合后）
type DICFrameResult struct {
	U            float64 // X 方向位移
	V            float64 // Y 方向位移
	Displacement float64 // 总位移幅值
	SubsetCount  int     // 有效追踪点数
}

// DICResult 累计 DIC 分析结果
type DICResult struct {
	FrameResults    []DICFrameResult
	Displacements   []float64
	MaxDisplacement float64
}

// ---------- IC-GN 引擎 ----------

// icgnEngine IC-GN 逆合成高斯-牛顿引擎
// 管理单个子集的参考数据（梯度、Hessian），支持多帧重复追踪
//
// IC-GN 核心优势：Hessian 矩阵只依赖参考子集的梯度（固定），可预计算一次，后续每帧只需迭代求解。
type icgnEngine struct {
	subsetSize int
	// 参考子集 ZNSSD 归一化值 (f - f_mean) / f_std
	refPixels []float64
	// 预计算的 Hessian 逆矩阵 [36]，行优先
	hessian [36]float64
	// 子集内各像素的 ξ 坐标 [pixel_index][0=x, 1=y]
	coords [][2]float64
	// 每个像素的 steepest descent 向量 [pixel_index][6]
	sdTable [][]float64
}

func newIcgnEngine(subsetSize int) *icgnEngine {
	nPixels := subsetSize * subsetSize
	return &icgnEngine{
		subsetSize: subsetSize,
		refPixels:  make([]float64, nPixels),
		coords:     make([][2]float64, nPixels),
		sdTable:    make([][]float64, nPixels),
	}
}

// initFromRoi 从参考图像 ROI 初始化 IC-GN 引擎
//
//	refRoi: subsetSize×subsetSize 灰度像素值
//	gradXRoi, gradYRoi: 对应区域的 Sobel 梯度值 (float32 → float64)
func (eng *icgnEngine) initFromRoi(refRoi, gradXRoi, gradYRoi []float64) bool {
	half := eng.subsetSize / 2
	nPixels := eng.subsetSize * eng.subsetSize

	// 1. 计算参考子集均值和标准差 → ZNSSD 归一化
	var mean float64
	for i := 0; i < nPixels; i++ {
		eng.refPixels[i] = refRoi[i]
		mean += refRoi[i]
	}
	mean /= float64(nPixels)

	var sumSq float64
	for i := 0; i < nPixels; i++ {
		d := refRoi[i] - mean
		sumSq += d * d
	}
	std := math.Sqrt(sumSq / float64(nPixels-1))
	if std < 1e-10 {
		return false
	}
	for i := 0; i < nPixels; i++ {
		eng.refPixels[i] = (refRoi[i] - mean) / std
	}

	// 2. 填充局部坐标 ξ = (dx, dy)
	idx := 0
	for dy := -half; dy <= half; dy++ {
		for dx := -half; dx <= half; dx++ {
			eng.coords[idx] = [2]float64{float64(dx), float64(dy)}
			idx++
		}
	}

	// 3. 预计算 Steepest Descent 图像和 Hessian 矩阵
	//    一阶仿射形函数的 Jacobian ∂W/∂p:
	//      [1, 0, ξ_x, ξ_y, 0,  0  ]
	//      [0, 1, 0,  0,  ξ_x, ξ_y]
	//    SD (6 维) = ∇f · ∂W/∂p :
	//      [∇f_x, ∇f_y, ∇f_x·ξ_x, ∇f_x·ξ_y, ∇f_y·ξ_x, ∇f_y·ξ_y]
	for i := 0; i < 36; i++ {
		eng.hessian[i] = 0
	}

	for i := 0; i < nPixels; i++ {
		gx := gradXRoi[i]
		gy := gradYRoi[i]
		xi := eng.coords[i][0]
		yi := eng.coords[i][1]

		sd := make([]float64, 6)
		sd[0] = gx
		sd[1] = gy
		sd[2] = gx * xi
		sd[3] = gx * yi
		sd[4] = gy * xi
		sd[5] = gy * yi
		eng.sdTable[i] = sd

		// H += SD^T · SD
		for r := 0; r < 6; r++ {
			for c := 0; c < 6; c++ {
				eng.hessian[r*6+c] += sd[r] * sd[c]
			}
		}
	}

	// 求 Hessian 逆矩阵
	return mat6Invert(eng.hessian[:])
}

type dicRefSubset struct {
	centerX    float64
	centerY    float64
	centerXi   int
	centerYi   int
	pixels     []float64
	normPixels []float64
	mean       float64
	std        float64
	template   *gocv.Mat
}

// ---------- 主 DIC 服务 ----------

// ExtensometerService DIC 引伸计服务
type ExtensometerService struct {
	originalImg *gocv.Mat // 参考图像（灰度）
	dicParam    *DICParam
	ROIF        Rect2f
	baseROIF    Rect2f
	axis        TrackingAxis
	result      *DICResult

	// 多点追踪状态
	mIsInitialized bool
	mInitialPoints [][2]float64 // 初始子集中心 (cx, cy)
	mEngines       []*icgnEngine
	mSubsets       []dicRefSubset
	// 上一帧的位移（用于预测）
	prevDx     float64
	prevDy     float64
	prevPrevDx float64
	prevPrevDy float64
	// 参考图像的 Sobel 梯度（全局只需算一次）
	refGradX *gocv.Mat
	refGradY *gocv.Mat
}

// NewExtensometerService 创建 DIC 引伸计服务
func NewExtensometerService() *ExtensometerService {
	return &ExtensometerService{
		dicParam: DefaultDICParam(),
	}
}

// SetOrignalImg 设置参考图像（自动转灰度），同时重置追踪状态
func (e *ExtensometerService) SetOrignalImg(originalImg gocv.Mat) {
	if originalImg.Empty() {
		return
	}

	grayImg := gocv.NewMat()
	defer grayImg.Close()
	if originalImg.Channels() > 1 {
		gocv.CvtColor(originalImg, &grayImg, gocv.ColorBGRToGray)
	} else {
		originalImg.CopyTo(&grayImg)
	}

	processed := e.PreprocessGrayForDIC(grayImg)
	defer processed.Close()

	if e.originalImg != nil {
		e.originalImg.Close()
	}
	cloned := processed.Clone()
	e.originalImg = &cloned

	e.Reset()
}

// PreprocessGrayForDIC applies the same denoise/sharpen pipeline used by DIC tracking.
func (e *ExtensometerService) PreprocessGrayForDIC(grayImg gocv.Mat) gocv.Mat {
	return preprocessGrayPipeline(grayImg, e.dicParam)
}

// PreprocessRegionsForDIC 仅对 frame 的指定区域做 DIC 预处理（其余像素保持原样），
// 返回整幅帧的克隆。区域会按搜索半径 + 子集半径 + 核尺寸外扩，保证 DIC 的
// 模板搜索窗口与亚像素精读范围全部落在已处理像素内。用于跳过全图模糊，
// 将预处理耗时从 ~9ms 降到 ~1ms。
func (e *ExtensometerService) PreprocessRegionsForDIC(frame gocv.Mat, regions []image.Rectangle) gocv.Mat {
	prepared := frame.Clone()
	if e.dicParam == nil || !e.dicParam.PreprocessEnabled || frame.Empty() {
		return prepared
	}

	kernel := oddKernel(e.dicParam.DenoiseKernel)
	margin := e.dicParam.SearchRadius + e.dicParam.SubsetSize/2 + kernel + 32
	for _, r := range regions {
		rect := expandClampRect(r, margin, frame.Cols(), frame.Rows())
		if rect.Dx() <= 0 || rect.Dy() <= 0 {
			continue
		}
		sub := prepared.Region(rect)
		processed := preprocessGrayPipeline(sub, e.dicParam)
		processed.CopyTo(&sub)
		processed.Close()
		sub.Close()
	}
	return prepared
}

// DIC 预处理方法常量
const (
	DICPreprocessNone      = "none"      // 不处理
	DICPreprocessGauss     = "gauss"     // 高斯去噪 + 可选锐化（默认）
	DICPreprocessBilateral = "bilateral" // 双边滤波（保边降噪）+ 可选锐化
	DICPreprocessNLM       = "nlm"       // 非局部均值降噪
	DICPreprocessHighPass  = "highpass"  // 光照平场化（去除缓慢光照变化）
)

// DICPreprocessMethods 返回支持的预处理方法列表
func DICPreprocessMethods() []string {
	return []string{DICPreprocessNone, DICPreprocessGauss, DICPreprocessBilateral, DICPreprocessNLM, DICPreprocessHighPass}
}

// preprocessGrayPipeline DIC 预处理管线：按 PreprocessMethod 分派
func preprocessGrayPipeline(grayImg gocv.Mat, param *DICParam) gocv.Mat {
	if param == nil || !param.PreprocessEnabled || grayImg.Empty() {
		return grayImg.Clone()
	}

	switch param.PreprocessMethod {
	case DICPreprocessNone:
		return grayImg.Clone()

	case DICPreprocessBilateral:
		// 双边滤波：降噪同时保留斑点边缘
		denoised := gocv.NewMat()
		gocv.BilateralFilter(grayImg, &denoised, oddKernel(param.DenoiseKernel)+2, 30, 5)
		return applySharpen(denoised, param)

	case DICPreprocessNLM:
		// 非局部均值：降噪质量最好，计算量较大（仅 ROI 区域内使用）
		denoised := gocv.NewMat()
		gocv.FastNlMeansDenoising(grayImg, &denoised)
		return denoised

	case DICPreprocessHighPass:
		// 光照平场化：减去大核模糊背景再抬回中灰，消除缓慢光照不均与漂移
		bg := gocv.NewMat()
		gocv.GaussianBlur(grayImg, &bg, image.Pt(51, 51), 0, 0, gocv.BorderDefault)
		flat := gocv.NewMat()
		gocv.AddWeighted(grayImg, 1, bg, -1, 128, &flat)
		bg.Close()
		return flat

	default: // gauss
		kernel := oddKernel(param.DenoiseKernel)
		denoised := gocv.NewMat()
		gocv.GaussianBlur(grayImg, &denoised, image.Pt(kernel, kernel), 0, 0, gocv.BorderDefault)
		return applySharpen(denoised, param)
	}
}

// applySharpen 反锐化掩模（SharpenAmount<=0 时原样返回）
func applySharpen(denoised gocv.Mat, param *DICParam) gocv.Mat {
	if param.SharpenAmount <= 0 {
		return denoised
	}
	kernel := oddKernel(param.DenoiseKernel)
	blurred := gocv.NewMat()
	defer blurred.Close()
	gocv.GaussianBlur(denoised, &blurred, image.Pt(kernel, kernel), 0, 0, gocv.BorderDefault)

	sharpened := gocv.NewMat()
	_ = gocv.AddWeighted(denoised, 1+param.SharpenAmount, blurred, -param.SharpenAmount, 0, &sharpened)
	denoised.Close()
	return sharpened
}

// expandClampRect 矩形按 margin 外扩并裁剪到图像范围内
func expandClampRect(r image.Rectangle, margin, cols, rows int) image.Rectangle {
	x0 := r.Min.X - margin
	y0 := r.Min.Y - margin
	x1 := r.Max.X + margin
	y1 := r.Max.Y + margin
	if x0 < 0 {
		x0 = 0
	}
	if y0 < 0 {
		y0 = 0
	}
	if x1 > cols {
		x1 = cols
	}
	if y1 > rows {
		y1 = rows
	}
	return image.Rect(x0, y0, x1, y1)
}

func (e *ExtensometerService) preprocessGrayForDIC(grayImg gocv.Mat) gocv.Mat {
	return e.PreprocessGrayForDIC(grayImg)
}

func oddKernel(k int) int {
	if k < 3 {
		return 3
	}
	if k%2 == 0 {
		return k + 1
	}
	return k
}

func (e *ExtensometerService) computeRefGradients() {
	if e.originalImg == nil {
		return
	}
	if e.refGradX != nil {
		e.refGradX.Close()
	}
	if e.refGradY != nil {
		e.refGradY.Close()
	}
	gradX := gocv.NewMat()
	gradY := gocv.NewMat()
	gocv.Sobel(*e.originalImg, &gradX, gocv.MatTypeCV32F, 1, 0, 3, 1, 0, gocv.BorderDefault)
	gocv.Sobel(*e.originalImg, &gradY, gocv.MatTypeCV32F, 0, 1, 3, 1, 0, gocv.BorderDefault)
	e.refGradX = &gradX
	e.refGradY = &gradY
}

// SetDICParam 设置 DIC 参数
func (e *ExtensometerService) SetDICParam(param *DICParam) {
	e.dicParam = param
}

// SetROI 设置感兴趣区域
func (e *ExtensometerService) SetROI(roiRect Rect2f) {
	e.ROIF = roiRect
	e.baseROIF = roiRect
	e.Reset()
}

func (e *ExtensometerService) SetTrackingAxis(axis TrackingAxis) {
	if e.axis == axis {
		return
	}
	e.axis = axis
	e.Reset()
}

func (e *ExtensometerService) CurrentROI() Rect2f {
	return e.ROIF
}

// ---------- 多点阵列初始化 ----------

// initMultiPoints 初始化多点追踪阵列
// 沿 ROI 中心线垂直方向连续分布追踪点，确保每个 subset 完整落在 ROI 内。
func (e *ExtensometerService) initMultiPoints() bool {
	if e.originalImg == nil || e.originalImg.Empty() {
		return false
	}

	e.computeRefGradients()
	if e.refGradX == nil || e.refGradY == nil {
		return false
	}

	imgW := e.originalImg.Cols()
	imgH := e.originalImg.Rows()
	refBytes := e.originalImg.ToBytes()

	cx := e.ROIF.X + e.ROIF.Width/2
	cy := e.ROIF.Y + e.ROIF.Height/2
	halfSize := e.dicParam.SubsetSize / 2
	spacing := e.dicParam.Spacing
	if spacing < 1 {
		spacing = 1
	}

	e.mInitialPoints = nil
	e.mEngines = nil
	e.mSubsets = nil

	addPoint := func(px, py float64) {
		if !subsetInside(px, py, halfSize, imgW, imgH) {
			return
		}

		pixels, mean, std, ok := sampleSubset(refBytes, imgW, imgH, px, py, e.dicParam.SubsetSize)
		if !ok || std < 1e-10 {
			return
		}
		centerXi := int(math.Round(px))
		centerYi := int(math.Round(py))
		templateROI := e.originalImg.Region(image.Rect(centerXi-halfSize, centerYi-halfSize, centerXi+halfSize+1, centerYi+halfSize+1))
		templateMat := templateROI.Clone()
		templateROI.Close()
		gradX := sampleFloat32MatSubset(e.refGradX, centerXi, centerYi, e.dicParam.SubsetSize)
		gradY := sampleFloat32MatSubset(e.refGradY, centerXi, centerYi, e.dicParam.SubsetSize)
		engine := newIcgnEngine(e.dicParam.SubsetSize)
		if !engine.initFromRoi(pixels, gradX, gradY) {
			templateMat.Close()
			return
		}

		normPixels := make([]float64, len(pixels))
		for j, v := range pixels {
			normPixels[j] = (v - mean) / std
		}

		e.mInitialPoints = append(e.mInitialPoints, [2]float64{px, py})
		e.mEngines = append(e.mEngines, engine)
		e.mSubsets = append(e.mSubsets, dicRefSubset{
			centerX:    px,
			centerY:    py,
			centerXi:   centerXi,
			centerYi:   centerYi,
			pixels:     pixels,
			normPixels: normPixels,
			mean:       mean,
			std:        std,
			template:   &templateMat,
		})
	}

	if e.axis == TrackingAxisHorizontal {
		startX := int(math.Ceil(e.ROIF.X)) + halfSize
		endX := int(math.Floor(e.ROIF.X+e.ROIF.Width)) - halfSize
		for pxInt := startX; pxInt <= endX; pxInt += spacing {
			addPoint(float64(pxInt), cy)
		}
	} else {
		startY := int(math.Ceil(e.ROIF.Y)) + halfSize
		endY := int(math.Floor(e.ROIF.Y+e.ROIF.Height)) - halfSize
		for pyInt := startY; pyInt <= endY; pyInt += spacing {
			addPoint(cx, float64(pyInt))
		}
	}

	if len(e.mSubsets) == 0 {
		return false
	}
	e.mIsInitialized = true
	return true
}

func subsetInside(cx, cy float64, halfSize, width, height int) bool {
	margin := float64(halfSize + 2)
	return cx-margin >= 0 && cy-margin >= 0 && cx+margin < float64(width) && cy+margin < float64(height)
}

func sampleSubset(img []uint8, width, height int, cx, cy float64, subsetSize int) ([]float64, float64, float64, bool) {
	half := subsetSize / 2
	pixels := make([]float64, subsetSize*subsetSize)
	var sum float64
	idx := 0
	for dy := -half; dy <= half; dy++ {
		for dx := -half; dx <= half; dx++ {
			x := cx + float64(dx)
			y := cy + float64(dy)
			if x < 1 || x >= float64(width-2) || y < 1 || y >= float64(height-2) {
				return nil, 0, 0, false
			}
			v := bicubicSampleCR(img, width, height, x, y)
			pixels[idx] = v
			sum += v
			idx++
		}
	}

	mean := sum / float64(len(pixels))
	var sumSq float64
	for _, v := range pixels {
		d := v - mean
		sumSq += d * d
	}
	std := math.Sqrt(sumSq / float64(len(pixels)-1))
	return pixels, mean, std, true
}

func sampleFloat32MatSubset(mat *gocv.Mat, cx, cy, subsetSize int) []float64 {
	half := subsetSize / 2
	values := make([]float64, subsetSize*subsetSize)
	idx := 0
	for dy := -half; dy <= half; dy++ {
		for dx := -half; dx <= half; dx++ {
			values[idx] = float64(mat.GetFloatAt(cy+dy, cx+dx))
			idx++
		}
	}
	return values
}

func subsetZNCC(defBytes []uint8, defW, defH int, ref dicRefSubset, dx, dy float64, subsetSize int) float64 {
	if ref.std < 1e-10 {
		return -1
	}

	half := subsetSize / 2
	cx := ref.centerXi + int(dx)
	cy := ref.centerYi + int(dy)
	if cx-half < 0 || cy-half < 0 || cx+half >= defW || cy+half >= defH {
		return -1
	}

	nPixels := subsetSize * subsetSize
	var sum float64
	for sy := -half; sy <= half; sy++ {
		row := (cy + sy) * defW
		for sx := -half; sx <= half; sx++ {
			sum += float64(defBytes[row+cx+sx])
		}
	}
	mean := sum / float64(nPixels)

	var sumSq float64
	var corr float64
	idx := 0
	for sy := -half; sy <= half; sy++ {
		row := (cy + sy) * defW
		for sx := -half; sx <= half; sx++ {
			d := float64(defBytes[row+cx+sx]) - mean
			sumSq += d * d
			corr += ref.normPixels[idx] * d
			idx++
		}
	}

	std := math.Sqrt(sumSq / float64(nPixels-1))
	if std < 1e-10 {
		return -1
	}
	return corr / (std * float64(nPixels))
}

func quadraticPeakOffset(left, center, right float64) float64 {
	den := left - 2*center + right
	if math.Abs(den) < 1e-12 {
		return 0
	}
	offset := 0.5 * (left - right) / den
	if offset < -1 || offset > 1 || math.IsNaN(offset) || math.IsInf(offset, 0) {
		return 0
	}
	return offset
}

func searchSubsetDisplacement(defBytes []uint8, defW, defH int, ref dicRefSubset, predDx, predDy float64, param *DICParam) (float64, float64, float64, bool) {
	baseDx := int(math.Round(predDx))
	baseDy := int(math.Round(predDy))

	search := func(radius int) (float64, int, int) {
		bestScore := -1.0
		bestDx := baseDx
		bestDy := baseDy

		for dy := baseDy - radius; dy <= baseDy+radius; dy++ {
			for dx := baseDx - radius; dx <= baseDx+radius; dx++ {
				score := subsetZNCC(defBytes, defW, defH, ref, float64(dx), float64(dy), param.SubsetSize)
				if score > bestScore {
					bestScore = score
					bestDx = dx
					bestDy = dy
				}
			}
		}
		return bestScore, bestDx, bestDy
	}

	radius := effectiveSearchRadius(param, predDx, predDy)
	bestScore, bestDx, bestDy := search(radius)
	if bestScore < param.ZNCCThreshold && radius < param.SearchRadius {
		bestScore, bestDx, bestDy = search(param.SearchRadius)
	}
	if bestScore < param.ZNCCThreshold {
		return 0, 0, bestScore, false
	}

	center := bestScore
	left := subsetZNCC(defBytes, defW, defH, ref, float64(bestDx-1), float64(bestDy), param.SubsetSize)
	right := subsetZNCC(defBytes, defW, defH, ref, float64(bestDx+1), float64(bestDy), param.SubsetSize)
	up := subsetZNCC(defBytes, defW, defH, ref, float64(bestDx), float64(bestDy-1), param.SubsetSize)
	down := subsetZNCC(defBytes, defW, defH, ref, float64(bestDx), float64(bestDy+1), param.SubsetSize)

	subDx := quadraticPeakOffset(left, center, right)
	subDy := quadraticPeakOffset(up, center, down)
	return float64(bestDx) + subDx, float64(bestDy) + subDy, bestScore, true
}

func effectiveSearchRadius(param *DICParam, predDx, predDy float64) int {
	if param.TrackingSearchRadius <= 0 || param.TrackingSearchRadius >= param.SearchRadius {
		return param.SearchRadius
	}
	if predDx == 0 && predDy == 0 {
		return param.SearchRadius
	}
	return param.TrackingSearchRadius
}

func searchSubsetDisplacementMat(defMat gocv.Mat, ref dicRefSubset, predDx, predDy float64, param *DICParam) (float64, float64, float64, bool) {
	if ref.template == nil || ref.template.Empty() {
		return 0, 0, -1, false
	}

	baseDx := int(math.Round(predDx))
	baseDy := int(math.Round(predDy))
	half := param.SubsetSize / 2
	mask := gocv.NewMat()
	defer mask.Close()

	search := func(radius int) (float64, float64, float64, bool) {
		centerX := ref.centerXi + baseDx
		centerY := ref.centerYi + baseDy
		searchX := centerX - half - radius
		searchY := centerY - half - radius
		searchW := param.SubsetSize + 2*radius
		searchH := param.SubsetSize + 2*radius

		if searchX < 0 {
			searchW += searchX
			searchX = 0
		}
		if searchY < 0 {
			searchH += searchY
			searchY = 0
		}
		if searchX+searchW > defMat.Cols() {
			searchW = defMat.Cols() - searchX
		}
		if searchY+searchH > defMat.Rows() {
			searchH = defMat.Rows() - searchY
		}
		if searchW < param.SubsetSize || searchH < param.SubsetSize {
			return -1, 0, 0, false
		}

		roi := defMat.Region(image.Rect(searchX, searchY, searchX+searchW, searchY+searchH))
		defer roi.Close()

		result := gocv.NewMat()
		defer result.Close()
		if err := gocv.MatchTemplate(roi, *ref.template, &result, gocv.TmCcoeffNormed, mask); err != nil {
			return -1, 0, 0, false
		}

		_, maxVal, _, maxLoc := gocv.MinMaxLoc(result)
		bestDx := float64(searchX + maxLoc.X + half - ref.centerXi)
		bestDy := float64(searchY + maxLoc.Y + half - ref.centerYi)
		if maxLoc.X > 0 && maxLoc.X+1 < result.Cols() {
			left := float64(result.GetFloatAt(maxLoc.Y, maxLoc.X-1))
			right := float64(result.GetFloatAt(maxLoc.Y, maxLoc.X+1))
			bestDx += quadraticPeakOffset(left, float64(maxVal), right)
		}
		if maxLoc.Y > 0 && maxLoc.Y+1 < result.Rows() {
			up := float64(result.GetFloatAt(maxLoc.Y-1, maxLoc.X))
			down := float64(result.GetFloatAt(maxLoc.Y+1, maxLoc.X))
			bestDy += quadraticPeakOffset(up, float64(maxVal), down)
		}
		return float64(maxVal), bestDx, bestDy, true
	}

	radius := effectiveSearchRadius(param, predDx, predDy)
	bestScore, bestDx, bestDy, ok := search(radius)
	if (!ok || bestScore < param.ZNCCThreshold) && radius < param.SearchRadius {
		bestScore, bestDx, bestDy, ok = search(param.SearchRadius)
	}
	if !ok || bestScore < param.ZNCCThreshold {
		return 0, 0, bestScore, false
	}

	return float64(bestDx), float64(bestDy), bestScore, true
}

func refineSubsetDisplacementDIC(defBytes []uint8, defW, defH int, ref dicRefSubset, engine *icgnEngine, coarseDx, coarseDy, coarseScore float64, param *DICParam) (float64, float64, float64, bool) {
	if engine == nil {
		return coarseDx, coarseDy, coarseScore, coarseScore >= param.ZNCCThreshold
	}

	p := []float64{coarseDx, coarseDy, 0, 0, 0, 0}
	score, converged := icgnRefineFull(defBytes, defW, defH, ref.centerX, ref.centerY, p, engine, param.MaxIter, param.ConvergeEps)
	if converged && score >= param.ZNCCThreshold && !math.IsNaN(p[0]) && !math.IsNaN(p[1]) && !math.IsInf(p[0], 0) && !math.IsInf(p[1], 0) {
		return p[0], p[1], score, true
	}

	return coarseDx, coarseDy, coarseScore, coarseScore >= param.ZNCCThreshold
}

// ---------- ZNCC 整像素粗搜索 ----------

// integerSearch 整像素 ZNCC 模板匹配粗搜索（带位移预测）
// 返回: (dx, dy, ZNCC_score)
func (e *ExtensometerService) integerSearch(defBytes []uint8, defW, defH int,
	cxRef, cyRef float64, predDx, predDy float64) (float64, float64, float64) {

	halfSize := e.dicParam.SubsetSize / 2
	searchR := e.dicParam.SearchRadius

	// 预测中心 = 参考中心 + 上一帧位移
	predCx := cxRef + predDx
	predCy := cyRef + predDy

	// 搜索区域边界
	searchX := int(math.Round(predCx)) - searchR - halfSize
	searchY := int(math.Round(predCy)) - searchR - halfSize
	searchW := e.dicParam.SubsetSize + 2*searchR
	searchH := e.dicParam.SubsetSize + 2*searchR

	// 边界裁剪（对应 C++ searchRect &= cv::Rect(0,0,w,h)）
	if searchX < 0 {
		searchX = 0
	}
	if searchY < 0 {
		searchY = 0
	}
	if searchX+searchW > defW {
		searchW = defW - searchX
	}
	if searchY+searchH > defH {
		searchH = defH - searchY
	}
	if searchW < e.dicParam.SubsetSize || searchH < e.dicParam.SubsetSize {
		return 0, 0, 0
	}

	refBytes := e.originalImg.ToBytes()
	refW := e.originalImg.Cols()
	nPixels := e.dicParam.SubsetSize * e.dicParam.SubsetSize

	// 参考子集 ZNSSD 归一化
	var refMean float64
	for dy := -halfSize; dy <= halfSize; dy++ {
		for dx := -halfSize; dx <= halfSize; dx++ {
			xi := int(math.Round(cxRef)) + dx
			yi := int(math.Round(cyRef)) + dy
			refMean += float64(refBytes[yi*refW+xi])
		}
	}
	refMean /= float64(nPixels)

	var refSumSq float64
	for dy := -halfSize; dy <= halfSize; dy++ {
		for dx := -halfSize; dx <= halfSize; dx++ {
			xi := int(math.Round(cxRef)) + dx
			yi := int(math.Round(cyRef)) + dy
			d := float64(refBytes[yi*refW+xi]) - refMean
			refSumSq += d * d
		}
	}
	refStd := math.Sqrt(refSumSq / float64(nPixels-1))
	if refStd < 1e-10 {
		return 0, 0, 0
	}

	refNorm := make([]float64, nPixels)
	for dy := -halfSize; dy <= halfSize; dy++ {
		for dx := -halfSize; dx <= halfSize; dx++ {
			xi := int(math.Round(cxRef)) + dx
			yi := int(math.Round(cyRef)) + dy
			v := float64(refBytes[yi*refW+xi])
			refNorm[(dy+halfSize)*e.dicParam.SubsetSize+(dx+halfSize)] = (v - refMean) / refStd
		}
	}

	// 滑动窗口计算 ZNCC
	bestScore := -1.0
	bestDx := 0.0
	bestDy := 0.0

	maxSY := searchY + searchH - e.dicParam.SubsetSize
	maxSX := searchX + searchW - e.dicParam.SubsetSize

	for sy := searchY; sy <= maxSY; sy++ {
		for sx := searchX; sx <= maxSX; sx++ {
			var sMean, sSumSq float64
			for dy := 0; dy < e.dicParam.SubsetSize; dy++ {
				for dx := 0; dx < e.dicParam.SubsetSize; dx++ {
					v := float64(defBytes[(sy+dy)*defW+(sx+dx)])
					sMean += v
					sSumSq += v * v
				}
			}
			sMean /= float64(nPixels)
			sVar := sSumSq/float64(nPixels) - sMean*sMean
			sStd := math.Sqrt(sVar * float64(nPixels) / float64(nPixels-1))
			if sStd < 1e-10 {
				continue
			}

			var corr float64
			for dy := 0; dy < e.dicParam.SubsetSize; dy++ {
				for dx := 0; dx < e.dicParam.SubsetSize; dx++ {
					v := float64(defBytes[(sy+dy)*defW+(sx+dx)])
					sNorm := (v - sMean) / sStd
					corr += refNorm[dy*e.dicParam.SubsetSize+dx] * sNorm
				}
			}
			corr /= float64(nPixels)

			if corr > bestScore {
				bestScore = corr
				bestDx = float64(sx+halfSize) - cxRef
				bestDy = float64(sy+halfSize) - cyRef
			}
		}
	}

	if bestScore < e.dicParam.ZNCCThreshold {
		return 0, 0, 0
	}
	return bestDx, bestDy, bestScore
}

// ---------- IC-GN 亚像素精搜索 ----------

// icgnRefineFull IC-GN 逆合成高斯-牛顿亚像素精搜索
//
// 形函数: 一阶仿射 (6 DOF)
//
//	W(ξ; p) = [u + du/dx·ξ_x + du/dy·ξ_y,
//	           v + dv/dx·ξ_x + dv/dy·ξ_y]
//	其中 p = [u, v, du/dx, du/dy, dv/dx, dv/dy]
//
// IC-GN 迭代公式（逆合成）:
//  1. 采样 g_warped(ξ) = g(W(ξ; p)) 变形图像亚像素值
//  2. ZNSSD 残差 r(ξ) = (g - g_mean)/g_std - (f - f_mean)/f_std
//  3. Δp = H^(-1) · Σ[SD^T · r]
//  4. p ← p - Δp  (逆合成更新)
//  5. 收敛检查 ||Δp[0:1]|| < eps
//
// 返回: (ZNCC 评分, 是否收敛)
func icgnRefineFull(defBytes []uint8, defW, defH int,
	cxRef, cyRef float64, p []float64,
	eng *icgnEngine, maxIter int, eps float64) (float64, bool) {

	nPixels := eng.subsetSize * eng.subsetSize
	refPixels := eng.refPixels // ZNSSD 归一化值 (固定)
	gWarped := make([]float64, nPixels)

	for iter := 0; iter < maxIter; iter++ {
		// 1. 变形图像亚像素采样 g(W(ξ; p))
		var gSum, gSumSq float64
		validCount := 0

		for i := 0; i < nPixels; i++ {
			xi := eng.coords[i][0]
			yi := eng.coords[i][1]
			xDef := cxRef + xi + p[0] + p[2]*xi + p[3]*yi
			yDef := cyRef + yi + p[1] + p[4]*xi + p[5]*yi

			if xDef < 1 || xDef >= float64(defW-2) || yDef < 1 || yDef >= float64(defH-2) {
				gWarped[i] = 0
				continue
			}
			gVal := bicubicSampleCR(defBytes, defW, defH, xDef, yDef)
			gWarped[i] = gVal
			gSum += gVal
			gSumSq += gVal * gVal
			validCount++
		}

		if validCount < nPixels/2 {
			return 0, false
		}

		// 2. ZNSSD 归一化
		gMean := gSum / float64(validCount)
		gVar := gSumSq/float64(validCount) - gMean*gMean
		gStd := math.Sqrt(gVar * float64(validCount) / float64(validCount-1))
		if gStd < 1e-10 {
			return 0, false
		}

		// 3. 计算 ZNSSD 残差误差向量
		//    r(ξ) = (g - g_mean)/g_std - f_znssd(ξ)
		//    errVec[j] += SD[j](ξ) · r(ξ)
		var errVec [6]float64
		for i := 0; i < nPixels; i++ {
			if gWarped[i] == 0 {
				continue
			}
			gNorm := (gWarped[i] - gMean) / gStd
			r := gNorm - refPixels[i]
			sd := eng.sdTable[i]
			for j := 0; j < 6; j++ {
				errVec[j] += sd[j] * r
			}
		}

		// 4. Δp = H^(-1) · errVec
		var delta [6]float64
		for r := 0; r < 6; r++ {
			for c := 0; c < 6; c++ {
				delta[r] += eng.hessian[r*6+c] * errVec[c]
			}
		}

		// 5. 逆合成更新: p ← p - Δp（含阻尼防止发散）
		transNorm := math.Sqrt(delta[0]*delta[0] + delta[1]*delta[1])
		const maxStep = 2.0
		if transNorm > maxStep {
			scale := maxStep / transNorm
			for j := 0; j < 6; j++ {
				delta[j] *= scale
			}
		}
		for j := 0; j < 6; j++ {
			p[j] -= delta[j]
		}

		// 6. 收敛检查（平移分量）
		if transNorm < eps {
			zncc := computeZNCC(defBytes, defW, defH, cxRef, cyRef, p, eng.coords, refPixels, eng.subsetSize)
			return zncc, true
		}
	}

	zncc := computeZNCC(defBytes, defW, defH, cxRef, cyRef, p, eng.coords, refPixels, eng.subsetSize)
	return zncc, false
}

// computeZNCC 计算 ZNCC 相关系数（IC-GN 收敛后的质量评价）
func computeZNCC(defBytes []uint8, defW, defH int,
	cxRef, cyRef float64, p []float64,
	coords [][2]float64, refPixels []float64, subsetSize int) float64 {

	nPixels := subsetSize * subsetSize
	gWarped := make([]float64, nPixels)
	var gSum, gSumSq float64
	validCount := 0

	for i := 0; i < nPixels; i++ {
		xi := coords[i][0]
		yi := coords[i][1]
		xDef := cxRef + xi + p[0] + p[2]*xi + p[3]*yi
		yDef := cyRef + yi + p[1] + p[4]*xi + p[5]*yi
		if xDef < 1 || xDef >= float64(defW-2) || yDef < 1 || yDef >= float64(defH-2) {
			continue
		}
		gVal := bicubicSampleCR(defBytes, defW, defH, xDef, yDef)
		gWarped[i] = gVal
		gSum += gVal
		gSumSq += gVal * gVal
		validCount++
	}

	if validCount < nPixels/2 {
		return 0
	}
	gMean := gSum / float64(validCount)
	gVar := gSumSq/float64(validCount) - gMean*gMean
	gStd := math.Sqrt(gVar * float64(validCount) / float64(validCount-1))
	if gStd < 1e-10 {
		return 0
	}

	var corr float64
	count := 0
	for i := 0; i < nPixels; i++ {
		if gWarped[i] == 0 {
			continue
		}
		gNorm := (gWarped[i] - gMean) / gStd
		corr += refPixels[i] * gNorm
		count++
	}
	if count == 0 {
		return 0
	}
	return corr / float64(count)
}

// ---------- 双三次插值 ----------

// bicubicSampleCR Catmull-Rom 样条双三次插值 (a = -0.5)
func bicubicSampleCR(img []uint8, width, height int, x, y float64) float64 {
	ix := int(math.Floor(x))
	iy := int(math.Floor(y))
	fx := x - float64(ix)
	fy := y - float64(iy)

	if ix-1 < 0 || ix+2 >= width || iy-1 < 0 || iy+2 >= height {
		return bilinearSample(img, width, height, x, y)
	}

	colVals := make([]float64, 4)
	for j := 0; j < 4; j++ {
		py := iy - 1 + j
		p0 := float64(img[py*width+(ix-1)])
		p1 := float64(img[py*width+ix])
		p2 := float64(img[py*width+(ix+1)])
		p3 := float64(img[py*width+(ix+2)])
		colVals[j] = catmullRom(p0, p1, p2, p3, fx)
	}
	return catmullRom(colVals[0], colVals[1], colVals[2], colVals[3], fy)
}

// catmullRom Catmull-Rom 三次插值
func catmullRom(p0, p1, p2, p3, t float64) float64 {
	t2 := t * t
	t3 := t2 * t
	return 0.5 * ((2 * p1) +
		(-p0+p2)*t +
		(2*p0-5*p1+4*p2-p3)*t2 +
		(-p0+3*p1-3*p2+p3)*t3)
}

// bilinearSample 双线性插值（边界回退方案）
func bilinearSample(img []uint8, width, height int, x, y float64) float64 {
	ix := int(math.Floor(x))
	iy := int(math.Floor(y))
	fx := x - float64(ix)
	fy := y - float64(iy)
	ix1 := minInt(ix+1, width-1)
	iy1 := minInt(iy+1, height-1)

	v00 := float64(img[iy*width+ix])
	v10 := float64(img[iy*width+ix1])
	v01 := float64(img[iy1*width+ix])
	v11 := float64(img[iy1*width+ix1])
	v0 := v00*(1-fx) + v10*fx
	v1 := v01*(1-fx) + v11*fx
	return v0*(1-fy) + v1*fy
}

// ---------- 6x6 矩阵运算 ----------

// mat6Invert 6x6 矩阵求逆（高斯-约当消元），结果原地替换
func mat6Invert(m []float64) bool {
	n := 6
	aug := make([]float64, 72) // 6 x 12
	for i := 0; i < n; i++ {
		for j := 0; j < n; j++ {
			aug[i*2*n+j] = m[i*n+j]
		}
		aug[i*2*n+n+i] = 1
	}

	for col := 0; col < n; col++ {
		pivot := col
		for row := col + 1; row < n; row++ {
			if math.Abs(aug[row*2*n+col]) > math.Abs(aug[pivot*2*n+col]) {
				pivot = row
			}
		}
		if math.Abs(aug[pivot*2*n+col]) < 1e-15 {
			return false
		}
		if pivot != col {
			for j := 0; j < 2*n; j++ {
				aug[col*2*n+j], aug[pivot*2*n+j] = aug[pivot*2*n+j], aug[col*2*n+j]
			}
		}
		pivotVal := aug[col*2*n+col]
		for j := 0; j < 2*n; j++ {
			aug[col*2*n+j] /= pivotVal
		}
		for row := 0; row < n; row++ {
			if row != col {
				factor := aug[row*2*n+col]
				for j := 0; j < 2*n; j++ {
					aug[row*2*n+j] -= factor * aug[col*2*n+j]
				}
			}
		}
	}

	for i := 0; i < n; i++ {
		for j := 0; j < n; j++ {
			m[i*n+j] = aug[i*2*n+n+j]
		}
	}
	return true
}

// ---------- 主追踪逻辑 ----------

// calculateDisplacement 核心多点 DIC 追踪逻辑（对应 C++ 版 calculateDisplacement）
//
// 流程:
//  1. 首次调用 → 初始化多点阵列（沿 ROI 中心线连续分布点）
//  2. 对每个点:
//     a. 整像素 ZNCC 粗搜索（带位移预测，对应 C++ 步骤 A）
//     b. IC-GN 亚像素精搜索（一阶仿射 6 DOF，对应 C++ 步骤 B）
//  3. 中值融合剔除离群点（对应 C++ 步骤 3）
func (e *ExtensometerService) calculateDisplacement(defBytes []uint8, defW, defH int) *DICFrameResult {
	if !e.mIsInitialized {
		if !e.initMultiPoints() {
			return nil
		}
	}
	if len(e.mSubsets) == 0 {
		return nil
	}

	uList := make([]float64, 0, len(e.mSubsets))
	vList := make([]float64, 0, len(e.mSubsets))

	for _, ref := range e.mSubsets {
		dx, dy, score, ok := searchSubsetDisplacement(defBytes, defW, defH, ref, e.prevDx, e.prevDy, e.dicParam)
		if !ok || score < e.dicParam.ZNCCThreshold || math.IsNaN(dx) || math.IsNaN(dy) {
			continue
		}
		uList = append(uList, dx)
		vList = append(vList, dy)
	}

	if len(uList) == 0 {
		return nil
	}

	uMedian := medianFloat64(uList)
	vMedian := medianFloat64(vList)

	e.prevDx = uMedian
	e.prevDy = vMedian

	return &DICFrameResult{
		U:            uMedian,
		V:            vMedian,
		Displacement: math.Sqrt(uMedian*uMedian + vMedian*vMedian),
		SubsetCount:  len(uList),
	}
}

func (e *ExtensometerService) calculateDisplacementMat(defMat gocv.Mat) *DICFrameResult {
	return e.calculateDisplacementMatShared(defMat, defMat.ToBytes())
}

// calculateDisplacementMatShared 多点 DIC 追踪核心，defBytes 为 defMat 的灰度字节缓冲
func (e *ExtensometerService) calculateDisplacementMatShared(defMat gocv.Mat, defBytes []uint8) *DICFrameResult {
	if !e.mIsInitialized {
		if !e.initMultiPoints() {
			return nil
		}
	}
	if len(e.mSubsets) == 0 {
		return nil
	}

	type subsetResult struct {
		dx, dy float64
		ok     bool
	}
	results := make([]subsetResult, len(e.mSubsets))
	defW := defMat.Cols()
	defH := defMat.Rows()

	// 线性外推预测：pred = 2*prev - prevPrev，提供更平滑的追踪初始估计
	predDx := e.prevDx
	predDy := e.prevDy
	if e.prevPrevDx != 0 || e.prevPrevDy != 0 {
		predDx = 2*e.prevDx - e.prevPrevDx
		predDy = 2*e.prevDy - e.prevPrevDy
	}

	var wg sync.WaitGroup
	for i, ref := range e.mSubsets {
		wg.Add(1)
		go func(i int, ref dicRefSubset) {
			defer wg.Done()
			dx, dy, score, ok := searchSubsetDisplacementMat(defMat, ref, predDx, predDy, e.dicParam)
			if ok {
				var engine *icgnEngine
				if i < len(e.mEngines) {
					engine = e.mEngines[i]
				}
				dx, dy, score, ok = refineSubsetDisplacementDIC(defBytes, defW, defH, ref, engine, dx, dy, score, e.dicParam)
			}
			if !ok || score < e.dicParam.ZNCCThreshold || math.IsNaN(dx) || math.IsNaN(dy) {
				return
			}
			results[i] = subsetResult{dx: dx, dy: dy, ok: true}
		}(i, ref)
	}
	wg.Wait()

	uList := make([]float64, 0, len(e.mSubsets))
	vList := make([]float64, 0, len(e.mSubsets))
	for _, result := range results {
		if !result.ok {
			continue
		}
		uList = append(uList, result.dx)
		vList = append(vList, result.dy)
	}
	if len(uList) == 0 {
		return nil
	}

	// IQR 离群点过滤：按位移幅值剔除异常点
	type uvPair struct{ u, v float64 }
	pairs := make([]uvPair, len(uList))
	dispList := make([]float64, len(uList))
	for i := range uList {
		pairs[i] = uvPair{uList[i], vList[i]}
		dispList[i] = math.Sqrt(uList[i]*uList[i] + vList[i]*vList[i])
	}
	validIndices := iqrFilter(dispList, 1.5)
	if len(validIndices) == 0 {
		validIndices = make([]int, len(uList))
		for i := range validIndices {
			validIndices[i] = i
		}
	}
	filteredU := make([]float64, 0, len(validIndices))
	filteredV := make([]float64, 0, len(validIndices))
	for _, idx := range validIndices {
		filteredU = append(filteredU, pairs[idx].u)
		filteredV = append(filteredV, pairs[idx].v)
	}

	uMedian := medianFloat64(filteredU)
	vMedian := medianFloat64(filteredV)
	e.prevPrevDx = e.prevDx
	e.prevPrevDy = e.prevDy
	e.prevDx = uMedian
	e.prevDy = vMedian

	return &DICFrameResult{
		U:            uMedian,
		V:            vMedian,
		Displacement: math.Sqrt(uMedian*uMedian + vMedian*vMedian),
		SubsetCount:  len(filteredU),
	}
}

// ---------- 公共 API ----------

// RunDIC 对 ROIF 区域执行完整 DIC 分析
// targetImg: 当前帧变形图像
func (e *ExtensometerService) RunDIC(targetImg gocv.Mat) (*DICResult, error) {
	if e.originalImg == nil {
		return nil, fmt.Errorf("原始图像未设置，请先调用 SetOrignalImg")
	}
	if targetImg.Empty() {
		return nil, fmt.Errorf("目标图像为空")
	}

	grayTarget := gocv.NewMat()
	defer grayTarget.Close()
	if targetImg.Channels() > 1 {
		gocv.CvtColor(targetImg, &grayTarget, gocv.ColorBGRToGray)
	} else {
		targetImg.CopyTo(&grayTarget)
	}

	return e.RunDICGrayMat(grayTarget)
}

func (e *ExtensometerService) RunDICGrayMat(defMat gocv.Mat) (*DICResult, error) {
	if e.originalImg == nil {
		return nil, fmt.Errorf("original image is not set")
	}
	if defMat.Empty() {
		return nil, fmt.Errorf("target image is empty")
	}

	processed := e.PreprocessGrayForDIC(defMat)
	defer processed.Close()

	return e.RunDICPreparedGrayMat(processed)
}

// RunDICPreparedGrayMat tracks a grayscale frame that has already gone through PreprocessGrayForDIC.
func (e *ExtensometerService) RunDICPreparedGrayMat(defMat gocv.Mat) (*DICResult, error) {
	if e.originalImg == nil {
		return nil, fmt.Errorf("original image is not set")
	}
	if defMat.Empty() {
		return nil, fmt.Errorf("target image is empty")
	}
	return e.RunDICPreparedGrayMatShared(defMat, defMat.ToBytes())
}

// RunDICPreparedGrayMatShared 与 RunDICPreparedGrayMat 相同，但允许调用方传入
// 已提取的字节缓冲（多 ROI 共享同一帧时省去重复 ToBytes 全图拷贝）。
func (e *ExtensometerService) RunDICPreparedGrayMatShared(defMat gocv.Mat, defBytes []uint8) (*DICResult, error) {
	if e.originalImg == nil {
		return nil, fmt.Errorf("original image is not set")
	}
	if defMat.Empty() {
		return nil, fmt.Errorf("target image is empty")
	}

	frameResult := e.calculateDisplacementMatShared(defMat, defBytes)
	if frameResult == nil {
		return nil, fmt.Errorf("DIC tracking failed")
	}

	e.ROIF.X = e.baseROIF.X + frameResult.U
	e.ROIF.Y = e.baseROIF.Y + frameResult.V

	if e.result == nil {
		e.result = &DICResult{}
	}
	e.result.FrameResults = append(e.result.FrameResults, *frameResult)
	e.result.Displacements = append(e.result.Displacements, frameResult.Displacement)
	if frameResult.Displacement > e.result.MaxDisplacement {
		e.result.MaxDisplacement = frameResult.Displacement
	}
	return e.result, nil
}

func (e *ExtensometerService) RunDICGrayBytes(defBytes []uint8, defW, defH int) (*DICResult, error) {
	if e.originalImg == nil {
		return nil, fmt.Errorf("原始图像未设置，请先调用 SetOrignalImg")
	}
	if len(defBytes) == 0 || defW <= 0 || defH <= 0 {
		return nil, fmt.Errorf("目标图像为空")
	}

	defMat, err := gocv.NewMatFromBytes(defH, defW, gocv.MatTypeCV8U, defBytes)
	if err != nil || defMat.Empty() {
		return nil, fmt.Errorf("target image is empty")
	}
	defer defMat.Close()

	return e.RunDICGrayMat(defMat)
}

// Reset 重置追踪状态（换试样时调用）
func (e *ExtensometerService) Reset() {
	e.closeRefSubsets()
	e.mIsInitialized = false
	e.mInitialPoints = nil
	e.mEngines = nil
	e.mSubsets = nil
	e.prevDx = 0
	e.prevDy = 0
	e.prevPrevDx = 0
	e.prevPrevDy = 0
	e.result = nil
}

func (e *ExtensometerService) closeRefSubsets() {
	for _, subset := range e.mSubsets {
		if subset.template != nil {
			subset.template.Close()
		}
	}
}

// GetResult 获取当前 DIC 结果
func (e *ExtensometerService) GetResult() *DICResult {
	return e.result
}

// GetCurrentDisplacement 获取当前帧位移
func (e *ExtensometerService) GetCurrentDisplacement() float64 {
	if e.result == nil || len(e.result.FrameResults) == 0 {
		return 0
	}
	return e.result.FrameResults[len(e.result.FrameResults)-1].Displacement
}

// ---------- 工具函数 ----------

func readFloat32(data []byte, idx int) float32 {
	idx *= 4
	return float32(uint32(data[idx]) | uint32(data[idx+1])<<8 |
		uint32(data[idx+2])<<16 | uint32(data[idx+3])<<24)
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

// medianFloat64 计算中值
func medianFloat64(v []float64) float64 {
	if len(v) == 0 {
		return 0
	}
	sort.Float64s(v)
	return v[len(v)/2]
}

// iqrFilter 基于 IQR（四分位距）的离群点过滤，返回通过过滤的索引
// 使用 1.5×IQR 规则：[Q1 - k*IQR, Q3 + k*IQR] 之外视为离群点
func iqrFilter(values []float64, k float64) []int {
	n := len(values)
	if n < 4 {
		indices := make([]int, n)
		for i := range indices {
			indices[i] = i
		}
		return indices
	}

	sorted := make([]float64, n)
	copy(sorted, values)
	sort.Float64s(sorted)

	q1 := sorted[n/4]
	q3 := sorted[3*n/4]
	iqr := q3 - q1
	if iqr < 1e-12 {
		indices := make([]int, n)
		for i := range indices {
			indices[i] = i
		}
		return indices
	}

	lower := q1 - k*iqr
	upper := q3 + k*iqr

	valid := make([]int, 0, n)
	for i, v := range values {
		if v >= lower && v <= upper {
			valid = append(valid, i)
		}
	}
	return valid
}
