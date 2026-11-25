// Nova Notes editor UI. Talks to the plugin through JUCE native functions;
// opened in a normal browser it runs on built-in demo data so it can be previewed.

const inPlugin = typeof window.__JUCE__ !== "undefined" && window.__JUCE__.backend;
let getNativeFunction = null;
if (inPlugin) ({ getNativeFunction } = await import("./juce/index.js"));

const nativeCache = {};
function native(name, ...args) {
  if (!inPlugin) return Promise.resolve(Mock[name] ? Mock[name](...args) : undefined);
  nativeCache[name] = nativeCache[name] || getNativeFunction(name);
  return nativeCache[name](...args);
}
window.addEventListener("error", e => { if (inPlugin) native("jsError", String(e.message) + " @" + e.lineno); });

// ── constants & helpers ─────────────────────────────────────────────────────
const SHARP = ["C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"];
const FLAT  = ["C","D♭","D","E♭","E","F","G♭","G","A♭","A","B♭","B"];
const MAJOR = [0,2,4,5,7,9,11], MINOR = [0,2,3,5,7,8,10];
const $ = id => document.getElementById(id);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const pc = m => ((Math.round(m) % 12) + 12) % 12;

// ── state ───────────────────────────────────────────────────────────────────
let track = null;            // {hop, sr, dec, frames, midi[], db[]}
let notes = [];              // [{id,s,e,c,p,dr,vb,tr,fm,g,ts}]
let info = { tonic: 0, minor: false, keyConf: 0, seconds: 0, bpm: 0, ppq: 0 };
let status = { cap: 0, capSec: 0, play: false, ph: -1, busy: false, analysing: false, undo: false, redo: false, orig: false, msg: "" };
let tool = "main";
const sel = new Set();
const view = { t0: 0, pps: 120, midiTop: 76, rowH: 20 };
let follow = true;
const derived = new Map();   // note id -> {key, dev[], slow[], fast[], db[]}

const useFlats = () => [1,3,5,8,10].includes(info.tonic) || (info.minor && [0,2,5,7].includes(info.tonic));
const nameOf = m => (useFlats() ? FLAT : SHARP)[pc(m)] + (Math.floor(Math.round(m) / 12) - 1);
const scaleSet = () => new Set((info.minor ? MINOR : MAJOR).map(s => (s + info.tonic) % 12));
const frameSec = () => track ? track.hop / track.sr : 0.005;
const noteStart = n => n.s * frameSec() + n.ts;
const noteEnd = n => n.e * frameSec() + n.ts;

// per-note slow/fast decomposition (mirrors the C++ engine, at display resolution)
function movingAvg(v, r) {
  const n = v.length, cum = new Float64Array(n + 1), out = new Float32Array(n);
  for (let i = 0; i < n; i++) cum[i + 1] = cum[i] + v[i];
  for (let i = 0; i < n; i++) { const a = Math.max(0, i - r), b = Math.min(n, i + r + 1); out[i] = (cum[b] - cum[a]) / (b - a); }
  return out;
}
function derive(n) {
  const key = n.s + ":" + n.e + ":" + n.c;
  const d = derived.get(n.id);
  if (d && d.key === key) return d;
  const dec = track.dec, a = Math.floor(n.s / dec), b = Math.max(a + 1, Math.ceil(n.e / dec));
  const dev = new Float32Array(b - a), db = new Float32Array(b - a), voiced = new Uint8Array(b - a);
  let last = null;
  for (let i = 0; i < dev.length; i++) {
    const m = track.midi[a + i] || 0;
    db[i] = track.db[a + i] ?? -90;
    if (m > 0) { last = m - n.c; voiced[i] = 1; }
    dev[i] = last ?? 0;
  }
  const firstV = voiced.indexOf(1);
  if (firstV > 0) for (let i = 0; i < firstV; i++) dev[i] = dev[firstV];
  const r = Math.max(1, Math.round(15 / dec));
  const slow = movingAvg(movingAvg(dev, r), r);
  const fast = dev.map((v, i) => v - slow[i]);
  const out = { key, a, dev, slow, fast, db, voiced };
  derived.set(n.id, out);
  return out;
}
const pitchAt = (n, d, i) => n.p + d.dev[i] - n.dr * d.slow[i] + (n.vb - 1) * d.fast[i];

// ── canvas setup ────────────────────────────────────────────────────────────
const roll = $("roll"), ctx = roll.getContext("2d");
const ov = $("overview"), octx = ov.getContext("2d");
const KEYW = 46, RULER = 26;
let W = 0, H = 0, OW = 0, OH = 0, dpr = 1;
function resize() {
  dpr = window.devicePixelRatio || 1;
  const r = roll.getBoundingClientRect(); W = r.width; H = r.height;
  roll.width = W * dpr; roll.height = H * dpr;
  const o = ov.getBoundingClientRect(); OW = o.width; OH = o.height;
  ov.width = OW * dpr; ov.height = OH * dpr;
  draw();
}
new ResizeObserver(resize).observe($("editor"));
new ResizeObserver(resize).observe(ov);

const xOf = t => KEYW + (t - view.t0) * view.pps;
const tOf = x => view.t0 + (x - KEYW) / view.pps;
const yOf = m => RULER + (view.midiTop - m) * view.rowH;
const mOf = y => view.midiTop - (y - RULER) / view.rowH;

// ── drawing ─────────────────────────────────────────────────────────────────
let drawQueued = false;
function draw() { if (!drawQueued) { drawQueued = true; requestAnimationFrame(() => { drawQueued = false; drawRoll(); drawOverview(); }); } }

