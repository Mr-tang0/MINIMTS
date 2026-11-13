package devices

import (
	"MINIMTS/services/config"
	"MINIMTS/services/devices/minimts"
	"bufio"
	"encoding/csv"
	"encoding/json"
	"fmt"
	"math"
	"net"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"sync"
	"time"

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
	heartbeat         MINIMTSHeartbeat
	heartbeatMu       sync.RWMutex       // 保护 heartbeat 的读写锁
	dataList          []MINIMTSHeartbeat // 数据列表，用于存储心跳数据
	autoStopTriggered bool               // 本次试验是否已触发阈值停机，heartbeatMu 保护
	cacheActive       bool               // 是否处于测量中（仅在测量过程中缓存数据，heartbeatMu 保护）
	cacheSaveMu       sync.Mutex         // 串行化文件保存，避免定时保存相互覆盖

	driver   minimts.Driver // 启动时选定的设备协议驱动
	protocol string

	systemConfig  *config.SystemConfig  // 系统配置
	projectConfig *config.ProjectConfig // 项目配置

	extensometer *HIKExtensometerService // 引伸计服务，用于读取比例标定系数

	videoMu           sync.Mutex
	videoBaseDistance float64 // ROI A/B 的初始欧几里得距离（像素）
	videoBaseReady    bool    // 初始距离是否已记录

	eposFiltered     float32 // 视频位移一阶低通输出（heartbeatMu 保护）
	estrainFiltered  float32 // 视频应变一阶低通输出（heartbeatMu 保护）
	videoFilterReady bool    // 一阶低通状态是否已初始化（heartbeatMu 保护）

	eposWindow       [videoWindowSize]float32 // 视频位移滑动窗口环形缓冲（heartbeatMu 保护）
	estrainWindow    [videoWindowSize]float32 // 视频应变滑动窗口环形缓冲（heartbeatMu 保护）
	videoWindowIdx   int                      // 滑动窗口下一个写入位置（heartbeatMu 保护）
	videoWindowCount int                      // 滑动窗口已填充样本数（heartbeatMu 保护）

	triggerStopCh chan struct{}

	// Diffract 联机（与外部 Diffract 主程序的 TCP 连接）
	onlineMu        sync.Mutex
	onlinePort      int
	onlineConn      net.Conn
	onlineConnected bool
	diffractStopCh  chan struct{}                   // 衍射仪触发 loop 的停止信号
	diffractDoneCh  chan struct{}                   // loop 退出信号
	diffractNextNum int                             // 下一张 TIFF 的序号
	diffractPending map[string]chan json.RawMessage // save_path → 等 capture_done 的 channel
	diffractRespMu  sync.Mutex

	testMu     sync.Mutex
	testStopCh chan struct{}

	pauseMu     sync.Mutex
	pauseStopCh chan struct{}
	pauseDoneCh chan struct{}

	blockedMu        sync.RWMutex
	blockedDirection int // 0=无限位；+1=正向被限位（禁止 speed>0）；-1=反向被限位（禁止 speed<0）
}

func NewMINIMTSService(extensometer *HIKExtensometerService, protocol string) *MINIMTSService {
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
		protocol:     protocol,
		extensometer: extensometer,
	}
	return svc
}

func (m *MINIMTSService) Startup(systemConfig *config.SystemConfig, projectConfig *config.ProjectConfig) error {
	fmt.Println("MTS Startup")

	driver, err := minimts.NewDriver(m.protocol, systemConfig)
	if err != nil {
		return err
	}
	m.driver = driver
	m.systemConfig = systemConfig
	m.projectConfig = projectConfig
	fmt.Printf("MTS 设备协议: %s\n", m.protocol)
	return nil
}

