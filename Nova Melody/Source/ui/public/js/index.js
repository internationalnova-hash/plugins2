// Nova Melody editor. Talks to the plugin through JUCE native functions; in a normal
// browser it runs on demo data for design previews.
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

const GENRES = ["Trap", "Drill", "R&B", "Latin Urban", "Reggaeton", "Afrobeats", "Pop", "House", "Lo-fi", "Gospel"];
const VIBES = ["Dark", "Emotional", "Dreamy", "Bright", "Hype", "Sexy"];
const PARTS = ["Lead melody", "Bassline", "808", "Counter-melody", "Arp", "Hook (call & response)"];
const REGS = ["Low register", "Mid register", "High register"];
const BARS = ["2 bars", "4 bars", "8 bars", "16 bars"];
const SCALES = ["Major", "Minor", "Harmonic minor", "Dorian", "Phrygian", "Mixolydian", "Pentatonic minor", "Pentatonic major", "Blues"];
const NOTES = ["C", "C♯", "D", "E♭", "E", "F", "F♯", "G", "A♭", "A", "B♭", "B"];
const BLACK = [1, 3, 6, 8, 10];
const STEPS = [[0,2,4,5,7,9,11],[0,2,3,5,7,8,10],[0,2,3,5,7,8,11],[0,2,3,5,7,9,10],[0,1,3,5,7,8,10],[0,2,4,5,7,9,10],[0,3,5,7,10],[0,2,4,7,9],[0,3,5,6,7,10]];
const HUMAS = ["Bassline", "808", "Melody", "Chords under it"];

// ── scaling: one design size, scaled to the window ─────────────────────────
const DW = 1240;
let S = 1;
function applyScale() { S = Math.max(0.3, window.innerWidth / DW); $("app").style.transform = `scale(${S})`; drawAll(); }

// ── state ──────────────────────────────────────────────────────────────────
let params = { genre: 3, vibe: 1, part: 0, register: 1, bars: 2, density: 55, rhythm: 70, swing: 20, repetition: 60, followChords: 1, stayInKey: 1,
  leaveSpace: 0, keyTonic: 7, keyScale: 1, keyAutoGen: 1, keyAutoHum: 1, humAs: 0, pitchSnap: 0, timeSnap: 0, minBlip: 40, octave: 0, autoOctave: 1, sound: 0, midiOut: 1, volume: -6 };
let state = null;
let frame = { beat: -1, rec: false, recSec: 0, level: 0, bpm: 120 };

function setParam(id, v) { params[id] = v; native("setParam", id, v); renderControls(); }
function act(what, arg) { return native("act", what, arg === undefined ? 0 : arg); }

// ── controls ───────────────────────────────────────────────────────────────
function chips(el, names, pid) {
  el.innerHTML = names.map((n, i) => `<span class="chip" data-i="${i}">${n}</span>`).join("");
  el.querySelectorAll(".chip").forEach(c => c.onclick = () => setParam(pid, +c.dataset.i));
}
function options(sel, names, values) { sel.innerHTML = names.map((n, i) => `<option value="${values ? values[i] : i}">${n}</option>`).join(""); }

chips($("genres"), GENRES, "genre");
chips($("vibes"), VIBES, "vibe");
chips($("humAs"), HUMAS, "humAs");
options($("part"), PARTS); $("part").onchange = e => setParam("part", +e.target.value);
options($("register"), REGS); $("register").onchange = e => setParam("register", +e.target.value);
options($("bars"), BARS); $("bars").onchange = e => setParam("bars", +e.target.value);
options($("octave"), ["−2 octaves", "−1 octave", "as is", "+1 octave", "+2 octaves"], [-2, -1, 0, 1, 2]); $("octave").onchange = e => setParam("octave", +e.target.value);
$("autoOct").onclick = () => setParam("autoOctave", params.autoOctave > 0.5 ? 0 : 1);
document.querySelectorAll(".toggle").forEach(t => t.onclick = () => setParam(t.dataset.p, params[t.dataset.p] > 0.5 ? 0 : 1));

