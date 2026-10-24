package config

import (
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
)

const defaultSystemConfigFile = "system.json"

// SystemConfig 系统配置数据结构
type SystemConfig struct {
	Motor_ID         byte    `json:"motor_id"`         // 电机ID
	Motor_Resolution float32 `json:"motor_resolution"` // 电机分辨率
	Motor_Direction  int     `json:"motor_direction"`  // 电机方向，1 或 -1

	Weigh_ID         byte    `json:"weigh_id"`         // 称重模块ID
	Weigh_Resolution float32 `json:"weigh_resolution"` // 称重模块分辨率
	Weigh_Direction  int     `json:"weigh_direction"`  // 称重模块方向，1 或 -1

	Limit_ID        byte `json:"limit_id"`        // 限位模块ID
	Limit_Enabled   bool `json:"limit_enabled"`   // 是否启用限位模块
	Limit_Direction int  `json:"limit_direction"` // 限位模块方向，1 或 -1

	Temp_ID      byte `json:"temp_id"`      // 温度模块ID
	Temp_Enabled bool `json:"temp_enabled"` // 是否启用温度模块
	Temp_Offset  int  `json:"temp_offset"`  // 温度模块偏移量

	PID_P        float64 `json:"pid_p"`         // pidP值
	PID_I        float64 `json:"pid_i"`         // pidI值
	PID_D        float64 `json:"pid_d"`         // pidD值
	PID_DeltaT   float64 `json:"pid_delta_t"`   // 时间步长
	PID_MaxSpeed float64 `json:"pid_max_speed"` // 最大速度
	PID_MinSpeed float64 `json:"pid_min_speed"` // 最小速度
	PID_DIR      int     `json:"pid_dir"`       // pid方向，1 或 -1
}

func NewSystemConfig() *SystemConfig {
	return &SystemConfig{
		Motor_ID:         1,          // 默认电机ID
		Motor_Resolution: 1312.47251, // 默认电机分辨率 (单位: mm)
		Motor_Direction:  1,          // 默认电机方向

		Weigh_ID:         2,   // 默认称重模块ID
		Weigh_Resolution: 1.0, // 默认称重模块分辨率
		Weigh_Direction:  1,   // 默认称重模块方向

		Limit_ID:        4,    // 默认限位模块ID
		Limit_Enabled:   true, // 默认启用限位模块
		Limit_Direction: 2,    // 默认限位模块方向：CW对应的端口

		Temp_ID:      5,     // 默认温度模块ID
		Temp_Enabled: false, // 默认不启用温度模块
		Temp_Offset:  0,     // 默认温度模块偏移量为0

		PID_P:        68,   // 默认pidP值
		PID_I:        0.2,  // 默认pidI值
		PID_D:        5,    // 默认pidD值
		PID_DeltaT:   0.2,  // 默认时间步长
		PID_MaxSpeed: 0.3,  // 默认最大速度
		PID_MinSpeed: -0.3, // 默认最小速度
		PID_DIR:      1,    // 默认pid方向
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
