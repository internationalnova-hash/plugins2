// Nova Pitch UI Engine - Knob Interactive System
'use strict';

// ── Knob Parameters ──────────────────────────────────────────
const knobParams = [
  { id: 'key', label: 'KEY', min: 0, max: 11, value: 0, notes: ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'] },
  { id: 'correction', label: 'CORRECTION', min: 0, max: 100, value: 85, format: (v) => v.toFixed(0) + '%' },
  { id: 'tolerance', label: 'TOLERANCE', min: 0, max: 100, value: 50, format: (v) => v.toFixed(0) + '%' },
  { id: 'confidence', label: 'CONFIDENCE', min: 0, max: 100, value: 70, format: (v) => v.toFixed(0) + '%' },
];

// ── State Management ──────────────────────────────────────────
const state = {
  detectedPitch: 0,
  targetPitch: 0,
  spectralData: new Array(256).fill(0),
  activeKey: -1,
  knobValues: {},
  knobCanvases: {},
};

// Initialize knob values
knobParams.forEach(p => {
  state.knobValues[p.id] = p.value;
});

// ── DOM References ────────────────────────────────────────────
const elements = {
  spectralCanvas: document.getElementById('spectralCanvas'),
  knobGrid: document.getElementById('knobGrid'),
  keyboardDisplay: document.getElementById('keyboardDisplay'),
  detectedInfo: document.getElementById('detectedInfo'),
  targetInfo: document.getElementById('targetInfo'),
  scaleSelect: document.getElementById('scaleSelect'),
};

let spectralCtx = null;

// ── Initialize ────────────────────────────────────────────────
function init() {
  setupCanvases();
  createKnobs();
  createKeyboard();
  setupControls();
  
  requestAnimationFrame(update);
  setInterval(updateMeters, 50);
  
  console.log('Nova Pitch UI initialized');
}

// ── Setup Canvas ──────────────────────────────────────────────
function setupCanvases() {
  spectralCtx = elements.spectralCanvas.getContext('2d');
  
  const dpr = window.devicePixelRatio || 1;
  const rect = elements.spectralCanvas.parentElement.getBoundingClientRect();
  elements.spectralCanvas.width = rect.width * dpr;
  elements.spectralCanvas.height = rect.height * dpr;
  spectralCtx.scale(dpr, dpr);
  
  generateSpectralData();
}

// ── Create Interactive Knobs ─────────────────────────────────
function createKnobs() {
  knobParams.forEach((param, idx) => {
    const group = document.createElement('div');
    group.className = 'knob-group';
    
    const label = document.createElement('div');
    label.className = 'knob-label';
    label.textContent = param.label;
    
    const container = document.createElement('div');
    container.className = 'knob-container';
    container.id = `knob-${param.id}`;
    
    const canvas = document.createElement('canvas');
    canvas.width = 80;
    canvas.height = 80;
    canvas.style.cursor = 'pointer';
    container.appendChild(canvas);
    
    const value = document.createElement('div');
    value.className = 'knob-value';
    value.id = `value-${param.id}`;
    updateKnobLabel(param);
    
    group.appendChild(label);
    group.appendChild(container);
    group.appendChild(value);
    elements.knobGrid.appendChild(group);
    
    // Store canvas and setup interaction
    state.knobCanvases[param.id] = canvas;
    setupKnobInteraction(param, canvas);
    drawKnob(canvas, state.knobValues[param.id], param);
  });
}

// ── Setup Knob Interaction ───────────────────────────────────
function setupKnobInteraction(param, canvas) {
  let isDragging = false;
  let startY = 0;
  let startValue = state.knobValues[param.id];
  
  const onMouseDown = (e) => {
    isDragging = true;
    startY = e.clientY || e.touches?.[0].clientY;
    startValue = state.knobValues[param.id];
  };

  const onMouseMove = (e) => {
    if (!isDragging) return;
    
    const currentY = e.clientY || e.touches?.[0].clientY;
    const delta = startY - currentY;
    const sensitivity = (param.max - param.min) / 200;
    const newValue = Math.max(param.min, Math.min(param.max, startValue + delta * sensitivity));
    
    state.knobValues[param.id] = newValue;
    syncParameterToNative(param.id, normalizeValue(newValue, param));
    drawKnob(canvas, newValue, param);
    updateKnobLabel(param);
  };

  const onMouseUp = () => {
    isDragging = false;
  };

  canvas.addEventListener('mousedown', onMouseDown);
  canvas.addEventListener('touchstart', onMouseDown);
  document.addEventListener('mousemove', onMouseMove);
  document.addEventListener('touchmove', onMouseMove);
  document.addEventListener('mouseup', onMouseUp);
  document.addEventListener('touchend', onMouseUp);
}

// ── Draw Knob ────────────────────────────────────────────────
function drawKnob(canvas, value, param) {
  const ctx = canvas.getContext('2d');
  const centerX = canvas.width / 2;
  const centerY = canvas.height / 2;
  const radius = 32;
  
  // Background
  ctx.fillStyle = 'rgba(139, 111, 255, 0.08)';
  ctx.beginPath();
  ctx.arc(centerX, centerY, radius, 0, Math.PI * 2);
  ctx.fill();
  
  // Border
  ctx.strokeStyle = 'rgba(139, 111, 255, 0.25)';
  ctx.lineWidth = 1;
  ctx.stroke();
  
  // Indicator ring
  ctx.strokeStyle = 'rgba(139, 111, 255, 0.1)';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(centerX, centerY, radius - 4, 0, Math.PI * 2);
  ctx.stroke();
  
  // Normalize value to 0-1
  const normalized = (value - param.min) / (param.max - param.min);
  
  // Active arc (gradient)
  const gradient = ctx.createLinearGradient(0, 0, canvas.width, canvas.height);
  gradient.addColorStop(0, 'rgba(139, 111, 255, 0.8)');
  gradient.addColorStop(1, 'rgba(0, 232, 255, 0.8)');
  
  ctx.strokeStyle = gradient;
  ctx.lineWidth = 3;
  ctx.lineCap = 'round';
  ctx.beginPath();
  const startAngle = -Math.PI * 0.75;
  const endAngle = startAngle + (Math.PI * 1.5) * normalized;
  ctx.arc(centerX, centerY, radius - 6, startAngle, endAngle);
  ctx.stroke();
  
  // Center dot with glow
  ctx.shadowColor = 'rgba(0, 232, 255, 0.4)';
  ctx.shadowBlur = 12;
  ctx.fillStyle = 'rgba(0, 232, 255, 0.9)';
  ctx.beginPath();
  ctx.arc(centerX, centerY, 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.shadowBlur = 0;
}

// ── Update Knob Label ────────────────────────────────────────
function updateKnobLabel(param) {
  const valueEl = document.getElementById(`value-${param.id}`);
  const val = state.knobValues[param.id];
  
  let display = '';
  if (param.notes) {
    display = param.notes[Math.round(val)];
  } else if (param.format) {
    display = param.format(val);
  } else {
    display = val.toFixed(0);
  }
  
  valueEl.textContent = display;
}

// ── Normalize Value for Native ──────────────────────────────
function normalizeValue(value, param) {
  return (value - param.min) / (param.max - param.min);
}

// ── Create Keyboard ──────────────────────────────────────────
function createKeyboard() {
  const noteNames = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
  
  // First row (6 keys)
  const row1 = document.createElement('div');
  row1.className = 'key-row';
  for (let i = 0; i < 6; i++) {
    const key = document.createElement('div');
    key.className = 'piano-key';
    key.setAttribute('data-key', i);
    key.textContent = noteNames[i];
    key.addEventListener('click', () => activateKey(i, 150));
    row1.appendChild(key);
  }
  elements.keyboardDisplay.appendChild(row1);
  
  // Second row (6 keys)
  const row2 = document.createElement('div');
  row2.className = 'key-row';
  for (let i = 6; i < 12; i++) {
    const key = document.createElement('div');
    key.className = 'piano-key';
    key.setAttribute('data-key', i);
    key.textContent = noteNames[i];
    key.addEventListener('click', () => activateKey(i, 150));
    row2.appendChild(key);
  }
  elements.keyboardDisplay.appendChild(row2);
}

// ── Activate Keyboard Key ────────────────────────────────────
function activateKey(noteIndex) {
  const keyEl = document.querySelector(`[data-key="${noteIndex}"]`);
  if (keyEl) {
    keyEl.classList.add('active');
    setTimeout(() => keyEl.classList.remove('active'), 150);
  }
}

// ── Setup Controls ──────────────────────────────────────────
function setupControls() {
  elements.scaleSelect.addEventListener('change', (e) => {
    syncParameterToNative('scale', e.target.value / 4);
  });
}

// ── Generate Spectral Data ──────────────────────────────────
function generateSpectralData() {
  for (let i = 0; i < state.spectralData.length; i++) {
    const freq = i / state.spectralData.length;
    
    let value = Math.sin(freq * Math.PI * 2) * 0.5;
    value += Math.sin(freq * Math.PI * 4) * 0.25 * (1 - freq);
    value += Math.sin(freq * Math.PI * 6) * 0.125 * (1 - freq * 0.8);
    value += Math.random() * 0.05;
    
    state.spectralData[i] = Math.abs(value);
  }
}

// ── Draw Spectral Display ────────────────────────────────────
function drawSpectral() {
  if (!spectralCtx) return;
  
  const w = elements.spectralCanvas.width;
  const h = elements.spectralCanvas.height;
  
  // Background
  spectralCtx.fillStyle = 'rgba(0, 0, 0, 0.2)';
  spectralCtx.fillRect(0, 0, w, h);
  
  // Grid
  spectralCtx.strokeStyle = 'rgba(139, 111, 255, 0.05)';
  spectralCtx.lineWidth = 1;
  for (let i = 0; i <= 10; i++) {
    const y = (h / 10) * i;
    spectralCtx.beginPath();
    spectralCtx.moveTo(0, y);
    spectralCtx.lineTo(w, y);
    spectralCtx.stroke();
  }
  
  // Draw spectrum bars
  const barWidth = w / state.spectralData.length;
  const gradient = spectralCtx.createLinearGradient(0, 0, 0, h);
  gradient.addColorStop(0, 'rgba(139, 111, 255, 0.8)');
  gradient.addColorStop(0.5, 'rgba(0, 232, 255, 0.6)');
  gradient.addColorStop(1, 'rgba(139, 111, 255, 0.4)');
  
  spectralCtx.fillStyle = gradient;
  
  for (let i = 0; i < state.spectralData.length; i++) {
    const val = state.spectralData[i];
    const barHeight = val * h * 0.9;
    const x = i * barWidth;
    const y = h - barHeight;
    
    spectralCtx.fillRect(x, y, barWidth - 1, barHeight);
  }
  
  // Fundamental line
  if (state.detectedPitch > 0) {
    spectralCtx.strokeStyle = 'rgba(0, 255, 163, 0.6)';
    spectralCtx.lineWidth = 2;
    spectralCtx.beginPath();
    const fundamentalX = Math.min((state.detectedPitch / 400) * w, w - 1);
    spectralCtx.moveTo(fundamentalX, h);
    spectralCtx.lineTo(fundamentalX, 0);
    spectralCtx.stroke();
  }
}

// ── Update Meters ────────────────────────────────────────────
function updateMeters() {
  elements.detectedInfo.textContent = state.detectedPitch > 0 ? 
    state.detectedPitch.toFixed(1) + ' Hz' : '-- Hz';
  elements.targetInfo.textContent = state.targetPitch > 0 ? 
    state.targetPitch.toFixed(1) + ' Hz' : '-- Hz';
}

// ── Animation Loop ────────────────────────────────────────────
function update() {
  generateSpectralData();
  
  if (state.detectedPitch > 0) {
    const pitchIndex = Math.floor((state.detectedPitch / 400) * state.spectralData.length);
    if (pitchIndex >= 0 && pitchIndex < state.spectralData.length) {
      state.spectralData[pitchIndex] *= 1.5;
    }
  }
  
  drawSpectral();
  queryNativeData();
  
  requestAnimationFrame(update);
}

// ── Query Native Data ────────────────────────────────────────
function queryNativeData() {
  if (window.NativeBridge && window.NativeBridge.callNativeFunction) {
    try {
      const detected = window.NativeBridge.callNativeFunction('getParameter', { parameter: 'detectedPitch' });
      const corrected = window.NativeBridge.callNativeFunction('getParameter', { parameter: 'correctedPitch' });
      
      if (detected !== null && detected !== undefined) {
        state.detectedPitch = parseFloat(detected) || 0;
        const semitone = Math.round(12 * Math.log2(detected / 16.35)) % 12;
        if (semitone >= 0 && semitone < 12) {
          activateKey(semitone);
        }
      }
      
      if (corrected !== null && corrected !== undefined) {
        state.targetPitch = parseFloat(corrected) || 0;
      }
    } catch (e) {
      // Silently fail
    }
  }
}

// ── Sync Parameter to Native ──────────────────────────────────
function syncParameterToNative(paramName, value) {
  if (window.NativeBridge && window.NativeBridge.callNativeFunction) {
    try {
      window.NativeBridge.callNativeFunction('setParameter', {
        parameter: paramName,
        value: value
      });
    } catch (e) {
      console.log('Could not sync parameter:', paramName);
    }
  }
}

// ── Start Demo Mode ──────────────────────────────────────────
function startDemoMode() {
  console.log('Starting demo mode...');
  let pitchPhase = 0;
  
  setInterval(() => {
    state.detectedPitch = 100 + 80 * Math.sin(pitchPhase);
    state.targetPitch = 110;
    pitchPhase += 0.02;
    
    if (Math.random() < 0.08) {
      activateKey(Math.floor(Math.random() * 12));
    }
  }, 100);
}

// ── Auto-start ────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', () => {
  init();
  
  if (!window.juceNativeMode) {
    setTimeout(startDemoMode, 500);
  }
});

window.addEventListener('resize', setupCanvases);

