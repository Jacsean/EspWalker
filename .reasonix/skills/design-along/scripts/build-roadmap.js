#!/usr/bin/env node
// 从 roadmap.yaml 生成技术路线树 + Mermaid 源（含废弃分支）
const path = require("path");
const fs = require("fs");
const lib = require("./lib");

function stripQuote(s) {
  if (!s) return "";
  s = s.trim();
  if ((s.charAt(0) === "\"" && s.charAt(s.length - 1) === "\"") ||
      (s.charAt(0) === "\x27" && s.charAt(s.length - 1) === "\x27")) {
    return s.slice(1, -1);
  }
  return s;
}

function parseRoadmapYaml(text) {
  const items = [];
  let cur = null;
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].replace(/\s+$/, "");
    if (!line || line.trim().charAt(0) === "#") continue;
    const m = line.match(/^\s*-\s+([A-Za-z0-9_]+):\s*(.*)$/);
    if (m) {
      cur = {};
      cur[m[1]] = stripQuote(m[2]);
      items.push(cur);
    } else {
      const kv = line.match(/^\s+([A-Za-z0-9_]+):\s*(.*)$/);
      if (kv && cur) cur[kv[1]] = stripQuote(kv[2]);
    }
  }
  return items;
}

function statusMap(s) {
  return ({ active: "ok", superseded: "old", abandoned: "bad", revisited: "warn" })[s] || "ok";
}

function renderTree(id, depth, children, parts) {
  const list = children[id] || [];
  for (let i = 0; i < list.length; i++) {
    const it = list[i];
    const status = it.status || "active";
    parts.push("<div class=\"rm-node rm-" + lib.esc(status) + "\" style=\"margin-left:" + (depth * 28) + "px\">");
    parts.push("<div class=\"rm-head\">");
    if (it.date) parts.push("<span class=\"rm-date\">" + lib.esc(it.date) + "</span>");
    parts.push("<span class=\"rm-label\">" + lib.esc(it.label || it.id || "") + "</span>");
    parts.push("<span class=\"badge badge-" + statusMap(status) + "\">" + lib.esc(status) + "</span>");
    parts.push("</div>");
    if (it.reason) parts.push("<div class=\"rm-reason\">原因：" + lib.esc(it.reason) + "</div>");
    if (it.decision) parts.push("<div class=\"rm-link\">关联：<code>" + lib.esc(it.decision) + "</code></div>");
    parts.push("</div>");
    if (it.id) renderTree(it.id, depth + 1, children, parts);
  }
}

function buildRoadmap(knowledgeDir) {
  const yamlPath = path.join(knowledgeDir, "roadmap.yaml");
  if (!fs.existsSync(yamlPath)) {
    return { title: "技术路线", label: "技术路线", bodyHtml: "<p class=\"empty\">还没有 roadmap.yaml。参考 <code>references/roadmap-example.yaml</code> 在 knowledge/ 下创建。</p>", count: 0, mermaid: "" };
  }
  const items = parseRoadmapYaml(fs.readFileSync(yamlPath, "utf8"));
  if (items.length === 0) {
    return { title: "技术路线", label: "技术路线", bodyHtml: "<p class=\"empty\">roadmap.yaml 为空。</p>", count: 0, mermaid: "" };
  }

  const byId = {};
  items.forEach(function (it) { if (it.id) byId[it.id] = it; });
  const children = { __root__: [] };
  items.forEach(function (it) {
    const p = (it.parent && byId[it.parent]) ? it.parent : "__root__";
    if (!children[p]) children[p] = [];
    children[p].push(it);
  });

  const parts = [];
  parts.push("<p class=\"hint\">数据源：<code>knowledge/roadmap.yaml</code>。废弃分支标红虚线，superseded 标灰。</p>");
  parts.push("<div class=\"roadmap\">");
  renderTree("__root__", 0, children, parts);
  parts.push("</div>");

  const mm = ["flowchart TD"];
  items.forEach(function (it, i) {
    if (!it.id) return;
    const label = (it.label || it.id).replace(/"/g, "\x27");
    mm.push("  " + it.id + "[\"" + label + "\"]");
  });
  items.forEach(function (it) {
    if (!it.id || !it.parent || !byId[it.parent]) return;
    const arrow = it.status === "abandoned" ? "-.->|废弃|" : "-->";
    mm.push("  " + it.parent + " " + arrow + " " + it.id);
  });
  const ids = { active: [], abandoned: [], superseded: [] };
  items.forEach(function (it) {
    if (!it.id) return;
    const k = it.status || "active";
    if (ids[k]) ids[k].push(it.id);
  });
  if (ids.active.length) { mm.push("  classDef active fill:#e6f6ec,stroke:#1a7f37;"); mm.push("  class " + ids.active.join(",") + " active;"); }
  if (ids.abandoned.length) { mm.push("  classDef abandoned fill:#fdecec,stroke:#c0392b,stroke-dasharray:4 3;"); mm.push("  class " + ids.abandoned.join(",") + " abandoned;"); }
  if (ids.superseded.length) { mm.push("  classDef superseded fill:#eef1f5,stroke:#667;"); mm.push("  class " + ids.superseded.join(",") + " superseded;"); }

  return { title: "技术路线", label: "技术路线", bodyHtml: parts.join("\n"), count: items.length, mermaid: mm.join("\n") };
}

module.exports = { buildRoadmap: buildRoadmap };