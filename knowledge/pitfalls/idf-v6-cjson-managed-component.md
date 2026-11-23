---
type: pitfall
slug: idf-v6-cjson-managed-component
date: 2026-11-23
status: active
related: [idf-v6-main-explicit-deps, idf-v6-espnow-not-a-component]
---

**症状**：ESP-IDF v6.1 构建报 `Failed to resolve component 'json' required by component 'main': unknown name`。

**根因**：cJSON 在 v6 已从 IDF 内置组件移除，改由组件管理器提供（`espressif/cjson`），CMake 组件名变为 `espressif__cjson`。

**修复**：`main/idf_component.yml` 加 `espressif/cjson: "^1.7.19"`；代码里用 `REQUIRES espressif__cjson`（main 组件也会自动 require 托管组件）。

**预防信号**：v6 构建报 `unknown name` + 找不到 json/cjson 组件。

**学习提示**：v6 很多原内置组件（cjson、usb、touch_element 等）都移到 registry，报 unknown name 先查组件管理器。
