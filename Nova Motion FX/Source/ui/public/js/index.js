// Nova Motion FX — UI Controller
// Handles knob interaction, parameter sync, animations

"use strict";

// ── JUCE bridge (graceful fallback for browser preview) ───────────────────────
let juceBackend = null;
try { juceBackend = window.__JUCE__?.initialisationData ? window.__JUCE__ : null; } catch (_) {}

function sendToJuce(param, value) {
  try {
    if (juceBackend && juceBackend.backend) {
      juceBackend.backend.emitEvent("paramChange", JSON.stringify({ param, value }));
    }
  } catch (_) {}
}

// ── Parameter state ───────────────────────────────────────────────────────────
const params = {
  motion:     50,
  cutoff:     60,
  resonance:  35,
  drive:      22,
  feedback:   35,
  delay_mix:  40,
  size:       60,
  decay:      55,
  reverb_mix: 35,
  lfo_rate:   50,
  lfo_depth:  75,
  input:      50,
  output:     50,
  mix:        100,
};

// ── Presets ───────────────────────────────────────────────────────────────────
const PRESETS = [
  { name: "CITRUS SPLASH",    motion:50,  cutoff:60,  resonance:35, drive:22, feedback:35, delay_mix:40, size:60, decay:55, reverb_mix:35 },
  { name: "DEEP SPACE",       motion:70,  cutoff:30,  resonance:55, drive:10, feedback:60, delay_mix:55, size:80, decay:75, reverb_mix:50 },
  { name: "NOVA SWEEP",       motion:40,  cutoff:75,  resonance:20, drive:35, feedback:25, delay_mix:30, size:45, decay:40, reverb_mix:25 },
  { name: "VOCAL CLOUD",      motion:55,  cutoff:50,  resonance:45, drive:15, feedback:50, delay_mix:45, size:70, decay:65, reverb_mix:45 },
  { name: "TAPE FLUTTER",     motion:30,  cutoff:80,  resonance:15, drive:50, feedback:20, delay_mix:20, size:35, decay:30, reverb_mix:15 },
  { name: "INFINITE HALL",    motion:80,  cutoff:45,  resonance:60, drive:8,  feedback:75, delay_mix:60, size:95, decay:90, reverb_mix:70 },
  { name: "MOTION FREEZE",    motion:65,  cutoff:40,  resonance:70, drive:18, feedback:55, delay_mix:50, size:85, decay:100,reverb_mix:80 },
];
let presetIndex = 0;

function applyPreset(p) {
  Object.assign(params, p);
  updateAllKnobs();
  document.getElementById("preset-name").textContent = p.name;
}

document.getElementById("preset-prev").addEventListener("click", () => {
  presetIndex = (presetIndex - 1 + PRESETS.length) % PRESETS.length;
  applyPreset(PRESETS[presetIndex]);
});
document.getElementById("preset-next").addEventListener("click", () => {
  presetIndex = (presetIndex + 1) % PRESETS.length;
  applyPreset(PRESETS[presetIndex]);
});

// ── Knob rendering ────────────────────────────────────────────────────────────
const ARC_FULL   = 270;   // degrees of travel
const ARC_START  = 135;   // rotation offset (degrees)
const CIRCUMF_80 = 2 * Math.PI * 32;   // r=32 for standard knob viewBox 80
const CIRCUMF_36 = 2 * Math.PI * 14.5; // r=14.5 for mini knob viewBox 36
const CIRCUMF_120 = 2 * Math.PI * 46;  // motion knob r=46

