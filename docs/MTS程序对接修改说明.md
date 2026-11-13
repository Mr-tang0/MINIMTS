# MTS 程序对接修改说明

> 本文档描述 MTS 独立程序（MTS.exe）与 Diffract 主程序之间的通讯约定，供修改 MTS 程序时参照实现。

## 1. 总体架构

```
┌─ Diffract 主程序(Go/Wails) ─────┐         ┌─ MTS.exe(独立程序) ──┐
│ 用户点击"MTS 使能"按钮:          │  启动    │                      │
│  1. 监听 127.0.0.1 随机端口      │────────►│ 读取启动参数获取端口   │
│  2. 启动 MTS.exe, 端口作参数     │  回连    │ 作为TCP客户端连接     │
│  3. Accept 等待接入 ◄────────────┼─────────┘                      │
│  4. JSON 行帧 双向收发 ◄─────────┼────────► 换行分隔JSON          │
└─────────────────────────────────┘         └──────────────────────┘
```

- 主程序负责**拉起** MTS.exe 并把监听端口作为**第一个命令行参数**传入
- MTS.exe 必须**主动作为 TCP 客户端**回连主程序，不能自己监听端口
- 图像由主程序在本地保存为 TIFF 文件，**图像数据不经过 TCP 通道回传**，MTS 只收发控制/状态信号

## 2. 启动参数

主程序调用方式（Go）：

```go
exec.Command(mtsExePath, strconv.Itoa(port)) // argv[1] = 端口号字符串
```

MTS.exe 启动后需要：

1. 从 `os.Args[1]`（或 Python `sys.argv[1]`）读取端口号
2. 连接 `127.0.0.1:<端口>`
3. 连接失败时打印错误并退出（主程序会检测到进程退出并清理状态）

Go 示例：

```go
func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "用法: MTS.exe <port>")
		os.Exit(1)
	}
	conn, err := net.Dial("tcp", "127.0.0.1:"+os.Args[1])
	if err != nil {
		fmt.Fprintln(os.Stderr, "连接主程序失败:", err)
		os.Exit(1)
	}
	// ...进入消息循环
}
```

Python 示例：

```python
import sys, socket, json

port = int(sys.argv[1])
sock = socket.create_connection(("127.0.0.1", port))

def send(obj):
    sock.sendall((json.dumps(obj, ensure_ascii=False) + "\n").encode("utf-8"))
```

## 3. 帧格式

- 每条消息为**一行 JSON 文本**，以 `\n` 结尾，UTF-8 编码
- 单行长度不要超过 4MB（主程序读取上限）
- JSON 必须是单个对象（`{...}`），不要发数组或裸值

## 4. 采集握手协议（核心）

### 4.1 消息一览

| 方向 | action | 字段 | 说明 |
|---|---|---|---|
| MTS → 主程序 | `capture` | `save_path`(必填,字符串) | 采集信号，不定时发送 |
| 主程序 → MTS | `capture_start` | `save_path`,`success`,`error` | 已开始采集（已触发探测器） |
| 主程序 → MTS | `capture_done` | `save_path`,`success`,`error` | 采集完毕（图像帧已收到并保存，或失败/被拒绝） |
| MTS → 主程序 | 其他任意值或不带 action | 任意 | 状态/数据透传，主程序原样转发给前端界面显示 |
| 主程序 → MTS | 任意 | 任意 | 主程序主动下发的命令，MTS 应忽略不认识的消息 |

### 4.2 时序

```
MTS.exe                                主程序
   │ {"action":"capture",
   │  "save_path":"D:\\data\\a.tiff"} ──► 挂起路径, 触发探测器采集
   │
   │◄─────────────────────────────────── {"action":"capture_start",
   │                                      "save_path":"...","success":true,"error":""}
   │                                      （表示已开始采集，非图像就绪）
   │
   │                                     （探测器回传图像帧，主程序保存 TIFF 到 save_path）
   │
   │◄─────────────────────────────────── {"action":"capture_done",
   │                                      "save_path":"...","success":true,"error":""}
```

### 4.3 MTS 侧必须遵守的规则

1. **串行握手**：发出 `capture` 后必须等到对应 `capture_done`（`save_path` 匹配）才能发下一个采集信号
2. **busy 拒绝**：上一帧未完成时来新信号，会收到 `capture_done` 且 `success=false, error="busy"`，此时该次采集未执行，MTS 可稍后重发
3. **失败处理**：`capture_done` 中 `success=false` 时读取 `error` 字段（保存路径无效、目录不存在、保存失败等），按需重试或告警
4. **save_path 要求**：
   - 绝对路径，目标**目录必须已存在**
   - 主程序保存为 TIFF 格式，扩展名建议 `.tiff`
   - 文件名由 MTS 侧生成并保证唯一（如带序号/时间戳），主程序不会追加序号
5. **capture_start ≠ 图像就绪**：收到 `capture_start` 只代表探测器已触发采集，图像就绪以 `capture_done` 为准

### 4.4 主程序侧回执汇总（MTS 判断逻辑）

```python
reply = wait_for_line_json()          # 读一行, 解析JSON
if reply["action"] == "capture_start":
    ...                                # 采集已触发, 可继续等待
elif reply["action"] == "capture_done":
    if reply["success"]:
        ...                            # 本轮采集成功, 图像已在 reply["save_path"]
    elif reply["error"] == "busy":
        ...                            # 被拒绝, 稍后重发该 save_path
    else:
        ...                            # 采集或保存失败, 按 reply["error"] 处理
```

## 5. 生命周期与退出

- **使能/停止由主程序控制**：用户在主程序界面点击按钮启动/停止；停止时主程序会关闭连接并**强制终止 MTS.exe 进程**
- MTS 程序应具备随时被杀死而不会有半写文件等副作用的能力（文件写入放到主程序侧，MTS 侧无文件操作）
- MTS 检测到 TCP 连接断开时可直接退出（主程序无论如何会清理）
- MTS 崩溃或主动退出后，主程序会自动清理状态；重新启动必须由用户在主程序界面重新点击"使能"
- 主程序**只接受一个** TCP 连接（Accept 一次后即关闭监听），MTS 不要尝试建立第二条连接

## 6. 其他数据透传（可选）

MTS 若需要把测量数据/状态显示到主程序界面（如载荷、位移曲线），随时发送任意 JSON 行即可：

```json
{"force": 12.5, "position": 0.0012, "strain": 0.0003, "time": 1.25}
```

- 主程序解析成功后原样以 `mts_heartbeat` 事件转发给前端，前端可按需消费
- 注意：**不要**把此类消息的 `action` 字段设为 `"capture"`，否则会被当作采集信号处理

## 7. 修改验收清单

- [ ] 从 `argv[1]` 读取端口并回连 `127.0.0.1:<port>`，失败即退出
- [ ] 所有消息按"一行 JSON + \n"发送，UTF-8
- [ ] 采集走 `{"action":"capture","save_path":...}`，且等 `capture_done` 后才发下一个
- [ ] 正确处理 `capture_start` / `capture_done(success/error/busy)` 三种回执
- [ ] 对主程序下发的未知 JSON 消息做忽略处理，不崩溃
- [ ] 连接断开、进程被杀时能干净退出
- [ ] 不在 MTS 侧读写图像文件（图像由主程序保存）
