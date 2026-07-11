// Nova Motion FX — UI Controller
"use strict";

// ── JUCE bridge ───────────────────────────────────────────────────────────────
function juceEmit(paramId, eventType, value) {
  try {
    const b = window.__JUCE__ && window.__JUCE__.backend;
    if (!b) return;
    const payload = { eventType };
    if (value !== undefined) payload.value = value;
    b.emitEvent("__juce__slider" + paramId, payload);
  } catch (_) {}
}
function sendToJuce(param, value)  { juceEmit(param, "valueChanged", value); }
function beginGesture(param)       { juceEmit(param, "sliderDragStarted"); }
function endGesture(param)         { juceEmit(param, "sliderDragEnded"); }
function requestInitialUpdates()   { for (const p of Object.keys(params)) juceEmit(p, "requestInitialUpdate"); }

// Subscribe to parameter updates pushed from JUCE host
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

// Called by PluginEditor.cpp timer (30 Hz): updateMeters(l, r) — linear 0..1
window.updateMeters = function(l, r) {
  const elL = document.getElementById("meter-l");
  const elR = document.getElementById("meter-r");
  if (elL) elL.style.height = Math.min(100, l * 100) + "%";
  if (elR) elR.style.height = Math.min(100, r * 100) + "%";
};

// ── Parameter state (0–100 scale internally) ──────────────────────────────────
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
  if (p.lfo_rate   !== undefined) params.lfo_rate   = p.lfo_rate;
  if (p.lfo_depth  !== undefined) params.lfo_depth  = p.lfo_depth;
  document.getElementById("preset-name").textContent = p.name;
  updateAllKnobs();
}

// ── Arc circumference constants (match SVG r values in HTML) ──────────────────
// Motion:   r=50  → circ=314.159, full270=235.619
// Standard: r=29  → circ=182.212, full270=136.659
// Mini:     r=15  → circ=94.248,  full270=70.686
const CIRC_MOTION = 2 * Math.PI * 50;
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
        : ((20 + n * 19980) / 1000).toFixed(1) + " kHz";
      break;
    case "decay":
      el.textContent = (0.1 + n * 9.9).toFixed(2) + " s";
      break;
    case "input":
    case "output":
      { const db = (n - 0.5) * 24;
        el.textContent = (db >= 0 ? "+" : "") + db.toFixed(1) + " dB"; }
      break;
    case "drive":
      { const db = n * 24 - 12;
        el.textContent = (db >= 0 ? "+" : "") + db.toFixed(1) + " dB"; }
      break;
    case "lfo_rate":
      { const steps = ["1/32","1/16","1/8","1/4","1/2","1","2","4"];
        el.textContent = steps[Math.round(n * (steps.length - 1))]; }
      break;
    default:
      el.textContent = Math.round(val) + "%";
  }
}

function updateAllKnobs() {
  for (const [p, v] of Object.entries(params)) updateKnobDisplay(p, v);
  drawFilterCurve();
}

// ── Knob drag ────────────────────────────────────────────────────────────────
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
    const def = parseFloat(el.dataset.default) * 100 || 50;
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
}

// ── Filter curve canvas ───────────────────────────────────────────────────────
let filterCanvas, filterCtx;