function drawRoll() {
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.fillStyle = "#0a0a10"; ctx.fillRect(0, 0, W, H);
  const sc = scaleSet();
  const lo = Math.floor(mOf(H)) - 1, hi = Math.ceil(mOf(RULER)) + 1;

  // lanes
  for (let m = lo; m <= hi; m++) {
    const y = yOf(m + 0.5), black = [1,3,6,8,10].includes(((m % 12) + 12) % 12);
    ctx.fillStyle = sc.has(((m % 12) + 12) % 12) ? (black ? "#101019" : "#12121c") : "#0b0b12";
    ctx.fillRect(KEYW, y, W - KEYW, view.rowH);
    ctx.fillStyle = ((m % 12) + 12) % 12 === 0 ? "#262634" : "#17171f";
    ctx.fillRect(KEYW, y + view.rowH - 1, W - KEYW, 1);
  }
  drawGrid();

  if (track) {
    const t0 = tOf(KEYW) - 1, t1 = tOf(W) + 1;
    const vis = notes.filter(n => noteEnd(n) > t0 && noteStart(n) < t1);
    // ghosts of moved notes
    ctx.setLineDash([3, 3]); ctx.strokeStyle = "rgba(255,184,107,.55)"; ctx.lineWidth = 1;
    for (const n of vis) {
      if (Math.abs(n.p - n.c) < 0.02 && Math.abs(n.ts) < 0.001) continue;
      const x0 = xOf(n.s * frameSec()), x1 = xOf(n.e * frameSec());
      roundRect(x0, yOf(n.c) - view.rowH * 0.42, x1 - x0, view.rowH * 0.84, 7); ctx.stroke();
    }
    ctx.setLineDash([]);
    // transitions
    ctx.strokeStyle = "rgba(156,125,255,.45)"; ctx.lineWidth = 1;
    for (let i = 0; i + 1 < notes.length; i++) {
      const a = notes[i], b = notes[i + 1];
      if (noteEnd(a) < t0 || noteStart(b) > t1 || (b.s - a.e) > 12) continue;
      const da = derive(a), dbb = derive(b);
      ctx.beginPath();
      ctx.moveTo(xOf(noteEnd(a)), yOf(pitchAt(a, da, da.dev.length - 1)));
      ctx.lineTo(xOf(noteStart(b)), yOf(pitchAt(b, dbb, 0)));
      ctx.stroke();
    }
    for (const n of vis) drawNote(n);
  }

  // playhead
  if (status.ph >= 0) {
    const x = xOf(status.ph);
    if (x >= KEYW && x <= W) {
      ctx.fillStyle = "#7cd1ff"; ctx.fillRect(x - 0.75, 0, 1.5, H);
      ctx.beginPath(); ctx.moveTo(x - 6, 0); ctx.lineTo(x + 6, 0); ctx.lineTo(x, 8); ctx.fill();
    }
  }
  if (marquee) {
    ctx.fillStyle = "rgba(156,125,255,.10)"; ctx.strokeStyle = "rgba(183,155,255,.7)";
    const x = Math.min(marquee.x0, marquee.x1), y = Math.min(marquee.y0, marquee.y1);
    ctx.fillRect(x, y, Math.abs(marquee.x1 - marquee.x0), Math.abs(marquee.y1 - marquee.y0));
    ctx.strokeRect(x + .5, y + .5, Math.abs(marquee.x1 - marquee.x0), Math.abs(marquee.y1 - marquee.y0));
  }
  drawKeys(lo, hi, sc);
}

function roundRect(x, y, w, h, r) {
  r = Math.min(r, w / 2, h / 2);
  ctx.beginPath(); ctx.moveTo(x + r, y); ctx.arcTo(x + w, y, x + w, y + h, r); ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r); ctx.arcTo(x, y, x + w, y, r); ctx.closePath();
}

function drawNote(n) {
  const d = derive(n), N = d.dev.length, isSel = sel.has(n.id);
  const x0 = xOf(noteStart(n)), x1 = xOf(noteEnd(n)), yc = yOf(n.p);
  const gainScale = Math.pow(10, n.g / 40);
  // blob shaped by loudness
  ctx.beginPath();
  for (let i = 0; i < N; i++) {
    const x = x0 + (x1 - x0) * (N > 1 ? i / (N - 1) : 0.5);
    const amp = clamp((d.db[i] + 52) / 42, 0, 1) * gainScale;
    const h = view.rowH * (0.18 + 0.30 * clamp(amp, 0, 1.3));
    i ? ctx.lineTo(x, yc - h) : ctx.moveTo(x, yc - h);
  }
  for (let i = N - 1; i >= 0; i--) {
    const x = x0 + (x1 - x0) * (N > 1 ? i / (N - 1) : 0.5);
    const amp = clamp((d.db[i] + 52) / 42, 0, 1) * gainScale;
    ctx.lineTo(x, yc + view.rowH * (0.18 + 0.30 * clamp(amp, 0, 1.3)));
  }
  ctx.closePath();
  const g = ctx.createLinearGradient(0, yc - view.rowH * 0.5, 0, yc + view.rowH * 0.5);
  const edited = Math.abs(n.p - n.c) > 0.02 || n.dr > 0.001 || Math.abs(n.vb - 1) > 0.01 || Math.abs(n.fm) > 0.5 || Math.abs(n.g) > 0.05 || Math.abs(n.ts) > 0.001;
  if (isSel) { g.addColorStop(0, "#c3adff"); g.addColorStop(1, "#8f6cff"); }
  else if (edited) { g.addColorStop(0, "#8d74f2"); g.addColorStop(1, "#6247d6"); }
  else { g.addColorStop(0, "#6c56c8"); g.addColorStop(1, "#4b37a3"); }
  ctx.fillStyle = g; ctx.fill();
  ctx.strokeStyle = isSel ? "#e2d7ff" : edited ? "#a58cff" : "#7a63de"; ctx.lineWidth = isSel ? 1.5 : 1; ctx.stroke();
  if (Math.abs(n.fm) > 0.5) { ctx.fillStyle = "rgba(124,209,255,.85)"; ctx.font = "600 9px -apple-system"; ctx.fillText((n.fm > 0 ? "+" : "") + Math.round(n.fm) + "¢F", x0 + 3, yc - view.rowH * 0.55); }

  // pitch curve (what will be heard)
  if (x1 - x0 > 6) {
    ctx.beginPath(); let pen = false;
    for (let i = 0; i < N; i++) {
      if (!d.voiced[i]) { pen = false; continue; }
      const x = x0 + (x1 - x0) * (N > 1 ? i / (N - 1) : 0.5), y = yOf(pitchAt(n, d, i));
      pen ? ctx.lineTo(x, y) : ctx.moveTo(x, y); pen = true;
    }
    ctx.strokeStyle = isSel ? "#ffffff" : "rgba(236,230,255,.85)"; ctx.lineWidth = 1.3; ctx.stroke();
  }
}

