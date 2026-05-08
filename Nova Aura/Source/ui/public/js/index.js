(function () {
  'use strict';

  var PARAMS = {
    mid_aura: { min: 0, max: 100, def: 42 },
    high_aura: { min: 0, max: 100, def: 56 },
    mix: { min: 0, max: 100, def: 100 },
    safe: { min: 0, max: 1, def: 1 },
    wide: { min: 0, max: 1, def: 0 },
    low_latency: { min: 0, max: 1, def: 0 }
  };

  var values = { mid_aura: 42, high_aura: 56, mix: 100, safe: 1, wide: 0, low_latency: 0 };
  var states = {};
  var juceAvailable = false;
  var dragging = null;

  var telemetry = { inL: 0, inR: 0, outL: 0, outR: 0, aura: 0, harsh: 0, midAura: 42, highAura: 56, safe: 1, wide: 0, lowLatency: 0 };

  // A/B System
  var abSnapshots = {
    A: { mid_aura: 42, high_aura: 56, mix: 100, safe: 1, wide: 0, low_latency: 0 },
    B: { mid_aura: 42, high_aura: 56, mix: 100, safe: 1, wide: 0, low_latency: 0 }
  };
  var abActive = 'A';
  var abTransitioning = false;
  var abTransitionProgress = 0;
  var abTransitionStart = null;
  var ABTransitionDuration = 120; // ms for smooth transition

  var PRESETS = [
    { name: 'DEFAULT', values: { mid_aura: 42, high_aura: 56, safe: 1, wide: 0, mix: 100, low_latency: 0 } },
    { name: 'VOCAL LIFT', values: { mid_aura: 58, high_aura: 42, safe: 1, wide: 0, mix: 100, low_latency: 0 } },
    { name: 'AIRY FEMALE', values: { mid_aura: 36, high_aura: 72, safe: 1, wide: 1, mix: 100, low_latency: 0 } },
    { name: 'MODERN RAP SHINE', values: { mid_aura: 64, high_aura: 60, safe: 0, wide: 0, mix: 100, low_latency: 0 } },
    { name: 'SMOOTH PRESENCE', values: { mid_aura: 48, high_aura: 28, safe: 1, wide: 0, mix: 85, low_latency: 0 } },
    { name: 'STEREO POLISH', values: { mid_aura: 30, high_aura: 48, safe: 1, wide: 1, mix: 70, low_latency: 0 } },
    { name: 'EXPENSIVE TOP END', values: { mid_aura: 22, high_aura: 78, safe: 1, wide: 1, mix: 80, low_latency: 0 } },
    { name: 'PODCAST CLARITY', values: { mid_aura: 54, high_aura: 24, safe: 1, wide: 0, mix: 90, low_latency: 0 } },
    { name: 'ETHEREAL', values: { mid_aura: 26, high_aura: 84, safe: 0, wide: 1, mix: 100, low_latency: 0 } },
    { name: 'HYPER GLOSS', values: { mid_aura: 72, high_aura: 82, safe: 0, wide: 1, mix: 100, low_latency: 0 } }
  ];
  var currentPresetIndex = 0;

  var canvas = null;
  var ctx = null;
  var dpr = 1;
  var phase = 0;
  var frame = 0;
  var isInitialised = false;
  var auraEnergySmooth = 0;
  var auraTransient = 0;
  var auraBursts = [];
  var cosmicReferenceEl = null;

  function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }
  function lerp(a, b, t) { return a + (b - a) * t; }

  // A/B Snapshot Management
  function saveSnapshot(slot) {
    var snap = abSnapshots[slot];
    Object.keys(PARAMS).forEach(function (param) {
      snap[param] = values[param];
    });
  }

  function switchAB(targetSlot) {
    var nextSlot = targetSlot || (abActive === 'A' ? 'B' : 'A');
    if (abTransitioning || nextSlot === abActive) return;
    saveSnapshot(abActive);
    abActive = nextSlot;
    startABTransition();
  }

  function startABTransition() {
    abTransitioning = true;
    abTransitionProgress = 0;
    abTransitionStart = Date.now();
    animateABTransition();
  }

  function animateABTransition() {
    var now = Date.now();
    var elapsed = now - abTransitionStart;
    abTransitionProgress = Math.min(elapsed / ABTransitionDuration, 1);

    var targetSnapshot = abSnapshots[abActive];
    var easeProgress = abTransitionProgress < 0.5 
      ? 2 * abTransitionProgress * abTransitionProgress 
      : -1 + (4 - 2 * abTransitionProgress) * abTransitionProgress;

    Object.keys(PARAMS).forEach(function (param) {
      var current = values[param];
      var target = targetSnapshot[param];
      var interpolated = lerp(current, target, easeProgress);
      setParam(param, interpolated, false);
    });

    updateABVisual();

    if (abTransitionProgress < 1) {
      window.requestAnimationFrame(animateABTransition);
    } else {
      abTransitioning = false;
      // Final values
      Object.keys(PARAMS).forEach(function (param) {
        setParam(param, targetSnapshot[param], false);
      });
      updateABVisual();
    }
  }

  function updateABVisual() {
    var pill = document.querySelector('.ab-pill');
    if (!pill) return;

    var letters = pill.querySelectorAll('.ab-letter');
    Array.prototype.forEach.call(letters, function (letter) {
      var active = letter.getAttribute('data-slot') === abActive;
      letter.classList.toggle('active', active);
    });
    pill.style.transition = 'none';
    
    if (abTransitioning) {
      var opacity = 0.7 + 0.3 * Math.sin(abTransitionProgress * Math.PI);
      pill.style.opacity = opacity;
      pill.style.boxShadow = '0 0 ' + (8 + abTransitionProgress * 12) + 'px rgba(240, 218, 163, ' + (0.3 + abTransitionProgress * 0.4) + ')';
    } else {
      pill.style.opacity = '1';
      pill.style.boxShadow = '0 0 8px rgba(240, 218, 163, 0.3)';
    }
  }

  function createListenerList() {
    return {
      list: [],
      add: function (fn) { this.list.push(fn); },
      fire: function (v) { for (var i = 0; i < this.list.length; i++) this.list[i](v); }
    };
  }

  function createSliderState(param) {
    if (!juceAvailable || !window.__JUCE__ || !window.__JUCE__.backend) return null;

    var backend = window.__JUCE__.backend;
    var id = '__juce__slider' + param;

    var state = {
      scaledValue: PARAMS[param].def,
      properties: { start: PARAMS[param].min, end: PARAMS[param].max, interval: 0, numSteps: 100 },
      valueChangedEvent: createListenerList(),
      setNormalisedValue: function (norm) {
        var scaled = this.properties.start + clamp(norm, 0, 1) * (this.properties.end - this.properties.start);
        this.scaledValue = scaled;
        backend.emitEvent(id, { eventType: 'valueChanged', value: scaled });
      },
      sliderDragStarted: function () { backend.emitEvent(id, { eventType: 'sliderDragStarted' }); },
      sliderDragEnded: function () { backend.emitEvent(id, { eventType: 'sliderDragEnded' }); }
    };

    backend.addEventListener(id, function (event) {
      if (!event) return;
      if (event.eventType === 'valueChanged' && typeof event.value === 'number') {
        state.scaledValue = event.value;
        state.valueChangedEvent.fire(event.value);
      }
    });

    backend.emitEvent(id, { eventType: 'requestInitialUpdate' });
    return state;
  }

  function pushParam(param) {
    if (!states[param]) return;
    var norm = (values[param] - PARAMS[param].min) / (PARAMS[param].max - PARAMS[param].min);
    states[param].setNormalisedValue(norm);
  }

  function knobDegrees(norm) {
    return -118 + norm * 236;
  }

  function knobProgressDegrees(norm) {
    return 250 + norm * 88;
  }

  function updateKnobVisual(param) {
    var norm = values[param] / 100;
    var ind = document.getElementById(param === 'mid_aura' ? 'mid-ind' : 'high-ind');
    var knob = document.getElementById(param === 'mid_aura' ? 'mid-knob' : 'high-knob');
    var val = document.getElementById(param === 'mid_aura' ? 'mid-value' : 'high-value');
    var deg = knobDegrees(norm);
    if (ind) ind.style.transform = 'rotate(' + deg.toFixed(2) + 'deg)';
    if (knob) knob.style.setProperty('--angle', deg.toFixed(2) + 'deg');
    if (knob) knob.style.setProperty('--end-angle', knobProgressDegrees(norm).toFixed(2) + 'deg');
    if (val) val.textContent = Math.round(values[param]) + '%';
  }

  function updateMixVisual() {
    var norm = values.mix / 100;
    var dot = document.getElementById('output-dot');
    var fill = document.getElementById('mix-fill');
    var val = document.getElementById('mix-value');
    if (dot) dot.style.left = (norm * 100).toFixed(2) + '%';
    if (fill) fill.style.width = (norm * 100).toFixed(2) + '%';
    if (val) val.textContent = Math.round(values.mix) + '%';
  }

  function updateToggleVisual(param) {
    var el = document.querySelector('.toggle[data-toggle="' + param + '"]');
    if (el) el.setAttribute('data-on', values[param] > 0.5 ? '1' : '0');
  }

  function setParam(param, v, push) {
    var meta = PARAMS[param];
    var next = clamp(v, meta.min, meta.max);
    if (meta.max === 1) next = Math.round(next);
    values[param] = next;

    // Update telemetry to reflect UI changes so visualization responds immediately
    if (param === 'mid_aura') telemetry.midAura = next;
    if (param === 'high_aura') telemetry.highAura = next;
    if (param === 'mix') telemetry.mix = next;

    if (param === 'mid_aura' || param === 'high_aura') updateKnobVisual(param);
    else if (param === 'mix') updateMixVisual();
    else updateToggleVisual(param);

    if (push) pushParam(param);
  }

  function initBridge() {
    juceAvailable = !!(window.__JUCE__ && window.__JUCE__.backend);
    Object.keys(PARAMS).forEach(function (param) {
      states[param] = createSliderState(param);
      if (states[param]) {
        states[param].valueChangedEvent.add(function () {
          setParam(param, states[param].scaledValue, false);
        });
      }
    });
  }

  function initKnobs() {
    var knobs = document.querySelectorAll('.aura-knob[data-param], .knob[data-param]');
    Array.prototype.forEach.call(knobs, function (el) {
      var param = el.dataset.param;
      el.addEventListener('pointerdown', function (ev) {
        ev.preventDefault();
        dragging = { param: param, pointerId: ev.pointerId, lastY: ev.clientY };
        el.setPointerCapture(ev.pointerId);
        if (states[param]) states[param].sliderDragStarted();
      });

      el.addEventListener('pointermove', function (ev) {
        if (!dragging || dragging.param !== param || dragging.pointerId !== ev.pointerId) return;
        var dy = dragging.lastY - ev.clientY;
        var fine = ev.shiftKey ? 0.25 : 1;
        setParam(param, values[param] + dy * 1.0 * fine, false);
        pushParam(param);
        dragging.lastY = ev.clientY;
      });

      function endDrag(ev) {
        if (!dragging || dragging.param !== param || dragging.pointerId !== ev.pointerId) return;
        if (states[param]) states[param].sliderDragEnded();
        dragging = null;
      }

      el.addEventListener('pointerup', endDrag);
      el.addEventListener('pointercancel', endDrag);
      el.addEventListener('lostpointercapture', function () { dragging = null; });
    });
  }

  function initToggles() {
    var toggles = document.querySelectorAll('.toggle[data-toggle]');
    Array.prototype.forEach.call(toggles, function (el) {
      var param = el.dataset.toggle;
      el.addEventListener('click', function () {
        setParam(param, values[param] > 0.5 ? 0 : 1, true);
      });
    });
  }

  function initMixSlider() {
    var rail = document.getElementById('mix-rail');
    if (!rail) return;

    function setMixFromPointer(clientX, push) {
      var rect = rail.getBoundingClientRect();
      if (!rect.width) return;
      var norm = clamp((clientX - rect.left) / rect.width, 0, 1);
      setParam('mix', norm * 100, false);
      if (push) pushParam('mix');
    }

    rail.addEventListener('pointerdown', function (ev) {
      ev.preventDefault();
      dragging = { param: 'mix', pointerId: ev.pointerId };
      rail.setPointerCapture(ev.pointerId);
      if (states.mix) states.mix.sliderDragStarted();
      setMixFromPointer(ev.clientX, true);
    });

    rail.addEventListener('pointermove', function (ev) {
      if (!dragging || dragging.param !== 'mix' || dragging.pointerId !== ev.pointerId) return;
      setMixFromPointer(ev.clientX, true);
    });

    function endMixDrag(ev) {
      if (!dragging || dragging.param !== 'mix' || dragging.pointerId !== ev.pointerId) return;
      if (states.mix) states.mix.sliderDragEnded();
      dragging = null;
    }

    rail.addEventListener('pointerup', endMixDrag);
    rail.addEventListener('pointercancel', endMixDrag);
    rail.addEventListener('lostpointercapture', function () {
      if (dragging && dragging.param === 'mix') dragging = null;
    });
  }

  function initAB() {
    var pill = document.querySelector('.ab-pill');
    if (!pill) return;
    pill.addEventListener('click', function (ev) {
      var target = ev.target.closest('.ab-letter');
      if (!target) return;
      switchAB(target.getAttribute('data-slot'));
    });
    updateABVisual();
  }

  function updatePresetVisual() {
    var nameEl = document.getElementById('preset-name');
    if (nameEl) nameEl.textContent = PRESETS[currentPresetIndex].name;

    var menu = document.getElementById('preset-menu');
    if (!menu) return;
    var items = menu.querySelectorAll('.preset-item');
    Array.prototype.forEach.call(items, function (item) {
      var isActive = Number(item.getAttribute('data-index')) === currentPresetIndex;
      item.classList.toggle('active', isActive);
    });
  }

  function applyPreset(index, push) {
    var count = PRESETS.length;
    currentPresetIndex = (index % count + count) % count;
    var preset = PRESETS[currentPresetIndex];

    Object.keys(preset.values).forEach(function (param) {
      setParam(param, preset.values[param], false);
      if (push) pushParam(param);
    });

    updatePresetVisual();
  }

  function renderPresetMenu() {
    var menu = document.getElementById('preset-menu');
    if (!menu) return;

    menu.innerHTML = '';
    PRESETS.forEach(function (preset, i) {
      var btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'preset-item';
      btn.textContent = preset.name;
      btn.setAttribute('data-index', String(i));
      menu.appendChild(btn);
    });
  }

  function initPresets() {
    var menu = document.getElementById('preset-menu');
    var nameEl = document.getElementById('preset-name');
    if (!menu || !nameEl) return;

    renderPresetMenu();

    var navButtons = document.querySelectorAll('.preset-btn[data-dir]');
    Array.prototype.forEach.call(navButtons, function (btn) {
      btn.addEventListener('click', function () {
        var dir = Number(btn.getAttribute('data-dir')) || 0;
        applyPreset(currentPresetIndex + dir, true);
      });
    });

    nameEl.addEventListener('click', function () {
      menu.classList.toggle('open');
    });

    menu.addEventListener('click', function (ev) {
      var item = ev.target.closest('.preset-item');
      if (!item) return;
      applyPreset(Number(item.getAttribute('data-index')), true);
      menu.classList.remove('open');
    });

    document.addEventListener('click', function (ev) {
      if (!menu.classList.contains('open')) return;
      if (menu.contains(ev.target) || nameEl.contains(ev.target)) return;
      menu.classList.remove('open');
    });

    document.addEventListener('keydown', function (ev) {
      if (ev.key === 'Escape') menu.classList.remove('open');
    });

    applyPreset(0, false);
  }
  function drawField() {
    if (!ctx || !canvas) return;
    var w = canvas.clientWidth || Math.round(canvas.width / (dpr || 1));
    var h = canvas.clientHeight || Math.round(canvas.height / (dpr || 1));
    if (w < 2 || h < 2) return;

    var mid = (telemetry.midAura || values.mid_aura) / 100;
    var high = (telemetry.highAura || values.high_aura) / 100;
    var aura = clamp(telemetry.aura || 0, 0, 1);
    var harsh = clamp(telemetry.harsh || 0, 0, 1);
    var wide = (telemetry.wide || values.wide) > 0.5;
    var inEnergy = clamp((Math.abs(telemetry.inL || 0) + Math.abs(telemetry.inR || 0)) * 0.62, 0, 1);
    var outEnergy = clamp((Math.abs(telemetry.outL || 0) + Math.abs(telemetry.outR || 0)) * 0.62, 0, 1);
    var audioEnergy = clamp(inEnergy * 0.42 + outEnergy * 0.32 + aura * 0.2 + high * 0.06, 0, 1);

    var prevSmooth = auraEnergySmooth;
    auraEnergySmooth = lerp(auraEnergySmooth, audioEnergy, 0.12);
    auraTransient = Math.max(0, audioEnergy - prevSmooth);

    if (cosmicReferenceEl) {
      cosmicReferenceEl.style.opacity = '0';
      cosmicReferenceEl.style.visibility = 'hidden';
      cosmicReferenceEl.style.pointerEvents = 'none';
    }

    var midX = w * 0.5;
    var midY = h * 0.53;
    var leftX = w * 0.08;
    var rightX = w * 0.92;
    var rightPlasmaEndX = rightX - Math.max(14, w * 0.035);
    var pulse = 0.5 + 0.5 * Math.sin(phase * 0.72);
    var energyFlicker = 0.985 + 0.015 * Math.sin(phase * 3.15 + auraEnergySmooth * 2.2);
    var asymDrift = Math.sin(phase * 0.47 + 0.9) * 0.008 + Math.sin(phase * 1.03 + 2.1) * 0.005;
    var imbalancePulse = 0.5 + 0.5 * Math.sin(phase * 0.41 + Math.sin(phase * 0.12) * 1.9);
    var nodeR = Math.min(w, h) * (0.042 + auraEnergySmooth * 0.018 + high * 0.014);
    var haloR = nodeR * (3.3 + auraEnergySmooth * 0.8 + high * 0.4);

    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'rgb(3,5,13)';
    ctx.fillRect(0, 0, w, h);

    var bg = ctx.createRadialGradient(midX, midY, 0, midX, midY, w * 0.55);
    bg.addColorStop(0, 'rgba(86,146,255,' + (0.11 + high * 0.09).toFixed(3) + ')');
    bg.addColorStop(0.34, 'rgba(168,106,248,' + (0.07 + mid * 0.08).toFixed(3) + ')');
    bg.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, w, h);

    // Environmental lighting: soft reactor spill onto surrounding UI surfaces.
    var envPulse = 0.985 + 0.02 * Math.sin(phase * 1.25 + auraEnergySmooth * 3.6);
    var envSpread = ctx.createRadialGradient(midX, midY + h * 0.03, nodeR * 0.7, midX, midY + h * 0.03, w * 0.9);
    envSpread.addColorStop(0, 'rgba(142,214,255,' + ((0.028 + auraEnergySmooth * 0.024) * envPulse).toFixed(3) + ')');
    envSpread.addColorStop(0.46, 'rgba(154,118,246,' + ((0.018 + mid * 0.02) * envPulse).toFixed(3) + ')');
    envSpread.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = envSpread;
    ctx.fillRect(0, 0, w, h);

    // Bezel-edge leakage and soft panel reflection response.
    var topLeak = ctx.createLinearGradient(0, h * 0.12, 0, h * 0.34);
    topLeak.addColorStop(0, 'rgba(166,212,255,0)');
    topLeak.addColorStop(1, 'rgba(166,212,255,' + ((0.012 + high * 0.009) * envPulse).toFixed(3) + ')');
    ctx.fillStyle = topLeak;
    ctx.fillRect(0, 0, w, h * 0.34);

    var lowerLeak = ctx.createLinearGradient(0, h * 0.62, 0, h);
    lowerLeak.addColorStop(0, 'rgba(178,132,248,0)');
    lowerLeak.addColorStop(1, 'rgba(178,132,248,' + ((0.016 + auraEnergySmooth * 0.014) * envPulse).toFixed(3) + ')');
    ctx.fillStyle = lowerLeak;
    ctx.fillRect(0, h * 0.62, w, h * 0.38);

    // Premium reflective streaks: very faint, panel-conformal highlights.
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    for (var rs = 0; rs < 3; rs++) {
      var ry = h * (0.36 + rs * 0.1 + 0.01 * Math.sin(phase * 0.9 + rs));
      var rLen = w * (0.22 + rs * 0.06);
      var rx = midX - rLen * 0.5 + Math.sin(phase * 0.55 + rs * 1.2) * w * 0.01;
      var refl = ctx.createLinearGradient(rx, ry, rx + rLen, ry);
      var reflAlpha = (0.007 + rs * 0.003 + auraEnergySmooth * 0.006) * envPulse;
      refl.addColorStop(0, 'rgba(206,236,255,0)');
      refl.addColorStop(0.5, 'rgba(206,236,255,' + reflAlpha.toFixed(3) + ')');
      refl.addColorStop(1, 'rgba(206,236,255,0)');
      ctx.fillStyle = refl;
      ctx.fillRect(rx, ry - 0.8, rLen, 1.6 + rs * 0.5);
    }
    ctx.restore();

    // Volumetric depth planes for slight holographic spatial feeling.
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    for (var dp = 0; dp < 2; dp++) {
      var planeY = midY + h * (dp === 0 ? -0.03 : 0.05);
      var planeW = w * (0.44 + dp * 0.14);
      var planeH = h * (0.12 + dp * 0.03);
      var planeShift = Math.sin(phase * (0.42 + dp * 0.15) + dp * 1.3) * w * 0.012;
      var plane = ctx.createRadialGradient(midX + planeShift, planeY, 0, midX + planeShift, planeY, planeW * 0.5);
      plane.addColorStop(0, 'rgba(148,212,255,' + ((0.014 + auraEnergySmooth * 0.01 - dp * 0.003) * envPulse).toFixed(3) + ')');
      plane.addColorStop(0.68, 'rgba(168,124,244,' + ((0.01 + mid * 0.008 - dp * 0.002) * envPulse).toFixed(3) + ')');
      plane.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = plane;
      ctx.beginPath();
      ctx.ellipse(midX + planeShift, planeY, planeW * 0.5, planeH * 0.5, dp === 0 ? -0.08 : 0.06, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();

    // Extremely faint horizontal atmosphere band for cinematic scale.
    var hazeBand = ctx.createLinearGradient(midX - w * 0.38, midY, midX + w * 0.38, midY);
    hazeBand.addColorStop(0, 'rgba(128,188,255,0)');
    hazeBand.addColorStop(0.5, 'rgba(156,210,255,' + (0.016 + high * 0.012).toFixed(3) + ')');
    hazeBand.addColorStop(1, 'rgba(128,188,255,0)');
    ctx.fillStyle = hazeBand;
    ctx.fillRect(midX - w * 0.38, midY - h * 0.055, w * 0.76, h * 0.11);

    ctx.save();
    ctx.globalCompositeOperation = 'screen';

    var leftLen = (midX - nodeR * 1.18) - leftX;
    var leftBands = 4;
    for (var lb = 0; lb < leftBands; lb++) {
      var lbShift = (lb - (leftBands - 1) * 0.5) * h * 0.04;
      if (lb === 2) lbShift += h * (0.007 * asymDrift);
      var lbAmp = h * (0.015 + auraEnergySmooth * 0.026 + lb * 0.004);
      var lbFreq = 0.037 + lb * 0.008;
      var lbSpeed = 2.6 + lb * 0.6 + (lb === 1 ? 0.035 : 0) + (lb === 3 ? -0.02 : 0);
      var wavePulse = 0.5 + 0.5 * Math.sin(phase * (1.25 + lb * 0.1) + lb * 1.1);

      var lGrad = ctx.createLinearGradient(leftX, midY + lbShift, midX - nodeR * 1.1, midY + lbShift);
      var lAlpha = (0.18 + auraEnergySmooth * 0.18 + lb * 0.03) * energyFlicker;
      lGrad.addColorStop(0, 'rgba(152,98,246,' + (lAlpha * 0.72).toFixed(3) + ')');
      lGrad.addColorStop(0.34, 'rgba(126,186,255,' + (lAlpha * (0.72 + wavePulse * 0.2)).toFixed(3) + ')');
      lGrad.addColorStop(0.66, 'rgba(92,200,255,' + (lAlpha * (0.9 + wavePulse * 0.12)).toFixed(3) + ')');
      lGrad.addColorStop(1, 'rgba(232,246,255,' + (lAlpha * 0.72).toFixed(3) + ')');
      ctx.strokeStyle = lGrad;
      ctx.lineWidth = 1.6 + (leftBands - lb) * 0.6;
      ctx.beginPath();
      for (var lx = 0; lx <= leftLen; lx += 2) {
        var fade = Math.min(1, lx / 56) * Math.min(1, (leftLen - lx) / 44);
        var ly = midY + lbShift + Math.sin(lx * lbFreq + phase * lbSpeed) * lbAmp * fade;
        ly += Math.sin(lx * (lbFreq * 2.2) - phase * (lbSpeed * 0.55)) * lbAmp * 0.26 * fade;
        if (lx === 0) ctx.moveTo(leftX, ly);
        else ctx.lineTo(leftX + lx, ly);
      }
      ctx.stroke();
    }

    var threadCount = 12 + Math.round(auraEnergySmooth * 16 + high * 9);
    var spread = h * (0.34 + (wide ? 0.11 : 0) + mid * 0.09);

    function bezierPoint(t, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y) {
      var u = 1 - t;
      var tt = t * t;
      var uu = u * u;
      var uuu = uu * u;
      var ttt = tt * t;
      return {
        x: uuu * p0x + 3 * uu * t * p1x + 3 * u * tt * p2x + ttt * p3x,
        y: uuu * p0y + 3 * uu * t * p1y + 3 * u * tt * p2y + ttt * p3y
      };
    }

    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 0, rightPlasmaEndX - 2, h);
    ctx.clip();

    for (var i = 0; i < threadCount; i++) {
      var s = i / Math.max(1, threadCount - 1);
      var edge = Math.abs(s - 0.5) * 2;
      var yTarget = midY + (s - 0.5) * spread;
      yTarget += Math.sin(phase * (0.92 + i * 0.025) + i * 0.62) * (2 + harsh * 7 + high * 6);
      yTarget += Math.sin(phase * (0.39 + i * 0.011) + i * 0.87) * (0.45 + high * 0.55) * (0.6 + 0.4 * imbalancePulse);

      var cpXoff = 62 + (wide ? 52 : 30) + high * 24;
      var cp1x = midX + cpXoff;
      var cp1y = midY + Math.sin(phase * 1.16 + i * 0.56) * (4 + mid * 11);
      var cp2x = midX + cpXoff * 2.1 + (wide ? 74 : 44);
      var cp2y = yTarget - Math.sin(phase * 0.82 + i * 0.44) * (3 + high * 10);

      var alpha = (0.2 + auraEnergySmooth * 0.42) * (0.7 + (1 - edge) * 0.34) * energyFlicker;

      var pGlow = ctx.createLinearGradient(midX, midY, rightPlasmaEndX, yTarget);
      pGlow.addColorStop(0, 'rgba(174,112,252,' + (alpha * 0.64).toFixed(3) + ')');
      pGlow.addColorStop(0.52, 'rgba(92,204,255,' + (alpha * 0.8).toFixed(3) + ')');
      pGlow.addColorStop(0.9, 'rgba(255,182,92,' + (alpha * 0.18).toFixed(3) + ')');
      pGlow.addColorStop(1, 'rgba(255,182,92,0)');

      var pCore = ctx.createLinearGradient(midX, midY, rightPlasmaEndX, yTarget);
      pCore.addColorStop(0, 'rgba(194,126,255,' + alpha.toFixed(3) + ')');
      pCore.addColorStop(0.55, 'rgba(112,220,255,' + (alpha * 0.9).toFixed(3) + ')');
      pCore.addColorStop(0.88, 'rgba(246,206,132,' + (alpha * 0.12).toFixed(3) + ')');
      pCore.addColorStop(1, 'rgba(246,206,132,0)');

      var segments = 22;
      for (var layer = 0; layer < 2; layer++) {
        ctx.beginPath();
        ctx.lineCap = 'butt';
        for (var tt = 0; tt <= segments; tt++) {
          var t = (tt / segments) * 0.985;
          var bp = bezierPoint(t, midX, midY, cp1x, cp1y, cp2x, cp2y, rightPlasmaEndX, yTarget);
          var env = Math.sin(t * Math.PI);
          var twist = Math.sin(phase * (1.2 + i * 0.015) + t * 17 + i * 0.91);
          var flutter = Math.sin(phase * (2.1 + layer * 0.45) + t * 31 + i * 0.57);
          var microTurb = Math.sin(phase * (0.73 + i * 0.007) + t * 23 + i * 1.7) * (0.12 + high * 0.2);
          var turb = (twist * 0.9 + flutter * 0.5 + microTurb) * env * (3.2 + high * 7 + auraEnergySmooth * 5) * (0.96 + 0.04 * energyFlicker);
          var drift = Math.cos(phase * 0.76 + i * 0.3 + t * 8) * env * (1.1 + harsh * 2.2);
          var px = bp.x + drift;
          var py = bp.y + turb * (layer === 0 ? 0.75 : 0.32);
          if (tt === 0) ctx.moveTo(px, py);
          else ctx.lineTo(px, py);
        }

        if (layer === 0) {
          ctx.strokeStyle = pGlow;
          ctx.lineWidth = 2.7 + (1 - edge) * 2.6;
          ctx.globalAlpha = 0.2 + alpha * 0.09;
        } else {
          ctx.strokeStyle = pCore;
          ctx.lineWidth = 1.0 + (1 - edge) * 0.9 + Math.sin(phase * 1.8 + i * 0.3) * 0.08;
          ctx.globalAlpha = 0.82 + (0.16 * Math.sin(phase * 2.4 + i * 0.6));
        }
        ctx.stroke();
      }
      ctx.globalAlpha = 1;

      if ((i % 4) === 0) {
        var branchT = 0.58 + (i % 3) * 0.1;
        var bp = bezierPoint(branchT, midX, midY, cp1x, cp1y, cp2x, cp2y, rightPlasmaEndX, yTarget);
        var bLen = 18 + high * 22;
        var bAng = (s - 0.5) * 1.25 + Math.sin(phase * 0.7 + i) * 0.22;
        ctx.beginPath();
        ctx.moveTo(bp.x, bp.y);
        ctx.lineTo(bp.x + Math.cos(bAng) * bLen, bp.y + Math.sin(bAng) * bLen);
        ctx.strokeStyle = 'rgba(184,236,255,' + (0.12 + alpha * 0.35).toFixed(3) + ')';
        ctx.lineWidth = 0.8;
        ctx.stroke();
      }
    }
    ctx.restore();

    var seamHalf = h * (0.017 + auraEnergySmooth * 0.022 + mid * 0.01);
    var seam = ctx.createLinearGradient(midX - w * 0.26, midY, midX + w * 0.26, midY);
    seam.addColorStop(0, 'rgba(255,160,72,0)');
    seam.addColorStop(0.22, 'rgba(106,210,255,' + (0.15 + high * 0.1).toFixed(3) + ')');
    var seamShimmer = 0.988 + 0.022 * Math.sin(phase * 6.4 + auraTransient * 18 + high * 2.4);
    seam.addColorStop(0.5, 'rgba(255,249,236,' + ((0.56 + auraEnergySmooth * 0.26) * seamShimmer).toFixed(3) + ')');
    seam.addColorStop(0.78, 'rgba(176,122,255,' + (0.2 + mid * 0.09).toFixed(3) + ')');
    seam.addColorStop(1, 'rgba(255,160,72,0)');
    ctx.fillStyle = seam;
    ctx.fillRect(midX - w * 0.26, midY - seamHalf, w * 0.52, seamHalf * 2);

    // Hero filament: one crisp, luminous spine for premium contrast.
    var filamentLeft = midX - w * 0.2;
    var filamentRight = rightPlasmaEndX - w * 0.02;
    var filamentY = midY + Math.sin(phase * 0.9) * h * 0.004;
    var filamentGlow = ctx.createLinearGradient(filamentLeft, filamentY, filamentRight, filamentY);
    filamentGlow.addColorStop(0, 'rgba(172,118,252,0)');
    filamentGlow.addColorStop(0.5, 'rgba(184,236,255,' + (0.12 + auraEnergySmooth * 0.12).toFixed(3) + ')');
    filamentGlow.addColorStop(1, 'rgba(132,206,255,0)');
    ctx.strokeStyle = filamentGlow;
    ctx.lineWidth = 2.2;
    ctx.beginPath();
    for (var fx = 0; fx <= 44; fx++) {
      var ft = fx / 44;
      var px = filamentLeft + (filamentRight - filamentLeft) * ft;
      var py = filamentY
        + Math.sin(ft * Math.PI * 2.4 + phase * 1.05) * h * 0.003
        + Math.sin(ft * Math.PI * 8.2 - phase * 0.6) * h * 0.0009;
      if (fx === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    var filamentCore = ctx.createLinearGradient(filamentLeft, filamentY, filamentRight, filamentY);
    filamentCore.addColorStop(0, 'rgba(206,234,255,0)');
    filamentCore.addColorStop(0.5, 'rgba(242,252,255,' + (0.55 + auraEnergySmooth * 0.18).toFixed(3) + ')');
    filamentCore.addColorStop(1, 'rgba(206,234,255,0)');
    ctx.strokeStyle = filamentCore;
    ctx.lineWidth = 0.85;
    ctx.beginPath();
    for (var fx2 = 0; fx2 <= 44; fx2++) {
      var ft2 = fx2 / 44;
      var px2 = filamentLeft + (filamentRight - filamentLeft) * ft2;
      var py2 = filamentY
        + Math.sin(ft2 * Math.PI * 2.4 + phase * 1.05) * h * 0.0025
        + Math.sin(ft2 * Math.PI * 8.2 - phase * 0.6) * h * 0.0007;
      if (fx2 === 0) ctx.moveTo(px2, py2);
      else ctx.lineTo(px2, py2);
    }
    ctx.stroke();

    var impactPulse = 0.5 + 0.5 * Math.sin(phase * 2.9);
    var shockR = nodeR * (2.6 + impactPulse * 0.32 + auraEnergySmooth * 0.28);
    ctx.beginPath();
    ctx.arc(midX, midY, shockR, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(196,236,255,' + (0.038 + auraEnergySmooth * 0.035).toFixed(3) + ')';
    ctx.lineWidth = 0.9;
    ctx.stroke();

    // Soft distortion shell for subtle collision impact.
    var distort = ctx.createRadialGradient(midX, midY, nodeR * 0.95, midX, midY, nodeR * 2.4);
    distort.addColorStop(0, 'rgba(216,246,255,0)');
    distort.addColorStop(0.56, 'rgba(162,232,255,' + (0.03 + auraEnergySmooth * 0.025).toFixed(3) + ')');
    distort.addColorStop(1, 'rgba(216,246,255,0)');
    ctx.fillStyle = distort;
    ctx.beginPath();
    ctx.arc(midX, midY, nodeR * 2.45, 0, Math.PI * 2);
    ctx.fill();

    var halo = ctx.createRadialGradient(midX, midY, nodeR * 0.2, midX, midY, haloR);
    halo.addColorStop(0, 'rgba(228,246,255,0.22)');
    halo.addColorStop(0.28, 'rgba(110,222,255,' + (0.19 + high * 0.1).toFixed(3) + ')');
    halo.addColorStop(0.58, 'rgba(170,118,255,' + (0.13 + mid * 0.08).toFixed(3) + ')');
    halo.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = halo;
    ctx.beginPath();
    ctx.arc(midX, midY, haloR, 0, Math.PI * 2);
    ctx.fill();

    var centerFogA = ctx.createRadialGradient(midX, midY + h * 0.01, nodeR * 0.7, midX, midY + h * 0.03, haloR * 1.05);
    centerFogA.addColorStop(0, 'rgba(130,202,255,' + (0.055 + auraEnergySmooth * 0.03).toFixed(3) + ')');
    centerFogA.addColorStop(0.58, 'rgba(154,116,246,' + (0.04 + mid * 0.03).toFixed(3) + ')');
    centerFogA.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = centerFogA;
    ctx.beginPath();
    ctx.arc(midX, midY + h * 0.02, haloR * 1.02, 0, Math.PI * 2);
    ctx.fill();

    // Center-focused contrast shaping: slightly darken outer center to keep bite.
    var centerContrast = ctx.createRadialGradient(midX, midY, nodeR * 1.15, midX, midY, haloR * 1.32);
    centerContrast.addColorStop(0, 'rgba(0,0,0,0)');
    centerContrast.addColorStop(0.62, 'rgba(2,4,12,0.08)');
    centerContrast.addColorStop(1, 'rgba(2,4,12,0.16)');
    ctx.fillStyle = centerContrast;
    ctx.beginPath();
    ctx.arc(midX, midY, haloR * 1.34, 0, Math.PI * 2);
    ctx.fill();

    for (var rr = 0; rr < 2; rr++) {
      var ringR = nodeR * (1.8 + rr * 1.1) + pulse * nodeR * (0.2 + rr * 0.12);
      ctx.beginPath();
      ctx.arc(midX, midY, ringR, 0, Math.PI * 2);
      ctx.strokeStyle = 'rgba(194,234,255,' + (0.08 + (1 - rr) * 0.1 + auraEnergySmooth * 0.06).toFixed(3) + ')';
      ctx.lineWidth = 1.1 + rr * 0.7;
      ctx.stroke();
    }

    var coreFlicker = 0.5 + 0.5 * Math.sin(phase * 4.1 + auraEnergySmooth * 3.2);
    var coreWarpX = w * (0.0014 * asymDrift + 0.0009 * Math.sin(phase * 1.9));
    var coreWarpY = h * (0.0012 * Math.sin(phase * 1.3 + 0.7) + 0.0008 * asymDrift);
    var centerBreath = 0.996 + 0.012 * Math.sin(phase * 1.45 + auraEnergySmooth * 4.8);
    var reactiveShimmer = 0.988 + 0.022 * Math.sin(phase * (6.1 + 0.22 * asymDrift) + auraTransient * 18 + high * 2.4 + imbalancePulse * 0.5);
    var coreGlow = ctx.createRadialGradient(midX + coreWarpX, midY + coreWarpY, 0, midX + coreWarpX, midY + coreWarpY, nodeR * (1.16 * centerBreath));
    coreGlow.addColorStop(0, 'rgba(255,255,255,' + ((0.82 + auraEnergySmooth * 0.16 + coreFlicker * 0.04) * reactiveShimmer).toFixed(3) + ')');
    coreGlow.addColorStop(0.22, 'rgba(150,240,255,' + (0.6 + high * 0.14).toFixed(3) + ')');
    coreGlow.addColorStop(0.56, 'rgba(174,124,255,' + (0.24 + mid * 0.08).toFixed(3) + ')');
    coreGlow.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = coreGlow;
    ctx.beginPath();
    ctx.arc(midX + coreWarpX, midY + coreWarpY, nodeR * (1.16 * centerBreath), 0, Math.PI * 2);
    ctx.fill();

    var hotCore = ctx.createRadialGradient(midX + coreWarpX, midY + coreWarpY, 0, midX + coreWarpX, midY + coreWarpY, nodeR * (0.62 * centerBreath));
    hotCore.addColorStop(0, 'rgba(255,255,255,' + ((0.98 + coreFlicker * 0.02) * reactiveShimmer).toFixed(3) + ')');
    hotCore.addColorStop(0.34, 'rgba(236,252,255,0.98)');
    hotCore.addColorStop(0.72, 'rgba(122,226,255,0.2)');
    hotCore.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = hotCore;
    ctx.beginPath();
    ctx.arc(midX + coreWarpX, midY + coreWarpY, nodeR * ((0.58 + pulse * 0.08) * centerBreath), 0, Math.PI * 2);
    ctx.fill();

    var nucleus = ctx.createRadialGradient(midX + coreWarpX, midY + coreWarpY, 0, midX + coreWarpX, midY + coreWarpY, nodeR * 0.28);
    nucleus.addColorStop(0, 'rgba(255,255,255,' + ((0.96 + coreFlicker * 0.03) * reactiveShimmer).toFixed(3) + ')');
    nucleus.addColorStop(0.62, 'rgba(220,248,255,0.88)');
    nucleus.addColorStop(1, 'rgba(184,236,255,0)');
    ctx.fillStyle = nucleus;
    ctx.beginPath();
    ctx.arc(midX + coreWarpX, midY + coreWarpY, nodeR * 0.29, 0, Math.PI * 2);
    ctx.fill();

    // Sharper nucleus edge anchor.
    ctx.beginPath();
    ctx.arc(midX, midY, nodeR * 0.315, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(214,244,255,' + (0.2 + auraEnergySmooth * 0.1).toFixed(3) + ')';
    ctx.lineWidth = 0.85;
    ctx.stroke();

    // Micro high-frequency details: sparse precision sparks/branches.
    for (var ms = 0; ms < 8; ms++) {
      var sa = phase * 0.72 + ms * 0.79;
      var sr = nodeR * (0.48 + (ms % 3) * 0.15);
      var sx = midX + Math.cos(sa) * sr;
      var sy = midY + Math.sin(sa * 1.18) * sr * 0.72;
      var sl = 3.6 + (ms % 2) * 2.3;
      var sd = sa + Math.sin(phase * 1.9 + ms) * 0.32;
      ctx.beginPath();
      ctx.moveTo(sx, sy);
      ctx.lineTo(sx + Math.cos(sd) * sl, sy + Math.sin(sd) * sl);
      ctx.strokeStyle = 'rgba(220,244,255,' + (0.07 + auraEnergySmooth * 0.06).toFixed(3) + ')';
      ctx.lineWidth = 0.7;
      ctx.stroke();
    }

    // Optional ultra-faint HUD glass arc.
    ctx.beginPath();
    ctx.ellipse(midX, midY - h * 0.006, nodeR * 2.55, nodeR * 1.18, -0.06, Math.PI * 0.1, Math.PI * 0.9);
    ctx.strokeStyle = 'rgba(198,226,255,' + (0.035 + high * 0.02).toFixed(3) + ')';
    ctx.lineWidth = 0.8;
    ctx.stroke();

    // Near-invisible drifting dust around center for depth.
    for (var d = 0; d < 18; d++) {
      var dn = d / 18;
      var da = phase * (0.06 + (d % 4) * 0.012) + d * 0.77;
      var dr = nodeR * (1.1 + dn * 2.4 + 0.22 * Math.sin(phase * 0.5 + d));
      var dx = midX + Math.cos(da) * dr;
      var dy = midY + Math.sin(da * 1.17) * dr * 0.62;
      var dAlpha = (0.01 + 0.012 * (1 - dn)) * (0.8 + 0.2 * Math.sin(phase * 1.3 + d));
      ctx.fillStyle = 'rgba(206,234,255,' + dAlpha.toFixed(3) + ')';
      ctx.beginPath();
      ctx.arc(dx, dy, 0.45 + (d % 3) * 0.28, 0, Math.PI * 2);
      ctx.fill();
    }

    ctx.restore();

    if (auraTransient > 0.045 && audioEnergy > 0.2 && auraBursts.length < 18) {
      var burstStrength = clamp((auraTransient - 0.045) * 7.2, 0, 1);
      var emitCount = 1 + Math.round(burstStrength * 2);
      for (var e = 0; e < emitCount; e++) {
        var ang = ((e / Math.max(1, emitCount)) * Math.PI * 2) + phase * 0.2;
        auraBursts.push({
          x: midX + Math.cos(ang) * nodeR * 0.16,
          y: midY + Math.sin(ang) * nodeR * 0.16,
          vx: Math.cos(ang) * (0.45 + burstStrength * 1.2),
          vy: Math.sin(ang) * (0.45 + burstStrength * 1.2),
          life: 0.72,
          size: 0.6 + burstStrength * 0.9
        });
      }
    }

    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    for (var b = auraBursts.length - 1; b >= 0; b--) {
      var particle = auraBursts[b];
      particle.life -= 0.05;
      particle.x += particle.vx;
      particle.y += particle.vy;
      particle.vx *= 0.985;
      particle.vy *= 0.985;
      if (particle.life <= 0) {
        auraBursts.splice(b, 1);
        continue;
      }
      var pA = particle.life * (0.13 + auraEnergySmooth * 0.16);
      ctx.fillStyle = 'rgba(220,244,255,' + pA.toFixed(3) + ')';
      ctx.beginPath();
      ctx.arc(particle.x, particle.y, particle.size * (0.6 + particle.life), 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();

    var vignette = ctx.createRadialGradient(w * 0.5, h * 0.5, w * 0.22, w * 0.5, h * 0.5, w * 0.82);
    vignette.addColorStop(0, 'rgba(0,0,0,0)');
    vignette.addColorStop(1, 'rgba(1,2,8,0.78)');
    ctx.fillStyle = vignette;
    ctx.fillRect(0, 0, w, h);

    var outEl = document.getElementById('out-meter');
    if (outEl) outEl.textContent = 'MIX';
  }

  function stopRenderLoop() {
    if (frame) {
      window.cancelAnimationFrame(frame);
      frame = 0;
    }
  }

  function startRenderLoop() {
    if (frame) return;
    
    function loop() {
      frame = window.requestAnimationFrame(loop);
      phase += 0.008;
      drawField();
    }
    
    loop();
  }

  function resizeCanvas() {
    if (!canvas || !ctx) return;
    dpr = window.devicePixelRatio || 1;
    var r = canvas.getBoundingClientRect();
    canvas.width = Math.max(1, Math.floor(r.width * dpr));
    canvas.height = Math.max(1, Math.floor(r.height * dpr));
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  window.updateAuraTelemetry = function (data) {
    if (!data) return;
    telemetry.inL = Number(data.inL || telemetry.inL || 0);
    telemetry.inR = Number(data.inR || telemetry.inR || 0);
    telemetry.outL = Number(data.outL || telemetry.outL || 0);
    telemetry.outR = Number(data.outR || telemetry.outR || 0);
    telemetry.aura = Number(data.aura || telemetry.aura || 0);
    telemetry.harsh = Number(data.harsh || telemetry.harsh || 0);
    telemetry.midAura = Number(data.midAura || telemetry.midAura || values.mid_aura || 42);
    telemetry.highAura = Number(data.highAura || telemetry.highAura || values.high_aura || 56);
    telemetry.safe = Number(data.safe || telemetry.safe || values.safe || 0);
    telemetry.wide = Number(data.wide || telemetry.wide || values.wide || 0);
    telemetry.lowLatency = Number(data.lowLatency || telemetry.lowLatency || values.low_latency || 0);
  };

  function init() {
    if (isInitialised) return;
    isInitialised = true;

    initBridge();
    initKnobs();
    initToggles();
    initMixSlider();
    initPresets();
    initAB();

    canvas = document.getElementById('aura-canvas');
    ctx = canvas ? canvas.getContext('2d') : null;
    cosmicReferenceEl = document.getElementById('cosmic-reference');

    Object.keys(PARAMS).forEach(function (p) { setParam(p, values[p], false); });

    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
    startRenderLoop();
  }

  function recoverAfterVisibility() {
    if (!canvas || !ctx) {
      canvas = document.getElementById('aura-canvas');
      ctx = canvas ? canvas.getContext('2d') : null;
    }

    if (canvas && ctx) {
      resizeCanvas();
      startRenderLoop();
    }
  }

  document.addEventListener('visibilitychange', function () {
    if (document.hidden) {
      stopRenderLoop();
      return;
    }

    recoverAfterVisibility();
  });

  window.addEventListener('pageshow', function () {
    recoverAfterVisibility();
  });

  window.addEventListener('pagehide', function () {
    stopRenderLoop();
  });

  if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', init);
  else
    init();
})();
