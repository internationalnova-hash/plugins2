import * as Juce from "./juce/index.js";

// ── Preset Bank ───────────────────────────────────────────────────────────────
// morph/texture/form/air: 0–10 | blend: 0–100 | mode: 0=Clean 1=Digital 2=Hybrid 3=Extreme
const PRESETS = [
  // ── Lead ──────────────────────────────────────────────────────────────────
  { name: "Clean Lift",       category: "Lead",         tags: ["Airy","Clean","Polished"],        morph: 3.0, texture: 2.0, form: 5.0, air: 7.0, blend: 35, mode: 0 },
  { name: "Velvet Nights",    category: "Lead",         tags: ["Smooth","Warm","Dark"],           morph: 4.0, texture: 3.5, form: 5.5, air: 5.0, blend: 45, mode: 2 },
  { name: "Midnight Glow",    category: "Lead",         tags: ["Glossy","Airy","Emotional"],      morph: 5.5, texture: 4.0, form: 5.5, air: 6.0, blend: 50, mode: 2 },
  { name: "Topline Polish",   category: "Lead",         tags: ["Bright","Airy","Polished"],       morph: 3.5, texture: 2.5, form: 5.0, air: 8.0, blend: 30, mode: 0 },
  { name: "Glass Pop",        category: "Lead",         tags: ["Glossy","Airy","Futuristic"],     morph: 4.0, texture: 2.0, form: 4.5, air: 8.5, blend: 35, mode: 1 },
  { name: "Future Pop",       category: "Lead",         tags: ["Futuristic","Wide","Glossy"],     morph: 6.0, texture: 4.5, form: 5.0, air: 7.0, blend: 55, mode: 1 },

  // ── Adlibs ────────────────────────────────────────────────────────────────
  { name: "Space Adlib",      category: "Adlibs",       tags: ["Futuristic","Airy","Wide"],       morph: 7.0, texture: 5.0, form: 6.0, air: 6.5, blend: 65, mode: 1 },
  { name: "Adlib Shine",      category: "Adlibs",       tags: ["Airy","Glossy","Bright"],         morph: 5.0, texture: 3.0, form: 5.0, air: 8.0, blend: 45, mode: 2 },
  { name: "Astro Tone",       category: "Adlibs",       tags: ["Futuristic","Robotic","Wide"],    morph: 7.5, texture: 5.5, form: 6.5, air: 6.0, blend: 60, mode: 1 },
  { name: "Ghost Double",     category: "Adlibs",       tags: ["Wide","Smooth","Ethereal"],       morph: 4.0, texture: 3.0, form: 5.0, air: 5.0, blend: 35, mode: 2 },
  { name: "Neon Character",   category: "Adlibs",       tags: ["Futuristic","Synthetic","Aggressive"], morph: 6.5, texture: 5.0, form: 5.5, air: 7.0, blend: 55, mode: 1 },

  // ── Stacks ────────────────────────────────────────────────────────────────
  { name: "Wide Stack",       category: "Stacks",       tags: ["Wide","Smooth","Warm"],           morph: 3.0, texture: 4.0, form: 5.0, air: 5.0, blend: 40, mode: 2 },
  { name: "Soft Layer",       category: "Stacks",       tags: ["Smooth","Airy","Clean"],          morph: 2.5, texture: 2.0, form: 5.0, air: 5.5, blend: 30, mode: 0 },
  { name: "Soft Double",      category: "Stacks",       tags: ["Smooth","Clean","Wide"],          morph: 3.0, texture: 2.5, form: 5.0, air: 4.5, blend: 35, mode: 0 },
  { name: "Ghost Layer",      category: "Stacks",       tags: ["Ethereal","Airy","Wide"],         morph: 4.5, texture: 3.0, form: 5.5, air: 6.0, blend: 40, mode: 2 },
  { name: "Dream Vocal",      category: "Stacks",       tags: ["Ethereal","Airy","Emotional"],    morph: 5.0, texture: 3.5, form: 5.0, air: 7.0, blend: 45, mode: 2 },

  // ── FX ────────────────────────────────────────────────────────────────────
  { name: "Alien Lead",       category: "FX",           tags: ["Robotic","Aggressive","Futuristic"], morph: 8.5, texture: 7.0, form: 7.0, air: 5.0, blend: 75, mode: 3 },
  { name: "Hyper Voice",      category: "FX",           tags: ["Aggressive","Synthetic","Futuristic"], morph: 8.0, texture: 8.0, form: 5.5, air: 7.0, blend: 80, mode: 3 },
  { name: "Bubble Tone",      category: "FX",           tags: ["Synthetic","Futuristic","Wide"],  morph: 7.0, texture: 7.5, form: 4.5, air: 8.0, blend: 70, mode: 3 },
  { name: "Digital Angel",    category: "FX",           tags: ["Futuristic","Glossy","Airy"],     morph: 7.5, texture: 5.0, form: 5.0, air: 8.5, blend: 65, mode: 1 },
  { name: "Ethereal FX",      category: "FX",           tags: ["Ethereal","Airy","Emotional"],    morph: 6.5, texture: 5.0, form: 6.0, air: 8.0, blend: 60, mode: 2 },

  // ── Experimental ──────────────────────────────────────────────────────────
  { name: "Deep Form",        category: "Experimental", tags: ["Dark","Aggressive","Robotic"],    morph: 6.0, texture: 6.0, form: 2.0, air: 4.0, blend: 65, mode: 3 },
  { name: "Dark Bounce",      category: "Experimental", tags: ["Dark","Aggressive","Synthetic"],  morph: 7.0, texture: 6.5, form: 3.0, air: 4.5, blend: 70, mode: 3 },
  { name: "Alt Texture",      category: "Experimental", tags: ["Synthetic","Aggressive","Wide"],  morph: 5.5, texture: 8.0, form: 5.5, air: 5.0, blend: 60, mode: 1 },
  { name: "Energy Mode",      category: "Experimental", tags: ["Aggressive","Wide","Synthetic"],  morph: 8.0, texture: 7.0, form: 5.0, air: 6.0, blend: 75, mode: 3 },
  { name: "Velvet Robot",     category: "Experimental", tags: ["Robotic","Dark","Synthetic"],     morph: 6.5, texture: 7.5, form: 4.0, air: 5.5, blend: 65, mode: 1 },

  // ── Signature ─────────────────────────────────────────────────────────────
  { name: "International Nova", category: "Signature",  tags: ["Glossy","Airy","Wide"],          morph: 5.5, texture: 4.5, form: 5.5, air: 7.0, blend: 55, mode: 2 },
  { name: "Nova Signature",   category: "Signature",    tags: ["Airy","Glossy","Emotional"],      morph: 5.0, texture: 4.0, form: 5.5, air: 7.5, blend: 50, mode: 2 },
  { name: "Vocal Glow",       category: "Signature",    tags: ["Glossy","Airy","Smooth"],         morph: 4.5, texture: 3.5, form: 5.0, air: 8.0, blend: 45, mode: 2 },
  { name: "Future Nova",      category: "Signature",    tags: ["Futuristic","Glossy","Wide"],     morph: 6.5, texture: 5.0, form: 5.0, air: 7.0, blend: 60, mode: 1 },
  { name: "Silk Voice",       category: "Signature",    tags: ["Smooth","Airy","Clean"],          morph: 3.5, texture: 3.0, form: 5.0, air: 6.5, blend: 40, mode: 0 },
];