function drawGrid() {
  // beat/bar grid when the host tempo is known, seconds otherwise
  ctx.fillStyle = "#101018"; ctx.fillRect(0, 0, W, RULER);
  ctx.fillStyle = "#23232f"; ctx.fillRect(0, RULER - 1, W, 1);
  ctx.font = "600 11px -apple-system"; ctx.fillStyle = "#8a8a9c";
  const t0 = tOf(KEYW), t1 = tOf(W);
  if (info.bpm > 0) {
    const spb = 60 / info.bpm;
    const beatPx = spb * view.pps;
    const sub = beatPx > 120 ? 4 : beatPx > 50 ? 2 : 1;
    const kFirst = Math.floor(info.ppq + t0 / spb) - 1, kLast = Math.ceil(info.ppq + t1 / spb) + 1;
    for (let k = kFirst; k <= kLast; k++) {
      for (let s = 0; s < sub; s++) {
        const beat = k + s / sub, x = xOf((beat - info.ppq) * spb);
        if (x < KEYW) continue;
        const bar = Math.abs(beat % 4) < 1e-6;
        if (beatPx * 4 < 40 && !bar) continue;
        ctx.fillStyle = bar ? "#2a2a3a" : s === 0 ? "#1a1a24" : "#14141c";
        ctx.fillRect(x, RULER, 1, H - RULER);
        if (bar) { ctx.fillStyle = "#8a8a9c"; ctx.fillText(String(Math.floor(beat / 4) + 1), x + 5, 17); }
      }
    }
  } else {
    const step = [0.1, 0.25, 0.5, 1, 2, 5, 10, 30].find(s => s * view.pps > 70) || 60;
    for (let t = Math.floor(t0 / step) * step; t <= t1; t += step) {
      const x = xOf(t); if (x < KEYW) continue;
      ctx.fillStyle = "#1c1c27"; ctx.fillRect(x, RULER, 1, H - RULER);
      ctx.fillStyle = "#8a8a9c"; ctx.fillText(fmtTime(t), x + 5, 17);
    }
  }
}

function drawKeys(lo, hi, sc) {
  ctx.fillStyle = "#0a0a10"; ctx.fillRect(0, RULER, KEYW, H - RULER);
  for (let m = lo; m <= hi; m++) {
    const y = yOf(m + 0.5), p = ((m % 12) + 12) % 12, black = [1,3,6,8,10].includes(p);
    if (y + view.rowH < RULER) continue;
    ctx.fillStyle = black ? "#1a1a24" : "#d9d9e3";
    ctx.fillRect(0, y + 0.5, KEYW - 1, view.rowH - 1);
    if (sc.has(p)) { ctx.fillStyle = "#6ee7a8"; ctx.fillRect(KEYW - 4, y + 1.5, 3, view.rowH - 3); }
    if (p === 0 || p === info.tonic) {
      ctx.fillStyle = black ? "#d9d9e3" : "#0a0a10"; ctx.font = "700 9.5px -apple-system";
      ctx.fillText(nameOf(m), 5, y + view.rowH - 6);
    }
  }
  ctx.fillStyle = "#101018"; ctx.fillRect(0, 0, KEYW, RULER);
}

function drawOverview() {
  octx.setTransform(dpr, 0, 0, dpr, 0, 0);
  octx.fillStyle = "#0e0e15"; octx.fillRect(0, 0, OW, OH);
  octx.font = "700 9.5px -apple-system"; octx.fillStyle = "#555566";
  if (!track || !info.seconds) { octx.fillText("NO TAKE", 10, 14); return; }
  const total = info.seconds, n = track.db.length;
  octx.fillStyle = "#3b2f6e";
  for (let x = 0; x < OW; x++) {
    const i0 = Math.floor(x / OW * n), i1 = Math.max(i0 + 1, Math.floor((x + 1) / OW * n));
    let mx = -90; for (let i = i0; i < i1; i++) mx = Math.max(mx, track.db[i]);
    const h = clamp((mx + 60) / 55, 0, 1) * (OH / 2 - 6);
    octx.fillRect(x, OH / 2 - h, 1, h * 2);
  }
  octx.fillStyle = "rgba(183,155,255,.9)";
  for (const nt of notes) {
    const x0 = noteStart(nt) / total * OW, x1 = noteEnd(nt) / total * OW;
    const y = OH - 6 - clamp((nt.p - 40) / 45, 0, 1) * (OH - 12);
    octx.fillRect(x0, y, Math.max(1, x1 - x0), 2);
  }
  const vx0 = (view.t0 / total) * OW, vx1 = (tOf(W) / total) * OW;
  octx.fillStyle = "rgba(156,125,255,.12)"; octx.strokeStyle = "#9c7dff";
  octx.fillRect(vx0, 3, vx1 - vx0, OH - 6); octx.strokeRect(vx0 + .5, 3.5, vx1 - vx0 - 1, OH - 7);
  if (status.ph >= 0) { octx.fillStyle = "#7cd1ff"; octx.fillRect(status.ph / total * OW, 0, 1.5, OH); }
  octx.fillStyle = "#8a8a9c"; octx.fillText("TAKE", 8, 13);
}

