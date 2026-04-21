'use strict';

const state = {
  retune: 0,
  humanize: 0,
  flex: 0,
  vibrato: 0,
  formant: 0,
  key: 0,
  scale: 1,
  phase: 0,
  trail: [],
  waveParticles: [],
  smoothPitch: null,
  knobHover: null,
  knobActive: null,
  retuneDrive: 0,
  signalLevel: 0.55,
  lowLatency: false,
  latencyPressUntil: 0,
  // DSP sync — driven by NativeBridge or derived from simulation
  dsp: {
    correctionAmount:    0,   // 0–1: distance from input to target note
    trackingConfidence:  1,   // 0–1: pitch detection reliability
    retuneActive:        false,
    snapFlash:           0,   // decay accumulator for fast-retune flash
  },
  // Preset browser
  preset: {
    open:      false,
    activeId:  null,
    favorites: [],
    query:     '',
    category:  'all',
    activeTag: null,
  },
};

const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const scales = ['Chromatic', 'Major', 'Minor', 'Pentatonic', 'Blues'];

const els = {
  canvas:           document.getElementById('pitchCanvas'),
  bgCanvas:         document.getElementById('bg-canvas'),
  keySelect:        document.getElementById('keySelect'),
  scaleButton:      document.getElementById('scaleButton'),
  currentNote:      document.getElementById('currentNote'),
  pianoKeys:        document.getElementById('pianoKeys'),
  lowLatencyToggle: document.getElementById('lowLatencyToggle'),
  presetToggle:     document.getElementById('presetToggle'),
  presetPanel:      document.getElementById('presetPanel'),
  presetClose:      document.getElementById('presetClose'),
  presetSearch:     document.getElementById('presetSearch'),
  presetCategory:   document.getElementById('presetCategory'),
  presetList:       document.getElementById('presetList'),
};

const paramBridges = {};

function hasJuceBackend() {
  return typeof window !== 'undefined' && !!(window.__JUCE__ && window.__JUCE__.backend);
}

function getParamRange(param) {
  if (param === 'key') return { min: 0, max: 11 };
  if (param === 'scale') return { min: 0, max: scales.length - 1 };
  if (param === 'lowLatency') return { min: 0, max: 1 };
  return { min: 0, max: 100 };
}

function getParamBridge(param) {
  if (paramBridges[param]) return paramBridges[param];
  if (!hasJuceBackend()) {
    console.warn(`[NovaSync] JUCE backend not available for param: ${param}`);
    return null;
  }

  const identifier = `__juce__slider${param}`;
  const range = getParamRange(param);

  const bridge = {
    setValue(actualValue) {
      const clamped = Math.max(range.min, Math.min(range.max, actualValue));
      console.log(`[NovaSync] emitting ${identifier} = ${clamped}`);
      window.__JUCE__.backend.emitEvent(identifier, {
        eventType: 'valueChanged',
        value: clamped,
      });
    },
    sliderDragStarted() {
      window.__JUCE__.backend.emitEvent(identifier, { eventType: 'sliderDragStarted' });
    },
    sliderDragEnded() {
      window.__JUCE__.backend.emitEvent(identifier, { eventType: 'sliderDragEnded' });
    },
  };

  paramBridges[param] = bridge;
  return bridge;
}

// ── Cosmic background ────────────────────────────────────────────
const bg = {
  stars: [],
  phase: 0,
};

// Star color palette per spec (60% ice-blue, 25% cyan-white, 15% violet-white)
const STAR_COLORS = [
  [234, 248, 255], // #EAF8FF ice-blue-white
  [234, 248, 255],
  [234, 248, 255],
  [234, 248, 255],
  [221, 251, 255], // #DDFBFF cool-cyan-white
  [221, 251, 255],
  [240, 232, 255], // #F0E8FF soft-violet-white
];

function pickStarColor() {
  const c = STAR_COLORS[Math.floor(Math.random() * STAR_COLORS.length)];
  return c;
}

// Edge-weight a position so 70% of stars land near outer edges.
function edgeWeightedPos(W, H) {
  if (Math.random() < 0.70) {
    // Edge region: pick one of 4 borders
    const side = Math.floor(Math.random() * 4);
    const margin = 0.22; // 22% strip along each edge
    switch (side) {
      case 0: return { x: Math.random() * W,          y: Math.random() * H * margin };
      case 1: return { x: Math.random() * W,          y: H * (1 - margin) + Math.random() * H * margin };
      case 2: return { x: Math.random() * W * margin, y: Math.random() * H };
      case 3: return { x: W * (1 - margin) + Math.random() * W * margin, y: Math.random() * H };
    }
  }
  // Interior — but avoid dead center (pitch graph area ~20-80% of W, 15-75% of H)
  let x, y, tries = 0;
  do {
    x = Math.random() * W;
    y = Math.random() * H;
    tries++;
  } while (
    tries < 8 &&
    x > W * 0.18 && x < W * 0.82 &&
    y > H * 0.13 && y < H * 0.78
  );
  return { x, y };
}

