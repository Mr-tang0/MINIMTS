package devices

import (
	"MINIMTS/services/config"
	"MINIMTS/services/devices/hik"
	"encoding/binary"
	"fmt"
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

	LimitDIR    string  `json:"limitDIR"`    // 限位被触发方向（idle-无限位，cw-正向限位，ccw-反向限位）
	Temperature float32 `json:"temperature"` // 当前温度值

	Duration  float32   `json:"duration"`  // 实验时间，单位秒
	StartTime time.Time `json:"startTime"` // 实验开始时间
}

type MINIMTSService struct {
	heartbeat   MINIMTSHeartbeat
	heartbeatMu sync.RWMutex       // 保护 heartbeat 的读写锁
	dataList    []MINIMTSHeartbeat // 数据列表，用于存储心跳数据

	Device *protocol.SerialCommunicator //连接句柄

	systemConfig  *config.SystemConfig  // 系统配置
	projectConfig *config.ProjectConfig // 项目配置

	extensometer *HIKExtensometerService // 引伸计服务，用于读取比例标定系数

	videoMu           sync.Mutex
	videoBaseDistance float64 // ROI A/B 的初始欧几里得距离（像素）
	videoBaseReady    bool    // 初始距离是否已记录

	triggerStopCh chan struct{}

	testMu     sync.Mutex
	testStopCh chan struct{}
}

func NewMINIMTSService(extensometer *HIKExtensometerService) *MINIMTSService {
	svc := &MINIMTSService{
		heartbeat: MINIMTSHeartbeat{
			Load:     0,
			Stress:   0,
			ZeroLoad: 0,

			Disp:     0,
			Strain:   0,
			ZeroDisp: 0,

			EPos:     0,
			EZeroPos: 0,
			EStrain:  0,

			LimitDIR:    "idle",
			Temperature: 25,
			Duration:    0,
		},
		Device:       &protocol.SerialCommunicator{},
		extensometer: extensometer,
	}
	return svc
}

func (m *MINIMTSService) Startup(systemConfig *config.SystemConfig, projectConfig *config.ProjectConfig) error {
	fmt.Println("MTS Startup")

	m.systemConfig = systemConfig
	m.projectConfig = projectConfig
	return nil
}

// ListenExtensometer 订阅引伸计结果事件，根据 ROI 距离计算视频位移与应变并写入 heartbeat。
// 需在 application 初始化之后调用。
func (m *MINIMTSService) ListenExtensometer() {
	app := application.Get()
	if app == nil {
		return
	}

	// 引伸计每帧推送两个 ROI 中心坐标与欧几里得距离
	app.Event.On("hik_extensometer_result", func(event *application.CustomEvent) {
		result, ok := event.Data.(ExtensometerResult)
		if !ok {
			return
		}
		m.updateVideoMetrics(result.Distance)
	})

	// ROI 重新绘制后，距离基准需要重新采集
	app.Event.On("hik_roi_selected", func(event *application.CustomEvent) {
		m.resetExtensometerBaseline()
	})
}

// updateVideoMetrics 根据两个 ROI 的欧几里得距离计算视频位移与应变。
//
// EPos = 当前距离（已比例标定时换算为 mm，否则为像素）
// EStrain = (当前距离 - 初始距离) / 初始距离
var lastTime time.Time
var videoMetricsDebug = hik.NewDebugAverager(30) // 调试：视频指标降频输出

func (m *MINIMTSService) updateVideoMetrics(distance float64) {
	if gap := time.Since(lastTime); gap < time.Second {
		videoMetricsDebug.Add("事件间隔", gap)
	}
	lastTime = time.Now()
	if avg := videoMetricsDebug.Report(); avg != "" {
		fmt.Println("[视频指标]", avg)
	}

	if distance <= 0 {
		return
	}

	m.videoMu.Lock()
	if !m.videoBaseReady {
		m.videoBaseDistance = distance
		m.videoBaseReady = true
	}
	baseDistance := m.videoBaseDistance
	m.videoMu.Unlock()

	epos := distance
	if m.extensometer != nil {
		if ratio := m.extensometer.GetResolutionRatio(); ratio > 0 {
			epos = distance / ratio
		}
	}

	var estrain float64
	if baseDistance > 0 {
		estrain = (distance - baseDistance) / baseDistance
	}

	m.heartbeatMu.Lock()
	m.heartbeat.EPos = float32(epos)
	m.heartbeat.EStrain = float32(estrain)
	m.heartbeatMu.Unlock()
}

// resetExtensometerBaseline 清除视频距离基准（ROI 变更时调用）
func (m *MINIMTSService) resetExtensometerBaseline() {
	m.videoMu.Lock()
	m.videoBaseDistance = 0
	m.videoBaseReady = false
	m.videoMu.Unlock()

	m.heartbeatMu.Lock()
	m.heartbeat.EPos = 0
	m.heartbeat.EStrain = 0
	m.heartbeatMu.Unlock()
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
	m.heartbeat.StartTime = time.Now()
	go m.go_heartbeat()

	fmt.Println("连接成功，心跳协程已启动")
	return nil
}

func (m *MINIMTSService) CloseMINIMTS() error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	return m.Device.Disconnect()
}

func (m *MINIMTSService) IsOpened() bool {
	return m.Device.IsConnected()
}

