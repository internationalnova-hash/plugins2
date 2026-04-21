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

if (typeof window !== 'undefined') {
  window.__novaPitchState = state;
}

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

    // Level 2 — standard stars: 5–9 (reduced ~50% to not compete with waveform particles)
    const stdCount = 5 + Math.floor(Math.random() * 5);
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

    // Level 3 — micro-dust: 18–28 (reduced ~50% to not compete with waveform particles)
    const microCount = 18 + Math.floor(Math.random() * 11);
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
  retuneKnob: { key: 'retune', min: 0, max: 100, colorA: '#A566FF', colorB: '#D4A7FF', param: 'amount', emphasis: 1.35 },
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
  ring.addColorStop(0.55, isRetune ? '#C88BFF' : '#E0FBFF');
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
    ctx.strokeStyle = `rgba(182, 120, 255, ${0.14 + glowBoost * 0.06})`;
    ctx.lineWidth = 2.0;
    ctx.shadowColor = '#A566FF';
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
  syncPitchCanvasSize();
}

function syncPitchCanvasSize() {
  const c = els.canvas;
  if (!c) return;

  const rect = c.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  const targetW = Math.max(1, Math.floor(rect.width * dpr));
  const targetH = Math.max(1, Math.floor(rect.height * dpr));

  if (c.width !== targetW || c.height !== targetH) {
    c.width = targetW;
    c.height = targetH;
  }
}

