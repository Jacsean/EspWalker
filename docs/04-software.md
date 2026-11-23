# 04 · 软件架构与构建

## 1. 仓库结构

```
Esp32Walker/
├─ README.md                     # 总览入口
├─ docs/                         # 设计文档
│  ├─ 01-architecture.md         # 总体架构
│  ├─ 02-hardware.md             # 硬件设计方案
│  ├─ 03-protocol.md             # 通信协议
│  └─ 04-software.md             # 本文档
├─ firmware/                     # ESP32-S3 固件（ESP-IDF v6.1）
│  ├─ CMakeLists.txt
│  ├─ sdkconfig.defaults         # 项目基线配置（引脚/分区/开关）
│  ├─ partitions.csv             # 分区表
│  ├─ main/                      # 主程序：app_main + 指令分发 + Wi-Fi
│  │  ├─ CMakeLists.txt
│  │  ├─ idf_component.yml
│  │  ├─ app_main.c
│  │  ├─ app.h
│  │  ├─ board.h                 # 引脚宏
│  │  ├─ cmd_dispatch.c / .h     # 协议指令分发与状态机
│  │  └─ wifi_sta.c / .h         # Wi-Fi 连接
│  └─ components/                # 自定义组件
│     ├─ motor/                  # 电机 PWM 驱动
│     ├─ chassis/                # 底盘运动学（两轮/履带/麦轮）
│     ├─ proximity/              # 障碍感知 + 本地避障闭环
│     ├─ camera/                 # 拍照（可选，需 esp32-camera）
│     ├─ audio/                  # 语音采集/播放（可选，含唤醒词预留）
│     └─ comm/                   # TCP 客户端 + 帧协议编解码
└─ host/                         # 中心端（PC 上位机，Python）
   ├─ main.py                    # TCP 服务端入口
   ├─ protocol.py                # 帧编解码
   ├─ llm.py / asr.py / tts.py   # 对话 / 识别 / 合成
   ├─ config.example.json        # 配置模板
   └─ requirements.txt
```

## 2. 固件架构（firmware/）

### 2.1 组件职责与依赖

| 组件 | 依赖（PRIV_REQUIRES） | 职责 |
|---|---|---|
| `motor` | `esp_driver_ledc` `esp_driver_gpio` | 单电机速度/方向控制（LEDC PWM） |
| `chassis` | `motor` | `(vx, vy, ω) → 各轮占空比`，三种底盘运动学 |
| `proximity` | `esp_driver_gpio`（TOF 时 `esp_driver_i2c`） | 多路测距 + 阈值判定 + 避障回调 |
| `camera` | `espressif__esp32-camera`（可选） | 抓帧 JPEG，回调上传 |
| `audio` | `esp_driver_i2s` | I2S 录音 / 放音；唤醒词预留 ESP-SR |
| `comm` | `esp_wifi` `esp_netif` `json`(cJSON) `lwip` | TCP 客户端、帧编解码、收发队列 |

### 2.2 主任务模型（FreeRTOS）

```
app_main
 ├─ nvs / 日志 / 外设初始化
 ├─ comm 启动：Wi-Fi STA 连网 → 连中心端 TCP
 ├─ 任务：comm_rx_task    解析下行指令 → 分发
 ├─ 任务：comm_tx_task    从发送队列取帧 → 写出
 ├─ 任务：sensor_task     周期采样距离 → 避障判定 → 上发 status/proximity
 └─ 任务：app 主循环      按钮/心跳/看门狗
（camera / audio 任务按需由指令拉起，见组件实现）
```

### 2.3 关键配置（sdkconfig.defaults）

- `CONFIG_IDF_TARGET="esp32s3"`
- 分区表：`partitions.csv`（factory 4 MB + nvs + coredump，预留 OTA 注释）
- Wi-Fi：SSID / 密码 / 中心端 IP / 端口
- 引脚：全部经 `board.h` 宏映射，`sdkconfig.defaults` 用 `CONFIG_APP_*` 键配置
- 开关：`CONFIG_APP_ENABLE_CAMERA`、`CONFIG_APP_ENABLE_AUDIO`、`CONFIG_APP_ENABLE_WAKE` 默认关闭，避免离线拉依赖

## 3. 固件构建 / 烧录

```powershell
# 1) 激活 ESP-IDF v6.1（本机路径）
. E:\esp\v6.1\esp-idf\export.ps1
idf.py --version          # 期望 ESP-IDF v6.1

# 2) 配置（改 Wi-Fi / 中心端 IP / 引脚）
cd firmware
idf.py set-target esp32s3
idf.py menuconfig         # 或直接编辑 sdkconfig.defaults 后重新构建

# 3) 构建
idf.py build

# 4) 烧录（COM 口按实物改）
idf.py -p COM3 -b 921600 flash monitor
```

> 首次启用摄像头 / 唤醒词需联网拉取组件：`idf.py add-dependency "espressif/esp32-camera"`、`"espressif/esp-sr"`，然后 `idf.py update-dependencies`。

## 4. 中心端（host/）

Python 3.10+，无强制第三方依赖即可跑通「运动 + 状态 + 拍照预览」基础链路；对话能力可选接入。

```powershell
cd host
pip install -r requirements.txt          # 基础为可选
copy config.example.json config.json     # 填 LLM / ASR / TTS 配置
python main.py --config config.json      # 监听 0.0.0.0:8888
```

| 文件 | 说明 |
|---|---|
| `protocol.py` | 帧编解码（JSON 行 + 二进制帧） |
| `main.py` | TCP 服务端：收帧分发、命令交互（键盘输入 `move/capture/...`） |
| `llm.py` | LLM 编排（OpenAI 兼容接口；可换通义/讯飞/Kimi） |
| `asr.py` / `tts.py` | 语音识别 / 合成（本地引擎或云端 API，接口可插拔） |

## 5. 扩展点

| 扩展 | 做法 |
|---|---|
| 多板（感知板/底盘板） | 从板经 UART/ESP-NOW 上报，主控 `comm` 侧新增帧类型，见 `02-hardware.md §10` |
| 更换云端模型 | 改 `host/llm.py` 适配层，固件无需改动 |
| OTA 升级 | `partitions.csv` 切双 OTA 表 + `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`（见 esp32-idf-dev 手册） |
| 落卡存储 | 摄像头 JPEG 旁路写 SD（`esp_vfs_fat_sdcard` 组件） |