function renderKnobSVG(el, normalValue, isMini, isMotion) {
  const svg = el.querySelector("svg");
  if (!svg) return;
  const id = el.dataset.param || el.id;

  if (isMotion) {
    const arc = svg.getElementById ? svg : el.closest(".plugin").querySelector(`#motion-arc`);
    const ind = el.closest(".plugin").querySelector(`#motion-ind`);
    const filled = normalValue * ARC_FULL;
    const dash = (filled / 360) * CIRCUMF_120;
    const gap  = CIRCUMF_120 - dash;
    const arcEl = el.querySelector("#motion-arc");
    if (arcEl) arcEl.setAttribute("stroke-dasharray", `${dash} ${gap}`);
    const indEl = el.querySelector("#motion-ind");
    if (indEl) {
      const deg = -135 + normalValue * 270;
      indEl.style.transform = `rotate(${deg}deg)`;
    }
    return;
  }

  const circ  = isMini ? CIRCUMF_36 : CIRCUMF_80;
  const r     = isMini ? 14.5 : 32;
  const cx    = isMini ? 18 : 40;
  const filled = normalValue * ARC_FULL;
  const dash   = (filled / 360) * circ;
  const gap    = circ - dash;

  svg.innerHTML = `
    <defs>
      <radialGradient id="kb-${id}" cx="34%" cy="28%" r="78%">
        <stop offset="0%" stop-color="#d8c8f8" stop-opacity="0.38"/>
        <stop offset="28%" stop-color="#261a46" stop-opacity="0.5"/>
        <stop offset="80%" stop-color="#0d0920" stop-opacity="0.98"/>
        <stop offset="100%" stop-color="#080614" stop-opacity="1"/>
      </radialGradient>
      <linearGradient id="ka-${id}" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stop-color="#8E5BFF"/>
        <stop offset="100%" stop-color="#c48aff"/>
      </linearGradient>
    </defs>
    <!-- Track -->
    <circle cx="${cx}" cy="${cx}" r="${r}" fill="none"
            stroke="rgba(142,91,255,0.15)" stroke-width="${isMini?3:5}"
            stroke-dasharray="${(ARC_FULL/360)*circ} 9999"
            stroke-linecap="round"
            transform="rotate(${ARC_START} ${cx} ${cx})"/>
    <!-- Active arc -->
    <circle cx="${cx}" cy="${cx}" r="${r}" fill="none"
            stroke="url(#ka-${id})" stroke-width="${isMini?3:5}"
            stroke-dasharray="${dash} ${gap}"
            stroke-linecap="round"
            transform="rotate(${ARC_START} ${cx} ${cx})"
            id="arc-${id}"
            style="filter:drop-shadow(0 0 6px rgba(142,91,255,0.85));"/>
    <!-- Body -->
    <circle cx="${cx}" cy="${cx}" r="${isMini?11:27}" fill="url(#kb-${id})"
            stroke="rgba(220,200,255,0.18)" stroke-width="1"/>
    ${!isMini ? `<ellipse cx="${cx-8}" cy="${cx-14}" rx="16" ry="10" fill="rgba(255,255,255,0.18)"/>` : ""}
    <!-- Indicator -->
    <g id="ind-${id}" style="transform-origin:${cx}px ${cx}px;transform:rotate(${-ARC_START + normalValue*ARC_FULL}deg)">
      <rect x="${cx-1.5}" y="${isMini?3:8}" width="3" height="${isMini?5:9}" rx="1.5" fill="#c48aff"/>
    </g>
  `;
}

function updateKnobDisplay(param, val) {
  const el = document.getElementById(`${param}-knob`);
  if (!el) return;
  const n = val / 100;
  const isMini  = el.classList.contains("mini-knob");
  const isMotion = param === "motion";
  renderKnobSVG(el, n, isMini, isMotion);

  const valEl = document.getElementById(`${param}-value`);
  if (!valEl) return;

  // Format display value
  switch (param) {
    case "cutoff":
      valEl.textContent = n < 0.4 ? `${Math.round(20 + n*980)} Hz` : `${((20 + n * 19980)/1000).toFixed(1)} kHz`;
      break;
    case "decay":
      valEl.textContent = `${(0.1 + n * 9.9).toFixed(2)} s`;
      break;
    case "input": case "output":
      valEl.textContent = n < 0.5 ? `${((n-0.5)*24).toFixed(1)} dB` : `+${((n-0.5)*24).toFixed(1)} dB`;
      break;
    case "lfo_rate":
      { const rates = ["1/32","1/16","1/8","1/4","1/2","1","2","4"];
        valEl.textContent = rates[Math.round(n * (rates.length-1))]; break; }
    case "motion":
      valEl.textContent = `${Math.round(val)}%`;
      break;
    default:
      valEl.textContent = `${Math.round(val)}%`;
  }
}

