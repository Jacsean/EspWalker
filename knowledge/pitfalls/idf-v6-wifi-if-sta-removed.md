---
type: pitfall
slug: idf-v6-wifi-if-sta-removed
date: 2026-11-23
status: active
related: [idf-v6-espnow-not-a-component]
---

**症状**：v6.1 编译报 `'ESP_IF_WIFI_STA' undeclared`。

**根因**：v6 删除了 `ESP_IF_WIFI_STA` / `ESP_IF_WIFI_AP` 宏。

**修复**：改用枚举值 `WIFI_IF_STA` / `WIFI_IF_AP`（如 esp_now_peer_info_t 的 ifidx 字段）。

**预防信号**：HINT 提示 "macros 'ESP_IF_WIFI_STA' have been removed, use WIFI_IF_STA/WIFI_IF_AP"。

**学习提示**：v6 的 Wi-Fi 接口枚举统一为 WIFI_IF_*，见 esp32-idf-dev 手册 v5→v6 表。
