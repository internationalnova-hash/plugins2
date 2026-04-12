// Nova Pitch UI Engine
'use strict';

// ── State Management ──────────────────────────────────────────
const state = {
  detectedPitch: 0,
  targetPitch: 0,
  spectralData: new Array(256).fill(0),
  pitchHistory: new Array(512).fill(0),
  pitchHistoryIndex: 0,
  activeKey: -1,
  keyStates: new Array(12).fill(false),
};

// ── DOM References ────────────────────────────────────────────
const elements = {
  spectralCanvas: document.getElementById('spectralCanvas'),
  pitchCurveCanvas: document.getElementById('pitchCurveCanvas'),
  keyboardContainer: document.getElementById('keyboardContainer'),
  detectedMeter: document.getElementById('detectedMeter'),
  targetMeter: document.getElementById('targetMeter'),
  keySelect: document.getElementById('keySelect'),
  scaleSelect: document.getElementById('scaleSelect'),
  amountSlider: document.getElementById('amountSlider'),
  toleranceSlider: document.getElementById('toleranceSlider'),
  confidenceSlider: document.getElementById('confidenceSlider'),
  amountValue: document.getElementById('amountValue'),
  toleranceValue: document.getElementById('toleranceValue'),
  confidenceValue: document.getElementById('confidenceValue'),
};

// ── Canvas Contexts ──────────────────────────────────────────
let spectralCtx = null;
let pitchCtx = null;

// ── Initialize ────────────────────────────────────────────────
function init() {
  // Canvas setup
  setupCanvases();
  setupKeyboard();
  setupControls();
  
  // Start animation loop
  requestAnimationFrame(update);
  
  // Update loop for meters
  setInterval(updateMeters, 50);
  
  console.log('Nova Pitch UI initialized');
}

// ── Setup Canvas ──────────────────────────────────────────────
function setupCanvases() {
  spectralCtx = elements.spectralCanvas.getContext('2d');
  pitchCtx = elements.pitchCurveCanvas.getContext('2d');
  
  // Set DPI for crisp rendering
  const dpr = window.devicePixelRatio || 1;
  
  // Spectral (larger)
  const spectralRect = elements.spectralCanvas.parentElement.getBoundingClientRect();
  elements.spectralCanvas.width = spectralRect.width * dpr;
  elements.spectralCanvas.height = spectralRect.height * dpr;
  spectralCtx.scale(dpr, dpr);
  
  // Pitch curve (smaller)
  const pitchRect = elements.pitchCurveCanvas.parentElement.getBoundingClientRect();
  elements.pitchCurveCanvas.width = pitchRect.width * dpr;
  elements.pitchCurveCanvas.height = pitchRect.height * dpr;
  pitchCtx.scale(dpr, dpr);
  
  // Generate initial spectral data
  generateSpectralData();
}

// ── Setup Keyboard ───────────────────────────────────────────
function setupKeyboard() {
  const noteNames = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
  
  for (let i = 0; i < 12; i++) {
    const key = document.createElement('div');
    key.className = 'piano-key';
    key.setAttribute('data-key', i);
    key.innerHTML = `<span class="piano-key-label">${noteNames[i]}</span>`;
    key.addEventListener('click', () => activateKey(i, 200));
    elements.keyboardContainer.appendChild(key);
  }
}

// ── Setup Controls ────────────────────────────────────────────
function setupControls() {
  elements.amountSlider.addEventListener('input', (e) => {
    const val = e.target.value;
    elements.amountValue.textContent = val;
    syncParameterToNative('amount', val / 100);
  });
  
  elements.toleranceSlider.addEventListener('input', (e) => {
    const val = e.target.value;
    elements.toleranceValue.textContent = val;
    syncParameterToNative('tolerance', val / 100);
  });
  
  elements.confidenceSlider.addEventListener('input', (e) => {
    const val = e.target.value;
    elements.confidenceValue.textContent = val;
    syncParameterToNative('confidenceThreshold', val / 100);
  });
  
  elements.keySelect.addEventListener('change', (e) => {
    syncParameterToNative('key', e.target.value / 11);
  });
  
  elements.scaleSelect.addEventListener('change', (e) => {
    syncParameterToNative('scale', e.target.value / 4);
  });
}

// ── Generate Spectral Data ────────────────────────────────────
function generateSpectralData() {
  // Create smooth, realistic spectral data
  for (let i = 0; i < state.spectralData.length; i++) {
    const freq = i / state.spectralData.length;
    
    // Fundamental + harmonics
    let value = Math.sin(freq * Math.PI * 2) * 0.5;
    value += Math.sin(freq * Math.PI * 4) * 0.25 * (1 - freq);
    value += Math.sin(freq * Math.PI * 6) * 0.125 * (1 - freq * 0.8);
    value += Math.random() * 0.05;
    
    state.spectralData[i] = Math.abs(value);
  }
}

