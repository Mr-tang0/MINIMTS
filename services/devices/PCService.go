package devices

import (
	"fmt"
	"os"
	"runtime"
	"sync/atomic"
	"time"

	"github.com/shirou/gopsutil/v3/cpu"
	"github.com/shirou/gopsutil/v3/disk"
	"github.com/shirou/gopsutil/v3/mem"
	"github.com/wailsapp/wails/v3/pkg/application"
	// 如果需要使用 NVIDIA GPU 监控，可引入 gonvml: github.com/mindprince/gonvml
)

// PCService 存储系统实时状态
type PCService struct {
	CPU    float64 `json:"cpu"`    // CPU 使用率 (%)
	Memory float64 `json:"memory"` // 内存使用率 (%)
	HDD    float64 `json:"hdd"`    // 磁盘使用率 (%)
	TIME   string  `json:"time"`   // 采集时间

	// 使用 int32 配合 atomic 包实现无锁的并发安全控制
	freshing int32
}

func NewPCService() *PCService {
	return &PCService{
		CPU:      0,
		Memory:   0,
		HDD:      0,
		TIME:     time.Now().Format("15:04:05"),
		freshing: 0,
	}
}

// FreshPCInfo 开启后台 goroutine 定时刷新系统信息
// 系统时间每 1 秒更新一次，CPU/内存/磁盘每 5 秒更新一次
func (p *PCService) FreshPCInfo() {
	if !atomic.CompareAndSwapInt32(&p.freshing, 0, 1) {
		return
	}

	go func() {
		ticker := time.NewTicker(1 * time.Second)
		defer ticker.Stop()

		p.updateSystemInfo() // 启动时立即采集一次

		elapsed := 0
		for range ticker.C {
			if atomic.LoadInt32(&p.freshing) != 1 {
				return
			}
			elapsed++
			p.TIME = time.Now().Format("15:04:05")
			if elapsed >= 5 {
				p.updateSystemInfo()
				elapsed = 0
			}
			snapshot := *p
			application.Get().Event.Emit("pc-heartbeat", snapshot)
		}
	}()
}

func (p *PCService) CallExit() {
	fmt.Println("Stop PCService")
	atomic.StoreInt32(&p.freshing, 0)
}

// updateSystemInfo 核心采集逻辑 (CPU/内存/磁盘)
func (p *PCService) updateSystemInfo() {
	// 1. 获取 CPU 使用率
	// 传入 0 表示获取自上次调用以来的平均使用率，非阻塞，避免影响每秒的时间刷新
	cpuPercents, err := cpu.Percent(0, false)
	if err == nil && len(cpuPercents) > 0 {
		p.CPU = cpuPercents[0]
	}

	// 2. 获取内存使用率
	memInfo, err := mem.VirtualMemory()
	if err == nil {
		p.Memory = memInfo.UsedPercent
	}

	// 3. 获取系统盘使用率
	diskPath := "/"
	if runtime.GOOS == "windows" {
		diskPath = os.Getenv("SystemDrive") + `\`
	}
	diskInfo, err := disk.Usage(diskPath)
	if err == nil {
		p.HDD = diskInfo.UsedPercent
	}
}
