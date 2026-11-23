---
name: design-along
description: 捕获设计原理/决策/方法/踩坑并写入 knowledge/ 分类目录。用户说"记一下""沉淀""为什么选""路线""踩坑了""这个原理"时触发；方案对比或错误修复完成后可提议。默认写入核心字段，用户说"记详细点"才展开扩展字段。
---

# design-along

把开发过程中的设计知识按四类沉淀到 `knowledge/`：principles / decisions / methods / pitfalls。

## 写入流程

1. **判定分类**。原理→principles；选型→decisions；步骤→methods；错误→pitfalls。多类拆多条，`related` 互链。
2. **去重**。`node scripts/dedupe.js <slug>`。EXISTS 时先读再决定；NEW 时继续。
3. **选模板**。读 `references/templates.md`。默认核心字段；用户说"记详细点"才展开。
4. **写文件**。`knowledge/<分类>/<slug>.md`，frontmatter 含 `type` `slug` `date` `status` `related`。
5. **追加日志**。`knowledge/_log.md` 追加 `<YYYY-MM-DD> <create|update> <相对路径>`。
6. **重建索引**。`node scripts/rebuild-index.js`。
7. **构建站点**。`node scripts/build-site.js`。会生成三个视图到 `knowledge/_site/`：知识点索引、关联图、技术路线。

## 可视化产物

- `knowledge/_site/index.html` — 三视图 tab 入口（知识点索引 / 关联图 / 技术路线）
- `knowledge/_site/index-all.html` — 独立的知识点索引
- `knowledge/_site/related.html` — 独立的关联图（SVG）
- `knowledge/_site/roadmap.html` — 独立的技术路线树
- `knowledge/related.mmd` — Mermaid 源，供专业工具渲染
- `knowledge/roadmap.mmd` — Mermaid 源，含废弃分支虚线

关联图从每条 md 的 frontmatter `related` 和 `depends_on` 生成。技术路线图从 `knowledge/roadmap.yaml` 生成（格式见 `references/roadmap-example.yaml`）。

## 读取约束

- 参考已有知识：先读 `knowledge/index.head.md` 定位，再读**单个**目标文件。
- 禁止 `ls` 后批量读取所有条目。
- 条目内 `related` ≥3 条时才生成 Mermaid 关联图；否则只用 `related` 列表。

## 触发由钩子负责

关键字、commit 后提议、会话收尾候选、SessionStart 注入均由 `.reasonix/settings.json` 的钩子完成。本技能只负责收到触发后的写入。