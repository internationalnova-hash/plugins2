// Nova Loop Lab UI. Talks to C++ through JUCE native functions; in a plain
// browser it runs on a mock so the UI can be previewed.
const inJuce = !!(window.__JUCE__ && window.__JUCE__.backend);
const native = {};
const NATIVE_NAMES = ["getState", "setParam", "setProgram", "loadSample", "dropSample", "generate", "setLock", "setChopFx",
                      "setAllChopFx", "setTempoMult", "auditionChop", "auditionLoop", "dragLoop", "dragChop", "activateLicense"];

const $ = (id) => document.getElementById(id);
const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
const pad2 = (n) => String(n).padStart(2, "0");

let S = { hasSample: false, chops: [], fx: [], steps: [], locked: [], peaks: [], presets: [], tempoMult: 1 };
let L = { playhead: -1, pattern: 0, busy: false, hostBpm: 0, hostPlaying: false, auditionLoop: false };
const P = { chop_mode: 0, sensitivity: 60, grid: 1, bars: 1, variation: 55, swing: 20, stutter: 15, pattern: 1, auto_play: 1, output: 0 };
let bypass = false, program = 0, sel = 0, modelVersion = -1, lastError = "";
const dragging = new Set();

function fit() {
  const s = Math.min(window.innerWidth / 1120, window.innerHeight / 700);
  $("plugin").style.transform = `translate(-50%, -50%) scale(${s})`;
}
window.addEventListener("resize", () => { fit(); drawAll(); });

function toast(msg, err = false) {
  const t = $("toast");
  t.textContent = msg;
  t.className = "toast on" + (err ? " err" : "");
  clearTimeout(toast.h);
  toast.h = setTimeout(() => (t.className = "toast"), err ? 5000 : 3500);
}

// ── Chop colours ─────────────────────────────────────────────────────────
const HUES = [40, 28, 210, 48, 18, 195, 34, 225, 55, 12, 185, 44, 240, 24, 200, 36];
const chopColor = (i, a = 1) => { const h = HUES[i % 16], cool = h > 150; return `hsla(${h}, ${cool ? 70 : 92}%, ${cool ? 66 : 60}%, ${a})`; };

