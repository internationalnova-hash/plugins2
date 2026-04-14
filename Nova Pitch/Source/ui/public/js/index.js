'use strict';

const state = {
  retune: 88,
  humanize: 34,
  flex: 52,
  vibrato: 26,
  formant: 50,
  key: 0,
  scale: 1,
  phase: 0,
  trail: [],
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
  if (id === 'retuneKnob') return `${Math.round(100 - value)}%`;
  return `${Math.round(value)}%`;
}

function updateKnobReadout(id, value) {
  const readout = document.getElementById(`${id}Value`);
  if (!readout) return;
  readout.textContent = formatKnobValue(id, value);
}

function callNative(name, payload) {
  if (!window.NativeBridge) return null;
  try {
    if (typeof window.NativeBridge.callNativeFunction === 'function') {
      return window.NativeBridge.callNativeFunction(name, payload);
    }

    // JUCE interop compatibility path (window.NativeBridge = window.JuceAPI)
    if (name === 'setParameter' && typeof window.NativeBridge.setParameter === 'function') {
      return window.NativeBridge.setParameter(payload.parameter, payload.value);
    }
    if (name === 'getParameter' && typeof window.NativeBridge.getParameter === 'function') {
      return window.NativeBridge.getParameter(payload.parameter);
    }
    if (name === 'setLowLatencyMode') {
      if (typeof window.NativeBridge.setParameter === 'function') {
        return window.NativeBridge.setParameter('lowLatency', payload.enabled ? 1 : 0);
      }
      if (typeof window.NativeBridge.send === 'function') {
        return window.NativeBridge.send('setLowLatencyMode', payload);
      }
    }
    if (typeof window.NativeBridge.send === 'function') {
      return window.NativeBridge.send(name, payload);
    }
    return null;
  } catch (_) {
    return null;
  }
}