const KNOB_PARAMS = ["morph", "texture", "form", "air", "blend"];
const ALL_PARAMS  = [...KNOB_PARAMS, "voice_mode"];

// ── State ─────────────────────────────────────────────────────────────────────
const currentValues = { morph: 5.0, texture: 4.0, form: 5.5, air: 5.0, blend: 55, voice_mode: 2 };
const parameterStates = {};
let juceAvailable    = false;
let activeDrag       = null;
let isApplyingPreset = false;
let currentCategory  = "Lead";
let currentPresetIdx = 0;

// ── Normalization ─────────────────────────────────────────────────────────────
function normalize(id, value) {
  if (id === "blend")      return value / 100;
  if (id === "voice_mode") return value / 3;
  return value / 10;
}

function denormalize(id, nv) {
  const c = Math.max(0, Math.min(1, nv));
  if (id === "blend")      return c * 100;
  if (id === "voice_mode") return Math.round(c * 3);
  return c * 10;
}

// ── Knob visual ───────────────────────────────────────────────────────────────
const TOTAL_ARC = 125.7; // 240° of a r=30 circle (2*PI*30 * 240/360)

function updateKnob(id, value) {
  currentValues[id] = value;
  const col = document.querySelector(`.knob-col[data-param="${id}"]`);
  if (!col) return;

  const max  = id === "blend" ? 100 : 10;
  const frac = Math.max(0, Math.min(1, value / max));

  const fill  = col.querySelector(".knob-fill");
  const dot   = col.querySelector(".knob-indicator");
  const label = col.querySelector(".knob-value");

  if (fill)  fill.setAttribute("stroke-dasharray", `${frac * TOTAL_ARC} 188.5`);
  if (dot)   dot.style.transform = `rotate(${-120 + frac * 240}deg)`;
  if (label) label.textContent = id === "blend" ? Math.round(value) + "%" : value.toFixed(1);
}

