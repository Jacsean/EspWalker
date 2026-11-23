#!/usr/bin/env node
const KEYWORDS = ["记一下", "沉淀", "为什么选", "路线", "踩坑了", "这个原理"];
let raw = "";
process.stdin.on("data", function (c) { raw += c; });
process.stdin.on("end", function () {
  let prompt = "";
  try { const p = JSON.parse(raw); prompt = p.prompt || p.user_prompt || ""; } catch (e) {}
  const hit = KEYWORDS.find(function (k) { return prompt.indexOf(k) >= 0; });
  if (hit) console.log("[design-along] 检测到触发词「" + hit + "」。按 SKILL.md 的四类模板写入 knowledge/，写完运行 rebuild-index.js 和 build-site.js。");
});