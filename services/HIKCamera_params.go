package services

import (
	"MINIMTS/services/hik"
	"fmt"
)

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