// ── Mode visual ───────────────────────────────────────────────────────────────
function updateMode(modeIdx) {
  currentValues.voice_mode = modeIdx;
  document.querySelectorAll(".mode-btn").forEach(btn => {
    btn.classList.toggle("active", parseInt(btn.dataset.mode, 10) === modeIdx);
  });
}

// ── Push to JUCE ─────────────────────────────────────────────────────────────
function pushParam(id, value) {
  const state = parameterStates[id];
  if (state) state.setNormalisedValue(normalize(id, value));
}

// ── Preset browser helpers ────────────────────────────────────────────────────
function getFilteredPresets(category) {
  return category === "All" ? PRESETS : PRESETS.filter(p => p.category === category);
}

function updateBrowserUI(preset) {
  const nameEl = document.getElementById("preset-name");
  const tagsEl = document.getElementById("preset-tags");
  if (nameEl) nameEl.textContent = preset.name;
  if (tagsEl) tagsEl.innerHTML = preset.tags.map(t => `<span class="tag">${t}</span>`).join("");
}

// ── Apply preset with smooth animation ───────────────────────────────────────
function applyPreset(preset, pushToPlugin = true, animate = true) {
  if (isApplyingPreset) return;

  updateBrowserUI(preset);

  const targets = { morph: preset.morph, texture: preset.texture, form: preset.form, air: preset.air, blend: preset.blend };

  if (!animate) {
    KNOB_PARAMS.forEach(id => { updateKnob(id, targets[id]); if (pushToPlugin) pushParam(id, targets[id]); });
    updateMode(preset.mode);
    if (pushToPlugin) pushParam("voice_mode", preset.mode);
    return;
  }

  isApplyingPreset = true;
  const duration  = 220;
  const startTime = performance.now();
  const starts    = Object.fromEntries(KNOB_PARAMS.map(id => [id, currentValues[id]]));

  function tick(now) {
    const t    = Math.min(1, (now - startTime) / duration);
    const ease = t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;

    KNOB_PARAMS.forEach(id => {
      const v = starts[id] + (targets[id] - starts[id]) * ease;
      updateKnob(id, v);
      if (pushToPlugin) pushParam(id, v);
    });

    if (t < 1) {
      requestAnimationFrame(tick);
    } else {
      KNOB_PARAMS.forEach(id => { updateKnob(id, targets[id]); if (pushToPlugin) pushParam(id, targets[id]); });
      updateMode(preset.mode);
      if (pushToPlugin) pushParam("voice_mode", preset.mode);
      isApplyingPreset = false;
    }
  }
  requestAnimationFrame(tick);
}

