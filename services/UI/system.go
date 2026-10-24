package ui

import (
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
)

const defaultSystemConfigFile = "system.json"

// SystemConfig 系统配置数据结构
type SystemConfig struct {
	MotorID         byte    `json:"motor_id"`         // 电机ID
	MotorResolution float32 `json:"motor_resolution"` // 电机分辨率
	MotorDirection  int     `json:"motor_direction"`  // 电机方向，1 或 -1

	WeighID         byte    `json:"weigh_id"`         // 称重模块ID
	WeighResolution float32 `json:"weigh_resolution"` // 称重模块分辨率
	WeighDirection  int     `json:"weigh_direction"`  // 称重模块方向，1 或 -1

	LimitID        byte `json:"limit_id"`        // 限位模块ID
	LimitEnabled   bool `json:"limit_enabled"`   // 是否启用限位模块
	LimitDirection int  `json:"limit_direction"` // 限位模块方向，1 或 -1

	TempID      byte `json:"temp_id"`      // 温度模块ID
	TempEnabled bool `json:"temp_enabled"` // 是否启用温度模块
}

func NewSystemConfig() *SystemConfig {
	return &SystemConfig{
		MotorID:         1,          // 默认电机ID
		MotorResolution: 1312.47251, // 默认电机分辨率 (单位: mm)
		MotorDirection:  1,          // 默认电机方向

		WeighID:         2,   // 默认称重模块ID
		WeighResolution: 1.0, // 默认称重模块分辨率 (单位: kg)
		WeighDirection:  1,   // 默认称重模块方向

		LimitID:        4,    // 默认限位模块ID
		LimitEnabled:   true, // 默认启用限位模块
		LimitDirection: 2,    // 默认限位模块方向：CW对应的端口

		TempID:      5,     // 默认温度模块ID
		TempEnabled: false, // 默认不启用温度模块
	}
}

func (s *SystemConfig) GetFromLocalPath(path string) (*SystemConfig, error) {
	configPath, err := systemConfigPath(path)
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

	config := NewSystemConfig()
	if err := json.Unmarshal(data, config); err != nil {
		return nil, err
	}
	*s = *config
	return s, nil
}

func (s *SystemConfig) SaveToLocalPath(path string, config *SystemConfig) error {
	if config == nil {
		return errors.New("系统配置不能为空")
	}

	configPath, err := systemConfigPath(path)
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

func systemConfigPath(path string) (string, error) {
	if path != "" {
		return path, nil
	}

	configDir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(configDir, "MINIMTS", defaultSystemConfigFile), nil
}
