'use strict';

const state = {
  mode: 0,
  clean: 39,
  preserve: 65,
  mix: 100,
  outputGain: 0,
  lowLatency: false,
  listenRemoved: false,
  preview: false,
  advanced: true,
  dsp: { inputL: 0, inputR: 0, outputL: 0, outputR: 0, removedNorm: 0, removedCount: 0 },
  peakHoldL: 0,
  peakHoldR: 0,
  markerPulse: 0,
  markerPulseT: 0,
  cleanAdjustT: 0,
  t: 0,
  // Visual engine additions
  particles: [],        // {x, y, vx, vy, life, maxLife, hue}
  clickEvents: [],      // {x, birth, yAnchor}  — animated click removal
  lastRemovedCount: 0,
  nextClickSpawn: 0,    // time to spawn next demo click
  spectralAmp: [],      // per-column smoothed amplitude (90 cols)
};

const els = {
  app: document.getElementById('app'),
  canvas: document.getElementById('displayCanvas'),
  focusZone: document.getElementById('focusZone'),
  cleanKnob: document.getElementById('cleanKnobWrap'),
  preserveKnob: document.getElementById('preserveKnob'),
  mixKnob: document.getElementById('mixKnob'),
  outputKnob: document.getElementById('outputKnob'),
  modeButtons: [document.getElementById('modeVocal'), document.getElementById('modeDigital'), document.getElementById('modeCrackle')],
  btnLowLatency: document.getElementById('btnLowLatency'),
  btnListenRemoved: document.getElementById('btnListenRemoved'),
  btnAdvanced: document.getElementById('btnAdvanced'),
  advancedPanel: document.getElementById('advancedPanel'),
  cleanValue: document.getElementById('cleanValue'),
  preserveValue: document.getElementById('preserveValue'),
  mixValue: document.getElementById('mixValue'),
  outputValue: document.getElementById('outputValue'),
  meterL: document.getElementById('meterL'),
  meterR: document.getElementById('meterR'),
  meterPeakL: document.getElementById('meterPeakL'),
  meterPeakR: document.getElementById('meterPeakR'),
  inputBar: document.getElementById('inputBar'),
  inputDb: document.getElementById('inputDb'),
  mixSlider: document.getElementById('mixSlider'),
};

function hasBackend() {
  return !!(window.__JUCE__ && window.__JUCE__.backend);
}

function emitParam(name, value) {
  if (!hasBackend()) return;
  window.__JUCE__.backend.emitEvent(`__juce__slider${name}`, { eventType: 'valueChanged', value });
}

function setMode(index) {
  state.mode = index;
  els.modeButtons.forEach((b, i) => b.classList.toggle('active', i === index));
  emitParam('mode', index);
}

function syncUi() {
  els.cleanValue.textContent = `${Math.round(state.clean)}%`;
  els.preserveValue.textContent = `${Math.round(state.preserve)}%`;
  els.mixValue.textContent = `${Math.round(state.mix)}%`;
  els.outputValue.textContent = `${state.outputGain.toFixed(1)} dB`;

  els.btnLowLatency.classList.toggle('active', state.lowLatency);
  els.btnLowLatency.classList.toggle('cyan', state.lowLatency);
  els.btnListenRemoved.classList.toggle('active', state.listenRemoved);
  els.btnListenRemoved.classList.toggle('purple', state.listenRemoved);
  els.btnAdvanced.classList.toggle('active', state.advanced);
  els.btnAdvanced.classList.toggle('purple', state.advanced);

  els.advancedPanel.classList.toggle('collapsed', !state.advanced);

  const outL = Math.max(0, Math.min(1, state.dsp.outputL));
  const outR = Math.max(0, Math.min(1, state.dsp.outputR));
  const inL = Math.max(0, Math.min(1, state.dsp.inputL));

  state.peakHoldL = Math.max(outL, state.peakHoldL - 0.008);
  state.peakHoldR = Math.max(outR, state.peakHoldR - 0.008);

  const meterSegL = els.meterL.querySelectorAll('.meterSeg');
  const meterSegR = els.meterR.querySelectorAll('.meterSeg');
  const activeL = Math.round(outL * meterSegL.length);
  const activeR = Math.round(outR * meterSegR.length);

  meterSegL.forEach((seg, i) => {
    const on = i < activeL;
    seg.classList.toggle('active', on);
    seg.classList.toggle('hot', on && i >= meterSegL.length - 2);
    seg.classList.toggle('cap', on && i === activeL - 1);
  });
  meterSegR.forEach((seg, i) => {
    const on = i < activeR;
    seg.classList.toggle('active', on);
    seg.classList.toggle('hot', on && i >= meterSegR.length - 2);
    seg.classList.toggle('cap', on && i === activeR - 1);
  });

  els.meterPeakL.style.bottom = `${Math.max(4, state.peakHoldL * 100)}%`;
  els.meterPeakR.style.bottom = `${Math.max(4, state.peakHoldR * 100)}%`;
  els.inputBar.style.width = `${Math.max(2, inL * 100)}%`;
  const db = 20 * Math.log10(Math.max(1e-4, inL));
  els.inputDb.textContent = `${db.toFixed(1)} dB`;
}

function pulseCleanKnob() {
  state.cleanAdjustT = 0.55;
  els.cleanKnob.classList.add('is-adjusting');
  setTimeout(() => {
    if (state.cleanAdjustT <= 0) els.cleanKnob.classList.remove('is-adjusting');
  }, 620);
}

function setClean(value) {
  const clamped = Math.max(0, Math.min(100, value));
  if (Math.abs(clamped - state.clean) < 0.001) return;
  state.clean = clamped;
  emitParam('clean', clamped);
  pulseCleanKnob();
  syncUi();
}

function setPreserve(value) {
  const clamped = Math.max(0, Math.min(100, value));
  if (Math.abs(clamped - state.preserve) < 0.001) return;
  state.preserve = clamped;
  emitParam('preserve', clamped);
  syncUi();
}

function setMix(value) {
  const clamped = Math.max(0, Math.min(100, value));
  if (Math.abs(clamped - state.mix) < 0.001) return;
  state.mix = clamped;
  if (els.mixSlider) els.mixSlider.value = `${Math.round(clamped)}`;
  emitParam('mix', clamped);
  syncUi();
}

function setOutputGainDb(value) {
  const clamped = Math.max(-12, Math.min(12, value));
  if (Math.abs(clamped - state.outputGain) < 0.001) return;
  state.outputGain = clamped;
  emitParam('outputGain', clamped);
  syncUi();
}

