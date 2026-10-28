package devices

import (
	"MINIMTS/services/config"
	"encoding/binary"
	"encoding/csv"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
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

	EPos        float32 `json:"ePos"`        // 当前视频位置
	EZeroPos    float32 `json:"eZeroPos"`    // 归零后的视频位置值
	EStrain     float32 `json:"eStrain"`     // 当前视频应变值
	EZeroStrain float32 `json:"eZeroStrain"` // 归零后的视频应变值

	LimitDIR    string  `json:"limitDIR"`    // 限位被触发方向（idle-无限位，cw-正向限位，ccw-反向限位）
	Temperature float32 `json:"temperature"` // 当前温度值

	Duration  float32   `json:"duration"`  // 实验时间，单位秒
	StartTime time.Time `json:"startTime"` // 实验开始时间
}

type MINIMTSService struct {
	heartbeat   MINIMTSHeartbeat
	heartbeatMu sync.RWMutex       // 保护 heartbeat 的读写锁
	dataList    []MINIMTSHeartbeat // 数据列表，用于存储心跳数据
	cacheSaveMu sync.Mutex         // 串行化文件保存，避免定时保存相互覆盖

	Device *protocol.SerialCommunicator //连接句柄

	systemConfig  *config.SystemConfig  // 系统配置
	projectConfig *config.ProjectConfig // 项目配置

	extensometer *HIKExtensometerService // 引伸计服务，用于读取比例标定系数

	videoMu           sync.Mutex
	videoBaseDistance float64 // ROI A/B 的初始欧几里得距离（像素）
	videoBaseReady    bool    // 初始距离是否已记录

	eposFiltered     float32 // 视频位移滤波状态（heartbeatMu 保护）
	estrainFiltered  float32 // 视频应变滤波状态（heartbeatMu 保护）
	videoFilterReady bool    // 滤波状态是否已初始化（heartbeatMu 保护）

	triggerStopCh chan struct{}

	testMu     sync.Mutex
	testStopCh chan struct{}

	pauseMu     sync.Mutex
	pauseStopCh chan struct{}
	pauseDoneCh chan struct{}
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

			EPos:        0,
			EZeroPos:    0,
			EStrain:     0,
			EZeroStrain: 0,

			LimitDIR:    "idle",
			Temperature: 25,
			Duration:    0,
		},
		Device: &protocol.SerialCommunicator{
			MaxRetries: 3,
			Timeout:    100 * time.Millisecond,
		},
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

// videoFilterAlpha 视频位移/应变一阶低通滤波系数（0~1，越小越平滑但滞后越大）。
// 取 0.2 时噪声标准差约衰减为原来的 1/3，时间常数约为 4 帧。
const videoFilterAlpha = 0.2

