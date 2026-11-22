// Nova 2-Track UI: six shaping controls around a stereo-image scope, plus Input,
// Soft Clipper (with ceiling) and Output. Talks to C++ through JUCE native functions;
// in a plain browser it runs on a mock so the UI can be previewed.

const inJuce = !!(window.__JUCE__ && window.__JUCE__.backend);
const native = {};
const NATIVE_NAMES = ["getState", "setParam", "setProgram", "activateLicense"];

const $ = (id) => document.getElementById(id);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

// id: [min, max, default, step, unit]
const RANGES = {
  input: [-12, 12, 0, 0.1, "dB"], width: [0, 200, 100, 1, "%"], vox_carve: [0, 100, 0, 1, "%"], punch: [0, 100, 0, 1, "%"],
  low_end: [0, 100, 0, 1, "%"], instruments: [0, 100, 0, 1, "%"], energy: [0, 100, 0, 1, "%"],
  ceiling: [-12, 0, -0.3, 0.1, "dBFS"], output: [-12, 6, 0, 0.1, "dB"], clip: [0, 1, 0, 1, ""],
};
const GROUPS = {
  groupImage: [
    ["width", "WIDTH", "Opens the stereo spread above the low end and keeps the centre focused."],
    ["vox_carve", "VOX CARVE", "A pocket in the centre where the vocal sits. The sides are untouched."],
    ["low_end", "LOW END", "Folds the bass and kick to the centre and adds bottom weight."],
  ],
  groupImpact: [
    ["punch", "PUNCH", "Low-end transient shaper: kick and bass impact without bloat."],
    ["energy", "ENERGY", "Mid-range transient shaper that brings life back to flat beats."],
    ["instruments", "INSTRUMENTS", "Richer, louder instruments in the stereo field."],
  ],
};

let S = null;
const P = {};
for (const k in RANGES) P[k] = RANGES[k][2];
P.bypass = false;
let program = 0;
const dragging = new Set();
const draw = {};

function fit() {
  const s = Math.min(window.innerWidth / 1040, window.innerHeight / 640);
  $("plugin").style.transform = `translate(-50%, -50%) scale(${s})`;
}
window.addEventListener("resize", fit);
fit();

function fmt(id, v) {
  const [, , , step, unit] = RANGES[id];
  if (unit === "%") return `${Math.round(v)} %`;
  return `${(v > 0 && unit === "dB" ? "+" : "")}${v.toFixed(step < 1 ? 1 : 0)} ${unit}`;
}
function send(id, value, phase) {
  P[id] = value;
  native.setParam(id, value, phase);
  render();
}