function installPressFeedback(element) {
  if (!element) return;
  element.addEventListener('pointerdown', () => element.classList.add('is-pressed'));
  element.addEventListener('pointerup', () => element.classList.remove('is-pressed'));
  element.addEventListener('pointerleave', () => element.classList.remove('is-pressed'));
}

function setupInteractions() {
  els.modeButtons.forEach((b, i) => b.addEventListener('click', () => setMode(i)));

  els.btnLowLatency.addEventListener('click', () => {
    state.lowLatency = !state.lowLatency;
    if (state.lowLatency) emitParam('hqMode', 0);
    emitParam('lowLatency', state.lowLatency ? 1 : 0);
    syncUi();
  });

  els.btnListenRemoved.addEventListener('click', () => {
    state.listenRemoved = !state.listenRemoved;
    emitParam('listenRemoved', state.listenRemoved ? 1 : 0);
    syncUi();
  });

  els.btnAdvanced.addEventListener('click', () => {
    state.advanced = !state.advanced;
    emitParam('advanced', state.advanced ? 1 : 0);
    syncUi();
  });

  // ── Process button state machine ──────────────────────
  const processBtn = document.getElementById('processBtn');
  const previewBtn = document.getElementById('previewBtn');

  // Preview toggle — OFF = dry audio, ON = real-time processed
  if (previewBtn) {
    previewBtn.addEventListener('click', () => {
      state.preview = !state.preview;
      previewBtn.classList.toggle('active', state.preview);
      const outputPanel = document.getElementById('outputPanel');
      if (outputPanel) outputPanel.classList.toggle('preview-live', state.preview);
      emitParam('bypass', state.preview ? 0 : 1); // bypass=1 means dry (preview off)
      // Reduce process glow when preview is off (safety hint)
      if (processBtn) processBtn.classList.toggle('preview-off', !state.preview);
    });
  }

  if (processBtn) {
    installPressFeedback(processBtn);
    processBtn.addEventListener('click', () => {
      if (processBtn.classList.contains('processing')) return;
      // → Processing state
      processBtn.classList.add('processing');
      processBtn.classList.remove('preview-off');
      processBtn.textContent = '⚡ PROCESSING…';
      emitParam('process', 1);
      const processDuration = 1800;
      setTimeout(() => {
        processBtn.classList.remove('processing');
        processBtn.classList.add('done');
        processBtn.textContent = '✓ DONE';
        emitParam('process', 0);
        setTimeout(() => {
          processBtn.classList.remove('done');
          processBtn.textContent = '⚡ PROCESS';
          // Restore preview-off dim if preview is still off
          if (!state.preview) processBtn.classList.add('preview-off');
        }, 900);
      }, processDuration);
    });
  }

  els.mixSlider.addEventListener('input', (e) => {
    setMix(Number(e.target.value));
  });

  installPressFeedback(els.cleanKnob);
  installPressFeedback(els.preserveKnob);
  installPressFeedback(els.mixKnob);
  installPressFeedback(els.outputKnob);

  let cleanDragging = false;
  let cleanStartY = 0;
  let cleanStartValue = 0;

  els.cleanKnob.addEventListener('pointerdown', (e) => {
    cleanDragging = true;
    cleanStartY = e.clientY;
    cleanStartValue = state.clean;
    pulseCleanKnob();
    els.cleanKnob.setPointerCapture(e.pointerId);
  });

  els.cleanKnob.addEventListener('pointermove', (e) => {
    if (!cleanDragging) return;
    const delta = (cleanStartY - e.clientY) * 0.35;
    setClean(cleanStartValue + delta);
  });

  const stopCleanDrag = () => {
    cleanDragging = false;
  };

  els.cleanKnob.addEventListener('pointerup', stopCleanDrag);
  els.cleanKnob.addEventListener('pointercancel', stopCleanDrag);

  // Preserve / Mix / Output knob drags
  let mediumDrag = null;

  const beginMediumDrag = (e, startValue, scale, applyFn) => {
    mediumDrag = {
      pointerId: e.pointerId,
      startY: e.clientY,
      startValue,
      scale,
      applyFn,
    };
    e.currentTarget.setPointerCapture(e.pointerId);
  };

  const moveMediumDrag = (e) => {
    if (!mediumDrag || e.pointerId !== mediumDrag.pointerId) return;
    const delta = (mediumDrag.startY - e.clientY) * mediumDrag.scale;
    mediumDrag.applyFn(mediumDrag.startValue + delta);
  };

  const endMediumDrag = (e) => {
    if (!mediumDrag || e.pointerId !== mediumDrag.pointerId) return;
    mediumDrag = null;
  };

  els.preserveKnob.addEventListener('pointerdown', (e) => beginMediumDrag(e, state.preserve, 0.35, setPreserve));
  els.mixKnob.addEventListener('pointerdown', (e) => beginMediumDrag(e, state.mix, 0.35, setMix));
  els.outputKnob.addEventListener('pointerdown', (e) => beginMediumDrag(e, state.outputGain, 0.09, setOutputGainDb));

  window.addEventListener('pointermove', moveMediumDrag);
  window.addEventListener('pointerup', endMediumDrag);
  window.addEventListener('pointercancel', endMediumDrag);

  let draggingFocus = false;
  let offsetX = 0;
  let offsetY = 0;

  els.focusZone.addEventListener('mousedown', (e) => {
    draggingFocus = true;
    const rect = els.focusZone.getBoundingClientRect();
    offsetX = e.clientX - rect.left;
    offsetY = e.clientY - rect.top;
  });

  window.addEventListener('mousemove', (e) => {
    if (!draggingFocus) return;
    const appRect = els.app.getBoundingClientRect();
    const dispRect = els.canvas.getBoundingClientRect();
    const x = e.clientX - appRect.left - offsetX;
    const y = e.clientY - appRect.top - offsetY;

    const minX = dispRect.left - appRect.left;
    const minY = dispRect.top - appRect.top;
    const maxX = minX + dispRect.width - els.focusZone.offsetWidth;
    const maxY = minY + dispRect.height - els.focusZone.offsetHeight;

    els.focusZone.style.left = `${Math.max(minX, Math.min(maxX, x))}px`;
    els.focusZone.style.top = `${Math.max(minY, Math.min(maxY, y))}px`;
  });

  window.addEventListener('mouseup', () => { draggingFocus = false; });
}

// ─── VISUAL ENGINE UPGRADE ────────────────────────────────────────────────────