// ── Preset navigation ─────────────────────────────────────────────────────────
function navigatePreset(direction) {
  const list = getFilteredPresets(currentCategory);
  if (list.length === 0) return;
  currentPresetIdx = (currentPresetIdx + direction + list.length) % list.length;
  applyPreset(list[currentPresetIdx]);
}

// ── Bind preset browser controls ─────────────────────────────────────────────
function bindBrowser() {
  const sel  = document.getElementById("category-select");
  const prev = document.getElementById("prev-preset");
  const next = document.getElementById("next-preset");

  if (sel) {
    sel.addEventListener("change", () => {
      currentCategory  = sel.value;
      currentPresetIdx = 0;
      const list = getFilteredPresets(currentCategory);
      if (list.length > 0) applyPreset(list[0]);
    });
  }
  if (prev) prev.addEventListener("click", () => navigatePreset(-1));
  if (next) next.addEventListener("click", () => navigatePreset(1));
}

// ── Knob drag ─────────────────────────────────────────────────────────────────
function clientY(e) {
  if (e.touches && e.touches.length)               return e.touches[0].clientY;
  if (e.changedTouches && e.changedTouches.length) return e.changedTouches[0].clientY;
  return typeof e.clientY === "number" ? e.clientY : 0;
}

function bindKnobs() {
  document.querySelectorAll(".knob-col[data-param]").forEach(col => {
    const id  = col.dataset.param;
    const min = parseFloat(col.dataset.min || "0");
    const max = parseFloat(col.dataset.max || "10");

    const onStart = e => {
      if (typeof e.button === "number" && e.button !== 0) return;
      e.preventDefault();
      col.classList.add("dragging");
      activeDrag = { id, min, max, startY: clientY(e), startVal: currentValues[id] };
      if (col.setPointerCapture && e.pointerId != null) {
        try { col.setPointerCapture(e.pointerId); } catch (_) {}
      }
      parameterStates[id]?.sliderDragStarted?.();
    };

    col.addEventListener("pointerdown", onStart);
    col.addEventListener("mousedown",   onStart);
    col.addEventListener("touchstart",  onStart, { passive: false });

    col.addEventListener("dblclick", () => {
      const list   = getFilteredPresets(currentCategory);
      const preset = list[currentPresetIdx];
      const def    = preset ? preset[id] : (id === "blend" ? 55 : 5);
      updateKnob(id, def);
      pushParam(id, def);
    });
  });

  const onMove = e => {
    if (!activeDrag) return;
    e.preventDefault();
    const { id, min, max, startY, startVal } = activeDrag;
    const v = Math.max(min, Math.min(max, startVal + (startY - clientY(e)) * (max - min) * 0.005));
    updateKnob(id, v);
    pushParam(id, v);
  };

  const onEnd = () => {
    if (!activeDrag) return;
    document.querySelector(`.knob-col[data-param="${activeDrag.id}"]`)?.classList.remove("dragging");
    parameterStates[activeDrag.id]?.sliderDragEnded?.();
    activeDrag = null;
  };

  window.addEventListener("pointermove",  onMove, { passive: false });
  window.addEventListener("mousemove",    onMove, { passive: false });
  window.addEventListener("touchmove",    onMove, { passive: false });
  window.addEventListener("pointerup",    onEnd);
  window.addEventListener("mouseup",      onEnd);
  window.addEventListener("touchend",     onEnd);
  window.addEventListener("touchcancel",  onEnd);
}

// ── Mode buttons ──────────────────────────────────────────────────────────────
function bindModeButtons() {
  document.querySelectorAll(".mode-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const m = parseInt(btn.dataset.mode, 10);
      updateMode(m);
      pushParam("voice_mode", m);
    });
  });
}