const RANGES = { density: [0, 100], rhythm: [0, 100], swing: [0, 100], repetition: [0, 100], pitchSnap: [0, 100], timeSnap: [0, 100], minBlip: [10, 200] };
function sliderText(id, v) {
  if (id === "pitchSnap" || id === "timeSnap") return v < 1 ? "Exact" : (v > 99 ? "Clean" : Math.round(v) + "%");
  if (id === "minBlip") return Math.round(v) + " ms";
  return Math.round(v) + "%";
}
document.querySelectorAll(".bar").forEach(b => {
  const id = b.dataset.p, [lo, hi] = RANGES[id];
  const setFrom = e => {
    const r = b.getBoundingClientRect();
    setParam(id, Math.round(lo + clamp((e.clientX - r.left) / r.width, 0, 1) * (hi - lo)));
  };
  b.onpointerdown = e => {
    b.setPointerCapture(e.pointerId); native("gesture", id, true); setFrom(e);
    b.onpointermove = setFrom;
    b.onpointerup = () => { b.onpointermove = null; native("gesture", id, false); };
  };
  b.ondblclick = () => setParam(id, id === "minBlip" ? 40 : ({ density: 55, rhythm: 70, swing: 20, repetition: 60 }[id] ?? 0));
});

// key pickers (Generate and Hum it share the picked key; each has its own Auto)
document.querySelectorAll(".keypick").forEach(kp => {
  const kind = kp.dataset.kind, autoId = kind === "gen" ? "keyAutoGen" : "keyAutoHum";
  const sel = kp.querySelector(".scale");
  options(sel, SCALES);
  sel.onchange = e => { setParam("keyScale", +e.target.value); setParam(autoId, 0); };
  kp.querySelector(".auto").onclick = () => setParam(autoId, params[autoId] > 0.5 ? 0 : 1);
});
function keyInUse(kind) {
  const autoOn = params[kind === "gen" ? "keyAutoGen" : "keyAutoHum"] > 0.5;
  const k = kind === "gen" ? state?.genKey : state?.hum?.key;
  if (autoOn && k && k.heard) return { tonic: k.tonic, scale: k.scale, how: kind === "gen" ? "heard from your chords" : (k.unsure ? "heard — not sure, pick one" : "heard from your humming"), unsure: !!k.unsure };
  return { tonic: params.keyTonic, scale: params.keyScale, how: autoOn ? (kind === "gen" ? "auto: play chords in to set it" : "auto: hum to set it") : "you picked it", unsure: false };
}
function renderKeys() {
  document.querySelectorAll(".keypick").forEach(kp => {
    const kind = kp.dataset.kind, autoOn = params[kind === "gen" ? "keyAutoGen" : "keyAutoHum"] > 0.5;
    const k = keyInUse(kind), steps = STEPS[k.scale] || STEPS[1];
    kp.querySelector(".keyrow").innerHTML = NOTES.map((n, i) => {
      const inScale = steps.includes((i - k.tonic + 12) % 12);
      return `<div class="k ${BLACK.includes(i) ? "b" : "w"} ${i === k.tonic ? "on" : ""} ${inScale && i !== k.tonic ? "in" : ""}" data-i="${i}">${n}</div>`;
    }).join("");
    kp.querySelectorAll(".k").forEach(el => el.onclick = () => { setParam("keyTonic", +el.dataset.i); setParam(kind === "gen" ? "keyAutoGen" : "keyAutoHum", 0); });
    kp.querySelector(".scale").value = String(k.scale);
    kp.querySelector(".auto").classList.toggle("on", autoOn);
    kp.querySelector(".keyhint").textContent = k.how;
  });
}

