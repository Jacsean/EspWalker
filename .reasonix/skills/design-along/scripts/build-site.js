#!/usr/bin/env node
// 组装三视图 tab 页面 + 独立页面 + Mermaid 源
const fs = require("fs");
const path = require("path");
const { buildIndexHtml } = require("./build-index-html");
const { buildRelatedMap } = require("./build-related-map");
const { buildRoadmap } = require("./build-roadmap");
const lib = require("./lib");

const root = path.resolve(__dirname, "..", "..", "..", "..", "knowledge");
const out = path.join(root, "_site");
fs.mkdirSync(out, { recursive: true });

const idx = buildIndexHtml(root);
const rel = buildRelatedMap(root);
const rmp = buildRoadmap(root);

const STYLE = [
  "body{font-family:system-ui,-apple-system,\"Segoe UI\",\"Microsoft YaHei\",sans-serif;margin:0;color:#222;background:#fafbfc}",
  "header{padding:20px 32px 12px;background:#fff;border-bottom:1px solid #e5e9ef}",
  "header h1{margin:0;font-size:20px;color:#1a3a5c}",
  "header .sub{margin:4px 0 0;color:#789;font-size:13px}",
  "nav.tabs{display:flex;gap:4px;padding:12px 32px 0;background:#fff;border-bottom:1px solid #e5e9ef}",
  "nav.tabs button{background:none;border:none;padding:8px 16px;font-size:14px;color:#556;cursor:pointer;border-bottom:2px solid transparent}",
  "nav.tabs button.active{color:#0b6bcb;border-bottom-color:#0b6bcb;font-weight:500}",
  "main{padding:20px 32px 48px}",
  ".tab-panel{display:none}",
  ".tab-panel.active{display:block}",
  "a{color:#0b6bcb;text-decoration:none}",
  "a:hover{text-decoration:underline}",
  ".cat-section{margin-bottom:28px;background:#fff;border:1px solid #e5e9ef;border-radius:8px;padding:16px 20px}",
  ".cat-section h2{margin:0 0 12px;font-size:16px;color:#334}",
  ".count{color:#789;font-weight:400;font-size:13px;margin-left:6px}",
  ".empty{color:#789;font-size:14px}",
  "table.cat-table{width:100%;border-collapse:collapse;font-size:14px}",
  "table.cat-table th{text-align:left;padding:8px;background:#f3f5f8;border-bottom:1px solid #e5e9ef;font-weight:500}",
  "table.cat-table td{padding:8px;border-bottom:1px solid #f0f2f5;vertical-align:top}",
  "code{background:#eef1f5;padding:1px 6px;border-radius:3px;font-size:12px;color:#456}",
  ".muted{color:#aab}",
  ".badge{display:inline-block;padding:1px 8px;border-radius:10px;font-size:12px}",
  ".badge-ok{background:#e6f6ec;color:#1a7f37}",
  ".badge-old{background:#eef1f5;color:#667}",
  ".badge-bad{background:#fdecec;color:#c0392b}",
  ".badge-warn{background:#fef6e0;color:#b7791f}",
  ".svg-wrap{overflow:auto;background:#fff;border:1px solid #e5e9ef;border-radius:8px;padding:12px}",
  ".legend{display:flex;gap:20px;margin-top:8px;font-size:13px;color:#556}",
  ".legend span{display:inline-flex;align-items:center;gap:6px}",
  ".hint{color:#667;font-size:13px;margin:0 0 12px}",
  ".roadmap{padding:8px 0}",
  ".rm-node{background:#fff;border:1px solid #e5e9ef;border-radius:6px;padding:10px 14px;margin-bottom:8px;border-left:4px solid #27ae60}",
  ".rm-abandoned{border-left-color:#c0392b;background:#fef8f8;opacity:0.9}",
  ".rm-superseded{border-left-color:#aab;background:#fafafa;opacity:0.85}",
  ".rm-revisited{border-left-color:#b7791f}",
  ".rm-head{display:flex;gap:10px;align-items:center;font-size:14px}",
  ".rm-date{color:#789;font-size:12px;min-width:90px}",
  ".rm-label{font-weight:500}",
  ".rm-reason,.rm-link{margin-top:4px;font-size:13px;color:#667}"
].join("\n");

