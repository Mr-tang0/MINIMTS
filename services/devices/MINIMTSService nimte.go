package devices

// import (
// 	"MINIMTS/services/config"
// 	"encoding/csv"
// 	"fmt"
// 	"os"
// 	"path/filepath"
// 	"reflect"
// 	"strconv"
// 	"strings"
// 	"sync"
// 	"time"

// 	"github.com/Mr-tang0/PIMSGoMod/protocol/XinJie"
// 	"github.com/wailsapp/wails/v3/pkg/application"
// )

// const (
// 	Y_SPEED_HD  uint16 = 2
// 	Y_POS_HD    uint16 = 4
// 	MTS_FORCE_D uint16 = 88
// 	Y_JOG_M     uint16 = 1
// 	Y_STOP_M    uint16 = 2
// )

// type MINIMTSHeartbeat struct {
// 	Load     float32 `json:"load"`     // 当前载荷
// 	Stress   float32 `json:"stress"`   // 当前应力值
// 	ZeroLoad float32 `json:"zeroLoad"` // 归零后的载荷值

// 	Disp     float32 `json:"disp"`     // 当前位移
// 	Strain   float32 `json:"strain"`   // 当前应变值
// 	ZeroDisp float32 `json:"zeroDisp"` // 归零后的位移值

// 	EPos        float32 `json:"ePos"`        // 当前视频位置
// 	EZeroPos    float32 `json:"eZeroPos"`    // 归零后的视频位置值
// 	EStrain     float32 `json:"eStrain"`     // 当前视频应变值
// 	EZeroStrain float32 `json:"eZeroStrain"` // 归零后的视频应变值

// 	LimitDIR    string  `json:"limitDIR"`    // 限位被触发方向（idle-无限位，cw-正向限位，ccw-反向限位）
// 	Temperature float32 `json:"temperature"` // 当前温度值

// 	Duration  float32   `json:"duration"`  // 实验时间，单位秒
// 	StartTime time.Time `json:"startTime"` // 实验开始时间
// }

// type MINIMTSService struct {
// 	heartbeat   MINIMTSHeartbeat
// 	heartbeatMu sync.RWMutex       // 保护 heartbeat 的读写锁
// 	dataList    []MINIMTSHeartbeat // 数据列表，用于存储心跳数据
// 	cacheActive bool               // 是否处于测量中（仅在测量过程中缓存数据，heartbeatMu 保护）
// 	cacheSaveMu sync.Mutex         // 串行化文件保存，避免定时保存相互覆盖

// 	PLC       *XinJie.XinjieClient // 连接句柄
// 	plcOpened bool                 // 是否已打开连接

// 	systemConfig  *config.SystemConfig  // 系统配置
// 	projectConfig *config.ProjectConfig // 项目配置

// 	extensometer *HIKExtensometerService // 引伸计服务，用于读取比例标定系数

// 	videoMu           sync.Mutex
// 	videoBaseDistance float64 // ROI A/B 的初始欧几里得距离（像素）
// 	videoBaseReady    bool    // 初始距离是否已记录

// 	eposFiltered     float32 // 视频位移一阶低通输出（heartbeatMu 保护）
// 	estrainFiltered  float32 // 视频应变一阶低通输出（heartbeatMu 保护）
// 	videoFilterReady bool    // 一阶低通状态是否已初始化（heartbeatMu 保护）

// 	eposWindow       [videoWindowSize]float32 // 视频位移滑动窗口环形缓冲（heartbeatMu 保护）
// 	estrainWindow    [videoWindowSize]float32 // 视频应变滑动窗口环形缓冲（heartbeatMu 保护）
// 	videoWindowIdx   int                      // 滑动窗口下一个写入位置（heartbeatMu 保护）
// 	videoWindowCount int                      // 滑动窗口已填充样本数（heartbeatMu 保护）

// 	triggerStopCh chan struct{}

// 	testMu     sync.Mutex
// 	testStopCh chan struct{}

// 	pauseMu     sync.Mutex
// 	pauseStopCh chan struct{}
// 	pauseDoneCh chan struct{}

// 	blockedMu        sync.RWMutex
// 	blockedDirection int // 0=无限位；+1=正向被限位（禁止 speed>0）；-1=反向被限位（禁止 speed<0）
// }

// func NewMINIMTSService(extensometer *HIKExtensometerService) *MINIMTSService {
// 	svc := &MINIMTSService{
// 		heartbeat: MINIMTSHeartbeat{
// 			Load:     0,
// 			Stress:   0,
// 			ZeroLoad: 0,