const fmtTime = t => { const m = Math.floor(t / 60), s = t - m * 60; return m + ":" + (s < 10 ? "0" : "") + s.toFixed(s % 1 ? 1 : 0); };

// ── hit testing ─────────────────────────────────────────────────────────────
function noteAt(x, y) {
  const t = tOf(x), m = mOf(y);
  let best = null, bestD = 0.75;
  for (const n of notes) {
    if (t < noteStart(n) || t > noteEnd(n)) continue;
    const d = Math.abs(m - n.p);
    if (d < bestD) { bestD = d; best = n; }
  }
  return best;
}

// ── editing (optimistic locally, then sent to the plugin) ───────────────────
let pendingPatches = new Map(), patchTimer = null, patchUndo = false;
function patch(list, pushUndo) {
  for (const p of list) { const n = notes.find(o => o.id === p.id); if (n) Object.assign(n, p); }
  for (const p of list) pendingPatches.set(p.id, Object.assign(pendingPatches.get(p.id) || {}, p));
  patchUndo = patchUndo || pushUndo;
  if (!patchTimer) patchTimer = setTimeout(flushPatches, 30);
  draw(); updateInspector();
}
function flushPatches() {
  clearTimeout(patchTimer); patchTimer = null;
  if (!pendingPatches.size) return;
  const list = [...pendingPatches.values()], undo = patchUndo;
  pendingPatches.clear(); patchUndo = false;
  native("patch", list, undo);
}
const selectedIds = () => [...sel];
const scopeIds = () => sel.size ? [...sel] : [];

// ── mouse ───────────────────────────────────────────────────────────────────
let drag = null, marquee = null;
roll.addEventListener("mousedown", e => {
  if (!track) return;
  const r = roll.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top;
  if (x < KEYW || y < RULER) return;
  const n = noteAt(x, y);

  if (tool === "split") {
    if (n) { const f = Math.round((tOf(x) - n.ts) / frameSec()); native("split", n.id, f); }
    return;
  }
  if (!n) {
    if (!e.shiftKey) sel.clear();
    marquee = { x0: x, y0: y, x1: x, y1: y, base: new Set(sel) };
    updateInspector(); draw(); return;
  }
  if (e.shiftKey) { sel.has(n.id) ? sel.delete(n.id) : sel.add(n.id); }
  else if (!sel.has(n.id)) { sel.clear(); sel.add(n.id); }
  updateInspector();
  const start = notes.filter(o => sel.has(o.id)).map(o => ({ ...o }));
  drag = { x0: x, y0: y, start, axis: null, first: true };
  draw();
});
window.addEventListener("mousemove", e => {
  const r = roll.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top;
  if (marquee) {
    marquee.x1 = x; marquee.y1 = y;
    const ta = tOf(Math.min(marquee.x0, x)), tb = tOf(Math.max(marquee.x0, x));
    const ma = mOf(Math.max(marquee.y0, y)), mb = mOf(Math.min(marquee.y0, y));
    sel.clear(); marquee.base.forEach(id => sel.add(id));
    for (const n of notes) if (noteEnd(n) > ta && noteStart(n) < tb && n.p > ma - 0.5 && n.p < mb + 0.5) sel.add(n.id);
    updateInspector(); draw(); return;
  }
  if (!drag) { roll.style.cursor = hoverCursor(x, y); return; }
  const dx = x - drag.x0, dy = y - drag.y0;
  if (!drag.axis) {
    if (Math.abs(dx) < 4 && Math.abs(dy) < 4) return;
    drag.axis = (tool === "time" || (tool === "main" && Math.abs(dx) > Math.abs(dy))) ? "x" : "y";
  }
  const semis = -dy / view.rowH, list = [];
  for (const o of drag.start) {
    const p = { id: o.id };
    if (drag.axis === "x") {
      if (tool !== "main" && tool !== "time") continue;
      p.ts = clamp(o.ts + dx / view.pps, -2, 2);
    } else if (tool === "main") {
      p.p = e.altKey ? o.p + semis : (Math.abs(semis) < 0.5 ? o.p : Math.round(o.p) + Math.round(semis));
    } else if (tool === "pitch") {
      p.p = o.p + semis * (e.shiftKey ? 0.25 : 1) * 0.5;
    } else if (tool === "drift") {
      if (e.shiftKey) p.vb = clamp(o.vb - dy / 120, 0, 2); else p.dr = clamp(o.dr + dy / 120, 0, 1);
    } else if (tool === "formant") {
      p.fm = clamp(o.fm - dy * 3, -1200, 1200);
    } else if (tool === "level") {
      p.g = clamp(o.g - dy * 0.1, -48, 12);
    } else continue;
    list.push(p);
  }
  if (list.length) { patch(list, drag.first); drag.first = false; }
});
window.addEventListener("mouseup", () => {
  if (marquee) { marquee = null; draw(); }
  if (drag) { drag = null; flushPatches(); }
});
roll.addEventListener("dblclick", e => {
  const r = roll.getBoundingClientRect(), n = noteAt(e.clientX - r.left, e.clientY - r.top);
  if (!n || (tool !== "main" && tool !== "pitch")) return;
  const ids = sel.has(n.id) ? [...sel] : [n.id];
  patch(ids.map(id => { const o = notes.find(q => q.id === id); return { id, p: Math.round(o.c) }; }), true);
  flushPatches();
});
function hoverCursor(x, y) {
  if (!track || x < KEYW || y < RULER) return "default";
  const n = noteAt(x, y);
  if (!n) return "crosshair";
  return { main: "move", pitch: "ns-resize", drift: "ns-resize", time: "ew-resize", formant: "ns-resize", level: "ns-resize", split: "col-resize" }[tool];
}