function renderControls() {
  const mode = state ? state.mode : 0;
  document.querySelectorAll(".tab").forEach(t => t.classList.toggle("on", +t.dataset.mode === mode));
  $("sideGen").hidden = mode !== 0; $("varsGen").hidden = mode !== 0; $("rollGen").hidden = mode !== 0;
  $("sideHum").hidden = mode !== 1; $("humWrap").hidden = mode !== 1;
  const mark = (el, idx) => el.querySelectorAll(".chip").forEach(c => c.classList.toggle("on", +c.dataset.i === idx));
  mark($("genres"), Math.round(params.genre)); mark($("vibes"), Math.round(params.vibe)); mark($("humAs"), Math.round(params.humAs));
  $("part").value = String(Math.round(params.part)); $("register").value = String(Math.round(params.register));
  $("bars").value = String(Math.round(params.bars)); $("octave").value = String(Math.round(params.octave));
  $("autoOct").classList.toggle("on", params.autoOctave > 0.5);
  document.querySelectorAll(".toggle").forEach(t => t.querySelector(".sw").classList.toggle("off", !(params[t.dataset.p] > 0.5)));
  document.querySelectorAll(".bar").forEach(b => {
    const id = b.dataset.p, [lo, hi] = RANGES[id], v = params[id];
    b.querySelector("i").style.width = (100 * (v - lo) / (hi - lo)) + "%";
    b.nextElementSibling.textContent = sliderText(id, v);
  });
  renderKeys();
  const k = keyInUse(mode === 0 ? "gen" : "hum");
  $("pKey").innerHTML = `Key <b>${NOTES[k.tonic]} ${SCALES[k.scale].toLowerCase()}</b> · ${k.how}`;
  $("pKey").classList.toggle("warn", k.unsure);
  $("pLen").textContent = mode === 0 ? BARS[Math.round(params.bars)] : (state?.hum?.ok ? Math.round(state.hum.len / 4) + " bars" : "—");
}

// ── tabs and buttons ───────────────────────────────────────────────────────
document.querySelectorAll(".tab").forEach(t => t.onclick = () => act("mode", +t.dataset.mode));
$("gen").onclick = () => act("generate");
$("redo").onclick = () => act("redo");
$("unlock").onclick = () => act("clearLocks");
document.querySelectorAll(".var").forEach(v => v.onclick = () => act("select", +v.dataset.v));
$("groove").onclick = () => act("humExtra", 1);
$("answer").onclick = () => act("humExtra", 2);
$("pick").onclick = () => act("chooseFile");
$("recBtn").onclick = () => frame.rec ? act("stop") : act("record", $("recSrc").value === "1");

// drop a voice memo anywhere on the Hum it page
const take = $("take");
window.addEventListener("dragover", e => { e.preventDefault(); if (state?.mode === 1) take.classList.add("drop"); });
window.addEventListener("dragleave", e => { if (e.target === document.documentElement || e.clientX <= 0) take.classList.remove("drop"); });
window.addEventListener("drop", e => {
  e.preventDefault(); take.classList.remove("drop");
  const f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
  if (!f) return;
  if (state?.mode !== 1) act("mode", 1);
  const rd = new FileReader();
  rd.onload = () => {
    const bytes = new Uint8Array(rd.result), CH = 384 * 1024, total = Math.max(1, Math.ceil(bytes.length / CH));
    for (let i = 0; i < total; i++) {
      let s = ""; const part = bytes.subarray(i * CH, (i + 1) * CH);
      for (let j = 0; j < part.length; j += 0x8000) s += String.fromCharCode.apply(null, part.subarray(j, j + 0x8000));
      native("dropChunk", f.name, btoa(s), i, total);
    }
  };
  rd.readAsArrayBuffer(f);
});

// ── drawing ────────────────────────────────────────────────────────────────
function fitCanvas(c) {
  const r = c.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
  const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
  if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
  const g = c.getContext("2d");
  g.setTransform(w / (r.width / S), 0, 0, h / (r.height / S), 0, 0);    // draw in design pixels
  return { g, W: r.width / S, H: r.height / S };
}
const median = a => { if (!a.length) return 60; const s = [...a].sort((x, y) => x - y); return s[s.length >> 1]; };

