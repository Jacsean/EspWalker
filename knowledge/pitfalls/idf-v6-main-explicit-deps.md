---
type: pitfall
slug: idf-v6-main-explicit-deps
date: 2026-11-23
status: active
related: [idf-v6-cjson-managed-component]
---

**症状**：v6.1 编译 main 组件时，include esp_timer.h 等报 `esp_timer component(s) is not in the requirements list of "main"`。

**根因**：v6 不再让 main 组件自动 require 所有 IDF 内置组件；非公共组件（esp_timer / esp_wifi / esp_driver_gpio / nvs_flash 等）必须显式列出。公共头文件暴露了别的组件类型时，还要把 PRIV_REQUIRES 提升为 REQUIRES。

**修复**：main/CMakeLists.txt 里 `REQUIRES` 全部用到的组件；chassis.h 里 include motor.h 时，motor 要从 PRIV_REQUIRES 提到 REQUIRES。

**预防信号**：`... is not in the requirements list of "xxx"` / `... is in the private requirements list`。

**学习提示**：v6 的组件依赖检查更严格，公共头文件泄露的依赖必须 REQUIRES。