// AttachOnlinePort 以联机模式回连 Diffract 主程序 127.0.0.1:<port>。
// port <= 0 时视为非联机模式，直接返回 nil。
func (m *MINIMTSService) AttachOnlinePort(port int) error {
	if port <= 0 {
		fmt.Println("非联机模式")
		return nil
	}

	fmt.Println("联机模式")
	m.onlineMu.Lock()
	if m.onlineConnected {
		m.onlineMu.Unlock()
		return nil
	}
	m.onlinePort = port
	conn, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%d", port), 5*time.Second)
	if err != nil {
		m.onlineMu.Unlock()
		return fmt.Errorf("连接 Diffract 主程序 127.0.0.1:%d 失败: %w", port, err)
	}
	m.onlineConn = conn
	m.onlineConnected = true
	m.diffractPending = make(map[string]chan json.RawMessage)
	m.onlineMu.Unlock()
	go m.onlineRecvLoop(conn)
	fmt.Printf("联机模式: 已连接 Diffract 主程序 127.0.0.1:%d\n", port)
	return nil
}

// IsOnline 返回当前是否处于联机模式且 TCP 连接有效。
func (m *MINIMTSService) IsOnline() bool {
	m.onlineMu.Lock()
	defer m.onlineMu.Unlock()
	return m.onlineConnected && m.onlineConn != nil
}

// SendOnline 向 Diffract 主程序发送一行 JSON。
func (m *MINIMTSService) SendOnline(obj interface{}) error {
	m.onlineMu.Lock()
	conn := m.onlineConn
	m.onlineMu.Unlock()
	if conn == nil {
		return fmt.Errorf("未联机")
	}
	data, err := json.Marshal(obj)
	if err != nil {
		return err
	}
	data = append(data, '\n')
	_, err = conn.Write(data)
	return err
}

func (m *MINIMTSService) onlineRecvLoop(conn net.Conn) {
	scanner := bufio.NewScanner(conn)
	scanner.Buffer(make([]byte, 0, 4096), 4*1024*1024)
	for scanner.Scan() {
		line := scanner.Bytes()
		var msg map[string]json.RawMessage
		if err := json.Unmarshal(line, &msg); err != nil {
			fmt.Println("[online] 无效 JSON:", string(line))
			continue
		}
		m.handleOnlineMessage(msg)
	}
	if err := scanner.Err(); err != nil {
		fmt.Println("[online] 读连接错误:", err)
	}
	m.closeOnlineConn()
}

func (m *MINIMTSService) handleOnlineMessage(msg map[string]json.RawMessage) {
	actionRaw := msg["action"]
	var action string
	_ = json.Unmarshal(actionRaw, &action)

	switch action {
	case "capture_done":
		var savePath string
		_ = json.Unmarshal(msg["save_path"], &savePath)
		if savePath == "" {
			_ = json.Unmarshal(msg["savePath"], &savePath)
		}
		if savePath == "" {
			return
		}
		m.diffractRespMu.Lock()
		ch, ok := m.diffractPending[savePath]
		if ok {
			delete(m.diffractPending, savePath)
		}
		m.diffractRespMu.Unlock()
		if ok {
			select {
			case ch <- msg["success"]:
			default:
			}
		}
	default:
		// 透传到前端显示（其他任意消息原样转发）
		app := application.Get()
		if app != nil {
			app.Event.Emit("mts_online", msg)
		}
	}
}

func (m *MINIMTSService) closeOnlineConn() {
	m.onlineMu.Lock()
	if m.onlineConn != nil {
		_ = m.onlineConn.Close()
		m.onlineConn = nil
	}
	m.onlineConnected = false
	for _, ch := range m.diffractPending {
		select {
		case ch <- nil:
		default:
		}
	}
	m.diffractPending = nil
	m.onlineMu.Unlock()
}