function updateAllKnobs() {
  for (const [param, val] of Object.entries(params)) {
    updateKnobDisplay(param, val);
  }
  drawFilterCurve();
}

// ── Knob drag interaction ─────────────────────────────────────────────────────
function setupKnob(el) {
  let startY = 0, startVal = 0;
  const param = el.dataset.param;
  if (!param) return;

  function onMove(e) {
    const dy   = startY - (e.touches ? e.touches[0].clientY : e.clientY);
    const newVal = Math.max(0, Math.min(100, startVal + dy * 0.5));
    params[param] = newVal;
    updateKnobDisplay(param, newVal);
    sendToJuce(param, newVal / 100);
    if (param === "motion") applyMotionMacro(newVal);
  }

  function onUp() {
    window.removeEventListener("mousemove", onMove);
    window.removeEventListener("mouseup",   onUp);
    window.removeEventListener("touchmove", onMove);
    window.removeEventListener("touchend",  onUp);
  }

  function onDown(e) {
    e.preventDefault();
    startY   = e.touches ? e.touches[0].clientY : e.clientY;
    startVal = params[param] ?? 50;
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup",   onUp);
    window.addEventListener("touchmove", onMove, { passive: false });
    window.addEventListener("touchend",  onUp);
  }

  el.addEventListener("mousedown",  onDown);
  el.addEventListener("touchstart", onDown, { passive: false });
  el.addEventListener("dblclick", () => {
    params[param] = parseFloat(el.dataset.default) || 50;
    updateKnobDisplay(param, params[param]);
    sendToJuce(param, params[param] / 100);
  });
}

// ── MOTION macro ──────────────────────────────────────────────────────────────
function applyMotionMacro(v) {
  const n = v / 100;
  params.cutoff      = Math.max(0, Math.min(100, 80 - n * 60));
  params.resonance   = Math.min(100, 20 + n * 55);
  params.drive       = Math.min(100, n * 45);
  params.delay_mix   = Math.min(100, 10 + n * 60);
  params.feedback    = Math.min(100, 20 + n * 55);
  params.reverb_mix  = Math.min(100, 15 + n * 65);
  updateAllKnobs();
  drawFilterCurve();
}

// ── Filter curve ──────────────────────────────────────────────────────────────
let filterCanvas, filterCtx;

function drawFilterCurve() {
  if (!filterCanvas) return;
  const W = filterCanvas.width, H = filterCanvas.height;
  filterCtx.clearRect(0, 0, W, H);

  const cutoff   = params.cutoff / 100;
  const resonance = params.resonance / 100;
  const drive    = params.drive / 100;

  // Background grid
  filterCtx.strokeStyle = "rgba(142,91,255,0.07)";
  filterCtx.lineWidth = 1;
  for (let x = 0; x < W; x += W/6) {
    filterCtx.beginPath(); filterCtx.moveTo(x, 0); filterCtx.lineTo(x, H); filterCtx.stroke();
  }

  // Curve
  const grad = filterCtx.createLinearGradient(0, 0, W, 0);
  grad.addColorStop(0,   "rgba(142,91,255,0.9)");
  grad.addColorStop(0.5, "rgba(180,138,255,0.9)");
  grad.addColorStop(1,   "rgba(142,91,255,0.6)");

  filterCtx.beginPath();
  filterCtx.strokeStyle = grad;
  filterCtx.lineWidth = 2;
  filterCtx.shadowColor = "rgba(142,91,255,0.7)";
  filterCtx.shadowBlur = 8;

  const cutX = cutoff * W;
  const resonH = H * 0.12 + resonance * H * 0.28;
  const driveBoost = 1 + drive * 0.4;

  for (let px = 0; px <= W; px++) {
    const t = px / W;
    let y;
    if (t < cutoff) {
      // passband — slightly above center
      y = H * 0.35 - (t / cutoff) * drive * H * 0.1;
    } else if (Math.abs(t - cutoff) < 0.04) {
      // resonance peak
      const rel = (t - cutoff) / 0.04;
      y = H * 0.35 - resonance * H * 0.45 * Math.exp(-rel * rel * 12) * driveBoost;
    } else {
      // rolloff
      const rolloff = (t - cutoff) / (1 - cutoff + 0.001);
      y = H * 0.35 + rolloff * rolloff * H * 0.55;
    }
    y = Math.max(2, Math.min(H - 2, y));
    px === 0 ? filterCtx.moveTo(px, y) : filterCtx.lineTo(px, y);
  }
  filterCtx.stroke();
  filterCtx.shadowBlur = 0;

  // Fill under curve
  filterCtx.lineTo(W, H); filterCtx.lineTo(0, H);
  filterCtx.closePath();
  const fillGrad = filterCtx.createLinearGradient(0, 0, 0, H);
  fillGrad.addColorStop(0, "rgba(142,91,255,0.12)");
  fillGrad.addColorStop(1, "rgba(142,91,255,0)");
  filterCtx.fillStyle = fillGrad;
  filterCtx.fill();
}