roll.addEventListener("wheel", e => {
  e.preventDefault();
  if (e.metaKey || e.ctrlKey) {
    const r = roll.getBoundingClientRect(), x = e.clientX - r.left, t = tOf(x);
    view.pps = clamp(view.pps * Math.exp(-e.deltaY * 0.004), 8, 1200);
    view.t0 = t - (x - KEYW) / view.pps;
  } else if (e.altKey) {
    const r = roll.getBoundingClientRect(), y = e.clientY - r.top, m = mOf(y);
    view.rowH = clamp(view.rowH * Math.exp(-e.deltaY * 0.004), 8, 48);
    view.midiTop = m + (y - RULER) / view.rowH;
  } else {
    const horiz = e.shiftKey ? e.deltaY : e.deltaX;
    view.t0 += horiz / view.pps;
    if (!e.shiftKey) view.midiTop -= e.deltaY / view.rowH * 0.5;
    if (Math.abs(horiz) > 0) follow = false;
  }
  clampView(); draw();
}, { passive: false });

ov.addEventListener("mousedown", e => {
  if (!info.seconds) return;
  const move = ev => {
    const r = ov.getBoundingClientRect();
    const t = clamp((ev.clientX - r.left) / r.width, 0, 1) * info.seconds;
    view.t0 = t - (tOf(W) - view.t0) / 2; clampView(); follow = false; draw();
  };
  move(e);
  const up = () => { window.removeEventListener("mousemove", move); window.removeEventListener("mouseup", up); };
  window.addEventListener("mousemove", move); window.addEventListener("mouseup", up);
});

function clampView() {
  const span = (W - KEYW) / view.pps;
  view.t0 = clamp(view.t0, -0.5, Math.max(0, (info.seconds || 10) - span * 0.5));
  view.midiTop = clamp(view.midiTop, 30, 100);
}
function fitView() {
  if (!track || !notes.length) return;
  const span = Math.max(1, info.seconds);
  view.pps = clamp((W - KEYW - 20) / span, 8, 1200);
  view.t0 = 0;
  fitPitch(notes);
  draw();
}
function fitPitch(list) {
  let lo = 127, hi = 0;
  for (const n of list) { lo = Math.min(lo, n.p); hi = Math.max(hi, n.p); }
  const rows = (H - RULER) / view.rowH;
  view.midiTop = (lo + hi) / 2 + rows / 2;
  if (hi - lo + 4 > rows) { view.rowH = clamp((H - RULER) / (hi - lo + 4), 8, 48); view.midiTop = hi + 2; }
}
function zoomBy(f) {
  const tc = tOf((W + KEYW) / 2);
  view.pps = clamp(view.pps * f, 8, 1200);
  view.t0 = tc - (W - KEYW) / 2 / view.pps; clampView(); draw();
}

// ── keyboard ────────────────────────────────────────────────────────────────
const TOOLS = ["main", "pitch", "drift", "time", "formant", "level", "split"];
window.addEventListener("keydown", e => {
  if (e.target.tagName === "SELECT" || e.target.tagName === "INPUT" && e.target.type !== "range") return;
  const cmd = e.metaKey || e.ctrlKey;
  if (cmd && e.key.toLowerCase() === "z") { e.preventDefault(); flushPatches(); native(e.shiftKey ? "redo" : "undo"); return; }
  if (cmd && e.key.toLowerCase() === "a") { e.preventDefault(); notes.forEach(n => sel.add(n.id)); updateInspector(); draw(); return; }
  if (e.key === "Escape") { sel.clear(); updateInspector(); draw(); return; }
  if (!cmd && e.key >= "1" && e.key <= "7") { setTool(TOOLS[+e.key - 1]); return; }
  if ((e.key === "ArrowUp" || e.key === "ArrowDown") && sel.size) {
    e.preventDefault();
    const d = (e.key === "ArrowUp" ? 1 : -1) * (e.altKey ? 0.1 : 1);
    patch(notes.filter(n => sel.has(n.id)).map(n => ({ id: n.id, p: e.altKey ? n.p + d : Math.round(n.p) + d })), true);
    flushPatches(); return;
  }
  if ((e.key === "Backspace" || e.key === "Delete") && sel.size) { native("reset", [...sel]); return; }
  if (e.key === "f") { follow = !follow; toast(follow ? "Following the playhead" : "Not following the playhead"); }
});

// ── toolbar / top bar ───────────────────────────────────────────────────────
function setTool(t) {
  tool = t;
  document.querySelectorAll(".tool").forEach(b => b.classList.toggle("on", b.dataset.t === t));
}
document.querySelectorAll(".tool").forEach(b => b.onclick = () => setTool(b.dataset.t));

