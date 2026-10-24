package services

import (
	ui "MINIMTS/services/UI"
	"encoding/binary"
	"fmt"
	"math"
	"sync"
	"time"

	"github.com/Mr-tang0/PIMSGoMod/protocol"
	"github.com/wailsapp/wails/v3/pkg/application"
)

type MINIMTSHeartbeat struct {
	Load     float32 `json:"load"`     // 当前载荷
	Stress   float32 `json:"stress"`   // 当前应力值
	ZeroLoad float32 `json:"zeroLoad"` // 归零后的载荷值

	Disp     float32 `json:"disp"`     // 当前位移
	Strain   float32 `json:"strain"`   // 当前应变值
	ZeroDisp float32 `json:"zeroDisp"` // 归零后的位移值

	EPos     float32 `json:"ePos"`     // 当前视频位置
	EZeroPos float32 `json:"eZeroPos"` // 归零后的视频位置值
	EStrain  float32 `json:"eStrain"`  // 当前视频应变值

	Limit string  `json:"limit"` // 禁止运动方向（idle-无，forward-正向，reverse-反向）
	Temp  float32 `json:"temp"`  // 当前温度值

	Time      float32   `json:"time"`      // 实验时间，单位秒
	StartTime time.Time `json:"startTime"` // 开始时间，用于计算实验时间

	Connected bool `json:"connected"` // MINIMTS 设备是否已连接
}

type MINIMTSService struct {
	Device *protocol.SerialCommunicator

	heartbeat   MINIMTSHeartbeat
	heartbeatMu sync.RWMutex       // 保护 heartbeat 的读写锁
	dataList    []MINIMTSHeartbeat // 数据列表，用于存储心跳数据

	systemConfig  *ui.SystemConfig  // 系统配置
	projectConfig *ui.ProjectConfig // 项目配置

	triggerStopCh chan struct{}

	testMu     sync.Mutex
	testStopCh chan struct{}
}

func NewMINIMTSService() *MINIMTSService {
	svc := &MINIMTSService{
		heartbeat: MINIMTSHeartbeat{
			Load:      0,
			Stress:    0,
			ZeroLoad:  0,
			Disp:      0,
			Strain:    0,
			ZeroDisp:  0,
			EPos:      0,
			EZeroPos:  0,
			EStrain:   0,
			Limit:     "idle",
			Time:      0,
			StartTime: time.Now(),
			Connected: false,
		},
		Device: &protocol.SerialCommunicator{},
	}
	return svc
}

func (m *MINIMTSService) Startup(systemConfig *ui.SystemConfig, projectConfig *ui.ProjectConfig) error {
	m.systemConfig = systemConfig
	m.projectConfig = projectConfig
	// m.StartHeartbeatTest()
	return nil
}

func (m *MINIMTSService) CallExit() {
	fmt.Println("MTS CallExit")
	m.StopHeartbeatTest()
	if err := m.CloseMINIMTS(); err != nil {
		fmt.Println("关闭MINIMTS失败:", err)
	}
}

// StartHeartbeatTest 以 25ms 周期向前端发送模拟心跳数据，用于测试实时图表性能。
func (m *MINIMTSService) StartHeartbeatTest() {
	m.testMu.Lock()
	if m.testStopCh != nil {
		m.testMu.Unlock()
		return
	}
	stopCh := make(chan struct{})
	m.testStopCh = stopCh
	m.testMu.Unlock()

	go func() {
		ticker := time.NewTicker(150 * time.Millisecond)
		defer ticker.Stop()

		startTime := time.Now()
		for {
			select {
			case <-stopCh:
				return
			case now := <-ticker.C:
				elapsed := float32(now.Sub(startTime).Seconds())
				phase := float64(elapsed) * 0.35
				motorStrain := float32(math.Mod(float64(elapsed)*0.02, 1.0))
				videoStrain := float32(math.Mod(float64(elapsed)*0.016, 0.8))
				stress := 25 + motorStrain*225 + float32(math.Sin(phase*8))*1.5

				heartbeat := MINIMTSHeartbeat{
					Load:      stress * 10,
					Stress:    stress,
					Disp:      motorStrain * 50,
					Strain:    motorStrain,
					EPos:      videoStrain * 10000,
					EStrain:   videoStrain,
					Limit:     "idle",
					Temp:      25 + float32(math.Sin(phase))*2,
					Time:      elapsed,
					StartTime: startTime,
					Connected: true,
				}
				application.Get().Event.Emit("mts-heartbeat", heartbeat)
			}
		}
	}()
}

