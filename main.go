package main

import (
	Config "MINIMTS/services/config"
	Devices "MINIMTS/services/devices"
	"embed"
	"log"

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

	camera := Devices.NewHIKCamera()
	calibration := Devices.NewHIKCalibration(camera)
	extensometer := Devices.NewHIKExtensometer(camera)

	// 从配置套用 DIC 预处理设置
	if err := extensometer.SetDICPreprocess(systemConfig.DIC_PreprocessMethod, systemConfig.DIC_SharpenAmount); err != nil {
		log.Fatal(err)
	}

	minimts := Devices.NewMINIMTSService(extensometer)
	if err := minimts.Startup(systemConfig, projectConfig); err != nil {
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