// ── Knobs (same construction as Nova Mid) ─────────────────────────────────
const NS = "http://www.w3.org/2000/svg";
const A0 = 135, SWEEP = 270;
const polar = (cx, cy, r, deg) => { const a = deg * Math.PI / 180; return [cx + r * Math.cos(a), cy + r * Math.sin(a)]; };
function arcPath(cx, cy, r, a0, a1) {
  const [x0, y0] = polar(cx, cy, r, a0), [x1, y1] = polar(cx, cy, r, a1);
  return `M${x0} ${y0} A${r} ${r} 0 ${Math.abs(a1 - a0) > 180 ? 1 : 0} ${a1 >= a0 ? 1 : 0} ${x1} ${y1}`;
}
function el(tag, attrs, parent) {
  const n = document.createElementNS(NS, tag);
  for (const k in attrs) n.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(n);
  return n;
}
let gid = 0;
function buildKnob(host, size, centreDetent = null) {
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
    const a = A0 + SWEEP * norm;
    const from = centreDetent == null ? A0 : A0 + SWEEP * centreDetent;
    fill.setAttribute("d", Math.abs(a - from) > 0.5 ? arcPath(c, c, R, from, a) : "");
    const [x0, y0] = polar(c, c, body * 0.45, a), [x1, y1] = polar(c, c, body - 3, a);
    ptr.setAttribute("x1", x0); ptr.setAttribute("y1", y0); ptr.setAttribute("x2", x1); ptr.setAttribute("y2", y1);
  };
}
function dragValue(elm, id, px = 220) {
  const [lo, hi, def, step] = RANGES[id];
  const q = (v) => Math.round(clamp(v, lo, hi) / step) * step;
  let y0 = 0, v0 = 0;
  elm.addEventListener("pointerdown", (e) => {
    e.preventDefault(); elm.setPointerCapture(e.pointerId);
    y0 = e.clientY; v0 = P[id]; dragging.add(id); native.setParam(id, P[id], "begin");
  });
  elm.addEventListener("pointermove", (e) => {
    if (!dragging.has(id)) return;
    const v = q(v0 + (y0 - e.clientY) * (hi - lo) / px * (e.shiftKey ? 0.2 : 1));
    if (v !== P[id]) send(id, v, "move");
  });
  const end = () => { if (dragging.delete(id)) native.setParam(id, P[id], "end"); };
  elm.addEventListener("pointerup", end); elm.addEventListener("pointercancel", end);
  elm.addEventListener("dblclick", () => send(id, def, "set"));
  elm.addEventListener("wheel", (e) => { e.preventDefault(); send(id, q(P[id] + (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? step : Math.max(step, (hi - lo) / 50))), "set"); }, { passive: false });
}

function buildControls() {
  for (const [gidName, list] of Object.entries(GROUPS)) {
    const g = $(gidName);
    for (const [id, name, desc] of list) {
      const row = document.createElement("div");
      row.className = "ctl";
      row.innerHTML = `<div class="knob" id="k_${id}"></div><div><div class="name">${name}</div><div class="desc">${desc}</div><div class="val" id="v_${id}"></div></div>`;
      g.appendChild(row);
    }
  }
  for (const id of Object.keys(RANGES)) {
    if (id === "clip") continue;
    const host = $(`k_${id}`);
    const size = host.closest(".mini") ? 66 : 82;
    draw[id] = buildKnob(host, size, id === "width" ? 0.5 : null);
    dragValue(host, id);
  }
}

// ── Stereo image scope ────────────────────────────────────────────────────
let scopePts = [];
function drawScope() {
  const cv = $("scope"), dpr = 2;
  const W = cv.clientWidth, H = cv.clientHeight;
  if (!W || !H) return;
  if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
  const g = cv.getContext("2d");
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, W, H);
  const cx = W / 2, cy = H - 26, R = Math.min(W * 0.44, H - 52);
  // guides: half-circle, M axis, L/R diagonals
  g.strokeStyle = "rgba(174,183,203,.10)"; g.lineWidth = 1;
  for (const f of [0.33, 0.66, 1]) { g.beginPath(); g.arc(cx, cy, R * f, Math.PI, 2 * Math.PI); g.stroke(); }
  g.beginPath(); g.moveTo(cx - R, cy); g.lineTo(cx + R, cy); g.stroke();
  g.strokeStyle = "rgba(255,189,74,.18)";
  g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx, cy - R); g.stroke();
  g.strokeStyle = "rgba(174,183,203,.12)";
  for (const s of [-1, 1]) { g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + s * R * Math.SQRT1_2, cy - R * Math.SQRT1_2); g.stroke(); }
  g.fillStyle = "rgba(174,183,203,.45)"; g.font = "10px Avenir Next, sans-serif"; g.textAlign = "center";
  g.fillText("M", cx, cy - R - 6); g.fillText("L", cx - R * 0.74, cy - R * 0.74); g.fillText("R", cx + R * 0.74, cy - R * 0.74);
  g.fillText("+S", cx + R + 0, cy + 14); g.fillText("-S", cx - R, cy + 14);

  // points (polar-folded so the image reads as a half-dome, like a modern vectorscope)
  let peak = 0.05;
  for (let i = 0; i < scopePts.length; i += 2) peak = Math.max(peak, Math.abs(scopePts[i]), Math.abs(scopePts[i + 1]));
  const k = R / Math.max(0.25, peak * 1.15);
  g.globalCompositeOperation = "lighter";
  for (let i = 0; i < scopePts.length; i += 2) {
    const l = scopePts[i], r = scopePts[i + 1];
    let m = (l + r) * Math.SQRT1_2, s = (l - r) * Math.SQRT1_2;
    if (m < 0) { m = -m; s = -s; }
    const x = cx - s * k, y = cy - m * k;
    const age = i / scopePts.length;
    g.fillStyle = `rgba(255, ${170 + age * 60 | 0}, ${70 + age * 90 | 0}, ${0.08 + age * 0.5})`;
    g.fillRect(x - 1, y - 1, 2.2, 2.2);
  }
  g.globalCompositeOperation = "source-over";
}


