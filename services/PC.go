package services

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
	CPU    float64   `json:"cpu"`    // CPU 使用率 (%)
	Memory float64   `json:"memory"` // 内存使用率 (%)
	HDD    float64   `json:"hdd"`    // 磁盘使用率 (%)
	TIME   time.Time `json:"time"`   // 采集时间

	// 使用 int32 配合 atomic 包实现无锁的并发安全控制
	freshing int32
}

func NewPCService() *PCService {
	return &PCService{
		CPU:      0,
		Memory:   0,
		HDD:      0,
		TIME:     time.Now(),
		freshing: 0,
	}
}

// FreshPCInfo 开启后台 goroutine 定时刷新系统信息
func (p *PCService) FreshPCInfo() {
	if !atomic.CompareAndSwapInt32(&p.freshing, 0, 1) {
		return
	}

	go func() {
		for atomic.LoadInt32(&p.freshing) == 1 {
			p.updateSystemInfo()
			time.Sleep(2 * time.Second) // 建议适当延长采集间隔，避免频繁调用系统 API 消耗资源
		}
	}()
}

func (p *PCService) CallExit() {
	fmt.Println("Stop PCService")
	atomic.StoreInt32(&p.freshing, 0)
}

// updateSystemInfo 核心采集逻辑
func (p *PCService) updateSystemInfo() {
	// 1. 获取 CPU 使用率 (过去1秒的平均值)
	// percent 返回的是一个切片，false 表示获取整体平均值
	cpuPercents, err := cpu.Percent(time.Second, false)
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

	p.TIME = time.Now()
	snapshot := *p
	application.Get().Event.Emit("pc-heartbeat", snapshot)
}