// startDiffractLoop 联机模式下按配置周期向 Diffract 主程序发送 capture 信号。
// 只有 IsOnline() 且 DiffractPeriodMs>0 且 DiffractSaveFolder 非空时才运行。
func (m *MINIMTSService) startDiffractLoop() error {
	if m.projectConfig == nil {
		return fmt.Errorf("项目配置未初始化")
	}
	if !m.IsOnline() {
		return nil // 非联机模式下静默跳过
	}
	if m.projectConfig.DiffractPeriodMs <= 0 {
		return fmt.Errorf("衍射仪触发周期必须大于 0 ms")
	}
	if m.projectConfig.DiffractSaveFolder == "" {
		return fmt.Errorf("衍射仪保存目录不能为空")
	}

	m.stopDiffractLoop()

	stopCh := make(chan struct{})
	m.onlineMu.Lock()
	m.diffractStopCh = stopCh
	m.diffractNextNum = 0
	m.onlineMu.Unlock()

	period := time.Duration(m.projectConfig.DiffractPeriodMs) * time.Millisecond
	folder := m.projectConfig.DiffractSaveFolder

	go func() {
		defer func() {
			m.onlineMu.Lock()
			if m.diffractStopCh == stopCh {
				m.diffractStopCh = nil
			}
			m.onlineMu.Unlock()
		}()

		// 启动先发一次
		if err := m.diffractSendOne(folder); err != nil {
			fmt.Println("[diffract] 初始发送 capture 失败:", err)
		}

		ticker := time.NewTicker(period)
		defer ticker.Stop()

		for {
			select {
			case <-stopCh:
				return
			case <-ticker.C:
				if !m.IsOnline() {
					fmt.Println("[diffract] TCP 连接已断开，停止衍射触发")
					return
				}
				if err := m.diffractSendOne(folder); err != nil {
					fmt.Println("[diffract] capture 失败:", err)
				}
			}
		}
	}()
	return nil
}

func (m *MINIMTSService) stopDiffractLoop() {
	m.onlineMu.Lock()
	stopCh := m.diffractStopCh
	m.diffractStopCh = nil
	m.onlineMu.Unlock()
	if stopCh != nil {
		close(stopCh)
	}
}

func (m *MINIMTSService) diffractSendOne(folder string) error {
	m.onlineMu.Lock()
	m.diffractNextNum++
	seq := m.diffractNextNum
	m.onlineMu.Unlock()

	savePath := filepath.Join(folder, fmt.Sprintf("image_%06d.tiff", seq))

	doneCh := make(chan json.RawMessage, 1)
	m.diffractRespMu.Lock()
	if m.diffractPending == nil {
		m.diffractPending = make(map[string]chan json.RawMessage)
	}
	m.diffractPending[savePath] = doneCh
	m.diffractRespMu.Unlock()

	if err := m.SendOnline(map[string]interface{}{
		"action":    "capture",
		"save_path": savePath,
	}); err != nil {
		m.diffractRespMu.Lock()
		delete(m.diffractPending, savePath)
		m.diffractRespMu.Unlock()
		return err
	}

	// 串行握手：必须等对应 capture_done 后才能发下一个
	select {
	case <-doneCh:
	case <-time.After(30 * time.Second):
		m.diffractRespMu.Lock()
		delete(m.diffractPending, savePath)
		m.diffractRespMu.Unlock()
		return fmt.Errorf("等待 capture_done 超时: %s", savePath)
	}
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

// videoWindowSize 滑动窗口平均滤波器的窗口大小。
const videoWindowSize = 5

// videoFilterAlpha 视频位移/应变一阶低通滤波系数（0~1，越小越平滑但滞后越大）。
// 取 0.4 时噪声标准差约衰减为原来的 1/2.5，时间常数约为 2~3 帧。
// 一阶低通后再叠加窗口为 videoWindowSize 的滑动窗口平均，进一步抑制随机噪声。
const videoFilterAlpha = 0.4

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

	// 一阶低通输出送入滑动窗口环形缓冲，取窗口均值进一步平滑
	idx := m.videoWindowIdx
	m.eposWindow[idx] = m.eposFiltered
	m.estrainWindow[idx] = m.estrainFiltered
	m.videoWindowIdx = (idx + 1) % videoWindowSize
	if m.videoWindowCount < videoWindowSize {
		m.videoWindowCount++
	}
	var eposSum, estrainSum float32
	for i := 0; i < m.videoWindowCount; i++ {
		eposSum += m.eposWindow[i]
		estrainSum += m.estrainWindow[i]
	}
	eposAvg := eposSum / float32(m.videoWindowCount)
	estrainAvg := estrainSum / float32(m.videoWindowCount)

	m.heartbeat.EPos = eposAvg - m.heartbeat.EZeroPos
	m.heartbeat.EStrain = estrainAvg - m.heartbeat.EZeroStrain
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
	m.videoFilterReady = false  // 一阶低通状态随基准一同重置
	m.eposWindow = [videoWindowSize]float32{}
	m.estrainWindow = [videoWindowSize]float32{}
	m.videoWindowIdx = 0
	m.videoWindowCount = 0
	m.heartbeatMu.Unlock()
}

