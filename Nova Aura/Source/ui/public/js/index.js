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
    var w = canvas.width;
    var h = canvas.height;

    // Read knob positions: mapped to DSP character, not raw audio
    var mid = (telemetry.midAura || values.mid_aura) / 100;  // 0-1: purple energy, lower warmth
    var high = (telemetry.highAura || values.high_aura) / 100; // 0-1: cyan brilliance, upper shimmer
    var aura = clamp(telemetry.aura || 0, 0, 1);
    var harsh = clamp(telemetry.harsh || 0, 0, 1);
    var safe = (telemetry.safe || values.safe) > 0.5;
    var wide = (telemetry.wide || values.wide) > 0.5;

    // Base: maintain deep black foundation for elegance
    ctx.fillStyle = 'rgba(2,5,12,0.42)';
    ctx.fillRect(0, 0, w, h);

    // ========================================
    // CORE RADIAL GRADIENT: MID/HIGH mapping
    // ========================================
    // MID AURA: controls purple thickness & central body
    // HIGH AURA: controls cyan brilliance & upper lift
    var spreadX = wide ? 0.74 : 0.62;
    var coreY = h * (0.75 - high * 0.17);  // HIGH lifts center slightly
    var core = ctx.createRadialGradient(w * 0.5, coreY, 10, w * 0.5, h * 0.82, w * spreadX);
    
    // Purple: MID controls thickness (low mid = thin, high mid = rich)
    var purpleAlpha = 0.10 + mid * 0.24;
    core.addColorStop(0, 'rgba(190,140,246,' + purpleAlpha.toFixed(3) + ')');
    
    // Cyan: HIGH controls brilliance (low high = dim, high high = brilliant)
    var cyanAlpha = 0.07 + high * 0.20;
    core.addColorStop(0.42, 'rgba(142,198,248,' + cyanAlpha.toFixed(3) + ')');
    
    // Gold warmth: MID controls warmth glow (low mid = subtle, high mid = warm)
    var goldAlpha = 0.02 + mid * 0.06;
    core.addColorStop(0.7, 'rgba(236,195,126,' + goldAlpha.toFixed(3) + ')');
    
    core.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = core;
    ctx.fillRect(0, 0, w, h);

    // ========================================
    // FOG LAYERS: Atmospheric energy density
    // ========================================
    // MID AURA: controls lower-mid fog density & purple warmth
    // HIGH AURA: controls upper fog sparkle & motion intensity
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    var fogCount = 4 + Math.round(mid * 2); // Low MID: 4 layers, high MID: 6 layers (richer)
    for (var f = 0; f < fogCount; f++) {
      var fn = f / (fogCount - 1 || 1);
      
      // MID: positions fog in lower/mid region
      var vertPos = 0.58 + fn * 0.3 + mid * 0.12;
      
      // HIGH: adds upper shimmer motion
      var fx = w * (0.18 + fn * 0.64 + Math.sin(phase * (0.24 + fn * 0.2) + fn * 4.3) * 0.05);
      var fy = h * (vertPos - Math.sin(phase * (0.4 + fn * 0.22) + fn * 5) * (0.08 + high * 0.1));
      var fr = w * (0.15 + high * 0.12 + 0.065 * Math.sin(phase * 0.44 + f * 1.2));
      
      var fog = ctx.createRadialGradient(fx, fy, 0, fx, fy, fr);
      
      // MID controls purple fog (warmth/body)
      var purpleFog = 0.065 + mid * 0.09;
      fog.addColorStop(0, 'rgba(190,142,248,' + purpleFog.toFixed(3) + ')');
      
      // HIGH controls cyan fog (shimmer/detail)
      var cyanFog = 0.05 + high * 0.09;
      fog.addColorStop(0.44, 'rgba(122,186,242,' + cyanFog.toFixed(3) + ')');
      
      fog.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = fog;
      ctx.fillRect(0, 0, w, h);
    }
    ctx.restore();

    // ========================================
    // WAVEFORM RIBBONS: DSP energy body
    // ========================================
    // MID AURA: controls lower ribbon presence, body thickness, warmth
    // HIGH AURA: controls upper ribbon sharpness, energy sparkle, shimmer
    
    function drawRibbon(baseY, amp, freq, thickness, speed, a, b, alpha) {
      var grad = ctx.createLinearGradient(0, baseY, w, baseY);
      grad.addColorStop(0, 'rgba(' + a + ',' + (alpha * 0.7).toFixed(3) + ')');
      grad.addColorStop(0.5, 'rgba(' + b + ',' + alpha.toFixed(3) + ')');
      grad.addColorStop(1, 'rgba(' + a + ',' + (alpha * 0.7).toFixed(3) + ')');
      ctx.strokeStyle = grad;
      ctx.lineWidth = thickness;
      ctx.beginPath();
      for (var x = 0; x <= w; x++) {
        var n = x / w;
        var y = baseY + Math.sin((n * freq + phase * speed) * Math.PI * 2) * amp;
        y += Math.sin((n * (freq * 2.4) + phase * (speed * 1.7)) * Math.PI * 2) * amp * 0.34;
        if (x === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    // Upper ribbon: HIGH controls sharpness/brilliance
    var upperPresence = 0.28 + high * 0.36;  // Low HIGH: subtle, high HIGH: prominent
    drawRibbon(
      h * (0.5 - high * 0.12),
      h * (0.038 + high * 0.052),
      3.2 + mid * 2.1,
      1.8 + high * 1.2,
      0.2,
      '164,103,239',
      '103,209,255',
      upperPresence
    );
    
    // Mid ribbon: MID controls body/warmth
    var midPresence = 0.28 + mid * 0.32;  // Low MID: thin, high MID: rich
    drawRibbon(
      h * (0.61 - mid * 0.1),
      h * (0.038 + mid * 0.052),
      2.9 + high * 1.5,
      1.5 + mid * 1.4,
      0.16,
      '202,94,220',
      '103,190,250',
      midPresence
    );
    
    // Lower ribbon: MID controls presence (warmth floor)
    var lowerPresence = 0.15 + mid * 0.28;  // Low MID: sparse, high MID: warm
    drawRibbon(
      h * (0.71 - mid * 0.06),
      h * (0.028 + mid * 0.042),
      2.3 + mid * 1.15,
      1.2 + high * 0.6,
      0.12,
      '150,102,224',
      '91,169,234',
      lowerPresence
    );
    
    ctx.restore();

    // ========================================
    // PARTICLE SYSTEM: Energy activity distribution
    // ========================================
    // MID AURA: controls lower-half particle density, warmth palette activation
    // HIGH AURA: controls upper-half brightness, cyan/white sparkle, overall activity
    
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    // Particle count scales with energy: low = sparse, high = rich
    var midParticles = Math.round(110 + mid * 75);    // 110-185 particles based on MID
    var highParticles = Math.round(60 + high * 55);   // 60-115 particles based on HIGH
    var totalParticles = midParticles + highParticles;
    
    for (var i = 0; i < totalParticles; i++) {
      var seed = i * 0.723;
      var isMidDriven = i < midParticles;
      
      if (isMidDriven) {
        // MID-driven particles: lower region, warmth focus
        var drift = (phase * (0.012 + (i % 7) * 0.002)) % 1;
        var x = w * ((seed + drift * 0.34) % 1);
        var y = h * (0.48 + ((seed * 1.6 + drift * 0.48) % 1) * 0.32);  // Lower half
        var r = 0.35 + (i % 5) * 0.2;
        var opacity = (0.13 + mid * 0.17) * (0.6 + 0.4 * Math.sin(seed * 3.2 + phase * 0.06));
        
        // Palette: emphasize warm colors (purple, gold)
        var palette = i % 3;
        if (palette === 0) ctx.fillStyle = 'rgba(192,142,248,' + opacity.toFixed(3) + ')';  // Purple
        else if (palette === 1) ctx.fillStyle = 'rgba(235,196,126,' + opacity.toFixed(3) + ')';  // Gold
        else ctx.fillStyle = 'rgba(154,186,240,' + (opacity * 0.7).toFixed(3) + ')';  // Soft blue
      } else {
        // HIGH-driven particles: upper region, brightness/shimmer focus
        var highIdx = i - midParticles;
        var drift2 = (phase * (0.018 + (highIdx % 5) * 0.004)) % 1;
        var x = w * ((seed * 1.3 + drift2 * 0.54) % 1);
        var y = h * (0.16 + ((seed * 2.1 + drift2 * 0.68) % 1) * 0.3);  // Upper half
        var r = 0.36 + (highIdx % 4) * 0.26;
        var opacity = (0.10 + high * 0.22) * (0.5 + 0.5 * Math.sin(seed * 2.8 + phase * 0.08));
        
        // Palette: emphasize bright colors (cyan, white, pale)
        var brightPalette = highIdx % 3;
        if (brightPalette === 0) ctx.fillStyle = 'rgba(154,206,249,' + opacity.toFixed(3) + ')';  // Cyan
        else if (brightPalette === 1) ctx.fillStyle = 'rgba(230,240,252,' + opacity.toFixed(3) + ')';  // White
        else ctx.fillStyle = 'rgba(204,229,255,' + opacity.toFixed(3) + ')';  // Ice
      }
      
      ctx.beginPath();
      ctx.arc(x, y, r, 0, Math.PI * 2);
      ctx.fill();
    }
    
    ctx.restore();

    // ========================================
    // SHIMMER TRAILS: Energy sparkle & brilliance
    // ========================================
    // HIGH AURA: controls shimmer intensity, trail visibility, brilliance
    
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    var shimmerCount = 16 + Math.round(high * 28);  // 16-44 trails based on HIGH
    for (var s = 0; s < shimmerCount; s++) {
      var sn = s / (shimmerCount - 1 || 1);
      var sx = w * (0.08 + sn * 0.84 + Math.sin(s * 11.2 + phase * (0.24 + sn * 0.2)) * 0.02 * (wide ? 1.5 : 1));
      var sy = h * (0.16 + Math.abs(Math.sin(s * 7.2 + phase * 0.34)) * 0.5);
      var sh = h * (0.07 + (0.12 + high * 0.22) * Math.abs(Math.sin(s * 3.9 + phase * 0.46)));
      var sw = 0.5 + high * 0.64;
      var sa = (0.026 + high * 0.076 * Math.abs(Math.sin(s * 5.1 + phase * 0.22))) * (safe ? 0.86 : 1);
      
      var streak = ctx.createLinearGradient(sx, sy, sx, sy + sh);
      streak.addColorStop(0, 'rgba(255,255,255,0)');
      streak.addColorStop(0.5, 'rgba(204,229,255,' + sa.toFixed(3) + ')');
      streak.addColorStop(1, 'rgba(255,255,255,0)');
      ctx.fillStyle = streak;
      ctx.fillRect(sx, sy, sw, sh);
    }
    
    ctx.restore();

    // ========================================
    // BLOOM GLOW: Softness & warmth character
    // ========================================
    // MID AURA: controls bloom softness, warmth intensity
    // HIGH AURA: controls bloom position & distribution
    
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    var bloom = ctx.createRadialGradient(w * 0.5, h * (0.55 - high * 0.11), 0, w * 0.5, h * 0.58, w * (0.44 + high * 0.065));
    
    // Warmth glow: MID controls intensity (low MID = subtle, high MID = warm)
    bloom.addColorStop(0, 'rgba(248,230,208,' + (0.024 + mid * 0.034).toFixed(3) + ')');
    
    // Purple warmth: MID controls saturation
    bloom.addColorStop(0.34, 'rgba(181,120,244,' + (0.105 + mid * 0.115).toFixed(3) + ')');
    
    // Cyan lift: HIGH controls brilliance
    bloom.addColorStop(0.66, 'rgba(110,174,232,' + (0.094 + high * 0.11).toFixed(3) + ')');
    
    bloom.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = bloom;
    ctx.fillRect(0, 0, w, h);

    var rightBloom = ctx.createRadialGradient(w * 0.62, h * (0.49 - high * 0.06), 0, w * 0.62, h * 0.54, w * (0.28 + high * 0.05));
    rightBloom.addColorStop(0, 'rgba(214,236,255,' + (0.022 + high * 0.034).toFixed(3) + ')');
    rightBloom.addColorStop(0.42, 'rgba(128,190,244,' + (0.038 + high * 0.05).toFixed(3) + ')');
    rightBloom.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = rightBloom;
    ctx.fillRect(0, 0, w, h);
    
    ctx.restore();

    // ========================================
    // MICRO-DETAIL: Spectral character richness
    // ========================================
    // These layers communicate DSP subtlety without visual chaos.
    // They should remain imperceptible at low settings, delicate at high settings.
    
    // Spectral pockets: HIGH controls cyan sparkle richness
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    var pocketCount = Math.round(3 + high * 5);  // 3-8 pockets based on HIGH
    for (var p = 0; p < pocketCount; p++) {
      var pn = p / Math.max(1, pocketCount - 1);
      var px = w * (0.16 + pn * 0.68 + Math.sin(phase * 0.33 + p * 1.7) * 0.03);
      var py = h * (0.34 + Math.sin(phase * (0.27 + high * 0.08) + p * 2.3) * 0.16);
      var pr = w * (0.08 + pn * 0.04 + high * 0.02);
      
      var pocket = ctx.createRadialGradient(px, py, 0, px, py, pr);
      
      // HIGH controls: bias toward cyan/brilliant colors
      if (high > 0.5) {
        // High HIGH: cyan sparkle dominates
        pocket.addColorStop(0, 'rgba(118,198,255,' + (0.048 + high * 0.072).toFixed(3) + ')');
      } else if (high > 0.2) {
        // Mid HIGH: mixed palette
        if (p % 2 === 0) pocket.addColorStop(0, 'rgba(118,198,255,' + (0.036 + high * 0.05).toFixed(3) + ')');
        else pocket.addColorStop(0, 'rgba(184,126,244,' + (0.036 + high * 0.038).toFixed(3) + ')');
      } else {
        // Low HIGH: subtle purple warmth
        pocket.addColorStop(0, 'rgba(184,126,244,' + (0.024 + high * 0.026).toFixed(3) + ')');
      }
      
      pocket.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = pocket;
      ctx.fillRect(0, 0, w, h);
    }
    
    ctx.restore();

    // Micro threads: MID controls presence & motion energy
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    var threadCount = Math.round(3 + mid * 8);  // 3-11 threads based on MID
    for (var mt = 0; mt < threadCount; mt++) {
      var mtn = mt / Math.max(1, threadCount - 1);
      var yBase = h * (0.4 + mtn * 0.34);
      
      var mGrad = ctx.createLinearGradient(0, yBase, w, yBase);
      
      // MID controls: bias toward purple warmth
      var purpleStr = 0.02 + mid * 0.05;
      var cyanStr = 0.025 + mid * 0.035;
      
      mGrad.addColorStop(0, 'rgba(159,109,236,' + purpleStr.toFixed(3) + ')');
      mGrad.addColorStop(0.5, 'rgba(118,201,255,' + cyanStr.toFixed(3) + ')');
      mGrad.addColorStop(1, 'rgba(159,109,236,' + purpleStr.toFixed(3) + ')');
      
      ctx.strokeStyle = mGrad;
      ctx.lineWidth = 0.55 + mid * 0.15;
      ctx.beginPath();
      
      for (var mx = 0; mx <= w; mx += 2) {
        var mn = mx / w;
        // MID controls motion amplitude subtly
        var motionAmp = 0.002 + mtn * 0.002 + mid * 0.001;
        var my = yBase + Math.sin((mn * (4.6 + mtn * 2.2) + phase * (0.06 + mtn * 0.035 + mid * 0.02)) * Math.PI * 2) * h * motionAmp;
        if (mx === 0) ctx.moveTo(mx, my);
        else ctx.lineTo(mx, my);
      }
      ctx.stroke();
    }
    
    ctx.restore();

    // Star-like micro particles: MID/HIGH both contribute to atmospheric richness
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    
    var starCount = Math.round(80 + mid * 60 + high * 50);  // 80-190 stars based on both knobs
    for (var sp = 0; sp < starCount; sp++) {
      var ss = sp * 0.618;
      var sd = (phase * (0.004 + (sp % 5) * 0.0008)) % 1;
      var sx = w * ((ss + sd * 0.14) % 1);
      var sy = h * (0.12 + ((ss * 1.37 + sd * 0.24) % 1) * 0.76);
      var sr = 0.28 + (sp % 3) * 0.22;
      
      // Palette biased by MID/HIGH
      var starPalette = sp % 6;
      var opacity;
      
      if (mid > high) {
        // MID-dominant: warmer, purple-biased stars
        opacity = (0.09 + mid * 0.13) * (0.6 + 0.4 * Math.sin(ss * 2.1));
        if (starPalette < 3) ctx.fillStyle = 'rgba(192,150,244,' + opacity.toFixed(3) + ')';  // Purple
        else ctx.fillStyle = 'rgba(180,200,255,' + (opacity * 0.8).toFixed(3) + ')';  // Soft blue
      } else {
        // HIGH-dominant: brighter, cyan-biased stars
        opacity = (0.08 + high * 0.14) * (0.5 + 0.5 * Math.sin(ss * 1.8));
        if (starPalette < 3) ctx.fillStyle = 'rgba(198,228,255,' + opacity.toFixed(3) + ')';  // Cyan
        else ctx.fillStyle = 'rgba(220,235,255,' + (opacity * 0.9).toFixed(3) + ')';  // Pale
      }
      
      ctx.beginPath();
      ctx.arc(sx, sy, sr, 0, Math.PI * 2);
      ctx.fill();
    }
    
    ctx.restore();

    // ========================================
    // ATMOSPHERIC HAZE: Slow-drifting warm fog
    // ========================================
    // Large, ultra-soft, very low opacity blobs that drift slowly
    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    var hazeCount = 6;
    for (var hz = 0; hz < hazeCount; hz++) {
      var hzPhaseOffset = hz * 1.57;
      var hzX = w * (0.18 + hz * 0.165 + Math.sin(phase * 0.04 + hzPhaseOffset) * 0.06);
      var hzY = h * (0.42 + Math.sin(phase * 0.028 + hzPhaseOffset * 1.3) * 0.12);
      var hzR = w * (0.25 + hz * 0.042 + Math.sin(phase * 0.035 + hz) * 0.035);
      var hazeGrad = ctx.createRadialGradient(hzX, hzY, 0, hzX, hzY, hzR);
      var hzPurple = (0.032 + mid * 0.044) * (0.7 + 0.3 * Math.sin(phase * 0.06 + hz * 0.8));
      var hzCyan = (0.024 + high * 0.04) * (0.7 + 0.3 * Math.sin(phase * 0.05 + hz * 1.1));
      if (hz % 2 === 0) {
        hazeGrad.addColorStop(0, 'rgba(170,118,240,' + hzPurple.toFixed(3) + ')');
      } else {
        hazeGrad.addColorStop(0, 'rgba(106,180,245,' + hzCyan.toFixed(3) + ')');
      }
      hazeGrad.addColorStop(0.48, 'rgba(122,158,236,' + (0.012 + high * 0.02 + mid * 0.012).toFixed(3) + ')');
      hazeGrad.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = hazeGrad;
      ctx.fillRect(0, 0, w, h);
    }
    ctx.restore();

    // Preserve deep black floor: ensures energy reads as emerging light, not fog
    var blackFloor = ctx.createLinearGradient(0, h * 0.1, 0, h);
    blackFloor.addColorStop(0, 'rgba(0,0,0,0)');
    blackFloor.addColorStop(0.58, 'rgba(1,3,9,0.16)');
    blackFloor.addColorStop(1, 'rgba(1,2,8,0.36)');
    ctx.fillStyle = blackFloor;
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