// StopHeartbeatTest 停止模拟心跳数据发送。
func (m *MINIMTSService) StopHeartbeatTest() {
	m.testMu.Lock()
	defer m.testMu.Unlock()
	if m.testStopCh != nil {
		close(m.testStopCh)
		m.testStopCh = nil
	}
}

func (m *MINIMTSService) GetMINIMTSDevices() ([]string, error) {
	fmt.Println("GetMINIMTSDevices")
	return m.Device.ListAvailablePorts()
}

func (m *MINIMTSService) IsMINIMTSConnected() bool {
	return m.Device.IsConnected()
}

func (m *MINIMTSService) OpenMINIMTS(name string) error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	if m.Device.IsConnected() {
		return fmt.Errorf("设备已打开")
	}
	if err := m.Device.Connect(name, 115200); err != nil {
		return fmt.Errorf("无法打开端口 %s: %v", name, err)
	}
	fmt.Printf("串口 %s 打开成功\n", name)
	m.heartbeat.Connected = true

	go m.go_heartbeat()

	fmt.Println("心跳协程已启动")
	return nil
}

func (m *MINIMTSService) CloseMINIMTS() error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	m.heartbeat.Connected = false
	return m.Device.Disconnect()
}

func (m *MINIMTSService) go_heartbeat() {
	scanTicker := time.NewTicker(25 * time.Millisecond)
	emitTicker := time.NewTicker(200 * time.Millisecond)
	cacheTicker := time.NewTicker(60 * time.Second)
	defer scanTicker.Stop()
	defer emitTicker.Stop()

	for {
		select {
		case <-scanTicker.C:
			if err := m.scan(); err != nil {
				fmt.Println("扫描数据失败:", err)
				return
			}
		case <-emitTicker.C:
			m.heartbeatMu.RLock()
			heartbeat := m.heartbeat
			m.heartbeatMu.RUnlock()
			application.Get().Event.Emit("mts-heartbeat", heartbeat)
		case <-cacheTicker.C:
			m.save_cache() //每分钟存储缓存数据
		}
	}
}

func (m *MINIMTSService) scan() error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	if !m.heartbeat.Connected {
		return fmt.Errorf("设备未连接")
	}

	//与硬件通讯，更新状态
	if pos, err := m.updateMotorPosition(); err == nil {
		m.heartbeat.Disp = pos
		m.heartbeat.Strain = pos / m.projectConfig.SectionLength
	}
	if load, err := m.updateLoadData(); err == nil {
		m.heartbeat.Load = load
		m.heartbeat.Stress = load / m.projectConfig.SectionArea
	}
	if m.systemConfig.LimitEnabled {
		m.updateLimitStatus()
	}
	if m.systemConfig.TempEnabled {
		m.updateTempData()
	}

	// 保存数据到列表
	m.dataList = append(m.dataList, m.heartbeat)

	return nil
}

func (m *MINIMTSService) save_cache() {
	m.heartbeatMu.RLock()
	cache := m.dataList
	m.heartbeatMu.RUnlock()

	cache = cache[len(cache)-100:]
}

// CallDataToZero 数据归零函数
func (m *MINIMTSService) CallDataToZero(dataType string) error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	switch dataType {
	case "load", "stress":
		m.heartbeat.ZeroLoad += m.heartbeat.Load * float32(m.systemConfig.WeighDirection)
	case "disp", "strain":
		m.heartbeat.ZeroDisp += m.heartbeat.Disp * float32(m.systemConfig.MotorDirection)
	case "time":
		m.heartbeat.StartTime = time.Now()
	default:
		return fmt.Errorf("invalid data type: %s", dataType)
	}
	return nil
}

