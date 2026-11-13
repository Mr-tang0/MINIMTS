package minimts

import (
	"MINIMTS/services/config"
	"fmt"
	"time"

	"github.com/Mr-tang0/PIMSGoMod/protocol/XinJie"
)

const (
	nimteSpeedHD uint16 = 32
	nimtePosHD   uint16 = 34
	nimteForceD  uint16 = 88
	nimteJogM    uint16 = 31
	nimteStopM   uint16 = 32
)

type nimteDriver struct {
	PLC          *XinJie.XinjieClient
	opened       bool
	systemConfig *config.SystemConfig
}

func newNIMTEDriver(systemConfig *config.SystemConfig) Driver {
	return &nimteDriver{
		PLC:          XinJie.NewXinjieClient(""),
		systemConfig: systemConfig,
	}
}

func (d *nimteDriver) Devices() ([]string, error) {
	return []string{"192.168.11.6"}, nil
}

func (d *nimteDriver) Open(ip string) error {
	if d.opened {
		return fmt.Errorf("PLC 已打开")
	}
	if err := d.PLC.OpenTCP(ip, 1); err != nil {
		return err
	}
	d.opened = true
	return nil
}

func (d *nimteDriver) Close() error {
	d.PLC.Close()
	d.opened = false
	return nil
}

func (d *nimteDriver) IsOpen() bool {
	return d.opened
}

func (d *nimteDriver) ReadPosition() (float32, error) {
	rawPos, err := d.PLC.ReadRegister(XinJie.HD(nimtePosHD), XinJie.Float32, false)
	if err != nil {
		return 0, err
	}
	return rawPos.(float32), nil
}

func (d *nimteDriver) ReadLoad() (float32, error) {
	rawLoad, err := d.PLC.ReadRegister(XinJie.D(nimteForceD), XinJie.Float32, false)
	if err != nil {
		return 0, err
	}
	return float32(d.systemConfig.Weigh_Direction) * rawLoad.(float32) / d.systemConfig.Weigh_Resolution, nil
}

// NIMTE 温度模块尚未适配。
func (d *nimteDriver) ReadTemperature() (float32, error) {
	return 0, nil
}

// NIMTE 限位模块尚未适配。
func (d *nimteDriver) ReadLimit() (int, error) {
	return 0, nil
}

// NIMTE 当前由 PLC 管理电机使能。
func (d *nimteDriver) MotorOn() error {
	return nil
}

// NIMTE 当前由 PLC 管理电机使能。
func (d *nimteDriver) MotorOff() error {
	return nil
}

func (d *nimteDriver) MotorStop() error {
	return d.PLC.WriteCoil(XinJie.M(nimteStopM), true)
}

func (d *nimteDriver) MotorJog(speed float32) error {
	if speed == 0 {
		return d.MotorStop()
	}
	speed *= float32(d.systemConfig.Motor_Direction)
	if err := d.PLC.WriteRegister(XinJie.HD(nimteSpeedHD), speed, XinJie.Float32, false); err != nil {
		return err
	}
	return d.PLC.WriteCoil(XinJie.M(nimteJogM), true)
}

// NIMTE 外部触发尚未适配。
func (d *nimteDriver) Trigger(pulseWidth time.Duration) error {
	return nil
}
