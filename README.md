# MINIMTS

MINIMTS（MINIMTS PRO）是一款面向材料力学试验的桌面控制与数据采集软件，集成试验机控制、实时测量、工业相机采集和视频引伸计功能。

当前版本由原 Qt 项目使用 Go + Wails v3 + Vue 3 重构，主要面向 Windows 环境，后端负责设备通信、测量控制和图像处理，前端提供试验操作界面与实时曲线展示。

## 主要功能

- **试验机控制**：串口设备连接、电机使能、点动、停止、急停和方向限位处理。
- **试验流程**：拉伸与压缩试验参数配置，测量开始、暂停、恢复和停止，以及载荷维持 PID 控制。
- **实时测量**：采集并展示载荷、位移、温度及试验时间，结合试样参数计算应力和应变。
- **数据记录**：测量数据缓存、定时保存、CSV 导出及测量值归零。
- **工业相机**：海康相机接入、参数设置、实时预览与图像采集。
- **视频引伸计**：相机标定、ROI 跟踪、视频位移与应变计算，以及 DIC 图像预处理和定时保存。
- **参数管理**：试样信息、试验参数、设备参数和系统配置的本地保存与加载。

当前启用的试验机通信实现为 PIMS。NIMTE 实现文件作为开发内容保留，尚未启用。

## 技术栈

| 模块 | 技术 |
| --- | --- |
| 桌面框架 | Wails v3（当前使用 alpha 版本） |
| 后端 | Go |
| 前端 | Vue 3、Vue Router、Vite |
| 数据可视化 | ECharts |
| 设备通信 | PIMSGoMod、串口通信 |
| 图像处理 | GoCV / OpenCV |
| 工业相机 | 海康 MVS SDK（MvCameraControl） |

具体依赖版本以 `go.mod` 和 `frontend/package-lock.json` 为准。

## 开发环境

当前设备与相机实现依赖本地库，建议在 Windows 上开发和运行。构建前需要准备：

- Go：满足 `go.mod` 声明的版本要求（当前为 `1.26.4`）。
- Node.js 与 npm：使用与项目 Vite 版本兼容的 Node.js 版本。
- Wails v3 CLI：建议与项目依赖保持一致。
- CGO 所需的 C/C++ 编译工具链，例如 MinGW-w64。
- 与 GoCV `v0.43.0` 兼容的 OpenCV 开发库及运行库。
- 海康 MVS 相机 SDK、驱动及运行库。
- Windows WebView2 Runtime。

需要让编译器能够找到 OpenCV 头文件与链接库，并将 OpenCV、海康 SDK 所需的 DLL 所在目录加入 `PATH`，或按部署环境放置到可执行文件能够加载的位置。仓库中的海康头文件和链接库位于 `services/devices/hik/`，实际运行仍需配套驱动和运行库。

安装对应版本的 Wails CLI：

```powershell
go install github.com/wailsapp/wails/v3/cmd/wails3@v3.0.0-alpha2.103
```

## 本地开发

在项目根目录安装依赖：

```powershell
go mod download
npm --prefix frontend ci
```

确认本地库配置完成后，启用 CGO 并启动开发模式：

```powershell
$env:CGO_ENABLED = "1"
wails3 task dev
```

开发任务会启动前端开发服务并构建、运行桌面应用。

## 构建

在项目根目录执行：

```powershell
$env:CGO_ENABLED = "1"
wails3 build
```

Windows 默认可执行文件输出为 `bin/minimts.exe`。生成安装包可执行：

```powershell
wails3 task package
```

安装包任务还需具备所选打包方式对应的工具链。分发时需要同时处理 OpenCV、相机 SDK 等本地运行依赖。

## 使用流程

1. 启动应用，配置设备通信参数并连接试验机；需要图像测量时连接相机。
2. 核对电机方向、传感器系数和限位参数。
3. 填写试样尺寸、标距、试验方向、速度及数据保存位置。
4. 使用视频引伸计时，完成相机标定与 ROI 设置，并按需启用 DIC 图像保存。
5. 对测量值归零后开始试验，通过实时数值和曲线观察过程。
6. 结束测量并保存试验数据。

## 项目结构

```text
MINIMTS/
├── main.go                      # 应用入口、服务注册与事件连接
├── go.mod                       # Go 依赖与版本要求
├── Taskfile.yml                 # 开发、构建和打包任务入口
├── frontend/
│   ├── src/components/          # 试验、相机、标定及功能面板
│   ├── src/router/              # 页面路由
│   └── bindings/                # Wails 生成的前后端调用绑定
├── services/
│   ├── config/                  # 项目参数与系统配置
│   └── devices/                 # 试验机、相机及视频引伸计服务
│       └── hik/                 # 海康 SDK 封装、标定与图像处理
└── build/                       # Wails 配置、平台构建任务与打包资源
```

## 版本说明

- `main`：当前 Go / Wails 重构版本。
- `qt`：保留原 Qt 版本的代码与提交历史。
- `qt-final`：原 Qt 版本的最终快照标签。

需要查看旧版实现时，可在仓库中切换到 `qt` 分支或查看 `qt-final` 标签。
