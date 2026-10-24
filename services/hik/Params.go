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