function initBgCanvas() {
  const c = els.bgCanvas;
  if (!c) return;

  const sizeCanvas = () => {
    c.width  = Math.min(1060, window.innerWidth  - 24);
    c.height = Math.min(660,  window.innerHeight - 24);
  };
  sizeCanvas();
  window.addEventListener('resize', () => { sizeCanvas(); seedStars(); });

  function seedStars() {
    bg.stars = [];
    const W = c.width, H = c.height;

    // Level 1 — anchor stars: 2–4, largest, brightest
    const anchorCount = 2 + Math.floor(Math.random() * 3);
    for (let i = 0; i < anchorCount; i++) {
      const pos = edgeWeightedPos(W, H);
      const col = pickStarColor();
      bg.stars.push({
        x: pos.x, y: pos.y,
        r: 2.2 + Math.random() * 1.0,
        base: 0.18 + Math.random() * 0.10,  // 18–28%
        glow: 8 + Math.random() * 10,       // 8–18px halo
        twinkle: Math.random() * Math.PI * 2,
        twinkleSpeed: (2 * Math.PI) / ((4.5 + Math.random() * 4.5) * 60), // 4.5–9s cycle
        twinkleActive: true,
        col,
        tier: 1,
      });
    }

    // Level 2 — standard stars: 10–18
    const stdCount = 10 + Math.floor(Math.random() * 9);
    for (let i = 0; i < stdCount; i++) {
      const pos = edgeWeightedPos(W, H);
      const col = pickStarColor();
      bg.stars.push({
        x: pos.x, y: pos.y,
        r: 1.2 + Math.random() * 1.0,
        base: 0.10 + Math.random() * 0.08,  // 10–18%
        glow: 4 + Math.random() * 6,        // 4–10px halo
        twinkle: Math.random() * Math.PI * 2,
        twinkleSpeed: (2 * Math.PI) / ((4.5 + Math.random() * 4.5) * 60),
        twinkleActive: Math.random() < 0.12, // only 10–15% active at once
        col,
        tier: 2,
      });
    }

    // Level 3 — micro-dust: 35–60
    const microCount = 35 + Math.floor(Math.random() * 26);
    for (let i = 0; i < microCount; i++) {
      const pos = edgeWeightedPos(W, H);
      const col = pickStarColor();
      bg.stars.push({
        x: pos.x, y: pos.y,
        r: 0.5 + Math.random() * 0.5,
        base: 0.03 + Math.random() * 0.05,  // 3–8%
        glow: Math.random() * 3,
        twinkle: Math.random() * Math.PI * 2,
        twinkleSpeed: (2 * Math.PI) / ((5 + Math.random() * 4) * 60),
        twinkleActive: Math.random() < 0.10,
        col,
        tier: 3,
      });
    }
  }
  seedStars();
}

