# 07 · 通电调试作业指导书（单板方案）

> **本文档为单板方案（DevKitC-1 基准）的调试**；双板方案（ES3N28P + GOOUUU）的调试见 `09-dual-board-guide.md`。
> 适用：装配完成的 Esp32Walker（通过 `06-assembly.md` 自检）。固件默认「两轮差速 + 超声避障 + 摄像头/语音关闭」，上位机为 `host/main.py`。

## 1. 目的

按「上电前检查 → 分步上电 → 烧录 → 逐模块验证」的顺序完成整机调试，确认每个功能模块工作正常，并记录问题。

## 2. 上电前静态检查（通电前必做）

1. 对照 `06-assembly.md §7` 自检清单逐项确认。
2. 万用表测主控 3.3V–GND、5V–GND 电阻，均 **> 1 kΩ**（无短路）。
3. 确认电机驱动 STBY 已接 3.3V；INMP441（若接）L/R 已接地。
4. 确认没有接线插反（VCC/GND）。

## 3. 分步上电

| 步骤 | 操作 | 期望 |
|---|---|---|
| P1 | 只接 USB 逻辑电源（不接 VM） | 主控电源灯亮，无异常发热 |
| P2 | 观察主控板 | 无冒烟/异味，电流正常（USB 万用表约 <0.3 A 待机） |
| P3 | 接通电机电源 VM | 电机静止（STBY 已使能但无 PWM），无自转 |
| P4 | 断电，进行下一步烧录 | — |

> 若 P3 电机自转或异响，立即断电，检查 STBY / PWM 接线。

## 4. 固件烧录

```powershell
. E:\esp\v6.1\esp-idf\export.ps1
cd firmware
idf.py set-target esp32s3
idf.py menuconfig      # 改 Wi-Fi SSID/密码、中心端 IP、端口、底盘类型
idf.py build
idf.py -p COM3 -b 921600 flash monitor   # COM3 按实物改
```

- 串口枚举：`[System.IO.Ports.SerialPort]::GetPortNames()`；S3 原生 USB-JTAG 口无需按 BOOT。
- 烧录后串口应出现 `Esp32Walker boot (chassis=diff2)`、`wifi connected`、`ready`。

## 5. 启动上位机

```powershell
cd host
copy config.example.json config.json
python main.py --config config.json
```

期望：ESP32 连上后上位机打印 `[hello] fw=esp32walker/0.1 chassis=diff2 cap=[...]`，并周期打印 `[status] moving=... dist=...cm`。

## 6. 逐模块验证

### 6.1 状态灯
```
> led 1        → 状态 LED 亮；上位机回 [ack] code=0
> led 0        → LED 灭
```

### 6.2 运动（低→高速）
```
> move 0.3 0 0 500   → 前进 0.5 秒后自动停
> move -0.3 0 0 500  → 后退
> move 0 0 0.5 500   → 原地左转
> stop               → 立即刹车
```
⚠️ 先低速验证。**方向校正**：若 `move 0.3` 反而后退，交换该电机 M+/M−（或对调 AIN1/AIN2）后复测。

### 6.3 避障（本地闭环）
1. 悬空/架起底盘（轮子离地），`move 0.4 0 0` 持续前进。
2. 用手逐渐靠近超声探头，观察上位机 `[proximity] ... level=warn`（≤30 cm 减速）→ `level=stop`（≤15 cm 停车）。
3. 障碍移开后重新下发 `move` 恢复。
4. **IMU 姿态**：观察上位机 `[status]` 的 `roll/pitch/yaw` 字段；倾斜机身看 roll/pitch 变化；倾角 >60° 时机器人自动停车（防翻倒保护）。