$("capBtn").onclick = () => native("capture", status.cap === 0 ? "arm" : "stop");
let clearArmed = 0;
$("clearBtn").onclick = () => {
  if (!track) return;
  if (Date.now() - clearArmed < 2500) { native("capture", "clear"); clearArmed = 0; sel.clear(); return; }
  clearArmed = Date.now(); toast("Click ✕ again to throw away this take and its edits");
};
$("undoBtn").onclick = () => { flushPatches(); native("undo"); };
$("redoBtn").onclick = () => { flushPatches(); native("redo"); };
$("cmpOrig").onclick = () => native("setCompare", true);
$("cmpEdit").onclick = () => native("setCompare", false);
$("zoomIn").onclick = () => zoomBy(1.5);
$("zoomOut").onclick = () => zoomBy(1 / 1.5);
$("zoomFit").onclick = fitView;
$("resetBtn").onclick = () => native("reset", scopeIds());
$("quantBtn").onclick = () => native("quantize", scopeIds(), 0.8).then(() => toast(info.bpm > 0 ? "Note starts pulled toward the 1/16 grid" : "Quantize needs the song tempo - recapture with the DAW playing"));

const keySels = [$("keySel"), $("aiKey")];
for (const ks of keySels) {
  for (let t = 0; t < 12; t++) for (const mi of [false, true]) {
    const o = document.createElement("option"); o.value = t + ":" + (mi ? 1 : 0);
    o.textContent = ([1,3,5,8,10].includes(t) || (mi && [0,2,5,7].includes(t)) ? FLAT : SHARP)[t] + (mi ? " minor" : " major");
    ks.appendChild(o);
  }
  ks.onchange = () => {
    const [t, m] = ks.value.split(":");
    info = { ...info, tonic: +t, minor: m === "1", keyConf: 1, userKey: true };
    native("setKey", +t, m === "1"); updateKeyUi(); draw();
  };
}

// AI assist
let aiStyle = 0;
document.querySelectorAll("#aiStyle button").forEach(b => b.onclick = () => {
  aiStyle = +b.dataset.s; document.querySelectorAll("#aiStyle button").forEach(o => o.classList.toggle("on", o === b));
});
$("aiBtn").onclick = async () => {
  if (!track) return;
  flushPatches();
  $("aiBtn").textContent = "Listening…";
  const msg = await native("assist", aiStyle, scopeIds());
  $("aiBtn").textContent = "✦ Tune it for me";
  if (msg) toast("<b>AI Assist</b> · " + msg, 5000);
};

// Correct pitch macro
let snapMode = 0;
document.querySelectorAll("#snapMode button").forEach(b => b.onclick = () => {
  snapMode = +b.dataset.m; document.querySelectorAll("#snapMode button").forEach(o => o.classList.toggle("on", o === b));
});
for (const [id, v] of [["mCenter", "mvCenter"], ["mDrift", "mvDrift"]]) $(id).oninput = () => $(v).textContent = $(id).value + "%";
$("cpBtn").onclick = () => { flushPatches(); native("correct", scopeIds(), $("mCenter").value / 100, $("mDrift").value / 100, snapMode); };

// Inspector sliders (absolute values for every selected note)
const sliders = {
  sCenter: { v: "vCenter", fmt: v => v + "%", apply: (n, v) => { const t = snapNote(n); return { p: n.c + (t - n.c) * v / 100 }; } },
  sDrift:  { v: "vDrift",  fmt: v => v + "%", apply: (n, v) => ({ dr: v / 100 }) },
  sVib:    { v: "vVib",    fmt: v => v + "%", apply: (n, v) => ({ vb: v / 100 }) },
  sTrans:  { v: "vTrans",  fmt: v => v + "%", apply: (n, v) => ({ tr: v / 100 }) },
  sForm:   { v: "vForm",   fmt: v => (v > 0 ? "+" : "") + v + "¢", apply: (n, v) => ({ fm: +v }) },
  sGain:   { v: "vGain",   fmt: v => (v > 0 ? "+" : "") + (v / 10).toFixed(1) + " dB", apply: (n, v) => ({ g: v / 10 }) },
};
function snapNote(n) {
  if (snapMode === 1) return Math.round(n.c);
  const sc = scaleSet(); let best = Math.round(n.c), bd = 99;
  for (let k = Math.floor(n.c) - 6; k <= Math.ceil(n.c) + 6; k++) if (sc.has(((k % 12) + 12) % 12) && Math.abs(k - n.c) < bd) { bd = Math.abs(k - n.c); best = k; }
  return best;
}
for (const [id, s] of Object.entries(sliders)) {
  const el = $(id); let first = true;
  el.addEventListener("pointerdown", () => first = true);
  el.oninput = () => {
    $(s.v).textContent = s.fmt(+el.value);
    const list = notes.filter(n => sel.has(n.id)).map(n => ({ id: n.id, ...s.apply(n, +el.value) }));
    if (list.length) { patch(list, first); first = false; }
  };
  el.onchange = () => { flushPatches(); first = true; };
}

function updateInspector() {
  const list = notes.filter(n => sel.has(n.id));
  $("selCard").classList.toggle("disabled", !list.length);
  $("selCount").textContent = list.length ? (list.length === 1 ? `${notes.indexOf(list[0]) + 1} of ${notes.length}` : `${list.length} notes`) : "none";
  const scope = list.length ? `${list.length} SELECTED` : "ALL NOTES";
  $("aiScope").textContent = scope; $("cpScope").textContent = scope;
  if (!list.length) {
    $("selName").textContent = "–"; $("selCents").textContent = "";
    $("selMeta").textContent = track ? "Click a note, or drag a box around several" : "Capture a take to start";
    return;
  }
  const n = list[0];
  const cents = Math.round((n.p - Math.round(n.p)) * 100), rawC = Math.round((n.c - Math.round(n.c)) * 100);
  $("selName").textContent = list.length === 1 ? nameOf(n.p) : nameOf(n.p) + " …";
  $("selCents").textContent = (cents > 0 ? "+" : "") + cents + "¢";
  $("selCents").style.color = Math.abs(cents) < 6 ? "#6ee7a8" : "#ffb86b";
  const d = derive(n);
  let fe = 0; for (const v of d.fast) fe += v * v;
  const vib = Math.sqrt(fe / Math.max(1, d.fast.length)) > 0.12 ? " · vibrato" : "";
  $("selMeta").textContent = `sung ${nameOf(n.c)} ${rawC > 0 ? "+" : ""}${rawC}¢ · ${((n.e - n.s) * frameSec()).toFixed(2)} s${vib}`;
  const t = snapNote(n), cAmt = Math.abs(t - n.c) > 0.005 ? clamp(Math.round((n.p - n.c) / (t - n.c) * 100), 0, 100) : (Math.abs(n.p - n.c) < 0.005 ? 0 : 100);
  const set = (id, v) => { if (document.activeElement !== $(id)) { $(id).value = v; $(sliders[id].v).textContent = sliders[id].fmt(+$(id).value); } };
  set("sCenter", cAmt); set("sDrift", Math.round(n.dr * 100)); set("sVib", Math.round(n.vb * 100));
  set("sTrans", Math.round(n.tr * 100)); set("sForm", Math.round(n.fm)); set("sGain", Math.round(n.g * 10));
}

