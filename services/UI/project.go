package ui

import (
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
)

const defaultProjectConfigFile = "project.json"

// ProjectConfig 对应项目配置表单
type ProjectConfig struct {
	Experimenter string `json:"experimenter"` // 实验者
	SampleNo     string `json:"sampleNo"`     // 样本编号
	TestDate     string `json:"testDate"`     // 测试日期

	SampleShape   string  `json:"sampleShape"`   // 样本形状，dogbone 或 cylinder
	Width         float32 `json:"width"`         // 样本宽度
	Thickness     float32 `json:"thickness"`     // 样本厚度
	Diameter      float32 `json:"diameter"`      // 样本直径
	SectionArea   float32 `json:"sectionArea"`   // 样本截面面积
	SectionLength float32 `json:"sectionLength"` // 样本截面长度

	TestType      int     `json:"testType"`      // 1:tension, -1:compression
	Speed         float32 `json:"speed"`         // 测试速度，单位：mm/s
	StopCondition string  `json:"stopCondition"` // 停止条件，time 或 disp

	FilePath string `json:"filePath"` // 准静态保存路径
	FileName string `json:"fileName"` // 准静态文件名

	TriggerEnabled      bool `json:"triggerEnabled"`      // 是否启用触发器
	TriggerPeriodMs     int  `json:"triggerPeriodMs"`     // 触发器周期，单位毫秒
	TriggerPulseWidthMs int  `json:"triggerPulseWidthMs"` // 触发器脉冲宽度，单位毫秒

	DICEnable      bool    `json:"dicEnable"`
	DICFolder      string  `json:"dicFolder"`
	DICFileName    string  `json:"dicFileName"`
	TriggerType    string  `json:"triggerType"`
	VideoExtEnable bool    `json:"videoExtEnable"`
	PoissonEnable  bool    `json:"poissonEnable"`
	PixLength      float32 `json:"pixLength"`
	PhysLength     float32 `json:"physLength"`
}

func NewProjectConfig() *ProjectConfig {
	return &ProjectConfig{
		Experimenter:        "USER",
		SampleNo:            "1",
		TestDate:            "2023-01-01",
		SampleShape:         "dogbone",
		Width:               1.0,
		Thickness:           1.0,
		Diameter:            1.0,
		SectionArea:         1.0,
		SectionLength:       10.0,
		TestType:            1,
		Speed:               0.1,
		StopCondition:       "time",
		TriggerEnabled:      false,
		TriggerPeriodMs:     5000,
		TriggerPulseWidthMs: 1000,
		FilePath:            "./data/",
		FileName:            "test.json",
		TriggerType:         "internal",
		DICFileName:         "IMG_",
		PixLength:           400,
		PhysLength:          25,
	}
}

func (s *ProjectConfig) GetFromLocalPath(path string) (*ProjectConfig, error) {
	configPath, err := projectConfigPath(path)
	if err != nil {
		return nil, err
	}

	data, err := os.ReadFile(configPath)
	if errors.Is(err, os.ErrNotExist) {
		if err := s.SaveToLocalPath(path, s); err != nil {
			return nil, err
		}
		return s, nil
	}
	if err != nil {
		return nil, err
	}

	config := NewProjectConfig()
	if err := json.Unmarshal(data, config); err != nil {
		return nil, err
	}
	*s = *config
	return s, nil
}

func (s *ProjectConfig) SaveToLocalPath(path string, config *ProjectConfig) error {
	if config == nil {
		return errors.New("项目配置不能为空")
	}

	configPath, err := projectConfigPath(path)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(configPath), 0755); err != nil {
		return err
	}

	data, err := json.MarshalIndent(config, "", "  ")
	if err != nil {
		return err
	}
	if err := os.WriteFile(configPath, data, 0644); err != nil {
		return err
	}

	*s = *config
	return nil
}

func projectConfigPath(path string) (string, error) {
	if path != "" {
		return path, nil
	}

	configDir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(configDir, "MINIMTS", defaultProjectConfigFile), nil
}
