package minimts

import (
	"MINIMTS/services/config"
	"fmt"
	"strings"
	"time"
)

// Driver 封装设备协议差异，公共试验逻辑由 devices.MINIMTSService 统一维护。
type Driver interface {
	Devices() ([]string, error)
	Open(target string) error
	Close() error
	IsOpen() bool

	ReadPosition() (float32, error)
	ReadLoad() (float32, error)
	ReadTemperature() (float32, error)
	ReadLimit() (int, error)

	MotorOn() error
	MotorOff() error
	MotorStop() error
	MotorJog(speed float32) error
	Trigger(pulseWidth time.Duration) error
}

// NewDriver 按启动配置创建协议驱动，运行期间不再切换。
func NewDriver(protocol string, systemConfig *config.SystemConfig) (Driver, error) {
	switch strings.ToLower(strings.TrimSpace(protocol)) {
	case "", "pims":
		return newPIMSDriver(systemConfig), nil
	case "nimte":
		return newNIMTEDriver(systemConfig), nil
	default:
		return nil, fmt.Errorf("不支持的设备协议 %q，仅支持 pims 或 nimte", protocol)
	}
}
