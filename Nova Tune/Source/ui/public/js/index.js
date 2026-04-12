import * as Juce from "./juce/index.js";

// ── Preset Bank ───────────────────────────────────────────────────────────────
// shift: -12..+12 st | mutate/glitch/gender/space: 0-10 | mix: 0-100 | mode: 0=Natural 1=Chrome 2=Warp 3=Monster 4=Android
const PRESETS = [
  { name: "Nova Prime",   category: "Signature",  tags: ["Signature","Mutate","Lead"],   pitch: 0.0, morph: 5.6, texture: 2.8, form: 5.4, air: 4.6, blend: 68, mode: 2 },
  { name: "Chrome Saint", category: "Chrome",     tags: ["Glossy","Shiny","Synthetic"], pitch: 3.0, morph: 4.8, texture: 3.5, form: 6.3, air: 5.0, blend: 72, mode: 1 },
  { name: "Lead Melt",    category: "Lead",       tags: ["Wide","Modern","Lead"],       pitch: 2.0, morph: 6.0, texture: 2.2, form: 4.8, air: 4.4, blend: 66, mode: 2 },
  { name: "Monster Vox",  category: "Creature",   tags: ["Huge","Dark","Monster"],      pitch: -5.0, morph: 7.8, texture: 4.6, form: 2.8, air: 2.5, blend: 84, mode: 3 },
  { name: "Android Air",  category: "Android",    tags: ["Robotic","Sharp","Future"],   pitch: 1.0, morph: 6.2, texture: 5.1, form: 6.8, air: 6.5, blend: 74, mode: 4 },
  { name: "Choir Glass",  category: "Choir",      tags: ["Layered","Wide","Dreamy"],    pitch: 5.0, morph: 4.2, texture: 1.6, form: 5.9, air: 5.8, blend: 64, mode: 0 },
  { name: "Warp Child",   category: "Warp",       tags: ["Bent","Elastic","Alien"],     pitch: 7.0, morph: 7.4, texture: 3.2, form: 7.3, air: 4.8, blend: 78, mode: 2 },
  { name: "Atmos Bloom",  category: "Atmosphere", tags: ["Airy","Float","Soft"],        pitch: 0.0, morph: 4.4, texture: 1.8, form: 6.1, air: 7.4, blend: 62, mode: 0 },
];

const KNOB_PARAMS = ["pitch", "morph", "texture", "form", "air", "blend"];
const ALL_PARAMS  = [...KNOB_PARAMS, "voice_mode"];

// ── State ─────────────────────────────────────────────────────────────────────
const currentValues = { pitch: 0.0, morph: 4.8, texture: 2.5, form: 5.0, air: 4.2, blend: 60, voice_mode: 2 };
const parameterStates = {};
let juceAvailable    = false;
let activeDrag       = null;
let isApplyingPreset = false;
let currentCategory  = "Signature";
let currentPresetIdx = 0;

function getParamRange(id) {
  if (id === "pitch") return { min: -12, max: 12 };
  if (id === "blend") return { min: 0, max: 100 };
  if (id === "voice_mode") return { min: 0, max: 4 };
  return { min: 0, max: 10 };
}

function createFallbackSliderState(id) {
  if (typeof window.__JUCE__ === "undefined" || !window.__JUCE__.backend) return null;

  const identifier = `__juce__slider${id}`;
  const { min, max } = getParamRange(id);
  let normalisedValue = normalize(id, currentValues[id] ?? min);

  const valueChangedListeners = [];

  window.__JUCE__.backend.addEventListener(identifier, (event) => {
    if (!event || event.eventType !== "valueChanged") return;
    const scaled = Number(event.value);
    const nv = (scaled - min) / (max - min);
    normalisedValue = Math.max(0, Math.min(1, Number.isFinite(nv) ? nv : normalisedValue));
    valueChangedListeners.forEach((fn) => fn());
  });

  return {
    setNormalisedValue(newValue) {
      normalisedValue = Math.max(0, Math.min(1, newValue));
      const scaled = min + normalisedValue * (max - min);
      window.__JUCE__.backend.emitEvent(identifier, {
        eventType: "valueChanged",
        value: scaled,
      });
    },
    getNormalisedValue() {
      return normalisedValue;
    },
    sliderDragStarted() {
      window.__JUCE__.backend.emitEvent(identifier, { eventType: "sliderDragStarted" });
    },
    sliderDragEnded() {
      window.__JUCE__.backend.emitEvent(identifier, { eventType: "sliderDragEnded" });
    },
    valueChangedEvent: {
      addListener(fn) {
        valueChangedListeners.push(fn);
      },
    },
    propertiesChangedEvent: {
      addListener() {},
    },
  };
}

// ── Normalization ─────────────────────────────────────────────────────────────
function normalize(id, value) {
  if (id === "pitch")      return (value + 12) / 24;
  if (id === "blend")      return value / 100;
  if (id === "voice_mode") return value / 4;
  return value / 10;
}

function denormalize(id, nv) {
  const c = Math.max(0, Math.min(1, nv));
  if (id === "pitch")      return -12 + c * 24;
  if (id === "blend")      return c * 100;
  if (id === "voice_mode") return Math.round(c * 4);
  return c * 10;
}

