#!/usr/bin/env node
const fs = require("fs");
const path = require("path");
const headPath = path.join(process.cwd(), "knowledge", "index.head.md");
let raw = "";
process.stdin.on("data", function (c) { raw += c; });
process.stdin.on("end", function () {
  if (!fs.existsSync(headPath)) return;
  const content = fs.readFileSync(headPath, "utf8").trim();
  if (content) {
    console.log("[design-along] 当前知识库摘要：");
    console.log(content);
  }
});