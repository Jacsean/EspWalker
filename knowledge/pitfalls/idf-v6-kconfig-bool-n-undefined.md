---
type: pitfall
slug: idf-v6-kconfig-bool-n-undefined
date: 2026-11-23
status: active
related: []
---

**症状**：C 代码里写 `CONFIG_APP_ENABLE_CAMERA ? ... : ...` 报 `undeclared`。

**根因**：Kconfig 的 bool 选项设为 n 时，宏不会被定义为 0，而是完全不定义。`#if` 预处理把它当 0 没问题，但放在 C 表达式里就是未声明。

**修复**：只在 `#if` / `#ifdef` 里使用这类宏；要在 C 表达式用时，先用 #if 把值赋给普通变量。

**预防信号**：`'CONFIG_XXX' undeclared (did you mean 'CONFIG_YYY'?)`。

**学习提示**：Kconfig bool=n 的宏是「不存在」而非「=0」，C 表达式用不得。