// ── Activate Keyboard Key ────────────────────────────────────
function activateKey(noteIndex, duration = 100) {
  state.keyStates[noteIndex] = true;
  state.activeKey = noteIndex;
  
  // Update visual
  const keyEl = document.querySelector(`[data-key="${noteIndex}"]`);
  if (keyEl) keyEl.classList.add('active');
  
  // Auto deactivate
  setTimeout(() => {
    state.keyStates[noteIndex] = false;
    if (keyEl) keyEl.classList.remove('active');
  }, duration);
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
  spectralCtx.strokeStyle = 'rgba(0, 212, 255, 0.05)';
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
  gradient.addColorStop(0, 'rgba(0, 212, 255, 0.8)');
  gradient.addColorStop(0.5, 'rgba(212, 217, 224, 0.6)');
  gradient.addColorStop(1, 'rgba(61, 142, 255, 0.4)');
  
  spectralCtx.fillStyle = gradient;
  
  for (let i = 0; i < state.spectralData.length; i++) {
    const val = state.spectralData[i];
    const barHeight = val * h * 0.9;
    const x = i * barWidth;
    const y = h - barHeight;
    
    spectralCtx.fillRect(x, y, barWidth - 1, barHeight);
  }
  
  // Fundamental line
  spectralCtx.strokeStyle = 'rgba(0, 255, 163, 0.6)';
  spectralCtx.lineWidth = 2;
  spectralCtx.beginPath();
  const fundamentalX = Math.min(state.detectedPitch / 400 * w, w - 1);
  spectralCtx.moveTo(fundamentalX, h);
  spectralCtx.lineTo(fundamentalX, 0);
  spectralCtx.stroke();
}

// ── Draw Pitch Curve ──────────────────────────────────────────
function drawPitchCurve() {
  if (!pitchCtx) return;
  
  const w = elements.pitchCurveCanvas.width;
  const h = elements.pitchCurveCanvas.height;
  
  // Background
  pitchCtx.fillStyle = 'rgba(0, 0, 0, 0.3)';
  pitchCtx.fillRect(0, 0, w, h);
  
  // Grid lines (note boundaries)
  pitchCtx.strokeStyle = 'rgba(0, 212, 255, 0.1)';
  pitchCtx.lineWidth = 1;
  for (let i = 0; i <= 11; i++) {
    const x = (w / 12) * i;
    pitchCtx.beginPath();
    pitchCtx.moveTo(x, 0);
    pitchCtx.lineTo(x, h);
    pitchCtx.stroke();
  }
  
  // Draw pitch curve
  const padding = 10;
  pitchCtx.beginPath();
  pitchCtx.strokeStyle = 'rgba(0, 212, 255, 0.7)';
  pitchCtx.lineWidth = 2;
  
  for (let i = 0; i < state.pitchHistory.length; i++) {
    const pitch = state.pitchHistory[i];
    const x = padding + ((w - 2 * padding) * i) / state.pitchHistory.length;
    const y = h - padding - ((h - 2 * padding) * Math.min(pitch / 400, 1));
    
    if (i === 0) {
      pitchCtx.moveTo(x, y);
    } else if (pitch > 10) {
      pitchCtx.lineTo(x, y);
    }
  }
  pitchCtx.stroke();
  
  // Target line
  pitchCtx.strokeStyle = 'rgba(0, 255, 163, 0.5)';
  pitchCtx.lineWidth = 1.5;
  pitchCtx.setLineDash([4, 4]);
  pitchCtx.beginPath();
  const targetY = h - padding - ((h - 2 * padding) * Math.min(state.targetPitch / 400, 1));
  pitchCtx.moveTo(padding, targetY);
  pitchCtx.lineTo(w - padding, targetY);
  pitchCtx.stroke();
  pitchCtx.setLineDash([]);
}

// ── Update Meters ────────────────────────────────────────────
function updateMeters() {
  elements.detectedMeter.textContent = state.detectedPitch > 0 ? 
    state.detectedPitch.toFixed(1) + ' Hz' : '--';
  elements.targetMeter.textContent = state.targetPitch > 0 ? 
    state.targetPitch.toFixed(1) + ' Hz' : '--';
}

// ── Animation Loop ────────────────────────────────────────────
function update() {
  // Update spectral data with animation
  generateSpectralData();
  
  // Add some detected pitch influence
  if (state.detectedPitch > 0) {
    const pitchIndex = Math.floor((state.detectedPitch / 400) * state.spectralData.length);
    if (pitchIndex >= 0 && pitchIndex < state.spectralData.length) {
      state.spectralData[pitchIndex] *= 1.5;
    }
  }
  
  // Store pitch in history
  state.pitchHistory[state.pitchHistoryIndex] = state.detectedPitch;
  state.pitchHistoryIndex = (state.pitchHistoryIndex + 1) % state.pitchHistory.length;
  
  // Draw
  drawSpectral();
  drawPitchCurve();
  
  // Query native data if available
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
        
        // Flash keyboard key for active pitch
        const semitone = Math.round(12 * Math.log2(detected / 16.35)) % 12;
        if (semitone >= 0 && semitone < 12) {
          activateKey(semitone, 50);
        }
      }
      
      if (corrected !== null && corrected !== undefined) {
        state.targetPitch = parseFloat(corrected) || 0;
      }
    } catch (e) {
      // Silently fail if native bridge not available
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

// ── Fallback Demo Mode ────────────────────────────────────────
function startDemoMode() {
  console.log('Starting demo mode...');
  let pitchPhase = 0;
  
  setInterval(() => {
    // Simulate pitch oscillation
    state.detectedPitch = 100 + 80 * Math.sin(pitchPhase);
    state.targetPitch = 110;
    pitchPhase += 0.02;
    
    // Simulate keyboard activity
    if (Math.random() < 0.1) {
      activateKey(Math.floor(Math.random() * 12), 100);
    }
  }, 100);
}

// ── Auto-start ────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', () => {
  init();
  
  // Start demo if no native bridge
  if (!window.juceNativeMode) {
    setTimeout(startDemoMode, 500);
  }
});

// Graceful resize
window.addEventListener('resize', () => {
  setupCanvases();
});

