#!/usr/bin/env node
// 生成知识点索引页：分类分组、条目链接到 md、状态 badge、摘要、关联
const path = require("path");
const fs = require("fs");
const lib = require("./lib");

const CAT_LABELS = {
  principles: "原理 · 它为什么这样工作",
  decisions: "决策 · 为什么选这条路径",
  methods: "方法 · 怎样做才正确",
  pitfalls: "踩坑 · 哪里容易错"
};

function badge(status) {
  const map = { active: "ok", superseded: "old", abandoned: "bad", revisited: "warn", draft: "warn" };
  return "<span class=\"badge badge-" + (map[status] || "ok") + "\">" + lib.esc(status) + "</span>";
}

function buildIndexHtml(knowledgeDir) {
  const entries = lib.collectEntries(knowledgeDir);
  const groups = {};
  lib.CATS.forEach(function (c) { groups[c] = []; });
  entries.forEach(function (e) { groups[e.cat].push(e); });

  const parts = [];
  for (let i = 0; i < lib.CATS.length; i++) {
    const cat = lib.CATS[i];
    const items = groups[cat];
    parts.push("<section class=\"cat-section\">");
    parts.push("<h2>" + lib.esc(CAT_LABELS[cat]) + " <span class=\"count\">" + items.length + " 条</span></h2>");
    if (items.length === 0) {
      parts.push("<p class=\"empty\">暂无条目。在会话里说「记一下」即可新增。</p>");
    } else {
      parts.push("<table class=\"cat-table\"><thead><tr><th>条目</th><th>状态</th><th>摘要</th><th>关联</th></tr></thead><tbody>");
      for (let j = 0; j < items.length; j++) {
        const item = items[j];
        const rels = item.related.map(function (r) { return "<code>" + lib.esc(r) + "</code>"; }).join(" ");
        parts.push("<tr>");
        parts.push("<td><a href=\"../" + lib.esc(item.rel) + "\">" + lib.esc(item.title) + "</a></td>");
        parts.push("<td>" + badge(item.status) + "</td>");
        parts.push("<td>" + lib.esc(item.summary) + "</td>");
        parts.push("<td>" + (rels || "<span class=\"muted\">—</span>") + "</td>");
        parts.push("</tr>");
      }
      parts.push("</tbody></table>");
    }
    parts.push("</section>");
  }

  return { title: "知识点索引", label: "知识点索引", bodyHtml: parts.join("\n"), count: entries.length };
}

module.exports = { buildIndexHtml: buildIndexHtml };

if (require.main === module) {
  const root = path.resolve(__dirname, "..", "..", "..", "..", "knowledge");
  const out = path.join(root, "_site", "index-all.html");
  fs.mkdirSync(path.dirname(out), { recursive: true });
  const r = buildIndexHtml(root);
  fs.writeFileSync(out, "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\"><title>" + r.title + "</title></head><body>" + r.bodyHtml + "</body></html>", "utf8");
  console.log("[design-along] index: " + out);
}