// ── Knob visual ───────────────────────────────────────────────────────────────
const TOTAL_ARC  = 159.2;  // 240° of r=38 circle  (2*PI*38 * 240/360)
const KNOB_CIRC  = 238.76; // full circumference of r=38 circle

// Cinematic visualizer state
const particles = [];
let   wavePhase  = 0;
let   lastSpectrumTick = 0;
let   transientPulse = 0;
let   loudnessMemory = 0;

function updateKnob(id, value) {
  currentValues[id] = value;
  const col = document.querySelector(`.knob-col[data-param="${id}"]`);
  if (!col) return;

  const min = Number.parseFloat(col.dataset.min || (id === "pitch" ? "-12" : "0"));
  const max = Number.parseFloat(col.dataset.max || (id === "blend" ? "100" : id === "pitch" ? "12" : "10"));
  const frac = Math.max(0, Math.min(1, (value - min) / (max - min)));

  const fill  = col.querySelector(".knob-fill");
  const dot   = col.querySelector(".knob-indicator");
  const label = col.querySelector(".knob-value");

  if (fill)  fill.setAttribute("stroke-dasharray", `${frac * TOTAL_ARC} ${KNOB_CIRC}`);
  if (dot)   dot.style.transform = `rotate(${-120 + frac * 240}deg)`;
  if (label)
  {
    if (id === "blend") label.textContent = Math.round(value) + "%";
    else if (id === "pitch") label.textContent = `${value >= 0 ? "+" : ""}${value.toFixed(1)} st`;
    else label.textContent = value.toFixed(1);
  }
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
  if (!state) return;
  try {
    state.setNormalisedValue(normalize(id, value));
  } catch (e) {
    console.warn("pushParam failed", id, e);
  }
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
function applyPreset(preset, pushToPlugin = true, animate = false) {
  if (isApplyingPreset) return;

  updateBrowserUI(preset);

  const targets = {
    pitch: preset.pitch ?? 0.0,
    morph: preset.morph,
    texture: preset.texture,
    form: preset.form,
    air: preset.air,
    blend: preset.blend,
  };

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
      const def    = preset ? (preset[id] ?? (id === "pitch" ? 0 : undefined)) : (id === "blend" ? 55 : id === "pitch" ? 0 : 5);
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

// ── Demo mode (generates synthetic spectrum) ─────────────────────────────────
let demoPhase = 0;
function demoSpectrum() {
  demoPhase += 0.05;
  const input = new Array(BINS).fill(0).map((_, b) => {
    const tNorm = b / BINS;
    // Fundamental + harmonics with formant peaks
    const fundamental = Math.pow(Math.sin(demoPhase * 0.5 + tNorm * 4) * 0.4 + 0.5, 2);
    const harmonic1 = Math.pow(Math.sin(demoPhase * 0.7 + tNorm * 8) * 0.3 + 0.3, 2);
    const harmonic2 = Math.pow(Math.sin(demoPhase * 1.1 + tNorm * 12) * 0.2 + 0.2, 2);
    const formant = Math.exp(-Math.pow((tNorm - (0.3 + Math.sin(demoPhase * 0.3) * 0.1)) * 3, 2)) * 0.6;
    // Clamp to [0,1] matching the C++ analyzer output range
    return Math.min(1, (fundamental + harmonic1 + harmonic2 + formant) * 0.55);
  });
  const problem = input.map((v, b) => {
    const tNorm = b / BINS;
    const harsh = Math.pow(Math.sin(demoPhase * 1.5 + tNorm * 16) * 0.5, 2) * (0.3 + 0.2 * Math.sin(demoPhase * 0.2));
    return Math.min(1, harsh * 0.85);
  });
  const reduction = input.map((v, b) => {
    const tNorm = b / BINS;
    return Math.max(0, Math.min(1, (Math.sin(tNorm * 8 + demoPhase) * 0.5 + 0.5) * 0.4));
  });
  window.updateVoiceSpectrum?.(input, problem, reduction);
}

// ── JUCE parameter connection ─────────────────────────────────────────────────
function connectParameters() {
  if (!juceAvailable && typeof window.__JUCE__ === "undefined") {
    // Demo mode: generate synthetic spectrum data
    setInterval(demoSpectrum, 30);
    return;
  }

  if (!juceAvailable) {
    console.warn("JUCE host detected but module bridge unavailable; using fallback parameter bridge.");
  }

  ALL_PARAMS.forEach(id => {
    try {
      if (juceAvailable && Juce?.getSliderState) {
        parameterStates[id] = Juce.getSliderState(id);
      }
    } catch (_) {}

    if (!parameterStates[id]) {
      parameterStates[id] = createFallbackSliderState(id);
    }
  });

  KNOB_PARAMS.forEach(id => {
    try {
      const state = parameterStates[id];
      if (!state) return;
      const onValueChanged = () => {
        if (isApplyingPreset) return;
        updateKnob(id, denormalize(id, state.getNormalisedValue()));
      };

      if (typeof state.addValueChangedListener === "function") {
        state.addValueChangedListener(onValueChanged);
      } else if (state.valueChangedEvent?.addListener) {
        state.valueChangedEvent.addListener(onValueChanged);
      }

      // Also sync once when properties arrive (covers initial value race)
      if (typeof state.addPropertiesChangedListener === "function") {
        state.addPropertiesChangedListener(() => {
          updateKnob(id, denormalize(id, state.getNormalisedValue()));
        });
      } else if (state.propertiesChangedEvent?.addListener) {
        state.propertiesChangedEvent.addListener(() => {
          updateKnob(id, denormalize(id, state.getNormalisedValue()));
        });
      }

      updateKnob(id, denormalize(id, state.getNormalisedValue()));
    } catch (e) {
      console.warn("Failed to connect param", id, e);
    }
  });

  try {
    const modeState = parameterStates["voice_mode"];
    if (modeState) {
      const onModeChanged = () => {
        if (isApplyingPreset) return;
        updateMode(denormalize("voice_mode", modeState.getNormalisedValue()));
      };

      if (typeof modeState.addValueChangedListener === "function") {
        modeState.addValueChangedListener(onModeChanged);
      } else if (modeState.valueChangedEvent?.addListener) {
        modeState.valueChangedEvent.addListener(onModeChanged);
      }

      updateMode(denormalize("voice_mode", modeState.getNormalisedValue()));
    }
  } catch (e) {
    console.warn("Failed to connect voice_mode", e);
  }
}

// ── VU Meters ─────────────────────────────────────────────────────────────────
const METER_SEGS = 26;
let   inputPeakSmooth  = 0;
let   outputPeakSmooth = 0;
let   inputPeakHold = 0;
let   outputPeakHold = 0;

function buildMeter(id) {
  const el = document.getElementById(id);
  if (!el) return;
  el.innerHTML = "";
  el.style.position = "relative";
  const mc = document.createElement("canvas");
  mc.id = id + "-canvas";
  mc.style.cssText = "position:absolute;inset:0;width:100%;height:100%;display:block;";
  el.appendChild(mc);
}

function updateMeterEl(id, level) {
  const mc = document.getElementById(id + '-canvas');
  if (!mc) return;
  const clamped = Math.max(0, Math.min(1, level));
  if (id === 'input-meter') {
    inputPeakHold = Math.max(inputPeakHold * 0.972, clamped);
  } else {
    outputPeakHold = Math.max(outputPeakHold * 0.974, clamped);
  }
  const hold   = id === 'input-meter' ? inputPeakHold : outputPeakHold;
  const parent = mc.parentElement;
  const cw = parent ? (parent.clientWidth  || 54) : 54;
  const ch = parent ? (parent.clientHeight || 200) : 200;
  if (mc.width !== cw || mc.height !== ch) { mc.width = cw; mc.height = ch; }

  const bctx   = mc.getContext('2d');
  bctx.clearRect(0, 0, cw, ch);

  const isInput = id === 'input-meter';
  const barW  = 7;
  const padT  = 10;
  const padB  = 16;
  const barH  = ch - padT - padB;
  const barX  = isInput ? cw - barW - 3 : 3;
  const fillH = barH * clamped;
  const fillY = padT + barH - fillH;

  // Track background
  bctx.fillStyle = 'rgba(14, 8, 40, 0.82)';
  bctx.fillRect(barX, padT, barW, barH);

  // Filled gradient (bottom=deep purple, top=bright cyan)
  if (fillH > 1) {
    const grad = bctx.createLinearGradient(0, fillY + fillH, 0, fillY);
    grad.addColorStop(0.00, 'rgba(55,  22, 145, 0.78)');
    grad.addColorStop(0.28, 'rgba(110, 42, 215, 0.90)');
    grad.addColorStop(0.58, 'rgba(168, 72, 255, 0.96)');
    grad.addColorStop(0.82, 'rgba(100, 192, 255, 0.98)');
    grad.addColorStop(1.00, 'rgba(0,   245, 255, 1.00)');
    bctx.shadowBlur  = 16;
    bctx.shadowColor = clamped > 0.72 ? 'rgba(60, 230, 255, 0.80)' : 'rgba(145, 72, 255, 0.65)';
    bctx.fillStyle   = grad;
    bctx.fillRect(barX, fillY, barW, fillH);
    bctx.shadowBlur  = 0;
    // Bright top-edge glow line
    bctx.strokeStyle = clamped > 0.70 ? 'rgba(0, 255, 255, 0.90)' : 'rgba(190, 145, 255, 0.80)';
    bctx.lineWidth   = 1.2;
    bctx.shadowBlur  = 12;
    bctx.shadowColor = bctx.strokeStyle;
    bctx.beginPath(); bctx.moveTo(barX, fillY); bctx.lineTo(barX + barW, fillY); bctx.stroke();
    bctx.shadowBlur  = 0;
  }

  // Peak hold tick
  if (hold > 0.04) {
    const holdY = padT + barH * (1 - hold);
    bctx.fillStyle   = 'rgba(235, 220, 255, 0.95)';
    bctx.shadowBlur  = 12;
    bctx.shadowColor = 'rgba(210, 235, 255, 0.92)';
    bctx.fillRect(barX - 1, holdY - 1, barW + 2, 2);
    bctx.shadowBlur  = 0;
  }

  // dB scale labels
  const dBMarks = [{ db: 0, lbl: '0' }, { db: -6, lbl: '-6' }, { db: -12, lbl: '-12' },
                   { db: -18, lbl: '-18' }, { db: -24, lbl: '-24' }, { db: -36, lbl: '-36' }];
  const dbFloor = 48;
  bctx.font = "7px 'SF Pro Display',Arial,sans-serif";
  dBMarks.forEach(({ db, lbl }) => {
    const y = padT + barH * (-db / dbFloor);
    if (y < padT || y > padT + barH) return;
    bctx.fillStyle = 'rgba(130, 120, 195, 0.28)';
    const tickX = isInput ? barX - 1 : barX + barW + 1;
    bctx.fillRect(isInput ? tickX - 4 : tickX, y - 0.5, 4, 1);
    bctx.fillStyle = 'rgba(158, 148, 220, 0.42)';
    bctx.textAlign = isInput ? 'right' : 'left';
    bctx.fillText(lbl, isInput ? barX - 7 : barX + barW + 7, y + 2.5);
  });
  // 'dB' footer
  bctx.fillStyle = 'rgba(130, 120, 195, 0.28)';
  bctx.font      = "6px 'SF Pro Display',Arial,sans-serif";
  bctx.textAlign = isInput ? 'right' : 'left';
  bctx.fillText('dB', isInput ? barX - 2 : barX + barW + 2, ch - 3);
}

// ── Spectrum data (from C++ timer) ───────────────────────────────────────────
const BINS            = 96;
const smoothedInput     = new Float32Array(BINS);
const smoothedProblem   = new Float32Array(BINS);
const smoothedReduction = new Float32Array(BINS);

window.updateVoiceSpectrum = (input, problem, reduction) => {
  lastSpectrumTick = performance.now();
  let sum = 0;
  for (let i = 0; i < BINS; i++) {
    smoothedInput[i]     = smoothedInput[i]     * 0.90 + (input[i]     || 0) * 0.10;
    smoothedProblem[i]   = smoothedProblem[i]   * 0.90 + (problem[i]   || 0) * 0.10;
    smoothedReduction[i] = smoothedReduction[i] * 0.90 + (reduction[i] || 0) * 0.10;
    sum += smoothedInput[i];
  }
  inputPeakSmooth = inputPeakSmooth * 0.88 + (sum / BINS) * 2.4 * 0.12;
};

window.updateOutputPeak = peak => {
  outputPeakSmooth = Math.max(outputPeakSmooth * 0.90, peak);
};

// ── Canvas / Visualizer ───────────────────────────────────────────────────────
let canvas, ctx;

// ── Wave drawing helpers ──────────────────────────────────────────────────────
function buildCenterWavePoints(data, baseY, amplitudePx, widthN, lowDrive, midDrive) {
  const pts = [];
  const W = canvas.width;
  const cx = W * 0.5;
  for (let b = 0; b < BINS; b++) {
    const t = b / (BINS - 1);
    const x0 = t * W;
    const x = cx + (x0 - cx) * (1 + widthN * 0.06);
    const flow = Math.sin(t * 6.8  + wavePhase * (0.72 + lowDrive * 0.55)) * amplitudePx * (0.11  + midDrive * 0.08)
               + Math.sin(t * 12.2 - wavePhase * 0.42)                      * amplitudePx * (0.028 + lowDrive * 0.024)
               + Math.sin(t * 3.4  + wavePhase * 0.38)                      * amplitudePx * (0.045 + lowDrive * 0.035);
    const y = baseY - data[b] * amplitudePx * (0.88 + midDrive * 0.36) + flow;
    pts.push({ x, y, e: data[b] });
  }
  return pts;
}

function traceSmoothPath(pts) {
  ctx.moveTo(pts[0].x, pts[0].y);
  for (let i = 0; i < pts.length - 2; i++) {
    const mx = (pts[i].x + pts[i + 1].x) / 2;
    const my = (pts[i].y + pts[i + 1].y) / 2;
    ctx.quadraticCurveTo(pts[i].x, pts[i].y, mx, my);
  }
  ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y);
}

function traceSmoothPathFromCurrent(pts) {
  for (let i = 0; i < pts.length - 2; i++) {
    const mx = (pts[i].x + pts[i + 1].x) / 2;
    const my = (pts[i].y + pts[i + 1].y) / 2;
    ctx.quadraticCurveTo(pts[i].x, pts[i].y, mx, my);
  }
  ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y);
}