// a piano roll: notes = [start, len, pitch, vel, locked, glide]
function drawRoll(c, notes, len, opt = {}) {
  const { g, W, H } = fitCanvas(c);
  g.clearRect(0, 0, W, H);
  const KEYW = 40, TOP = 22, curve = opt.curve || [];
  let ps = notes.map(n => n[2]).concat(curve.map(p => p[1] + (opt.curveShift || 0)));
  let lo = ps.length ? Math.min(...ps) - 3 : 55, hi = ps.length ? Math.max(...ps) + 3 : 79;
  if (hi - lo < 24) { const m = (hi + lo) / 2; lo = Math.round(m - 12); hi = lo + 24; }
  const rows = hi - lo + 1, rh = (H - TOP) / rows, gw = W - KEYW, beatW = gw / Math.max(1, len);
  // rows + keys
  for (let p = lo; p <= hi; p++) {
    const y = TOP + (hi - p) * rh, black = BLACK.includes(((p % 12) + 12) % 12);
    g.fillStyle = black ? "rgba(0,0,0,.2)" : "rgba(0,0,0,0)"; g.fillRect(KEYW, y, gw, rh);
    g.fillStyle = black ? "#0b0c12" : "#161925"; g.fillRect(0, y, KEYW, rh);
    g.fillStyle = "#1b1e28"; g.fillRect(0, y + rh - 1, W, 1);
    if (p % 12 === 0 && rh > 7) { g.fillStyle = "#55596b"; g.font = "9px -apple-system"; g.fillText("C" + (Math.floor(p / 12) - 1), 4, y + rh - 3); }
  }
  // beats / bars
  g.font = "10px -apple-system";
  for (let b = 0; b <= len; b++) {
    const x = KEYW + b * beatW;
    g.fillStyle = b % 4 === 0 ? "#2b2f3e" : "#1d2029"; g.fillRect(x, TOP, 1, H - TOP);
    if (b % 4 === 0 && b < len) { g.fillStyle = "#5b5f72"; g.fillText(String(b / 4 + 1), x + 5, 14); }
  }
  g.fillStyle = "#232634"; g.fillRect(0, TOP - 1, W, 1); g.fillRect(KEYW - 1, TOP, 1, H - TOP);
  // the hummed line
  if (curve.length) {
    g.strokeStyle = "rgba(63,209,181,.85)"; g.lineWidth = 2; g.beginPath();
    let prev = -1;
    curve.forEach(([b, p]) => {
      const x = KEYW + b * beatW, y = TOP + (hi - (p + (opt.curveShift || 0)) + 0.5) * rh;
      if (prev < 0 || b - prev > 0.15) g.moveTo(x, y); else g.lineTo(x, y);
      prev = b;
    });
    g.stroke();
  }
  // notes
  const hits = [];
  notes.forEach((n, i) => {
    const x = KEYW + n[0] * beatW + 1, w = Math.max(3, n[1] * beatW - 2), y = TOP + (hi - n[2]) * rh + 1, h = Math.max(3, rh - 2);
    const grad = g.createLinearGradient(0, y, 0, y + h);
    const [c1, c2] = n[4] ? ["#ffd08a", "#f0a030"] : (opt.color || ["#b98cff", "#8a4cf0"]);
    grad.addColorStop(0, c1); grad.addColorStop(1, c2);
    g.fillStyle = grad;
    roundRect(g, x, y, w, h, 3); g.fill();
    if (n[5]) { g.strokeStyle = "rgba(255,255,255,.6)"; g.beginPath(); g.moveTo(x - 6, y + h); g.lineTo(x, y + h / 2); g.stroke(); }
    hits.push({ x, y, w, h, i });
  });
  // playhead
  if (frame.beat >= 0 && frame.beat <= len) {
    const x = KEYW + frame.beat * beatW;
    g.fillStyle = "#fff"; g.shadowColor = "rgba(255,255,255,.6)"; g.shadowBlur = 8; g.fillRect(x, TOP, 2, H - TOP); g.shadowBlur = 0;
  }
  c._hits = hits;
}
function roundRect(g, x, y, w, h, r) { g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }

function drawThumbs() {
  document.querySelectorAll(".var").forEach(v => {
    const i = +v.dataset.v, notes = state?.vars?.[i] || [], len = (state?.bars || 8) * 4;
    v.classList.toggle("on", state && state.selected === i);
    if (!notes.length) { v.querySelector("svg").innerHTML = ""; return; }
    const ps = notes.map(n => n[2]), lo = Math.min(...ps), hi = Math.max(lo + 8, Math.max(...ps));
    v.querySelector("svg").innerHTML = notes.map(n => `<rect x="${n[0] / len * 104}" y="${(hi - n[2]) / (hi - lo + 1) * 23}" width="${Math.max(2, n[1] / len * 104 - 1)}" height="2.5" rx="1" fill="${state.selected === i ? "#b98cff" : "#6b6f85"}"/>`).join("");
  });
}

function drawTake() {
  const { g, W, H } = fitCanvas($("cTake"));
  g.clearRect(0, 0, W, H);
  const h = state?.hum;
  if (frame.rec) {
    // live: a level meter and the seconds so far
    const lv = Math.min(1, frame.level * 3);
    g.fillStyle = "#1d2a28"; g.fillRect(14, H / 2 - 4, W - 28, 8);
    g.fillStyle = "#3fd1b5"; g.fillRect(14, H / 2 - 4, (W - 28) * lv, 8);
    return;
  }
  if (!h || !h.ok || !h.curve.length) return;
  const len = Math.max(4, h.len), ps = h.curve.map(p => p[1]), lo = Math.min(...ps) - 2, hi = Math.max(...ps) + 2;
  const X = b => 14 + b / len * (W - 28), Y = p => 34 + (hi - p) / Math.max(1, hi - lo) * (H - 58);
  g.fillStyle = "rgba(63,209,181,.12)";
  (h.heard || []).forEach(n => g.fillRect(X(n[0]), Y(n[2]) - 5, Math.max(2, X(n[0] + n[1]) - X(n[0])), 10));
  g.strokeStyle = "#3fd1b5"; g.lineWidth = 2; g.beginPath();
  let prev = -1;
  h.curve.forEach(([b, p]) => { if (prev < 0 || b - prev > 0.15) g.moveTo(X(b), Y(p)); else g.lineTo(X(b), Y(p)); prev = b; });
  g.stroke();
  if (frame.beat >= 0) { g.fillStyle = "#fff"; g.fillRect(X(frame.beat), 28, 2, H - 46); }
}

function drawAll() {
  if (!state) return;
  if (state.mode === 0) {
    const notes = state.vars?.[state.selected] || [];
    drawRoll($("cGen"), notes, (state.bars || 8) * 4, { color: Math.round(params.part) === 1 || Math.round(params.part) === 2 ? ["#ff9a9a", "#e05555"] : null });
    drawThumbs();
  } else {
    const h = state.hum || {};
    const notes = h.notes || [], as = Math.round(params.humAs);
    // line the hummed curve up with the notes (they may sit an octave or two down)
    let shift = 0;
    if (h.heard && h.heard.length && notes.length && as !== 3) shift = Math.round((median(notes.map(n => n[2])) - median(h.heard.map(n => n[2]))) / 12) * 12;
    const color = as === 2 ? ["#b98cff", "#8a4cf0"] : (as === 3 ? ["#6fe3cc", "#2fae97"] : ["#ff9a9a", "#e05555"]);
    $("humColor").style.background = color[1];
    $("humWhat").textContent = (["bassline", "808", "melody", "chords"][as]) + " from your humming";
    drawRoll($("cHum"), notes, Math.max(4, h.len || 16), { curve: h.ok ? h.curve : [], curveShift: shift, color });
    $("humEmpty").hidden = !!(h.ok && notes.length) || frame.rec;
    drawTake();
  }
}

$("cGen").addEventListener("click", e => {
  const c = $("cGen"), r = c.getBoundingClientRect(), x = (e.clientX - r.left) / S, y = (e.clientY - r.top) / S;
  const hit = (c._hits || []).find(h => x >= h.x - 2 && x <= h.x + h.w + 2 && y >= h.y - 2 && y <= h.y + h.h + 2);
  if (hit) act("lock", hit.i);
});