// MotorOn 电机使能
func (m *MINIMTSService) MotorOn() error {
	cmd := []byte{0x3e, 0x88, m.systemConfig.MotorID, 0x00}
	resp, err := m.Device.Send3EBus(cmd, 5, 0x3e)
	if err != nil {
		return err
	}

	if cmd[0] == resp[0] && cmd[3] == resp[3] {
		fmt.Printf("电机使能成功\n")
		return nil
	}
	return fmt.Errorf("电机使能失败")
}

// MotorOff 电机禁止使能
func (m *MINIMTSService) MotorOff() error {
	cmd := []byte{0x3e, 0x80, m.systemConfig.MotorID, 0x00}
	resp, err := m.Device.Send3EBus(cmd, 5, 0x3e)
	if err != nil {
		return err
	}

	if cmd[0] == resp[0] && cmd[3] == resp[3] {
		fmt.Printf("电机禁止使能成功\n")
		return nil
	}

	return fmt.Errorf("电机禁止使能失败")
}

// MotorStop 电机停止
func (m *MINIMTSService) MotorStop() error {
	cmd := []byte{0x3e, 0x81, m.systemConfig.MotorID, 0x00}
	resp, err := m.Device.Send3EBus(cmd, 5, 0x3e)
	if err != nil {
		return err
	}

	if cmd[0] == resp[0] && cmd[3] == resp[3] {
		return nil
	}

	return fmt.Errorf("电机停止失败")
}

// MotorJog 电机速度模式移动
func (m *MINIMTSService) MotorJog(speed float32) error {
	m.heartbeatMu.RLock()
	blockedDirection := m.systemConfig.LimitDirection
	m.heartbeatMu.RUnlock()

	if blockedDirection == 1 && speed > 0 {
		fmt.Println("正向限位，禁止正向运动")
		return fmt.Errorf("正向限位，禁止正向运动")
	}
	if blockedDirection == -1 && speed < 0 {
		fmt.Println("反向限位，禁止反向运动")
		return fmt.Errorf("反向限位，禁止反向运动")
	}

	if speed == 0 {
		return m.MotorStop()
	}

	rate := int32(float32(m.systemConfig.MotorDirection) * float32(speed) * m.systemConfig.MotorResolution * 100)
	header := []byte{0x3e, 0xA2, m.systemConfig.MotorID, 0x04}
	headerWithCheck := m.Device.Calculate3ECheckSum(header)

	// 构造数据体 (4字节速率值)
	// 按照协议，速度通常采用小端序
	data := make([]byte, 4)
	binary.LittleEndian.PutUint32(data, uint32(rate))
	dataWithCheck := m.Device.Calculate3ECheckSum(data)

	// 4. 拼接完整指令
	fullCmd := append(headerWithCheck, dataWithCheck...)

	// 5. 发送并同步获取回复
	resp, err := m.Device.Send3EBusRaw(fullCmd, 5, 0x3e)
	if err != nil {
		return err
	}

	respCheck := m.Device.Calculate3ECheckSum(resp[:4])

	if respCheck[4] != resp[4] {
		return fmt.Errorf("Jog运动失败")
	} else {
		return nil
	}
}

func (m *MINIMTSService) Trigger(pulseWidth time.Duration) error {
	if _, err := m.Device.SendModbus(m.systemConfig.LimitID, 0x06, 0x00, 0x01); err != nil {
		return err
	}
	time.Sleep(pulseWidth)
	_, err := m.Device.SendModbus(m.systemConfig.LimitID, 0x06, 0x00, 0x00)
	return err
}

func (m *MINIMTSService) startTriggerLoop(period, pulseWidth time.Duration) {
	m.stopTriggerLoop()

	stopCh := make(chan struct{})
	doneCh := make(chan struct{})
	m.triggerStopCh = stopCh

	go func() {
		defer close(doneCh)
		ticker := time.NewTicker(period)
		defer ticker.Stop()

		m.Trigger(pulseWidth)

		for {
			select {
			case <-stopCh:
				return
			case <-ticker.C:
				if err := m.Trigger(pulseWidth); err != nil {
					fmt.Println("发送触发信号失败:", err)
				}
			}
		}
	}()
}