### 6.4 拍照（需先启用摄像头）
启用摄像头（默认关闭）：
```powershell
# 1) main/idf_component.yml 取消注释 espressif/esp32-camera
# 2) sdkconfig.defaults：CONFIG_APP_ENABLE_CAMERA=y，并按 02 §9 重排电机引脚
idf.py update-dependencies
idf.py build
idf.py -p COM3 flash monitor
```
联调：
```
> capture        → 上位机 [ack] ok 后保存 out/img_<时间戳>.jpg
```
打开图片确认成像；若花屏/噪点，缩短 DVP 排线并远离电机线。

### 6.5 语音（需先启用音频）
启用音频：`sdkconfig.defaults` 设 `CONFIG_APP_ENABLE_AUDIO=y`，重编烧录。
- 录音上传：`> rec` 逻辑见协议（中心端发 `{"t":"rec","on":true}`），ESP32 持续上发 PCM。
- 播放：中心端发 `play` + PCM 二进制帧，喇叭应出声。

### 6.6 对话聊天（混合链路）
1. 在 `config.json` 启用 `llm.enabled` 并填 `api_key`（OpenAI 兼容端点）；按需配置 `asr.engine` / `tts.engine`。
2. 触发 `> talk 5000`（或 ESP32 本地唤醒词命中）。
3. 期望链路：`rec` 采集 → ASR → LLM → TTS → `play` 回传播放；上位机打印 `[voice] you said: ...`、`[bot] ...`。

## 7. 常见故障排查表

| 现象 | 可能原因 | 处置 |
|---|---|---|
| 串口无输出 / 乱码 | 波特率不符、晶振配置错 | monitor 用 `-b 115200` 对齐；检查 `CONFIG_ESP_CONSOLE_UART_BAUDRATE` |
| 烧录失败 `Timed out waiting for packet header` | 未进下载模式 / 串口被占 | S3 按住 BOOT 点 RST；关闭占用串口的程序；降 `-b 115200` |
| 复位循环 `Brownout detector was triggered` | 供电不足（Wi-Fi 发射掉压） | 换 5V/2A 电源、短粗线，主板加 100 µF 电容 |
| 连不上中心端 | IP/端口错、不同网段、Wi-Fi 未连 | 核对 `CONFIG_APP_SERVER_IP/PORT`；确认同一局域网 |
| 电机不转 | STBY 未使能、VM 无电、PWM 脚错 | 查 STBY=3.3V、VM=5V、接线对照 §5 |
| 电机转反 | M+/M− 或方向脚接反 | 交换 M+/M− 或对调 AIN1/AIN2 |
| 避障不触发 | 超声 Trig/Echo 接反、阈值过大 | 查 GPIO16/17；`config <stop_cm> <slow_cm>` 调整 |
| 拍照无图/花屏 | 摄像头未启用、排线长、引脚冲突 | 确认启用并重排引脚；排线 ≤15 cm 远离电机线 |
| 无声 | 功放 SD/GAIN、音频未启用 | 查 MAX98357A SD 引脚与 DIN；确认 `CONFIG_APP_ENABLE_AUDIO=y` |
| panic `Guru Meditation Error` | 空指针/栈溢出 | 用 `xtensa-esp32s3-elf-addr2line -pfiaC -e build/esp32walker.elf <地址>` 定位 |
| 姿态恒 0 / 无变化 | IMU 未接或地址错 | 查 I2C 接线（GPIO41/42）与 `APP_IMU_ADDR`（0x68），确认 `CONFIG_APP_ENABLE_IMU=y` |

## 8. 调试记录模板

| 项 | 记录 |
|---|---|
| 日期 / 人员 | |
| COM 口 / 烧录波特率 | |
| 固件版本 / 底盘类型 | |
| Wi-Fi / 中心端 IP:端口 | |
| 主控 / 摄像头 / 麦克风型号 | |
| LED | □ 通过 □ 异常： |
| 运动（前进/后退/转向/刹车） | □ 通过 □ 异常： |
| 避障（warn/stop） | □ 通过 □ 异常： |
| 拍照成像 | □ 通过 □ 异常： |
| 语音录放 / 对话 | □ 通过 □ 异常： |
| 遗留问题 | |
