package main

import (
	"MINIMTS/services"
	UI "MINIMTS/services/UI"
	"embed"
	"log"

	"github.com/wailsapp/wails/v3/pkg/application"
)

//go:embed all:frontend/dist
var assets embed.FS

func init() {
	application.RegisterEvent[services.MINIMTSHeartbeat]("mts-heartbeat")
	application.RegisterEvent[services.PCService]("pc-heartbeat")
}

func main() {
	pc := services.NewPCService()
	pc.FreshPCInfo()

	systemConfig := UI.NewSystemConfig()
	projectConfig := UI.NewProjectConfig()
	if _, err := projectConfig.GetFromLocalPath(""); err != nil {
		log.Fatal(err)
	}
	minimts := services.NewMINIMTSService()
	if err := minimts.Startup(systemConfig, projectConfig); err != nil {
		log.Fatal(err)
	}
	camera := services.NewHIKCamera()

	app := application.New(application.Options{
		Name:        "MINIMTS",
		Description: "MINIMTS PRO",
		Services: []application.Service{
			application.NewService(minimts),
			application.NewService(camera),
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

	pc.CallExit()
	minimts.CallExit()
	camera.CallExit()
}
