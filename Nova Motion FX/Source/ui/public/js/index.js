"use strict";

// ── JUCE bridge ──────────────────────────────────────────────────────────────
function juceEmit(paramId, eventType, value) {
  try {
    const b = window.__JUCE__ && window.__JUCE__.backend;
    if (!b) return;
    const payload = { eventType };
    if (value !== undefined) payload.value = value;
    b.emitEvent("__juce__slider" + paramId, payload);
  } catch (_) {}
}
function sendToJuce(param, value) { juceEmit(param, "valueChanged", value); }
function beginGesture(param)      { juceEmit(param, "sliderDragStarted"); }
function endGesture(param)        { juceEmit(param, "sliderDragEnded"); }
function requestInitialUpdates()  { for (const p of Object.keys(params)) juceEmit(p, "requestInitialUpdate"); }

function subscribeJuceParams() {
  try {
    const b = window.__JUCE__ && window.__JUCE__.backend;
    if (!b) return;
    for (const p of Object.keys(params)) {
      b.addEventListener("__juce__slider" + p, (evt) => {
        try {
          const data = JSON.parse(evt.data);
          if (data.sliderValue !== undefined) {
            params[p] = data.sliderValue * 100;
            updateKnobDisplay(p, params[p]);
          }
        } catch (_) {}
      });
    }
  } catch (_) {}
}

// Meter update called from C++ timer at 30 Hz
window.updateMeters = function(l, r) {
  juceMetersActive = true;
  audioLevel = Math.max(l, r);
  const elL = document.getElementById("meter-l");
  const elR = document.getElementById("meter-r");
  if (elL) elL.style.height = Math.min(100, l * 100) + "%";
  if (elR) elR.style.height = Math.min(100, r * 100) + "%";
};

// ── Parameter state (0–100 internally) ───────────────────────────────────────
const params = {
  motion:     50,
  cutoff:     60,
  resonance:  30,
  drive:      20,
  feedback:   40,
  delay_mix:  50,
  size:       45,
  decay:      55,
  reverb_mix: 35,
  lfo_rate:   35,
  lfo_depth:  60,
  input:      75,
  output:     80,
  mix:        100,
};

// ── Presets ───────────────────────────────────────────────────────────────────
const PRESETS = [
  { name:"CITRUS SPLASH",  motion:50, cutoff:60, resonance:30, drive:20, feedback:40, delay_mix:50, size:45, decay:55, reverb_mix:35 },
  { name:"DEEP SPACE",     motion:70, cutoff:30, resonance:55, drive:10, feedback:60, delay_mix:55, size:80, decay:75, reverb_mix:50 },
  { name:"NOVA SWEEP",     motion:40, cutoff:75, resonance:20, drive:35, feedback:25, delay_mix:30, size:45, decay:40, reverb_mix:25 },
  { name:"VOCAL CLOUD",    motion:55, cutoff:50, resonance:45, drive:15, feedback:50, delay_mix:45, size:70, decay:65, reverb_mix:45 },
  { name:"TAPE FLUTTER",   motion:30, cutoff:80, resonance:15, drive:50, feedback:20, delay_mix:20, size:35, decay:30, reverb_mix:15 },
  { name:"INFINITE HALL",  motion:80, cutoff:45, resonance:60, drive:8,  feedback:75, delay_mix:60, size:95, decay:90, reverb_mix:70 },
  { name:"MOTION FREEZE",  motion:65, cutoff:40, resonance:70, drive:18, feedback:55, delay_mix:50, size:85, decay:100,reverb_mix:80 },
];
let presetIndex = 0;

function applyPreset(p) {
  Object.assign(params, p);
  document.getElementById("preset-name").textContent = p.name;
  updateAllKnobs();
}