function spawnParticle(x, y, burst) {
  if (state.particles.length > 80) return;
  const count = burst ? 6 : 1;
  for (let i = 0; i < count; i++) {
    const angle = burst ? (Math.random() * Math.PI * 2) : (Math.PI * 1.5 + (Math.random() - 0.5) * 0.8);
    const speed = burst ? (0.4 + Math.random() * 1.4) : (0.2 + Math.random() * 0.6);
    const hue = [220, 260, 190, 280][Math.floor(Math.random() * 4)]; // blue/purple/cyan
    state.particles.push({
      x: x + (Math.random() - 0.5) * 8,
      y: y + (Math.random() - 0.5) * 8,
      vx: Math.cos(angle) * speed,
      vy: Math.sin(angle) * speed - (burst ? 0.8 : 0.3),
      life: 1.0,
      maxLife: 60 + Math.random() * 80,
      hue,
    });
  }
}

function spawnClickEvent(W, H) {
  const x = 40 + Math.random() * (W - 80);
  state.clickEvents.push({ x, birth: state.t, yAnchor: H * 0.5 });
}

function updateParticles() {
  for (let i = state.particles.length - 1; i >= 0; i--) {
    const p = state.particles[i];
    p.x += p.vx;
    p.y += p.vy;
    p.vy += 0.012; // gravity
    p.vx *= 0.98;
    p.life -= 1 / p.maxLife;
    if (p.life <= 0) state.particles.splice(i, 1);
  }
}