function syncParam(param, value) {
  callNative('setParameter', { parameter: param, value });
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

  const handled = callNative('setLowLatencyMode', { enabled: next });
  if (handled == null) syncParam('lowLatency', next ? 1 : 0);
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
  body.addColorStop(0, 'rgba(240,247,255,0.48)');
  body.addColorStop(0.3, 'rgba(140,158,176,0.56)');
  body.addColorStop(0.62, 'rgba(56,70,86,0.72)');
  body.addColorStop(1, 'rgba(14,20,28,0.96)');
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
  ctx.strokeStyle = 'rgba(232, 245, 255, 0.28)';
  ctx.lineWidth = Math.max(2.6, r * 0.06);
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
    ? 10.5 + 4.2 * emphasis + 6.4 * glowBoost
    : 7 + 2 * emphasis + 8 * glowBoost;
  ctx.beginPath();
  ctx.arc(cx, cy, r - 7, start, end);
  ctx.stroke();

  // Retune additional outer halo — spec: 10-16% alpha, 18-34px spread
  if (isRetune) {
    ctx.strokeStyle = `rgba(106, 239, 255, ${0.08 + glowBoost * 0.045})`;
    ctx.lineWidth = 1.5;
    ctx.shadowColor = '#6AEFFF';
    ctx.shadowBlur = 14 + glowBoost * 12;
    ctx.beginPath();
    ctx.arc(cx, cy, r - 5, start, end);
    ctx.stroke();
  }

  // Crisp LED edge accent to make the ring read brighter.
  ctx.strokeStyle = 'rgba(230, 251, 255, 0.72)';
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

  ctx.fillStyle = 'rgba(20,26,34,0.92)';
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

    const normalized = (next - def.min) / (def.max - def.min);
    syncParam(def.param, normalized);
  };

  const end = () => {
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

  ctx.clearRect(0, 0, w, h);

  const bg = ctx.createLinearGradient(0, 0, 0, h);
  bg.addColorStop(0, '#132737');
  bg.addColorStop(1, '#0b1621');
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, w, h);

  const centerGlow = ctx.createRadialGradient(w * 0.52, h * 0.48, 10, w * 0.52, h * 0.48, h * 0.7);
  centerGlow.addColorStop(0, 'rgba(88, 200, 255, 0.2)');
  centerGlow.addColorStop(0.48, 'rgba(66, 120, 210, 0.1)');
  centerGlow.addColorStop(1, 'rgba(0, 0, 0, 0)');
  ctx.fillStyle = centerGlow;
  ctx.fillRect(0, 0, w, h);

  // Focus zone — narrow horizontal band under pitch line, 8–14% opacity
  const focusY = state.smoothPitch != null ? state.smoothPitch : h * 0.55;
  const focusZone = ctx.createRadialGradient(w * 0.5, focusY, 0, w * 0.5, focusY, w * 0.22);
  focusZone.addColorStop(0,   `rgba(72, 196, 255, ${0.11 + state.signalLevel * 0.03})`);
  focusZone.addColorStop(0.6, `rgba(60, 160, 240, ${0.06 + state.signalLevel * 0.02})`);
  focusZone.addColorStop(1,   'rgba(0,0,0,0)');
  ctx.fillStyle = focusZone;
  ctx.fillRect(w * 0.1, focusY - h * 0.18, w * 0.8, h * 0.36);

  const magentaMist = ctx.createRadialGradient(w * 0.38, h * 0.56, 10, w * 0.38, h * 0.56, h * 0.5);
  magentaMist.addColorStop(0, 'rgba(223, 72, 218, 0.08)');
  magentaMist.addColorStop(1, 'rgba(0, 0, 0, 0)');
  ctx.fillStyle = magentaMist;
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = 'rgba(160,195,222,0.13)';
  ctx.lineWidth = 1;
  for (let i = 0; i < 12; i++) {
    const y = (h / 12) * i;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }
  for (let i = 0; i < 10; i++) {
    const x = (w / 9) * i;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
  }

  // Very faint animated grain so the graph bed is not flat.
  ctx.strokeStyle = 'rgba(210, 228, 245, 0.03)';
  ctx.lineWidth = 1;
  for (let y = 0; y < h; y += 3) {
    const offset = Math.sin(y * 0.12 + state.phase * 4.2) * 1.2;
    ctx.beginPath();
    ctx.moveTo(offset, y + 0.5);
    ctx.lineTo(w + offset, y + 0.5);
    ctx.stroke();
  }

  // Playhead beam — spec: core #8DF3FF 1.5–2.5px 55–80%, outer glow #59E8FF 14–26%, pulse ±8% at 2.4–3.2s
  const markerX = ((state.phase * 1.6) % 1) * w;
  const playPulseRate = (2 * Math.PI) / (2.8 * 60 * 0.025 / 0.025); // ~2.8s cycle
  const playPulse = 0.5 + 0.5 * Math.sin(state.phase * playPulseRate * 40);
  // Glow intensifies slightly with signalLevel — feels more active when waveform moves
  const playheadBoost = state.signalLevel * 0.08;
  const coreBrightness = 0.55 + playPulse * 0.16 + playheadBoost;
  const outerGlowAlpha = 0.14 + playPulse * 0.12 + playheadBoost * 0.5;

  // Outer soft halo fill
  const haloW = 22;
  const haloGrad = ctx.createLinearGradient(markerX - haloW, 0, markerX + haloW, 0);
  haloGrad.addColorStop(0, 'rgba(0,0,0,0)');
  haloGrad.addColorStop(0.5, `rgba(89,232,255,${outerGlowAlpha})`);
  haloGrad.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = haloGrad;
  ctx.fillRect(markerX - haloW, 0, haloW * 2, h);

  // Outer glow line
  ctx.strokeStyle = `rgba(89, 232, 255, ${outerGlowAlpha})`;
  ctx.lineWidth = 2.0;
  ctx.shadowColor = '#59E8FF';
  ctx.shadowBlur = 16 + playPulse * 8; // 16–24px, spec 14–26px
  ctx.beginPath(); ctx.moveTo(markerX, 0); ctx.lineTo(markerX, h); ctx.stroke();

  // Core bright line
  ctx.strokeStyle = `rgba(141, 243, 255, ${coreBrightness})`;
  ctx.lineWidth = 1.8;
  ctx.shadowColor = '#8DF3FF';
  ctx.shadowBlur = 4;
  ctx.beginPath(); ctx.moveTo(markerX, 0); ctx.lineTo(markerX, h); ctx.stroke();
  ctx.shadowBlur = 0;

  if (state.trail.length === 0) {
    for (let i = 0; i < 180; i++) state.trail.push(h * 0.58);
  }

  const natural = h * (0.56 + Math.sin(state.phase * 1.7) * 0.12 + Math.sin(state.phase * 0.43) * 0.08);
  const target = h * (0.52 - (state.key / 11) * 0.08);

  // Retune Speed is the primary intensity control.
  const retuneBlend = state.retune / 100;
  const correctedCore = natural + (target - natural) * retuneBlend;

  // ── DSP Intensity Engine ─────────────────────────────
  const correctionDist = Math.abs(natural - target);
  // Override with live DSP data when available; fall back to derived value
  if (!state.dsp.retuneActive) {
    state.dsp.correctionAmount = Math.min(1, correctionDist / (h * 0.18));
  }
  const dspIntensity = Math.min(1, state.dsp.correctionAmount * (0.3 + retuneBlend * 0.7));
  // Snap flash: fast retune + high correction = brief bright pulse at playhead
  if (state.dsp.snapFlash > 0) state.dsp.snapFlash *= 0.88;
  if (retuneBlend > 0.85 && state.dsp.correctionAmount > 0.35) {
    state.dsp.snapFlash = Math.min(1, state.dsp.snapFlash + 0.06);
  }

  // Vibrato is applied after pitch correction.
  const vibDepth = (state.vibrato / 100) * (h * 0.04);
  const vibRate = 1.2 + (state.vibrato / 100) * 4.0;
  const correctedWithVibrato = correctedCore + Math.sin(state.phase * vibRate) * vibDepth;

  // Smooth interpolation — low latency mode reduces smoothing for rawer, more responsive feel.
  if (state.smoothPitch == null) state.smoothPitch = correctedWithVibrato;
  const smoothBase   = state.lowLatency ? 0.44 : 0.20;
  const smoothAmount = Math.min(0.76, smoothBase + retuneBlend * 0.18);
  state.smoothPitch += (correctedWithVibrato - state.smoothPitch) * smoothAmount;

  const drift = Math.abs(correctedWithVibrato - state.smoothPitch) / Math.max(1, h * 0.12);
  const pseudoInput = 0.44 + Math.abs(Math.sin(state.phase * 0.9)) * 0.32;
  state.signalLevel = Math.max(0.2, Math.min(1, pseudoInput + drift * 0.35));

  // Stability jitter — subtle noise on low tracking confidence (1.0 = stable in sim).
  const jitterAmt = (1 - state.dsp.trackingConfidence) * 2.4;
  const jitter    = jitterAmt > 0.2 ? (Math.random() - 0.5) * jitterAmt * 2 : 0;
  // Vibrato glow boost for trail
  const vibratoTrailBoost = (state.vibrato / 100) * 0.08;
  state.trail.push(state.smoothPitch + jitter);
  if (state.trail.length > 220) state.trail.shift();

  const playheadIndex = Math.max(0, Math.min(state.trail.length - 1, Math.round(((state.phase * 1.6) % 1) * (state.trail.length - 1))));
  const playheadY = state.trail[playheadIndex] ?? state.smoothPitch;
  // Pitch line pulse: ±3–4% brightness tied to signal — restrained, never distracting
  const linePulse = 0.965 + (0.035 + state.signalLevel * 0.025) * (0.5 + 0.5 * Math.sin(state.phase * 2.3));

  const crossGlow = ctx.createRadialGradient(markerX, playheadY, 0, markerX, playheadY, 20 + state.signalLevel * 8);
  crossGlow.addColorStop(0, `rgba(246, 253, 255, ${0.26 + playPulse * 0.16})`);
  crossGlow.addColorStop(0.35, `rgba(89, 232, 255, ${0.16 + state.signalLevel * 0.1})`);
  crossGlow.addColorStop(1, 'rgba(89, 232, 255, 0)');
  ctx.fillStyle = crossGlow;
  ctx.fillRect(markerX - 32, playheadY - 32, 64, 64);

  // Correction beam — energy line showing pitch being pulled toward target.
  if (correctionDist > 5 && state.dsp.correctionAmount > 0.04) {
    const by1   = Math.min(state.smoothPitch, target);
    const by2   = Math.max(state.smoothPitch, target);
    const bGrad = ctx.createLinearGradient(markerX, by1, markerX, by2);
    bGrad.addColorStop(0,    `rgba( 89,232,255, ${state.dsp.correctionAmount * 0.42})`);
    bGrad.addColorStop(0.42, `rgba(175, 95,255, ${state.dsp.correctionAmount * 0.28})`);
    bGrad.addColorStop(0.58, `rgba(175, 95,255, ${state.dsp.correctionAmount * 0.28})`);
    bGrad.addColorStop(1,    `rgba(217, 92,255, ${state.dsp.correctionAmount * 0.42})`);
    ctx.strokeStyle  = bGrad;
    ctx.lineWidth    = 1.5 + state.dsp.correctionAmount * 1.5;
    ctx.lineCap      = 'round';
    ctx.shadowColor  = 'rgba(160,80,255,0.7)';
    ctx.shadowBlur   = 3 + dspIntensity * 12;
    ctx.beginPath();
    ctx.moveTo(markerX, by1);
    ctx.lineTo(markerX, by2);
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  // Correction line — spec: #D95CFF, 1.2–1.8px, 70–85% opacity, glow 4–10px
  ctx.strokeStyle = 'rgba(217, 92, 255, 0.78)';
  ctx.lineWidth = 1.5;
  ctx.shadowColor = '#D95CFF';
  ctx.shadowBlur = 7; // spec 4–10px
  ctx.beginPath();
  ctx.moveTo(0, target);
  ctx.lineTo(w, target);
  ctx.stroke();
  ctx.shadowBlur = 0;

  // Glow trail pass.
  for (let i = 1; i < state.trail.length; i++) {
    const t = i / (state.trail.length - 1);
    const prevX = ((i - 1) / (state.trail.length - 1)) * w;
    const prevY = state.trail[i - 1];
    const x = t * w;
    const y = state.trail[i];
    ctx.strokeStyle = `rgba(63,233,247,${0.04 + t * (0.26 + vibratoTrailBoost)})`;
    ctx.lineWidth = 1.8 + t * 2.2;
    ctx.shadowColor = 'rgba(63,233,247,0.28)';
    ctx.shadowBlur = 6 + t * 3;
    ctx.beginPath();
    ctx.moveTo(prevX, prevY);
    ctx.lineTo(x, y);
    ctx.stroke();
  }
  ctx.shadowBlur = 0;

  const path = () => {
    ctx.beginPath();
    for (let i = 0; i < state.trail.length; i++) {
      const x = (i / (state.trail.length - 1)) * w;
      const y = state.trail[i];
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
  };

  // Formant color tint: above 50%, shift outer aura slightly toward violet.
  const fTint = Math.max(0, (state.formant / 100 - 0.5) * 2); // 0 at ≤50%, 1 at 100%
  const auraR = Math.round(89  + fTint * 66);  // 89 → 155
  const auraG = Math.round(232 - fTint * 102); // 232 → 130

  // Spec: outer glow with DSP intensity boost
  ctx.globalCompositeOperation = 'lighter';
  ctx.lineCap = 'round'; ctx.lineJoin = 'round';
  ctx.strokeStyle = `rgba(${auraR}, ${auraG}, 255, ${(0.21 + state.signalLevel * 0.13 + dspIntensity * 0.08) * linePulse})`;
  ctx.lineWidth    = 10 + dspIntensity * 2;
  ctx.shadowColor  = `rgb(${auraR}, ${auraG}, 255)`;
  ctx.shadowBlur   = 14 + state.signalLevel * 7 + dspIntensity * 6;
  path(); ctx.stroke();

  // Core stroke — slightly brighter to give the Nova 'energy' feel
  ctx.strokeStyle = `rgba(200, 250, 255, ${0.97 * linePulse})`;
  ctx.lineWidth = 2.4;
  ctx.shadowColor = '#59E8FF';
  ctx.shadowBlur = 10;
  path(); ctx.stroke();

  // Inner near-white highlight — bumped to push toward flagship brightness
  ctx.strokeStyle = `rgba(255, 255, 255, ${(0.34 + state.signalLevel * 0.13) * linePulse})`;
  ctx.lineWidth = 1.0;
  ctx.shadowBlur = 5;
  path(); ctx.stroke();

  ctx.shadowBlur = 0;
  ctx.globalCompositeOperation = 'source-over';

  // Bright head highlight.
  const headY = state.trail[state.trail.length - 1];
  ctx.fillStyle = 'rgba(180, 248, 255, 0.9)';
  ctx.shadowColor = 'rgba(120, 245, 255, 0.65)';
  ctx.shadowBlur = 12;
  ctx.beginPath();
  ctx.arc(w - 6, headY, 2.2, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;

  ctx.fillStyle   = `rgba(246, 253, 255, ${0.86 + playPulse * 0.1})`;
  ctx.shadowColor = 'rgba(140, 244, 255, 0.78)';
  ctx.shadowBlur  = 14;
  ctx.beginPath();
  ctx.arc(markerX, playheadY, 1.9 + state.signalLevel * 0.5, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;

  // Snap flash — brief bright pulse on fast pitch snap (high retune + high correction).
  if (state.dsp.snapFlash > 0.05) {
    ctx.globalCompositeOperation = 'lighter';
    const sf = state.dsp.snapFlash;
    const sfGrad = ctx.createRadialGradient(markerX, state.smoothPitch, 0, markerX, state.smoothPitch, 44 * sf);
    sfGrad.addColorStop(0, `rgba(200, 250, 255, ${sf * 0.26})`);
    sfGrad.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = sfGrad;
    ctx.fillRect(markerX - 50, state.smoothPitch - 50, 100, 100);
    ctx.globalCompositeOperation = 'source-over';
  }

  // Highlight detected pitch note for vertical keyboard reference.
  const detectedMidi = 60 + Math.round((1 - natural / h) * 12) + state.key;
  const detectedNote = midiToNoteName(Math.max(24, Math.min(71, detectedMidi)));
  els.currentNote.textContent = detectedNote;
  setActivePianoNote(detectedNote);
}

function setupTopBar() {
  els.keySelect.value = noteNames[state.key];

  els.keySelect.addEventListener('change', () => {
    state.key = Math.max(0, noteNames.indexOf(els.keySelect.value));
    syncParam('key', state.key / 11);
  });

  els.scaleButton.addEventListener('click', () => {
    state.scale = (state.scale + 1) % scales.length;
    els.scaleButton.textContent = scales[state.scale];
    syncParam('scale', state.scale / (scales.length - 1));
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
  // Preset retune values are authored as ms (0..80).
  // Internal knob space is 0..100 where higher = faster retune.
  const retuneMs = Math.max(0, Math.min(80, v.retune));
  state.retune    = 100 - (retuneMs / 80) * 100;
  state.humanize  = v.humanize;
  state.flex      = v.flex;
  state.vibrato   = v.vibrato;
  state.formant   = v.formant;

  // Sync native + refresh knob readouts
  syncParam('amount',                state.retune   / 100);
  syncParam('confidenceThreshold',   state.humanize / 100);
  syncParam('tolerance',             state.flex     / 100);
  syncParam('vibrato',               state.vibrato  / 100);
  syncParam('formant',               state.formant  / 100);

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
  buildPianoKeys();
  setupTopBar();
  resizeCanvas();
  initBgCanvas();
  Object.keys(knobDefs).forEach(bindKnob);

  syncParam('amount', state.retune / 100);
  syncParam('tolerance', state.flex / 100);
  syncParam('confidenceThreshold', state.humanize / 100);
  syncParam('vibrato', state.vibrato / 100);
  syncParam('formant', state.formant / 100);
  syncParam('key', state.key / 11);
  syncParam('scale', state.scale / (scales.length - 1));
  setLowLatency(state.lowLatency, { animate: false });
  setupPresetBrowser();

  animate();
}

window.addEventListener('resize', resizeCanvas);
window.addEventListener('DOMContentLoaded', init);