// ── Arc constants ─────────────────────────────────────────────────────────────
// Motion: r=70  circ=439.82  full270=329.87
// Std:    r=29  circ=182.21  full270=136.66
// Mini:   r=15  circ=94.25   full270=70.69
const CIRC_MOTION = 2 * Math.PI * 70;
const CIRC_STD    = 2 * Math.PI * 29;
const CIRC_MINI   = 2 * Math.PI * 15;
const MINI_PARAMS = new Set(["lfo_rate","lfo_depth","input","output","mix"]);

function updateKnobVisual(param, normalValue) {
  const arcEl = document.getElementById("arc-" + param);
  const indEl = document.getElementById("ind-" + param);
  if (!arcEl && !indEl) return;
  const isMotion = param === "motion";
  const isMini   = MINI_PARAMS.has(param);
  const circ = isMotion ? CIRC_MOTION : (isMini ? CIRC_MINI : CIRC_STD);
  const full = circ * 270 / 360;
  const dash = normalValue * full;
  const gap  = circ - dash;
  if (arcEl) arcEl.setAttribute("stroke-dasharray", dash.toFixed(2) + " " + gap.toFixed(2));
  if (indEl) indEl.style.transform = "rotate(" + (-135 + normalValue * 270).toFixed(1) + "deg)";
}

function updateKnobDisplay(param, val) {
  const n = val / 100;
  updateKnobVisual(param, n);
  const el = document.getElementById("val-" + param);
  if (!el) return;
  switch (param) {
    case "cutoff":
      el.textContent = n < 0.4
        ? Math.round(20 + n * 980) + " Hz"
        : ((20 + n * 19980) / 1000).toFixed(1) + "kHz";
      break;
    case "decay":
      el.textContent = (0.1 + n * 9.9).toFixed(2) + "s";
      break;
    case "input": case "output": {
      const db = (n - 0.5) * 24;
      el.textContent = (db >= 0 ? "+" : "") + db.toFixed(1) + " dB"; break; }
    case "drive": {
      const db = n * 24 - 12;
      el.textContent = (db >= 0 ? "+" : "") + db.toFixed(1) + " dB"; break; }
    case "lfo_rate": {
      const steps = ["1/32","1/16","1/8","1/4","1/2","1","2","4"];
      el.textContent = steps[Math.round(n * (steps.length - 1))]; break; }
    default:
      el.textContent = Math.round(val) + "%";
  }
}

function updateAllKnobs() {
  for (const [p, v] of Object.entries(params)) updateKnobDisplay(p, v);
}

// ── Knob drag ─────────────────────────────────────────────────────────────────
function setupKnob(el) {
  const param = el.dataset.param;
  if (!param) return;
  let startY = 0, startVal = 0;

  function onMove(e) {
    const cy  = e.touches ? e.touches[0].clientY : e.clientY;
    const dy  = startY - cy;
    const newVal = Math.max(0, Math.min(100, startVal + dy * 0.5));
    params[param] = newVal;
    updateKnobDisplay(param, newVal);
    sendToJuce(param, newVal / 100);
    if (param === "motion") applyMotionMacro(newVal);
  }
  function onUp() {
    endGesture(param);
    window.removeEventListener("mousemove", onMove);
    window.removeEventListener("mouseup",   onUp);
    window.removeEventListener("touchmove", onMove);
    window.removeEventListener("touchend",  onUp);
  }
  function onDown(e) {
    e.preventDefault();
    startY   = e.touches ? e.touches[0].clientY : e.clientY;
    startVal = params[param] ?? 50;
    beginGesture(param);
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup",   onUp);
    window.addEventListener("touchmove", onMove, { passive: false });
    window.addEventListener("touchend",  onUp);
  }
  el.addEventListener("mousedown",  onDown);
  el.addEventListener("touchstart", onDown, { passive: false });
  el.addEventListener("dblclick", () => {
    const def = (parseFloat(el.dataset.default) || 0.5) * 100;
    params[param] = def;
    updateKnobDisplay(param, def);
    sendToJuce(param, def / 100);
  });
}

