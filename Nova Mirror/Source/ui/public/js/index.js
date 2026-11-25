// Nova Mirror editor. Talks to the plugin through JUCE native functions; in a normal
// browser it runs on animated demo data for design previews.
import { getNativeFunction } from "./juce/index.js";

const inPlugin = !window.__NM_PREVIEW__;
const cache = {};
function native(name, ...args) {
  if (!inPlugin) return Promise.resolve(Demo.call(name, ...args));
  cache[name] = cache[name] || getNativeFunction(name);
  return cache[name](...args);
}
const $ = id => document.getElementById(id);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const BANDS = 64, FMIN = 20, FMAX = 20000;
const bandHz = b => FMIN * Math.pow(FMAX / FMIN, b / (BANDS - 1));

// ── scaling: one design size, scaled to the window ─────────────────────────
const DW = 1100, DH = 700;
let S = 1;
function applyScale() { S = Math.max(0.3, window.innerWidth / DW); $("app").style.transform = `scale(${S})`; }

// ── state ──────────────────────────────────────────────────────────────────
let params = { cut: 50, boost: 50, link: 1, speed: 40, from: 20, to: 20000, output: 0, mode: 2, stereo: 0, punch: 1, peak: 1, freeze: 0, sections: 1, power: 1, listen: 0 };
let info = { kind: "nova", name: "Nova Balance", nova: [], user: [], loading: false, status: "", ab: 0, listenReady: false, ref: null };
let frame = null;

const speedMs = v => 20 * Math.pow(75, v / 100);
const fmtHz = v => v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + " kHz" : Math.round(v) + " Hz";
const KNOBS = {
  cut:    { min: 0, max: 100, def: 50, fmt: v => Math.round(v) + "%" },
  boost:  { min: 0, max: 100, def: 50, fmt: v => Math.round(v) + "%" },
  speed:  { min: 0, max: 100, def: 40, fmt: v => { const ms = speedMs(v); return ms >= 1000 ? (ms / 1000).toFixed(1) + " s" : Math.round(ms) + " ms"; } },
  from:   { min: 20, max: 20000, def: 20, log: true, fmt: fmtHz },
  to:     { min: 20, max: 20000, def: 20000, log: true, fmt: fmtHz },
  output: { min: -12, max: 12, def: 0, fmt: v => (v > 0 ? "+" : "") + v.toFixed(1) + " dB" },
};
const toNorm = (k, v) => k.log ? Math.log(v / k.min) / Math.log(k.max / k.min) : (v - k.min) / (k.max - k.min);
const fromNorm = (k, u) => k.log ? k.min * Math.pow(k.max / k.min, u) : k.min + u * (k.max - k.min);

function setParam(id, v) { params[id] = v; native("setParam", id, v); refreshControls(); }

