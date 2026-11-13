package minimts

import (
	"MINIMTS/services/config"
	"encoding/binary"
	"fmt"
	"time"

	"github.com/Mr-tang0/PIMSGoMod/protocol"
)

type pimsDriver struct {
	Device       *protocol.SerialCommunicator
	systemConfig *config.SystemConfig
}

func newPIMSDriver(systemConfig *config.SystemConfig) Driver {
	return &pimsDriver{
		Device: &protocol.SerialCommunicator{
			MaxRetries: 3,
			Timeout:    100 * time.Millisecond,
		},
		systemConfig: systemConfig,
	}
}

func (d *pimsDriver) Devices() ([]string, error) {
	return d.Device.ListAvailablePorts()
}

func (d *pimsDriver) Open(name string) error {
	if d.Device.IsConnected() {
		return fmt.Errorf("设备已打开")
	}
	return d.Device.Connect(name, 115200)
}

func (d *pimsDriver) Close() error {
	return d.Device.Disconnect()
}

func (d *pimsDriver) IsOpen() bool {
	return d.Device.IsConnected()
}

func (d *pimsDriver) ReadPosition() (float32, error) {
	cmd := []byte{0x3e, 0x92, d.systemConfig.Motor_ID, 0x00}
	resp, err := d.Device.Send3EBus(cmd, 14, 0x3e)
	if err != nil {
		return 0, fmt.Errorf("获取位移数据失败: %w", err)
	}
	if len(resp) != 14 || resp[1] != 0x92 {
		return 0, nil
	}

	dataPart := resp[5:13]
	motorAngle := int64(binary.LittleEndian.Uint64(dataPart))
	length := float32(motorAngle) / 100.0
	return float32(d.systemConfig.Weigh_Direction) * float32(d.systemConfig.Motor_Direction) * length / d.systemConfig.Motor_Resolution, nil
}

func (d *pimsDriver) ReadLoad() (float32, error) {
	resp, err := d.Device.SendModbus(d.systemConfig.Weigh_ID, 0x03, 0x00, 0x02)
	if err != nil {
		return 0, fmt.Errorf("获取载荷数据失败: %w", err)
	}
	if len(resp) == 5 && resp[1]&0x80 != 0 {
		return 0, fmt.Errorf("称重模块 Modbus 异常: slave=%d function=0x%02X code=0x%02X", resp[0], resp[1], resp[2])
	}
	if len(resp) != 9 {
		return 0, fmt.Errorf("称重模块响应长度错误: got=%d frame=% X", len(resp), resp)
	}

	dataSwapped := []byte{resp[4], resp[3], resp[6], resp[5]}
	rawLoad := int32(binary.LittleEndian.Uint32(dataSwapped))
	return float32(d.systemConfig.Weigh_Direction) * float32(rawLoad) / d.systemConfig.Weigh_Resolution, nil
}

func (d *pimsDriver) ReadTemperature() (float32, error) {
	resp, err := d.Device.SendModbus(d.systemConfig.Temp_ID, 0x04, 0x00, 0x01)
	if err != nil {
		return 0, err
	}
	if len(resp) < 7 {
		return 0, fmt.Errorf("invalid response length")
	}
	rawValue := int16(binary.BigEndian.Uint16(resp[3:5]))
	return float32(rawValue) / 10.0, nil
}

func (d *pimsDriver) ReadLimit() (int, error) {
	resp, err := d.Device.SendModbus(d.systemConfig.Limit_ID, 0x04, 0x00, 0x02)
	if err != nil {
		return 0, err
	}
	if len(resp) < 9 {
		return 0, fmt.Errorf("限位状态响应长度不足: %d", len(resp))
	}
	if resp[4] == 0x01 {
		return 1, nil
	}
	if resp[6] == 0x01 {
		return 2, nil
	}
	return 0, nil
}

func (d *pimsDriver) MotorOn() error {
	cmd := []byte{0x3e, 0x88, d.systemConfig.Motor_ID, 0x00}
	resp, err := d.Device.Send3EBus(cmd, 5, 0x3e)
	if err != nil {
		return err
	}
	if len(resp) >= 4 && cmd[0] == resp[0] && cmd[3] == resp[3] {
		fmt.Println("电机使能成功")
		return nil
	}
	return fmt.Errorf("电机使能失败")
}

func (d *pimsDriver) MotorOff() error {
	cmd := []byte{0x3e, 0x80, d.systemConfig.Motor_ID, 0x00}
	resp, err := d.Device.Send3EBus(cmd, 5, 0x3e)
	if err != nil {
		return err
	}
	if len(resp) >= 4 && cmd[0] == resp[0] && cmd[3] == resp[3] {
		fmt.Println("电机禁止使能成功")
		return nil
	}
	return fmt.Errorf("电机禁止使能失败")
}

func (d *pimsDriver) MotorStop() error {
	cmd := []byte{0x3e, 0x81, d.systemConfig.Motor_ID, 0x00}
	resp, err := d.Device.Send3EBus(cmd, 5, 0x3e)
	if err != nil {
		return err
	}
	if len(resp) >= 4 && cmd[0] == resp[0] && cmd[3] == resp[3] {
		return nil
	}
	return fmt.Errorf("电机停止失败")
}

func (d *pimsDriver) MotorJog(speed float32) error {
	if speed == 0 {
		return d.MotorStop()
	}

	rate := int32(float32(d.systemConfig.Motor_Direction) * speed * d.systemConfig.Motor_Resolution * 100)
	header := []byte{0x3e, 0xA2, d.systemConfig.Motor_ID, 0x04}
	headerWithCheck := d.Device.Calculate3ECheckSum(header)
	data := make([]byte, 4)
	binary.LittleEndian.PutUint32(data, uint32(rate))
	dataWithCheck := d.Device.Calculate3ECheckSum(data)
	fullCmd := append(headerWithCheck, dataWithCheck...)

	resp, err := d.Device.Send3EBusRaw(fullCmd, 5, 0x3e)
	if err != nil {
		return err
	}
	if len(resp) < 5 {
		return fmt.Errorf("Jog 运动响应长度不足")
	}
	respCheck := d.Device.Calculate3ECheckSum(resp[:4])
	if len(respCheck) < 5 || respCheck[4] != resp[4] {
		return fmt.Errorf("Jog运动失败")
	}
	return nil
}

func (d *pimsDriver) Trigger(pulseWidth time.Duration) error {
	if _, err := d.Device.SendModbus(d.systemConfig.Limit_ID, 0x06, 0x00, 0x01); err != nil {
		return err
	}
	time.Sleep(pulseWidth)
	_, err := d.Device.SendModbus(d.systemConfig.Limit_ID, 0x06, 0x00, 0x00)
	return err
}