// ── MOTION macro ──────────────────────────────────────────────────────────────
function applyMotionMacro(v) {
  const n = v / 100;
  params.cutoff     = Math.max(0, Math.min(100, 80 - n * 60));
  params.resonance  = Math.min(100, 20 + n * 55);
  params.drive      = Math.min(100, n * 45);
  params.delay_mix  = Math.min(100, 10 + n * 60);
  params.feedback   = Math.min(100, 20 + n * 55);
  params.reverb_mix = Math.min(100, 15 + n * 65);
  updateAllKnobs();
  // send all macro-affected params to JUCE
  for (const p of ["cutoff","resonance","drive","delay_mix","feedback","reverb_mix"]) {
    sendToJuce(p, params[p] / 100);
  }
}

// ── Energy ring (Motion knob halo) ────────────────────────────────────────────
let energyCanvas, energyCtx, energyPhase = 0, audioLevel = 0;

function drawEnergyRing() {
  if (!energyCanvas) { requestAnimationFrame(drawEnergyRing); return; }
  const W = energyCanvas.width, H = energyCanvas.height;
  const cx = W / 2, cy = H / 2;
  const r = 90; // ring radius in canvas pixels

  energyCtx.clearRect(0, 0, W, H);
  energyPhase += 0.007;

  const pulse = 0.3 + Math.sin(energyPhase * 1.2) * 0.08 + audioLevel * 0.5;
  const motion = params.motion / 100;

  // Outer diffuse glow
  const outerGrad = energyCtx.createRadialGradient(cx, cy, r - 18, cx, cy, r + 26);
  outerGrad.addColorStop(0, `rgba(142,91,255,${(0.18 + motion * 0.14) * pulse})`);
  outerGrad.addColorStop(0.5, `rgba(120,70,230,${(0.09 + motion * 0.08) * pulse})`);
  outerGrad.addColorStop(1, "rgba(100,50,200,0)");
  energyCtx.beginPath();
  energyCtx.arc(cx, cy, r, 0, Math.PI * 2);
  energyCtx.lineWidth = 44;
  energyCtx.strokeStyle = outerGrad;
  energyCtx.stroke();

  // Sharp ring line
  energyCtx.beginPath();
  energyCtx.arc(cx, cy, r, 0, Math.PI * 2);
  energyCtx.lineWidth = 1.5;
  const alpha = (0.3 + motion * 0.3 + audioLevel * 0.3) * (0.85 + Math.sin(energyPhase) * 0.15);
  energyCtx.strokeStyle = `rgba(180,130,255,${alpha})`;
  energyCtx.shadowColor = "rgba(142,91,255,0.7)";
  energyCtx.shadowBlur  = 10;
  energyCtx.stroke();
  energyCtx.shadowBlur  = 0;

  // Rotating spark nodes
  const nodeCount = 6;
  for (let i = 0; i < nodeCount; i++) {
    const angle  = energyPhase * 0.4 + (i / nodeCount) * Math.PI * 2;
    const wobble = Math.sin(energyPhase * 2.5 + i * 1.1) * 4;
    const sx = cx + Math.cos(angle) * (r + wobble);
    const sy = cy + Math.sin(angle) * (r + wobble);
    const sa = (0.5 + Math.sin(energyPhase * 1.8 + i * 1.3) * 0.3) * pulse;
    energyCtx.beginPath();
    energyCtx.arc(sx, sy, 2, 0, Math.PI * 2);
    energyCtx.fillStyle = `rgba(210,170,255,${sa})`;
    energyCtx.shadowColor = "rgba(180,130,255,0.8)";
    energyCtx.shadowBlur  = 6;
    energyCtx.fill();
    energyCtx.shadowBlur  = 0;
  }

  requestAnimationFrame(drawEnergyRing);
}

// ── Aurora visualizer ─────────────────────────────────────────────────────────
let auroraCanvas, auroraCtx, auroraPhase = 0;