func (m *MINIMTSService) stopTriggerLoop() {
	stopCh := m.triggerStopCh
	m.triggerStopCh = nil
	if stopCh != nil {
		close(stopCh)
	}
}

// StartMeasurement 开始测量逻辑
func (m *MINIMTSService) StartMeasurement() error {
	if m.projectConfig.TriggerEnabled {
		if m.projectConfig.TriggerPeriodMs <= 0 {
			return fmt.Errorf("触发周期必须大于 0 ms")
		}
		if m.projectConfig.TriggerPulseWidthMs <= 0 || m.projectConfig.TriggerPulseWidthMs > m.projectConfig.TriggerPeriodMs {
			return fmt.Errorf("触发脉宽必须大于 0 ms 且不能超过触发周期")
		}
	}

	if m.projectConfig.TriggerEnabled {
		m.startTriggerLoop(
			time.Duration(m.projectConfig.TriggerPeriodMs)*time.Millisecond,
			time.Duration(m.projectConfig.TriggerPulseWidthMs)*time.Millisecond,
		)
	}

	if err := m.MotorOn(); err != nil {
		return err
	}

	if err := m.MotorJog(m.projectConfig.Speed * float32(m.projectConfig.TestType)); err != nil {
		return err
	}

	return nil
}

// StopMeasurement 停止测量
func (m *MINIMTSService) StopMeasurement() error {
	err := m.MotorStop()
	m.stopTriggerLoop()
	return err
}

// EmergencyStop 紧急停止
func (m *MINIMTSService) EmergencyStop() error {
	stopErr := m.MotorStop()
	m.stopTriggerLoop()
	if stopErr != nil {
		return stopErr
	}
	return m.MotorOff()
}

func (m *MINIMTSService) updateMotorPosition() (float32, error) {
	cmd := []byte{0x3e, 0x92, m.systemConfig.MotorID, 0x00}

	resp, err := m.Device.Send3EBus(cmd, 14, 0x3e)
	if err != nil {
		fmt.Println("获取位移数据失败:", err)
		return 0, err
	}

	if len(resp) == 14 && resp[1] == 0x92 {
		dataPart := resp[5:13]
		motorAngle := int64(binary.LittleEndian.Uint64(dataPart))
		length := float32(motorAngle) / 100.0
		Pos := float32(m.systemConfig.MotorDirection) * length / m.systemConfig.MotorResolution

		return Pos, nil
	}

	return 0, nil
}

func (m *MINIMTSService) updateLoadData() (float32, error) {
	resp, err := m.Device.SendModbus(m.systemConfig.WeighID, 0x03, 0x00, 0x02)
	if err != nil {
		fmt.Println("获取载荷数据失败:", err)
		return 0, err
	}

	if len(resp) < 7 {
		return 0, fmt.Errorf("invalid response length")
	}

	dataSwapped := []byte{resp[4], resp[3], resp[6], resp[5]}
	rawLoad := int32(binary.LittleEndian.Uint32(dataSwapped))

	load := float32(m.systemConfig.WeighDirection) * float32(rawLoad) / float32(m.systemConfig.WeighResolution)

	return load, nil
}

func (m *MINIMTSService) updateTempData() (float32, error) {
	resp, err := m.Device.SendModbus(m.systemConfig.TempID, 0x04, 0x00, 0x01)
	if err != nil {
		return 0, err
	}
	if len(resp) >= 7 {
		dataPart := resp[3:5]
		//16位有符号整数
		rawVal := int16(binary.BigEndian.Uint16(dataPart))
		Temp := float32(rawVal) / 10.0
		return Temp, nil
	}

	return 0, fmt.Errorf("invalid response length")

}

func (m *MINIMTSService) updateLimitStatus() (int, error) {

	resp, err := m.Device.SendModbus(m.systemConfig.LimitID, 0x04, 0x00, 0x02)
	if err != nil {
		fmt.Println("获取限位状态失败:", err)
		return 0, err
	}
	if len(resp) < 9 {
		return 0, fmt.Errorf("限位状态响应长度不足: %d", len(resp))
	}

	limit := 0
	if resp[4] == 0x01 {
		limit = 1
	} else if resp[6] == 0x01 {
		limit = 2
	}
	return limit, nil
}