const TAB_SCRIPT = [
  "<script>",
  "document.querySelectorAll(\"nav.tabs button\").forEach(function(b){",
  "  b.addEventListener(\"click\",function(){",
  "    document.querySelectorAll(\"nav.tabs button\").forEach(function(x){x.classList.remove(\"active\")});",
  "    document.querySelectorAll(\".tab-panel\").forEach(function(p){p.classList.remove(\"active\")});",
  "    b.classList.add(\"active\");",
  "    var p=document.getElementById(b.getAttribute(\"data-tab\"));if(p)p.classList.add(\"active\");",
  "  });",
  "});",
  "</script>"
].join("\n");

function wrapPage(title, sections, stats) {
  const navParts = ["<nav class=\"tabs\">"];
  const mainParts = ["<main>"];
  sections.forEach(function (s, i) {
    const active = i === 0 ? " active" : "";
    navParts.push("<button data-tab=\"tab-" + i + "\" class=\"" + active.trim() + "\">" + lib.esc(s.label) + "</button>");
    mainParts.push("<div id=\"tab-" + i + "\" class=\"tab-panel" + active + "\">" + s.bodyHtml + "</div>");
  });
  navParts.push("</nav>");
  mainParts.push("</main>");
  return [
    "<!DOCTYPE html>",
    "<html lang=\"zh-CN\">",
    "<head>",
    "<meta charset=\"utf-8\">",
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">",
    "<title>" + lib.esc(title) + "</title>",
    "<style>" + STYLE + "</style>",
    "</head>",
    "<body>",
    "<header><h1>Design-Along 知识库</h1><p class=\"sub\">" + stats.count + " 条知识点 · " + stats.edges + " 条关联 · " + stats.routes + " 个路线节点</p></header>",
    navParts.join(""),
    mainParts.join(""),
    TAB_SCRIPT,
    "</body></html>"
  ].join("\n");
}

function wrapStandalone(title, bodyHtml) {
  return "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\"><title>" + lib.esc(title) + "</title><style>" + STYLE + "</style></head><body><main>" + bodyHtml + "</main></body></html>";
}

const sections = [
  { label: "知识点索引 (" + idx.count + ")", bodyHtml: idx.bodyHtml },
  { label: "关联图 (" + rel.edges.length + ")", bodyHtml: rel.bodyHtml },
  { label: "技术路线 (" + rmp.count + ")", bodyHtml: rmp.bodyHtml }
];
const stats = { count: idx.count, edges: rel.edges.length, routes: rmp.count };

fs.writeFileSync(path.join(out, "index.html"), wrapPage("Design-Along 知识库", sections, stats), "utf8");
fs.writeFileSync(path.join(out, "index-all.html"), wrapStandalone("知识点索引", idx.bodyHtml), "utf8");
fs.writeFileSync(path.join(out, "related.html"), wrapStandalone("关联图", rel.bodyHtml), "utf8");
fs.writeFileSync(path.join(out, "roadmap.html"), wrapStandalone("技术路线", rmp.bodyHtml), "utf8");
console.log("[design-along] site: " + path.join(out, "index.html"));

function idSafe(id) { return id.replace(/[^A-Za-z0-9_]/g, "_"); }

if (rel.edges.length) {
  const mm = ["flowchart LR"];
  const seen = {};
  rel.edges.forEach(function (e) {
    if (!seen[e.from]) { mm.push("  " + idSafe(e.from) + "[\"" + e.from + "\"]"); seen[e.from] = true; }
    if (!seen[e.to]) { mm.push("  " + idSafe(e.to) + "[\"" + e.to + "\"]"); seen[e.to] = true; }
  });
  rel.edges.forEach(function (e) {
    const arrow = e.kind === "depends_on" ? "-.->" : "-->";
    mm.push("  " + idSafe(e.from) + " " + arrow + " " + idSafe(e.to));
  });
  fs.writeFileSync(path.join(root, "related.mmd"), mm.join("\n"), "utf8");
  console.log("[design-along] mermaid: " + path.join(root, "related.mmd"));
}

if (rmp.mermaid) {
  fs.writeFileSync(path.join(root, "roadmap.mmd"), rmp.mermaid, "utf8");
  console.log("[design-along] mermaid: " + path.join(root, "roadmap.mmd"));
}