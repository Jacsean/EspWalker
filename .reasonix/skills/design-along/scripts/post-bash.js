#!/usr/bin/env node
let raw = "";
process.stdin.on("data", function (c) { raw += c; });
process.stdin.on("end", function () {
  let p = {};
  try { p = JSON.parse(raw); } catch (e) {}
  const cmd = (p.toolArgs && (p.toolArgs.command || p.toolArgs.cmd)) || (p.tool_input && p.tool_input.command) || "";
  const code = (p.toolResult && p.toolResult.exitCode);
  if (code !== 0 && code !== undefined) return;
  if (!/git\s+commit/.test(cmd)) return;
  console.log("[design-along] 本次 commit 成功。如果包含方案选型或非平凡修复，回复「记一下」即可沉淀。");
});