// ── Canvas helpers ───────────────────────────────────────────────────────
function ctxFor(cv) {
  const r = cv.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
  const m = getComputedStyle($("plugin")).transform.match(/matrix\(([^,]+)/);
  const s = m ? parseFloat(m[1]) || 1 : 1;
  const w = Math.max(1, Math.round(r.width / s)), h = Math.max(1, Math.round(r.height / s));
  cv.width = w * dpr * s; cv.height = h * dpr * s;
  const g = cv.getContext("2d"); g.setTransform(dpr * s, 0, 0, dpr * s, 0, 0);
  return [g, w, h];
}
function drawPeaks(g, x0, w, h, from, to, color, mid = h / 2, scale = 0.9) {
  const pk = S.peaks, N = pk.length;
  if (!N || w <= 0) return;
  const a = Math.floor(from * N), b = Math.max(a + 1, Math.floor(to * N));
  g.fillStyle = color;
  for (let x = 0; x < w; x++) {
    const i0 = a + Math.floor((x / w) * (b - a)), i1 = Math.max(i0 + 1, a + Math.floor(((x + 1) / w) * (b - a)));
    let v = 0;
    for (let i = i0; i < i1 && i < N; i++) v = Math.max(v, pk[i]);
    const y = v * mid * scale;
    g.fillRect(x0 + x, mid - y, 1, Math.max(1, y * 2));
  }
}

function drawWave() {
  const [g, w, h] = ctxFor($("wave"));
  g.clearRect(0, 0, w, h);
  $("emptyMsg").style.display = S.hasSample ? "none" : "grid";
  if (!S.hasSample) return;
  const beats = Math.max(1, Math.round(S.sampleBars * 4));
  for (let i = 0; i <= beats; i++) {
    g.fillStyle = i % 4 === 0 ? "rgba(255,189,74,.16)" : "rgba(255,255,255,.04)";
    g.fillRect(Math.round(i / (S.sampleBars * 4) * w), 0, 1, h);
  }
  S.chops.forEach((c, i) => {
    const x0 = c.s * w, x1 = c.e * w, f = S.fx[i] || {};
    g.fillStyle = chopColor(i, i === sel ? 0.16 : 0.06);
    g.fillRect(x0, 0, x1 - x0, h);
    g.save(); g.beginPath(); g.rect(x0, 0, x1 - x0, h); g.clip();
    drawPeaks(g, 0, w, h, 0, 1, chopColor(i, f.use === false ? 0.22 : 0.8), h / 2, 0.72);
    g.restore();
    g.fillStyle = "#ffbd4a"; g.fillRect(Math.round(x0), 0, 1.5, h);
    g.beginPath(); g.moveTo(x0, 0); g.lineTo(x0 + 7, 0); g.lineTo(x0, 8); g.fill();
    g.fillStyle = "rgba(233,237,246,.75)"; g.font = "600 9.5px Avenir Next, sans-serif";
    if (x1 - x0 > 16) g.fillText(pad2(i + 1), x0 + 4, h - 6);
  });
}

// ── Pads ─────────────────────────────────────────────────────────────────
function buildPads() {
  const host = $("pads"); host.innerHTML = "";
  for (let i = 0; i < 16; i++) {
    const d = document.createElement("div");
    d.className = "pad";
    d.innerHTML = `<canvas></canvas><div class="n">${pad2(i + 1)}</div><div class="fx"></div>`;
    let x0 = 0, y0 = 0, down = false, started = false;
    d.addEventListener("pointerdown", (e) => { if (i >= S.chops.length) return; down = true; started = false; x0 = e.clientX; y0 = e.clientY; });
    d.addEventListener("pointermove", (e) => {
      if (!down || started || Math.hypot(e.clientX - x0, e.clientY - y0) < 6) return;
      started = true; down = false;
      native.dragChop(i).then(onExport);
    });
    d.addEventListener("pointerup", () => {
      if (!down) return;
      down = false;
      sel = i;
      native.auditionChop(i);
      d.classList.add("hit"); setTimeout(() => d.classList.remove("hit"), 160);
      renderPads(); syncEditor(); drawWave();
    });
    host.appendChild(d);
  }
}
function renderPads() {
  document.querySelectorAll(".pad").forEach((p, i) => {
    p.className = "pad" + (i >= S.chops.length ? " off" : "") + (i === sel && i < S.chops.length ? " sel" : "");
    const [g, w, h] = ctxFor(p.querySelector("canvas"));
    g.clearRect(0, 0, w, h);
    p.querySelector(".fx").textContent = "";
    if (i >= S.chops.length) return;
    const c = S.chops[i], f = S.fx[i] || {};
    g.save();
    if (f.reverse) { g.translate(w, 0); g.scale(-1, 1); }
    drawPeaks(g, 6, w - 12, h, c.s, c.e, chopColor(i, f.use === false ? 0.3 : 0.9), h / 2 + 4, 0.7);
    g.restore();
    const tags = [];
    if (f.pitch) tags.push((f.pitch > 0 ? "+" : "") + f.pitch + "st");
    if (f.reverse) tags.push("REV");
    if (f.stretch && f.stretch !== 100) tags.push(f.stretch + "%");
    if (f.use === false) tags.push("OUT");
    p.querySelector(".fx").textContent = tags.join(" ");
  });
}

// ── Patterns ─────────────────────────────────────────────────────────────
const curPattern = () => (L.hostPlaying || L.auditionLoop ? L.pattern : Math.round(P.pattern) - 1);
function drawPattern(g, w, h, steps, total, mini) {
  for (const [t, len, chop, stut] of steps || []) {
    const x0 = t / total * w, x1 = (t + len) / total * w, padX = mini ? 1 : 2;
    g.fillStyle = chopColor(chop, mini ? 0.75 : 0.22);
    g.fillRect(x0 + padX, mini ? h * 0.35 : 4, x1 - x0 - padX * 2, mini ? h * 0.5 : h - 8);
    if (mini) continue;
    const c = S.chops[chop];
    if (c) {
      g.save(); g.beginPath(); g.rect(x0 + padX, 4, x1 - x0 - padX * 2, h - 8); g.clip();
      if ((S.fx[chop] || {}).reverse) { g.translate(x0 + x1, 0); g.scale(-1, 1); }
      drawPeaks(g, x0 + padX, x1 - x0 - padX * 2, h, c.s, c.e, chopColor(chop, 0.85), h / 2, 0.6);
      g.restore();
    }
    if (stut) { g.fillStyle = "rgba(255,255,255,.5)"; for (let k = 1; k < 4; k++) g.fillRect(x0 + (x1 - x0) * k / 4, 6, 1, h - 12); }
    g.fillStyle = "rgba(233,237,246,.85)"; g.font = "600 10px Avenir Next, sans-serif";
    if (x1 - x0 > 20) g.fillText(pad2(chop + 1), x0 + 6, 18);
  }
}
function buildVars() {
  const host = $("vars"); host.innerHTML = "";
  for (let v = 0; v < 8; v++) {
    const d = document.createElement("div");
    d.className = "var";
    d.innerHTML = `<canvas></canvas><div class="vn">V${v + 1}</div><div class="lock">&#128274;</div>`;
    d.title = "Click to play this flip. Double-click to lock it so GENERATE keeps it.";
    d.addEventListener("click", () => { P.pattern = v + 1; native.setParam("pattern", v + 1, "set"); L.pattern = v; renderVars(); drawLane(); });
    d.addEventListener("dblclick", () => {
      const lock = !S.locked[v];
      S.locked[v] = lock;
      native.setLock(v, lock);
      renderVars();
      toast(lock ? `V${v + 1} locked: GENERATE will keep it` : `V${v + 1} unlocked`);
    });
    host.appendChild(d);
  }
}
function renderVars() {
  const cur = curPattern(), total = (S.loopBars || 2) * 16;
  document.querySelectorAll(".var").forEach((d, v) => {
    d.className = "var" + (v === cur ? " on" : "") + (S.locked[v] ? " locked" : "");
    const [g, w, h] = ctxFor(d.querySelector("canvas"));
    g.clearRect(0, 0, w, h);
    if (S.hasSample) drawPattern(g, w, h, S.steps[v], total, true);
  });
}
function drawLane() {
  const [g, w, h] = ctxFor($("lane"));
  g.clearRect(0, 0, w, h);
  const bars = S.loopBars || [1, 2, 4][Math.round(P.bars)] || 2, total = bars * 16;
  for (let i = 0; i <= total; i++) {
    g.fillStyle = i % 16 === 0 ? "rgba(255,189,74,.28)" : i % 4 === 0 ? "rgba(255,255,255,.08)" : "rgba(255,255,255,.025)";
    g.fillRect(Math.round(i / total * w), 0, 1, h);
  }
  g.fillStyle = "rgba(174,183,203,.55)"; g.font = "9px Avenir Next, sans-serif";
  for (let b = 0; b < bars; b++) g.fillText(`BAR ${b + 1}`, b / bars * w + 6, h - 8);
  if (!S.hasSample) {
    g.fillStyle = "rgba(111,122,146,.9)"; g.font = "11px Avenir Next, sans-serif"; g.textAlign = "center";
    g.fillText("LOAD A SAMPLE, THEN HIT GENERATE", w / 2, h / 2); g.textAlign = "start";
    return;
  }
  drawPattern(g, w, h, S.steps[curPattern()], total, false);
  if (L.playhead >= 0) {
    const x = L.playhead * w;
    g.fillStyle = "#ffe3a6"; g.shadowColor = "#ffbd4a"; g.shadowBlur = 8;
    g.fillRect(x, 0, 2, h); g.shadowBlur = 0;
  }
}

// ── Chop editor knobs ────────────────────────────────────────────────────
const NS = "http://www.w3.org/2000/svg";
const A0 = 135, SWEEP = 270;
const polar = (cx, cy, r, deg) => { const a = deg * Math.PI / 180; return [cx + r * Math.cos(a), cy + r * Math.sin(a)]; };
function arcPath(cx, cy, r, a0, a1) {
  const [x0, y0] = polar(cx, cy, r, a0), [x1, y1] = polar(cx, cy, r, a1);
  return `M${x0} ${y0} A${r} ${r} 0 ${Math.abs(a1 - a0) > 180 ? 1 : 0} ${a1 >= a0 ? 1 : 0} ${x1} ${y1}`;
}
function el(tag, attrs, parent) { const n = document.createElementNS(NS, tag); for (const k in attrs) n.setAttribute(k, attrs[k]); if (parent) parent.appendChild(n); return n; }
let gid = 0;
function buildKnob(host, size, centre) {
  const svg = el("svg", { viewBox: `0 0 ${size} ${size}` }, host);
  const c = size / 2, R = size * 0.43, w = size * 0.055, body = size * 0.33;
  const defs = el("defs", {}, svg), id = `k${gid++}`;
  const g = el("linearGradient", { id, x1: "0", y1: "1", x2: "1", y2: "0" }, defs);
  el("stop", { offset: "0%", "stop-color": "#ff8a16" }, g); el("stop", { offset: "100%", "stop-color": "#ffe0a0" }, g);
  const rg = el("radialGradient", { id: id + "b", cx: "50%", cy: "32%", r: "70%" }, defs);
  el("stop", { offset: "0%", "stop-color": "#2a344d" }, rg); el("stop", { offset: "60%", "stop-color": "#0d1424" }, rg); el("stop", { offset: "100%", "stop-color": "#04070f" }, rg);
  for (let i = 0; i <= 10; i++) {
    const a = A0 + SWEEP * i / 10;
    const [x0, y0] = polar(c, c, R + w * 1.4, a), [x1, y1] = polar(c, c, R + w * (i % 5 ? 2.2 : 2.9), a);
    el("line", { x1: x0, y1: y0, x2: x1, y2: y1, stroke: i % 5 ? "rgba(174,183,203,.28)" : "rgba(255,189,74,.65)", "stroke-width": 1.2 }, svg);
  }
  el("path", { d: arcPath(c, c, R, A0, A0 + SWEEP), stroke: "rgba(255,255,255,.07)", "stroke-width": w, fill: "none", "stroke-linecap": "round" }, svg);
  const fill = el("path", { stroke: `url(#${id})`, "stroke-width": w, fill: "none", "stroke-linecap": "round", style: `filter: drop-shadow(0 0 ${w}px rgba(255,170,56,.5))` }, svg);
  el("circle", { cx: c, cy: c, r: body, fill: `url(#${id}b)`, stroke: "rgba(255,176,62,.28)", "stroke-width": 1 }, svg);
  const ptr = el("line", { stroke: "#ffbd4a", "stroke-width": size * 0.03, "stroke-linecap": "round" }, svg);
  return (norm) => {
    norm = clamp(norm, 0, 1);
    const a = A0 + SWEEP * norm, from = centre == null ? A0 : A0 + SWEEP * centre;
    fill.setAttribute("d", Math.abs(a - from) > 0.5 ? arcPath(c, c, R, from, a) : "");
    const [x0, y0] = polar(c, c, body * 0.45, a), [x1, y1] = polar(c, c, body - 3, a);
    ptr.setAttribute("x1", x0); ptr.setAttribute("y1", y0); ptr.setAttribute("x2", x1); ptr.setAttribute("y2", y1);
  };
}
const KNOBS = [
  ["pitch", "PITCH", -12, 12, 1, 0.5, 0, (v) => (v > 0 ? "+" : "") + v + " st", "Repitch like a sampler (also changes the length)."],
  ["stretch", "STRETCH", 50, 200, 1, null, 100, (v) => v + "%", "Longer or shorter, same pitch."],
  ["filter", "FILTER", -100, 100, 1, 0.5, 0, (v) => v === 0 ? "OPEN" : v < 0 ? "LP " + -v : "HP " + v, "Left: low-pass. Right: high-pass."],
  ["tone", "LO-FI", 0, 100, 1, null, 0, (v) => v + "%", "Crunch, dusty top end, tape drive."],
];
const setters = {};
const curFx = () => S.fx[sel] || { pitch: 0, stretch: 100, filter: 0, tone: 0, reverse: false, use: true };
function buildKnobs() {
  const host = $("knobs");
  for (const [id, name, lo, hi, step, centre, def, fmt, tip] of KNOBS) {
    const kc = document.createElement("div"); kc.className = "kc"; kc.title = tip + " Double-click to reset.";
    kc.innerHTML = `<div class="knob" id="k_${id}"></div><div class="nm">${name}</div><div class="val" id="v_${id}"></div>`;
    host.appendChild(kc);
    const set = buildKnob($(`k_${id}`), 70, centre);
    setters[id] = () => { const v = curFx()[id]; set((v - lo) / (hi - lo)); $(`v_${id}`).textContent = fmt(v); };
    const k = $(`k_${id}`); let y0 = 0, v0 = 0, drag = false;
    const commit = () => { native.setChopFx(sel, { [id]: curFx()[id] }); };
    k.addEventListener("pointerdown", (e) => { if (!S.hasSample) return; drag = true; y0 = e.clientY; v0 = curFx()[id]; k.setPointerCapture(e.pointerId); });
    k.addEventListener("pointermove", (e) => {
      if (!drag) return;
      const v = Math.round(clamp(v0 + (y0 - e.clientY) * (hi - lo) / 200 * (e.shiftKey ? 0.25 : 1), lo, hi) / step) * step;
      if (S.fx[sel]) { S.fx[sel][id] = v; setters[id](); renderPads(); }
    });
    k.addEventListener("pointerup", () => { if (drag) { drag = false; commit(); } });
    k.addEventListener("dblclick", () => { if (S.fx[sel]) { S.fx[sel][id] = def; setters[id](); renderPads(); commit(); } });
    k.addEventListener("wheel", (e) => {
      e.preventDefault();
      if (!S.fx[sel]) return;
      S.fx[sel][id] = clamp(curFx()[id] + (e.deltaY < 0 ? step : -step) * (id === "pitch" ? 1 : 5), lo, hi);
      setters[id](); renderPads(); clearTimeout(k.t); k.t = setTimeout(commit, 250);
    }, { passive: false });
  }
}
function syncEditor() {
  $("selN").textContent = pad2(sel + 1);
  for (const id in setters) setters[id]();
  $("revBtn").classList.toggle("on", !!curFx().reverse);
  $("useBtn").classList.toggle("on", curFx().use !== false);
}

// ── Params ───────────────────────────────────────────────────────────────
const SLIDERS = [["sens", "sensitivity"], ["varAmt", "variation"], ["swing", "swing"], ["stut", "stutter"]];
function renderParams() {
  for (const [el_, id] of SLIDERS) {
    if (dragging.has(id)) continue;
    const i = $(el_);
    i.value = Math.round(P[id]);
    i.style.setProperty("--p", i.value + "%");
    $(el_ + "V").textContent = i.value + "%";
  }
  const mode = Math.round(P.chop_mode);
  document.querySelectorAll("#modeSeg button").forEach((b, i) => b.classList.toggle("on", i === mode));
  $("sensWrap").style.display = mode === 1 ? "none" : "";
  $("gridSeg").style.display = mode === 1 ? "" : "none";
  document.querySelectorAll("#gridSeg button").forEach((b, i) => b.classList.toggle("on", i === Math.round(P.grid)));
  document.querySelectorAll("#barsSeg button").forEach((b, i) => b.classList.toggle("on", i === Math.round(P.bars)));
  $("outV").textContent = (P.output > 0 ? "+" : "") + Number(P.output).toFixed(1);
  $("power").classList.toggle("off", bypass);
  $("plugin").classList.toggle("bypassed", bypass);
  const wanted = [4, 8, 16][Math.round(P.grid)];
  const note = $("gridNote");
  if (mode === 1 && S.hasSample && S.effectiveGridDiv && S.effectiveGridDiv !== wanted) {
    note.style.display = ""; note.innerHTML = `USING <b>1/${S.effectiveGridDiv}</b> &middot; 16 PAD MAX`;
  } else note.style.display = "none";
  $("styleName").textContent = S.presets[program] || "Custom";
  document.querySelectorAll("#plist div").forEach((d, i) => d.classList.toggle("on", i === program));
}
function renderSample() {
  if (S.hasSample) {
    $("sName").innerHTML = `${escapeHtml(S.sampleName)}<small>${Number(S.sampleBars).toFixed(1)} BARS</small>`;
    $("bpm").textContent = Number(S.sampleBpm).toFixed(1);
    $("keyTxt").textContent = S.key;
  } else {
    $("sName").textContent = "No sample loaded";
    $("bpm").textContent = "--"; $("keyTxt").textContent = "--";
  }
  $("chopCount").textContent = S.chops.length;
  $("genBtn").disabled = !S.hasSample;
  $("dragOut").classList.toggle("disabled", !S.hasSample);
  sel = Math.min(sel, Math.max(0, S.chops.length - 1));
}
const escapeHtml = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

function drawAll() { drawWave(); renderPads(); renderVars(); drawLane(); }
function renderAll() { renderSample(); renderParams(); syncEditor(); drawAll(); }

async function refreshState() {
  const st = await native.getState();
  if (!st) return;
  S = { ...S, chops: [], fx: [], steps: [], ...st };
  modelVersion = st.modelVersion;
  Object.assign(P, st.params);
  bypass = st.bypass; program = st.program;
  buildPresetList();
  renderAll();
}

function onExport(r) {
  if (!r) return;
  if (r.error) toast(r.error, true);
  else if (!r.dragged) toast(`Saved "${r.name}" to Music › Nova Loop Lab › Exports. It's open in Finder, so drag it from there.`);
}

// ── Live updates from C++ (30 Hz) ────────────────────────────────────────
window.novaLoopLabUpdate = (d) => {
  const prevPattern = curPattern(), prevHead = L.playhead;
  L = { ...L, ...d };
  for (const k in d.params || {}) if (!dragging.has(k)) P[k] = d.params[k];
  if (typeof d.bypass === "boolean") bypass = d.bypass;
  if (typeof d.program === "number") program = d.program;
  $("busy").classList.toggle("on", !!d.busy);
  $("syncTxt").textContent = d.hostBpm > 0 ? `HOST ${Number(d.hostBpm).toFixed(1)}`
                           : S.hasSample ? `SAMPLE ${Math.round(S.loopBpm)}` : "HOST --";
  $("playBtn").innerHTML = d.auditionLoop ? '<svg viewBox="0 0 24 24"><path d="M6 5h4v14H6zM14 5h4v14h-4z"/></svg>' : '<svg viewBox="0 0 24 24"><path d="M7 4l13 8-13 8z"/></svg>';
  if (d.error && d.error !== lastError) toast(d.error, true);
  lastError = d.error || "";
  if (d.modelVersion !== modelVersion) { modelVersion = d.modelVersion; refreshState(); return; }
  renderParams();
  if (curPattern() !== prevPattern) renderVars();
  if (curPattern() !== prevPattern || d.playhead !== prevHead) drawLane();
};

// ── Wiring ───────────────────────────────────────────────────────────────
function buildPresetList() {
  const list = $("plist"); list.innerHTML = "";
  S.presets.forEach((name, i) => {
    const d = document.createElement("div");
    d.textContent = name;
    d.addEventListener("click", (e) => { e.stopPropagation(); choose(i); list.classList.remove("open"); });
    list.appendChild(d);
  });
}
async function choose(i) {
  const n = S.presets.length;
  if (!n) return;
  program = (i + n) % n;
  const st = await native.setProgram(program);
  if (st) { Object.assign(P, st.params); }
  renderParams();
  toast(`${S.presets[program]}: hit GENERATE for fresh flips in this style`);
}

function wireSlider(elId, id) {
  const i = $(elId);
  i.addEventListener("pointerdown", () => { dragging.add(id); native.setParam(id, +i.value, "begin"); });
  i.addEventListener("input", () => {
    P[id] = +i.value; i.style.setProperty("--p", i.value + "%"); $(elId + "V").textContent = i.value + "%";
    native.setParam(id, +i.value, dragging.has(id) ? "move" : "set");
  });
  const end = () => { if (dragging.delete(id)) native.setParam(id, +i.value, "end"); };
  i.addEventListener("pointerup", end); i.addEventListener("change", end);
}
function wireSeg(segId, attr, id) {
  $(segId).addEventListener("click", (e) => {
    const b = e.target.closest("button"); if (!b) return;
    const idx = [...$(segId).querySelectorAll("button")].indexOf(b);
    P[id] = idx; native.setParam(id, idx, "set"); renderParams();
  });
}
function startDragOnMove(elm, fn) {
  let x0 = 0, y0 = 0, down = false;
  elm.addEventListener("pointerdown", (e) => { down = true; x0 = e.clientX; y0 = e.clientY; e.preventDefault(); });
  elm.addEventListener("pointermove", (e) => {
    if (!down || Math.hypot(e.clientX - x0, e.clientY - y0) < 5) return;
    down = false; fn();
  });
  elm.addEventListener("pointerup", () => { if (down) { down = false; fn(); } });
}

async function fileToBase64(file) {
  const buf = new Uint8Array(await file.arrayBuffer());
  let bin = "";
  for (let i = 0; i < buf.length; i += 0x8000) bin += String.fromCharCode.apply(null, buf.subarray(i, i + 0x8000));
  return btoa(bin);
}

function wire() {
  SLIDERS.forEach(([e, id]) => wireSlider(e, id));
  wireSeg("modeSeg", "m", "chop_mode");
  wireSeg("gridSeg", "g", "grid");
  wireSeg("barsSeg", "b", "bars");

  $("genBtn").addEventListener("click", () => native.generate());
  $("loadBtn").addEventListener("click", () => native.loadSample());
  $("tHalf").addEventListener("click", () => native.setTempoMult((S.tempoMult || 1) * 0.5));
  $("tDouble").addEventListener("click", () => native.setTempoMult((S.tempoMult || 1) * 2));
  $("revBtn").addEventListener("click", () => { if (!S.fx[sel]) return; S.fx[sel].reverse = !S.fx[sel].reverse; syncEditor(); renderPads(); native.setChopFx(sel, { reverse: S.fx[sel].reverse }); });
  $("useBtn").addEventListener("click", () => { if (!S.fx[sel]) return; S.fx[sel].use = !(S.fx[sel].use !== false); syncEditor(); renderPads(); native.setChopFx(sel, { use: S.fx[sel].use }); });
  $("allBtn").addEventListener("click", () => {
    if (!S.fx[sel]) return;
    const { pitch, stretch, filter, tone, reverse } = S.fx[sel];
    native.setAllChopFx({ pitch, stretch, filter, tone, reverse });
    toast(`Chop ${pad2(sel + 1)}'s settings copied to every chop`);
  });
  $("playBtn").addEventListener("click", () => {
    if (L.hostPlaying) { toast("Your DAW is playing: Loop Lab is already following it"); return; }
    native.auditionLoop(!L.auditionLoop);
  });
  startDragOnMove($("dragOut"), () => native.dragLoop(curPattern()).then(onExport));

  $("prev").addEventListener("click", () => choose(program - 1));
  $("next").addEventListener("click", () => choose(program + 1));
  $("pname").addEventListener("click", () => $("plist").classList.toggle("open"));
  document.addEventListener("click", (e) => { if (!$("pname").contains(e.target)) $("plist").classList.remove("open"); });
  $("power").addEventListener("click", () => { bypass = !bypass; native.setParam("bypass", bypass ? 1 : 0, "set"); renderParams(); });

  const oc = $("outChip"); let oy = 0, ov = 0, od = false;
  oc.addEventListener("pointerdown", (e) => { od = true; oy = e.clientY; ov = P.output; oc.setPointerCapture(e.pointerId); dragging.add("output"); native.setParam("output", P.output, "begin"); });
  oc.addEventListener("pointermove", (e) => { if (!od) return; P.output = Math.round(clamp(ov + (oy - e.clientY) * 0.1, -24, 6) * 10) / 10; native.setParam("output", P.output, "move"); renderParams(); });
  oc.addEventListener("pointerup", () => { if (od) { od = false; dragging.delete("output"); native.setParam("output", P.output, "end"); } });
  oc.addEventListener("dblclick", () => { P.output = 0; native.setParam("output", 0, "set"); renderParams(); });

  const ww = $("waveWrap");
  ["dragenter", "dragover"].forEach((t) => ww.addEventListener(t, (e) => { e.preventDefault(); ww.classList.add("dragover"); }));
  ww.addEventListener("dragleave", () => ww.classList.remove("dragover"));
  ww.addEventListener("drop", async (e) => {
    e.preventDefault(); ww.classList.remove("dragover");
    const f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
    if (!f) { toast("Drag an audio file from Finder, or click LOAD.", true); return; }
    if (f.size > 120 * 1024 * 1024) { toast("That file is too big. Try a loop under a minute.", true); return; }
    toast(`Loading ${f.name}…`);
    const r = await native.dropSample(await fileToBase64(f), f.name);
    if (r !== "ok") toast(r || "Couldn't load that file.", true);
  });
  ww.addEventListener("dblclick", () => { if (!S.hasSample) native.loadSample(); });

  $("licBtn").addEventListener("click", async () => {
    $("licMsg").textContent = "Checking…";
    const r = await native.activateLicense($("licKey").value);
    const msgs = { ok: "", invalid: "That key isn't valid.", revoked: "That key was revoked.", seats: "That key is already on its max number of computers.",
                   network: "Couldn't reach the license server. Check your connection.", empty: "Enter your key." };
    if (r === "ok") $("licenseOverlay").classList.remove("open");
    else $("licMsg").textContent = msgs[r] || "Activation failed.";
  });
}

async function main() {
  fit();
  if (inJuce) {
    const { getNativeFunction } = await import("./juce/index.js");
    for (const n of NATIVE_NAMES) native[n] = getNativeFunction(n);
  } else {
    Object.assign(native, createMock());
  }
  buildPads(); buildVars(); buildKnobs();
  wire();
  await refreshState();
  if (S.licenseRequired && !S.unlocked) $("licenseOverlay").classList.add("open");
}
main();

// ── Browser-preview mock (not used inside the plugin) ─────────────────────
function createMock() {
  const N = 2400;
  const peaks = [];
  let seed = 7;
  const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  const hits = [];
  for (let b = 0; b < 16; b++) { hits.push({ t: b / 16, a: b % 4 === 0 ? 1 : 0.55 + rnd() * 0.35, d: 0.02 + rnd() * 0.03 }); if (rnd() > 0.55) hits.push({ t: b / 16 + 1 / 32, a: 0.4, d: 0.015 }); }
  for (let i = 0; i < N; i++) {
    const t = i / N;
    let v = 0.18 + 0.08 * Math.sin(t * Math.PI * 8);
    for (const h of hits) if (t >= h.t) v += h.a * Math.exp(-(t - h.t) / h.d) * 0.8;
    peaks.push(Math.min(1, v * (0.85 + rnd() * 0.15)));
  }
  const presets = ["Boom Bap Flip", "Soul Chop", "Classic Order", "Drill Chop", "Trap Bounce", "Lo-Fi Flip", "Reverse Dreams", "Slowed Flip", "Stutter Edit", "Chaos Lab"];
  const st = { params: { ...P }, bypass: false, program: 0, locked: Array(8).fill(false), version: 1, tempoMult: 1, fx: [], seeds: [1, 2, 3, 4, 5, 6, 7, 8] };
  let audition = false, head = 0;
  const chopsFor = () => {
    const mode = Math.round(st.params.chop_mode);
    let pts = [0];
    if (mode === 1) { const n = 16; for (let i = 1; i < n; i++) pts.push(i / n); }
    else if (mode === 2) pts = [0, 0.25, 0.5, 0.75];
    else { const want = Math.round(4 + st.params.sensitivity / 100 * 12); pts = hits.slice().sort((a, b) => b.a - a.a).slice(0, want).map((h) => h.t).concat([0]); pts = [...new Set(pts)].sort((a, b) => a - b); }
    return pts.map((p, i) => ({ s: p, e: i + 1 < pts.length ? pts[i + 1] : 1 }));
  };
  const gen = (chops, s0) => {
    let s = s0 * 9301 + 49297; const r = () => ((s = (s * 9301 + 49297) % 233280) / 233280);
    const bars = [1, 2, 4][Math.round(st.params.bars)], out = []; let t = 0, last = -1;
    while (t < bars * 16) { const len = Math.min([1, 2, 2, 4, 4][Math.floor(r() * 5)], bars * 16 - t); const c = r() < 0.5 && last >= 0 ? (last + 1) % chops.length : Math.floor(r() * chops.length); out.push([t, len, c, r() < st.params.stutter / 200 ? 1 : 0]); last = c; t += len; }
    return out;
  };
  const state = () => {
    const chops = chopsFor();
    while (st.fx.length < 16) st.fx.push({ pitch: 0, stretch: 100, filter: 0, tone: 0, reverse: false, use: true });
    return { presets, program: st.program, params: { ...st.params }, bypass: st.bypass, modelVersion: st.version, busy: false, error: "", tempoMult: st.tempoMult,
             locked: st.locked, hasSample: true, sampleName: "soul_loop_Cmin.wav", sampleBpm: 92 * st.tempoMult, sampleBars: 4, key: "C MIN", loopBars: [1, 2, 4][Math.round(st.params.bars)],
             loopBpm: 92, effectiveGridDiv: 4, peaks, chops, fx: st.fx.slice(0, chops.length), steps: st.seeds.map((sd) => gen(chops, sd)), licenseRequired: false, unlocked: true };
  };
  setInterval(() => {
    if (audition) head = (head + 1 / 30 / (([1, 2, 4][Math.round(st.params.bars)] * 4) * 60 / 92)) % 1;
    window.novaLoopLabUpdate({ playhead: audition ? head : -1, pattern: Math.round(st.params.pattern) - 1, modelVersion: st.version, busy: false, hostBpm: 92, hostPlaying: false,
                               auditionLoop: audition, error: "", program: st.program, bypass: st.bypass, params: { ...st.params } });
  }, 1000 / 30);
  const bump = () => { st.version++; };
  return {
    getState: async () => state(),
    setParam: async (id, v) => { if (id === "bypass") st.bypass = !!v; else { st.params[id] = v; if (id !== "pattern" && id !== "output") bump(); } },
    setProgram: async (i) => { st.program = i; bump(); return state(); },
    loadSample: async () => true, dropSample: async () => "ok",
    generate: async () => { st.seeds = st.seeds.map((s, v) => (st.locked[v] ? s : Math.floor(Math.random() * 1e6))); bump(); },
    setLock: async (v, l) => { st.locked[v] = l; },
    setChopFx: async (i, f) => { Object.assign(st.fx[i], f); bump(); },
    setAllChopFx: async (f) => { st.fx.forEach((x) => Object.assign(x, f)); bump(); },
    setTempoMult: async (m) => { st.tempoMult = m; bump(); },
    auditionChop: async () => {}, auditionLoop: async (on) => { audition = on; },
    dragLoop: async () => ({ dragged: false, name: "soul_loop_Cmin flip V1 92bpm 3fa2c.wav" }), dragChop: async () => ({ dragged: false, name: "soul_loop_Cmin chop 03 a1b2c.wav" }),
    activateLicense: async () => "ok",
  };
}
