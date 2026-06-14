'use strict';

const state = {
  retune: 20,
  tolerance: 0,
  amount: 100,
  formant: 1,
  key: 0,
  scale: 0,
  phase: 0,
  trail: [],
  waveParticles: [],
  smoothPitch: null,
  knobHover: null,
  knobActive: null,
  retuneDrive: 0,
  signalLevel: 0.55,
  // DSP sync — driven by NativeBridge or derived from simulation
  dsp: {
    correctionAmount:    0,
    trackingConfidence:  1,
    retuneActive:        false,
    snapFlash:           0,
  },
  // Preset browser
  preset: {
    open:      false,
    activeId:  null,
    query:     '',
    category:  'all',
    activeTag: null,
  },
};

if (typeof window !== 'undefined') {
  window.__novaAutotuneState = state;
}

const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const scales = ['Chromatic', 'Major', 'Minor', 'Pentatonic', 'Blues', 'Dorian'];

const els = {
  canvas:         document.getElementById('pitchCanvas'),
  bgCanvas:       document.getElementById('bg-canvas'),
  keySelect:      document.getElementById('keySelect'),
  scaleButton:    document.getElementById('scaleButton'),
  currentNote:    document.getElementById('currentNote'),
  pianoKeys:      document.getElementById('pianoKeys'),
  presetToggle:   document.getElementById('presetToggle'),
  presetPanel:    document.getElementById('presetPanel'),
  presetClose:    document.getElementById('presetClose'),
  presetSearch:   document.getElementById('presetSearch'),
  presetCategory: document.getElementById('presetCategory'),
  presetList:     document.getElementById('presetList'),
};

const paramBridges = {};

function hasJuceBackend() {
  return typeof window !== 'undefined' && !!(window.__JUCE__ && window.__JUCE__.backend);
}

function getParamRange(param) {
  if (param === 'key')         return { min: 0, max: 11 };
  if (param === 'scale')       return { min: 0, max: scales.length - 1 };
  if (param === 'tolerance')   return { min: 0, max: 50 };
  if (param === 'formant')     return { min: 0, max: 1 };
  return { min: 0, max: 100 };
}