// ── JUCE parameter connection ─────────────────────────────────────────────────
function connectParameters() {
  if (!juceAvailable) return;

  ALL_PARAMS.forEach(id => {
    try { parameterStates[id] = Juce.getSliderState(id); } catch (_) {}
  });

  KNOB_PARAMS.forEach(id => {
    const state = parameterStates[id];
    if (!state) return;
    state.addValueChangedListener(() => {
      if (isApplyingPreset) return;
      updateKnob(id, denormalize(id, state.getNormalisedValue()));
    });
    updateKnob(id, denormalize(id, state.getNormalisedValue()));
  });

  const modeState = parameterStates["voice_mode"];
  if (modeState) {
    modeState.addValueChangedListener(() => {
      if (isApplyingPreset) return;
      updateMode(denormalize("voice_mode", modeState.getNormalisedValue()));
    });
    updateMode(denormalize("voice_mode", modeState.getNormalisedValue()));
  }
}

// ── VU Meters ─────────────────────────────────────────────────────────────────
const METER_SEGS     = 20;
let   inputPeakSmooth  = 0;
let   outputPeakSmooth = 0;

function buildMeter(id) {
  const el = document.getElementById(id);
  if (!el) return;
  el.innerHTML = "";
  for (let i = 0; i < METER_SEGS; i++) {
    const seg = document.createElement("div");
    seg.className = "meter-seg";
    el.appendChild(seg);
  }
}

function updateMeterEl(id, level) {
  const el = document.getElementById(id);
  if (!el) return;
  const segs  = el.querySelectorAll(".meter-seg");
  const count = Math.round(Math.max(0, Math.min(1, level)) * METER_SEGS);
  segs.forEach((seg, i) => {
    seg.classList.toggle("active", i < count);
    seg.classList.toggle("hot",    i < count && i >= METER_SEGS - 2);
  });
}

// ── Spectrum data (from C++ timer) ───────────────────────────────────────────
const BINS            = 96;
const smoothedInput     = new Float32Array(BINS);
const smoothedProblem   = new Float32Array(BINS);
const smoothedReduction = new Float32Array(BINS);

window.updateVoiceSpectrum = (input, problem, reduction) => {
  let sum = 0;
  for (let i = 0; i < BINS; i++) {
    smoothedInput[i]     = smoothedInput[i]     * 0.80 + (input[i]     || 0) * 0.20;
    smoothedProblem[i]   = smoothedProblem[i]   * 0.80 + (problem[i]   || 0) * 0.20;
    smoothedReduction[i] = smoothedReduction[i] * 0.80 + (reduction[i] || 0) * 0.20;
    sum += smoothedInput[i];
  }
  inputPeakSmooth = inputPeakSmooth * 0.88 + (sum / BINS) * 2.4 * 0.12;
};

window.updateOutputPeak = peak => {
  outputPeakSmooth = Math.max(outputPeakSmooth * 0.90, peak);
};

// ── Canvas / Visualizer ───────────────────────────────────────────────────────
let canvas, ctx;