function drawRibbon(centerPts, widthBase, grad, alpha) {
  const upper = [];
  const lower = [];
  for (let i = 0; i < centerPts.length; i++) {
    const p = centerPts[i];
    const half = widthBase * (0.35 + p.e * 1.10);
    upper.push({ x: p.x, y: p.y - half });
    lower.push({ x: p.x, y: p.y + half });
  }

  ctx.globalAlpha = alpha;
  ctx.fillStyle   = grad;
  const lowerRev = lower.slice().reverse();

  ctx.beginPath();
  ctx.moveTo(upper[0].x, upper[0].y);
  traceSmoothPathFromCurrent(upper);
  ctx.lineTo(lowerRev[0].x, lowerRev[0].y);
  traceSmoothPathFromCurrent(lowerRev);
  ctx.closePath();
  ctx.fill();
  ctx.globalAlpha = 1;
}

function drawLine(pts, color, width, blur) {
  ctx.strokeStyle = color;
  ctx.lineWidth   = width;
  ctx.shadowBlur  = blur;
  ctx.shadowColor = color;
  ctx.beginPath();
  traceSmoothPath(pts);
  ctx.stroke();
  ctx.shadowBlur = 0;
}

function offsetWavePoints(pts, shiftY, phaseOffset, amp) {
  return pts.map((p, i) => {
    const t = i / (pts.length - 1);
    return {
      x: p.x,
      y: p.y + shiftY + Math.sin(t * 8 + wavePhase + phaseOffset) * amp,
      e: p.e,
    };
  });
}