// 			Disp:     0,
// 			Strain:   0,
// 			ZeroDisp: 0,

// 			EPos:        0,
// 			EZeroPos:    0,
// 			EStrain:     0,
// 			EZeroStrain: 0,

// 			LimitDIR:    "idle",
// 			Temperature: 25,
// 			Duration:    0,
// 		},
// 		PLC:          XinJie.NewXinjieClient(""),
// 		extensometer: extensometer,
// 	}
// 	return svc
// }

// func (m *MINIMTSService) Startup(systemConfig *config.SystemConfig, projectConfig *config.ProjectConfig) error {
// 	fmt.Println("MTS Startup")

// 	m.systemConfig = systemConfig
// 	m.projectConfig = projectConfig
// 	return nil
// }

// // ListenExtensometer 订阅引伸计结果事件，根据 ROI 距离计算视频位移与应变并写入 heartbeat。
// // 需在 application 初始化之后调用。
// func (m *MINIMTSService) ListenExtensometer() {
// 	app := application.Get()
// 	if app == nil {
// 		return
// 	}

// 	// 引伸计每帧推送两个 ROI 中心坐标与欧几里得距离
// 	app.Event.On("hik_extensometer_result", func(event *application.CustomEvent) {
// 		result, ok := event.Data.(ExtensometerResult)
// 		if !ok {
// 			return
// 		}
// 		m.updateVideoMetrics(result.Distance)
// 	})

// 	// ROI 重新绘制后，距离基准需要重新采集
// 	app.Event.On("hik_roi_selected", func(event *application.CustomEvent) {
// 		m.resetExtensometerBaseline()
// 	})
// }

// // videoWindowSize 滑动窗口平均滤波器的窗口大小。
// const videoWindowSize = 5

// // videoFilterAlpha 视频位移/应变一阶低通滤波系数（0~1，越小越平滑但滞后越大）。
// // 取 0.4 时噪声标准差约衰减为原来的 1/2.5，时间常数约为 2~3 帧。
// // 一阶低通后再叠加窗口为 videoWindowSize 的滑动窗口平均，进一步抑制随机噪声。
// const videoFilterAlpha = 0.4

// // updateVideoMetrics 根据两个 ROI 的欧几里得距离计算视频位移与应变。
// //
// // EPos = 当前距离（已比例标定时换算为 mm，否则为像素）
// // EStrain = (当前距离 - 初始距离) / 初始距离
// //
// // 引伸计原始数据存在约 0.01 像素的随机噪声，输出前做一阶低通滤波。
// func (m *MINIMTSService) updateVideoMetrics(distance float64) {
// 	if distance <= 0 {
// 		return
// 	}

// 	m.videoMu.Lock()
// 	if !m.videoBaseReady {
// 		m.videoBaseDistance = distance
// 		m.videoBaseReady = true
// 	}
// 	baseDistance := m.videoBaseDistance
// 	m.videoMu.Unlock()

// 	epos := distance

// 	var estrain float64
// 	if baseDistance > 0 {
// 		estrain = (distance - baseDistance) / baseDistance
// 	}

// 	direction := float32(1)
// 	if m.systemConfig != nil {
// 		direction = float32(m.systemConfig.Weigh_Direction)
// 	}

// 	m.heartbeatMu.Lock()
// 	// 一阶低通滤波抑制随机噪声；先滤波再扣归零偏移，归零才能立即生效
// 	eposValue := float32(epos) * direction
// 	estrainValue := float32(estrain) * direction
// 	if m.videoFilterReady {
// 		m.eposFiltered += (eposValue - m.eposFiltered) * videoFilterAlpha
// 		m.estrainFiltered += (estrainValue - m.estrainFiltered) * videoFilterAlpha
// 	} else {
// 		m.eposFiltered = eposValue
// 		m.estrainFiltered = estrainValue
// 		m.videoFilterReady = true
// 	}

// 	// 一阶低通输出送入滑动窗口环形缓冲，取窗口均值进一步平滑
// 	idx := m.videoWindowIdx
// 	m.eposWindow[idx] = m.eposFiltered
// 	m.estrainWindow[idx] = m.estrainFiltered
// 	m.videoWindowIdx = (idx + 1) % videoWindowSize
// 	if m.videoWindowCount < videoWindowSize {
// 		m.videoWindowCount++
// 	}
// 	var eposSum, estrainSum float32
// 	for i := 0; i < m.videoWindowCount; i++ {
// 		eposSum += m.eposWindow[i]
// 		estrainSum += m.estrainWindow[i]
// 	}
// 	eposAvg := eposSum / float32(m.videoWindowCount)
// 	estrainAvg := estrainSum / float32(m.videoWindowCount)