// updateVideoMetrics 根据两个 ROI 的欧几里得距离计算视频位移与应变。
//
// EPos = 当前距离（已比例标定时换算为 mm，否则为像素）
// EStrain = (当前距离 - 初始距离) / 初始距离
//
// 引伸计原始数据存在约 0.01 像素的随机噪声，输出前做一阶低通滤波。
func (m *MINIMTSService) updateVideoMetrics(distance float64) {
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

	direction := float32(1)
	if m.systemConfig != nil {
		direction = float32(m.systemConfig.Weigh_Direction)
	}

	m.heartbeatMu.Lock()
	// 一阶低通滤波抑制随机噪声；先滤波再扣归零偏移，归零才能立即生效
	eposValue := float32(epos) * direction
	estrainValue := float32(estrain) * direction
	if m.videoFilterReady {
		m.eposFiltered += (eposValue - m.eposFiltered) * videoFilterAlpha
		m.estrainFiltered += (estrainValue - m.estrainFiltered) * videoFilterAlpha
	} else {
		m.eposFiltered = eposValue
		m.estrainFiltered = estrainValue
		m.videoFilterReady = true
	}
	m.heartbeat.EPos = m.eposFiltered - m.heartbeat.EZeroPos
	m.heartbeat.EStrain = m.estrainFiltered - m.heartbeat.EZeroStrain
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
	m.heartbeat.EZeroPos = 0    // ROI 变更后像素基准已不同，归零偏移需一并清除
	m.heartbeat.EZeroStrain = 0 // 应变归零偏移同样失效
	m.videoFilterReady = false  // 滤波状态随基准一同重置
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
	// 单次扫描实测约 32ms（峰值 40ms），周期需大于该耗时，否则 ticker 会持续积压
	scanTicker := time.NewTicker(50 * time.Millisecond) // 扫描数据周期
	cacheTicker := time.NewTicker(60 * time.Second)     // 缓存数据自动保存周期，单位秒

	defer scanTicker.Stop()
	defer cacheTicker.Stop()

	for {
		select {
		case <-scanTicker.C:
			// 扫描数据
			if err := m.go_scan(); err != nil {
				return
			}
			//更新时间
			m.heartbeatMu.RLock()
			m.heartbeat.Duration = float32(time.Now().Sub(m.heartbeat.StartTime).Seconds())
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
		pos -= m.heartbeat.ZeroDisp
		m.heartbeat.Disp = pos
		m.heartbeat.Strain = pos / m.projectConfig.SectionLength
	}

	// 更新载荷数据
	if load, err := m.updateLoadData(); err == nil {
		load -= m.heartbeat.ZeroLoad
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
	fmt.Println("go_save_cache")
	// if err := m.SaveCache(); err != nil {
	// 	fmt.Println("保存缓存数据失败:", err)
	// }
}

// SaveCache 将当前缓存快照保存为 CSV；path 和 name 为空时使用项目配置。
func (m *MINIMTSService) SaveCache() error {
	m.cacheSaveMu.Lock()
	defer m.cacheSaveMu.Unlock()

	if m.projectConfig == nil {
		return fmt.Errorf("项目配置未初始化")
	}
	path := m.projectConfig.FilePath
	name := m.projectConfig.FileName

	if path == "" || name == "" {
		return fmt.Errorf("缓存保存路径和文件名不能为空")
	}
	name = strings.TrimSuffix(name, filepath.Ext(name)) + ".csv"

	m.heartbeatMu.RLock()
	data := append([]MINIMTSHeartbeat(nil), m.dataList...)
	m.heartbeatMu.RUnlock()

	if err := os.MkdirAll(path, 0755); err != nil {
		return err
	}
	file, err := os.CreateTemp(path, ".minimts-*.csv")
	if err != nil {
		return err
	}
	defer os.Remove(file.Name())

	writer := csv.NewWriter(file)
	typ := reflect.TypeOf(MINIMTSHeartbeat{})
	header := make([]string, typ.NumField())
	for i := range header {
		header[i] = typ.Field(i).Tag.Get("json")
	}
	if err := writer.Write(header); err != nil {
		file.Close()
		return err
	}
	for _, heartbeat := range data {
		value := reflect.ValueOf(heartbeat)
		row := make([]string, len(header))
		for i := range row {
			field := value.Field(i)
			if field.Type() == reflect.TypeOf(time.Time{}) {
				row[i] = field.Interface().(time.Time).Format(time.RFC3339Nano)
			} else if field.Kind() == reflect.Float32 {
				row[i] = strconv.FormatFloat(field.Float(), 'f', -1, 32)
			} else {
				row[i] = fmt.Sprint(field.Interface())
			}
		}
		if err := writer.Write(row); err != nil {
			file.Close()
			return err
		}
	}
	writer.Flush()
	if err := writer.Error(); err != nil {
		file.Close()
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	return os.Rename(file.Name(), filepath.Join(path, name))
}

// ClearDataCache 清空内存中的心跳数据缓存，不删除已保存的文件。
func (m *MINIMTSService) ClearDataCache() {
	m.heartbeatMu.Lock()
	m.dataList = nil
	m.heartbeatMu.Unlock()
}

// CallDataToZero 数据归零函数
func (m *MINIMTSService) CallDataToZero(dataType string) error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	switch dataType {
	case "load", "stress":
		m.heartbeat.ZeroLoad += m.heartbeat.Load
	case "disp", "strain":
		m.heartbeat.ZeroDisp += m.heartbeat.Disp
	case "espos", "ePos", "videoDisp":
		m.heartbeat.EZeroPos += m.heartbeat.EPos
		m.heartbeat.EZeroStrain += m.heartbeat.EStrain
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
	m.stopLoadControl()
	err := m.MotorStop()
	m.stopTriggerLoop()
	return err
}

// PauseMeasurement 根据项目配置暂停测量。
func (m *MINIMTSService) PauseMeasurement() error {
	if m.projectConfig == nil {
		return fmt.Errorf("项目配置未初始化")
	}

	switch m.projectConfig.PauseCondition {
	case "stop", "disp-maintain":
		m.stopLoadControl()
		return m.MotorStop()
	case "load-maintain":
		return m.startLoadControl()
	default:
		return fmt.Errorf("暂停条件 %s 未知", m.projectConfig.PauseCondition)
	}
}

// ResumeMeasurement 结束暂停状态并恢复试验速度。
func (m *MINIMTSService) ResumeMeasurement() error {
	if m.projectConfig == nil {
		return fmt.Errorf("项目配置未初始化")
	}

	m.stopLoadControl()
	return m.MotorJog(m.projectConfig.Speed * float32(m.projectConfig.TestType))
}

func (m *MINIMTSService) startLoadControl() error {
	if m.systemConfig == nil {
		return fmt.Errorf("系统配置未初始化")
	}
	if m.systemConfig.PID_DeltaT <= 0 {
		return fmt.Errorf("PID 控制周期必须大于 0")
	}
	if m.systemConfig.PID_MaxSpeed < m.systemConfig.PID_MinSpeed {
		return fmt.Errorf("PID 最大速度不能小于最小速度")
	}

	m.stopLoadControl()

	m.heartbeatMu.RLock()
	targetLoad := m.heartbeat.Load
	m.heartbeatMu.RUnlock()

	stopCh := make(chan struct{})
	doneCh := make(chan struct{})
	m.pauseMu.Lock()
	m.pauseStopCh = stopCh
	m.pauseDoneCh = doneCh
	m.pauseMu.Unlock()

	go m.runLoadControl(stopCh, doneCh, targetLoad)
	return nil
}

func (m *MINIMTSService) runLoadControl(stopCh <-chan struct{}, doneCh chan<- struct{}, targetLoad float32) {
	defer close(doneCh)

	deltaT := m.systemConfig.PID_DeltaT
	ticker := time.NewTicker(time.Duration(deltaT * float64(time.Second)))
	defer ticker.Stop()

	integral := 0.0
	previousError := 0.0
	for {
		select {
		case <-stopCh:
			return
		case <-ticker.C:
			m.heartbeatMu.RLock()
			currentLoad := m.heartbeat.Load
			m.heartbeatMu.RUnlock()

			errorValue := float64(targetLoad - currentLoad)
			integral += errorValue * deltaT
			derivative := (errorValue - previousError) / deltaT
			output := (m.systemConfig.PID_P/1000*errorValue +
				m.systemConfig.PID_I/1000*integral +
				m.systemConfig.PID_D/1000*derivative) * 0.1

			if output > m.systemConfig.PID_MaxSpeed {
				output = m.systemConfig.PID_MaxSpeed
			} else if output < m.systemConfig.PID_MinSpeed {
				output = m.systemConfig.PID_MinSpeed
			}

			motorOutput := float32(m.systemConfig.PID_DIR*m.projectConfig.TestType) * float32(output)
			if err := m.MotorJog(motorOutput); err != nil {
				fmt.Println("载荷维持控制失败:", err)
			}
			previousError = errorValue
		}
	}
}

func (m *MINIMTSService) stopLoadControl() {
	m.pauseMu.Lock()
	stopCh := m.pauseStopCh
	doneCh := m.pauseDoneCh
	m.pauseStopCh = nil
	m.pauseDoneCh = nil
	m.pauseMu.Unlock()

	if stopCh != nil {
		close(stopCh)
		<-doneCh
	}
}

// EmergencyStop 紧急停止
func (m *MINIMTSService) EmergencyStop() error {
	m.stopLoadControl()
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
		Pos := float32(m.systemConfig.Weigh_Direction) * float32(m.systemConfig.Motor_Direction) * length / m.systemConfig.Motor_Resolution

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

	if len(resp) == 5 && resp[1]&0x80 != 0 {
		return 0, fmt.Errorf(
			"称重模块 Modbus 异常: slave=%d function=0x%02X code=0x%02X",
			resp[0], resp[1], resp[2],
		)
	}
	if len(resp) != 9 {
		return 0, fmt.Errorf("称重模块响应长度错误: got=%d frame=% X", len(resp), resp)
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