// ── Cinematic drawFrame ────────────────────────────────────────────────────────
function drawFrame() {
  requestAnimationFrame(drawFrame);
  if (!ctx) return;
  const W = canvas.width, H = canvas.height;
  if (!W || !H) return;

  wavePhase += 0.0085;

  const morphN   = currentValues.morph   / 10;
  const textureN = currentValues.texture / 10;
  const airN     = currentValues.air     / 10;

  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#030210";
  ctx.fillRect(0, 0, W, H);

  // Subtle grid
  ctx.strokeStyle = "rgba(139,111,255,0.045)";
  ctx.lineWidth   = 1;
  for (let i = 1; i < 4; i++) {
    const y = Math.round(H * i / 4) + 0.5;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  }

  const scale = 0.55 + morphN * 0.15;
  const hostStalled = juceAvailable && (performance.now() - lastSpectrumTick > 900);
  const displayInput = new Float32Array(BINS);
  let totalEnergy = 0;
  let stereoEnergy = 0;
  let lowSum = 0, midSum = 0, highSum = 0;
  const lowEnd = Math.floor(BINS * 0.23);
  const midEnd = Math.floor(BINS * 0.67);

  for (let b = 0; b < BINS; b++) {
    const t = b / (BINS - 1);
    const idleFloor = 0.045
      + Math.sin(t * 6.0 + wavePhase) * 0.020
      + Math.sin(t * 13.0 + wavePhase * 1.35) * 0.010;
    const fallback = Math.max(0.015, idleFloor);
    const src = hostStalled ? fallback : Math.max(smoothedInput[b], fallback);
    displayInput[b] = Math.max(0, Math.min(1, src));
    totalEnergy += displayInput[b];
    stereoEnergy += smoothedProblem[b] || 0;
    if (b < lowEnd) lowSum += displayInput[b];
    else if (b < midEnd) midSum += displayInput[b];
    else highSum += displayInput[b];
  }
  totalEnergy /= BINS;
  stereoEnergy /= BINS;
  const lowN  = lowSum / Math.max(1, lowEnd);
  const midN  = midSum / Math.max(1, midEnd - lowEnd);
  const highN = highSum / Math.max(1, BINS - midEnd);

  // Transient pulse memory for musical glow surges without jitter.
  const transient = Math.max(0, totalEnergy - loudnessMemory);
  loudnessMemory = loudnessMemory * 0.86 + totalEnergy * 0.14;
  transientPulse = Math.max(transientPulse * 0.88, transient * 7.5);
  wavePhase += lowN * 0.0045;

  const glowBreath = 0.5 + 0.5 * Math.sin(wavePhase * 0.85);
  const audioPulse = Math.max(0, Math.min(1, totalEnergy * 2.8));
  const pulseMix = 0.15 + glowBreath * 0.20 + audioPulse * 0.24 + Math.min(0.32, transientPulse * 0.30);
  const stereoWidthN = Math.max(0, Math.min(1, stereoEnergy * 2.0 + lowN * 0.34 + audioPulse * 0.22));
  const centerY = H * 0.50;
  const ampPx = H * scale;

  // Deep alien nebula gas — two side-biased haze clouds behind the wave
  const nebulaA = ctx.createRadialGradient(W * 0.28, centerY - H * 0.08, 20, W * 0.28, centerY - H * 0.08, H * 0.58);
  nebulaA.addColorStop(0,    `rgba(170, 45, 255, ${0.10 + morphN  * 0.09})`);
  nebulaA.addColorStop(0.50, `rgba(105, 28, 210, ${0.05 + pulseMix * 0.05})`);
  nebulaA.addColorStop(1,    'rgba(0,0,0,0)');
  ctx.fillStyle = nebulaA;
  ctx.fillRect(0, 0, W, H);

  const nebulaB = ctx.createRadialGradient(W * 0.74, centerY + H * 0.07, 15, W * 0.74, centerY + H * 0.07, H * 0.48);
  nebulaB.addColorStop(0,    `rgba(28, 175, 255, ${0.09 + airN    * 0.08})`);
  nebulaB.addColorStop(0.50, `rgba(18, 100, 220, ${0.04 + pulseMix * 0.04})`);
  nebulaB.addColorStop(1,    'rgba(0,0,0,0)');
  ctx.fillStyle = nebulaB;
  ctx.fillRect(0, 0, W, H);

  // Atmospheric band glow around the spectral ribbon.
  const envGlow = ctx.createRadialGradient(W * 0.5, centerY, 15, W * 0.5, centerY, H * 0.65);
  envGlow.addColorStop(0,    `rgba(150, 70, 255, ${0.11 + totalEnergy * 0.20})`);
  envGlow.addColorStop(0.35, `rgba(120, 55, 240, ${0.07 + pulseMix   * 0.14})`);
  envGlow.addColorStop(0.65, `rgba(80,  38, 195, ${0.04 + pulseMix   * 0.06})`);
  envGlow.addColorStop(1,    'rgba(0,0,0,0)');
  ctx.fillStyle = envGlow;
  ctx.fillRect(0, 0, W, H);

  const mainPts = buildCenterWavePoints(displayInput, centerY, ampPx, stereoWidthN, lowN, midN);
  const layerMid = offsetWavePoints(mainPts, 7 + lowN * 4.5, 0.9, 1.8 + midN * 3.2);
  const layerTop = offsetWavePoints(mainPts, -3.5 - highN * 1.3, -0.8, 1.4 + highN * 2.1);

  // ── Layer 1: deep shadow ghost (wide, behind everything) ──────────────────
  const shadowPts = mainPts.map(p => ({ x: p.x, y: p.y + 18, e: p.e }));
  const sg = ctx.createLinearGradient(0, 0, W, 0);
  sg.addColorStop(0,   `rgba(135, 48, 225, ${0.30 + morphN  * 0.12})`);
  sg.addColorStop(0.5, `rgba(60,  80, 215, ${0.24 + textureN * 0.10})`);
  sg.addColorStop(1,   `rgba(0,  145, 225, ${0.22 + airN    * 0.10})`);
  drawRibbon(shadowPts, 22 + lowN * 16, sg, 0.75);
  drawLine(shadowPts, `rgba(145, 65, 245, ${0.14 + morphN * 0.08})`, 1.4, 0);

  // ── Layer 2: dimensional middle ribbon ───────────────────────────────────
  const md = ctx.createLinearGradient(0, 0, W, 0);
  md.addColorStop(0,   `rgba(170,  68, 255, ${0.34 + morphN  * 0.13})`);
  md.addColorStop(0.45,`rgba(65,  135, 245, ${0.28 + textureN * 0.12})`);
  md.addColorStop(1,   `rgba(0,   210, 245, ${0.28 + airN    * 0.11})`);
  drawRibbon(layerMid, 16 + lowN * 9, md, 0.67);

  // ── Layer 3: main fill — vivid violet→cyan ────────────────────────────────
  const mg = ctx.createLinearGradient(0, 0, W, 0);
  mg.addColorStop(0,    `rgba(200,  55, 255, ${0.64 + morphN  * 0.22})`);
  mg.addColorStop(0.38, `rgba(135,  72, 255, ${0.56 + textureN * 0.16})`);
  mg.addColorStop(0.72, `rgba(32,  188, 255, ${0.52 + airN    * 0.14})`);
  mg.addColorStop(1,    `rgba(0,   248, 255, ${0.58 + airN    * 0.18})`);
  drawRibbon(mainPts, 14 + lowN * 10, mg, 0.97);

  // ── Layer 4: upper translucent highlight ribbon ───────────────────────────
  const up = ctx.createLinearGradient(0, 0, W, 0);
  up.addColorStop(0,    `rgba(225, 158, 255, ${0.28 + pulseMix * 0.11})`);
  up.addColorStop(0.45, `rgba(145, 215, 255, ${0.24 + pulseMix * 0.10})`);
  up.addColorStop(1,    `rgba(55,  248, 255, ${0.22 + pulseMix * 0.11})`);
  drawRibbon(layerTop, 8 + lowN * 5, up, 0.50);

  // ── Layer 5: vertical top-light glow ──────────────────────────────────────
  const tg = ctx.createLinearGradient(0, 0, 0, H);
  tg.addColorStop(0,   `rgba(200, 155, 255, ${0.08 + morphN  * 0.09})`);
  tg.addColorStop(0.5, `rgba(105,  78, 225, ${0.12 + textureN * 0.06})`);
  tg.addColorStop(1,   'transparent');
  drawRibbon(mainPts, 14, tg, 0.55);

  // ── Layer 6: outer bloom aura ─────────────────────────────────────────────
  const aura = ctx.createLinearGradient(0, 0, W, 0);
  aura.addColorStop(0,   `rgba(180, 95, 255, ${0.14 + pulseMix * 0.13})`);
  aura.addColorStop(0.5, `rgba(105, 215, 255, ${0.14 + pulseMix * 0.13})`);
  aura.addColorStop(1,   `rgba(30,  248, 255, ${0.14 + pulseMix * 0.13})`);
  drawLine(mainPts, aura, 22 + pulseMix * 5, 28 + pulseMix * 18);

  // ── Layer 7: wide soft bloom pass ─────────────────────────────────────────
  const lg1 = ctx.createLinearGradient(0, 0, W, 0);
  lg1.addColorStop(0,   `rgba(205, 115, 255, ${0.24 + morphN  * 0.13 + pulseMix * 0.18})`);
  lg1.addColorStop(0.5, `rgba(125, 215, 255, ${0.22 + textureN * 0.12 + pulseMix * 0.18})`);
  lg1.addColorStop(1,   `rgba(0,   252, 255, ${0.22 + airN    * 0.12 + pulseMix * 0.18})`);
  drawLine(mainPts, lg1, 10 + pulseMix * 4, 6 + pulseMix * 16);

  // ── Layer 8: sharp bright spine line ─────────────────────────────────────
  const lg2 = ctx.createLinearGradient(0, 0, W, 0);
  lg2.addColorStop(0,   `rgba(230, 178, 255, ${0.74 + morphN  * 0.18 + pulseMix * 0.22})`);
  lg2.addColorStop(0.5, `rgba(165, 235, 255, ${0.78 + textureN * 0.13 + pulseMix * 0.24})`);
  lg2.addColorStop(1,   `rgba(0,   255, 255, ${0.78 + airN    * 0.13 + pulseMix * 0.24})`);
  drawLine(mainPts, lg2, 1.6 + pulseMix * 1.0, 18 + morphN * 10 + pulseMix * 16);

  // ── Alien chroma fringe lines (high-freq shimmer) ─────────────────────────
  const fringeTop = offsetWavePoints(mainPts, -3.0, 1.7, 1.0 + highN * 3.0);
  const fringeBot = offsetWavePoints(mainPts,  3.2, -1.3, 0.9 + highN * 2.5);
  drawLine(fringeTop, `rgba(75,  245, 255, ${0.24 + highN * 0.35})`, 1.1 + highN * 1.1, 9  + highN * 16);
  drawLine(fringeBot, `rgba(210, 95,  255, ${0.20 + highN * 0.28})`, 1.0 + highN * 1.0, 8  + highN * 14);

  // ── Reduction accent ──────────────────────────────────────────────────────
  if (smoothedReduction.some(v => v > 0.01)) {
    const redPts = buildCenterWavePoints(smoothedReduction, centerY + 36, H * 0.08, stereoWidthN * 0.6);
    const rg = ctx.createLinearGradient(0, 0, W, 0);
    rg.addColorStop(0, `rgba(255,80,130,${0.09 + textureN * 0.07})`);
    rg.addColorStop(1, `rgba(255,60,100,${0.09 + textureN * 0.07})`);
    drawRibbon(redPts, 5.5, rg, 0.44);
    drawLine(redPts, `rgba(255,110,145,${0.16 + textureN * 0.09})`, 1, 6);
  }

  // ── Alien spark particles — bidirectional, cyan + violet ─────────────────
  const spawnProb = Math.min(0.60, totalEnergy * 0.90 + highN * 0.48 + transientPulse * 0.22);
  if (totalEnergy > 0.03 && Math.random() < spawnProb) {
    const count = (totalEnergy > 0.10 && Math.random() < 0.50) ? 3 : (Math.random() < 0.45 ? 2 : 1);
    for (let s = 0; s < count; s++) {
      const bi  = Math.floor(Math.random() * BINS);
      const px  = (bi / (BINS - 1)) * W;
      const py  = mainPts[bi]?.y ?? (H * 0.5 - displayInput[bi] * H * scale);
      const dir = Math.random() > 0.50 ? -1 : 1;
      const hue = Math.random() > 0.55 ? 0 : 1; // 0=cyan, 1=violet
      particles.push({
        x:    px + (Math.random() - 0.5) * 16,
        y:    py + (Math.random() - 0.5) * 8,
        vx:   (Math.random() - 0.5) * 1.4,
        vy:   dir * (0.30 + Math.random() * (1.6 + highN * 1.4)),
        life: 1.0,
        size: Math.random() * (1.6 + highN * 1.5) + 0.28,
        hue
      });
    }
  }
  for (let i = particles.length - 1; i >= 0; i--) {
    const p = particles[i];
    p.x  += p.vx; p.y += p.vy; p.life -= 0.016;
    if (p.life <= 0) { particles.splice(i, 1); continue; }
    const a  = (p.life * 0.72).toFixed(2);
    const c  = p.hue === 0 ? `rgba(0, 242, 255, ${a})` : `rgba(205, 120, 255, ${a})`;
    const gl = p.hue === 0 ? '#00F2FF' : '#CD78FF';
    ctx.shadowBlur  = 9 + p.life * 5;
    ctx.shadowColor = gl;
    ctx.fillStyle   = c;
    ctx.beginPath(); ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2); ctx.fill();
    ctx.shadowBlur  = 0;
  }

  // VU meters
  const meterIn = hostStalled
    ? Math.min(0.55, 0.20 + totalEnergy * 1.1)
    : inputPeakSmooth;
  updateMeterEl("input-meter",  meterIn);
  updateMeterEl("output-meter", outputPeakSmooth);
  outputPeakSmooth *= 0.978;
}