// 	m.heartbeat.EPos = eposAvg - m.heartbeat.EZeroPos
// 	m.heartbeat.EStrain = estrainAvg - m.heartbeat.EZeroStrain
// 	m.heartbeatMu.Unlock()
// }

// // resetExtensometerBaseline 清除视频距离基准（ROI 变更时调用）
// func (m *MINIMTSService) resetExtensometerBaseline() {
// 	m.videoMu.Lock()
// 	m.videoBaseDistance = 0
// 	m.videoBaseReady = false
// 	m.videoMu.Unlock()

// 	m.heartbeatMu.Lock()
// 	m.heartbeat.EPos = 0
// 	m.heartbeat.EStrain = 0
// 	m.heartbeat.EZeroPos = 0    // ROI 变更后像素基准已不同，归零偏移需一并清除
// 	m.heartbeat.EZeroStrain = 0 // 应变归零偏移同样失效
// 	m.videoFilterReady = false  // 一阶低通状态随基准一同重置
// 	m.eposWindow = [videoWindowSize]float32{}
// 	m.estrainWindow = [videoWindowSize]float32{}
// 	m.videoWindowIdx = 0
// 	m.videoWindowCount = 0
// 	m.heartbeatMu.Unlock()
// }

// func (m *MINIMTSService) OpenMINIMTS(ip string) error {
// 	m.heartbeatMu.Lock()
// 	defer m.heartbeatMu.Unlock()

// 	if m.plcOpened {
// 		return fmt.Errorf("plc已打开")
// 	}
// 	if err := m.PLC.OpenTCP(ip, 1); err != nil {
// 		return fmt.Errorf("无法打开ip %s: %v", ip, err)
// 	}
// 	m.plcOpened = true
// 	m.heartbeat.StartTime = time.Now()
// 	go m.go_heartbeat()

// 	fmt.Println("连接成功，心跳协程已启动")
// 	return nil
// }

// func (m *MINIMTSService) CloseMINIMTS() error {
// 	m.heartbeatMu.Lock()
// 	defer m.heartbeatMu.Unlock()

// 	m.PLC.Close()
// 	m.plcOpened = false
// 	return nil
// }

// func (m *MINIMTSService) IsOpened() bool {
// 	return m.plcOpened
// }

// func (m *MINIMTSService) go_heartbeat() {
// 	scanTicker := time.NewTicker(25 * time.Millisecond)
// 	cacheTicker := time.NewTicker(60 * time.Second)

// 	defer scanTicker.Stop()
// 	defer cacheTicker.Stop()

// 	for {
// 		select {
// 		case <-scanTicker.C:
// 			if err := m.go_scan(); err != nil {
// 				return
// 			}
// 			m.heartbeatMu.Lock()
// 			m.heartbeat.Duration = float32(time.Since(m.heartbeat.StartTime).Milliseconds()) / 1000.0
// 			heartbeat := m.heartbeat
// 			m.heartbeatMu.Unlock()

// 			application.Get().Event.Emit("mts-heartbeat", heartbeat)
// 		case <-cacheTicker.C:
// 			// 缓存数据自动保存（仅在测量过程中）
// 			m.heartbeatMu.RLock()
// 			active := m.cacheActive
// 			m.heartbeatMu.RUnlock()
// 			if active {
// 				go m.go_save_cache() //每分钟存储缓存数据
// 			}
// 		}
// 	}
// }

// func (m *MINIMTSService) go_scan() error {
// 	m.heartbeatMu.Lock()
// 	defer m.heartbeatMu.Unlock()

// 	if !m.IsOpened() {
// 		return fmt.Errorf("设备未连接")
// 	}

// 	// 更新电机位置
// 	if pos, err := m.updateMotorPosition(); err == nil {
// 		pos -= m.heartbeat.ZeroDisp
// 		m.heartbeat.Disp = pos
// 		m.heartbeat.Strain = pos / m.projectConfig.SectionLength
// 	}