function drawGraph() {
  const c = els.canvas;
  if (!c) return;

  // In JUCE webviews, bounds can change after DOMContentLoaded without a window resize.
  // Keep pixel backing size in sync so waveform/particles never render into a stale 1x1 canvas.
  syncPitchCanvasSize();

  const ctx = c.getContext('2d');
  const w = c.width;
  const h = c.height;

  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
  ctx.filter = 'none';

  const centerY = h * 0.36;
  const maxOffset = Math.min(18, Math.max(14, h * 0.09));

  // Guard against any accidental state mutation that could break path rendering.
  if (!Array.isArray(state.trail)) state.trail = [];
  if (!Array.isArray(state.waveParticles)) state.waveParticles = [];
  if (!Number.isFinite(state.phase)) state.phase = 0;
  if (!Number.isFinite(state.key)) state.key = 0;
  if (!Number.isFinite(state.retune)) state.retune = 0;
  if (!Number.isFinite(state.vibrato)) state.vibrato = 0;
  if (typeof state.dsp !== 'object' || state.dsp == null) {
    state.dsp = { correctionAmount: 0, trackingConfidence: 1, retuneActive: false, snapFlash: 0 };
  }
  if (!Number.isFinite(state.dsp.trackingConfidence)) state.dsp.trackingConfidence = 1;
  if (!Number.isFinite(state.dsp.correctionAmount)) state.dsp.correctionAmount = 0;
  if (!Number.isFinite(state.dsp.snapFlash)) state.dsp.snapFlash = 0;

  // Strip any non-finite values that can poison path/particle math.
  state.trail = state.trail.filter((y) => Number.isFinite(y));
  if (state.trail.length > 120) state.trail = state.trail.slice(-120);

  if (w < 4 || h < 4) return;

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
  if (state.trail.length < 24) {
    state.trail.length = 0;
    for (let i = 0; i < 120; i++) {
      const t = i / 119;
      const seedWave = Math.sin(t * Math.PI * 2.0) * maxOffset * 0.34;
      const seedDrift = Math.sin(t * Math.PI * 5.0 + 0.6) * maxOffset * 0.09;
      state.trail.push(centerY + seedWave + seedDrift);
    }
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
  state.trail.push(Number.isFinite(constrainedPitch) ? constrainedPitch : centerY);
  if (state.trail.length > 120) state.trail.shift();

  if (state.trail.length < 2) {
    state.trail = [centerY - maxOffset * 0.15, centerY + maxOffset * 0.15];
  }

  // ── Waveform-driven particle system ─────────────────────
  // Particles spawn ON the pitch line, trail along the curve, then drift outward and fade.
  try {
    const trailN = state.trail.length;
    const dx = w / Math.max(1, trailN - 1);

    // Measure recent motion to scale emission rate.
    let recentMotion = 0;
    for (let i = Math.max(1, trailN - 10); i < trailN; i++) {
      recentMotion += Math.abs(state.trail[i] - state.trail[i - 1]);
    }
    recentMotion /= 9;
    const motionIntensity = Math.max(0.25, Math.min(1, recentMotion / Math.max(1, maxOffset * 0.22)));

    // Emit 18–34 particles per frame, all anchored to trail points.
    const spawnN = Math.round(18 + motionIntensity * 16);
    for (let s = 0; s < spawnN; s++) {
      // Pick a random trail index (weighted toward recent / right side for "live" feel).
      const idx = Math.max(1, Math.min(trailN - 2,
        Math.floor(Math.random() < 0.55
          ? trailN - 1 - Math.random() * trailN * 0.35   // recent 35%
          : Math.random() * (trailN - 2) + 1)));

      const t = idx / (trailN - 1);
      const spawnX = t * w;
      const spawnY = state.trail[idx];

      // Tangent from neighbouring trail points.
      const txRaw = dx * 2;
      const tyRaw = state.trail[idx + 1] - state.trail[idx - 1];
      const tLen = Math.max(0.0001, Math.hypot(txRaw, tyRaw));
      const tanX = txRaw / tLen;
      const tanY = tyRaw / tLen;

      // Normal (perpendicular) — random up or down.
      const ns = Math.random() < 0.5 ? -1 : 1;
      const norX = -tanY * ns;
      const norY =  tanX * ns;

      // Particle trails along curve then disperses very wide from the waveform.
      const along   = 6 + Math.random() * 12;     // px along tangent
      const outward = 132 + Math.random() * 111;  // +50% wider vs previous ghost-trail spread
      const size    = 1 + Math.random() * 2.2;    // 1–3.2px (pixel dust)
      // Life: 70–130 frames (~1.15s–2.15s)
      const lifeFrames  = 70 + Math.random() * 60;
      // Opacity: 42–76% at birth
      const baseOpacity = 0.42 + Math.random() * 0.34;
      const brightness  = 0.92 + Math.random() * 0.14;

      state.waveParticles.push({
        x: spawnX, y: spawnY,
        tanX, tanY, norX, norY,
        along, outward, size, lifeFrames,
        age: 0, baseOpacity, brightness, t,
      });
    }

    // Hard cap to prevent runaway accumulation (max ~760 live particles).
    if (state.waveParticles.length > 760) {
      state.waveParticles = state.waveParticles.slice(-760);
    }

    // Draw and age every particle.
    ctx.save();
    ctx.globalCompositeOperation = 'source-over';
    state.waveParticles = state.waveParticles.filter((p) => p.age < p.lifeFrames);
    for (const p of state.waveParticles) {
      p.age += 1;
      const progress = p.age / p.lifeFrames;
      // Phase 1 (0→0.25): travel along tangent; phase 2: drift outward.
      const alongEase   = Math.min(1, progress / 0.25);
      const outwardEase = Math.max(0, (progress - 0.02) / 0.98);
      // Gentler fade so dust stays visible longer
      const fade = Math.pow(1 - progress, 0.85);

      const disperseJitter = (1 - progress) * (3.0 + motionIntensity * 3.0);
      const px = p.x + p.tanX * p.along * alongEase + p.norX * p.outward * outwardEase
        + (Math.random() - 0.5) * disperseJitter;
      const py = p.y + p.tanY * p.along * alongEase + p.norY * p.outward * outwardEase
        + (Math.random() - 0.5) * disperseJitter;

      // Vivid purple (t=0) → indigo (t=0.4) → bright cyan (t=1) — matches waveform gradient
      const r = Math.round((190 - p.t * 110) * p.brightness);
      const g = Math.round((60  + p.t * 185) * p.brightness);
      const b = Math.round((255 - p.t * 10)  * Math.min(1, p.brightness * 1.02));
      ctx.globalAlpha = p.baseOpacity * fade;
      ctx.fillStyle = `rgb(${r},${g},${b})`;

      // Pixel-dust look: snap to raster and draw tiny square sprites, not soft circles.
      const qx = Math.round(px);
      const qy = Math.round(py);
      const side = Math.max(1, Math.round(p.size));
      ctx.fillRect(qx - (side >> 1), qy - (side >> 1), side, side);
      if (side <= 2 && Math.random() < 0.32) {
        ctx.fillRect(qx + 1, qy, 1, 1);
      }
    }
    ctx.globalAlpha = 1;
    ctx.restore();

    // ── Micro sparkles: tiny bright bursts ON the line every 3–5s ───
    if (!state._sparkles) state._sparkles = [];
    // Spawn chance: ~1 every 4s @ 60fps
    if (Math.random() < 1 / 240) {
      const idx = Math.floor(Math.random() * (trailN - 2)) + 1;
      state._sparkles.push({
        x: (idx / (trailN - 1)) * w,
        y: state.trail[idx],
        age: 0,
        life: Math.round(10 + Math.random() * 5), // <250ms
        t: idx / (trailN - 1),
      });
    }
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    state._sparkles = state._sparkles.filter((sp) => sp.age < sp.life);
    for (const sp of state._sparkles) {
      sp.age++;
      const sfade = Math.pow(1 - sp.age / sp.life, 1.4);
      const sr = Math.round(200 + sp.t * 55);
      const sg = Math.round(200 + sp.t * 55);
      const sb = 255;
      ctx.globalAlpha = 0.72 * sfade;
      ctx.fillStyle = `rgb(${sr},${sg},${sb})`;
      const sx = Math.round(sp.x);
      const sy = Math.round(sp.y);
      ctx.fillRect(sx - 1, sy - 1, 3, 3);
      ctx.fillRect(sx, sy, 1, 1);
    }
    ctx.globalAlpha = 1;
    ctx.restore();

  } catch (err) {
    state.waveParticles = [];
    state._sparkles = [];
    console.warn('[NovaPitch UI] Particle system error:', err);
  }

  // ── Pitch line — purple → blue → cyan gradient ────────────
  const trailLen = state.trail.length;

  if (trailLen < 2) {
    const fallbackY = centerY + Math.sin(state.phase * 1.9) * (maxOffset * 0.28);
    ctx.strokeStyle = 'rgba(176, 244, 255, 0.95)';
    ctx.lineWidth = 2.2;
    ctx.beginPath();
    ctx.moveTo(0, fallbackY);
    ctx.lineTo(w, fallbackY);
    ctx.stroke();
    return;
  }

  const buildPath = () => {
    ctx.beginPath();
    for (let i = 0; i < trailLen; i++) {
      const x = (i / (trailLen - 1)) * w;
      const yRaw = state.trail[i];
      const y = Number.isFinite(yRaw) ? yRaw : centerY;
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
  };

  // ── Waveform line render — deep purple tail → bright cyan head ──────────────
  // The gradient runs left (history = purple/violet) → right (live = cyan-white)
  // matching the reference design.
  ctx.globalCompositeOperation = 'lighter';
  ctx.lineCap  = 'round';
  ctx.lineJoin = 'round';

  // Outer glow layer — wide bloom, strong purple on left
  const glowGrad = ctx.createLinearGradient(0, 0, w, 0);
  glowGrad.addColorStop(0.00, 'rgba(130, 60, 255, 0.28)');   // deep purple bloom left
  glowGrad.addColorStop(0.35, 'rgba(110, 100, 255, 0.20)');  // purple-indigo mid
  glowGrad.addColorStop(0.65, 'rgba(70, 170, 255, 0.18)');   // blue
  glowGrad.addColorStop(1.00, 'rgba(100, 240, 255, 0.32)');  // cyan bloom right
  ctx.strokeStyle = glowGrad;
  ctx.lineWidth   = 13;
  ctx.shadowColor = '#7040FF';
  ctx.shadowBlur  = 18;
  buildPath(); ctx.stroke();

  // Mid glow — tighter bloom matches reference purple shoulder
  const midGlowGrad = ctx.createLinearGradient(0, 0, w, 0);
  midGlowGrad.addColorStop(0.00, 'rgba(170, 80, 255, 0.38)');  // vivid purple
  midGlowGrad.addColorStop(0.30, 'rgba(140, 120, 255, 0.32)'); // purple-blue
  midGlowGrad.addColorStop(0.65, 'rgba(80, 190, 255, 0.28)');  // blue
  midGlowGrad.addColorStop(1.00, 'rgba(120, 248, 255, 0.40)'); // bright cyan
  ctx.strokeStyle = midGlowGrad;
  ctx.lineWidth   = 5;
  ctx.shadowColor = '#9050FF';
  ctx.shadowBlur  = 10;
  buildPath(); ctx.stroke();

  // Core line — the actual drawn path, purple left → cyan-white right
  const coreGrad = ctx.createLinearGradient(0, 0, w, 0);
  coreGrad.addColorStop(0.00, 'rgba(190, 110, 255, 0.90)');  // deep violet-purple
  coreGrad.addColorStop(0.20, 'rgba(175, 120, 255, 0.92)');  // purple extends through left third
  coreGrad.addColorStop(0.42, 'rgba(130, 170, 255, 0.95)');  // purple-blue blend
  coreGrad.addColorStop(0.65, 'rgba(100, 220, 255, 0.98)');  // sky blue
  coreGrad.addColorStop(0.85, 'rgba(160, 248, 255, 1.00)');  // near-white cyan
  coreGrad.addColorStop(1.00, 'rgba(220, 255, 255, 1.00)');  // bright white-cyan head
  ctx.strokeStyle = coreGrad;
  ctx.lineWidth   = 2.6;
  ctx.shadowColor = '#AA60FF';
  ctx.shadowBlur  = 8;
  buildPath(); ctx.stroke();

  // Hair-line specular on top of core — white-cyan only on the right half
  const specGrad = ctx.createLinearGradient(0, 0, w, 0);
  specGrad.addColorStop(0.00, 'rgba(190, 110, 255, 0.00)');  // invisible on left
  specGrad.addColorStop(0.45, 'rgba(220, 248, 255, 0.00)');  // fades in
  specGrad.addColorStop(0.70, 'rgba(235, 252, 255, 0.45)');  // white-cyan specular
  specGrad.addColorStop(1.00, 'rgba(255, 255, 255, 0.62)');  // bright at head
  ctx.globalCompositeOperation = 'source-over';
  ctx.strokeStyle = specGrad;
  ctx.lineWidth = 1.1;
  ctx.shadowBlur = 0;
  buildPath(); ctx.stroke();

  // Guaranteed visibility — gradient passthrough so purple left is preserved
  ctx.globalCompositeOperation = 'source-over';
  const visGrad = ctx.createLinearGradient(0, 0, w, 0);
  visGrad.addColorStop(0.00, 'rgba(185, 108, 255, 0.88)');  // purple left
  visGrad.addColorStop(0.40, 'rgba(120, 160, 255, 0.90)');  // indigo mid
  visGrad.addColorStop(1.00, 'rgba(176, 244, 255, 0.96)');  // cyan right
  ctx.strokeStyle = visGrad;
  ctx.lineWidth = 2.45;
  ctx.shadowBlur = 0;
  buildPath();
  ctx.stroke();

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

  drawSafetyWaveOverlay(ctx, w, h);
}

// Safety overlay removed — particle system now handles all waveform-driven effects.
function drawSafetyWaveOverlay() {}

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
  state.phase += 0.020;
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
  { id: 'nova-sig',   section: 'Signature', name: 'Nova Signature', description: 'Warm polished vocal tone', tags: ['smooth','bright'],
    values: { retune: 9, humanize: 60, flex: 55, vibrato: 12, formant: 52 } },
  { id: 'midnight',   section: 'Signature', name: 'Midnight Glow',  description: 'Dreamy melodic harmony', tags: ['smooth','bright'],
    values: { retune: 50, humanize: 70, flex: 65, vibrato: 18, formant: 55 } },
  { id: 'atl-glide',  section: 'Signature', name: 'ATL Glide',      description: 'Hard tuned robotic trap', tags: ['robotic','aggressive'],
    values: { retune: 28, humanize: 40, flex: 45, vibrato: 10, formant: 50 } },

  // ── Core ───────────────────────────────────────
  { id: 'natural',    section: 'Core',      name: 'Natural',        description: 'Subtle and transparent', tags: ['smooth','natural'],
    values: { retune: 65, humanize: 80, flex: 75, vibrato: 10, formant: 50 } },
  { id: 'smooth',     section: 'Core',      name: 'Smooth',         description: 'Polished and modern', tags: ['smooth','bright'],
    values: { retune: 45, humanize: 65, flex: 60, vibrato: 15, formant: 50 } },
  { id: 'tight',      section: 'Core',      name: 'Tight',          description: 'Controlled and focused', tags: ['aggressive'],
    values: { retune: 18, humanize: 25, flex: 30, vibrato:  5, formant: 50 } },
  { id: 'hard-tune',  section: 'Core',      name: 'Hard Tune',      description: 'Hard tuned robotic trap', tags: ['robotic','aggressive'],
    values: { retune:   5, humanize: 0, flex: 10, vibrato:  0, formant: 52 } },

  // ── Creative ───────────────────────────────────
  { id: 'robot',      section: 'Creative',  name: 'Robot',          description: 'Synthetic robotic contour', tags: ['robotic','aggressive'],
    values: { retune:   2, humanize: 0, flex:  0, vibrato:  0, formant: 60 } },
  { id: 'glide',      section: 'Creative',  name: 'Glide',          description: 'Smooth melodic transitions', tags: ['smooth','natural'],
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
    const secLower = p.section.toLowerCase();
    if (cat !== 'all' && secLower !== cat) return false;
    if (tag && !p.tags.includes(tag)) return false;
    const searchable = `${p.name} ${p.description || ''}`.toLowerCase();
    if (q && !searchable.includes(q)) return false;
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

  // Group by section maintaining order: Signature→Core→Creative
  const sections = ['Signature', 'Core', 'Creative'];

  const buildItem = (preset) => {
    const item = document.createElement('div');
    item.className = 'preset-item'
      + (preset.id === state.preset.activeId ? ' is-active' : '')
      + (preset.section === 'Signature' ? ' is-signature' : '');
    item.setAttribute('role', 'option');
    item.setAttribute('aria-selected', preset.id === state.preset.activeId ? 'true' : 'false');

    const lead = document.createElement('div');
    lead.className = 'preset-item__lead';
    lead.textContent = preset.section === 'Signature' ? '★' : '✦';

    const body = document.createElement('div');
    body.className = 'preset-item__body';

    const name = document.createElement('div');
    name.className = 'preset-item__name';
    name.textContent = preset.name;

    const desc = document.createElement('div');
    desc.className = 'preset-item__desc';
    desc.textContent = preset.description || 'Curated vocal profile';

    body.appendChild(name);
    body.appendChild(desc);

    const tagsWrap = document.createElement('div');
    tagsWrap.className = 'preset-item__tags';
    preset.tags.forEach(t => {
      const span = document.createElement('span');
      span.className = 'preset-item__tag';
      span.dataset.tag = t;
      span.textContent = t;
      tagsWrap.appendChild(span);
    });

    const go = document.createElement('div');
    go.className = 'preset-item__go';
    go.textContent = '›';

    item.appendChild(lead);
    item.appendChild(body);
    item.appendChild(tagsWrap);
    item.appendChild(go);

    item.addEventListener('click', () => {
      applyPreset(preset);
      closePresetPanel();
      item.classList.remove('is-pulsing');
      void item.offsetWidth;
      item.classList.add('is-pulsing');
      setTimeout(() => item.classList.remove('is-pulsing'), 160);
    });
    return item;
  };

  sections.forEach(sec => {
    const group = filtered.filter(p => p.section === sec);
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
