# STM32 PC Monitor 上位机（初版）

面向当前 STM32F103C8T6 + FreeRTOS 固件的 Windows 上位机。程序实时显示 PC 指标，自动扫描 USB CDC 虚拟串口，通过 `$PING` 握手识别 MCU，并按可调周期发送状态帧。

## 功能

- CPU 占用率、CPU 温度、内存占用率实时显示
- GPU 占用率、GPU 温度、显存占用率实时显示
- 每 2 秒自动扫描串口，使用 `$PING\r\n` / `$ACK,PONG` 验证设备
- 发送周期可在 500–10000 ms 调节，默认 1000 ms
- 顶部状态进度条以 1% 为最小显示步长，1–9% 的低占用也可见
- 显示 MCU 的 ACK/ERR 应答与通信日志
- 可远程切换 MCU 的 CPU、GPU 页面
- 采集与串口通信均在后台线程执行，界面不会因 I/O 阻塞

## 运行环境

- Windows 10/11
- Python 3.10 或更新版本（已在 Python 3.13 下验证）
- 固件枚举出的 USB CDC / Virtual COM Port

首次运行，在此目录打开 PowerShell：

```powershell
python -m pip install -r requirements.txt
python main.py
```

也可以安装依赖后双击 `run.bat`。

## 单文件 EXE

已构建的程序位于：

```text
dist\STM32_PC_Monitor.exe
```

直接双击即可运行，不需要单独安装 Python 或复制 `tools` 目录。重新构建：

```powershell
python -m pip install pyinstaller
.\build_exe.ps1
```

## 设置保存与开机启动

- 自动扫描并连接、持续发送、发送周期、开机启动、上次端口和窗口位置会自动保存。
- 配置文件位于 `%LOCALAPPDATA%\STM32PCMonitor\settings.json`，重启电脑或更新 EXE 后仍然保留。
- 勾选“开机自动启动上位机”后，程序写入当前用户的 Windows 启动项，不需要管理员权限。
- 勾选“关闭窗口后在后台继续运行”后，关闭主窗口不会停止采集、USB 扫描和数据发送。
- 单击系统托盘图标可重新显示串口界面；右键托盘图标可选择显示界面或退出程序。
- “开机自动启动”和“后台运行”同时开启时，开机后只显示托盘图标，不弹出主窗口。
- 如果移动了 EXE，请从新位置手动运行一次，程序会自动更新开机启动路径。

## MCU 协议对应关系

上位机发送的状态帧与 `MCUcode/USB/usb_protocol.c` 完全对应：

```text
$PC,CPU=68,CT=52,RAM=54,GPU=72,GT=61,VRAM=45\r\n
```

| 字段 | 含义 | 发送范围 |
|---|---|---:|
| CPU | CPU 占用率 | 0–100 |
| CT | CPU 温度（°C） | -40–150 |
| RAM | 内存占用率 | 0–100 |
| GPU | GPU 占用率 | 0–100 |
| GT | GPU 温度（°C） | -40–150 |
| VRAM | 显存占用率 | 0–100 |

每帧均使用 ASCII 编码并以 `\r\n` 结束。程序还使用：

- `$PING\r\n`：设备识别，期望 `$ACK,PONG`
- `$PAGE,CPU\r\n`、`$PAGE,GPU\r\n`、`$PAGE,NEXT\r\n`：页面控制

## 指标采集说明

- CPU 和内存：由 `psutil` 获取。
- CPU 温度：Windows 下内置 LibreHardwareMonitor 轻量读取后端，并依次回退到
  LibreHardwareMonitor/OpenHardwareMonitor WMI、LibreHardwareMonitor Web 和 ACPI。
  如果界面提示缺少温度驱动，可点击“安装 CPU 温度驱动”，按向导安装官方签名的
  PawnIO 硬件访问层，随后重启电脑和本上位机。安装操作只在用户点击并确认后执行。
- NVIDIA GPU：优先使用 NVML（`nvidia-ml-py`），失败时回退到系统中的 `nvidia-smi`。
- 如果所有温度来源都不可用，界面会显示对应原因，协议中发送安全值 `CT=0`。
- 没有可用 GPU 接口时，GPU 三项在界面显示“不可用”，协议发送 0；这可以保证 MCU 的范围检查始终通过。
- 当前初版默认监测第 1 块 NVIDIA GPU。

## 测试

```powershell
python -m unittest discover -s tests -v
python -m compileall pc_monitor main.py
```

串口被其他软件（如串口助手）占用时，上位机无法打开该端口。测试前请关闭占用程序。