// 	// 更新载荷数据
// 	if load, err := m.updateLoadData(); err == nil {
// 		load -= m.heartbeat.ZeroLoad
// 		m.heartbeat.Load = load
// 		m.heartbeat.Stress = load / m.projectConfig.SectionArea
// 	}

// 	// 缓存保存数据到列表（仅在测量过程中缓存）
// 	if m.cacheActive {
// 		m.dataList = append(m.dataList, m.heartbeat)
// 	}
// 	return nil
// }

// func (m *MINIMTSService) CallExit() {
// 	fmt.Println("MTS CallExit")

// 	if err := m.CloseMINIMTS(); err != nil {
// 		fmt.Println("关闭MINIMTS失败:", err)
// 	}
// }

// func (m *MINIMTSService) GetMINIMTSDevices() ([]string, error) {
// 	fmt.Println("GetMINIMTSDevices")
// 	return []string{"192.168.11.6"}, nil
// }

// func (m *MINIMTSService) IsMINIMTSConnected() bool {
// 	return m.plcOpened
// }

// // go_save_cache 自动缓存数据到本地文件，防止未手动保存导致数据丢失。
// func (m *MINIMTSService) go_save_cache() {
// 	if err := m.saveCacheFile(); err != nil {
// 		fmt.Println("保存缓存数据失败:", err)
// 	}
// }

// // cacheFileName 自动缓存文件名：项目文件名追加 "_cache"。
// func (m *MINIMTSService) cacheFileName() string {
// 	return strings.TrimSuffix(m.projectConfig.FileName, filepath.Ext(m.projectConfig.FileName)) + "_cache.csv"
// }

// // saveCacheFile 将内存中的心跳数据写入自动缓存文件（m.projectConfig.FilePath + "_cache" 文件名）。
// func (m *MINIMTSService) saveCacheFile() error {
// 	m.cacheSaveMu.Lock()
// 	defer m.cacheSaveMu.Unlock()

// 	if m.projectConfig == nil {
// 		return fmt.Errorf("项目配置未初始化")
// 	}
// 	path := m.projectConfig.FilePath
// 	if path == "" || m.projectConfig.FileName == "" {
// 		return fmt.Errorf("缓存保存路径和文件名不能为空")
// 	}
// 	name := m.cacheFileName()

// 	m.heartbeatMu.RLock()
// 	data := append([]MINIMTSHeartbeat(nil), m.dataList...)
// 	m.heartbeatMu.RUnlock()

// 	if err := os.MkdirAll(path, 0755); err != nil {
// 		return err
// 	}
// 	file, err := os.CreateTemp(path, ".minimts-cache-*.csv")
// 	if err != nil {
// 		return err
// 	}
// 	defer os.Remove(file.Name())

// 	if err := m.writeHeartbeatCSV(file, data); err != nil {
// 		file.Close()
// 		return err
// 	}
// 	if err := file.Close(); err != nil {
// 		return err
// 	}
// 	return os.Rename(file.Name(), filepath.Join(path, name))
// }

// // SaveCache 将当前缓存快照保存为 CSV；path 和 name 为空时使用项目配置。
// func (m *MINIMTSService) SaveCache() error {
// 	m.cacheSaveMu.Lock()
// 	defer m.cacheSaveMu.Unlock()

// 	if m.projectConfig == nil {
// 		return fmt.Errorf("项目配置未初始化")
// 	}
// 	path := m.projectConfig.FilePath
// 	name := m.projectConfig.FileName

// 	if path == "" || name == "" {
// 		return fmt.Errorf("缓存保存路径和文件名不能为空")
// 	}
// 	name = strings.TrimSuffix(name, filepath.Ext(name)) + ".csv"

// 	m.heartbeatMu.RLock()
// 	data := append([]MINIMTSHeartbeat(nil), m.dataList...)
// 	m.heartbeatMu.RUnlock()

// 	if err := os.MkdirAll(path, 0755); err != nil {
// 		return err
// 	}
// 	file, err := os.CreateTemp(path, ".minimts-*.csv")
// 	if err != nil {
// 		return err
// 	}
// 	defer os.Remove(file.Name())

// 	if err := m.writeHeartbeatCSV(file, data); err != nil {
// 		file.Close()
// 		return err
// 	}
// 	if err := file.Close(); err != nil {
// 		return err
// 	}
// 	if err := os.Rename(file.Name(), filepath.Join(path, uniqueFileName(path, name))); err != nil {
// 		return err
// 	}
// 	// 手动保存成功后删除自动缓存文件，避免保留重复数据
// 	os.Remove(filepath.Join(path, m.cacheFileName()))
// 	return nil
// }