// ── Starfield (drawn once onto bg-canvas) ─────────────────────────────────────
function initStarfield() {
  const bg = document.getElementById("bg-canvas");
  if (!bg) return;
  const bctx = bg.getContext("2d");
  const wrap = bg.parentElement?.parentElement;
  const r = wrap?.getBoundingClientRect();
  const w = Math.max(1, Math.round(r?.width || 1080));
  const h = Math.max(1, Math.round(r?.height || 680));

  bg.width  = w;
  bg.height = h;
  bctx.clearRect(0, 0, w, h);

  // Rich galaxy nebula — five overlapping haze clouds for deep space atmosphere
  const cloudA = bctx.createRadialGradient(w * 0.22, h * 0.52, 10, w * 0.22, h * 0.52, Math.max(w, h) * 0.38);
  cloudA.addColorStop(0,    'rgba(178, 60, 255, 0.30)');
  cloudA.addColorStop(0.35, 'rgba(135, 45, 225, 0.16)');
  cloudA.addColorStop(0.65, 'rgba(90,  25, 185, 0.07)');
  cloudA.addColorStop(1,    'rgba(0,0,0,0)');
  bctx.fillStyle = cloudA;
  bctx.fillRect(0, 0, w, h);

  const cloudB = bctx.createRadialGradient(w * 0.80, h * 0.32, 8, w * 0.80, h * 0.32, Math.max(w, h) * 0.32);
  cloudB.addColorStop(0,    'rgba(55, 200, 255, 0.24)');
  cloudB.addColorStop(0.40, 'rgba(28, 130, 225, 0.13)');
  cloudB.addColorStop(0.70, 'rgba(10,  68, 185, 0.05)');
  cloudB.addColorStop(1,    'rgba(0,0,0,0)');
  bctx.fillStyle = cloudB;
  bctx.fillRect(0, 0, w, h);

  const cloudC = bctx.createRadialGradient(w * 0.52, h * 0.46, 5, w * 0.52, h * 0.46, Math.max(w, h) * 0.45);
  cloudC.addColorStop(0,    'rgba(125, 55, 245, 0.20)');
  cloudC.addColorStop(0.45, 'rgba(85,  32, 205, 0.10)');
  cloudC.addColorStop(1,    'rgba(0,0,0,0)');
  bctx.fillStyle = cloudC;
  bctx.fillRect(0, 0, w, h);

  const cloudD = bctx.createRadialGradient(w * 0.67, h * 0.26, 5, w * 0.67, h * 0.26, Math.max(w, h) * 0.27);
  cloudD.addColorStop(0,    'rgba(38, 178, 255, 0.20)');
  cloudD.addColorStop(0.50, 'rgba(20, 112, 215, 0.09)');
  cloudD.addColorStop(1,    'rgba(0,0,0,0)');
  bctx.fillStyle = cloudD;
  bctx.fillRect(0, 0, w, h);

  const cloudE = bctx.createRadialGradient(w * 0.12, h * 0.28, 5, w * 0.12, h * 0.28, Math.max(w, h) * 0.22);
  cloudE.addColorStop(0,    'rgba(215, 95, 255, 0.18)');
  cloudE.addColorStop(0.50, 'rgba(165, 68, 235, 0.08)');
  cloudE.addColorStop(1,    'rgba(0,0,0,0)');
  bctx.fillStyle = cloudE;
  bctx.fillRect(0, 0, w, h);

  // Small dim stars
  for (let i = 0; i < 620; i++) {
    const x = Math.random() * w;
    const y = Math.random() * h;
    const r = Math.random() * 1.45 + 0.14;
    const a = (Math.random() * 0.52 + 0.03).toFixed(2);
    bctx.beginPath(); bctx.arc(x, y, r, 0, Math.PI * 2);
    bctx.fillStyle = `rgba(255,255,255,${a})`; bctx.fill();
  }
  // Larger glowing stars
  for (let i = 0; i < 58; i++) {
    const x = Math.random() * w;
    const y = Math.random() * h;
    bctx.shadowBlur  = 12;
    bctx.shadowColor = "rgba(185,205,255,0.85)";
    bctx.beginPath(); bctx.arc(x, y, Math.random() * 1.35 + 0.95, 0, Math.PI * 2);
    bctx.fillStyle = "rgba(232,240,255,0.90)"; bctx.fill();
    bctx.shadowBlur = 0;
  }
}

