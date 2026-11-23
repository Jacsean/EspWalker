# 03 · 通信协议（ESP32 ⇄ 中心端）

传输层：**TCP**（默认端口 `8888`）。ESP32-S3 作为客户端，主动连接中心端（手机 / PC）。
中心端地址可在固件 `sdkconfig.defaults` 配置，或通过 mDNS / UDP 广播自动发现（见 `04-software.md`）。

## 1. 帧格式

同一 TCP 流上混合两类帧：

### 1.1 文本帧（JSON 控制 / 状态）

一行 UTF-8 JSON，以 `\n`（0x0A）结尾。用于命令与状态，体量小、易调试。

```json
{ "t": "move", "id": 1, "vx": 0.5, "omega": 0.0, "ms": 1000 }
```

### 1.2 二进制帧（图片 / 音频）

用于 JPEG、PCM 等大块数据。帧头固定 8 字节（大端序）：

| 偏移 | 长度 | 字段 | 说明 |
|---|---|---|---|
| 0 | 2 | magic | 固定 `0xAA 0x57` |
| 2 | 1 | type | 负载类型 |
| 3 | 1 | seq | 帧序号（0~255 回绕，用于丢帧检测） |
| 4 | 4 | len | 负载字节数（big-endian） |
| 8 | len | payload | 负载 |

`type` 定义：

| type | 含义 | 方向 |
|---|---|---|
| `0x01` | JPEG 图片 | ESP32 → 中心端 |
| `0x02` | PCM 音频（16 kHz / 16 bit / 单声道） | ESP32 → 中心端 |
| `0x03` | PCM 音频（下发播放，同上格式） | 中心端 → ESP32 |

## 2. JSON 消息通用字段

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `t` | string | 是 | 消息类型 |
| `id` | int | 是 | 消息序号，`ack` 用它关联请求 |
| 其余 | 按类型 | — | 各消息自定义 |

## 3. 上行消息（ESP32 → 中心端）

### hello — 连接握手
```json
{ "t":"hello", "id":1, "fw":"esp32walker/0.1", "board":"esp32s3-n16r8",
  "chassis":"diff2", "sensors":["tof","button"], "cap":["image","audio","wake"] }
```
- `chassis`：`diff2`（两轮差速）/ `track`（履带）/ `mecanum`（麦克纳姆四轮）。
- `cap`：能力位图，中心端据此决定是否下发拍照 / 录音 / 唤醒配置。

### ack — 指令应答
```json
{ "t":"ack", "id":5, "code":0, "msg":"ok" }
```
`code`：`0` 成功；非 0 为错误码（`1` 参数非法、`2` 忙、`3` 不支持、`4` 硬件故障）。

### status — 周期状态（默认 1 s 一次，可 `config` 调整）
```json
{ "t":"status", "id":2, "moving":false, "dist_cm":120, "batt_mv":4100, "temp_c":45 }
```

### proximity — 障碍距离事件（越过阈值时）
```json
{ "t":"proximity", "id":3, "zone":"front", "dist_cm":18, "level":"warn" }
```
`level`：`warn`（减速区）/ `stop`（停车区）。

### event — 通用事件（按钮等）
```json
{ "t":"event", "id":4, "evt":"button", "sub":"capture" }
```

### wake — 本地唤醒词命中
```json
{ "t":"wake", "id":6, "word":"hi lexin" }
```
收到 `wake` 后中心端可下发 `rec` 开始采集并做 ASR。

## 4. 下行消息（中心端 → ESP32）

### move — 运动指令
```json
{ "t":"move", "id":10, "vx":0.5, "vy":0.0, "omega":0.0, "ms":1000 }
```
- `vx` 前后、`vy` 左右、`omega` 旋转，范围 `[-1.0, 1.0]` 归一化速度；麦轮才使用 `vy`。
- `ms` 持续时间：`0` 表示持续运动直到收到 `stop`；`>0` 到时自动停车。

### stop — 立即停车
```json
{ "t":"stop", "id":11 }
```

### capture — 拍照
```json
{ "t":"capture", "id":12, "res":"800x600", "quality":12 }
```
ESP32 拍照完成后先回 `ack`，再发 JPEG 二进制帧（type 0x01）。

### rec — 开始 / 结束录音上传
```json
{ "t":"rec", "id":13, "on":true, "ms":6000 }
```
`on=true` 开始以二进制帧（type 0x02）持续上传 PCM；`ms` 到时自动停止；中心端判定说完话后可发 `on=false` 提前结束。

### play — 播放音频
```json
{ "t":"play", "id":14, "fmt":"pcm16", "rate":16000, "ch":1, "bytes":12345 }
```
随后紧跟一个二进制帧（type 0x03），`bytes` 为其负载长度。ESP32 播放完回 `ack`。

### led — 状态灯
```json
{ "t":"led", "id":15, "on":true }
```

### config — 运行参数
```json
{ "t":"config", "id":16, "prox_stop_cm":15, "prox_slow_cm":30, "status_ms":1000 }
```

### query — 查询状态
```json
{ "t":"query", "id":17 }
```
ESP32 立即回一帧 `status`。

## 5. 典型时序

### 拍照
```
中心端 --capture--> ESP32
ESP32  --ack(ok)--> 中心端
ESP32  --JPEG 二进制帧--> 中心端
```

### 语音对话
```
ESP32  --wake--> 中心端
中心端 --rec(on)--> ESP32
ESP32  --PCM 二进制帧 x N--> 中心端   （中心端做 ASR→LLM→TTS）
中心端 --rec(off)--> ESP32
中心端 --play + PCM 二进制帧--> ESP32
ESP32  --ack(播放完成)--> 中心端
```

### 避障（本地闭环，不经中心端）
```
障碍物进入减速区 → ESP32 本地降速，并上发 proximity(level=warn)
障碍物进入停车区 → ESP32 本地停车，并上发 proximity(level=stop)
```