function drawAurora() {
  if (!auroraCanvas) { requestAnimationFrame(drawAurora); return; }
  const W = auroraCanvas.width, H = auroraCanvas.height;
  auroraCtx.clearRect(0, 0, W, H);
  auroraPhase += 0.003;

  const motion  = params.motion  / 100;
  const cutoff  = params.cutoff  / 100;
  const reverb  = params.reverb_mix / 100;
  const delay_  = params.delay_mix  / 100;

  const LAYERS = [
    { speed:1.0, freq:3.2, hue:268, yBase:0.50, amp:0.22, alpha:0.55 },
    { speed:1.5, freq:4.8, hue:285, yBase:0.56, amp:0.17, alpha:0.42 },
    { speed:0.7, freq:2.4, hue:252, yBase:0.44, amp:0.18, alpha:0.38 },
    { speed:2.0, freq:6.0, hue:308, yBase:0.53, amp:0.13, alpha:0.30 },
  ];

  for (const L of LAYERS) {
    const pts = [];
    const step = 3;
    for (let x = 0; x <= W; x += step) {
      const t = x / W;
      const w1 = Math.sin(t * Math.PI * (L.freq + cutoff * 3) + auroraPhase * L.speed) * L.amp;
      const w2 = Math.sin(t * Math.PI * (L.freq * 1.6 + motion * 2) + auroraPhase * L.speed * 1.4 + 1.3) * L.amp * 0.45;
      const w3 = Math.sin(t * Math.PI * (L.freq * 0.5) + auroraPhase * L.speed * 0.6 + 2.7) * L.amp * 0.25;
      const y = H * Math.max(0.02, Math.min(0.98, L.yBase + w1 + w2 + w3));
      pts.push([x, y]);
    }

    // Filled aurora shape
    auroraCtx.beginPath();
    auroraCtx.moveTo(0, H);
    for (const [x, y] of pts) auroraCtx.lineTo(x, y);
    auroraCtx.lineTo(W, H);
    auroraCtx.closePath();

    const fill = auroraCtx.createLinearGradient(0, 0, 0, H);
    const a0 = L.alpha * (0.7 + motion * 0.35 + reverb * 0.2);
    fill.addColorStop(0,   `hsla(${L.hue},100%,68%,${a0})`);
    fill.addColorStop(0.55, `hsla(${L.hue},90%,60%,${a0 * 0.35})`);
    fill.addColorStop(1,   `hsla(${L.hue},80%,55%,0)`);
    auroraCtx.fillStyle = fill;
    auroraCtx.fill();

    // Bright edge
    auroraCtx.beginPath();
    auroraCtx.moveTo(pts[0][0], pts[0][1]);
    for (const [x, y] of pts.slice(1)) auroraCtx.lineTo(x, y);
    auroraCtx.lineWidth = 1.5;
    auroraCtx.strokeStyle = `hsla(${L.hue},100%,75%,${L.alpha * 0.9})`;
    auroraCtx.shadowColor = `hsla(${L.hue},100%,70%,0.6)`;
    auroraCtx.shadowBlur  = 7;
    auroraCtx.stroke();
    auroraCtx.shadowBlur  = 0;
  }

  requestAnimationFrame(drawAurora);
}

// ── LFO canvas ────────────────────────────────────────────────────────────────
let lfoCanvas, lfoCtx, lfoPhase = 0;

function drawLFO() {
  if (!lfoCanvas) { requestAnimationFrame(drawLFO); return; }
  const W = lfoCanvas.width, H = lfoCanvas.height;
  lfoCtx.clearRect(0, 0, W, H);
  const rate  = 0.5 + (params.lfo_rate  / 100) * 3.5;
  const depth = params.lfo_depth / 100;
  lfoPhase += rate * 0.008;

  lfoCtx.beginPath();
  const grad = lfoCtx.createLinearGradient(0, 0, W, 0);
  grad.addColorStop(0,   "rgba(142,91,255,0.9)");
  grad.addColorStop(0.5, "rgba(180,130,255,0.9)");
  grad.addColorStop(1,   "rgba(142,91,255,0.6)");

  lfoCtx.strokeStyle = grad;
  lfoCtx.lineWidth = 1.5;
  lfoCtx.shadowColor = "rgba(142,91,255,0.6)";
  lfoCtx.shadowBlur  = 5;

  for (let px = 0; px <= W; px++) {
    const t = (px / W) * Math.PI * 4;
    const y = H / 2 - Math.sin(t + lfoPhase) * depth * H * 0.42;
    px === 0 ? lfoCtx.moveTo(px, y) : lfoCtx.lineTo(px, y);
  }
  lfoCtx.stroke();
  lfoCtx.shadowBlur = 0;
  requestAnimationFrame(drawLFO);
}