function drawFilterCurve() {
  if (!filterCanvas) return;
  const W = filterCanvas.width, H = filterCanvas.height;
  filterCtx.clearRect(0, 0, W, H);

  const cutoff    = params.cutoff    / 100;
  const resonance = params.resonance / 100;
  const drive     = params.drive     / 100;

  filterCtx.strokeStyle = "rgba(142,91,255,0.07)";
  filterCtx.lineWidth = 1;
  for (let x = 0; x <= W; x += W / 6) {
    filterCtx.beginPath(); filterCtx.moveTo(x, 0); filterCtx.lineTo(x, H); filterCtx.stroke();
  }

  const grad = filterCtx.createLinearGradient(0, 0, W, 0);
  grad.addColorStop(0,   "rgba(142,91,255,0.9)");
  grad.addColorStop(0.5, "rgba(176,138,255,0.9)");
  grad.addColorStop(1,   "rgba(142,91,255,0.6)");

  filterCtx.beginPath();
  filterCtx.strokeStyle = grad;
  filterCtx.lineWidth = 2;
  filterCtx.shadowColor = "rgba(142,91,255,0.7)";
  filterCtx.shadowBlur  = 8;

  const driveBoost = 1 + drive * 0.4;
  for (let px = 0; px <= W; px++) {
    const t = px / W;
    let y;
    if (t < cutoff) {
      y = H * 0.35 - (t / cutoff) * drive * H * 0.1;
    } else if (Math.abs(t - cutoff) < 0.04) {
      const rel = (t - cutoff) / 0.04;
      y = H * 0.35 - resonance * H * 0.45 * Math.exp(-rel * rel * 12) * driveBoost;
    } else {
      const rolloff = (t - cutoff) / (1 - cutoff + 0.001);
      y = H * 0.35 + rolloff * rolloff * H * 0.55;
    }
    y = Math.max(2, Math.min(H - 2, y));
    px === 0 ? filterCtx.moveTo(px, y) : filterCtx.lineTo(px, y);
  }
  filterCtx.stroke();
  filterCtx.shadowBlur = 0;

  filterCtx.lineTo(W, H); filterCtx.lineTo(0, H); filterCtx.closePath();
  const fill = filterCtx.createLinearGradient(0, 0, 0, H);
  fill.addColorStop(0, "rgba(142,91,255,0.12)");
  fill.addColorStop(1, "rgba(142,91,255,0)");
  filterCtx.fillStyle = fill;
  filterCtx.fill();
}

// ── Delay echo animation ──────────────────────────────────────────────────────
let delayCanvas, delayCtx, delayPhase = 0;
function drawDelay() {
  if (!delayCanvas) return;
  const W = delayCanvas.width, H = delayCanvas.height;
  delayCtx.clearRect(0, 0, W, H);

  const feedback = params.feedback / 100;
  const mix      = params.delay_mix / 100;
  const echos    = Math.max(1, Math.round(feedback * 6));
  delayPhase += 0.018;

  for (let e = 0; e < echos; e++) {
    const amp    = mix * Math.pow(feedback, e) * H * 0.38;
    const offset = (e / echos) * W * 0.7;
    const alpha  = (1 - e / echos) * 0.8;
    delayCtx.beginPath();
    delayCtx.strokeStyle = `rgba(142,91,255,${alpha})`;
    delayCtx.lineWidth   = Math.max(0.5, 2 - e * 0.4);
    delayCtx.shadowColor = "rgba(142,91,255,0.5)";
    delayCtx.shadowBlur  = 4;
    for (let px = 0; px <= W - offset; px++) {
      const t = (px / W) * Math.PI * 8;
      const y = H / 2 + Math.sin(t + delayPhase) * amp * Math.exp(-px / W * 3);
      px === 0 ? delayCtx.moveTo(px + offset, y) : delayCtx.lineTo(px + offset, y);
    }
    delayCtx.stroke();
    delayCtx.shadowBlur = 0;
  }
  requestAnimationFrame(drawDelay);
}

// ── LFO waveform animation ────────────────────────────────────────────────────
let lfoCanvas, lfoCtx, lfoPhase = 0;
function drawLFO() {
  if (!lfoCanvas) return;
  const W = lfoCanvas.width, H = lfoCanvas.height;
  lfoCtx.clearRect(0, 0, W, H);

  const rate  = params.lfo_rate  / 100;
  const depth = params.lfo_depth / 100;
  lfoPhase += 0.03 + rate * 0.08;

  lfoCtx.beginPath();
  lfoCtx.strokeStyle = "rgba(142,91,255,0.8)";
  lfoCtx.lineWidth   = 1.5;
  lfoCtx.shadowColor = "rgba(142,91,255,0.5)";
  lfoCtx.shadowBlur  = 4;
  for (let px = 0; px <= W; px++) {
    const t = (px / W) * Math.PI * 4;
    const y = H / 2 + Math.sin(t + lfoPhase) * depth * H * 0.42;
    px === 0 ? lfoCtx.moveTo(px, y) : lfoCtx.lineTo(px, y);
  }
  lfoCtx.stroke();
  lfoCtx.shadowBlur = 0;
  requestAnimationFrame(drawLFO);
}

