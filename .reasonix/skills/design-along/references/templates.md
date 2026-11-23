# 条目模板

默认只填「核心字段」；用户说"记详细点"时追加「扩展字段」。

---

## decisions

~~~markdown
---
type: decision
slug: <slug>
date: <YYYY-MM-DD>
status: active
related: []
depends_on: []
---

**决策**：<一句话陈述选了什么>

**理由**：<1-2 句核心权衡>

**学习提示**：<1 句>
~~~

扩展字段：`## 考虑过的方案`（表格）、`## 背景与约束`。

---

## principles

~~~markdown
---
type: principle
slug: <slug>
date: <YYYY-MM-DD>
status: active
related: []
---

**原理**：<1-2 句机制解释>

**不变量**：<1-3 条，没有可留空>

**学习提示**：<1 句>
~~~

扩展字段：`## 边界条件`、`## 失效场景`。

---

## methods

~~~markdown
---
type: method
slug: <slug>
date: <YYYY-MM-DD>
status: active
related: []
---

**方法**：<一句话概括>

**步骤**：
1. <步骤>
2. <步骤>

**学习提示**：<1 句>
~~~

扩展字段：`## 检查清单`、`## 适用条件`。

---

## pitfalls

核心四段始终保留，只压缩叙述长度：

~~~markdown
---
type: pitfall
slug: <slug>
date: <YYYY-MM-DD>
status: active
related: []
---

**症状**：<观察到的现象>

**根因**：<为什么会发生>

**修复**：<怎么修的>

**预防信号**：<下次什么信号提示这个坑>

**学习提示**：<1 句>
~~~