// ── Meter simulation ──────────────────────────────────────────────────────────
let meterL = 0, meterR = 0, juceMetersActive = false;
function animateMeterSim() {
  if (juceMetersActive) return;
  meterL = meterL * 0.88 + (0.2 + Math.random() * 0.5) * (params.mix / 100) * 0.12;
  meterR = meterR * 0.88 + (0.2 + Math.random() * 0.5) * (params.mix / 100) * 0.12;
  const elL = document.getElementById("meter-l");
  const elR = document.getElementById("meter-r");
  if (elL) elL.style.height = Math.min(100, meterL * 100) + "%";
  if (elR) elR.style.height = Math.min(100, meterR * 100) + "%";
  requestAnimationFrame(animateMeterSim);
}

// ── Init ──────────────────────────────────────────────────────────────────────
window.addEventListener("DOMContentLoaded", () => {

  // Energy ring canvas
  energyCanvas = document.getElementById("energy-canvas");
  if (energyCanvas) energyCtx = energyCanvas.getContext("2d");

  // Aurora canvas — size to element
  const auroraEl = document.getElementById("aurora-canvas");
  if (auroraEl) {
    auroraCanvas = auroraEl;
    const section = auroraEl.parentElement;
    auroraCanvas.width  = section.offsetWidth  || 960;
    auroraCanvas.height = section.offsetHeight || 72;
    auroraCtx = auroraCanvas.getContext("2d");
  }

  // LFO canvas
  const lfoEl = document.getElementById("lfo-canvas");
  if (lfoEl) {
    lfoCanvas = lfoEl;
    const p = lfoEl.parentElement;
    lfoCanvas.width  = p ? (p.offsetWidth  - 110) : 200;
    lfoCanvas.height = 54;
    lfoCtx = lfoCanvas.getContext("2d");
  }

  // Wire all knobs
  document.querySelectorAll(".knob, .mini-knob").forEach(setupKnob);

  // Preset navigation
  document.getElementById("prev-preset")?.addEventListener("click", () => {
    presetIndex = (presetIndex - 1 + PRESETS.length) % PRESETS.length;
    applyPreset(PRESETS[presetIndex]);
  });
  document.getElementById("next-preset")?.addEventListener("click", () => {
    presetIndex = (presetIndex + 1) % PRESETS.length;
    applyPreset(PRESETS[presetIndex]);
  });

  // Pills
  function setupPills(sel) {
    document.querySelectorAll(sel).forEach(btn => {
      btn.addEventListener("click", () => {
        document.querySelectorAll(sel).forEach(b => b.classList.remove("active"));
        btn.classList.add("active");
      });
    });
  }
  setupPills(".ftype");
  setupPills(".dmode");
  setupPills(".rtype");

  document.getElementById("delay-sync")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("active");
  });
  document.getElementById("freeze-btn")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("active");
  });
  document.getElementById("hq-toggle")?.addEventListener("click", e => {
    const on = e.currentTarget.classList.toggle("active");
    e.currentTarget.textContent = on ? "ON" : "OFF";
  });

  // Initial display
  updateAllKnobs();

  // Start animations
  drawEnergyRing();
  drawAurora();
  drawLFO();
  animateMeterSim();

  // Connect to JUCE
  subscribeJuceParams();
  requestInitialUpdates();
});
