#!/usr/bin/env node
const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..", "..", "..", "..", "knowledge");
fs.mkdirSync(root, { recursive: true });
const cats = ["principles", "decisions", "methods", "pitfalls"];
const counts = {};
const all = [];
for (let i = 0; i < cats.length; i++) {
  const c = cats[i];
  const p = path.join(root, c);
  if (!fs.existsSync(p)) { counts[c] = 0; continue; }
  const items = fs.readdirSync(p).filter(function (f) { return f.endsWith(".md"); });
  counts[c] = items.length;
  for (let j = 0; j < items.length; j++) {
    const f = items[j];
    const full = path.join(p, f);
    all.push({ rel: c + "/" + f, mtime: fs.statSync(full).mtimeMs });
  }
}
all.sort(function (a, b) { return b.mtime - a.mtime; });
const recent = all.slice(0, 3).map(function (x) { return "- " + x.rel; });
const head = ["# 知识库索引", "", "## 分类计数",
  "- principles: " + counts.principles,
  "- decisions: " + counts.decisions,
  "- methods: " + counts.methods,
  "- pitfalls: " + counts.pitfalls,
  "", "## 最近条目",
  recent.length ? recent.join("\n") : "- (暂无)"
].join("\n");
fs.writeFileSync(path.join(root, "index.head.md"), head, "utf8");
const fullLines = ["# 完整索引", ""];
for (let i = 0; i < cats.length; i++) {
  const c = cats[i];
  const p = path.join(root, c);
  if (!fs.existsSync(p)) continue;
  const items = fs.readdirSync(p).filter(function (f) { return f.endsWith(".md"); }).map(function (f) { return f.replace(/\.md$/, ""); }).sort();
  fullLines.push("## " + c);
  items.forEach(function (i2) { fullLines.push("- " + i2); });
  fullLines.push("");
}
fs.writeFileSync(path.join(root, "index.full.md"), fullLines.join("\n"), "utf8");
console.log("[design-along] index.head.md / index.full.md rebuilt");