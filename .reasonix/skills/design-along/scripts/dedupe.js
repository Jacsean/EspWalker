#!/usr/bin/env node
const fs = require("fs");
const path = require("path");
const slug = process.argv[2];
if (!slug) { console.error("usage: node dedupe.js <slug>"); process.exit(1); }
const root = path.resolve(__dirname, "..", "..", "..", "..", "knowledge");
if (!fs.existsSync(root)) { console.log("NEW"); process.exit(0); }
function find(dir) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    if (e.name === "_site") continue;
    const full = path.join(dir, e.name);
    if (e.isDirectory()) { const r = find(full); if (r) return r; }
    else if (e.name === slug + ".md") return full;
  }
  return null;
}
const found = find(root);
if (found) console.log("EXISTS " + found);
else console.log("NEW");