function drawBgFrame() {
  const c = els.bgCanvas;
  if (!c) return;
  const ctx = c.getContext('2d');
  const W = c.width;
  const H = c.height;
  if (!W || !H) return;

  bg.phase += 0.005;
  // Background breathing: ±4% intensity, ~8s cycle
  // phase += 0.005/frame @60fps → 0.3/s → period = 2π/(k*0.3) → k = 2π/(8*0.3) ≈ 2.618
  const breath = 1 + 0.04 * Math.sin(bg.phase * 2.618);
  const sig = state.signalLevel;

  // Gradually cycle which stars twinkle (10-15% active at once).
  // Every ~90 frames swap one random star in/out.
  if (Math.floor(bg.phase / 0.005) % 90 === 0) {
    const active   = bg.stars.filter(s => s.twinkleActive && s.tier !== 1);
    const inactive = bg.stars.filter(s => !s.twinkleActive && s.tier !== 1);
    const targetActive = Math.ceil(bg.stars.length * 0.12); // ~12%
    if (active.length > targetActive && inactive.length > 0) {
      active[Math.floor(Math.random() * active.length)].twinkleActive = false;
      inactive[Math.floor(Math.random() * inactive.length)].twinkleActive = true;
    } else if (active.length < targetActive && inactive.length > 0) {
      inactive[Math.floor(Math.random() * inactive.length)].twinkleActive = true;
    }
  }

  // Clear to transparent — CSS provides the atmosphere base layer.
  ctx.clearRect(0, 0, W, H);

  // Breathing radial center glow — spec: #163A5A inner, 20–28% opacity
  const breathAlpha = (0.24 + sig * 0.06) * breath;
  const cg = ctx.createRadialGradient(W * 0.5, H * 0.44, 5, W * 0.5, H * 0.44, Math.min(W * 0.32, H * 0.46));
  cg.addColorStop(0,    `rgba(22, 58, 90,  ${breathAlpha})`);
  cg.addColorStop(0.55, `rgba(16, 43, 69,  ${breathAlpha * 0.5})`);
  cg.addColorStop(1,    'rgba(8, 17, 29, 0)');
  ctx.fillStyle = cg;
  ctx.fillRect(0, 0, W, H);

  // Left side glow — spec: #0E2842, 10% opacity
  const lgL = ctx.createRadialGradient(0, H * 0.5, 0, 0, H * 0.5, W * 0.22);
  lgL.addColorStop(0,   `rgba(14, 40, 66, ${0.10 + sig * 0.02})`);
  lgL.addColorStop(1,   'rgba(14, 40, 66, 0)');
  ctx.fillStyle = lgL;
  ctx.fillRect(0, 0, W, H);

  // Right side glow — spec: #10233F, 10% opacity
  const lgR = ctx.createRadialGradient(W, H * 0.5, 0, W, H * 0.5, W * 0.22);
  lgR.addColorStop(0,   `rgba(16, 35, 63, ${0.10 + sig * 0.02})`);
  lgR.addColorStop(1,   'rgba(16, 35, 63, 0)');
  ctx.fillStyle = lgR;
  ctx.fillRect(0, 0, W, H);

  // Draw star layers (tier 3 first — deepest)
  for (const tier of [3, 2, 1]) {
    for (const s of bg.stars) {
      if (s.tier !== tier) continue;
      // Advance twinkle only for active stars
      if (s.twinkleActive) s.twinkle += s.twinkleSpeed;
      // Twinkle amount: ±4–7% brightness shift per spec
      const twinkShift = s.twinkleActive ? 0.05 * Math.sin(s.twinkle) : 0;
      const reactiveLift = s.tier === 3 ? state.signalLevel * 0.01 : state.signalLevel * 0.018;
      const alpha = Math.max(0, Math.min(0.55, s.base + twinkShift + reactiveLift));

      const [r, g, b] = s.col;

      if (s.glow > 0) {
        // Soft glow halo using radial gradient
        const grd = ctx.createRadialGradient(s.x, s.y, 0, s.x, s.y, s.r + s.glow);
        grd.addColorStop(0,   `rgba(${r},${g},${b},${alpha})`);
        grd.addColorStop(0.35,`rgba(${r},${g},${b},${alpha * 0.4})`);
        grd.addColorStop(1,   `rgba(${r},${g},${b},0)`);
        ctx.fillStyle = grd;
        ctx.fillRect(s.x - s.r - s.glow, s.y - s.r - s.glow, (s.r + s.glow) * 2, (s.r + s.glow) * 2);
      }

      // Star core dot
      ctx.beginPath();
      ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${r},${g},${b},${Math.min(0.55, alpha * 1.4)})`;
      ctx.fill();
    }
  }
}

const pianoRows = [];

const knobDefs = {
  flexKnob: { key: 'flex', min: 0, max: 100, colorA: '#6AEFFF', colorB: '#B2F8FF', param: 'tolerance' },
  retuneKnob: { key: 'retune', min: 0, max: 100, colorA: '#6AEFFF', colorB: '#C8FBFF', param: 'amount', emphasis: 1.35 },
  humanizeKnob: { key: 'humanize', min: 0, max: 100, colorA: '#6AEFFF', colorB: '#B2F8FF', param: 'confidenceThreshold' },
  vibratoKnob: { key: 'vibrato', min: 0, max: 100, colorA: '#6AEFFF', colorB: '#B2F8FF', param: 'vibrato' },
  formantKnob: { key: 'formant', min: 0, max: 100, colorA: '#6AEFFF', colorB: '#B2F8FF', param: 'formant' },
};

function formatKnobValue(id, value) {
  if (id === 'retuneKnob') {
    const rounded = Math.round(value);
    if (rounded <= 10) return `${rounded} (Slow)`;
    if (rounded >= 90) return `${rounded} (Fast)`;
    return `${rounded}`;
  }
  return `${Math.round(value)}%`;
}

function updateKnobReadout(id, value) {
  const readout = document.getElementById(`${id}Value`);
  if (!readout) return;
  readout.textContent = formatKnobValue(id, value);
}

function syncParam(param, value) {
  const bridge = getParamBridge(param);
  if (!bridge) return;
  bridge.setValue(value);
}

const paramStateBindings = {
  amount: { key: 'retune', knobId: 'retuneKnob' },
  tolerance: { key: 'flex', knobId: 'flexKnob' },
  confidenceThreshold: { key: 'humanize', knobId: 'humanizeKnob' },
  vibrato: { key: 'vibrato', knobId: 'vibratoKnob' },
  formant: { key: 'formant', knobId: 'formantKnob' },
};

function applyHostParamValue(param, value) {
  if (param === 'key') {
    const next = Math.max(0, Math.min(11, Math.round(value)));
    state.key = next;
    if (els.keySelect) els.keySelect.value = noteNames[next];
    return;
  }

  if (param === 'scale') {
    const next = Math.max(0, Math.min(scales.length - 1, Math.round(value)));
    state.scale = next;
    if (els.scaleButton) els.scaleButton.textContent = scales[next];
    return;
  }

  if (param === 'lowLatency') {
    setLowLatency(value >= 0.5, { animate: false, sync: false });
    return;
  }

  const binding = paramStateBindings[param];
  if (!binding) return;

  const next = Math.max(0, Math.min(100, value));
  state[binding.key] = next;
  updateKnobReadout(binding.knobId, next);
}

function requestInitialHostParameters() {
  if (!hasJuceBackend()) return;

  const params = ['amount', 'tolerance', 'confidenceThreshold', 'vibrato', 'formant', 'key', 'scale', 'lowLatency'];

  params.forEach((param) => {
    const identifier = `__juce__slider${param}`;

    window.__JUCE__.backend.addEventListener(identifier, (event) => {
      if (!event || event.eventType !== 'valueChanged' || typeof event.value !== 'number')
        return;
      applyHostParamValue(param, event.value);
    });

    window.__JUCE__.backend.emitEvent(identifier, { eventType: 'requestInitialUpdate' });
  });
}

// ── NativeBridge DSP receive ───────────────────────────────
// Called by JUCE WebView to push real-time DSP telemetry into the UI.
window.receiveDSP = function(data) {
  if (!data || typeof data !== 'object') return;
  if (typeof data.correctionAmount   === 'number') state.dsp.correctionAmount   = Math.max(0, Math.min(1, data.correctionAmount));
  if (typeof data.trackingConfidence === 'number') state.dsp.trackingConfidence = Math.max(0, Math.min(1, data.trackingConfidence));
  if (typeof data.retuneActive       === 'boolean') state.dsp.retuneActive      = data.retuneActive;
};

function renderLowLatencyToggle() {
  const button = els.lowLatencyToggle;
  if (!button) return;

  const pressed = performance.now() < state.latencyPressUntil;
  button.classList.toggle('is-active', state.lowLatency);
  button.classList.toggle('is-pressed', pressed);
  button.setAttribute('aria-pressed', String(state.lowLatency));
}

function setLowLatency(enabled, options = {}) {
  const next = Boolean(enabled);
  const shouldAnimate = options.animate !== false;
  const shouldSync = options.sync !== false;

  state.lowLatency = next;
  if (shouldAnimate) state.latencyPressUntil = performance.now() + 105;
  renderLowLatencyToggle();

  if (!shouldSync) return;
  syncParam('lowLatency', next ? 1 : 0);
}

function drawKnob(canvas, value, def) {
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;
  const cx = w * 0.5;
  const cy = h * 0.5;
  const r = Math.min(w, h) * 0.42;

  const t = Math.max(0, Math.min(1, (value - def.min) / (def.max - def.min)));
  const start = -Math.PI * 0.75;
  const end = start + Math.PI * 1.5 * t;
  const emphasis = def.emphasis || 1;
  const hoverBoost = state.knobHover === canvas.id ? 0.22 : 0;
  const activeBoost = state.knobActive === canvas.id ? 0.34 : 0;
  const breath = 0.96 + Math.sin(state.phase * 0.18) * 0.04;
  const retunePulse = canvas.id === 'retuneKnob' ? state.retuneDrive : 0;
  const glowBoost = (hoverBoost + activeBoost + retunePulse * 0.5) * breath;
  const isRetune = canvas.id === 'retuneKnob';

  ctx.clearRect(0, 0, w, h);

  const glow = ctx.createRadialGradient(cx, cy, r * 0.16, cx, cy, r + (isRetune ? 12 : 16) * emphasis + (isRetune ? 8 : 10) * glowBoost);
  glow.addColorStop(0, 'rgba(255,255,255,0.26)');
  glow.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = glow;
  ctx.beginPath();
  ctx.arc(cx, cy, r + (isRetune ? 9 : 12), 0, Math.PI * 2);
  ctx.fill();

  const body = ctx.createRadialGradient(cx - 9, cy - 12, 4, cx, cy, r + 6);
  body.addColorStop(0, 'rgba(245,251,255,0.56)');
  body.addColorStop(0.28, 'rgba(148,165,182,0.58)');
  body.addColorStop(0.60, 'rgba(44,56,70,0.80)');
  body.addColorStop(1, 'rgba(8,13,20,0.98)');
  ctx.fillStyle = body;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.fill();

  ctx.strokeStyle = 'rgba(195,210,225,0.25)';
  ctx.lineWidth = 1.3;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();

  ctx.strokeStyle = 'rgba(205,220,235,0.13)';
  ctx.lineWidth = Math.max(4, r * 0.12);
  ctx.beginPath();
  ctx.arc(cx, cy, r - 7, -Math.PI * 0.75, Math.PI * 0.75);
  ctx.stroke();

  const innerGlow = ctx.createRadialGradient(cx, cy, r * 0.12, cx, cy, r * 0.86);
  innerGlow.addColorStop(0, 'rgba(168, 216, 246, 0.34)');
  innerGlow.addColorStop(0.56, 'rgba(42, 58, 76, 0.2)');
  innerGlow.addColorStop(1, 'rgba(0, 0, 0, 0)');
  ctx.fillStyle = innerGlow;
  ctx.beginPath();
  ctx.arc(cx, cy, r - 3, 0, Math.PI * 2);
  ctx.fill();

  // Inner top highlight for a more jewel-like face.
  const innerHighlight = ctx.createLinearGradient(0, cy - r, 0, cy + r);
  innerHighlight.addColorStop(0, 'rgba(238, 248, 255, 0.28)');
  innerHighlight.addColorStop(0.35, 'rgba(238, 248, 255, 0.08)');
  innerHighlight.addColorStop(1, 'rgba(0, 0, 0, 0)');
  ctx.fillStyle = innerHighlight;
  ctx.beginPath();
  ctx.arc(cx, cy, r - 6, -Math.PI, 0);
  ctx.lineTo(cx + (r - 6), cy);
  ctx.closePath();
  ctx.fill();

  // Glass reflection streak.
  ctx.strokeStyle = 'rgba(238, 250, 255, 0.40)';
  ctx.lineWidth = Math.max(2.8, r * 0.07);
  ctx.beginPath();
  ctx.arc(cx - r * 0.06, cy - r * 0.08, r * 0.72, -Math.PI * 0.86, -Math.PI * 0.58);
  ctx.stroke();

  const ring = ctx.createLinearGradient(0, 0, w, h);
  ring.addColorStop(0, def.colorA);
  ring.addColorStop(0.55, '#E0FBFF');
  ring.addColorStop(1, def.colorB);
  ctx.strokeStyle = ring;
  ctx.lineWidth = Math.max(4.8, r * (0.14 * emphasis + glowBoost * 0.07));
  ctx.lineCap = 'round';
  ctx.shadowColor = def.colorA;
  // Spec: standard 6-12px blur; retune 12-22px blur
  ctx.shadowBlur = isRetune
    ? 14 + 5 * emphasis + 8 * glowBoost
    : 6 + 1.5 * emphasis + 6 * glowBoost;
  ctx.beginPath();
  ctx.arc(cx, cy, r - 7, start, end);
  ctx.stroke();

  // Retune additional outer halo — stronger than others
  if (isRetune) {
    ctx.strokeStyle = `rgba(106, 239, 255, ${0.13 + glowBoost * 0.06})`;
    ctx.lineWidth = 2.0;
    ctx.shadowColor = '#6AEFFF';
    ctx.shadowBlur = 20 + glowBoost * 16;
    ctx.beginPath();
    ctx.arc(cx, cy, r - 5, start, end);
    ctx.stroke();
  }

  // Crisp LED edge accent to make the ring read brighter.
  ctx.strokeStyle = isRetune ? 'rgba(230, 251, 255, 0.88)' : 'rgba(230, 251, 255, 0.62)';
  ctx.lineWidth = Math.max(1, r * 0.03);
  ctx.shadowBlur = 0;
  ctx.beginPath();
  ctx.arc(cx, cy, r - 7.6, start, end);
  ctx.stroke();

  const px = cx + Math.cos(end) * (r - 10);
  const py = cy + Math.sin(end) * (r - 10);
  ctx.fillStyle = '#d9edf7';
  ctx.beginPath();
  ctx.arc(px, py, 2.7, 0, Math.PI * 2);
  ctx.fill();

  // Darker center pupil for stronger contrast
  const pupil = ctx.createRadialGradient(cx, cy, 0, cx, cy, r * 0.28);
  pupil.addColorStop(0, 'rgba(4,7,12,0.99)');
  pupil.addColorStop(1, 'rgba(10,15,22,0.96)');
  ctx.fillStyle = pupil;
  ctx.beginPath();
  ctx.arc(cx, cy, r * 0.28, 0, Math.PI * 2);
  ctx.fill();
}

function bindKnob(id) {
  const canvas = document.getElementById(id);
  const def = knobDefs[id];
  if (!canvas || !def) return;

  const redraw = () => drawKnob(canvas, state[def.key], def);
  redraw();
  updateKnobReadout(id, state[def.key]);

  let drag = null;

  const start = (e) => {
    e.preventDefault();
    const y = e.touches ? e.touches[0].clientY : e.clientY;
    drag = { y, value: state[def.key] };
    state.knobActive = id;
    if (id === 'retuneKnob') state.retuneDrive = 1;

    const bridge = getParamBridge(def.param);
    if (bridge) bridge.sliderDragStarted();
  };

  const move = (e) => {
    if (!drag) return;
    const y = e.touches ? e.touches[0].clientY : e.clientY;
    const dy = drag.y - y;
    const range = def.max - def.min;
    const next = Math.max(def.min, Math.min(def.max, drag.value + dy * (range / 220)));
    state[def.key] = next;
    redraw();
    updateKnobReadout(id, next);
    if (id === 'retuneKnob') state.retuneDrive = 1;

    syncParam(def.param, next);
  };

  const end = () => {
    if (state.knobActive != null) {
      const activeDef = knobDefs[state.knobActive];
      if (activeDef) {
        const bridge = getParamBridge(activeDef.param);
        if (bridge) bridge.sliderDragEnded();
      }
    }
    drag = null;
    state.knobActive = null;
  };

  canvas.addEventListener('mousedown', start);
  canvas.addEventListener('touchstart', start, { passive: false });
  canvas.addEventListener('mouseenter', () => { state.knobHover = id; });
  canvas.addEventListener('mouseleave', () => {
    if (state.knobHover === id) state.knobHover = null;
  });
  window.addEventListener('mousemove', move, { passive: false });
  window.addEventListener('touchmove', move, { passive: false });
  window.addEventListener('mouseup', end);
  window.addEventListener('touchend', end);
}

function midiToNoteName(midi) {
  const idx = ((midi % 12) + 12) % 12;
  const octave = Math.floor(midi / 12) - 1;
  return `${noteNames[idx]}${octave}`;
}

function buildPianoKeys() {
  if (!els.pianoKeys) return;
  els.pianoKeys.innerHTML = '';
  pianoRows.length = 0;

  // Keep this slim: C1..B4, high at the top.
  for (let midi = 71; midi >= 24; midi--) {
    const idx = ((midi % 12) + 12) % 12;
    const isBlack = noteNames[idx].includes('#');
    const full = midiToNoteName(midi);

    const row = document.createElement('div');
    row.className = `key-row${isBlack ? ' black' : ''}`;
    row.dataset.noteFull = full;
    row.dataset.noteName = noteNames[idx];

    // Show sparse labels only on C notes for clarity.
    row.textContent = noteNames[idx] === 'C' ? full : '';

    els.pianoKeys.appendChild(row);
    pianoRows.push(row);
  }
}

function setActivePianoNote(note) {
  if (!pianoRows.length) return;

  for (const row of pianoRows) row.classList.remove('active');

  let active = pianoRows.find((row) => row.dataset.noteFull === note);
  if (!active) {
    const base = note.replace(/[0-9]/g, '');
    active = pianoRows.find((row) => row.dataset.noteName === base);
  }
  if (active) active.classList.add('active');
}

function resizeCanvas() {
  const rect = els.canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  els.canvas.width = Math.max(1, Math.floor(rect.width * dpr));
  els.canvas.height = Math.max(1, Math.floor(rect.height * dpr));
}

function drawGraph() {
  const c = els.canvas;
  const ctx = c.getContext('2d');
  const w = c.width;
  const h = c.height;
  const centerY = h * 0.58;
  const maxOffset = Math.min(18, h * 0.08);

  ctx.clearRect(0, 0, w, h);

  // ── Background ────────────────────────────────────────────
  const bgGrad = ctx.createLinearGradient(0, 0, 0, h);
  bgGrad.addColorStop(0, '#0e1a2c');
  bgGrad.addColorStop(1, '#070c14');
  ctx.fillStyle = bgGrad;
  ctx.fillRect(0, 0, w, h);

  // Subtle deep-blue ambient wash
  const ambGlow = ctx.createRadialGradient(w * 0.5, h * 0.5, 0, w * 0.5, h * 0.5, w * 0.45);
  ambGlow.addColorStop(0, 'rgba(40, 80, 160, 0.06)');
  ambGlow.addColorStop(1, 'rgba(0, 0, 0, 0)');
  ctx.fillStyle = ambGlow;
  ctx.fillRect(0, 0, w, h);

  // ── Grid ─────────────────────────────────────────────────
  ctx.strokeStyle = 'rgba(150, 185, 215, 0.10)';
  ctx.lineWidth = 1;
  for (let i = 0; i < 12; i++) {
    const y = (h / 12) * i;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
  }
  for (let i = 0; i < 10; i++) {
    const x = (w / 9) * i;
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
  }

  // ── Pitch logic (unchanged) ───────────────────────────────
  if (state.trail.length === 0) {
    for (let i = 0; i < 220; i++) state.trail.push(centerY);
  }

  const natural = centerY
    + Math.sin(state.phase * 1.7) * maxOffset * 0.78
    + Math.sin(state.phase * 0.43) * maxOffset * 0.42;
  const targetOffset = ((state.key / 11) - 0.5) * maxOffset * 1.5;
  const target  = centerY - targetOffset;
  const retuneBlend = state.retune / 100;
  const correctedCore = natural + (target - natural) * retuneBlend;

  const correctionDist = Math.abs(natural - target);
  if (!state.dsp.retuneActive) {
    state.dsp.correctionAmount = Math.min(1, correctionDist / (h * 0.18));
  }
  if (state.dsp.snapFlash > 0) state.dsp.snapFlash *= 0.88;

  const vibDepth = (state.vibrato / 100) * (maxOffset * 0.45);
  const vibRate  = 1.2 + (state.vibrato / 100) * 4.0;
  const correctedWithVibrato = correctedCore + Math.sin(state.phase * vibRate) * vibDepth;

  if (state.smoothPitch == null) state.smoothPitch = correctedWithVibrato;
  const smoothBase   = state.lowLatency ? 0.44 : 0.20;
  const smoothAmount = Math.min(0.76, smoothBase + retuneBlend * 0.18);
  state.smoothPitch += (correctedWithVibrato - state.smoothPitch) * smoothAmount;

  const drift = Math.abs(correctedWithVibrato - state.smoothPitch) / Math.max(1, h * 0.12);
  const pseudoInput = 0.44 + Math.abs(Math.sin(state.phase * 0.9)) * 0.32;
  state.signalLevel = Math.max(0.2, Math.min(1, pseudoInput + drift * 0.35));

  const jitterAmt = (1 - state.dsp.trackingConfidence) * 2.4;
  const jitter    = jitterAmt > 0.2 ? (Math.random() - 0.5) * jitterAmt * 2 : 0;
  const constrainedPitch = Math.max(centerY - maxOffset, Math.min(centerY + maxOffset, state.smoothPitch + jitter));
  state.trail.push(constrainedPitch);
  if (state.trail.length > 220) state.trail.shift();

  // ── Particle system ───────────────────────────────────────
  const recentMotionWindow = 8;
  let recentMotion = 0;
  for (let i = state.trail.length - recentMotionWindow; i < state.trail.length; i++) {
    if (i <= 0) continue;
    recentMotion += Math.abs(state.trail[i] - state.trail[i - 1]);
  }
  recentMotion /= Math.max(1, recentMotionWindow - 1);
  const motionIntensity = Math.max(0.18, Math.min(1, recentMotion / Math.max(1, maxOffset * 0.28)));

  const emissionWeights = [];
  let emissionWeightTotal = 0;
  for (let i = 2; i < state.trail.length - 2; i++) {
    const yPrev = state.trail[i - 1];
    const y = state.trail[i];
    const yNext = state.trail[i + 1];
    const slope = Math.abs(yNext - yPrev);
    const curvature = Math.abs(yPrev - (2 * y) + yNext);
    const t = i / (state.trail.length - 1);
    const regionBias = 1.18 - Math.pow(t, 0.82) * 0.52;
    const activity = Math.min(1, slope / Math.max(1, maxOffset * 0.55));
    const bend = Math.min(1, curvature / Math.max(1, maxOffset * 0.32));
    const weight = Math.max(0, (0.16 + activity * 0.56 + bend * 0.28) * regionBias * (0.6 + motionIntensity * 0.55));
    emissionWeights.push(weight);
    emissionWeightTotal += weight;
  }

  const emissionBudget = Math.max(0, Math.round((w / 210) * (0.4 + motionIntensity * 0.9)));
  const spawnN = Math.min(4, emissionBudget);
  for (let s = 0; s < spawnN && emissionWeightTotal > 0; s++) {
    let pick = Math.random() * emissionWeightTotal;
    let localIndex = 0;
    for (let i = 0; i < emissionWeights.length; i++) {
      pick -= emissionWeights[i];
      if (pick <= 0) {
        localIndex = i;
        break;
      }
    }

    const idx = localIndex + 2;
    const t = idx / (state.trail.length - 1);
    const x = t * w;
    const y = state.trail[idx];
    const yPrev = state.trail[idx - 1];
    const yNext = state.trail[idx + 1];
    const dx = w / Math.max(1, state.trail.length - 1);
    const txRaw = dx * 2;
    const tyRaw = yNext - yPrev;
    const tangentLength = Math.max(0.0001, Math.hypot(txRaw, tyRaw));
    const tangentX = txRaw / tangentLength;
    const tangentY = tyRaw / tangentLength;
    const normalSign = Math.random() < 0.5 ? -1 : 1;
    const normalX = (-tangentY / Math.max(0.0001, Math.hypot(tangentX, tangentY))) * normalSign;
    const normalY = (tangentX / Math.max(0.0001, Math.hypot(tangentX, tangentY))) * normalSign;
    const size = 1 + Math.random();
    const lifeFrames = 30 + Math.random() * 24;
    const baseOpacity = 0.08 + Math.random() * 0.10;
    const along = 1.2 + Math.random() * 2.2;
    const outward = 4 + Math.random() * 6;
    const brightness = 0.92 + Math.random() * 0.14;
    const originOffset = (Math.random() - 0.5) * 0.9;

    state.waveParticles.push({
      originX: x + normalX * originOffset,
      originY: y + normalY * originOffset,
      tangentX,
      tangentY,
      normalX,
      normalY,
      along,
      outward,
      size,
      lifeFrames,
      age: 0,
      baseOpacity,
      brightness,
      t,
    });
  }

  // Draw and age particles
  ctx.save();
  state.waveParticles = state.waveParticles.filter((p) => p.age < p.lifeFrames);
  for (const p of state.waveParticles) {
    p.age += 1;
    const progress = Math.min(1, p.age / p.lifeFrames);
    const ease = 1 - Math.pow(1 - progress, 3);
    const fade = Math.pow(1 - progress, 1.75);
    const px = p.originX + p.tangentX * p.along * ease + p.normalX * p.outward * ease;
    const py = p.originY + p.tangentY * p.along * ease + p.normalY * p.outward * ease;

    // Purple (t=0) → blue (t=0.5) → cyan (t=1), slightly brightness-varied per particle.
    const r = Math.round((155 - p.t * 60) * p.brightness);
    const g = Math.round((80 + p.t * 152) * p.brightness);
    const b = Math.round(255 * Math.min(1, p.brightness * 1.02));
    ctx.globalAlpha = p.baseOpacity * fade;
    ctx.fillStyle = `rgb(${r},${g},${b})`;
    ctx.beginPath();
    ctx.arc(px, py, p.size, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.globalAlpha = 1;
  ctx.restore();

  // ── Pitch line — purple → blue → cyan gradient ────────────
  const trailLen = state.trail.length;

  const buildPath = () => {
    ctx.beginPath();
    for (let i = 0; i < trailLen; i++) {
      const x = (i / (trailLen - 1)) * w;
      const y = state.trail[i];
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
  };

  // Outer glow — wide, soft, fades in from left
  ctx.globalCompositeOperation = 'lighter';
  ctx.lineCap  = 'round';
  ctx.lineJoin = 'round';
  const glowGrad = ctx.createLinearGradient(0, 0, w, 0);
  glowGrad.addColorStop(0.00, 'rgba(140, 70, 255, 0.07)');
  glowGrad.addColorStop(0.35, 'rgba(100, 140, 255, 0.13)');
  glowGrad.addColorStop(0.70, 'rgba(70, 190, 255, 0.16)');
  glowGrad.addColorStop(1.00, 'rgba(95, 232, 255, 0.20)');
  ctx.strokeStyle = glowGrad;
  ctx.lineWidth   = 10;
  ctx.shadowColor = '#4DA6FF';
  ctx.shadowBlur  = 14;
  buildPath(); ctx.stroke();

  // Mid glow — tighter
  const midGlowGrad = ctx.createLinearGradient(0, 0, w, 0);
  midGlowGrad.addColorStop(0.00, 'rgba(155, 80, 255, 0.16)');
  midGlowGrad.addColorStop(0.45, 'rgba(90, 160, 255, 0.22)');
  midGlowGrad.addColorStop(1.00, 'rgba(95, 232, 255, 0.28)');
  ctx.strokeStyle = midGlowGrad;
  ctx.lineWidth   = 4;
  ctx.shadowColor = '#7BB0FF';
  ctx.shadowBlur  = 7;
  buildPath(); ctx.stroke();

  // Core line — thin, bright
  const coreGrad = ctx.createLinearGradient(0, 0, w, 0);
  coreGrad.addColorStop(0.00, 'rgba(200, 160, 255, 0.80)');
  coreGrad.addColorStop(0.40, 'rgba(160, 210, 255, 0.92)');
  coreGrad.addColorStop(0.75, 'rgba(180, 240, 255, 0.97)');
  coreGrad.addColorStop(1.00, 'rgba(220, 252, 255, 1.00)');
  ctx.strokeStyle = coreGrad;
  ctx.lineWidth   = 2.0;
  ctx.shadowColor = '#9B5CFF';
  ctx.shadowBlur  = 5;
  buildPath(); ctx.stroke();

  ctx.shadowBlur = 0;
  ctx.globalCompositeOperation = 'source-over';

  // ── Head sparkle (right-edge, current position) ───────────
  const headY    = state.trail[trailLen - 1];
  const playPulse = 0.5 + 0.5 * Math.sin(state.phase * 2.8);
  const headX    = w - 4;

  ctx.globalCompositeOperation = 'lighter';
  const haloGrad = ctx.createRadialGradient(headX, headY, 0, headX, headY, 28 + playPulse * 6);
  haloGrad.addColorStop(0,   `rgba(220, 252, 255, ${0.55 + playPulse * 0.18})`);
  haloGrad.addColorStop(0.3, `rgba(95,  232, 255, ${0.30 + playPulse * 0.10})`);
  haloGrad.addColorStop(1,   'rgba(0, 0, 0, 0)');
  ctx.fillStyle = haloGrad;
  ctx.fillRect(headX - 36, headY - 36, 72, 72);
  ctx.globalCompositeOperation = 'source-over';

  ctx.fillStyle   = 'rgba(230, 254, 255, 0.96)';
  ctx.shadowColor = 'rgba(120, 245, 255, 0.90)';
  ctx.shadowBlur  = 16;
  ctx.beginPath();
  ctx.arc(headX, headY, 2.4, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;

  // ── Note detection ────────────────────────────────────────
  const detectedMidi = 60 + Math.round((1 - natural / h) * 12) + state.key;
  const detectedNote = midiToNoteName(Math.max(24, Math.min(71, detectedMidi)));
  els.currentNote.textContent = detectedNote;
  setActivePianoNote(detectedNote);
}

function setupTopBar() {
  els.keySelect.value = noteNames[state.key];
  els.scaleButton.textContent = scales[state.scale];

  els.keySelect.addEventListener('change', () => {
    state.key = Math.max(0, noteNames.indexOf(els.keySelect.value));
    syncParam('key', state.key);
  });

  els.scaleButton.addEventListener('click', () => {
    state.scale = (state.scale + 1) % scales.length;
    els.scaleButton.textContent = scales[state.scale];
    syncParam('scale', state.scale);
  });

  if (els.lowLatencyToggle) {
    renderLowLatencyToggle();
    els.lowLatencyToggle.addEventListener('click', () => {
      setLowLatency(!state.lowLatency);
    });
  }
}

function animate() {
  state.phase += 0.025;
  state.retuneDrive *= 0.92;
  renderLowLatencyToggle();
  drawBgFrame();
  drawGraph();
  Object.keys(knobDefs).forEach((id) => {
    const canvas = document.getElementById(id);
    const def = knobDefs[id];
    if (canvas && def) drawKnob(canvas, state[def.key], def);
  });
  requestAnimationFrame(animate);
}

// ── Preset System ──────────────────────────────────────────
// 9 curated starting points — Signature / Core / Creative
const PRESETS = [
  // ── Signature ──────────────────────────────────
  { id: 'nova-sig',   section: 'Signature', name: 'Nova Signature', tags: ['smooth','bright'],
    values: { retune: 9, humanize: 60, flex: 55, vibrato: 12, formant: 52 } },
  { id: 'midnight',   section: 'Signature', name: 'Midnight Glow',  tags: ['smooth','natural'],
    values: { retune: 50, humanize: 70, flex: 65, vibrato: 18, formant: 55 } },
  { id: 'atl-glide',  section: 'Signature', name: 'ATL Glide',      tags: ['smooth','bright'],
    values: { retune: 28, humanize: 40, flex: 45, vibrato: 10, formant: 50 } },

  // ── Core ───────────────────────────────────────
  { id: 'natural',    section: 'Core',      name: 'Natural',        tags: ['natural'],
    values: { retune: 65, humanize: 80, flex: 75, vibrato: 10, formant: 50 } },
  { id: 'smooth',     section: 'Core',      name: 'Smooth',         tags: ['smooth','natural'],
    values: { retune: 45, humanize: 65, flex: 60, vibrato: 15, formant: 50 } },
  { id: 'tight',      section: 'Core',      name: 'Tight',          tags: ['aggressive'],
    values: { retune: 18, humanize: 25, flex: 30, vibrato:  5, formant: 50 } },
  { id: 'hard-tune',  section: 'Core',      name: 'Hard Tune',      tags: ['aggressive','robotic'],
    values: { retune:   5, humanize: 0, flex: 10, vibrato:  0, formant: 52 } },

  // ── Creative ───────────────────────────────────
  { id: 'robot',      section: 'Creative',  name: 'Robot',          tags: ['robotic','aggressive'],
    values: { retune:   2, humanize: 0, flex:  0, vibrato:  0, formant: 60 } },
  { id: 'glide',      section: 'Creative',  name: 'Glide',          tags: ['smooth','natural'],
    values: { retune: 70, humanize: 75, flex: 80, vibrato: 25, formant: 50 } },
];

function loadFavorites() {
  try {
    const raw = localStorage.getItem('novaPitchFavs');
    state.preset.favorites = raw ? JSON.parse(raw) : [];
  } catch { state.preset.favorites = []; }
}
function saveFavorites() {
  try { localStorage.setItem('novaPitchFavs', JSON.stringify(state.preset.favorites)); } catch {}
}

function applyPreset(preset) {
  state.preset.activeId = preset.id;
  const v = preset.values;
  state.retune    = Math.max(0, Math.min(100, v.retune));
  state.humanize  = v.humanize;
  state.flex      = v.flex;
  state.vibrato   = v.vibrato;
  state.formant   = v.formant;

  // Sync native + refresh knob readouts
  syncParam('amount',                state.retune);
  syncParam('confidenceThreshold',   state.humanize);
  syncParam('tolerance',             state.flex);
  syncParam('vibrato',               state.vibrato);
  syncParam('formant',               state.formant);

  Object.keys(knobDefs).forEach((id) => {
    const canvas = document.getElementById(id);
    const def    = knobDefs[id];
    if (canvas && def) drawKnob(canvas, state[def.key], def);
    updateKnobReadout(id, state[def.key]);
  });
  renderPresets();
}

function getFilteredPresets() {
  const q   = state.preset.query.trim().toLowerCase();
  const cat = state.preset.category;
  const tag = state.preset.activeTag;
  return PRESETS.filter(p => {
    const secLower = p.section.toLowerCase().replace(/\s/g, '');
    if (cat !== 'all') {
      const catMap = { clean: 'core', hardtune: 'core', creative: 'creative', artist: 'signature' };
      const mapped = catMap[cat] || cat;
      if (cat === 'hardtune') {
        if (!p.tags.includes('aggressive') && !p.tags.includes('robotic')) return false;
      } else if (secLower !== mapped) return false;
    }
    if (tag && !p.tags.includes(tag)) return false;
    if (q && !p.name.toLowerCase().includes(q)) return false;
    return true;
  });
}

function renderPresets() {
  const list = els.presetList;
  if (!list) return;
  list.innerHTML = '';

  const filtered = getFilteredPresets();
  if (filtered.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'preset-empty';
    empty.textContent = 'No presets match the current filter.';
    list.appendChild(empty);
    return;
  }

  // Group by section maintaining order: Favorites bucket first, then Signature→Core→Creative
  const favIds = new Set(state.preset.favorites);
  const favPresets = filtered.filter(p => favIds.has(p.id));
  const sections = ['Signature', 'Core', 'Creative'];

  const buildItem = (preset) => {
    const item = document.createElement('div');
    item.className = 'preset-item' + (preset.id === state.preset.activeId ? ' is-active' : '');
    item.setAttribute('role', 'option');
    item.setAttribute('aria-selected', preset.id === state.preset.activeId ? 'true' : 'false');

    const isFav = favIds.has(preset.id);
    const favBtn = document.createElement('button');
    favBtn.className = 'preset-item__fav' + (isFav ? ' is-fav' : '');
    favBtn.type = 'button';
    favBtn.textContent = '★';
    favBtn.setAttribute('aria-label', (isFav ? 'Remove from' : 'Add to') + ' favourites');
    favBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      if (favIds.has(preset.id)) {
        state.preset.favorites = state.preset.favorites.filter(id => id !== preset.id);
      } else {
        state.preset.favorites.push(preset.id);
      }
      saveFavorites();
      renderPresets();
    });

    const name = document.createElement('div');
    name.className = 'preset-item__name';
    name.textContent = preset.name;

    const tagsWrap = document.createElement('div');
    tagsWrap.className = 'preset-item__tags';
    preset.tags.forEach(t => {
      const span = document.createElement('span');
      span.className = 'preset-item__tag';
      span.textContent = t;
      tagsWrap.appendChild(span);
    });

    item.appendChild(favBtn);
    item.appendChild(name);
    item.appendChild(tagsWrap);
    item.addEventListener('click', () => {
      applyPreset(preset);
      closePresetPanel();
    });
    return item;
  };

  if (favPresets.length > 0) {
    const label = document.createElement('div');
    label.className = 'preset-section-label';
    label.textContent = 'Favourites';
    list.appendChild(label);
    favPresets.forEach(p => list.appendChild(buildItem(p)));
  }

  sections.forEach(sec => {
    const group = filtered.filter(p => p.section === sec && (favPresets.length === 0 || !favIds.has(p.id)));
    if (group.length === 0) return;
    const label = document.createElement('div');
    label.className = 'preset-section-label';
    label.textContent = sec;
    list.appendChild(label);
    group.forEach(p => list.appendChild(buildItem(p)));
  });
}

function openPresetPanel() {
  state.preset.open = true;
  els.presetPanel.classList.add('is-open');
  els.presetPanel.setAttribute('aria-hidden', 'false');
  els.presetToggle.classList.add('is-active');
  els.presetToggle.setAttribute('aria-expanded', 'true');
  renderPresets();
}

function closePresetPanel() {
  state.preset.open = false;
  els.presetPanel.classList.remove('is-open');
  els.presetPanel.setAttribute('aria-hidden', 'true');
  els.presetToggle.classList.remove('is-active');
  els.presetToggle.setAttribute('aria-expanded', 'false');
}

function setupPresetBrowser() {
  loadFavorites();

  els.presetToggle.addEventListener('click', () => {
    state.preset.open ? closePresetPanel() : openPresetPanel();
  });
  els.presetClose.addEventListener('click', closePresetPanel);

  els.presetSearch.addEventListener('input', () => {
    state.preset.query = els.presetSearch.value;
    renderPresets();
  });

  els.presetCategory.addEventListener('change', () => {
    state.preset.category = els.presetCategory.value;
    renderPresets();
  });

  document.querySelectorAll('#presetTagBar .ptag').forEach(btn => {
    btn.addEventListener('click', () => {
      const tag = btn.dataset.tag;
      if (state.preset.activeTag === tag) {
        state.preset.activeTag = null;
        btn.classList.remove('is-active');
      } else {
        state.preset.activeTag = tag;
        document.querySelectorAll('#presetTagBar .ptag').forEach(b => b.classList.remove('is-active'));
        btn.classList.add('is-active');
      }
      renderPresets();
    });
  });
}

// ── End Preset System ──────────────────────────────────────

function init() {
  console.log('[NovaSync] Page initializing...');
  console.log('[NovaSync] JUCE backend available?', hasJuceBackend());
  buildPianoKeys();
  setupTopBar();
  resizeCanvas();
  initBgCanvas();
  Object.keys(knobDefs).forEach(bindKnob);
  requestInitialHostParameters();

  console.log('[NovaSync] Waiting for host parameter snapshot');
  setLowLatency(state.lowLatency, { animate: false, sync: false });
  setupPresetBrowser();

  animate();
}

window.addEventListener('resize', resizeCanvas);
window.addEventListener('DOMContentLoaded', init);