// // writeHeartbeatCSV 向已打开的文件写入：项目元数据注释行 + 过滤后的心跳 CSV。
// // 相机未连接时会把 videoExtEnable / dicEnable 强制视为 false，并隐藏 ePos / eStrain 列。
// func (m *MINIMTSService) writeHeartbeatCSV(file *os.File, data []MINIMTSHeartbeat) error {
// 	// ---- 相机连接判定 ----
// 	camConnected := m.extensometer != nil && m.extensometer.IsCameraConnected()
// 	videoEnabled := camConnected && m.projectConfig.VideoExtEnable
// 	dicEnabled := camConnected && m.projectConfig.DICEnable

// 	// ---- 列过滤 ----
// 	excluded := map[string]struct{}{
// 		"zeroLoad":    {},
// 		"zeroDisp":    {},
// 		"eZeroPos":    {},
// 		"eZeroStrain": {},
// 		"limitDIR":    {},
// 		"temperature": {},
// 		"startTime":   {},
// 	}
// 	if !videoEnabled {
// 		excluded["ePos"] = struct{}{}
// 		excluded["eStrain"] = struct{}{}
// 	}

// 	typ := reflect.TypeOf(MINIMTSHeartbeat{})
// 	type fieldIdx struct {
// 		idx int
// 		tag string
// 	}
// 	var keep []fieldIdx
// 	for i := 0; i < typ.NumField(); i++ {
// 		tag := typ.Field(i).Tag.Get("json")
// 		if _, skip := excluded[tag]; skip {
// 			continue
// 		}
// 		keep = append(keep, fieldIdx{i, tag})
// 	}

// 	// ---- 项目元数据注释行 ----
// 	pc := m.projectConfig
// 	meta := []struct{ k, v string }{
// 		{"experimenter", pc.Experimenter},
// 		{"sampleNo", pc.SampleNo},
// 		{"testDate", pc.TestDate},
// 		{"sampleShape", pc.SampleShape},
// 		{"testType", fmt.Sprint(pc.TestType)},
// 		{"width", formatNumber(pc.Width)},
// 		{"thickness", formatNumber(pc.Thickness)},
// 		{"diameter", formatNumber(pc.Diameter)},
// 		{"sectionArea", formatNumber(pc.SectionArea)},
// 		{"sectionLength", formatNumber(pc.SectionLength)},
// 		{"speed", formatNumber(pc.Speed)},
// 		{"pauseCondition", pc.PauseCondition},
// 		{"triggerEnabled", fmt.Sprint(pc.TriggerEnabled)},
// 		{"triggerPeriodMs", fmt.Sprint(pc.TriggerPeriodMs)},
// 		{"triggerPulseWidthMs", fmt.Sprint(pc.TriggerPulseWidthMs)},
// 		{"videoExtEnable", fmt.Sprint(videoEnabled)},
// 		{"dicEnable", fmt.Sprint(dicEnabled)},
// 	}
// 	for _, item := range meta {
// 		if _, err := fmt.Fprintf(file, "# %s: %s\n", item.k, item.v); err != nil {
// 			return err
// 		}
// 	}

// 	// ---- CSV 数据 ----
// 	writer := csv.NewWriter(file)
// 	header := make([]string, len(keep))
// 	for i, f := range keep {
// 		header[i] = f.tag
// 	}
// 	if err := writer.Write(header); err != nil {
// 		return err
// 	}
// 	for _, hb := range data {
// 		value := reflect.ValueOf(hb)
// 		row := make([]string, len(keep))
// 		for i, f := range keep {
// 			field := value.Field(f.idx)
// 			switch field.Kind() {
// 			case reflect.Float32:
// 				row[i] = strconv.FormatFloat(field.Float(), 'f', -1, 32)
// 			case reflect.Float64:
// 				row[i] = strconv.FormatFloat(field.Float(), 'f', -1, 64)
// 			case reflect.String:
// 				row[i] = field.String()
// 			default:
// 				row[i] = fmt.Sprint(field.Interface())
// 			}
// 		}
// 		if err := writer.Write(row); err != nil {
// 			return err
// 		}
// 	}
// 	writer.Flush()
// 	return writer.Error()
// }

// // formatNumber 去除浮点尾部多余零
// func formatNumber(f float32) string {
// 	return strconv.FormatFloat(float64(f), 'f', -1, 32)
// }

