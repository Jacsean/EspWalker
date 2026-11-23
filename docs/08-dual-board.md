# 08 · 双板方案设计

> 两块 ESP32-S3 分工互补：**ES3N28P 做交互主控**（有 8MB PSRAM + 板载屏 + 音频），**GOOUUU ESP32-S3 CAM 做运动从机**（有 24P DVP 摄像头座 + GPIO 充裕）。
> 两板通过 **ESP-NOW** 无线互联（免接线、不占 GPIO、毫秒级延迟），对外只暴露 ES3N28P 一个 Wi-Fi 节点连 PC 中心端。

## 1. 架构

```
        PC 中心端 (host/main.py)
             ▲  Wi-Fi TCP + JSON（原协议，不变）
             │
      ┌──────┴───────────────────────┐
      │  ES3N28P 交互主控 (master)    │
      │  • 板载 2.8" LCD（表情/状态）  │
      │  • 板载 ES8311 音频（麦克/功放）│
      │  • Wi-Fi STA + TCP（连 PC）   │
      │  • 决策：语音→对话→指令下发     │
      └──────┬───────────────────────┘
             │  ESP-NOW（运动指令 ⇄ 状态回传）
      ┌──────▼───────────────────────┐
      │  GOOUUU 运动从机 (motion)      │
      │  • 电机驱动 + 底盘运动学        │
      │  • IMU 陀螺仪（姿态/航向）      │
      │  • 避障（超声/TOF）本地闭环     │
      │  • OV3660 摄像头（低分辨率拍照）│
      └──────────────────────────────┘
```

## 2. 分工与硬件事实

| | ES3N28P 主控 | GOOUUU CAM 从机 |
|---|---|---|
| 模组 | ESP32-S3 + **8MB PSRAM** + 16MB Flash | ESP32-S3-WROOM-1-**N8**（**无 PSRAM**） |
| 板载外设 | 2.8" LCD(ILI9341)、ES8311 音频、RGB LED、电池 ADC | 24P DVP 摄像头座、SD 卡(SDIO)、WS2812B、CH340C |
| 引出 GPIO | 仅 8 个（2/3/14/15/16/21/43/44） | 排针约 24 个 |
| 承担 | 显示、语音、对话、通信（交互头） | 电机、IMU、避障、摄像头 |

**摄像头 DVP 引脚**（GOOUUU 核心板，从原理图逐脚还原）：

| esp32-camera 字段 | GPIO | | esp32-camera 字段 | GPIO |
|---|---|---|---|---|
| pin_xclk | 15 | | pin_d3 | 10 |
| pin_pclk | 13 | | pin_d4 | 12 |
| pin_vsync | 6 | | pin_d5 | 18 |
| pin_href | 7 | | pin_d6 | 17 |
| pin_sccb_sda | 4 | | pin_d7 | 16 |
| pin_sccb_scl | 5 | | pin_d0 | 11 |
| pin_d1 | 9 | | pin_d2 | 8 |

> ⚠️ 核心板无 PSRAM，帧缓冲走内部 SRAM，拍照限 **QVGA(320×240)** 级别；高清需换 N8R2/N8R8 模组。

## 3. GOOUUU 从机引脚分配（建议，以核心板排针丝印为准）

核心板被占用：摄像头 DVP（4~13、15~18）、SD 卡 SDIO（35~40）、USB OTG（19/20）。剩余可用分配：

| 功能 | GPIO | 说明 |
|---|---|---|
| 电机 1 PWM / IN1 / IN2 | 1 / 2 / 21 | 左轮 |
| 电机 2 PWM / IN1 / IN2 | 41 / 42 / 45 | 右轮 |
| IMU I2C SDA / SCL | 47 / 48 | MPU6050/9250 |
| 超声 Trig / Echo | 3 / 14 | 若排针未引出 3/14 则改用 46 或省去 |

> 结论：**2 电机差速/履带底盘正好**（6 电机脚 + 2 IMU + 2 超声 = 10 脚）；4 电机麦轮需 12+ 脚，此板不够，需另加 I2C PWM 扩展（PCA9685）或改用其他板。

## 4. ESP-NOW 消息协议

ESP-NOW 单帧 ≤ 250 字节，用紧凑打包结构体（`__attribute__((packed))`）：

```c
typedef enum {
    NOW_CMD_MOVE    = 0x01, /* 主→从：运动 vx/vy/omega + 时长 */
    NOW_CMD_STOP    = 0x02, /* 主→从：停车 */
    NOW_CMD_CAPTURE = 0x03, /* 主→从：拍照 */
    NOW_CMD_CONFIG  = 0x04, /* 主→从：避障阈值 */
    NOW_STATUS      = 0x10, /* 从→主：周期状态（距离/姿态/运动） */
    NOW_PROXIMITY   = 0x11, /* 从→主：避障事件 */
    NOW_ACK         = 0x20, /* 应答 */
} now_msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    union {
        struct { float vx, vy, omega; uint16_t ms; } move;
        struct { int16_t stop_cm, slow_cm; } config;
        struct { int16_t dist_cm; uint8_t moving; float roll, pitch, yaw; } status;
        struct { uint8_t level; int16_t dist_cm; } proximity;
        struct { uint8_t code; } ack;
    };
} now_msg_t;
```

**自动配对流程**：
1. 从机（GOOUUU）启动后周期性向广播地址 `FF:FF:FF:FF:FF:FF` 发 `NOW_STATUS`（含自身 MAC 即发送源）。
2. 主控（ES3N28P）收到后 `esp_now_add_peer(从机MAC)`，回发一帧 `NOW_ACK`。
3. 从机收到 `NOW_ACK` 后 `esp_now_add_peer(主控MAC)`，配对完成，进入正常收发。

**摄像头图传**（第二阶段）：JPEG 超过 250 字节需分片，第一版先做「运动 + 避障 + IMU」指令链路，图传与屏驱动后续补。

## 5. 固件结构（单工程、Kconfig 角色切换）

```
firmware/
├─ main/                 # app_main.c 按 CONFIG_APP_ROLE 分支初始化
├─ components/
│  ├─ motor/ chassis/ proximity/ imu/ camera/   # 复用（运动侧）
│  ├─ comm/              # TCP（仅 master 启用）
│  └─ espnow/            # 两板通信（master + motion 共用）
```

- `CONFIG_APP_ROLE=motion` → 编译 GOOUUU 运动板 bin：电机 + IMU + 避障 + 摄像头 + espnow（从机）。
- `CONFIG_APP_ROLE=master` → 编译 ES3N28P 主控板 bin：espnow（主控）+ TCP + 指令中转（屏/音频驱动第二阶段补）。

一次 `menuconfig` 切角色，两次 `idf.py build` 出两个 bin。

## 6. 实施阶段

| 阶段 | 内容 |
|---|---|
| ①（本次） | 双板骨架 + ESP-NOW 配对 + 运动/避障/IMU 指令链路 |
| ② | ES3N28P 屏驱动（ILI9341）+ 板载音频（ES8311） |
| ③ | 摄像头拍照分片图传 |
