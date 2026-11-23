---
type: pitfall
slug: idf-v6-espnow-not-a-component
date: 2026-11-23
status: active
related: [idf-v6-wifi-if-sta-removed, idf-v6-main-explicit-deps]
---

**症状**：自定义组件 `PRIV_REQUIRES esp_now` 时报 `Failed to resolve component 'esp_now': unknown name`。

**根因**：ESP-NOW 不是独立组件，它是 `esp_wifi` 组件的一部分（esp_now.h 与 esp_now_* API 都在 esp_wifi 里）。

**修复**：依赖改为 `PRIV_REQUIRES esp_wifi esp_netif esp_event`。

**预防信号**：引用 esp_now.h 时想当然把它当独立组件名。

**学习提示**：ESP-NOW 归属 Wi-Fi 协议栈，依赖它只需 esp_wifi。