// ── toast / empty state / status ────────────────────────────────────────────
let toastTimer = null;
function toast(html, ms = 2600) {
  const t = $("toast"); t.innerHTML = html; t.classList.add("show");
  clearTimeout(toastTimer); toastTimer = setTimeout(() => t.classList.remove("show"), ms);
}

function renderEmpty() {
  const box = $("emptyBox"), empty = $("empty");
  if (status.analysing) {
    empty.style.display = "grid";
    box.innerHTML = `<div class="spinner"></div><h2>Finding the notes…</h2><p>Nova Notes is listening to the take.</p>`;
  } else if (status.cap === 2) {
    empty.style.display = "grid";
    box.innerHTML = `<h2 style="color:#ff8a9a">● Capturing ${fmtTime(status.capSec)}</h2><p>Let the part play through. Stop the DAW (or click the capture button) when you're done.</p>`;
  } else if (status.cap === 1) {
    empty.style.display = "grid";
    box.innerHTML = `<h2 style="color:var(--orange)">Ready — press play in your DAW</h2><p>Play the section you want to tune from where it starts. Nova Notes records it once, then plays your edits back in time with the song.</p>`;
  } else if (!track) {
    empty.style.display = "grid";
    box.innerHTML = `<h2>Tune your vocal note by note</h2><p>Put Nova Notes first on the vocal track (before Nova Pitch). Press <b>Capture</b>, then play the part in your DAW. Every note shows up as a blob you can move, straighten or let AI Assist tune for you.</p><button class="big" id="emptyCap">● CAPTURE</button>`;
    $("emptyCap").onclick = () => native("capture", "arm");
  } else empty.style.display = "none";
}

let lastCap = -1, lastAnalysing = null;
function applyStatus(s) {
  const prevPh = status.ph;
  status = s;
  const cb = $("capBtn");
  cb.classList.toggle("armed", s.cap === 1); cb.classList.toggle("rec", s.cap === 2);
  $("capText").textContent = s.cap === 2 ? "REC " + fmtTime(s.capSec) : s.cap === 1 ? "ARMED · PRESS PLAY" : (track ? "RECAPTURE" : "CAPTURE");
  $("undoBtn").disabled = !s.undo; $("redoBtn").disabled = !s.redo;
  $("cmpOrig").classList.toggle("on", s.orig); $("cmpEdit").classList.toggle("on", !s.orig);
  const dot = $("stDot");
  dot.className = "dot" + (s.busy ? " busy" : s.play && s.ph >= 0 ? " on" : "");
  $("stText").innerHTML = s.msg ? s.msg
    : s.busy ? "Updating…"
    : !track ? "No take"
    : s.play && s.ph >= 0 ? `Playing <b>${s.orig ? "original" : "edited"}</b> audio in sync with your DAW`
    : s.play ? "Playhead is outside the captured take"
    : "Press play in your DAW to hear your edits";
  if (lastCap !== s.cap || lastAnalysing !== s.analysing || s.cap === 2) { renderEmpty(); lastCap = s.cap; lastAnalysing = s.analysing; }
  // follow the playhead page by page
  if (follow && s.play && s.ph >= 0 && track) {
    const x = xOf(s.ph);
    if (x > W - 40 || x < KEYW) { view.t0 = s.ph - 0.1 * (W - KEYW) / view.pps; clampView(); }
  }
  if (prevPh !== s.ph || s.cap === 2) draw();
}

// ── data from the plugin ────────────────────────────────────────────────────
let fittedFor = null;
window.NN = {
  onTrack(t, i) {
    track = t && t.midi ? t : null;
    derived.clear(); sel.clear();
    if (i) info = i;
    if (!track) { notes = []; fittedFor = null; }
    updateKeyUi(); renderEmpty(); draw(); updateInspector();
  },
  onNotes(list, i) {
    if (pendingPatches.size || drag) return;   // our own edit is in flight; next push will include it
    notes = list || [];
    if (i) { info = i; updateKeyUi(); }
    for (const id of [...sel]) if (!notes.some(n => n.id === id)) sel.delete(id);
    if (track && fittedFor !== track && notes.length) { fittedFor = track; fitView(); }
    renderEmpty(); draw(); updateInspector();
    $("takePill").innerHTML = track ? `Take <b>${fmtTime(info.seconds)}</b> · ${notes.length} notes` : "No take yet";
    $("quantBtn").disabled = !(track && info.bpm > 0);
  },
  onStatus(s) { applyStatus(s); },
};
function updateKeyUi() {
  for (const ks of keySels) ks.value = info.tonic + ":" + (info.minor ? 1 : 0);
  const unsure = track && !info.userKey && info.keyConf < 0.6;
  const txt = !track || info.userKey ? "" : unsure ? "CHECK KEY" : "DETECTED";
  for (const id of ["keyTag", "aiKeyTag"]) { $(id).textContent = txt; $(id).classList.toggle("warn", !!unsure); }
  if (info.srMismatch) toast("The session sample rate changed since this take was captured - recapture it");
}