// ── Spectrum view: live mid/side spectrum + what each control is doing ──
let specM = null, specS = null, view = "spec";
const pulse = { punch: 0, energy: 0 };
const FS = 48000;
const LO = Math.log10(20), HI = Math.log10(20000);
function biquadDb(c, f) {
  const w = 2 * Math.PI * f / FS, cw = Math.cos(w), c2 = Math.cos(2 * w), sw = Math.sin(w), s2 = Math.sin(2 * w);
  const nr = c.b0 + c.b1 * cw + c.b2 * c2, ni = -(c.b1 * sw + c.b2 * s2);
  const dr = c.a0 + c.a1 * cw + c.a2 * c2, di = -(c.a1 * sw + c.a2 * s2);
  return 10 * Math.log10((nr * nr + ni * ni) / (dr * dr + di * di));
}
function peakC(f0, q, g) {
  const A = Math.pow(10, g / 40), w0 = 2 * Math.PI * f0 / FS, al = Math.sin(w0) / (2 * q), cw = Math.cos(w0);
  return { b0: 1 + al * A, b1: -2 * cw, b2: 1 - al * A, a0: 1 + al / A, a1: -2 * cw, a2: 1 - al / A };
}
function shelfC(f0, g) {
  const A = Math.pow(10, g / 40), w0 = 2 * Math.PI * f0 / FS, cw = Math.cos(w0), al = Math.sin(w0) / 2 * Math.SQRT2, sA = 2 * Math.sqrt(A) * al;
  return { b0: A * ((A + 1) - (A - 1) * cw + sA), b1: 2 * A * ((A - 1) - (A + 1) * cw), b2: A * ((A + 1) - (A - 1) * cw - sA),
           a0: (A + 1) + (A - 1) * cw + sA, a1: -2 * ((A - 1) + (A + 1) * cw), a2: (A + 1) + (A - 1) * cw - sA };
}
function curves() {
  const low = P.low_end / 100, shelf = shelfC(90, 3 * low), carve = peakC(2000, 0.8, -6 * P.vox_carve / 100), inst = peakC(900, 0.5, 6 * P.instruments / 100);
  const w = P.width / 100, fold = 1 - low;
  const mid = (f) => P.bypass ? 0 : biquadDb(shelf, f) + biquadDb(carve, f);
  const side = (f) => {
    if (P.bypass) return 0;
    const r = Math.pow(f / 120, 4), lp = 1 / (1 + r), hp = r / (1 + r);
    return 20 * Math.log10(Math.max(1e-4, lp * fold + hp * w)) + biquadDb(inst, f);
  };
  return { mid, side };
}
function drawSpec() {
  const cv = $("spec"), dpr = 2, W = cv.clientWidth, H = cv.clientHeight;
  if (!W || !H) return;
  if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
  const g = cv.getContext("2d");
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, W, H);
  const padB = 18, plotH = H - padB;
  const fx = (f) => (Math.log10(f) - LO) / (HI - LO) * W;
  const sy = (db) => plotH - clamp((db + 84) / 78, 0, 1) * plotH;        // spectrum -84..-6 dB
  const cy = (db) => plotH * 0.5 - clamp(db, -15, 15) / 15 * plotH * 0.42; // curves ±15 dB

  // grid + axis
  g.strokeStyle = "rgba(174,183,203,.07)"; g.lineWidth = 1;
  g.fillStyle = "rgba(174,183,203,.42)"; g.font = "9.5px Avenir Next, sans-serif"; g.textAlign = "center";
  for (const [f, t] of [[50, "50"], [100, "100"], [200, "200"], [500, "500"], [1000, "1k"], [2000, "2k"], [5000, "5k"], [10000, "10k"]]) {
    g.beginPath(); g.moveTo(fx(f), 0); g.lineTo(fx(f), plotH); g.stroke();
    g.fillText(t, fx(f), H - 5);
  }
  g.strokeStyle = "rgba(255,189,74,.14)";
  g.beginPath(); g.moveTo(0, cy(0)); g.lineTo(W, cy(0)); g.stroke();

  // punch / energy zones (pulse with the transient shapers)
  const zone = (f0, f1, amt, p, label) => {
    if (amt <= 0 || P.bypass) return;
    const x0 = fx(f0), x1 = fx(f1);
    const a = 0.05 + 0.10 * amt + 0.35 * clamp(p / 4, 0, 1);
    const gr = g.createLinearGradient(0, 0, 0, plotH);
    gr.addColorStop(0, `rgba(255,138,22,${a})`); gr.addColorStop(1, `rgba(255,138,22,${a * 0.25})`);
    g.fillStyle = gr; g.fillRect(x0, 0, x1 - x0, plotH);
    tag(label + (p > 0.05 ? ` +${p.toFixed(1)} dB` : ""), (x0 + x1) / 2, 14, "#ffb06a", true);
  };
  const tag = (text, x, y, color, centred) => {
    g.font = "600 9.5px Avenir Next, sans-serif";
    const tw = g.measureText(text).width + 12;
    const bx = clamp(centred ? x - tw / 2 : x + 8, 4, W - tw - 4), by = clamp(y - 9, 2, plotH - 20);
    g.fillStyle = "rgba(3,7,16,.85)"; g.strokeStyle = color; g.lineWidth = 1;
    g.beginPath(); g.roundRect ? g.roundRect(bx, by, tw, 17, 5) : g.rect(bx, by, tw, 17); g.fill(); g.stroke();
    g.fillStyle = color; g.textAlign = "left"; g.fillText(text, bx + 6, by + 12);
  };
  zone(20, 150, P.punch / 100, pulse.punch, "PUNCH");
  zone(1200, 5000, P.energy / 100, pulse.energy, "ENERGY");

  // live spectrum (mid gold, side blue)
  const area = (arr, stroke, fillTop) => {
    if (!arr) return;
    g.beginPath();
    arr.forEach((v, i) => {
      const f = Math.pow(10, LO + (HI - LO) * (i + 0.5) / arr.length);
      i ? g.lineTo(fx(f), sy(v)) : g.moveTo(fx(f), sy(v));
    });
    g.lineTo(W, plotH); g.lineTo(0, plotH); g.closePath();
    const gr = g.createLinearGradient(0, 0, 0, plotH);
    gr.addColorStop(0, fillTop); gr.addColorStop(1, "rgba(0,0,0,0)");
    g.fillStyle = gr; g.fill();
    g.strokeStyle = stroke; g.lineWidth = 1; g.stroke();
  };
  area(specS, "rgba(127,178,255,.45)", "rgba(127,178,255,.20)");
  area(specM, "rgba(255,189,74,.45)", "rgba(255,170,56,.22)");

  // what the controls are doing
  const { mid, side } = curves();
  const line = (fn, color) => {
    g.beginPath();
    for (let x = 0; x <= W; x += 2) {
      const f = Math.pow(10, LO + x / W * (HI - LO)), y = cy(fn(f));
      x ? g.lineTo(x, y) : g.moveTo(x, y);
    }
    g.strokeStyle = color; g.lineWidth = 2.2; g.shadowColor = color; g.shadowBlur = 8; g.stroke(); g.shadowBlur = 0;
  };
  line(side, "#7fb2ff");
  line(mid, "#ffbd4a");

  // labels at each control's point on its curve
  const db1 = (v) => `${v > 0 ? "+" : ""}${v.toFixed(1)} dB`;
  if (!P.bypass) {
    if (P.vox_carve > 0) tag(`VOX CARVE ${db1(mid(2000) - biquadDb(shelfC(90, 3 * P.low_end / 100), 2000))}`, fx(2000), cy(mid(2000)) + 22, "#ffbd4a", true);
    if (P.low_end > 0) {
      tag(`LOW END ${db1(mid(45))}`, fx(45), cy(mid(45)) - 16, "#ffbd4a", false);
      tag("SIDE LOWS TO MONO", fx(60), cy(side(60)) + 6, "#7fb2ff", false);
    }
    if (P.instruments > 0) tag(`INSTRUMENTS ${db1(6 * P.instruments / 100)}`, fx(900), cy(side(900)) - 18, "#7fb2ff", true);
    if (Math.abs(P.width - 100) >= 1) tag(`WIDTH ${P.width > 0 ? db1(20 * Math.log10(P.width / 100)) : "MONO"}`, fx(7000), cy(side(7000)) - 18, "#7fb2ff", true);
  }
}
function setView(v) {
  view = v;
  $("tabSpec").classList.toggle("on", v === "spec");
  $("tabImage").classList.toggle("on", v === "image");
  $("spec").style.display = v === "spec" ? "block" : "none";
  $("scope").style.display = v === "image" ? "block" : "none";
  $("legend").style.display = v === "spec" ? "flex" : "none";
  $("wtag").style.display = v === "image" ? "block" : "none";
  try { localStorage.setItem("nova2track.view", v); } catch (e) {}
  v === "spec" ? drawSpec() : drawScope();
}

