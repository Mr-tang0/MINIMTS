package config

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"
)

const defaultHardwareConfigFile = "hardware.json"

// HardwareConfig 仅由管理员在启动前手动配置，不提供程序内保存或修改入口。
type HardwareConfig struct {
	Protocol string `json:"protocol"`
}

// LoadHardwareConfig 读取设备协议配置；文件或字段缺失时默认使用 PIMS。
func LoadHardwareConfig(path string) (*HardwareConfig, error) {
	if path == "" {
		path = defaultHardwareConfigFile
	}

	config := &HardwareConfig{Protocol: "pims"}
	data, err := os.ReadFile(path)
	if errors.Is(err, os.ErrNotExist) {
		return config, nil
	}
	if err != nil {
		return nil, fmt.Errorf("读取硬件配置失败: %w", err)
	}
	if err := json.Unmarshal(data, config); err != nil {
		return nil, fmt.Errorf("解析硬件配置失败: %w", err)
	}

	config.Protocol = strings.ToLower(strings.TrimSpace(config.Protocol))
	if config.Protocol == "" {
		config.Protocol = "pims"
	}
	switch config.Protocol {
	case "pims", "nimte":
		return config, nil
	default:
		return nil, fmt.Errorf("不支持的设备协议 %q，仅支持 pims 或 nimte", config.Protocol)
	}
}