// // uniqueFileName 在目录 dir 下为 baseName 寻找一个不冲突的文件名。
// // 若 baseName 已存在，依次尝试 stem(1).ext、stem(2).ext … 直到空闲。
// func uniqueFileName(dir, baseName string) string {
// 	target := filepath.Join(dir, baseName)
// 	if _, err := os.Stat(target); os.IsNotExist(err) {
// 		return baseName
// 	}

// 	ext := filepath.Ext(baseName)
// 	stem := strings.TrimSuffix(baseName, ext)
// 	for i := 1; i < 10000; i++ {
// 		candidate := fmt.Sprintf("%s(%d)%s", stem, i, ext)
// 		if _, err := os.Stat(filepath.Join(dir, candidate)); os.IsNotExist(err) {
// 			return candidate
// 		}
// 	}
// 	// 理论上不会走到
// 	return baseName
// }

// // ClearDataCache 清空内存中的心跳数据缓存，不删除已保存的文件。
// func (m *MINIMTSService) ClearDataCache() {
// 	m.heartbeatMu.Lock()
// 	m.dataList = nil
// 	m.heartbeatMu.Unlock()

// 	m.CallDataToZero("time")
// }

// // CallDataToZero 数据归零函数
// func (m *MINIMTSService) CallDataToZero(dataType string) error {
// 	m.heartbeatMu.Lock()
// 	defer m.heartbeatMu.Unlock()

// 	switch dataType {
// 	case "load", "stress":
// 		m.heartbeat.ZeroLoad += m.heartbeat.Load
// 	case "disp", "strain":
// 		m.heartbeat.ZeroDisp += m.heartbeat.Disp
// 	case "espos", "ePos", "videoDisp":
// 		m.heartbeat.EZeroPos += m.heartbeat.EPos
// 		m.heartbeat.EZeroStrain += m.heartbeat.EStrain
// 	case "time":
// 		m.heartbeat.StartTime = time.Now()
// 	default:
// 		return fmt.Errorf("invalid data type: %s", dataType)
// 	}
// 	return nil
// }

// // MotorOn 电机使能
// func (m *MINIMTSService) MotorOn() error {
// 	return nil
// }

// // MotorOff 电机禁止使能
// func (m *MINIMTSService) MotorOff() error {
// 	return nil
// }

// // MotorStop 电机停止
// func (m *MINIMTSService) MotorStop() error {
// 	return m.PLC.WriteCoil(XinJie.M(Y_STOP_M), true)
// }

// // MotorJog 电机速度模式移动
// func (m *MINIMTSService) MotorJog(speed float32) error {
// 	if speed == 0 {
// 		return m.MotorStop()
// 	}
// 	if err := m.PLC.WriteRegister(XinJie.HD(Y_SPEED_HD), speed, XinJie.Float32, false); err != nil {
// 		return err
// 	}
// 	return m.PLC.WriteCoil(XinJie.M(Y_JOG_M), true)
// }

// func (m *MINIMTSService) Trigger(pulseWidth time.Duration) error {
// 	return nil
// }

// func (m *MINIMTSService) startTriggerLoop(period, pulseWidth time.Duration) {
// 	m.stopTriggerLoop()

// 	stopCh := make(chan struct{})
// 	doneCh := make(chan struct{})
// 	m.triggerStopCh = stopCh

// 	go func() {
// 		defer close(doneCh)
// 		ticker := time.NewTicker(period)
// 		defer ticker.Stop()

// 		m.Trigger(pulseWidth)

// 		for {
// 			select {
// 			case <-stopCh:
// 				return
// 			case <-ticker.C:
// 				if err := m.Trigger(pulseWidth); err != nil {
// 					fmt.Println("发送触发信号失败:", err)
// 				}
// 			}
// 		}
// 	}()
// }

// func (m *MINIMTSService) stopTriggerLoop() {
// 	stopCh := m.triggerStopCh
// 	m.triggerStopCh = nil
// 	if stopCh != nil {
// 		close(stopCh)
// 	}
// }

// // emitDICCapture 通知相机服务开始/停止 DIC 图像保存；实际采集与落盘由相机服务负责
// func (m *MINIMTSService) emitDICCapture(start bool) {
// 	app := application.Get()
// 	if app == nil {
// 		return
// 	}
// 	if !start {
// 		app.Event.Emit("dic_capture_stop", nil)
// 		return
// 	}
// 	app.Event.Emit("dic_capture_start", DICCaptureConfig{
// 		Folder:   m.projectConfig.DICFolder,
// 		FileName: m.projectConfig.DICFileName,
// 		PeriodMs: m.projectConfig.DICCapturePeriodMs,
// 	})
// }