// ── Delay waveform animation ──────────────────────────────────────────────────
let delayCanvas, delayCtx, delayPhase = 0;

function drawDelayWave() {
  if (!delayCanvas) return;
  const W = delayCanvas.width, H = delayCanvas.height;
  delayCtx.clearRect(0, 0, W, H);

  const feedback = params.feedback / 100;
  const mix      = params.delay_mix / 100;
  const numEchos = Math.max(1, Math.round(feedback * 6));

  delayPhase += 0.018;

  for (let e = 0; e < numEchos; e++) {
    const amp    = mix * Math.pow(feedback, e) * H * 0.38;
    const offset = (e / numEchos) * W * 0.7;
    const alpha  = (1 - e / numEchos) * 0.8;

    delayCtx.beginPath();
    delayCtx.strokeStyle = `rgba(142,91,255,${alpha})`;
    delayCtx.lineWidth   = Math.max(0.6, 2 - e * 0.4);
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

  requestAnimationFrame(drawDelayWave);
}

// ── LFO waveform animation ────────────────────────────────────────────────────
let lfoCanvas, lfoCtx, lfoPhase = 0;

function drawLFO() {
  if (!lfoCanvas) return;
  const W = lfoCanvas.width, H = lfoCanvas.height;
  lfoCtx.clearRect(0, 0, W, H);

  const rate  = params.lfo_rate / 100;
  const depth = params.lfo_depth / 100;
  lfoPhase += 0.03 + rate * 0.08;

  lfoCtx.beginPath();
  lfoCtx.strokeStyle = "rgba(142,91,255,0.8)";
  lfoCtx.lineWidth = 1.5;
  lfoCtx.shadowColor = "rgba(142,91,255,0.5)";
  lfoCtx.shadowBlur = 4;

  for (let px = 0; px <= W; px++) {
    const t = (px / W) * Math.PI * 4;
    const y = H / 2 + Math.sin(t + lfoPhase) * depth * H * 0.42;
    px === 0 ? lfoCtx.moveTo(px, y) : lfoCtx.lineTo(px, y);
  }
  lfoCtx.stroke();
  lfoCtx.shadowBlur = 0;

  requestAnimationFrame(drawLFO);
}

// ── Reverb EQ ─────────────────────────────────────────────────────────────────
let reverbEqCanvas, reverbEqCtx;
const eqNodes = [
  { freq: 0.08, gain: -0.4, id: "lowCut" },
  { freq: 0.18, gain: 0.1,  id: "lowShelf" },
  { freq: 0.55, gain: 0.35, id: "highShelf" },
  { freq: 0.82, gain: 0.4,  id: "highCut" },
];
let draggingNode = null;

function drawReverbEQ() {
  if (!reverbEqCanvas) return;
  const W = reverbEqCanvas.width, H = reverbEqCanvas.height;
  reverbEqCtx.clearRect(0, 0, W, H);

  // Grid
  reverbEqCtx.strokeStyle = "rgba(142,91,255,0.06)";
  reverbEqCtx.lineWidth = 1;
  [0.25, 0.5, 0.75].forEach(t => {
    reverbEqCtx.beginPath(); reverbEqCtx.moveTo(t*W,0); reverbEqCtx.lineTo(t*W,H); reverbEqCtx.stroke();
    reverbEqCtx.beginPath(); reverbEqCtx.moveTo(0,t*H); reverbEqCtx.lineTo(W,t*H); reverbEqCtx.stroke();
  });

  // Curve
  reverbEqCtx.beginPath();
  reverbEqCtx.strokeStyle = "rgba(142,91,255,0.85)";
  reverbEqCtx.lineWidth = 1.8;
  reverbEqCtx.shadowColor = "rgba(142,91,255,0.6)";
  reverbEqCtx.shadowBlur = 6;

  for (let px = 0; px <= W; px++) {
    const t = px / W;
    let y = H * 0.5;
    for (const node of eqNodes) {
      const dist = Math.abs(t - node.freq);
      y -= node.gain * H * 0.35 * Math.exp(-dist * dist * 80);
    }
    y = Math.max(2, Math.min(H - 2, y));
    px === 0 ? reverbEqCtx.moveTo(px, y) : reverbEqCtx.lineTo(px, y);
  }
  reverbEqCtx.stroke();
  reverbEqCtx.shadowBlur = 0;

  // Fill
  reverbEqCtx.lineTo(W, H); reverbEqCtx.lineTo(0, H); reverbEqCtx.closePath();
  const fillG = reverbEqCtx.createLinearGradient(0, 0, 0, H);
  fillG.addColorStop(0, "rgba(142,91,255,0.1)"); fillG.addColorStop(1, "rgba(142,91,255,0)");
  reverbEqCtx.fillStyle = fillG; reverbEqCtx.fill();

  // Nodes
  for (const node of eqNodes) {
    const nx = node.freq * W;
    const ny = H * 0.5 - node.gain * H * 0.35;
    reverbEqCtx.beginPath();
    reverbEqCtx.arc(nx, Math.max(6, Math.min(H-6, ny)), 5, 0, Math.PI * 2);
    reverbEqCtx.fillStyle = "#8E5BFF";
    reverbEqCtx.shadowColor = "rgba(142,91,255,0.9)";
    reverbEqCtx.shadowBlur = 10;
    reverbEqCtx.fill();
    reverbEqCtx.shadowBlur = 0;
    reverbEqCtx.strokeStyle = "rgba(255,255,255,0.6)";
    reverbEqCtx.lineWidth = 1;
    reverbEqCtx.stroke();
  }
}

function setupReverbEQDrag() {
  if (!reverbEqCanvas) return;
  const rect = () => reverbEqCanvas.getBoundingClientRect();

  function hitNode(cx, cy) {
    const r = rect();
    const W = reverbEqCanvas.width, H = reverbEqCanvas.height;
    const px = (cx - r.left) * (W / r.width);
    const py = (cy - r.top) * (H / r.height);
    return eqNodes.find(n => {
      const nx = n.freq * W;
      const ny = H * 0.5 - n.gain * H * 0.35;
      return Math.hypot(px - nx, py - ny) < 14;
    }) || null;
  }

  reverbEqCanvas.addEventListener("mousedown", e => {
    draggingNode = hitNode(e.clientX, e.clientY);
  });
  window.addEventListener("mousemove", e => {
    if (!draggingNode) return;
    const r = rect();
    const W = reverbEqCanvas.width, H = reverbEqCanvas.height;
    draggingNode.freq = Math.max(0.02, Math.min(0.98, (e.clientX - r.left) * (W / r.width) / W));
    draggingNode.gain = Math.max(-1, Math.min(1, -(e.clientY - r.top - r.height/2) / (r.height * 0.35)));
    drawReverbEQ();
  });
  window.addEventListener("mouseup", () => { draggingNode = null; });
}

// ── Meter simulation ──────────────────────────────────────────────────────────
let meterL = 0, meterR = 0;
function animateMeter() {
  meterL = meterL * 0.88 + (0.3 + Math.random() * 0.6) * (params.mix / 100) * 0.12;
  meterR = meterR * 0.88 + (0.3 + Math.random() * 0.6) * (params.mix / 100) * 0.12;
  const elL = document.getElementById("meter-l");
  const elR = document.getElementById("meter-r");
  if (elL) elL.style.height = `${Math.min(100, meterL * 100)}%`;
  if (elR) elR.style.height = `${Math.min(100, meterR * 100)}%`;
  requestAnimationFrame(animateMeter);
}

// ── Toggle buttons ────────────────────────────────────────────────────────────
function setupToggle(id) {
  const el = document.getElementById(id);
  if (!el) return;
  el.addEventListener("click", () => el.classList.toggle("on"));
}

// ── Pill buttons ─────────────────────────────────────────────────────────────
function setupPills(selector, activeClass) {
  document.querySelectorAll(selector).forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(selector).forEach(b => b.classList.remove(activeClass));
      btn.classList.add(activeClass);
    });
  });
}