// ── Meters ────────────────────────────────────────────────────────────────
const M_LO = -48;
const mY = (db) => clamp((db - M_LO) / -M_LO, 0, 1);
const shown = { inL: M_LO, inR: M_LO, outL: M_LO, outR: M_LO, sc: 0 };
let clipUntil = 0, corrShown = 1;
function onMeters(d) {
  const now = performance.now();
  for (const k of ["inL", "inR", "outL", "outR"]) {
    shown[k] = Math.max(d[k] == null ? -120 : d[k], shown[k] - 0.8);
    $(k).style.height = `${mY(shown[k]) * 100}%`;
  }
  shown.sc = Math.max(d.over || 0, shown.sc - 0.3);
  if ((d.over || 0) > 0.1) clipUntil = now + 250;
  $("scFill").style.width = `${clamp(shown.sc / 9, 0, 1) * 100}%`;
  $("scVal").textContent = `${shown.sc.toFixed(1)} dB`;
  $("clipLed").classList.toggle("on", P.clip > 0.5 && now < clipUntil);
  corrShown += ((d.corr ?? 1) - corrShown) * 0.25;
  $("corrDot").style.left = `${(corrShown + 1) * 50}%`;
  if (d.scope) scopePts = d.scope;
  if (d.specM) { specM = d.specM; specS = d.specS; }
  pulse.punch = Math.max(d.punchDb || 0, pulse.punch * 0.8);
  pulse.energy = Math.max(d.energyDb || 0, pulse.energy * 0.8);
  view === "spec" ? drawSpec() : drawScope();
}

