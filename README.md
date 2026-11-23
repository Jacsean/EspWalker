# Esp32Walker

基于 **ESP32-S3** 的办公桌桌面机器人。ESP32-S3 作为「边缘采集与执行」节点，负责运动驱动、障碍感知、摄像头拍照、语音采集/播放；语音识别、对话、图像理解等 AI 处理下沉到手机 / PC / 云端（分布式架构，单板可完成全部边缘职责，可平滑扩展为多板）。

## 功能清单

| 需求 | 实现 |
|---|---|
| 两轮 / 履带 / 多轮运动底座 | 统一底盘接口 + 三种底座运动学（两轮差速 / 履带 / 麦克纳姆四轮） |
| 障碍物感知 | 超声（HC-SR04）/ TOF（VL53L0X）+ 本地实时避障闭环（减速 → 停车） |
| 摄像头拍照 | **OV2640 / OV3660**（DVP，自动探测），JPEG 经 Wi-Fi 上传 |
| 语音输入 | INMP441 I2S 麦克风 + 本地唤醒词（ESP-SR，可选） |
| 对话聊天 | 中心端 ASR → LLM → TTS，音频回传 ESP32 播放（混合架构，断网降级） |

## 目录结构

```
Esp32Walker/
├─ docs/                      # 设计文档（架构 / 硬件 / 协议 / 软件）
├─ firmware/                  # ESP32-S3 固件（ESP-IDF v6.1）
│  ├─ main/                   # 主程序 + 指令分发 + Wi-Fi + Kconfig/board.h
│  └─ components/             # motor / chassis / proximity / camera / audio / imu / espnow / comm
└─ host/                      # PC 上位机（Python 3.10+，标准库即可运行）
   ├─ main.py                 # TCP 服务端 + 指令交互 + 语音对话编排
   ├─ protocol.py             # 帧协议编解码
   └─ llm.py / asr.py / tts.py# 对话 / 识别 / 合成适配层（可插拔）
```

## 双板架构（可选）

单板即可跑通全部功能；引脚紧张或要接 OV3660 时用双板（详见 docs/08-dual-board.md）：

- **ES3N28P 主控板**（8MB PSRAM + 板载屏 + 音频）：Wi-Fi TCP 连 PC + ESP-NOW 主控，做交互头。
- **GOOUUU ESP32-S3 CAM 运动板**（24P DVP 摄像头座）：电机 + IMU + 避障 + OV3660，做运动/感知。

固件单工程通过 `CONFIG_APP_ROLE`（menuconfig → Board role）切换 master/motion 两个角色，各编一个 bin。

## 快速开始

### 1. 固件（ESP32-S3）

```powershell
. E:\esp\v6.1\esp-idf\export.ps1            # 激活 ESP-IDF v6.1（按本机路径）
cd firmware
idf.py set-target esp32s3
idf.py menuconfig                            # 改 Wi-Fi SSID/密码、中心端 IP/端口、底盘类型、引脚
idf.py build
idf.py -p COM3 -b 921600 flash monitor
```

默认配置（`sdkconfig.defaults`）：master 角色、两轮差速底盘、超声测距、摄像头/语音关闭。切运动板角色：menuconfig → Board role 选 Motion 后重编。
启用摄像头 / 语音：`main/idf_component.yml` 取消注释对应依赖 + `sdkconfig.defaults` 打开 `CONFIG_APP_ENABLE_CAMERA / CONFIG_APP_ENABLE_AUDIO`，再 `idf.py update-dependencies && idf.py build`。

### 2. 上位机（PC）

```powershell
cd host
copy config.example.json config.json        # 填 LLM / ASR / TTS 配置（可选）
python main.py --config config.json
```

交互命令：`move <vx> <vy> <w> [ms]`、`stop`、`capture`、`led <0|1>`、`query`、`config <stop_cm> <slow_cm>`、`talk [ms]`、`quit`。
基础链路（连接 / 运动 / 拍照 / 状态）仅需 Python 标准库；对话能力在 `config.json` 启用 `llm.enabled` 并配置 API key 后即可用。

## 文档

- [docs/01-architecture.md](docs/01-architecture.md) — 总体架构与数据流
- [docs/02-hardware.md](docs/02-hardware.md) — 硬件选型、接线、引脚表、BOM
- [docs/03-protocol.md](docs/03-protocol.md) — 通信协议（JSON + 二进制帧）
- [docs/04-software.md](docs/04-software.md) — 软件架构与构建
- [docs/05-bom.md](docs/05-bom.md) — 硬件清单（含可替代硬件）
- [docs/06-assembly.md](docs/06-assembly.md) — 装配作业指导书
- [docs/07-poweron.md](docs/07-poweron.md) — 通电调试作业指导书
- [docs/08-dual-board.md](docs/08-dual-board.md) — 双板方案（ES3N28P 主控 + GOOUUU 运动板）
- [docs/09-dual-board-guide.md](docs/09-dual-board-guide.md) — 双板作业指导（两板 BOM/装配/通电/调试）

## 通信协议

TCP（默认 `8888`），同一流混合「一行 JSON（以 `\n` 结尾）」与「二进制帧（`0xAA 0x57 | type | seq | len(4B 大端) | payload`）」。详见 `docs/03-protocol.md`。

## 已验证

- 固件 `idf.py build` 通过（ESP-IDF v6.1，target esp32s3；master 与 motion 双角色均编译通过）。
- 上位机语法检查 + 帧协议往返测试通过。