function renderHumText() {
  const h = state?.hum || {};
  $("humStatus").textContent = frame.rec ? `Recording… ${frame.recSec.toFixed(1)} s — press ■ to stop` : (state?.analysing ? "Listening to your take…" : (h.status || ""));
  $("takeInfo").textContent = h.ok ? `${h.heard.length} notes · ${Math.round(h.len / 4)} bars · ${h.key.name}` : "";
  $("recBtn").classList.toggle("on", frame.rec);
  $("recBtn").title = frame.rec ? "Stop" : "Record";
  $("groove").classList.toggle("on", h.extra === 1);
  $("answer").classList.toggle("on", h.extra === 2);
  $("groove").disabled = $("answer").disabled = !h.ok;
}

// ── from the plugin ────────────────────────────────────────────────────────
window.NMel = {
  onState(s) { state = s; renderControls(); renderHumText(); drawAll(); },
  onParams(p) { Object.assign(params, p); renderControls(); drawAll(); },
  onFrame(f) {
    const wasRec = frame.rec;
    frame = f;
    $("pTempo").textContent = Math.round(f.bpm) + " BPM";
    if (frame.rec || wasRec) renderHumText();
    drawAll();
  },
};

// ── design preview in a normal browser ─────────────────────────────────────
const Demo = {
  call(name, ...a) {
    if (name === "setParam") { params[a[0]] = a[1]; }
    if (name === "act") {
      if (a[0] === "mode") { state.mode = a[1]; }
      if (a[0] === "select") state.selected = a[1];
      if (a[0] === "lock") { const n = state.vars[state.selected][a[1]]; n[4] = n[4] ? 0 : 1; }
      if (a[0] === "generate") state.vars = [0, 1, 2, 3].map(i => Demo.mel(i + Math.random() * 9));
      NMel.onState(state);
    }
  },
  mel(seed) {
    const out = []; let p = 67, t = 0;
    while (t < 32) { const l = [0.5, 0.5, 1, 0.25, 1.5][Math.floor((Math.sin(t * 7 + seed) + 1) * 2.4)]; p = clamp(p + [-2, -1, 0, 1, 2, 3][Math.floor((Math.cos(t * 5 + seed) + 1) * 2.9)], 60, 79); out.push([t, l * 0.85, [55, 57, 58, 60, 62, 63, 65, 67, 69, 70, 72, 74, 75, 77, 79].reduce((a, b) => Math.abs(b - p) < Math.abs(a - p) ? b : a), 96, 0, 0]); t += l; }
    return out;
  },
};
if (!inPlugin) {
  const hum = [[0,1,55],[1,.5,55],[1.5,1.5,58],[3,1,53],[4,1.5,55],[5.5,.5,58],[6,2,50],[8,1,55],[9,.5,55],[9.5,1.5,58],[11,1,60],[12,1.5,58],[13.5,.5,55],[14,2,53]];
  const curve = []; hum.forEach(([s, l, p]) => { for (let t = 0; t <= 1; t += .06) curve.push([s + l * t * .95, p + Math.sin((s + t) * 9) * .2 + (t < .1 ? -.5 : 0)]); });
  state = { mode: 0, selected: 0, bars: 8, vars: [0, 1, 2, 3].map(i => Demo.mel(i)), genKey: { tonic: 7, scale: 1, name: "G minor", heard: true, auto: true },
            hum: { ok: true, status: "Heard 14 notes", len: 16, heard: hum.map(([s, l, p]) => [s, l, p, 100, 0, 0]), notes: hum.map(([s, l, p]) => [s, l * .95, p - 24, 108, 0, 0]),
                   curve, extra: 0, key: { tonic: 7, scale: 1, name: "G minor", heard: true, unsure: false } } };
  state.vars[0][0][4] = 1; state.vars[0][1][4] = 1;
  setTimeout(() => { NMel.onState(state); NMel.onFrame({ beat: 10.4, rec: false, recSec: 0, level: 0, bpm: 94 }); }, 0);
}

window.addEventListener("resize", applyScale);
applyScale();
renderControls();
native("ready");