// ── Canvas resize helper ──────────────────────────────────────────────────────
function fitCanvas(canvas) {
  const parent = canvas.parentElement;
  const rect   = parent.getBoundingClientRect();
  canvas.width  = Math.round(rect.width  * window.devicePixelRatio || rect.width);
  canvas.height = Math.round(rect.height * window.devicePixelRatio || rect.height);
  const ctx = canvas.getContext("2d");
  ctx.scale(window.devicePixelRatio || 1, window.devicePixelRatio || 1);
  return ctx;
}

// ── Init ──────────────────────────────────────────────────────────────────────
window.addEventListener("DOMContentLoaded", () => {
  // Canvases
  const filterEl   = document.getElementById("filter-canvas");
  const delayEl    = document.getElementById("delay-canvas");
  const lfoEl      = document.getElementById("lfo-canvas");
  const reverbEqEl = document.getElementById("reverb-eq-canvas");

  if (filterEl)   { filterCanvas = filterEl;   filterCtx   = fitCanvas(filterEl); }
  if (delayEl)    { delayCanvas  = delayEl;    delayCtx    = fitCanvas(delayEl); }
  if (lfoEl)      { lfoCanvas    = lfoEl;      lfoCtx      = fitCanvas(lfoEl); }
  if (reverbEqEl) { reverbEqCanvas = reverbEqEl; reverbEqCtx = fitCanvas(reverbEqEl); }

  // Knobs
  document.querySelectorAll(".knob, .mini-knob, .motion-knob").forEach(setupKnob);

  // Toggles
  setupToggle("filter-toggle");
  setupToggle("delay-toggle");
  setupToggle("reverb-toggle");

  // Filter type pills
  setupPills(".ftype", "active");
  setupPills(".dmode", "active");
  setupPills(".rtype", "active");

  // Delay sync
  document.getElementById("delay-sync")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("active");
  });

  // Freeze
  document.getElementById("freeze-btn")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("active");
  });

  // HQ
  document.getElementById("hq-btn")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("on");
    e.currentTarget.textContent = e.currentTarget.classList.contains("on") ? "ON" : "OFF";
  });

  // Power
  document.getElementById("power-btn")?.addEventListener("click", e => {
    e.currentTarget.classList.toggle("on");
  });

  // Initial render
  updateAllKnobs();
  drawFilterCurve();
  setupReverbEQDrag();
  drawReverbEQ();
  drawDelayWave();
  drawLFO();
  animateMeter();
});

// JUCE param update from host
if (juceBackend) {
  try {
    juceBackend.backend.addEventListener("paramUpdate", e => {
      const { param, value } = JSON.parse(e.data);
      if (param in params) {
        params[param] = value * 100;
        updateKnobDisplay(param, params[param]);
      }
    });
  } catch (_) {}
}
