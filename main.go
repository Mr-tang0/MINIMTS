package main

import (
	Config "MINIMTS/services/config"
	Devices "MINIMTS/services/devices"
	"embed"
	"log"
	"os"
	"strconv"

	"github.com/wailsapp/wails/v3/pkg/application"
)

//go:embed all:frontend/dist
var assets embed.FS

func init() {
	application.RegisterEvent[Devices.MINIMTSHeartbeat]("mts-heartbeat")
	application.RegisterEvent[Devices.PCService]("pc-heartbeat")
	application.RegisterEvent[Devices.ExtensometerResult]("hik_extensometer_result")
}

func main() {
	PC := Devices.NewPCService()
	PC.FreshPCInfo()

	systemConfig := Config.NewSystemConfig()
	projectConfig := Config.NewProjectConfig()
	if _, err := projectConfig.GetFromLocalPath(""); err != nil {
		log.Fatal(err)
	}
	if _, err := systemConfig.GetFromLocalPath(""); err != nil {
		log.Fatal(err)
	}
	hardwareConfig, err := Config.LoadHardwareConfig("")
	if err != nil {
		log.Fatal(err)
	}

	// 启动参数: 第一个位置参数为 Diffract 主程序监听端口；无参数即非联机模式。
	onlinePort := 0
	if len(os.Args) >= 2 {
		if p, err := strconv.Atoi(os.Args[1]); err == nil {
			onlinePort = p
		}
	}

	// 联机模式下默认使用 NIMTE 协议；非联机使用 hardware.json 配置（缺省 PIMS）。
	effectiveProtocol := hardwareConfig.Protocol
	if onlinePort > 0 {
		effectiveProtocol = "nimte"
	}

	camera := Devices.NewHIKCamera()
	calibration := Devices.NewHIKCalibration(camera)
	extensometer := Devices.NewHIKExtensometer(camera)

	// 从配置套用 DIC 预处理设置
	if err := extensometer.SetDICPreprocess(systemConfig.DIC_PreprocessMethod, systemConfig.DIC_SharpenAmount); err != nil {
		log.Fatal(err)
	}

	minimts := Devices.NewMINIMTSService(extensometer, effectiveProtocol)
	if err := minimts.Startup(systemConfig, projectConfig); err != nil {
		log.Fatal(err)
	}

	if err := minimts.AttachOnlinePort(onlinePort); err != nil {
		log.Fatal(err)
	}

	app := application.New(application.Options{
		Name:        "MINIMTS",
		Description: "MINIMTS PRO",
		Services: []application.Service{
			application.NewService(minimts),
			application.NewService(camera),
			application.NewService(calibration),
			application.NewService(extensometer),
			application.NewService(systemConfig),
			application.NewService(projectConfig),
		},
		Assets: application.AssetOptions{
			Handler: application.AssetFileServerFS(assets),
		},
		Mac: application.MacOptions{
			ApplicationShouldTerminateAfterLastWindowClosed: true,
		},
	})

	// MINIMTS 订阅引伸计结果，根据 ROI 距离计算视频位移与应变
	minimts.ListenExtensometer()

	// 相机订阅 DIC 图像保存的开始/停止事件（由 MINIMTS 测量控制发出）
	camera.ListenDICCapture()

	app.Window.NewWithOptions(application.WebviewWindowOptions{
		Title:     "MINIMTS PRO",
		Frameless: true,
		Width:     1200,
		Height:    900,
		Mac: application.MacWindow{
			InvisibleTitleBarHeight: 50,
			Backdrop:                application.MacBackdropTranslucent,
			TitleBar:                application.MacTitleBarHiddenInset,
		},
		BackgroundColour: application.NewRGB(27, 38, 54),
		URL:              "/",
	})

	// Run the application. This blocks until the application has been exited.
	// If an error occurred while running the application, log it and exit.
	if err := app.Run(); err != nil {
		log.Fatal(err)
	}

	PC.CallExit()
	minimts.CallExit()
	camera.CallExit()
}

//wails3 task common:update:build-assets