// // StartMeasurement 开始测量逻辑
// func (m *MINIMTSService) StartMeasurement() error {
// 	if m.projectConfig.TriggerEnabled {
// 		if m.projectConfig.TriggerPeriodMs <= 0 {
// 			return fmt.Errorf("触发周期必须大于 0 ms")
// 		}
// 		if m.projectConfig.TriggerPulseWidthMs <= 0 || m.projectConfig.TriggerPulseWidthMs > m.projectConfig.TriggerPeriodMs {
// 			return fmt.Errorf("触发脉宽必须大于 0 ms 且不能超过触发周期")
// 		}
// 	}

// 	if m.projectConfig.DICEnable {
// 		if m.projectConfig.DICCapturePeriodMs <= 0 {
// 			return fmt.Errorf("DIC 拍摄周期必须大于 0 ms")
// 		}
// 		if m.projectConfig.DICFolder == "" {
// 			return fmt.Errorf("DIC 图片保存路径不能为空")
// 		}
// 	}

// 	if m.projectConfig.TriggerEnabled {
// 		m.startTriggerLoop(
// 			time.Duration(m.projectConfig.TriggerPeriodMs)*time.Millisecond,
// 			time.Duration(m.projectConfig.TriggerPulseWidthMs)*time.Millisecond,
// 		)
// 	}

// 	if m.projectConfig.DICEnable {
// 		m.emitDICCapture(true)
// 	}

// 	if err := m.MotorOn(); err != nil {
// 		return err
// 	}

// 	if err := m.MotorJog(m.projectConfig.Speed * float32(m.projectConfig.TestType)); err != nil {
// 		return err
// 	}

// 	// 测量开始后才启用数据缓存与定时自动保存
// 	m.heartbeatMu.Lock()
// 	m.cacheActive = true
// 	m.heartbeatMu.Unlock()

// 	return nil
// }

// // StopMeasurement 停止测量
// func (m *MINIMTSService) StopMeasurement() error {
// 	// 停止测量后不再缓存数据
// 	m.heartbeatMu.Lock()
// 	m.cacheActive = false
// 	m.heartbeatMu.Unlock()

// 	m.stopLoadControl()
// 	err := m.MotorStop()
// 	m.stopTriggerLoop()
// 	m.emitDICCapture(false)
// 	return err
// }

// // PauseMeasurement 根据项目配置暂停测量。
// func (m *MINIMTSService) PauseMeasurement() error {
// 	if m.projectConfig == nil {
// 		return fmt.Errorf("项目配置未初始化")
// 	}

// 	m.stopTriggerLoop()
// 	m.emitDICCapture(false)

// 	switch m.projectConfig.PauseCondition {
// 	case "stop", "disp-maintain":
// 		m.stopLoadControl()
// 		return m.MotorStop()
// 	case "load-maintain":
// 		return m.startLoadControl()
// 	default:
// 		return fmt.Errorf("暂停条件 %s 未知", m.projectConfig.PauseCondition)
// 	}

// }

// // ResumeMeasurement 结束暂停状态并恢复试验速度。
// func (m *MINIMTSService) ResumeMeasurement() error {
// 	if m.projectConfig == nil {
// 		return fmt.Errorf("项目配置未初始化")
// 	}

// 	if m.projectConfig.TriggerEnabled {
// 		m.startTriggerLoop(
// 			time.Duration(m.projectConfig.TriggerPeriodMs)*time.Millisecond,
// 			time.Duration(m.projectConfig.TriggerPulseWidthMs)*time.Millisecond,
// 		)
// 	}

// 	if m.projectConfig.DICEnable {
// 		m.emitDICCapture(true)
// 	}

// 	m.stopLoadControl()
// 	return m.MotorJog(m.projectConfig.Speed * float32(m.projectConfig.TestType))
// }

// func (m *MINIMTSService) startLoadControl() error {
// 	if m.systemConfig == nil {
// 		return fmt.Errorf("系统配置未初始化")
// 	}
// 	if m.systemConfig.PID_DeltaT <= 0 {
// 		return fmt.Errorf("PID 控制周期必须大于 0")
// 	}
// 	if m.systemConfig.PID_MaxSpeed < m.systemConfig.PID_MinSpeed {
// 		return fmt.Errorf("PID 最大速度不能小于最小速度")
// 	}