// ── demo data for browser preview ───────────────────────────────────────────
const Mock = (() => {
  if (inPlugin) return {};
  let seed = 11; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  const hop = 240, sr = 48000, dec = 2, fps = sr / hop;
  const phrase = [[0.2,0.42,63.22,0],[0.7,0.25,64.86,0],[1.0,0.9,66.12,0.35],[2.0,0.22,63.9,0],[2.25,0.25,63.1,0],
    [2.6,1.2,61.18,0.45],[4.0,0.25,63.3,0],[4.3,0.25,64.15,0],[4.6,0.8,66.4,0.25],[5.5,0.22,68.25,0],
    [5.8,1.4,67.8,0.5],[7.4,0.25,66.2,0],[7.7,0.25,64.7,0],[8.0,0.4,63.35,0.1],[8.5,1.8,61.0,0.5]];
  const seconds = 10.5, frames = Math.floor(seconds * fps);
  const midi = new Array(Math.ceil(frames / dec)).fill(0), db = new Array(midi.length).fill(-80);
  const notesM = [];
  phrase.forEach(([s, len, c, vib], i) => {
    const a = Math.round(s * fps), b = Math.round((s + len) * fps), scoop = rnd() > .5 ? -.6 : .45, drift = (rnd() - .5) * .4;
    for (let f = a; f < b; f += dec) {
      const t = (f - a) / fps, u = (f - a) / (b - a);
      const v = c + Math.pow(Math.max(0, 1 - t / 0.06), 2) * scoop + drift * (u - .5) + vib * Math.sin(2 * Math.PI * 5.5 * t) * Math.min(1, t / .25);
      midi[Math.floor(f / dec)] = +v.toFixed(2);
      db[Math.floor(f / dec)] = Math.round(-14 - 10 * Math.pow(u, 2) - 4 * Math.sin(u * 3 + i) + Math.min(0, (t - 0.03) * 300));
    }
    notesM.push({ id: i + 1, s: a, e: b, c, p: c, dr: 0, vb: 1, tr: 0.5, fm: 0, g: 0, ts: 0 });
  });
  const st = { tr: { hop, sr, dec, frames, midi, db }, info: { tonic: 8, minor: true, keyConf: 0.62, seconds, bpm: 96, ppq: 64 }, undo: [], redo: [], orig: false, ph: 3.2 };
  const push = () => { st.undo.push(JSON.stringify(notesM)); st.redo = []; };
  const send = () => setTimeout(() => NN.onNotes(JSON.parse(JSON.stringify(notesM)), st.info), 0);
  const ids = l => l && l.length ? notesM.filter(n => l.includes(n.id)) : notesM;
  const scaleSnap = c => { const sc = new Set((st.info.minor ? MINOR : MAJOR).map(s => (s + st.info.tonic) % 12)); let b = Math.round(c), bd = 9; for (let k = Math.floor(c) - 6; k <= Math.ceil(c) + 6; k++) if (sc.has(((k % 12) + 12) % 12) && Math.abs(k - c) < bd) { bd = Math.abs(k - c); b = k; } return b; };
  setTimeout(() => { NN.onTrack(st.tr, st.info); NN.onNotes(JSON.parse(JSON.stringify(notesM)), st.info); }, 50);
  setInterval(() => { st.ph = (st.ph + 0.05) % seconds; NN.onStatus({ cap: 0, capSec: 0, play: true, ph: st.ph, busy: false, analysing: false, undo: st.undo.length > 0, redo: st.redo.length > 0, orig: st.orig, msg: "" }); }, 50);
  return {
    patch(list, u) { if (u) push(); for (const p of list) Object.assign(notesM.find(n => n.id === p.id), p); send(); },
    correct(l, c, d, mode) { push(); for (const n of ids(l)) { const t = mode === 1 ? Math.round(n.c) : scaleSnap(n.c); n.p = n.c + (t - n.c) * c; n.dr = d; } send(); },
    assist(style, l) { push(); let f = 0; for (const n of ids(l)) { const t = scaleSnap(n.c); n.p = n.c + (t - n.c) * (style ? 1 : .85); n.dr = [.2, .45, .7][style]; f++; } send(); return `Tuned ${f} notes · vibrato kept on 6`; },
    reset(l) { push(); for (const n of ids(l)) Object.assign(n, { p: n.c, dr: 0, vb: 1, tr: .5, fm: 0, g: 0, ts: 0 }); send(); },
    split(id, f) { push(); const i = notesM.findIndex(n => n.id === id), n = notesM[i]; if (f - n.s < 4 || n.e - f < 4) return; const r = { ...n, id: Math.max(...notesM.map(o => o.id)) + 1, s: f }; n.e = f; notesM.splice(i + 1, 0, r); send(); },
    undo() { if (!st.undo.length) return; st.redo.push(JSON.stringify(notesM)); const v = JSON.parse(st.undo.pop()); notesM.splice(0, notesM.length, ...v); send(); },
    redo() { if (!st.redo.length) return; st.undo.push(JSON.stringify(notesM)); const v = JSON.parse(st.redo.pop()); notesM.splice(0, notesM.length, ...v); send(); },
    setCompare(b) { st.orig = b; },
    setKey(t, m) { st.info = { ...st.info, tonic: t, minor: m, keyConf: 1 }; send(); },
    quantize() {}, capture() {},
  };
})();

if (inPlugin) native("ready");
resize(); renderEmpty(); updateInspector();