// ── Reverb tail display ───────────────────────────────────────────────────────
let reverbCanvas, reverbCtx, reverbPhase = 0;
function drawReverb() {
  if (!reverbCanvas) return;
  const W = reverbCanvas.width, H = reverbCanvas.height;
  reverbCtx.clearRect(0, 0, W, H);

  const size  = params.size       / 100;
  const decay = params.decay      / 100;
  const mix   = params.reverb_mix / 100;
  reverbPhase += 0.012;

  const layers = 4;
  for (let i = 0; i < layers; i++) {
    const alpha = mix * (1 - i / layers) * 0.65;
    const freq  = 3 + i * 1.5 + size * 4;
    reverbCtx.beginPath();
    reverbCtx.strokeStyle = `rgba(142,91,255,${alpha})`;
    reverbCtx.lineWidth   = 1.2 - i * 0.25;
    reverbCtx.shadowColor = "rgba(142,91,255,0.4)";
    reverbCtx.shadowBlur  = 3;
    for (let px = 0; px <= W; px++) {
      const t   = px / W;
      const amp = H * 0.38 * mix * Math.pow(decay, t * 3) * (1 - i * 0.18);
      const y   = H / 2 + Math.sin(t * Math.PI * freq + reverbPhase + i) * amp;
      px === 0 ? reverbCtx.moveTo(px, y) : reverbCtx.lineTo(px, y);
    }
    reverbCtx.stroke();
    reverbCtx.shadowBlur = 0;
  }
  requestAnimationFrame(drawReverb);
}

// ── Meter simulation (fallback when no JUCE audio) ────────────────────────────
let meterL = 0, meterR = 0;
let juceMetersActive = false;
function animateMeterSim() {
  if (juceMetersActive) return;
  meterL = meterL * 0.88 + (0.3 + Math.random() * 0.6) * (params.mix / 100) * 0.12;
  meterR = meterR * 0.88 + (0.3 + Math.random() * 0.6) * (params.mix / 100) * 0.12;
  const elL = document.getElementById("meter-l");
  const elR = document.getElementById("meter-r");
  if (elL) elL.style.height = Math.min(100, meterL * 100) + "%";
  if (elR) elR.style.height = Math.min(100, meterR * 100) + "%";
  requestAnimationFrame(animateMeterSim);
}

// Override updateMeters to flag that real meters are active
const _origUpdateMeters = window.updateMeters;
window.updateMeters = function(l, r) {
  juceMetersActive = true;
  _origUpdateMeters(l, r);
};

// ── Pill groups ───────────────────────────────────────────────────────────────
function setupPillGroup(sel) {
  document.querySelectorAll(sel).forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(sel).forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
    });
  });
}

// ── Canvas fit helper ─────────────────────────────────────────────────────────
function fitCanvas(canvas) {
  const parent = canvas.parentElement;
  const rect   = parent.getBoundingClientRect();
  const dpr    = window.devicePixelRatio || 1;
  canvas.width  = Math.round(rect.width  * dpr);
  canvas.height = Math.round(canvas.offsetHeight * dpr || canvas.height * dpr);
  const ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  return ctx;
}

// ── Init ──────────────────────────────────────────────────────────────────────
window.addEventListener("DOMContentLoaded", () => {
  // Wire canvases
  const fcEl = document.getElementById("filter-canvas");
  const dcEl = document.getElementById("delay-canvas");
  const lcEl = document.getElementById("lfo-canvas");
  const rcEl = document.getElementById("reverb-canvas");

  if (fcEl) { filterCanvas = fcEl;  filterCtx  = fitCanvas(fcEl); }
  if (dcEl) { delayCanvas  = dcEl;  delayCtx   = fitCanvas(dcEl); }
  if (lcEl) { lfoCanvas    = lcEl;  lfoCtx     = fitCanvas(lcEl); }
  if (rcEl) { reverbCanvas = rcEl;  reverbCtx  = fitCanvas(rcEl); }

  // Wire knobs — all knob containers have data-param
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
  setupPillGroup(".ftype");
  setupPillGroup(".dmode");
  setupPillGroup(".rtype");

  // Delay sync
  document.getElementById("delay-sync")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("active");
  });

  // Freeze
  document.getElementById("freeze-btn")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("active");
  });

  // HQ toggle
  document.getElementById("hq-toggle")?.addEventListener("click", e => {
    const on = e.currentTarget.classList.toggle("active");
    e.currentTarget.textContent = on ? "ON" : "OFF";
  });

  // Power
  document.getElementById("power-btn")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("on");
  });

  // Initial knob render
  updateAllKnobs();

  // Start animations
  drawDelay();
  drawLFO();
  drawReverb();
  animateMeterSim();

  // Subscribe to JUCE param updates then request current values
  subscribeJuceParams();
  requestInitialUpdates();
});