function initCanvas() {
  canvas = document.getElementById("voice-canvas");
  if (!canvas) return;
  ctx = canvas.getContext("2d");
  if (!ctx) return;

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
  if (typeof ResizeObserver === "function" && canvas.parentElement) {
    new ResizeObserver(resize).observe(canvas.parentElement);
  } else {
    window.addEventListener("resize", resize);
  }
  window.addEventListener("resize", initStarfield);
  drawFrame();
}

// ── Boot ──────────────────────────────────────────────────────────────────────
document.addEventListener("DOMContentLoaded", () => {
  juceAvailable = (typeof window.__JUCE__ !== "undefined") && !!Juce?.getSliderState;

  bindBrowser();
  bindKnobs();
  bindModeButtons();

  try {
    buildMeter("input-meter");
    buildMeter("output-meter");
    initCanvas();
    initStarfield();
  } catch (e) {
    console.warn("visual init error:", e);
  }

  try {
    connectParameters();
  } catch (e) {
    console.warn("connectParameters error:", e);
  }

  // Always apply default preset so knobs/UI initialize visually
  const sel = document.getElementById("category-select");
  if (sel) sel.value = "Signature";
  currentCategory = "Signature";

  const leadList   = getFilteredPresets("Signature");
  const defaultIdx = leadList.findIndex(p => p.name === "International Nova");
  currentPresetIdx = defaultIdx >= 0 ? defaultIdx : 0;

  if (leadList.length > 0) {
    // pushToPlugin=false so we don't overwrite host state on load;
    // JUCE value listeners will update knob visuals once backend responds
    applyPreset(leadList[currentPresetIdx], false, false);
  }

  window.__novaVoiceBooted = true;
});