// 	m.stopLoadControl()

// 	m.heartbeatMu.RLock()
// 	targetLoad := m.heartbeat.Load
// 	m.heartbeatMu.RUnlock()

// 	stopCh := make(chan struct{})
// 	doneCh := make(chan struct{})
// 	m.pauseMu.Lock()
// 	m.pauseStopCh = stopCh
// 	m.pauseDoneCh = doneCh
// 	m.pauseMu.Unlock()

// 	go m.runLoadControl(stopCh, doneCh, targetLoad)
// 	return nil
// }

// func (m *MINIMTSService) runLoadControl(stopCh <-chan struct{}, doneCh chan<- struct{}, targetLoad float32) {
// 	defer close(doneCh)

// 	deltaT := m.systemConfig.PID_DeltaT
// 	ticker := time.NewTicker(time.Duration(deltaT * float64(time.Second)))
// 	defer ticker.Stop()

// 	integral := 0.0
// 	previousError := 0.0
// 	for {
// 		select {
// 		case <-stopCh:
// 			return
// 		case <-ticker.C:
// 			m.heartbeatMu.RLock()
// 			currentLoad := m.heartbeat.Load
// 			m.heartbeatMu.RUnlock()

// 			errorValue := float64(targetLoad - currentLoad)
// 			integral += errorValue * deltaT
// 			derivative := (errorValue - previousError) / deltaT
// 			output := (m.systemConfig.PID_P/1000*errorValue +
// 				m.systemConfig.PID_I/1000*integral +
// 				m.systemConfig.PID_D/1000*derivative) * 0.1

// 			if output > m.systemConfig.PID_MaxSpeed {
// 				output = m.systemConfig.PID_MaxSpeed
// 			} else if output < m.systemConfig.PID_MinSpeed {
// 				output = m.systemConfig.PID_MinSpeed
// 			}

// 			motorOutput := float32(m.systemConfig.PID_DIR*m.projectConfig.TestType) * float32(output)
// 			if err := m.MotorJog(motorOutput); err != nil {
// 				fmt.Println("载荷维持控制失败:", err)
// 			}
// 			previousError = errorValue
// 		}
// 	}
// }

// func (m *MINIMTSService) stopLoadControl() {
// 	m.pauseMu.Lock()
// 	stopCh := m.pauseStopCh
// 	doneCh := m.pauseDoneCh
// 	m.pauseStopCh = nil
// 	m.pauseDoneCh = nil
// 	m.pauseMu.Unlock()

// 	if stopCh != nil {
// 		close(stopCh)
// 		<-doneCh
// 	}
// }

// // EmergencyStop 紧急停止
// func (m *MINIMTSService) EmergencyStop() error {
// 	m.stopLoadControl()
// 	stopErr := m.MotorStop()
// 	m.stopTriggerLoop()
// 	m.emitDICCapture(false)
// 	if stopErr != nil {
// 		return stopErr
// 	}
// 	return m.MotorOff()
// }

// func (m *MINIMTSService) updateMotorPosition() (float32, error) {
// 	rawPos, err := m.PLC.ReadRegister(XinJie.HD(Y_POS_HD), XinJie.Float32, false)
// 	if err != nil {
// 		return 0, err
// 	}
// 	return rawPos.(float32), nil
// }

// func (m *MINIMTSService) updateLoadData() (float32, error) {
// 	rawLoad, err := m.PLC.ReadRegister(XinJie.HD(MTS_FORCE_D), XinJie.Float32, false)
// 	if err != nil {
// 		return 0, err
// 	}
// 	load := float32(m.systemConfig.Weigh_Direction) * rawLoad.(float32) / float32(m.systemConfig.Weigh_Resolution)
// 	return load, nil
// }

// func (m *MINIMTSService) updateTempData() (float32, error) {
// 	return 0, nil
// }

// func (m *MINIMTSService) updateLimitStatus() (int, error) {
// 	return 0, nil
// }

// // GetBlockedDirection 返回当前被限位的方向（0=无，+1=正向挡，-1=反向挡）
// func (m *MINIMTSService) GetBlockedDirection() int {
// 	m.blockedMu.RLock()
// 	defer m.blockedMu.RUnlock()
// 	return m.blockedDirection
// }