// ── Render ────────────────────────────────────────────────────────────────
function presetMatches(p) {
  return Object.keys(p.params).every((k) => Math.abs(p.params[k] - P[k]) < 0.05);
}
function render() {
  for (const id in draw) {
    const [lo, hi] = RANGES[id];
    draw[id]((P[id] - lo) / (hi - lo));
    const v = $(`v_${id}`); if (v) v.textContent = fmt(id, P[id]);
  }
  $("wtag").textContent = `WIDTH ${Math.round(P.width)}%`;
  if (view === "spec") drawSpec();
  $("clipToggle").classList.toggle("on", P.clip > 0.5);
  $("clipper").classList.toggle("off", P.clip < 0.5);
  $("power").classList.toggle("off", P.bypass);
  $("plugin").classList.toggle("bypassed", P.bypass);
  if (S) {
    const p = S.presets[program];
    $("pnameText").textContent = p ? p.name : "Custom";
    $("pname").classList.toggle("modified", !!p && !presetMatches(p));
    [...$("plist").children].forEach((n, i) => n.classList.toggle("on", i === program));
  }
}

async function loadProgram(i) {
  const n = S.presets.length;
  program = ((i % n) + n) % n;
  const st = await native.setProgram(program);
  if (st && st.params) Object.assign(P, st.params);
  render();
}
function buildPresetList() {
  const list = $("plist");
  list.innerHTML = "";
  S.presets.forEach((p, i) => {
    const d = document.createElement("div");
    d.textContent = p.name;
    d.onclick = (e) => { e.stopPropagation(); list.classList.remove("open"); loadProgram(i); };
    list.appendChild(d);
  });
}