func (m *MINIMTSService) OpenMINIMTS(target string) error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	if m.driver.IsOpen() {
		return fmt.Errorf("设备已打开")
	}
	if err := m.driver.Open(target); err != nil {
		return fmt.Errorf("无法打开设备 %s: %v", target, err)
	}
	m.heartbeat.StartTime = time.Now()
	go m.go_heartbeat()

	fmt.Println("连接成功，心跳协程已启动")
	return nil
}

func (m *MINIMTSService) CloseMINIMTS() error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	return m.driver.Close()
}

func (m *MINIMTSService) IsOpened() bool {
	return m.driver.IsOpen()
}

func (m *MINIMTSService) go_heartbeat() {
	scanTicker := time.NewTicker(50 * time.Millisecond)
	auxTicker := time.NewTicker(200 * time.Millisecond)
	cacheTicker := time.NewTicker(60 * time.Second)

	defer scanTicker.Stop()
	defer auxTicker.Stop()
	defer cacheTicker.Stop()

	for {
		select {
		case <-scanTicker.C:
			if err := m.go_scan(); err != nil {
				return
			}
			m.checkAutoStop()
			m.heartbeatMu.Lock()
			m.heartbeat.Duration = float32(time.Since(m.heartbeat.StartTime).Seconds())
			heartbeat := m.heartbeat
			m.heartbeatMu.Unlock()

			application.Get().Event.Emit("mts-heartbeat", heartbeat)
		case <-auxTicker.C:
			if m.systemConfig != nil {
				if m.systemConfig.Limit_Enabled {
					_, _ = m.updateLimitStatus()
				}
				if m.systemConfig.Temp_Enabled {
					if temperature, err := m.updateTempData(); err == nil {
						m.heartbeatMu.Lock()
						m.heartbeat.Temperature = temperature
						m.heartbeatMu.Unlock()
					}
				}
			}
		case <-cacheTicker.C:
			// 缓存数据自动保存（仅在测量过程中）
			m.heartbeatMu.RLock()
			active := m.cacheActive
			m.heartbeatMu.RUnlock()
			if active {
				go m.go_save_cache() //每分钟存储缓存数据
			}
		}
	}
}

func (m *MINIMTSService) checkAutoStop() {
	m.heartbeatMu.Lock()
	if !m.cacheActive || m.autoStopTriggered || m.projectConfig == nil {
		m.heartbeatMu.Unlock()
		return
	}
	condition, target := m.projectConfig.StopCondition, m.projectConfig.StopValue
	var value float64
	var label, unit string
	switch condition {
	case "stress-stop":
		value, label, unit = float64(m.heartbeat.Stress), "应力", "MPa"
	case "strian-stop":
		strain := m.heartbeat.EStrain
		label = "视频应变"
		if strain == 0 {
			strain = m.heartbeat.Strain
			label = "位移应变"
		}
		value, unit = float64(strain)*100, "%"
	default:
		m.heartbeatMu.Unlock()
		return
	}
	if !(target > 0) || !(math.Abs(value) >= float64(target)) {
		m.heartbeatMu.Unlock()
		return
	}
	m.autoStopTriggered = true
	m.heartbeatMu.Unlock()

	// 防止载荷维持循环在停机后再次下发运动指令。
	m.stopLoadControl()
	err := m.MotorStop()
	message := fmt.Sprintf("%s已达到停止阈值 %g %s，电机已停止", label, target, unit)
	if err != nil {
		message = fmt.Sprintf("%s已达到停止阈值，但电机停止失败：%v", label, err)
	}
	application.Get().Event.Emit("mts-auto-stop", map[string]interface{}{
		"message": message,
		"success": err == nil,
	})
}