function drawDisplay() {
  const c = els.canvas;
  const ctx = c.getContext('2d');
  const W = c.width;
  const H = c.height;

  state.t += 0.022;
  state.markerPulseT += 0.028;
  state.markerPulse = 0.5 + 0.5 * Math.sin(state.markerPulseT * 3.4);
  state.cleanAdjustT = Math.max(0, state.cleanAdjustT - 0.016);
  if (state.cleanAdjustT <= 0) els.cleanKnob.classList.remove('is-adjusting');

  // ── Spawn demo click events ────────────────────────────────────────────────
  if (state.t > state.nextClickSpawn) {
    spawnClickEvent(W, H);
    const baseInterval = 3.5 - (state.clean / 100) * 2.2;
    state.nextClickSpawn = state.t + baseInterval + Math.random() * 1.5;
  }
  // Also react to DSP-driven events
  if (state.dsp.removedCount !== state.lastRemovedCount && state.dsp.removedCount > 0) {
    spawnClickEvent(W, H);
    state.lastRemovedCount = state.dsp.removedCount;
  }

  updateParticles();

  ctx.clearRect(0, 0, W, H);

  // ── LAYER 1: Background gradient ──────────────────────────────────────────
  const g = ctx.createLinearGradient(0, 0, 0, H);
  g.addColorStop(0, 'rgba(3,6,18,1.0)');
  g.addColorStop(0.45, 'rgba(14,5,34,0.92)');
  g.addColorStop(1, 'rgba(3,6,18,1.0)');
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, W, H);

  // ── LAYER 2: Galaxy texture (subtle star-field) ────────────────────────────
  // Use a seeded approach so positions are stable between frames
  const starSeed = 42;
  for (let i = 0; i < 120; i++) {
    const sx = ((starSeed * (i * 7919 + 1)) % W + W) % W;
    const sy = ((starSeed * (i * 6271 + 3)) % H + H) % H;
    const twinkle = 0.08 + 0.06 * Math.sin(state.t * 1.4 + i * 0.37);
    ctx.fillStyle = `rgba(180,190,255,${twinkle})`;
    ctx.fillRect(sx, sy, 1, 1);
  }

  // ── LAYER 3: Grid lines ───────────────────────────────────────────────────
  for (let i = 0; i < 8; i++) {
    const y = 16 + i * ((H - 32) / 7);
    ctx.strokeStyle = i === 4 ? 'rgba(120,160,255,0.14)' : 'rgba(90,110,160,0.09)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(W, y);
    ctx.stroke();
  }

  // ── LAYER 4: SPECTRAL HEATMAP (THE BIG ONE) ───────────────────────────────
  const SPEC_COLS = 90;
  if (state.spectralAmp.length === 0) {
    for (let i = 0; i < SPEC_COLS; i++) state.spectralAmp.push(Math.random());
  }
  const inputLevel = Math.max(state.dsp.inputL, state.dsp.inputR, 0.08);

  for (let i = 0; i < SPEC_COLS; i++) {
    // Target: noise-based + reactive to input level
    const normalized = i / (SPEC_COLS - 1); // 0=low, 1=high freq
    const target = (
      Math.sin(state.t * 0.6 + i * 0.18) * 0.3 +
      Math.sin(state.t * 1.1 + i * 0.07) * 0.2 +
      0.5
    ) * (0.5 + inputLevel * 0.6);
    state.spectralAmp[i] += (target - state.spectralAmp[i]) * 0.07; // smooth
    const amp = Math.max(0.05, Math.min(1, state.spectralAmp[i]));

    const x = (i / (SPEC_COLS - 1)) * W;
    const colW = W / SPEC_COLS + 1;
    const maxH = H * 0.88;
    const colH = maxH * amp;
    const colY = (H - colH) * 0.5;

    // Color: low freq = deep blue, mid = purple, high = cyan
    let r, g2, b;
    if (normalized < 0.5) {
      const t2 = normalized * 2; // 0→1 across low-mid
      r = Math.round(20 + 140 * t2);
      g2 = Math.round(10 + 20 * t2);
      b = Math.round(180 + 50 * (1 - t2));
    } else {
      const t2 = (normalized - 0.5) * 2; // 0→1 across mid-high
      r = Math.round(160 - 100 * t2);
      g2 = Math.round(30 + 180 * t2);
      b = Math.round(230 - 10 * t2);
    }

    const opacity = 0.22 + amp * 0.18;
    const sg = ctx.createLinearGradient(0, colY, 0, colY + colH);
    sg.addColorStop(0, `rgba(${r},${g2},${b},0.0)`);
    sg.addColorStop(0.35, `rgba(${r},${g2},${b},${opacity})`);
    sg.addColorStop(0.65, `rgba(${r},${g2},${b},${opacity})`);
    sg.addColorStop(1, `rgba(${r},${g2},${b},0.0)`);
    ctx.fillStyle = sg;
    ctx.fillRect(x, colY, colW, colH);
  }

  // ── LAYER 5: Waveform bar underlayer ──────────────────────────────────────
  for (let i = 0; i < 220; i++) {
    const x = (i / 219) * W;
    const w = 1.45;
    const amp = Math.sin(state.t + i * 0.14) * 0.5 + 0.5;
    const h = 20 + amp * 100;
    const y = H * 0.5 - h * 0.5;
    const cg = ctx.createLinearGradient(0, y, 0, y + h);
    cg.addColorStop(0, 'rgba(0,212,255,0.0)');
    cg.addColorStop(0.45, 'rgba(0,212,255,0.15)');
    cg.addColorStop(1, 'rgba(168,85,255,0.0)');
    ctx.fillStyle = cg;
    ctx.fillRect(x, y, w, h);
  }

  // ── LAYER 6: Waveform (enhanced with trailing glow) ───────────────────────
  const flowShift = Math.sin(state.t * 0.75) * 180;
  const wg = ctx.createLinearGradient(-220 + flowShift, H * 0.5, W + 220 + flowShift, H * 0.5);
  wg.addColorStop(0, '#00d4ff');
  wg.addColorStop(0.5, '#a855ff');
  wg.addColorStop(1, '#00d4ff');

  // Trailing glow pass (wider, more diffuse)
  ctx.strokeStyle = 'rgba(116,132,255,0.12)';
  ctx.lineWidth = 5.5;
  ctx.shadowBlur = 0;
  ctx.beginPath();
  for (let i = 0; i < W; i++) {
    const a = Math.sin(state.t * 2.75 + i * 0.036) * 0.85 + Math.sin(state.t * 4.35 + i * 0.012) * 0.4;
    const y = H * 0.5 + a * (5.2 + state.dsp.removedNorm * 16);
    if (i === 0) ctx.moveTo(i, y); else ctx.lineTo(i, y);
  }
  ctx.stroke();

  // Medium glow pass
  ctx.strokeStyle = 'rgba(140,100,255,0.22)';
  ctx.lineWidth = 3.2;
  ctx.beginPath();
  for (let i = 0; i < W; i++) {
    const a = Math.sin(state.t * 2.75 + i * 0.036) * 0.85 + Math.sin(state.t * 4.35 + i * 0.012) * 0.4;
    const y = H * 0.5 + a * (5.2 + state.dsp.removedNorm * 16);
    if (i === 0) ctx.moveTo(i, y); else ctx.lineTo(i, y);
  }
  ctx.stroke();

  // Primary waveform
  ctx.strokeStyle = wg;
  ctx.lineWidth = 1.85;
  ctx.shadowBlur = 7;
  ctx.shadowColor = 'rgba(104,122,255,0.32)';
  ctx.beginPath();
  for (let i = 0; i < W; i++) {
    const a = Math.sin(state.t * 2.75 + i * 0.036) * 0.85 + Math.sin(state.t * 4.35 + i * 0.012) * 0.4;
    const y = H * 0.5 + a * (5.2 + state.dsp.removedNorm * 16);
    if (i === 0) ctx.moveTo(i, y); else ctx.lineTo(i, y);
  }
  ctx.stroke();
  ctx.shadowBlur = 0;

  // ── LAYER 7: ANIMATED CLICK REMOVAL EVENTS ────────────────────────────────
  const DUR = 0.5; // seconds in t-units (t increments ~0.022/frame → ~23 frames/DUR)
  const T_DUR = DUR;
  for (let ci = state.clickEvents.length - 1; ci >= 0; ci--) {
    const ev = state.clickEvents[ci];
    const age = (state.t - ev.birth) / T_DUR; // 0→1
    if (age > 1.0) { state.clickEvents.splice(ci, 1); continue; }

    const x = ev.x;
    const waveY = ev.yAnchor + (Math.sin(state.t * 2.75 + x * 0.036) * 0.85) * (5.2 + state.dsp.removedNorm * 16);

    if (age < 0.25) {
      // Phase 1: Red line appears
      const fadeIn = age / 0.25;
      const lineAlpha = fadeIn * 0.85;
      const lineGrad = ctx.createLinearGradient(x, 20, x, H - 20);
      lineGrad.addColorStop(0, `rgba(255,45,109,0.0)`);
      lineGrad.addColorStop(0.15, `rgba(255,45,109,${lineAlpha})`);
      lineGrad.addColorStop(0.85, `rgba(255,45,109,${lineAlpha * 0.5})`);
      lineGrad.addColorStop(1, `rgba(255,45,109,0.0)`);
      ctx.strokeStyle = lineGrad;
      ctx.lineWidth = 1.4 + fadeIn * 0.6;
      ctx.shadowBlur = 8 * fadeIn;
      ctx.shadowColor = 'rgba(255,45,109,0.7)';
      ctx.beginPath(); ctx.moveTo(x, 22); ctx.lineTo(x, H - 22); ctx.stroke();
      ctx.shadowBlur = 0;

      // Spark at top
      if (fadeIn > 0.3) {
        const sparkAlpha = (fadeIn - 0.3) / 0.7;
        ctx.fillStyle = `rgba(255,220,100,${sparkAlpha * 0.9})`;
        ctx.shadowBlur = 12 * sparkAlpha;
        ctx.shadowColor = 'rgba(255,180,80,0.8)';
        ctx.beginPath();
        ctx.arc(x, waveY - 12, 2.5, 0, Math.PI * 2);
        ctx.fill();
        // Spark rays
        for (let r = 0; r < 6; r++) {
          const ang = (r / 6) * Math.PI * 2 + state.t * 4;
          const len = 5 + sparkAlpha * 6;
          ctx.strokeStyle = `rgba(255,200,80,${sparkAlpha * 0.6})`;
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.moveTo(x + Math.cos(ang) * 3, waveY - 12 + Math.sin(ang) * 3);
          ctx.lineTo(x + Math.cos(ang) * len, waveY - 12 + Math.sin(ang) * len);
          ctx.stroke();
        }
        ctx.shadowBlur = 0;
      }

      // Spectral spike in heatmap (brighter energy at this column)
      const spikeAlpha = fadeIn * 0.85;
      const spikeGrad = ctx.createLinearGradient(x - 4, 0, x + 4, 0);
      spikeGrad.addColorStop(0, 'rgba(255,80,140,0.0)');
      spikeGrad.addColorStop(0.5, `rgba(255,80,140,${spikeAlpha * 0.7})`);
      spikeGrad.addColorStop(1, 'rgba(255,80,140,0.0)');
      ctx.fillStyle = spikeGrad;
      ctx.fillRect(x - 4, 0, 8, H);

    } else if (age < 0.55) {
      // Phase 2: Pulse / glow expand
      const t2 = (age - 0.25) / 0.30;
      const pulseSize = 4 + t2 * 14;
      const pulseAlpha = (1 - t2) * 0.6;
      ctx.strokeStyle = `rgba(255,45,109,${(1 - t2) * 0.7})`;
      ctx.lineWidth = 0.8 + (1 - t2) * 1.2;
      ctx.shadowBlur = pulseSize * 1.4;
      ctx.shadowColor = 'rgba(255,45,109,0.8)';
      ctx.beginPath(); ctx.moveTo(x, 22); ctx.lineTo(x, H - 22); ctx.stroke();
      ctx.shadowBlur = 0;

      // Halo at wave intersection
      ctx.fillStyle = `rgba(255,100,140,${pulseAlpha})`;
      ctx.shadowBlur = pulseSize;
      ctx.shadowColor = 'rgba(255,45,109,0.7)';
      ctx.beginPath();
      ctx.arc(x, waveY, pulseSize, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;

      // Spawn particles at peak pulse
      if (t2 > 0.4 && t2 < 0.6) {
        spawnParticle(x, waveY, true);
      }

    } else {
      // Phase 3: Dissolve into particles → line fades, waveform "heals"
      const t3 = (age - 0.55) / 0.45;
      const fadeOut = 1 - t3;
      ctx.strokeStyle = `rgba(255,45,109,${fadeOut * 0.3})`;
      ctx.lineWidth = 0.8;
      ctx.beginPath(); ctx.moveTo(x, 22); ctx.lineTo(x, H - 22); ctx.stroke();

      // Healing glow on waveform
      const healAlpha = (1 - t3) * 0.45;
      ctx.strokeStyle = `rgba(168,85,255,${healAlpha})`;
      ctx.lineWidth = 3.5 * fadeOut;
      ctx.shadowBlur = 12 * fadeOut;
      ctx.shadowColor = 'rgba(168,85,255,0.6)';
      ctx.beginPath();
      for (let i = Math.max(0, x - 30); i < Math.min(W, x + 30); i++) {
        const a = Math.sin(state.t * 2.75 + i * 0.036) * 0.85 + Math.sin(state.t * 4.35 + i * 0.012) * 0.4;
        const y = H * 0.5 + a * (5.2 + state.dsp.removedNorm * 16);
        if (i === Math.max(0, x - 30)) ctx.moveTo(i, y); else ctx.lineTo(i, y);
      }
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Ambient particles drift
      if (Math.random() < 0.12) spawnParticle(x, waveY, false);
    }
  }

  // ── LAYER 8: Legacy static markers (still shown, but subtler) ────────────
  const markerCount = 4 + Math.floor(state.clean / 14);
  for (let i = 0; i < markerCount; i++) {
    const x = ((i + 1.7) / (markerCount + 2)) * W;
    const pulse = 0.52 + 0.22 * Math.sin(state.t * 3.3 + i + state.markerPulse * 0.7);
    const markerGrad = ctx.createLinearGradient(x, 52, x, H - 32);
    markerGrad.addColorStop(0, `rgba(255,45,109,${0.35 + 0.12 * pulse})`);
    markerGrad.addColorStop(0.3, `rgba(255,45,109,${0.18 + 0.08 * pulse})`);
    markerGrad.addColorStop(1, `rgba(255,45,109,0.04)`);
    ctx.strokeStyle = markerGrad;
    ctx.lineWidth = 1.1;
    ctx.beginPath(); ctx.moveTo(x, 52); ctx.lineTo(x, H - 32); ctx.stroke();
  }

  // ── LAYER 9: FOCUS ZONE (enhanced) ────────────────────────────────────────
  const zoneLeft = parseFloat(els.focusZone.style.left || '596');
  const zoneTop = parseFloat(els.focusZone.style.top || '40');
  const zoneW = els.focusZone.offsetWidth || 280;
  const zoneH = els.focusZone.offsetHeight || 220;

  ctx.save();
  ctx.beginPath();
  ctx.rect(zoneLeft, zoneTop, zoneW, zoneH);
  ctx.clip();

  // Increased spectral brightness inside zone
  const zoneGrad = ctx.createLinearGradient(zoneLeft, zoneTop, zoneLeft, zoneTop + zoneH);
  zoneGrad.addColorStop(0, 'rgba(178,124,255,0.12)');
  zoneGrad.addColorStop(0.5, 'rgba(100,80,255,0.06)');
  zoneGrad.addColorStop(1, 'rgba(0,212,255,0.10)');
  ctx.fillStyle = zoneGrad;
  ctx.fillRect(zoneLeft, zoneTop, zoneW, zoneH);

  // Boosted spectrogram inside zone
  ctx.filter = 'blur(0.6px)';
  for (let i = 0; i < 60; i++) {
    const x = zoneLeft + (i / 59) * zoneW;
    const normalized = i / 59;
    const amp = Math.max(0.1, Math.sin(state.t * 2.2 + i * 0.21) * 0.45 + Math.sin(state.t * 0.8 + i * 0.09) * 0.25 + 0.5);
    const h = 20 + amp * 110;
    const y = zoneTop + zoneH * 0.5 - h * 0.5;
    let r2 = normalized < 0.5 ? Math.round(40 + 160 * normalized * 2) : Math.round(200 - 140 * (normalized - 0.5) * 2);
    let g3 = normalized < 0.5 ? 10 : Math.round(10 + 190 * (normalized - 0.5) * 2);
    let b2 = normalized < 0.5 ? Math.round(220 - 60 * normalized * 2) : 220;
    const g2 = ctx.createLinearGradient(0, y, 0, y + h);
    g2.addColorStop(0, `rgba(${r2},${g3},${b2},0.0)`);
    g2.addColorStop(0.5, `rgba(${r2},${g3},${b2},0.52)`);
    g2.addColorStop(1, `rgba(${r2},${g3},${b2},0.0)`);
    ctx.fillStyle = g2;
    ctx.fillRect(x, y, 1.6, h);
  }
  ctx.filter = 'none';

  // Subtle zoom-feel: slightly taller waveform inside zone
  ctx.lineWidth = 2.15;
  ctx.strokeStyle = 'rgba(195,165,255,0.92)';
  ctx.shadowBlur = 10;
  ctx.shadowColor = 'rgba(168,85,255,0.35)';
  ctx.beginPath();
  for (let i = 0; i < zoneW; i++) {
    const worldX = zoneLeft + i;
    const a = Math.sin(state.t * 2.75 + worldX * 0.036) * 0.85 + Math.sin(state.t * 4.35 + worldX * 0.012) * 0.4;
    const y = zoneTop + zoneH * 0.5 + a * ((5.2 + state.dsp.removedNorm * 16) * 1.18);
    if (i === 0) ctx.moveTo(worldX, y); else ctx.lineTo(worldX, y);
  }
  ctx.stroke();
  ctx.shadowBlur = 0;

  // Animated shimmer sweep
  const shimmerX = zoneLeft + ((Math.sin(state.t * 1.1) * 0.5 + 0.5) * zoneW);
  const shimmer = ctx.createLinearGradient(shimmerX - 55, zoneTop, shimmerX + 55, zoneTop + zoneH);
  shimmer.addColorStop(0, 'rgba(255,255,255,0.0)');
  shimmer.addColorStop(0.5, 'rgba(210,220,255,0.07)');
  shimmer.addColorStop(1, 'rgba(255,255,255,0.0)');
  ctx.fillStyle = shimmer;
  ctx.fillRect(zoneLeft, zoneTop, zoneW, zoneH);
  ctx.restore();

  // ── LAYER 10: PARTICLES ───────────────────────────────────────────────────
  for (const p of state.particles) {
    const fadeIn = Math.min(1, (1 - p.life) * 8); // quick fade in
    const fadeOut = p.life < 0.3 ? p.life / 0.3 : 1.0;
    const alpha = Math.min(fadeIn, fadeOut) * 0.72;
    const radius = 1.2 + (1 - p.life) * 1.4;
    ctx.fillStyle = `hsla(${p.hue},80%,70%,${alpha})`;
    ctx.shadowBlur = 4;
    ctx.shadowColor = `hsla(${p.hue},80%,60%,${alpha * 0.5})`;
    ctx.beginPath();
    ctx.arc(p.x, p.y, radius, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.shadowBlur = 0;

  // ── LAYER 11: VIGNETTE (edges) ────────────────────────────────────────────
  const vig = ctx.createRadialGradient(W * 0.5, H * 0.5, H * 0.22, W * 0.5, H * 0.5, H * 0.85);
  vig.addColorStop(0, 'rgba(0,0,0,0.0)');
  vig.addColorStop(1, 'rgba(0,0,10,0.52)');
  ctx.fillStyle = vig;
  ctx.fillRect(0, 0, W, H);
}

// ─── 4-LAYER KNOB DRAWING ENGINE ──────────────────────────────────────────────
//
// drawKnob(ctx, cx, cy, r, value0to100, isLarge, adjusting, label)
//  Layer 1: Outer halo glow ring
//  Layer 2: Tick marks (major + minor) around arc range
//  Layer 3: Track arc + value arc (dual gradient purple→cyan)
//  Layer 4: Inner body (radial gradient) + glass reflection + notch indicator
//  Center:  value text
//
function drawKnob(ctx, cx, cy, r, value, isLarge, adjusting, label) {
  const v = Math.max(0, Math.min(100, value));
  const rHalo  = r * 0.97;
  const rTick  = r * 0.87;
  const rDot   = r * 0.93;
  const rArc   = r * 0.74;
  const rBody  = r * 0.60;

  const DEG = Math.PI / 180;
  const startA = 210 * DEG;
  const sweep  = 300 * DEG;
  const valA   = startA + (v / 100) * sweep;

  // LAYER 1: outer halo
  const haloAlpha = adjusting ? 0.66 : (isLarge ? 0.46 : 0.34);
  ctx.save();
  ctx.strokeStyle = adjusting
    ? `rgba(168,85,255,${haloAlpha * 0.40})`
    : `rgba(168,85,255,${haloAlpha * 0.30})`;
  ctx.lineWidth = isLarge ? 6.4 : 3.6;
  ctx.shadowBlur = isLarge ? (adjusting ? 44 : 30) : (adjusting ? 22 : 14);
  ctx.shadowColor = `rgba(168,85,255,${haloAlpha})`;
  ctx.beginPath();
  ctx.arc(cx, cy, rHalo, 0, Math.PI * 2);
  ctx.stroke();
  if (isLarge) {
    ctx.strokeStyle = `rgba(0,200,255,${haloAlpha * 0.18})`;
    ctx.lineWidth = 2;
    ctx.shadowBlur = 12;
    ctx.shadowColor = 'rgba(0,200,255,0.32)';
    ctx.beginPath();
    ctx.arc(cx, cy, rHalo - 5, 0, Math.PI * 2);
    ctx.stroke();
  }
  ctx.restore();

  // micro dotted ring
  const DOTS = isLarge ? 56 : 36;
  ctx.save();
  for (let i = 0; i < DOTS; i++) {
    const a = (i / DOTS) * Math.PI * 2;
    const px = cx + Math.cos(a) * rDot;
    const py = cy + Math.sin(a) * rDot;
    const rel = (a - startA + Math.PI * 2) % (Math.PI * 2);
    const active = rel <= ((v / 100) * sweep);
    ctx.fillStyle = active ? 'rgba(185,132,255,0.82)' : 'rgba(108,118,160,0.32)';
    ctx.shadowBlur = active ? 5 : 0;
    ctx.shadowColor = 'rgba(168,85,255,0.65)';
    ctx.beginPath();
    ctx.arc(px, py, isLarge ? 1.32 : 1.06, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();

  // tick ring
  const TICKS = 12;
  ctx.save();
  for (let i = 0; i <= TICKS; i++) {
    const ang = startA + (i / TICKS) * sweep;
    const isMajor = i % 3 === 0;
    const tickLen = isMajor ? (isLarge ? 7 : 4.5) : (isLarge ? 4 : 2.8);
    const outer = rTick;
    const inner = rTick - tickLen;
    const active = i <= (v / 100) * TICKS;
    ctx.strokeStyle = active
      ? `rgba(186,120,255,${isMajor ? 0.88 : 0.55})`
      : `rgba(85,95,140,${isMajor ? 0.44 : 0.24})`;
    ctx.lineWidth = isLarge ? (isMajor ? 2.0 : 1.2) : (isMajor ? 1.4 : 0.9);
    ctx.lineCap = 'round';
    ctx.shadowBlur = active && isMajor ? 5 : 0;
    ctx.shadowColor = 'rgba(168,85,255,0.65)';
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(ang) * inner, cy + Math.sin(ang) * inner);
    ctx.lineTo(cx + Math.cos(ang) * outer, cy + Math.sin(ang) * outer);
    ctx.stroke();
  }
  ctx.restore();

  // arc track + value
  ctx.save();
  ctx.strokeStyle = 'rgba(20,24,44,0.98)';
  ctx.lineWidth = isLarge ? 9.5 : 5.9;
  ctx.lineCap = 'round';
  ctx.beginPath();
  ctx.arc(cx, cy, rArc, startA, startA + sweep);
  ctx.stroke();

  if (v > 0.5) {
    const x0 = cx + Math.cos(startA) * rArc;
    const y0 = cy + Math.sin(startA) * rArc;
    const x1 = cx + Math.cos(valA) * rArc;
    const y1 = cy + Math.sin(valA) * rArc;
    const arcGrad = ctx.createLinearGradient(x0, y0, x1, y1);
    arcGrad.addColorStop(0, '#c07aff');
    arcGrad.addColorStop(0.58, '#80a0ff');
    arcGrad.addColorStop(1, '#30e6ff');
    ctx.strokeStyle = arcGrad;
    ctx.lineWidth = isLarge ? 9.5 : 5.9;
    ctx.shadowBlur = isLarge ? (adjusting ? 22 : 14) : 9;
    ctx.shadowColor = 'rgba(148,92,255,0.80)';
    ctx.beginPath();
    ctx.arc(cx, cy, rArc, startA, valA);
    ctx.stroke();
  }
  ctx.restore();

  // body
  ctx.save();
  ctx.strokeStyle = 'rgba(0,0,0,0.6)';
  ctx.lineWidth = isLarge ? 7 : 4;
  ctx.shadowBlur = isLarge ? 14 : 8;
  ctx.shadowColor = 'rgba(0,0,0,0.8)';
  ctx.beginPath();
  ctx.arc(cx, cy, rBody + (isLarge ? 3 : 2), 0, Math.PI * 2);
  ctx.stroke();
  ctx.shadowBlur = 0;

  const bodyGrad = ctx.createRadialGradient(
    cx - rBody * 0.30, cy - rBody * 0.34, 0,
    cx, cy, rBody
  );
  bodyGrad.addColorStop(0.00, '#364262');
  bodyGrad.addColorStop(0.34, '#1a2238');
  bodyGrad.addColorStop(0.75, '#0a0f1d');
  bodyGrad.addColorStop(1.00, '#060810');
  ctx.fillStyle = bodyGrad;
  ctx.beginPath();
  ctx.arc(cx, cy, rBody, 0, Math.PI * 2);
  ctx.fill();

  const reflGrad = ctx.createRadialGradient(
    cx - rBody * 0.32, cy - rBody * 0.40, 0,
    cx - rBody * 0.08, cy - rBody * 0.14, rBody * 0.68
  );
  reflGrad.addColorStop(0.00, 'rgba(255,255,255,0.24)');
  reflGrad.addColorStop(0.55, 'rgba(255,255,255,0.07)');
  reflGrad.addColorStop(1.00, 'rgba(255,255,255,0.00)');
  ctx.fillStyle = reflGrad;
  ctx.beginPath();
  ctx.arc(cx, cy, rBody, 0, Math.PI * 2);
  ctx.fill();

  ctx.strokeStyle = 'rgba(255,255,255,0.16)';
  ctx.lineWidth = isLarge ? 2.0 : 1.3;
  ctx.beginPath();
  ctx.arc(cx - rBody * 0.05, cy - rBody * 0.08, rBody * 0.72, Math.PI * 1.15, Math.PI * 1.72);
  ctx.stroke();
  ctx.restore();

  // notch
  ctx.save();
  ctx.strokeStyle = 'rgba(250,252,255,0.98)';
  ctx.lineWidth = isLarge ? 2.1 : 1.5;
  ctx.lineCap = 'round';
  ctx.shadowBlur = isLarge ? 7 : 4;
  ctx.shadowColor = 'rgba(255,255,255,0.82)';
  const notchStartX = cx + Math.cos(valA) * rBody * 0.46;
  const notchStartY = cy + Math.sin(valA) * rBody * 0.46;
  const notchEndX = cx + Math.cos(valA) * rBody * 0.92;
  const notchEndY = cy + Math.sin(valA) * rBody * 0.92;
  ctx.beginPath();
  ctx.moveTo(notchStartX, notchStartY);
  ctx.lineTo(notchEndX, notchEndY);
  ctx.stroke();

  // Crisp core line for precision feel
  ctx.strokeStyle = 'rgba(255,255,255,0.92)';
  ctx.lineWidth = isLarge ? 0.95 : 0.7;
  ctx.shadowBlur = 0;
  ctx.beginPath();
  ctx.moveTo(notchStartX, notchStartY);
  ctx.lineTo(notchEndX, notchEndY);
  ctx.stroke();

  // Bright tip dot
  ctx.fillStyle = 'rgba(255,255,255,0.98)';
  ctx.shadowBlur = isLarge ? 8 : 5;
  ctx.shadowColor = 'rgba(210,230,255,0.88)';
  ctx.beginPath();
  ctx.arc(notchEndX, notchEndY, isLarge ? 1.7 : 1.2, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();

  // text
  ctx.save();
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillStyle = 'rgba(252,254,255,1.0)';
  ctx.shadowBlur = 9;
  ctx.shadowColor = 'rgba(168,85,255,0.35)';
  if (isLarge) {
    ctx.font = '800 26px "Arial Narrow", sans-serif';
    ctx.fillText(`${Math.round(v)}%`, cx, cy);
    if (label) {
      ctx.font = '600 10px "Arial Narrow", sans-serif';
      ctx.fillStyle = 'rgba(180,190,220,0.6)';
      ctx.shadowBlur = 0;
      ctx.fillText(label, cx, cy + rBody * 0.45);
    }
  } else {
    ctx.font = '700 14px "Arial Narrow", sans-serif';
    ctx.fillText(`${Math.round(v)}%`, cx, cy);
  }
  ctx.restore();
}

// Draw all knobs in the control panels
function drawKnobs() {
  // ── CLEAN (large, dominant) ─────────────────────────────────────────────
  const cc = els.cleanCanvas;
  if (cc) {
    const cctx = cc.getContext('2d');
    cctx.clearRect(0, 0, cc.width, cc.height);
    const adjusting = state.cleanAdjustT > 0;
    drawKnob(cctx, cc.width/2, cc.height/2, cc.width/2 - 1, state.clean, true, adjusting);

    // Orbit dots and subtle shimmer for CLEAN hero treatment
    const orbR = cc.width/2 - 4;
    for (let d = 0; d < 6; d++) {
      const ang = state.t * 2.2 + (d / 6) * Math.PI * 2;
      const px = cc.width/2 + Math.cos(ang) * orbR;
      const py = cc.height/2 + Math.sin(ang) * orbR;
      const base = 0.10 + 0.12 * (Math.sin(state.t * 3.5 + d) * 0.5 + 0.5);
      const alpha = adjusting ? (base + 0.30 * state.cleanAdjustT) : base;
      cctx.fillStyle = `rgba(168,85,255,${alpha})`;
      cctx.shadowBlur = adjusting ? 7 : 4;
      cctx.shadowColor = 'rgba(168,85,255,0.9)';
      cctx.beginPath();
      cctx.arc(px, py, adjusting ? 2.2 : 1.6, 0, Math.PI * 2);
      cctx.fill();
      cctx.shadowBlur = 0;
    }

    const sh = state.t * 1.1;
    const x1 = cc.width/2 + Math.cos(sh) * 85;
    const y1 = cc.height/2 + Math.sin(sh) * 85;
    const x2 = cc.width/2 + Math.cos(sh + Math.PI) * 85;
    const y2 = cc.height/2 + Math.sin(sh + Math.PI) * 85;
    const shimmer = cctx.createLinearGradient(x1, y1, x2, y2);
    shimmer.addColorStop(0, 'rgba(255,255,255,0.0)');
    shimmer.addColorStop(0.5, adjusting ? 'rgba(210,230,255,0.12)' : 'rgba(210,230,255,0.06)');
    shimmer.addColorStop(1, 'rgba(255,255,255,0.0)');
    cctx.fillStyle = shimmer;
    cctx.beginPath();
    cctx.arc(cc.width/2, cc.height/2, cc.width/2 - 8, 0, Math.PI * 2);
    cctx.fill();
  }

  // ── PRESERVE ──────────────────────────────────────────────────────────────
  const pc = els.preserveCanvas;
  if (pc) {
    const pctx = pc.getContext('2d');
    pctx.clearRect(0, 0, pc.width, pc.height);
    drawKnob(pctx, pc.width/2, pc.height/2, pc.width/2 - 1, state.preserve, false, false);
  }

  // ── MIX ───────────────────────────────────────────────────────────────────
  const mc = els.mixCanvas;
  if (mc) {
    const mctx = mc.getContext('2d');
    mctx.clearRect(0, 0, mc.width, mc.height);
    drawKnob(mctx, mc.width/2, mc.height/2, mc.width/2 - 1, state.mix, false, false);
  }

  // ── OUTPUT GAIN (maps -12..+12 dB → 0..100%) ─────────────────────────────
  const oc = els.outputCanvas;
  if (oc) {
    const octx = oc.getContext('2d');
    octx.clearRect(0, 0, oc.width, oc.height);
    const normGain = Math.round(((state.outputGain + 12) / 24) * 100);
    drawKnob(octx, oc.width/2, oc.height/2, oc.width/2 - 1, normGain, false, false);

    // Slight extra glow so output knob matches overall lighting system.
    octx.save();
    octx.strokeStyle = 'rgba(168,85,255,0.28)';
    octx.lineWidth = 2.4;
    octx.shadowBlur = 10;
    octx.shadowColor = 'rgba(168,85,255,0.50)';
    octx.beginPath();
    octx.arc(oc.width/2, oc.height/2, oc.width/2 - 3, 0, Math.PI * 2);
    octx.stroke();
    octx.restore();

    // Override center text to show dB
    octx.save();
    octx.textAlign = 'center';
    octx.textBaseline = 'middle';
    octx.fillStyle = 'rgba(245,248,255,0.96)';
    octx.font = '700 11px "Arial Narrow", sans-serif';
    octx.shadowBlur = 4;
    octx.shadowColor = 'rgba(168,85,255,0.3)';
    octx.fillText(`${state.outputGain.toFixed(1)}`, oc.width/2, oc.height/2 - 3);
    octx.font = '600 9px "Arial Narrow", sans-serif';
    octx.fillStyle = 'rgba(160,170,200,0.7)';
    octx.shadowBlur = 0;
    octx.fillText('dB', oc.width/2, oc.height/2 + 9);
    octx.restore();
  }

  // ── ADVANCED PANEL hKnobs (drawn at 50% demo value) ──────────────────────
  if (els.hKnobCanvases) {
    els.hKnobCanvases.forEach(c => {
      const ctx = c.getContext('2d');
      ctx.clearRect(0, 0, c.width, c.height);
      drawKnob(ctx, c.width/2, c.height/2, c.width/2 - 1, 50, false, false);
    });
  }
}

function tick() {
  drawDisplay();
  drawKnobs();
  requestAnimationFrame(tick);
}

function resizeApp() {
  const pad = 12;
  const vw = Math.max(1, window.innerWidth - pad * 2);
  const vh = Math.max(1, window.innerHeight - pad * 2);
  const s = Math.min(vw / 1536, vh / 1024, 1);
  const offsetX = Math.floor((window.innerWidth - 1536 * s) * 0.5);
  const offsetY = Math.floor((window.innerHeight - 1024 * s) * 0.5);
  els.app.style.left = `${offsetX}px`;
  els.app.style.top = `${offsetY}px`;
  els.app.style.transform = `scale(${s})`;
}

window.receiveDSP = (dsp) => {
  state.dsp = { ...state.dsp, ...dsp };
  syncUi();
};

window.addEventListener('resize', resizeApp);
resizeApp();
setMode(0);
syncUi();
setupInteractions();

// ── Canvas knob refs ─────────────────────────────────────────────────────────
els.cleanCanvas   = document.getElementById('cleanKnobCanvas');
els.preserveCanvas = document.getElementById('preserveKnobCanvas');
els.mixCanvas     = document.getElementById('mixKnobCanvas');
els.outputCanvas  = document.getElementById('outputKnobCanvas');

// Inject canvases into .hKnob elements (advanced panel mini knobs)
els.hKnobCanvases = [];
document.querySelectorAll('.hKnob').forEach(knob => {
  knob.style.position = 'relative';
  knob.style.overflow = 'hidden';
  const c = document.createElement('canvas');
  c.width  = 72;
  c.height = 72;
  c.className = 'knobCanvas';
  knob.appendChild(c);
  els.hKnobCanvases.push(c);
});

tick();