// ── knobs ──────────────────────────────────────────────────────────────────
function drawKnob(el) {
  const id = el.dataset.p, k = KNOBS[id], v = params[id], u = clamp(toNorm(k, v), 0, 1), c = el.dataset.c, bi = el.dataset.bi;
  const deg = d => d * Math.PI / 180, pt = (a, r) => [27 + r * Math.sin(deg(a)), 27 - r * Math.cos(deg(a))];
  const arc = (a, b, r) => { const [x0, y0] = pt(a, r), [x1, y1] = pt(b, r); return `M${x0} ${y0} A${r} ${r} 0 ${Math.abs(b - a) > 180 ? 1 : 0} ${b > a ? 1 : 0} ${x1} ${y1}`; };
  const a1 = -135 + 270 * u, start = bi ? 0 : -135;
  const [px, py] = pt(a1, 14), [qx, qy] = pt(a1, 7);
  el.querySelector("svg").innerHTML = `<path d="${arc(-135, 135, 23)}" stroke="#23252d" stroke-width="2" fill="none" stroke-linecap="round"/>
    ${Math.abs(a1 - start) > 0.5 ? `<path d="${arc(start, a1, 23)}" stroke="${c}" stroke-width="2" fill="none" stroke-linecap="round"/>` : ""}
    <circle cx="27" cy="27" r="17" fill="#17181d" stroke="#2b2d36"/>
    <line x1="${qx}" y1="${qy}" x2="${px}" y2="${py}" stroke="#e6e7ec" stroke-width="1.6" stroke-linecap="round"/>`;
  el.querySelector(".v").textContent = k.fmt(v);
}
document.querySelectorAll(".knob").forEach(el => {
  const id = el.dataset.p, k = KNOBS[id];
  const svg = el.querySelector("svg");
  svg.addEventListener("pointerdown", e => {
    e.preventDefault(); svg.setPointerCapture(e.pointerId);
    const y0 = e.clientY, u0 = toNorm(k, params[id]);
    const other = params.link > 0.5 && (id === "cut" || id === "boost") ? (id === "cut" ? "boost" : "cut") : null;
    const o0 = other ? params[other] : 0;
    native("gesture", id, true); if (other) native("gesture", other, true);
    const move = ev => {
      const fine = ev.shiftKey ? 0.25 : 1;
      const u = clamp(u0 + (y0 - ev.clientY) / S / 160 * fine, 0, 1);
      const v = fromNorm(k, u);
      setParam(id, v);
      if (other) setParam(other, clamp(o0 + (v - fromNorm(k, u0)), 0, 100));
    };
    const up = () => { svg.removeEventListener("pointermove", move); svg.removeEventListener("pointerup", up); native("gesture", id, false); if (other) native("gesture", other, false); };
    svg.addEventListener("pointermove", move); svg.addEventListener("pointerup", up);
  });
  svg.addEventListener("dblclick", () => setParam(id, k.def));
});

// ── tabs, toggles, buttons ─────────────────────────────────────────────────
for (const grp of ["mode", "stereo"])
  $(grp).querySelectorAll("button").forEach(b => b.onclick = () => setParam(grp, +b.dataset.v));
document.querySelectorAll(".opt").forEach(o => o.onclick = () => setParam(o.dataset.p, params[o.dataset.p] > 0.5 ? 0 : 1));
$("link").onclick = () => setParam("link", params.link > 0.5 ? 0 : 1);
$("power").onclick = () => setParam("power", params.power > 0.5 ? 0 : 1);
$("listen").onclick = () => setParam("listen", params.listen > 0.5 ? 0 : 1);
$("sections").onclick = () => setParam("sections", params.sections > 0.5 ? 0 : 1);
$("abA").onclick = () => native("ab", 0);
$("abB").onclick = () => native("ab", 1);
$("clearRef").onclick = () => native("clearReference");
$("saveT").onclick = () => { $("saveBox").classList.add("show"); $("saveT").style.display = "none"; $("saveName").value = info.ref ? info.ref.name : ""; $("saveName").focus(); };
const closeSave = () => { $("saveBox").classList.remove("show"); $("saveT").style.display = ""; };
$("saveOk").onclick = async () => { const n = $("saveName").value.trim(); if (n) await native("saveTarget", n); closeSave(); };
$("saveName").addEventListener("keydown", e => { if (e.key === "Enter") $("saveOk").click(); if (e.key === "Escape") closeSave(); });

$("target").onchange = () => {
  const v = $("target").value;
  if (v === "load") { $("target").value = currentTargetValue(); native("chooseReference"); return; }
  const [kind, ...rest] = v.split(":");
  native("setTarget", kind, rest.join(":"));
};
const currentTargetValue = () => info.kind + ":" + (info.kind === "reference" ? "" : info.name);