function getParamBridge(param) {
  if (paramBridges[param]) return paramBridges[param];
  if (!hasJuceBackend()) {
    return null;
  }
  const identifier = `__juce__slider${param}`;
  const range = getParamRange(param);
  const bridge = {
    setValue(actualValue) {
      const clamped = Math.max(range.min, Math.min(range.max, actualValue));
      window.__JUCE__.backend.emitEvent(identifier, { eventType: 'valueChanged', value: clamped });
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

function sendParam(param, value) {
  const bridge = getParamBridge(param);
  if (bridge) bridge.setValue(value);
}

// DSP telemetry receiver — called from C++ via evaluateJavascript
window.receiveDSP = function(data) {
  if (typeof data.correctionAmount   === 'number') state.dsp.correctionAmount   = data.correctionAmount;
  if (typeof data.trackingConfidence === 'number') state.dsp.trackingConfidence = data.trackingConfidence;
  if (typeof data.retuneActive       === 'boolean') state.dsp.retuneActive      = data.retuneActive;

  if (state.dsp.retuneActive && state.retune < 5) {
    state.dsp.snapFlash = 1.0;
  }
};

// ── Knob canvases ─────────────────────────────────────────────────────────────
const knobs = [
  { id: 'toleranceKnob',  valueId: 'toleranceKnobValue', param: 'tolerance',   min: 0, max: 50,  step: 0.5,  label: v => v.toFixed(1) + ' st' },
  { id: 'retuneKnob',     valueId: 'retuneKnobValue',    param: 'retuneSpeed', min: 0, max: 100, step: 1,    label: v => {
    if (v < 1) return '0 (Snap)';
    if (v < 20) return v.toFixed(0) + ' (Fast)';
    if (v < 60) return v.toFixed(0) + ' (Med)';
    return v.toFixed(0) + ' (Slow)';
  }},
  { id: 'amountKnob',     valueId: 'amountKnobValue',    param: 'amount',      min: 0, max: 100, step: 1,    label: v => v.toFixed(0) + '%' },
  { id: 'formantKnob',    valueId: 'formantKnobValue',   param: 'formant',     min: 0, max: 1,   step: 1,    label: v => v > 0.5 ? 'On' : 'Off' },
  { id: 'dummyKnob',      valueId: 'dummyKnobValue',     param: null,          min: 0, max: 100, step: 1,    label: () => '—' },
];

const knobValues = {};
knobs.forEach(k => {
  if      (k.param === 'retuneSpeed') knobValues[k.id] = state.retune;
  else if (k.param === 'tolerance')   knobValues[k.id] = state.tolerance;
  else if (k.param === 'amount')      knobValues[k.id] = state.amount;
  else if (k.param === 'formant')     knobValues[k.id] = state.formant;
  else                                knobValues[k.id] = k.min;
});

function drawKnob(canvas, value, min, max, isRetune, isDragging) {
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);
  const cx = W / 2, cy = H / 2;
  const r = Math.min(W, H) * 0.38;
  const startAngle = Math.PI * 0.75;
  const endAngle   = Math.PI * 2.25;
  const t = (value - min) / (max - min + 1e-9);
  const valAngle = startAngle + t * (endAngle - startAngle);

  // Track background
  ctx.beginPath();
  ctx.arc(cx, cy, r, startAngle, endAngle);
  ctx.strokeStyle = 'rgba(40,60,80,0.9)';
  ctx.lineWidth = r * 0.22;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Value arc
  if (t > 0.001) {
    const grad = ctx.createLinearGradient(cx - r, cy, cx + r, cy);
    grad.addColorStop(0, isRetune ? '#3ee8f7' : '#4ea6ff');
    grad.addColorStop(1, isRetune ? '#df48da' : '#3ee8f7');
    ctx.beginPath();
    ctx.arc(cx, cy, r, startAngle, valAngle);
    ctx.strokeStyle = grad;
    ctx.lineWidth = r * 0.22;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Glow
    if (isRetune && state.dsp.retuneActive) {
      ctx.beginPath();
      ctx.arc(cx, cy, r, startAngle, valAngle);
      ctx.strokeStyle = 'rgba(62,232,247,0.18)';
      ctx.lineWidth = r * 0.44;
      ctx.lineCap = 'round';
      ctx.stroke();
    }
  }

  // Pointer dot
  const dotX = cx + Math.cos(valAngle) * r;
  const dotY = cy + Math.sin(valAngle) * r;
  ctx.beginPath();
  ctx.arc(dotX, dotY, r * 0.14, 0, Math.PI * 2);
  ctx.fillStyle = isDragging ? '#fff' : '#ddf6ff';
  ctx.shadowColor = '#3ee8f7';
  ctx.shadowBlur = 8;
  ctx.fill();
  ctx.shadowBlur = 0;
}

function redrawAllKnobs() {
  knobs.forEach(k => {
    const canvas = document.getElementById(k.id);
    if (!canvas) return;
    const val = knobValues[k.id];
    const isRetune = k.param === 'retuneSpeed';
    const isDragging = state.knobActive === k.id;
    drawKnob(canvas, val, k.min, k.max, isRetune, isDragging);
    const valEl = document.getElementById(k.valueId);
    if (valEl) valEl.textContent = k.label(val);
  });
}

// Knob drag interaction
let dragKnob = null, dragStartY = 0, dragStartVal = 0;

knobs.forEach(k => {
  const canvas = document.getElementById(k.id);
  if (!canvas) return;

  canvas.addEventListener('mousedown', e => {
    dragKnob = k;
    dragStartY = e.clientY;
    dragStartVal = knobValues[k.id];
    state.knobActive = k.id;
    const bridge = getParamBridge(k.param);
    if (bridge) bridge.sliderDragStarted();
    e.preventDefault();
  });

  canvas.addEventListener('dblclick', () => {
    // Reset to default
    let def = k.min;
    if (k.param === 'retuneSpeed') def = 20;
    else if (k.param === 'amount')  def = 100;
    else if (k.param === 'formant') def = 1;
    knobValues[k.id] = def;
    sendParam(k.param, def);
    updateStateFromParam(k.param, def);
    redrawAllKnobs();
  });
});

document.addEventListener('mousemove', e => {
  if (!dragKnob) return;
  const range = dragKnob.max - dragKnob.min;
  const sensitivity = dragKnob.id === 'retuneKnob' ? 200 : 160;
  const delta = -(e.clientY - dragStartY) / sensitivity * range;
  let newVal = Math.round((dragStartVal + delta) / dragKnob.step) * dragKnob.step;
  newVal = Math.max(dragKnob.min, Math.min(dragKnob.max, newVal));
  knobValues[dragKnob.id] = newVal;
  sendParam(dragKnob.param, newVal);
  updateStateFromParam(dragKnob.param, newVal);
  redrawAllKnobs();
});

document.addEventListener('mouseup', () => {
  if (dragKnob) {
    const bridge = getParamBridge(dragKnob.param);
    if (bridge) bridge.sliderDragEnded();
    dragKnob = null;
    state.knobActive = null;
    redrawAllKnobs();
  }
});

function updateStateFromParam(param, value) {
  if (param === 'retuneSpeed') state.retune    = value;
  if (param === 'tolerance')   state.tolerance = value;
  if (param === 'amount')      state.amount    = value;
  if (param === 'formant')     state.formant   = value;
}

// ── Key / Scale selectors ─────────────────────────────────────────────────────
els.keySelect && els.keySelect.addEventListener('change', () => {
  const idx = els.keySelect.selectedIndex;
  state.key = idx;
  sendParam('key', idx);
  buildPianoKeys();
});

els.scaleButton && els.scaleButton.addEventListener('click', () => {
  state.scale = (state.scale + 1) % scales.length;
  els.scaleButton.textContent = scales[state.scale];
  sendParam('scale', state.scale);
});

// ── Piano keys ────────────────────────────────────────────────────────────────
const BLACK_PCS = new Set([1, 3, 6, 8, 10]);
function buildPianoKeys() {
  if (!els.pianoKeys) return;
  els.pianoKeys.innerHTML = '';
  for (let midi = 71; midi >= 24; --midi) {
    const pc = midi % 12;
    const div = document.createElement('div');
    div.className = 'key-row' + (BLACK_PCS.has(pc) ? ' black' : '');
    div.dataset.midi = midi;
    div.textContent = pc === 0 ? noteNames[pc] + Math.floor(midi / 12 - 1) : '';
    els.pianoKeys.appendChild(div);
  }
}
buildPianoKeys();

// ── Pitch canvas ──────────────────────────────────────────────────────────────
const MAX_PARTICLES = 600;

function drawGraph() {
  const canvas = els.canvas;
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;

  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = 'rgba(14,28,40,0.72)';
  ctx.fillRect(0, 0, W, H);

  // Simulate pitch line
  const t = state.phase;
  const confidence = state.dsp.trackingConfidence;
  const correction = state.dsp.correctionAmount;

  const basePitch = 0.5 + 0.12 * Math.sin(t * 0.7) + 0.04 * Math.sin(t * 2.1);
  const targetPitch = Math.round(basePitch * 12) / 12;
  const displayPitch = basePitch + (targetPitch - basePitch) * (state.amount / 100) * confidence;

  const py = H * (1 - displayPitch);

  // Add particle
  if (Math.random() < 0.7 + correction * 0.3) {
    state.waveParticles.push({
      x: W, y: py + (Math.random() - 0.5) * 6,
      vx: -(3 + Math.random() * 2),
      vy: (Math.random() - 0.5) * 0.4,
      life: 1,
      r: 1.2 + Math.random() * 2.2,
      hue: state.dsp.retuneActive ? 195 : 210,
    });
  }
  if (state.waveParticles.length > MAX_PARTICLES)
    state.waveParticles.splice(0, state.waveParticles.length - MAX_PARTICLES);

  // Update & draw particles
  for (let i = state.waveParticles.length - 1; i >= 0; --i) {
    const p = state.waveParticles[i];
    p.x += p.vx;
    p.y += p.vy;
    p.life -= 0.012;
    if (p.life <= 0 || p.x < 0) { state.waveParticles.splice(i, 1); continue; }
    const alpha = p.life * 0.78;
    ctx.beginPath();
    ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
    ctx.fillStyle = `hsla(${p.hue},90%,74%,${alpha})`;
    ctx.fill();
  }

  // Waveform line
  if (state.waveParticles.length > 2) {
    ctx.beginPath();
    const sorted = [...state.waveParticles].sort((a, b) => a.x - b.x);
    ctx.moveTo(sorted[0].x, sorted[0].y);
    for (let i = 1; i < sorted.length; ++i) {
      ctx.lineTo(sorted[i].x, sorted[i].y);
    }
    ctx.strokeStyle = state.dsp.retuneActive
      ? 'rgba(62,232,247,0.45)'
      : 'rgba(78,166,255,0.35)';
    ctx.lineWidth = 1.5;
    ctx.stroke();
  }

  // Update note pill
  const midiNote = Math.round(basePitch * 24 + 48);
  const noteName = noteNames[midiNote % 12] + Math.floor(midiNote / 12 - 1);
  if (els.currentNote) els.currentNote.textContent = noteName;

  // Highlight piano key
  document.querySelectorAll('.key-row.active').forEach(el => el.classList.remove('active'));
  const activeKey = document.querySelector(`.key-row[data-midi="${midiNote}"]`);
  if (activeKey) activeKey.classList.add('active');
}

// ── Star background ───────────────────────────────────────────────────────────
let stars = [];
function initStars() {
  const canvas = els.bgCanvas;
  if (!canvas) return;
  stars = Array.from({ length: 120 }, () => ({
    x: Math.random(), y: Math.random(),
    size: 0.3 + Math.random() * 1.1,
    opacity: 0.1 + Math.random() * 0.55,
    twinkleSpeed: 0.008 + Math.random() * 0.02,
    twinklePhase: Math.random() * Math.PI * 2,
  }));
}
initStars();

function drawBgFrame() {
  const canvas = els.bgCanvas;
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  canvas.width  = canvas.offsetWidth  || 1060;
  canvas.height = canvas.offsetHeight || 672;
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);
  const t = state.phase;
  stars.forEach(s => {
    const twinkle = 0.5 + 0.5 * Math.sin(t * s.twinkleSpeed * 60 + s.twinklePhase);
    ctx.beginPath();
    ctx.arc(s.x * W, s.y * H, s.size, 0, Math.PI * 2);
    ctx.fillStyle = `rgba(200,228,255,${s.opacity * twinkle})`;
    ctx.fill();
  });
}

// ── Preset panel ──────────────────────────────────────────────────────────────
const PRESETS = [
  { id: 'p1', name: 'Hard Tune',    desc: 'Classic instant snap',      category: 'signature', tags: ['aggressive','robotic'],   params: { retuneSpeed: 0,  tolerance: 0,  amount: 100, formant: 1 } },
  { id: 'p2', name: 'Smooth Ride',  desc: 'Gentle natural correction',  category: 'core',      tags: ['smooth','natural'],       params: { retuneSpeed: 60, tolerance: 5,  amount: 85,  formant: 1 } },
  { id: 'p3', name: 'Pop Vox',      desc: 'Modern pop autotune',        category: 'signature', tags: ['smooth','bright'],        params: { retuneSpeed: 15, tolerance: 2,  amount: 100, formant: 1 } },
  { id: 'p4', name: 'Robot Mode',   desc: 'Full pitch quantize',        category: 'creative',  tags: ['robotic','aggressive'],   params: { retuneSpeed: 0,  tolerance: 0,  amount: 100, formant: 0 } },
  { id: 'p5', name: 'Vibrato Thru', desc: 'Preserves natural vibrato',  category: 'core',      tags: ['natural','smooth'],       params: { retuneSpeed: 80, tolerance: 20, amount: 70,  formant: 1 } },
  { id: 'p6', name: 'Tight Snap',   desc: 'Fast with warmth',           category: 'core',      tags: ['aggressive','bright'],    params: { retuneSpeed: 5,  tolerance: 1,  amount: 100, formant: 1 } },
  { id: 'p7', name: 'Whisper',      desc: 'Subtle background correction', category: 'creative', tags: ['natural','smooth'],      params: { retuneSpeed: 70, tolerance: 30, amount: 50,  formant: 1 } },
  { id: 'p8', name: 'Nova Signature', desc: 'Our go-to vocal preset',   category: 'signature', tags: ['smooth','bright'],        params: { retuneSpeed: 22, tolerance: 3,  amount: 95,  formant: 1 } },
];

function applyPreset(preset) {
  state.preset.activeId = preset.id;
  Object.entries(preset.params).forEach(([param, value]) => {
    // Find knob
    const knob = knobs.find(k => k.param === param);
    if (knob) { knobValues[knob.id] = value; }
    sendParam(param, value);
    updateStateFromParam(param, value);
  });
  redrawAllKnobs();
  renderPresetList();
}

function renderPresetList() {
  if (!els.presetList) return;
  const q   = state.preset.query.toLowerCase();
  const cat = state.preset.category;
  const tag = state.preset.activeTag;

  const filtered = PRESETS.filter(p => {
    if (cat !== 'all' && p.category !== cat) return false;
    if (tag && !p.tags.includes(tag))         return false;
    if (q && !p.name.toLowerCase().includes(q) && !p.desc.toLowerCase().includes(q)) return false;
    return true;
  });

  els.presetList.innerHTML = '';
  if (filtered.length === 0) {
    const em = document.createElement('div');
    em.className = 'preset-empty';
    em.textContent = 'No presets match your search.';
    els.presetList.appendChild(em);
    return;
  }

  filtered.forEach(p => {
    const item = document.createElement('div');
    item.className = 'preset-item' + (p.id === state.preset.activeId ? ' is-active' : '') + (p.category === 'signature' ? ' is-signature' : '');
    item.setAttribute('role', 'option');
    item.innerHTML = `
      <div class="preset-item__lead">${p.category === 'signature' ? '★' : '◯'}</div>
      <div class="preset-item__body">
        <div class="preset-item__name">${p.name}</div>
        <div class="preset-item__desc">${p.desc}</div>
        <div class="preset-item__tags">${p.tags.map(t => `<span class="preset-item__tag" data-tag="${t}">${t}</span>`).join('')}</div>
      </div>
      <div class="preset-item__go">›</div>
    `;
    item.addEventListener('click', () => {
      applyPreset(p);
      item.classList.add('is-pulsing');
      setTimeout(() => item.classList.remove('is-pulsing'), 200);
    });
    els.presetList.appendChild(item);
  });
}

function openPreset() {
  state.preset.open = true;
  els.presetPanel && els.presetPanel.classList.add('is-open');
  els.presetPanel && els.presetPanel.setAttribute('aria-hidden', 'false');
  els.presetToggle && els.presetToggle.classList.add('is-active');
  els.presetToggle && els.presetToggle.setAttribute('aria-expanded', 'true');
  renderPresetList();
}

function closePreset() {
  state.preset.open = false;
  els.presetPanel && els.presetPanel.classList.remove('is-open');
  els.presetPanel && els.presetPanel.setAttribute('aria-hidden', 'true');
  els.presetToggle && els.presetToggle.classList.remove('is-active');
  els.presetToggle && els.presetToggle.setAttribute('aria-expanded', 'false');
}

els.presetToggle && els.presetToggle.addEventListener('click', () => state.preset.open ? closePreset() : openPreset());
els.presetClose  && els.presetClose.addEventListener('click', closePreset);

els.presetSearch && els.presetSearch.addEventListener('input', () => {
  state.preset.query = els.presetSearch.value;
  renderPresetList();
});

els.presetCategory && els.presetCategory.addEventListener('change', () => {
  state.preset.category = els.presetCategory.value;
  renderPresetList();
});

document.querySelectorAll('#presetTagBar .ptag').forEach(btn => {
  btn.addEventListener('click', () => {
    const tag = btn.dataset.tag;
    if (state.preset.activeTag === tag) {
      state.preset.activeTag = null;
      btn.classList.remove('is-active');
    } else {
      document.querySelectorAll('#presetTagBar .ptag').forEach(b => b.classList.remove('is-active'));
      state.preset.activeTag = tag;
      btn.classList.add('is-active');
    }
    renderPresetList();
  });
});

document.querySelectorAll('#presetRail .preset-rail__item').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('#presetRail .preset-rail__item').forEach(b => b.classList.remove('is-active'));
    btn.classList.add('is-active');
    state.preset.category = btn.dataset.category;
    if (els.presetCategory) els.presetCategory.value = state.preset.category;
    renderPresetList();
  });
});