func (m *MINIMTSService) go_scan() error {
	m.heartbeatMu.Lock()
	defer m.heartbeatMu.Unlock()

	if !m.IsOpened() {
		return fmt.Errorf("设备未连接")
	}

	// 更新电机位置
	if pos, err := m.driver.ReadPosition(); err == nil {
		pos -= m.heartbeat.ZeroDisp
		m.heartbeat.Disp = pos
		m.heartbeat.Strain = pos / m.projectConfig.SectionLength
	}

	// 更新载荷数据
	if load, err := m.driver.ReadLoad(); err == nil {
		load -= m.heartbeat.ZeroLoad
		m.heartbeat.Load = load
		m.heartbeat.Stress = load / m.projectConfig.SectionArea
	}

	// 缓存保存数据到列表（仅在测量过程中缓存）
	if m.cacheActive {
		m.dataList = append(m.dataList, m.heartbeat)
	}
	return nil
}

func (m *MINIMTSService) CallExit() {
	fmt.Println("MTS CallExit")

	m.stopDiffractLoop()
	m.closeOnlineConn()

	if err := m.CloseMINIMTS(); err != nil {
		fmt.Println("关闭MINIMTS失败:", err)
	}
}

func (m *MINIMTSService) GetMINIMTSDevices() ([]string, error) {
	fmt.Println("GetMINIMTSDevices")
	return m.driver.Devices()
}

func (m *MINIMTSService) IsMINIMTSConnected() bool {
	return m.driver.IsOpen()
}

// go_save_cache 自动缓存数据到本地文件，防止未手动保存导致数据丢失。
func (m *MINIMTSService) go_save_cache() {
	if err := m.saveCacheFile(); err != nil {
		fmt.Println("保存缓存数据失败:", err)
	}
}

// cacheFileName 自动缓存文件名：项目文件名追加 "_cache"。
func (m *MINIMTSService) cacheFileName() string {
	return strings.TrimSuffix(m.projectConfig.FileName, filepath.Ext(m.projectConfig.FileName)) + "_cache.csv"
}