function wire() {
  $("clipToggle").onclick = () => send("clip", P.clip > 0.5 ? 0 : 1, "set");
  $("power").onclick = () => send("bypass", !P.bypass, "set");
  $("prev").onclick = () => loadProgram(program - 1);
  $("next").onclick = () => loadProgram(program + 1);
  $("pname").onclick = () => $("plist").classList.toggle("open");
  document.addEventListener("pointerdown", (e) => { if (!$("pname").contains(e.target)) $("plist").classList.remove("open"); });
  $("licGo").onclick = async () => {
    const r = await native.activateLicense($("licKey").value);
    const msgs = { invalid: "That key isn't valid.", revoked: "That key has been revoked.", seats: "That key is already active on 2 computers.",
                   network: "Couldn't reach the license server. Check your connection.", empty: "Enter your key.", error: "Something went wrong. Try again." };
    if (r === "ok") $("licenseOverlay").classList.remove("open"); else $("licMsg").textContent = msgs[r] || msgs.error;
  };
  window.addEventListener("resize", () => (view === "spec" ? drawSpec() : drawScope()));
  $("tabSpec").onclick = () => setView("spec");
  $("tabImage").onclick = () => setView("image");
  let saved = "spec"; try { saved = localStorage.getItem("nova2track.view") || "spec"; } catch (e) {}
  setView(saved === "image" ? "image" : "spec");
}

window.novaTwoTrackUpdate = (d) => {
  if (d.params) for (const k in d.params) if (!dragging.has(k)) P[k] = d.params[k];
  P.bypass = !!d.bypass;
  if (typeof d.program === "number") program = d.program;
  onMeters(d);
  render();
};

async function main() {
  if (inJuce) {
    const { getNativeFunction } = await import("./juce/index.js");
    for (const n of NATIVE_NAMES) native[n] = getNativeFunction(n);
  } else {
    Object.assign(native, createMock());
  }
  buildControls();
  S = await native.getState();
  Object.assign(P, S.params);
  P.bypass = S.bypass;
  program = S.program;
  buildPresetList();
  wire();
  render();
  if (S.licenseRequired && !S.unlocked) $("licenseOverlay").classList.add("open");
}
main();