// ── reference drop / choose ────────────────────────────────────────────────
const wave = $("wave");
wave.onclick = () => native("chooseReference");
wave.addEventListener("dragover", e => { e.preventDefault(); wave.classList.add("drag"); });
wave.addEventListener("dragleave", () => wave.classList.remove("drag"));
wave.addEventListener("drop", async e => {
  e.preventDefault(); wave.classList.remove("drag");
  const f = e.dataTransfer.files && e.dataTransfer.files[0];
  if (!f) return;
  $("waveEmpty").textContent = "Loading " + f.name + "…"; $("waveEmpty").style.display = "grid";
  const buf = new Uint8Array(await f.arrayBuffer());
  const CH = 384 * 1024, total = Math.max(1, Math.ceil(buf.length / CH));
  for (let i = 0; i < total; i++) {
    const part = buf.subarray(i * CH, Math.min(buf.length, (i + 1) * CH));
    let bin = ""; for (let k = 0; k < part.length; k += 8192) bin += String.fromCharCode.apply(null, part.subarray(k, k + 8192));
    await native("dropChunk", f.name, btoa(bin), i, total);
  }
});

// ── display ────────────────────────────────────────────────────────────────
const spec = $("spec"), sctx = spec.getContext("2d"), wc = $("waveC"), wctx = wc.getContext("2d");
let SW = 0, SH = 0, WW = 0, WH = 0, dpr = 1;
function resize() {
  dpr = window.devicePixelRatio || 1;
  SW = spec.clientWidth; SH = spec.clientHeight; spec.width = SW * dpr * S; spec.height = SH * dpr * S;
  WW = wc.clientWidth; WH = wc.clientHeight; wc.width = WW * dpr * S; wc.height = WH * dpr * S;
  drawWave();
}
const L = 44, Rm = 38, T = 26;
const xOfHz = (f, R, B) => L + (Math.log(f / FMIN) / Math.log(FMAX / FMIN)) * (R - L);
const hzOfX = (x, R) => FMIN * Math.pow(FMAX / FMIN, clamp((x - L) / (R - L), 0, 1));
let topDb = -20;   // display range follows the input level slowly
function drawSpec() {
  const R = SW - Rm, B = SH - 22;
  sctx.setTransform(dpr * S, 0, 0, dpr * S, 0, 0);
  sctx.clearRect(0, 0, SW, SH);
  const yOf = db => T + ((topDb - db) / 72) * (B - T);
  const gMid = T + (B - T) * 0.5, gScale = (B - T) / 2 / 12, yG = g => gMid - g * gScale;
  sctx.font = "10px ui-monospace, Menlo, monospace";
  for (const f of [30, 50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
    const x = xOfHz(f, R); sctx.fillStyle = f === 100 || f === 1000 || f === 10000 ? "#1b1d23" : "#15161b"; sctx.fillRect(x, T, 1, B - T);
    sctx.fillStyle = "#4b4e59"; sctx.textAlign = "center"; sctx.fillText(f >= 1000 ? f / 1000 + "k" : f, x, SH - 7);
  }
  sctx.textAlign = "right";
  for (let k = 1; k <= 5; k++) { const y = T + (B - T) * k / 6; sctx.fillStyle = "#15161b"; sctx.fillRect(L, y, R - L, 1); sctx.fillStyle = "#4b4e59"; sctx.fillText("-" + (12 * k), L - 8, y + 3); }
  sctx.textAlign = "left";
  for (const g of [12, 6, 0, -6, -12]) { const y = yG(g); if (g === 0) { sctx.fillStyle = "#2a2c35"; sctx.fillRect(L, y, R - L, 1); } sctx.fillStyle = "#4b4e59"; sctx.fillText((g > 0 ? "+" : "") + g, R + 8, y + 3); }

  const hasSig = frame && frame.in.some(v => v > -150);
  if (hasSig) {
    const mx = Math.max(...frame.in);
    topDb += ((mx + 6) - topDb) * 0.05;
    const line = (arr, toY) => { sctx.beginPath(); arr.forEach((v, b) => { const x = xOfHz(bandHz(b), R), y = toY(v); b ? sctx.lineTo(x, y) : sctx.moveTo(x, y); }); };
    // input
    line(frame.in, v => clamp(yOf(v), T, B)); sctx.lineTo(R, B); sctx.lineTo(L, B); sctx.closePath();
    const g = sctx.createLinearGradient(0, T, 0, B); g.addColorStop(0, "rgba(143,134,184,.22)"); g.addColorStop(1, "rgba(143,134,184,.02)");
    sctx.fillStyle = g; sctx.fill();
    line(frame.in, v => clamp(yOf(v), T, B)); sctx.strokeStyle = "rgba(143,134,184,.75)"; sctx.lineWidth = 1; sctx.stroke();
    // target
    if (frame.active) { line(frame.tgt, v => clamp(yOf(v), T, B)); sctx.strokeStyle = "rgba(233,228,214,.85)"; sctx.lineWidth = 1.25; sctx.stroke(); }
  }
  if (frame) {
    // gain curve
    const gains = frame.gain;
    for (const [sign, col] of [[-1, "rgba(224,114,124,.16)"], [1, "rgba(95,196,160,.16)"]]) {
      sctx.beginPath(); sctx.moveTo(L, gMid);
      gains.forEach((v, b) => sctx.lineTo(xOfHz(bandHz(b), R), yG(sign < 0 ? Math.min(0, v) : Math.max(0, v))));
      sctx.lineTo(R, gMid); sctx.closePath(); sctx.fillStyle = col; sctx.fill();
    }
    sctx.beginPath(); gains.forEach((v, b) => { const x = xOfHz(bandHz(b), R), y = yG(clamp(v, -12, 12)); b ? sctx.lineTo(x, y) : sctx.moveTo(x, y); });
    sctx.strokeStyle = params.power > 0.5 ? "#9c7dff" : "#4b4e59"; sctx.lineWidth = 1.6; sctx.stroke();
  }
  // FROM / TO
  const fx = xOfHz(params.from, R), tx = xOfHz(params.to, R);
  sctx.fillStyle = "rgba(12,13,16,.62)"; sctx.fillRect(L, T, Math.max(0, fx - L), B - T); sctx.fillRect(tx, T, Math.max(0, R - tx), B - T);
  sctx.fillStyle = hoverHandle === "from" || dragHandle === "from" ? "#9c7dff" : "#3a3d48"; sctx.fillRect(fx, T, 1, B - T);
  sctx.fillStyle = hoverHandle === "to" || dragHandle === "to" ? "#9c7dff" : "#3a3d48"; sctx.fillRect(tx, T, 1, B - T);
  sctx.fillStyle = "#9c7dff"; sctx.fillRect(fx - 3, T, 7, 2); sctx.fillRect(tx - 3, T, 7, 2);
  sctx.fillStyle = "#7d808c";
  if (params.from > 21) sctx.fillText(fmtHz(params.from), fx + 5, T + 12);
  if (params.to < 19900) { sctx.textAlign = "right"; sctx.fillText(fmtHz(params.to), tx - 5, T + 12); sctx.textAlign = "left"; }
}

// drag FROM/TO on the display
let dragHandle = null, hoverHandle = null;
const specPos = e => { const r = spec.getBoundingClientRect(); return (e.clientX - r.left) / S; };
spec.addEventListener("pointermove", e => {
  const R = SW - Rm, x = specPos(e);
  if (dragHandle) { const hz = hzOfX(x, R); setParam(dragHandle, dragHandle === "from" ? clamp(hz, 20, params.to / 1.1) : clamp(hz, params.from * 1.1, 20000)); return; }
  const fx = xOfHz(params.from, R), tx = xOfHz(params.to, R);
  hoverHandle = Math.abs(x - fx) < 6 ? "from" : Math.abs(x - tx) < 6 ? "to" : null;
  spec.style.cursor = hoverHandle ? "ew-resize" : "default";
});
spec.addEventListener("pointerdown", e => { if (hoverHandle) { dragHandle = hoverHandle; spec.setPointerCapture(e.pointerId); native("gesture", dragHandle, true); } });
spec.addEventListener("pointerup", () => { if (dragHandle) native("gesture", dragHandle, false); dragHandle = null; });

function drawWave() {
  wctx.setTransform(dpr * S, 0, 0, dpr * S, 0, 0);
  wctx.clearRect(0, 0, WW, WH);
  const ref = info.ref;
  if (!ref || !ref.peaks || !ref.peaks.length) return;
  const secs = ref.sections || [], dur = ref.seconds || 1;
  for (const s of secs) {
    const x0 = s.s / dur * WW, x1 = s.e / dur * WW;
    if (s.c === 3) { wctx.fillStyle = "rgba(156,125,255,.07)"; wctx.fillRect(x0, 0, x1 - x0, WH); }
    wctx.fillStyle = "#24262e"; wctx.fillRect(x0, 0, 1, WH);
  }
  const P = ref.peaks.length;
  for (let x = 0; x < WW; x += 1.5) {
    const i = Math.floor(x / WW * P), t = x / WW * dur;
    const sec = secs.find(s => t >= s.s && t < s.e);
    const amp = ref.peaks[i] * (WH / 2 - 8);
    wctx.fillStyle = sec && sec.c === 3 ? "rgba(190,178,240,.55)" : "rgba(140,143,158,.4)";
    wctx.fillRect(x, WH / 2 + 5 - amp, 1, Math.max(1, amp * 2 - 4));
  }
  if (params.listen > 0.5 && frame) { wctx.fillStyle = "#e6e7ec"; wctx.fillRect(frame.listenPos / dur * WW, 0, 1, WH); }
}

// ── refresh from plugin state ──────────────────────────────────────────────
function refreshControls() {
  document.querySelectorAll(".knob").forEach(drawKnob);
  for (const grp of ["mode", "stereo"]) $(grp).querySelectorAll("button").forEach(b => b.classList.toggle("on", +b.dataset.v === Math.round(params[grp])));
  document.querySelectorAll(".opt").forEach(o => o.classList.toggle("on", params[o.dataset.p] > 0.5));
  $("link").classList.toggle("on", params.link > 0.5);
  $("power").classList.toggle("on", params.power > 0.5);
  $("listen").classList.toggle("on", params.listen > 0.5);
  $("sections").classList.toggle("on", params.sections > 0.5);
}

function refreshInfo() {
  const sel = $("target"); sel.innerHTML = "";
  const add = (parent, value, text) => { const o = document.createElement("option"); o.value = value; o.textContent = text; parent.appendChild(o); };
  const g1 = document.createElement("optgroup"); g1.label = "Reference track";
  if (info.ref) add(g1, "reference:", info.ref.name);
  add(g1, "load", info.ref ? "Load another…" : "Load a reference…");
  sel.appendChild(g1);
  const g2 = document.createElement("optgroup"); g2.label = "Nova Targets";
  for (const n of info.nova || []) add(g2, "nova:" + n, n);
  sel.appendChild(g2);
  if ((info.user || []).length) {
    const g3 = document.createElement("optgroup"); g3.label = "My Targets";
    for (const n of info.user) add(g3, "user:" + n, n);
    sel.appendChild(g3);
  }
  sel.value = currentTargetValue();

  const ref = info.ref;
  $("waveEmpty").style.display = ref && !info.loading ? "none" : "grid";
  $("waveEmpty").textContent = info.loading ? (info.status || "Analysing…") : info.status || "Drop a reference track here, or click to choose one";
  $("refInfo").innerHTML = ref ? `${esc(ref.name)}<span class="num">${fmtDur(ref.seconds)}</span>` : "";
  $("listen").disabled = !(ref && info.listenReady && info.kind === "reference");
  $("saveT").disabled = !ref;
  $("clearRef").disabled = !ref;
  $("abA").classList.toggle("on", info.ab === 0); $("abB").classList.toggle("on", info.ab === 1);
  drawWave();
}
const esc = s => String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
const fmtDur = s => { const m = Math.floor(s / 60), r = Math.round(s - m * 60); return m + ":" + (r < 10 ? "0" : "") + r; };

window.NM = {
  onInfo(i) { info = i; refreshInfo(); },
  onParams(p) { params = { ...params, ...p }; refreshControls(); },
  onFrame(f) {
    frame = f;
    $("match").textContent = f.active ? Math.round(f.match) + "%" : "–";
    $("secn").innerHTML = f.active && params.sections > 0.5 && info.kind === "reference" && info.ref && info.ref.sections && info.ref.sections.length > 1
      ? `Section match&nbsp; <b>${f.section}</b>` : "";
    const silent = !f.in.some(v => v > -150);
    $("note").innerHTML = silent ? "Play your track to see Nova Mirror work" : "";
    drawSpec();
    if (params.listen > 0.5) drawWave();
  },
};

// ── demo (browser preview only) ────────────────────────────────────────────
const Demo = (() => {
  if (inPlugin) return { call() {} };
  let t = 0;
  const refShape = f => { const l = Math.log10(f); return -20 - 4.2 * (l - 2) * (l - 2) + 4 * Math.exp(-Math.pow((l - 1.85) / .18, 2)) - (f > 9000 ? Math.log10(f / 9000) * 16 : 0); };
  let peaks = []; let s = 7; const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
  const secs = [{ s: 0, e: 24, c: 1 }, { s: 24, e: 76, c: 2 }, { s: 76, e: 110, c: 3 }, { s: 110, e: 160, c: 2 }, { s: 160, e: 201, c: 3 }];
  for (let i = 0; i < 600; i++) { const tt = i / 600 * 201, sec = secs.find(q => tt >= q.s && tt < q.e); peaks.push((sec.c === 3 ? .8 : sec.c === 2 ? .5 : .3) + rnd() * .2); }
  setTimeout(() => NM.onInfo({ kind: "reference", name: "ozuna_reference", nova: ["Nova Balance", "Trap", "Drill", "R&B", "Reggaeton", "Afrobeats", "Pop", "House"], user: ["My Latin Hook"], loading: false, status: "", ab: 0, listenReady: true, ref: { name: "ozuna_reference", seconds: 201, peaks, sections: secs } }), 30);
  setTimeout(() => NM.onParams({ ...params, cut: 72, boost: 55, from: 180, to: 14000 }), 40);
  const gs = new Array(BANDS).fill(0);
  setInterval(() => {
    t += 0.033;
    const inn = [], tgt = [], gain = [];
    for (let b = 0; b < BANDS; b++) {
      const f = bandHz(b), l = Math.log10(f), r = refShape(f) + 40;
      const dev = 3.8 * Math.sin(l * 5.2 + t * 0.8) + 2.2 * Math.sin(l * 11 - t * 1.5) + 6 * Math.exp(-Math.pow((l - 2.45) / .09, 2)) * (0.6 + 0.4 * Math.sin(t * 2.6)) - 5 * Math.exp(-Math.pow((l - 3.9) / .15, 2));
      inn.push(r + dev); tgt.push(r);
      let d = -dev; d = d < 0 ? d * params.cut / 100 : d * params.boost / 100;
      if (f < params.from || f > params.to) d = 0;
      gs[b] += (clamp(d, -12, 12) - gs[b]) * 0.15; gain.push(gs[b]);
    }
    NM.onFrame({ in: inn, tgt, gain, match: 84 + 3 * Math.sin(t * .6), section: "Hook", active: true, listenPos: (t * 3) % 201, outPk: -3 });
  }, 33);
  return { call(name, ...a) { if (name === "setTarget") NM.onInfo({ ...info, kind: a[0], name: a[1] || info.name }); if (name === "ab") NM.onInfo({ ...info, ab: a[0] }); } };
})();

window.addEventListener("resize", () => { applyScale(); resize(); });
applyScale(); resize(); refreshControls(); refreshInfo();
if (inPlugin) native("ready");
