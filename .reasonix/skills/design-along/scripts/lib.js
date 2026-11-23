// 公共工具：frontmatter 解析、摘要提取、HTML 转义
const fs = require("fs");
const path = require("path");

const CATS = ["principles", "decisions", "methods", "pitfalls"];

function parseFrontmatter(text) {
  const m = text.match(/^---\r?\n([\s\S]*?)\r?\n---/);
  if (!m) return {};
  const fm = {};
  const lines = m[1].split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const idx = line.indexOf(":");
    if (idx < 0) continue;
    const key = line.slice(0, idx).trim();
    const raw = line.slice(idx + 1).trim();
    if (raw === "") {
      fm[key] = "";
    } else if (raw.charAt(0) === "[" && raw.charAt(raw.length - 1) === "]") {
      fm[key] = raw.slice(1, -1).split(",").map(function (s) { return s.trim(); }).filter(Boolean);
    } else {
      fm[key] = raw;
    }
  }
  return fm;
}

function stripFrontmatter(text) {
  return text.replace(/^---\r?\n[\s\S]*?\r?\n---\r?\n?/, "");
}

function extractSummary(body) {
  const lines = body.split(/\r?\n/).map(function (l) { return l.trim(); }).filter(function (l) { return l && l.charAt(0) !== "#"; });
  for (let i = 0; i < lines.length; i++) {
    const m = lines[i].match(/\*\*[^*]+\*\*[：:]\s*(.+)/);
    if (m) return truncate(m[1], 120);
  }
  return truncate(lines[0] || "", 120);
}

function truncate(s, n) {
  if (s.length <= n) return s;
  return s.slice(0, n - 1) + "…";
}

function esc(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function toArray(v) {
  if (!v) return [];
  if (Array.isArray(v)) return v;
  return [v];
}

function collectEntries(knowledgeDir) {
  const entries = [];
  for (let i = 0; i < CATS.length; i++) {
    const cat = CATS[i];
    const dir = path.join(knowledgeDir, cat);
    if (!fs.existsSync(dir)) continue;
    const files = fs.readdirSync(dir).filter(function (f) { return f.endsWith(".md"); });
    for (let j = 0; j < files.length; j++) {
      const f = files[j];
      const content = fs.readFileSync(path.join(dir, f), "utf8");
      const fm = parseFrontmatter(content);
      const body = stripFrontmatter(content);
      const slug = fm.slug || f.replace(/\.md$/, "");
      entries.push({
        id: cat + "/" + f.replace(/\.md$/, ""),
        cat: cat,
        rel: cat + "/" + f,
        slug: slug,
        title: slug,
        status: fm.status || "active",
        related: toArray(fm.related),
        depends_on: toArray(fm.depends_on),
        summary: extractSummary(body)
      });
    }
  }
  return entries;
}

module.exports = { CATS: CATS, parseFrontmatter: parseFrontmatter, stripFrontmatter: stripFrontmatter, extractSummary: extractSummary, esc: esc, truncate: truncate, collectEntries: collectEntries };