// ── Browser-preview mock (not used inside the plugin) ─────────────────────
function createMock() {
  const mk = (name, input, width, vox_carve, punch, low_end, instruments, energy, clip, ceiling) =>
    ({ name, params: { input, width, vox_carve, punch, low_end, instruments, energy, clip, ceiling, output: 0 } });
  const presets = [
    mk("Default", 0, 100, 0, 0, 0, 0, 0, 0, -0.3), mk("Vocal Over Beat", 0, 110, 45, 20, 35, 20, 15, 1, -1), mk("Rap Pocket", 0, 105, 60, 30, 40, 10, 10, 1, -1),
    mk("Singer Space", 0, 115, 40, 10, 30, 25, 10, 1, -1), mk("Wide and Open", 0, 140, 15, 0, 30, 35, 15, 0, -0.3), mk("Punchy Drums", 0, 100, 0, 60, 25, 0, 35, 1, -0.5),
    mk("Club Low End", 0, 100, 10, 35, 70, 0, 0, 1, -0.5), mk("Flat Beat Revive", 0, 115, 0, 35, 20, 30, 55, 1, -0.5), mk("Loud 2-Track", 4, 110, 20, 30, 40, 25, 25, 1, -0.3),
    mk("Mono Lows, Wide Top", 0, 130, 0, 0, 100, 20, 0, 0, -0.3), mk("Mix Bus Polish", 0, 108, 0, 15, 25, 15, 15, 0, -0.3), mk("Master Push", 3, 105, 0, 20, 30, 10, 15, 1, -0.3),
  ];
  const st = { params: { ...presets[1].params }, bypass: false, program: 1 };
  let t = 0;
  setInterval(() => {
    t += 1 / 30;
    const beat = Math.pow(Math.max(0, Math.sin(t * Math.PI * 2 * 1.2)), 5);
    const p = st.params, w = p.width / 100;
    const scope = [];
    for (let i = 0; i < 256; i++) {
      const ph = t * 40 + i * 0.09;
      const m = (0.35 + beat * 0.3) * Math.sin(ph) * (0.6 + 0.4 * Math.sin(ph * 0.37));
      const s = 0.22 * w * Math.sin(ph * 1.7 + 1.3) * (0.5 + Math.random() * 0.5);
      scope.push((m + s) * Math.SQRT1_2, (m - s) * Math.SQRT1_2);
    }
    const inDb = -12 + beat * 8 + Math.random();
    const over = p.clip ? Math.max(0, inDb + p.input + 4 + p.punch * 0.03 - p.ceiling) : 0;
    const out = p.clip ? Math.min(inDb + p.input + 4, p.ceiling) : inDb + p.input;
    const { mid, side } = curves();
    const base = (f, tilt) => -22 - 4.5 * Math.log2(Math.max(f, 30) / 100) * tilt + (f < 60 ? -8 * Math.log2(60 / f) : 0);
    const mk = (fn, off, lowCut) => Array.from({ length: 120 }, (_, i) => {
      const f = Math.pow(10, LO + (HI - LO) * (i + 0.5) / 120);
      const lc = lowCut && f < 150 ? -14 * Math.log2(150 / f) : 0;
      return base(f, 1) + off + lc + fn(f) + beat * (f < 150 ? 8 : 2) + (Math.random() - 0.5) * 5;
    });
    const specMock = { specM: mk(mid, 0, false), specS: mk(side, -9, true),
                       punchDb: p.punch / 100 * beat * 3.5, energyDb: p.energy / 100 * beat * 3 };
    window.novaTwoTrackUpdate({ ...specMock, inL: inDb, inR: inDb - 0.6, outL: out, outR: out - 0.4, over, corr: 0.55 - (w - 1) * 0.3, scope, ...st });
  }, 1000 / 30);
  return {
    getState: async () => ({ presets, ...st, licenseRequired: false, unlocked: true }),
    setParam: async (id, v) => { if (id === "bypass") st.bypass = v; else st.params[id] = v; },
    setProgram: async (i) => { st.program = i; st.params = { ...presets[i].params }; return { ...st }; },
    activateLicense: async () => "ok",
  };
}
