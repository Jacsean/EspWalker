#!/usr/bin/env node
const fs = require("fs");
const path = require("path");
const logPath = path.join(process.cwd(), "knowledge", "_log.md");
let raw = "";
process.stdin.on("data", function (c) { raw += c; });
process.stdin.on("end", function () {
  if (!fs.existsSync(logPath)) return;
  const today = new Date().toISOString().slice(0, 10);
  const todays = fs.readFileSync(logPath, "utf8").split("\n").filter(function (l) { return l.indexOf(today) === 0; });
  if (todays.length === 0) return;
  console.log("[design-along] 本轮已沉淀的条目：");
  todays.slice(-3).forEach(function (l) { console.log("  " + l.replace(/^\S+\s+\S+\s+/, "")); });
  console.log("如需补充其他知识点，回复「记一下」并说明分类。");
});