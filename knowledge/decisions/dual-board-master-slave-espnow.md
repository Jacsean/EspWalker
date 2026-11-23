---
type: decision
slug: dual-board-master-slave-espnow
date: 2026-11-23
status: active
related: [idf-v6-espnow-not-a-component, idf-v6-wifi-if-sta-removed]
depends_on: []
---

**决策**：桌面机器人用双板架构——ES3N28P（8MB PSRAM + 板载屏 + 音频）做交互主控，GOOUUU ESP32-S3 CAM（24P DVP 摄像头座）做运动/感知从机，两板 ESP-NOW 无线互联。

**理由**：单板 GPIO 不足以同时接电机 + IMU + OV3660（DVP 需 14+ 脚）+ 屏；两板各取所长，ESP-NOW 免接线、不占 GPIO、毫秒级延迟。

**学习提示**：外设密集时优先按「资源互补」拆板，而非硬塞单板。