// ── Subscribe to JUCE param changes ──────────────────────────────────────────
function requestInitialHostParameters() {
  if (!hasJuceBackend()) return;

  const paramList = ['key', 'scale', 'retuneSpeed', 'tolerance', 'amount', 'formant'];
  paramList.forEach(param => {
    const identifier = `__juce__slider${param}`;
    window.__JUCE__.backend.addEventListener(identifier, event => {
      const value = event.value;
      const knob = knobs.find(k => k.param === param);
      if (knob) { knobValues[knob.id] = value; }
      updateStateFromParam(param, value);
      if (param === 'key') {
        state.key = value;
        if (els.keySelect) els.keySelect.selectedIndex = Math.round(value);
      }
      if (param === 'scale') {
        state.scale = Math.round(value);
        if (els.scaleButton) els.scaleButton.textContent = scales[state.scale] || 'Chromatic';
      }
      redrawAllKnobs();
    });
  });
}

requestInitialHostParameters();
redrawAllKnobs();

// ── Animate ───────────────────────────────────────────────────────────────────
function animate() {
  state.phase += 0.020;
  if (state.dsp.snapFlash > 0) state.dsp.snapFlash = Math.max(0, state.dsp.snapFlash - 0.04);
  drawBgFrame();
  drawGraph();
  requestAnimationFrame(animate);
}

animate();