function drawFrame() {
  requestAnimationFrame(drawFrame);
  if (!ctx) return;

  const W = canvas.width;
  const H = canvas.height;
  if (W === 0 || H === 0) return;

  const morphN   = currentValues.morph   / 10;
  const textureN = currentValues.texture / 10;
  const airN     = currentValues.air     / 10;

  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#080810";
  ctx.fillRect(0, 0, W, H);

  // Subtle horizontal grid
  ctx.strokeStyle = "rgba(255,255,255,0.035)";
  ctx.lineWidth   = 1;
  for (let i = 1; i < 4; i++) {
    const y = Math.round(H * (i / 4)) + 0.5;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  }

  // ── Spectrum fill ─────────────────────────────────────────────────────────
  const fillGrad = ctx.createLinearGradient(0, 0, W, 0);
  fillGrad.addColorStop(0,    `rgba(122, 92, 255, ${0.38 + morphN * 0.32})`);
  fillGrad.addColorStop(0.45, `rgba(60, 180, 255, ${0.32 + textureN * 0.22})`);
  fillGrad.addColorStop(1,    `rgba(0, 212, 255,  ${0.35 + airN * 0.25})`);

  ctx.fillStyle = fillGrad;
  ctx.beginPath();
  ctx.moveTo(0, H);
  for (let b = 0; b < BINS; b++) {
    const x = (b / (BINS - 1)) * W;
    const y = H - smoothedInput[b] * H * (0.86 + morphN * 0.12);
    b === 0 ? ctx.lineTo(x, y) : ctx.lineTo(x, y);
  }
  ctx.lineTo(W, H);
  ctx.closePath();
  ctx.fill();

  // ── Glowing top edge ──────────────────────────────────────────────────────
  const lineGrad = ctx.createLinearGradient(0, 0, W, 0);
  lineGrad.addColorStop(0,   `rgba(140, 100, 255, ${0.6 + morphN * 0.28})`);
  lineGrad.addColorStop(0.5, `rgba(80,  200, 255, ${0.6 + textureN * 0.22})`);
  lineGrad.addColorStop(1,   `rgba(0,   220, 255, ${0.6 + airN * 0.22})`);

  ctx.strokeStyle = lineGrad;
  ctx.lineWidth   = 2;
  ctx.shadowBlur  = 10 + morphN * 10;
  ctx.shadowColor = "#7A5CFF";
  ctx.beginPath();
  for (let b = 0; b < BINS; b++) {
    const x = (b / (BINS - 1)) * W;
    const y = H - smoothedInput[b] * H * (0.86 + morphN * 0.12);
    b === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  }
  ctx.stroke();
  ctx.shadowBlur = 0;

  // ── Reduction accent ──────────────────────────────────────────────────────
  if (smoothedReduction.some(v => v > 0.02)) {
    ctx.fillStyle = `rgba(255, 70, 120, ${0.12 + textureN * 0.10})`;
    ctx.beginPath();
    ctx.moveTo(0, H);
    for (let b = 0; b < BINS; b++) {
      const x = (b / (BINS - 1)) * W;
      const y = H - smoothedReduction[b] * H * 0.55;
      b === 0 ? ctx.lineTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.lineTo(W, H);
    ctx.closePath();
    ctx.fill();
  }

  // Update VU meters
  updateMeterEl("input-meter",  inputPeakSmooth);
  updateMeterEl("output-meter", outputPeakSmooth);
  outputPeakSmooth *= 0.978;
}

function initCanvas() {
  canvas = document.getElementById("voice-canvas");
  if (!canvas) return;
  ctx = canvas.getContext("2d");

  const resize = () => {
    const wrap = canvas.parentElement;
    if (!wrap) return;
    const r = wrap.getBoundingClientRect();
    if (r.width > 0 && r.height > 0) {
      canvas.width  = Math.round(r.width);
      canvas.height = Math.round(r.height);
    }
  };
  resize();
  new ResizeObserver(resize).observe(canvas.parentElement);
  drawFrame();
}

// ── Boot ──────────────────────────────────────────────────────────────────────
document.addEventListener("DOMContentLoaded", () => {
  juceAvailable = typeof window.__JUCE__ !== "undefined";

  buildMeter("input-meter");
  buildMeter("output-meter");
  initCanvas();
  bindBrowser();
  bindKnobs();
  bindModeButtons();
  connectParameters();

  // Default: Lead → Midnight Glow
  const sel = document.getElementById("category-select");
  if (sel) sel.value = "Lead";
  currentCategory = "Lead";

  const leadList   = getFilteredPresets("Lead");
  const defaultIdx = leadList.findIndex(p => p.name === "Midnight Glow");
  currentPresetIdx = defaultIdx >= 0 ? defaultIdx : 0;

  if (leadList.length > 0) {
    applyPreset(leadList[currentPresetIdx], false, false);
  }
});