// saveCacheFile 将内存中的心跳数据写入自动缓存文件（m.projectConfig.FilePath + "_cache" 文件名）。
func (m *MINIMTSService) saveCacheFile() error {
	m.cacheSaveMu.Lock()
	defer m.cacheSaveMu.Unlock()

	if m.projectConfig == nil {
		return fmt.Errorf("项目配置未初始化")
	}
	path := m.projectConfig.FilePath
	if path == "" || m.projectConfig.FileName == "" {
		return fmt.Errorf("缓存保存路径和文件名不能为空")
	}
	name := m.cacheFileName()

	m.heartbeatMu.RLock()
	data := append([]MINIMTSHeartbeat(nil), m.dataList...)
	m.heartbeatMu.RUnlock()

	if err := os.MkdirAll(path, 0755); err != nil {
		return err
	}
	file, err := os.CreateTemp(path, ".minimts-cache-*.csv")
	if err != nil {
		return err
	}
	defer os.Remove(file.Name())

	if err := m.writeHeartbeatCSV(file, data); err != nil {
		file.Close()
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	return os.Rename(file.Name(), filepath.Join(path, name))
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

	if err := m.writeHeartbeatCSV(file, data); err != nil {
		file.Close()
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	if err := os.Rename(file.Name(), filepath.Join(path, uniqueFileName(path, name))); err != nil {
		return err
	}
	// 手动保存成功后删除自动缓存文件，避免保留重复数据
	os.Remove(filepath.Join(path, m.cacheFileName()))
	return nil
}

// writeHeartbeatCSV 向已打开的文件写入：项目元数据注释行 + 过滤后的心跳 CSV。
// 相机未连接时会把 videoExtEnable / dicEnable 强制视为 false，并隐藏 ePos / eStrain 列。
func (m *MINIMTSService) writeHeartbeatCSV(file *os.File, data []MINIMTSHeartbeat) error {
	// ---- 相机连接判定 ----
	camConnected := m.extensometer != nil && m.extensometer.IsCameraConnected()
	videoEnabled := camConnected && m.projectConfig.VideoExtEnable
	dicEnabled := camConnected && m.projectConfig.DICEnable

	// ---- 列过滤 ----
	excluded := map[string]struct{}{
		"zeroLoad":    {},
		"zeroDisp":    {},
		"eZeroPos":    {},
		"eZeroStrain": {},
		"limitDIR":    {},
		"temperature": {},
		"startTime":   {},
	}
	if !videoEnabled {
		excluded["ePos"] = struct{}{}
		excluded["eStrain"] = struct{}{}
	}

	typ := reflect.TypeOf(MINIMTSHeartbeat{})
	type fieldIdx struct {
		idx int
		tag string
	}
	var keep []fieldIdx
	for i := 0; i < typ.NumField(); i++ {
		tag := typ.Field(i).Tag.Get("json")
		if _, skip := excluded[tag]; skip {
			continue
		}
		keep = append(keep, fieldIdx{i, tag})
	}

	// ---- 项目元数据注释行 ----
	pc := m.projectConfig
	meta := []struct{ k, v string }{
		{"experimenter", pc.Experimenter},
		{"sampleNo", pc.SampleNo},
		{"testDate", pc.TestDate},
		{"sampleShape", pc.SampleShape},
		{"testType", fmt.Sprint(pc.TestType)},
		{"width", formatNumber(pc.Width)},
		{"thickness", formatNumber(pc.Thickness)},
		{"diameter", formatNumber(pc.Diameter)},
		{"sectionArea", formatNumber(pc.SectionArea)},
		{"sectionLength", formatNumber(pc.SectionLength)},
		{"speed", formatNumber(pc.Speed)},
		{"pauseCondition", pc.PauseCondition},
		{"triggerEnabled", fmt.Sprint(pc.TriggerEnabled)},
		{"triggerPeriodMs", fmt.Sprint(pc.TriggerPeriodMs)},
		{"triggerPulseWidthMs", fmt.Sprint(pc.TriggerPulseWidthMs)},
		{"videoExtEnable", fmt.Sprint(videoEnabled)},
		{"dicEnable", fmt.Sprint(dicEnabled)},
	}
	for _, item := range meta {
		if _, err := fmt.Fprintf(file, "# %s: %s\n", item.k, item.v); err != nil {
			return err
		}
	}

	// ---- CSV 数据 ----
	writer := csv.NewWriter(file)
	header := make([]string, len(keep))
	for i, f := range keep {
		header[i] = f.tag
	}
	if err := writer.Write(header); err != nil {
		return err
	}
	for _, hb := range data {
		value := reflect.ValueOf(hb)
		row := make([]string, len(keep))
		for i, f := range keep {
			field := value.Field(f.idx)
			switch field.Kind() {
			case reflect.Float32:
				row[i] = strconv.FormatFloat(field.Float(), 'f', -1, 32)
			case reflect.Float64:
				row[i] = strconv.FormatFloat(field.Float(), 'f', -1, 64)
			case reflect.String:
				row[i] = field.String()
			default:
				row[i] = fmt.Sprint(field.Interface())
			}
		}
		if err := writer.Write(row); err != nil {
			return err
		}
	}
	writer.Flush()
	return writer.Error()
}

// formatNumber 去除浮点尾部多余零
func formatNumber(f float32) string {
	return strconv.FormatFloat(float64(f), 'f', -1, 32)
}

// uniqueFileName 在目录 dir 下为 baseName 寻找一个不冲突的文件名。
// 若 baseName 已存在，依次尝试 stem(1).ext、stem(2).ext … 直到空闲。
func uniqueFileName(dir, baseName string) string {
	target := filepath.Join(dir, baseName)
	if _, err := os.Stat(target); os.IsNotExist(err) {
		return baseName
	}

	ext := filepath.Ext(baseName)
	stem := strings.TrimSuffix(baseName, ext)
	for i := 1; i < 10000; i++ {
		candidate := fmt.Sprintf("%s(%d)%s", stem, i, ext)
		if _, err := os.Stat(filepath.Join(dir, candidate)); os.IsNotExist(err) {
			return candidate
		}
	}
	// 理论上不会走到
	return baseName
}

// ClearDataCache 清空内存中的心跳数据缓存，不删除已保存的文件。
func (m *MINIMTSService) ClearDataCache() {
	m.heartbeatMu.Lock()
	m.dataList = nil
	m.heartbeatMu.Unlock()

	m.CallDataToZero("time")
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
	return m.driver.MotorOn()
}

// MotorOff 电机禁止使能
func (m *MINIMTSService) MotorOff() error {
	return m.driver.MotorOff()
}

// MotorStop 电机停止
func (m *MINIMTSService) MotorStop() error {
	return m.driver.MotorStop()
}

// MotorJog 电机速度模式移动
func (m *MINIMTSService) MotorJog(speed float32) error {
	blocked := m.GetBlockedDirection()
	if blocked > 0 && speed > 0 {
		return fmt.Errorf("正向已被限位，禁止正向运动")
	}
	if blocked < 0 && speed < 0 {
		return fmt.Errorf("反向已被限位，禁止反向运动")
	}
	return m.driver.MotorJog(speed)
}

func (m *MINIMTSService) Trigger(pulseWidth time.Duration) error {
	return m.driver.Trigger(pulseWidth)
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

// emitDICCapture 通知相机服务开始/停止 DIC 图像保存；实际采集与落盘由相机服务负责
func (m *MINIMTSService) emitDICCapture(start bool) {
	app := application.Get()
	if app == nil {
		return
	}
	if !start {
		app.Event.Emit("dic_capture_stop", nil)
		return
	}
	app.Event.Emit("dic_capture_start", DICCaptureConfig{
		Folder:   m.projectConfig.DICFolder,
		FileName: m.projectConfig.DICFileName,
		PeriodMs: m.projectConfig.DICCapturePeriodMs,
	})
}

// StartMeasurement 开始测量逻辑
func (m *MINIMTSService) StartMeasurement() error {
	if m.projectConfig.StopCondition != "manual-stop" {
		value := float64(m.projectConfig.StopValue)
		if value <= 0 || math.IsNaN(value) || math.IsInf(value, 0) {
			return fmt.Errorf("自动停止阈值必须大于 0 且为有限数值")
		}
	}
	if m.projectConfig.TriggerEnabled {
		if m.projectConfig.TriggerPeriodMs <= 0 {
			return fmt.Errorf("触发周期必须大于 0 ms")
		}
		if m.projectConfig.TriggerPulseWidthMs <= 0 || m.projectConfig.TriggerPulseWidthMs > m.projectConfig.TriggerPeriodMs {
			return fmt.Errorf("触发脉宽必须大于 0 ms 且不能超过触发周期")
		}
	}

	if m.projectConfig.DICEnable {
		if m.projectConfig.DICCapturePeriodMs <= 0 {
			return fmt.Errorf("DIC 拍摄周期必须大于 0 ms")
		}
		if m.projectConfig.DICFolder == "" {
			return fmt.Errorf("DIC 图片保存路径不能为空")
		}
	}

	if err := m.startDiffractLoop(); err != nil {
		return err
	}

	if m.projectConfig.TriggerEnabled {
		m.startTriggerLoop(
			time.Duration(m.projectConfig.TriggerPeriodMs)*time.Millisecond,
			time.Duration(m.projectConfig.TriggerPulseWidthMs)*time.Millisecond,
		)
	}

	if m.projectConfig.DICEnable {
		m.emitDICCapture(true)
	}

	if err := m.MotorOn(); err != nil {
		return err
	}

	if err := m.MotorJog(m.projectConfig.Speed * float32(m.projectConfig.TestType)); err != nil {
		return err
	}

	// 测量开始后才启用数据缓存与定时自动保存
	m.heartbeatMu.Lock()
	m.cacheActive = true
	m.autoStopTriggered = false
	m.heartbeatMu.Unlock()

	return nil
}

// StopMeasurement 停止测量
func (m *MINIMTSService) StopMeasurement() error {
	// 停止测量后不再缓存数据
	m.heartbeatMu.Lock()
	m.cacheActive = false
	m.heartbeatMu.Unlock()

	m.stopLoadControl()
	err := m.MotorStop()
	m.stopTriggerLoop()
	m.stopDiffractLoop()
	m.emitDICCapture(false)
	return err
}

// PauseMeasurement 根据项目配置暂停测量。
func (m *MINIMTSService) PauseMeasurement() error {
	if m.projectConfig == nil {
		return fmt.Errorf("项目配置未初始化")
	}

	m.stopTriggerLoop()
	m.stopDiffractLoop()
	m.emitDICCapture(false)

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

	if err := m.startDiffractLoop(); err != nil {
		return err
	}

	if m.projectConfig.TriggerEnabled {
		m.startTriggerLoop(
			time.Duration(m.projectConfig.TriggerPeriodMs)*time.Millisecond,
			time.Duration(m.projectConfig.TriggerPulseWidthMs)*time.Millisecond,
		)
	}

	if m.projectConfig.DICEnable {
		m.emitDICCapture(true)
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
	m.stopDiffractLoop()
	m.emitDICCapture(false)
	if stopErr != nil {
		return stopErr
	}
	return m.MotorOff()
}

func (m *MINIMTSService) updateTempData() (float32, error) {
	return m.driver.ReadTemperature()
}

// updateLimitStatus 将协议原始限位状态转换为运动方向，触发时立即停机并通知前端。
func (m *MINIMTSService) updateLimitStatus() (int, error) {
	limit, err := m.driver.ReadLimit()
	if err != nil {
		return 0, err
	}

	blocked := 0
	if limit != 0 {
		if limit == m.systemConfig.Limit_Direction {
			blocked = 1
		} else {
			blocked = -1
		}
	}

	var changed bool
	m.blockedMu.Lock()
	if m.blockedDirection != blocked {
		m.blockedDirection = blocked
		changed = true
	}
	m.blockedMu.Unlock()

	if changed && blocked != 0 {
		go func() {
			fmt.Printf("限位触发：端口 %d，挡 %s 方向，立即停机\n", limit, map[int]string{1: "正向", -1: "反向"}[blocked])
			_ = m.MotorStop()
		}()
	}
	if changed {
		application.Get().Event.Emit("mts-limit-changed", map[string]any{
			"blockedDirection": blocked,
		})
	}
	return limit, nil
}

// GetBlockedDirection 返回当前被限位的方向（0=无，+1=正向挡，-1=反向挡）
func (m *MINIMTSService) GetBlockedDirection() int {
	m.blockedMu.RLock()
	defer m.blockedMu.RUnlock()
	return m.blockedDirection
}
