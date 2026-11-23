#!/usr/bin/env node
// 从 frontmatter 的 related / depends_on 生成 SVG 关联图
const path = require("path");
const fs = require("fs");
const lib = require("./lib");

const CAT_COLOR = {
  principles: "#4a90e2",
  decisions: "#27ae60",
  methods: "#8e44ad",
  pitfalls: "#e67e22"
};

function resolveRef(ref, node, byId) {
  if (!ref) return null;
  if (ref.indexOf("/") >= 0) return byId[ref] ? ref : null;
  const sameCat = node.cat + "/" + ref;
  if (byId[sameCat]) return sameCat;
  for (let i = 0; i < lib.CATS.length; i++) {
    const cand = lib.CATS[i] + "/" + ref;
    if (byId[cand]) return cand;
  }
  return null;
}

function buildRelatedMap(knowledgeDir) {
  const entries = lib.collectEntries(knowledgeDir);
  const byId = {};
  entries.forEach(function (e) { byId[e.id] = e; });

  const edges = [];
  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    e.related.forEach(function (ref) {
      const t = resolveRef(ref, e, byId);
      if (t && t !== e.id) edges.push({ from: e.id, to: t, kind: "related" });
    });
    e.depends_on.forEach(function (ref) {
      const t = resolveRef(ref, e, byId);
      if (t && t !== e.id) edges.push({ from: e.id, to: t, kind: "depends_on" });
    });
  }

  if (entries.length === 0) {
    return { title: "关联图", label: "关联图", bodyHtml: "<p class=\"empty\">暂无条目。</p>", count: 0, edges: [] };
  }

  const cols = {};
  lib.CATS.forEach(function (c) { cols[c] = []; });
  entries.forEach(function (e) { cols[e.cat].push(e); });

  const NODE_W = 190, NODE_H = 40, COL_GAP = 70, ROW_GAP = 20, PAD = 40, HEADER_H = 32;
  let maxRows = 0;
  lib.CATS.forEach(function (c) { if (cols[c].length > maxRows) maxRows = cols[c].length; });
  const W = PAD * 2 + lib.CATS.length * NODE_W + (lib.CATS.length - 1) * COL_GAP;
  const H = PAD * 2 + HEADER_H + Math.max(1, maxRows) * (NODE_H + ROW_GAP);

  const pos = {};
  lib.CATS.forEach(function (cat, ci) {
    const x = PAD + ci * (NODE_W + COL_GAP);
    cols[cat].forEach(function (e, ri) {
      pos[e.id] = { x: x, y: PAD + HEADER_H + ri * (NODE_H + ROW_GAP) };
    });
  });

  const svg = [];
  svg.push("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + W + "\" height=\"" + H + "\" viewBox=\"0 0 " + W + " " + H + "\">");
  svg.push("<defs><marker id=\"arrow\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" orient=\"auto\"><path d=\"M 0 0 L 10 5 L 0 10 z\" fill=\"#8a99aa\"/></marker></defs>");

  lib.CATS.forEach(function (cat, ci) {
    const x = PAD + ci * (NODE_W + COL_GAP);
    svg.push("<text x=\"" + x + "\" y=\"" + (PAD + 14) + "\" font-size=\"13\" font-weight=\"600\" fill=\"" + CAT_COLOR[cat] + "\">" + lib.esc(cat) + "</text>");
  });

  edges.forEach(function (e) {
    const a = pos[e.from], b = pos[e.to];
    if (!a || !b) return;
    let x1, y1, x2, y2;
    if (b.x > a.x) { x1 = a.x + NODE_W; y1 = a.y + NODE_H / 2; x2 = b.x; y2 = b.y + NODE_H / 2; }
    else if (b.x < a.x) { x1 = a.x; y1 = a.y + NODE_H / 2; x2 = b.x + NODE_W; y2 = b.y + NODE_H / 2; }
    else { x1 = a.x + NODE_W / 2; y1 = a.y + NODE_H; x2 = b.x + NODE_W / 2; y2 = b.y; }
    const cx1 = (x1 + x2) / 2, cx2 = (x1 + x2) / 2;
    const dash = e.kind === "depends_on" ? " stroke-dasharray=\"5,3\"" : "";
    const stroke = e.kind === "depends_on" ? "#a25" : "#8a99aa";
    svg.push("<path d=\"M " + x1 + " " + y1 + " C " + cx1 + " " + y1 + ", " + cx2 + " " + y2 + ", " + x2 + " " + y2 + "\" fill=\"none\" stroke=\"" + stroke + "\" stroke-width=\"1.5\"" + dash + " marker-end=\"url(#arrow)\"/>");
  });

  entries.forEach(function (e) {
    const p = pos[e.id];
    const color = CAT_COLOR[e.cat] || "#888";
    svg.push("<g>");
    svg.push("<rect x=\"" + p.x + "\" y=\"" + p.y + "\" width=\"" + NODE_W + "\" height=\"" + NODE_H + "\" rx=\"6\" fill=\"#fff\" stroke=\"" + color + "\" stroke-width=\"1.5\"/>");
    const label = e.title.length > 24 ? e.title.slice(0, 23) + "…" : e.title;
    svg.push("<text x=\"" + (p.x + NODE_W / 2) + "\" y=\"" + (p.y + NODE_H / 2 + 4) + "\" text-anchor=\"middle\" font-size=\"12\" fill=\"#334\">" + lib.esc(label) + "</text>");
    svg.push("<title>" + lib.esc(e.id) + "</title>");
    svg.push("</g>");
  });
  svg.push("</svg>");

  const legend = "<div class=\"legend\"><span><strong>—</strong> related</span><span><strong style=\"color:#a25\">--</strong> depends_on</span></div>";
  const hint = "<p class=\"hint\">从每条 md 的 <code>related</code> / <code>depends_on</code> 生成。悬停节点看完整路径。</p>";
  const bodyHtml = hint + "<div class=\"svg-wrap\">" + svg.join("") + "</div>" + legend;

  return { title: "关联图", label: "关联图", bodyHtml: bodyHtml, count: entries.length, edges: edges };
}

module.exports = { buildRelatedMap: buildRelatedMap };