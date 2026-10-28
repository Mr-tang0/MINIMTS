package hik

type HIKCameraParams struct {
	ExposureAuto      uint32  `json:"exposureAuto"`
	ExposureTime      float64 `json:"exposureTime"`
	GainAuto          uint32  `json:"gainAuto"`
	Gain              float64 `json:"gain"`
	DigitalGain       float64 `json:"digitalGain"`
	DigitalGainEnable bool    `json:"digitalGainEnable"`
	FrameRate         float64 `json:"frameRate"`
	GammaEnable       bool    `json:"gammaEnable"`
	Gamma             float64 `json:"gamma"`
	BalanceWhiteAuto  uint32  `json:"balanceWhiteAuto"`
	BalanceWhite      int64   `json:"balanceWhite"`
}

type HIKCameraFrame struct {
	Raw    []uint16 `json:"raw"`
	Width  int      `json:"width"`
	Height int      `json:"height"`
}

type ROIRect struct {
	X      float64 `json:"x"`
	Y      float64 `json:"y"`
	Width  float64 `json:"width"`
	Height float64 `json:"height"`
}

type LineDirection struct {
	X1 float64 `json:"x1"`
	Y1 float64 `json:"y1"`
	X2 float64 `json:"x2"`
	Y2 float64 `json:"y2"`
}

// ---------- DIC 参数配置 ----------

// DICParam DIC 计算参数
type DICParam struct {
	PreprocessEnabled    bool
	PreprocessMethod     string // 预处理方法: none/gauss/bilateral/nlm/highpass
	DenoiseKernel        int
	SharpenAmount        float64
	TrackingSearchRadius int
	SubsetSize           int     // 子集大小（奇数，如 21）
	StepSize             int     // 网格步长
	SearchRadius         int     // 整像素搜索半径
	ZNCCThreshold        float64 // ZNCC 置信度阈值
	MaxIter              int     // IC-GN 最大迭代次数
	ConvergeEps          float64 // IC-GN 收敛阈值（像素）
	PointsCount          int     // 多点阵列的点数
	Spacing              int     // 点间距
	ResX                 int     // 分辨率 X
	ResY                 int     // 分辨率 Y
	Poisson              float64 // 泊松比
	YoungMod             float64 // 杨氏模量
}

// DefaultDICParam 返回默认 DIC 参数,未来可能交给用户配置
func DefaultDICParam() *DICParam {
	return &DICParam{
		PreprocessEnabled:    true,
		PreprocessMethod:     DICPreprocessGauss,
		DenoiseKernel:        3,
		SharpenAmount:        0.8,
		SubsetSize:           25,
		StepSize:             5,
		SearchRadius:         15,
		TrackingSearchRadius: 3,
		ZNCCThreshold:        0.6,
		MaxIter:              25,
		ConvergeEps:          5e-3,
		PointsCount:          7,
		Spacing:              1,
		Poisson:              0.3,
		YoungMod:             200e3,
	}
}