func (m *MINIMTSService) go_heartbeat() {
	scanTicker := time.NewTicker(20 * time.Millisecond) // 扫描数据周期
	emitTicker := time.NewTicker(50 * time.Millisecond) // 发送心跳周期
	cacheTicker := time.NewTicker(60 * time.Second)     // 缓存数据自动保存周期，单位秒

	defer scanTicker.Stop()
	defer emitTicker.Stop()
	defer cacheTicker.Stop()

	for {
		select {
		case <-scanTicker.C:
			//更新时间
			m.heartbeatMu.RLock()
			m.heartbeat.Duration = float32(time.Now().Sub(m.heartbeat.StartTime).Seconds())
			m.heartbeatMu.RUnlock()

			// // 扫描数据
			// if err := m.go_scan(); err != nil {
			// 	return
			// }
		case <-emitTicker.C:
			// 发送心跳数据
			m.heartbeatMu.RLock()
			heartbeat := m.heartbeat
			m.heartbeatMu.RUnlock()

			application.Get().Event.Emit("mts-heartbeat", heartbeat)
		case <-cacheTicker.C:
			// 缓存数据自动保存
			go m.go_save_cache() //每分钟存储缓存数据
		}
	}
}

func (m *MINIMTSService) go_scan() error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	if !m.IsOpened() {
		return fmt.Errorf("设备未连接")
	}

	// 更新电机位置
	if pos, err := m.updateMotorPosition(); err == nil {
		m.heartbeat.Disp = pos
		m.heartbeat.Strain = pos / m.projectConfig.SectionLength
	}

	// 更新载荷数据
	if load, err := m.updateLoadData(); err == nil {
		m.heartbeat.Load = load
		m.heartbeat.Stress = load / m.projectConfig.SectionArea
	}

	// 更新限位状态(如果已启用)
	if m.systemConfig.Limit_Enabled {
		m.updateLimitStatus()
	}

	// 更新温度数据(如果已启用)
	if m.systemConfig.Temp_Enabled {
		m.updateTempData()
	}

	// 缓存保存数据到列表
	m.dataList = append(m.dataList, m.heartbeat)
	return nil
}

func (m *MINIMTSService) CallExit() {
	fmt.Println("MTS CallExit")

	if err := m.CloseMINIMTS(); err != nil {
		fmt.Println("关闭MINIMTS失败:", err)
	}
}

func (m *MINIMTSService) GetMINIMTSDevices() ([]string, error) {
	fmt.Println("GetMINIMTSDevices")
	return m.Device.ListAvailablePorts()
}

func (m *MINIMTSService) IsMINIMTSConnected() bool {
	return m.Device.IsConnected()
}

func (m *MINIMTSService) go_save_cache() {
	// m.heartbeatMu.RLock()
	// cache := m.dataList
	// m.heartbeatMu.RUnlock()

	// cache = cache[len(cache)-100:]
}

// CallDataToZero 数据归零函数
func (m *MINIMTSService) CallDataToZero(dataType string) error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	switch dataType {
	case "load", "stress":
		m.heartbeat.ZeroLoad += m.heartbeat.Load * float32(m.systemConfig.Weigh_Direction)
	case "disp", "strain":
		m.heartbeat.ZeroDisp += m.heartbeat.Disp * float32(m.systemConfig.Motor_Direction)
	case "espos":
		m.heartbeat.EZeroPos = m.heartbeat.EPos
	case "time":
		m.heartbeat.StartTime = time.Now()
	default:
		return fmt.Errorf("invalid data type: %s", dataType)
	}
	return nil
}

// MotorOn 电机使能
func (m *MINIMTSService) MotorOn() error {
	cmd := []byte{0x3e, 0x88, m.systemConfig.Motor_ID, 0x00}
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
	cmd := []byte{0x3e, 0x80, m.systemConfig.Motor_ID, 0x00}
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
	cmd := []byte{0x3e, 0x81, m.systemConfig.Motor_ID, 0x00}
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
	blockedDirection := m.systemConfig.Limit_Direction
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

	rate := int32(float32(m.systemConfig.Motor_Direction) * float32(speed) * m.systemConfig.Motor_Resolution * 100)
	header := []byte{0x3e, 0xA2, m.systemConfig.Motor_ID, 0x04}
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
	if _, err := m.Device.SendModbus(m.systemConfig.Limit_ID, 0x06, 0x00, 0x01); err != nil {
		return err
	}
	time.Sleep(pulseWidth)
	_, err := m.Device.SendModbus(m.systemConfig.Limit_ID, 0x06, 0x00, 0x00)
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
	cmd := []byte{0x3e, 0x92, m.systemConfig.Motor_ID, 0x00}

	resp, err := m.Device.Send3EBus(cmd, 14, 0x3e)
	if err != nil {
		fmt.Println("获取位移数据失败:", err)
		return 0, err
	}

	if len(resp) == 14 && resp[1] == 0x92 {
		dataPart := resp[5:13]
		motorAngle := int64(binary.LittleEndian.Uint64(dataPart))
		length := float32(motorAngle) / 100.0
		Pos := float32(m.systemConfig.Motor_Direction) * length / m.systemConfig.Motor_Resolution

		return Pos, nil
	}

	return 0, nil
}

func (m *MINIMTSService) updateLoadData() (float32, error) {
	resp, err := m.Device.SendModbus(m.systemConfig.Weigh_ID, 0x03, 0x00, 0x02)
	if err != nil {
		fmt.Println("获取载荷数据失败:", err)
		return 0, err
	}

	if len(resp) < 7 {
		return 0, fmt.Errorf("invalid response length")
	}

	dataSwapped := []byte{resp[4], resp[3], resp[6], resp[5]}
	rawLoad := int32(binary.LittleEndian.Uint32(dataSwapped))

	load := float32(m.systemConfig.Weigh_Direction) * float32(rawLoad) / float32(m.systemConfig.Weigh_Resolution)

	return load, nil
}

func (m *MINIMTSService) updateTempData() (float32, error) {
	resp, err := m.Device.SendModbus(m.systemConfig.Temp_ID, 0x04, 0x00, 0x01)
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

	resp, err := m.Device.SendModbus(m.systemConfig.Limit_ID, 0x04, 0x00, 0x02)
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
