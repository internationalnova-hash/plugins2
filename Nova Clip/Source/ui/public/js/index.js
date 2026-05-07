(function () {
  'use strict';

  var PARAMS = {
    drive: { min: 0, max: 24, def: 6, kind: 'db' },
    clip_shape: { min: 0, max: 100, def: 52, kind: 'percent' },
    tone: { min: 0, max: 100, def: 50, kind: 'percent' },
    punch: { min: 0, max: 100, def: 65, kind: 'percent' },
    ceiling: { min: -12, max: 0, def: -0.3, kind: 'dbSigned' },
    mix: { min: 0, max: 100, def: 100, kind: 'percent' },
    mode: { min: 0, max: 3, def: 0, kind: 'int' },
    oversampling: { min: 0, max: 2, def: 2, kind: 'int' },
    low_latency: { min: 0, max: 1, def: 0, kind: 'bool' },
    safe_mode: { min: 0, max: 1, def: 1, kind: 'bool' },
    link_lr: { min: 0, max: 1, def: 1, kind: 'bool' }
  };

  var KNOB_PARAMS = ['drive', 'tone', 'punch', 'clip_shape', 'ceiling', 'mix'];
  var MODE_NAMES = ['Smooth', 'Punch', 'Loud', 'Hard'];
  var PRESETS = [
    {
      name: 'Default Preset',
      values: { drive: 6.0, clip_shape: 52, tone: 50, punch: 65, ceiling: -0.3, mix: 100, mode: 0, oversampling: 2, low_latency: 0, safe_mode: 1, link_lr: 1 }
    },
    {
      name: 'Smooth Master',
      values: { drive: 4.2, clip_shape: 25, tone: 42, punch: 38, ceiling: -0.5, mix: 88, mode: 0, oversampling: 2, low_latency: 0, safe_mode: 1, link_lr: 1 }
    },
    {
      name: 'Drum Punch',
      values: { drive: 9.4, clip_shape: 58, tone: 56, punch: 82, ceiling: -0.3, mix: 92, mode: 1, oversampling: 2, low_latency: 0, safe_mode: 1, link_lr: 1 }
    },
    {
      name: 'Loud Mixbus',
      values: { drive: 12.8, clip_shape: 64, tone: 54, punch: 60, ceiling: -0.2, mix: 100, mode: 2, oversampling: 2, low_latency: 0, safe_mode: 1, link_lr: 1 }
    },
    {
      name: 'Hard Edge',
      values: { drive: 15.5, clip_shape: 90, tone: 62, punch: 72, ceiling: -0.8, mix: 100, mode: 3, oversampling: 1, low_latency: 1, safe_mode: 0, link_lr: 0 }
    }
  ];

  var juceAvailable = false;
  var states = {};
  var values = {};
  var dragging = null;
  var activePreset = 0;

  var telemetry = {
    inL: 0,
    inR: 0,
    outL: 0,
    outR: 0,
    clipDb: 0,
    heat: 0,
    drive: 6,
    shape: 52,
    tone: 50,
    punch: 65,
    ceiling: -0.3,
    mix: 100,
    mode: 0,
    safeMode: 1
  };

  var canvas = null;
  var ctx = null;
  var waveCanvas = null;
  var waveCtx = null;
  var dpr = 1;
  var frameHandle = 0;
  var phase = 0;
  var sparks = [];
  var embers = [];
  var displayValues = {};
  var analyzerState = null;
  var demoTick = 0;

  var BASE_WIDTH = 1280;
  var BASE_HEIGHT = 760;

  function clamp(v, lo, hi) {
    return Math.min(hi, Math.max(lo, v));
  }

  function lerp(a, b, t) {
    return a + (b - a) * t;
  }

  function smoothstep(edge0, edge1, x) {
    var t = clamp((x - edge0) / (edge1 - edge0), 0, 1);
    return t * t * (3 - 2 * t);
  }

  function chaos(seed) {
    return Math.sin(seed * 12.9898 + phase * 0.73) * 0.6 + Math.sin(seed * 3.131 + phase * 1.41) * 0.4;
  }

  function formatParam(param, value) {
    if (param === 'drive') return value.toFixed(1) + ' dB';
    if (param === 'ceiling') return value.toFixed(1) + ' dB';
    if (param === 'tone' || param === 'mix' || param === 'punch' || param === 'clip_shape') return Math.round(value) + '%';
    if (param === 'oversampling') return ['1x', '2x', '4x'][Math.round(value)] || '1x';
    return String(Math.round(value));
  }

  function toNorm(param, value) {
    var meta = PARAMS[param];
    if (!meta) return 0;
    return clamp((value - meta.min) / (meta.max - meta.min), 0, 1);
  }

  function fromNorm(param, norm) {
    var meta = PARAMS[param];
    if (!meta) return 0;
    return meta.min + clamp(norm, 0, 1) * (meta.max - meta.min);
  }

  function getDisplayedValue(param) {
    return typeof displayValues[param] === 'number' ? displayValues[param] : values[param];
  }

  function getAnalyzerState() {
    var driveNorm = clamp((telemetry.drive || values.drive || 6) / 24, 0, 1);
    var shapeNorm = clamp((telemetry.shape || values.clip_shape || 52) / 100, 0, 1);
    var punchNorm = clamp((telemetry.punch || values.punch || 65) / 100, 0, 1);
    var mixNorm = clamp((telemetry.mix || values.mix || 100) / 100, 0, 1);
    var heatNorm = clamp(telemetry.heat || 0, 0, 1);
    var clipNorm = clamp((telemetry.clipDb || 0) / 6, 0, 1);
    var safeMode = (telemetry.safeMode || values.safe_mode || 0) > 0.5 ? 1 : 0;
    var ceilingDb = typeof telemetry.ceiling === 'number' ? telemetry.ceiling : (values.ceiling || -0.3);
    var ceilingNorm = clamp((ceilingDb + 12) / 12, 0, 1);
    var aggression = clamp(driveNorm * 0.34 + shapeNorm * 0.16 + punchNorm * 0.12 + clipNorm * 0.36 + heatNorm * 0.28, 0, 1);

    return {
      driveNorm: driveNorm,
      shapeNorm: shapeNorm,
      punchNorm: punchNorm,
      mixNorm: mixNorm,
      heatNorm: heatNorm,
      clipNorm: clipNorm,
      safeMode: safeMode,
      ceilingDb: ceilingDb,
      ceilingNorm: ceilingNorm,
      aggression: aggression,
      bloom: clamp(0.18 + aggression * 0.82, 0, 1)
    };
  }

  function smoothAnalyzerState(target) {
    if (!analyzerState) {
      analyzerState = {
        driveNorm: target.driveNorm,
        shapeNorm: target.shapeNorm,
        punchNorm: target.punchNorm,
        mixNorm: target.mixNorm,
        heatNorm: target.heatNorm,
        clipNorm: target.clipNorm,
        ceilingDb: target.ceilingDb,
        ceilingNorm: target.ceilingNorm,
        aggression: target.aggression,
        bloom: target.bloom
      };
      return analyzerState;
    }

    var dragBoost = dragging ? 1 : 0;
    var energy = dragBoost ? 0.42 : 0.22;
    var clip = dragBoost ? 0.46 : 0.24;
    var bloom = dragBoost ? 0.44 : 0.22;

    // Control-linked parameters should feel immediate, not delayed.
    analyzerState.driveNorm = target.driveNorm;
    analyzerState.shapeNorm = target.shapeNorm;
    analyzerState.punchNorm = target.punchNorm;
    analyzerState.mixNorm = target.mixNorm;
    analyzerState.ceilingDb = target.ceilingDb;
    analyzerState.ceilingNorm = target.ceilingNorm;
    analyzerState.aggression = target.aggression;

    // Meter/energy values keep a bit of smoothing for analog realism.
    analyzerState.heatNorm = lerp(analyzerState.heatNorm, target.heatNorm, energy);
    analyzerState.clipNorm = lerp(analyzerState.clipNorm, target.clipNorm, clip);
    analyzerState.bloom = lerp(analyzerState.bloom, target.bloom, bloom);

    return analyzerState;
  }

  function createListenerList() {
    return {
      list: [],
      add: function (fn) { this.list.push(fn); },
      fire: function (v) {
        for (var i = 0; i < this.list.length; i++) this.list[i](v);
      }
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
      getNormalisedValue: function () {
        var range = this.properties.end - this.properties.start;
        if (range === 0) return 0;
        return clamp((this.scaledValue - this.properties.start) / range, 0, 1);
      },
      setNormalisedValue: function (norm) {
        var start = this.properties.start;
        var end = this.properties.end;
        var interval = this.properties.interval;
        var scaled = start + clamp(norm, 0, 1) * (end - start);
        if (interval > 0) scaled = Math.round(scaled / interval) * interval;
        this.scaledValue = scaled;
        backend.emitEvent(id, { eventType: 'valueChanged', value: scaled });
      },
      sliderDragStarted: function () { backend.emitEvent(id, { eventType: 'sliderDragStarted' }); },
      sliderDragEnded: function () { backend.emitEvent(id, { eventType: 'sliderDragEnded' }); }
    };

    backend.addEventListener(id, function (event) {
      if (!event) return;
      if (event.eventType === 'propertiesChanged') {
        for (var key in event) {
          if (Object.prototype.hasOwnProperty.call(event, key) && key !== 'eventType') state.properties[key] = event[key];
        }
      }
      if (event.eventType === 'valueChanged' && typeof event.value === 'number') {
        state.scaledValue = event.value;
        state.valueChangedEvent.fire(event.value);
      }
    });

    backend.emitEvent(id, { eventType: 'requestInitialUpdate' });
    return state;
  }

  function updateKnobVisual(param) {
    var shownValue = getDisplayedValue(param);
    var norm = toNorm(param, shownValue);
    var ring = document.getElementById(param + '-ring');
    var ind = document.getElementById(param + '-ind');
    var val = document.getElementById(param + '-value');
    var shell = document.querySelector('.knob-shell[data-param="' + param + '"]');
    var activeBoost = dragging && dragging.param === param ? 1 : 0;
    var deg = -120 + norm * 240;

    if (ring) {
      var total = 220;
      var fill = 18 + norm * (total - 18);
      ring.style.strokeDasharray = fill.toFixed(2) + ' 420';
      ring.style.filter = 'drop-shadow(0 0 ' + (6 + norm * 9).toFixed(2) + 'px rgba(255,176,63,' + (0.28 + norm * 0.54).toFixed(3) + '))';
    }

    if (ind) {
      ind.style.transform = 'rotate(' + deg.toFixed(2) + 'deg)';
    }

    if (shell) {
      shell.classList.toggle('is-active', !!activeBoost);
      shell.style.setProperty('--knobGlowAlpha', (0.16 + norm * 0.32 + activeBoost * 0.18).toFixed(3));
      shell.style.setProperty('--knobGlowSize', (18 + norm * 16 + activeBoost * 9).toFixed(1) + 'px');
      shell.style.setProperty('--glareX', (50 + Math.cos((deg - 28) * Math.PI / 180) * 16).toFixed(1) + '%');
      shell.style.setProperty('--glareY', (32 + Math.sin((deg - 28) * Math.PI / 180) * 11).toFixed(1) + '%');
      shell.style.setProperty('--glareAlpha', (0.12 + norm * 0.14 + activeBoost * 0.08).toFixed(3));
    }

    if (val) {
      val.textContent = formatParam(param, shownValue);
      val.style.boxShadow = 'inset 0 1px 0 rgba(255,255,255,0.08), 0 0 ' + (10 + norm * 18).toFixed(1) + 'px rgba(255,170,55,' + (0.10 + norm * 0.28).toFixed(3) + ')';
      val.style.color = 'rgba(255, 199, 106, ' + (0.88 + norm * 0.12).toFixed(3) + ')';
      val.style.transform = 'translateY(' + (-norm * 0.8 - activeBoost * 1.1).toFixed(2) + 'px)';
    }
  }

  function tickKnobAnimations() {
    for (var i = 0; i < KNOB_PARAMS.length; i++) {
      var param = KNOB_PARAMS[i];
      var target = values[param];
      var current = typeof displayValues[param] === 'number' ? displayValues[param] : target;
      var speed = dragging && dragging.param === param ? 0.62 : 0.42;
      var next = lerp(current, target, speed);

      if (Math.abs(next - target) < 0.0015) next = target;
      if (next !== current) {
        displayValues[param] = next;
        updateKnobVisual(param);
      }
    }
  }

  function updateModeButtons() {
    var mode = Math.round(values.mode || 0);
    var buttons = document.querySelectorAll('.mode-btn');
    Array.prototype.forEach.call(buttons, function (button) {
      button.classList.toggle('active', Number(button.dataset.mode) === mode);
    });
  }

  function updateToggleDot(param) {
    var dot = document.getElementById(param + '-dot');
    if (!dot) return;
    dot.classList.toggle('on', (values[param] || 0) > 0.5);
  }

  function updateOversamplingSelect() {
    var select = document.getElementById('oversampling-select');
    if (!select) return;
    select.value = String(Math.round(values.oversampling || 0));
  }

  function pushParam(param, opts) {
    if (!states[param]) return;
    var options = opts || {};
    var norm = toNorm(param, values[param]);

    try {
      if (options.gesture) states[param].sliderDragStarted();
      states[param].setNormalisedValue(norm);
      if (options.gesture) states[param].sliderDragEnded();
    } catch (_) {}
  }

  function setParam(param, value, push) {
    var meta = PARAMS[param];
    if (!meta) return;

    var next = clamp(value, meta.min, meta.max);
    if (meta.kind === 'int' || meta.kind === 'bool') next = Math.round(next);
    values[param] = next;

    if (KNOB_PARAMS.indexOf(param) >= 0) updateKnobVisual(param);
    if (param === 'mode') updateModeButtons();
    if (param === 'low_latency' || param === 'safe_mode' || param === 'link_lr') updateToggleDot(param);
    if (param === 'oversampling') updateOversamplingSelect();

    if (push) pushParam(param, { gesture: false });
  }

  function setPreset(index, push) {
    activePreset = (index + PRESETS.length) % PRESETS.length;
    var preset = PRESETS[activePreset];

    var name = document.getElementById('preset-name');
    if (name) name.textContent = preset.name;

    for (var param in preset.values) {
      if (Object.prototype.hasOwnProperty.call(preset.values, param)) setParam(param, preset.values[param], false);
    }

    if (push) {
      for (var key in preset.values) {
        if (Object.prototype.hasOwnProperty.call(preset.values, key)) pushParam(key, { gesture: false });
      }
    }
  }

  function initKnobs() {
    var knobEls = document.querySelectorAll('.knob-shell[data-param]');

    Array.prototype.forEach.call(knobEls, function (el) {
      var param = el.dataset.param;
      if (!PARAMS[param]) return;

      el.addEventListener('pointerdown', function (ev) {
        ev.preventDefault();
        var now = (window.performance && window.performance.now) ? window.performance.now() : Date.now();
        dragging = {
          param: param,
          pointerId: ev.pointerId,
          startY: ev.clientY,
          startV: values[param],
          currentV: values[param],
          lastY: ev.clientY,
          lastT: now,
          velocity: 0,
          visualTarget: values[param]
        };
        el.setPointerCapture(ev.pointerId);
        updateKnobVisual(param);
        if (states[param]) states[param].sliderDragStarted();
      });

      el.addEventListener('pointermove', function (ev) {
        if (!dragging || dragging.param !== param || dragging.pointerId !== ev.pointerId) return;

        var meta = PARAMS[param];
        var span = meta.max - meta.min;
        var now = (window.performance && window.performance.now) ? window.performance.now() : Date.now();
        var dt = Math.max(1, now - dragging.lastT);
        var dy = dragging.lastY - ev.clientY;
        var speed = Math.abs(dy) / dt;

        // Fast drags accelerate smoothly; slow drags stay high-resolution.
        var accel = 1.18 + clamp(speed * 2.8, 0, 2.4);
        var fine = ev.shiftKey ? 0.24 : (ev.altKey ? 0.14 : 1);
        var delta = dy * (span / 255) * accel * fine;

        dragging.currentV = clamp(dragging.currentV + delta, meta.min, meta.max);
        dragging.velocity = lerp(dragging.velocity, (delta / dt) * 16.7, 0.55);
        dragging.visualTarget = clamp(dragging.currentV + dragging.velocity * 2.2, meta.min, meta.max);

        // Apply true parameter value immediately, but let display glide a touch ahead.
        setParam(param, dragging.currentV, false);
        displayValues[param] = lerp(displayValues[param], dragging.visualTarget, 0.68);
        updateKnobVisual(param);
        pushParam(param, { gesture: false });

        dragging.lastY = ev.clientY;
        dragging.lastT = now;
      });

      function finishDrag(ev) {
        if (!dragging || dragging.param !== param || dragging.pointerId !== ev.pointerId) return;
        var targetValue = dragging.currentV;
        if (states[param]) states[param].sliderDragEnded();
        dragging = null;
        // Gentle visual settle after release creates a premium inertia feel.
        displayValues[param] = lerp(displayValues[param], targetValue, 0.56);
        updateKnobVisual(param);
      }

      el.addEventListener('pointerup', finishDrag);
      el.addEventListener('pointercancel', finishDrag);
      el.addEventListener('lostpointercapture', function () {
        if (dragging && dragging.param === param && states[param]) states[param].sliderDragEnded();
        if (dragging && dragging.param === param) dragging = null;
        updateKnobVisual(param);
      });
    });
  }

  function initModes() {
    var buttons = document.querySelectorAll('.mode-btn[data-mode]');
    Array.prototype.forEach.call(buttons, function (button) {
      button.addEventListener('click', function () {
        var mode = Number(button.dataset.mode);
        setParam('mode', mode, true);
      });
    });
  }

  function initUtility() {
    var os = document.getElementById('oversampling-select');
    if (os) {
      os.addEventListener('change', function () {
        setParam('oversampling', Number(os.value), true);
      });
    }

    ['low_latency', 'safe_mode', 'link_lr'].forEach(function (param) {
      var dot = document.getElementById(param + '-dot');
      if (!dot) return;
      dot.addEventListener('click', function () {
        setParam(param, (values[param] || 0) > 0.5 ? 0 : 1, true);
      });
    });
  }

  function initPresetUI() {
    var prev = document.getElementById('preset-prev');
    var next = document.getElementById('preset-next');

    if (prev) prev.addEventListener('click', function () { setPreset(activePreset - 1, true); });
    if (next) next.addEventListener('click', function () { setPreset(activePreset + 1, true); });

    var save = document.getElementById('preset-save');
    var undo = document.getElementById('preset-undo');
    var settings = document.getElementById('preset-settings');

    if (save) save.addEventListener('click', function () {});
    if (undo) undo.addEventListener('click', function () { setPreset(activePreset, true); });
    if (settings) settings.addEventListener('click', function () {});
  }

  function peakToDb(peak) {
    var v = Math.max(1e-5, peak);
    return 20 * Math.log10(v);
  }

  function peakToMeterHeight(peak) {
    var db = clamp(peakToDb(peak), -60, 6);
    return ((db + 60) / 66) * 100;
  }

  function updateMeters() {
    var inL = document.getElementById('in-l');
    var inR = document.getElementById('in-r');
    var outL = document.getElementById('out-l');
    var outR = document.getElementById('out-r');

    if (inL) inL.style.height = peakToMeterHeight(telemetry.inL).toFixed(2) + '%';
    if (inR) inR.style.height = peakToMeterHeight(telemetry.inR).toFixed(2) + '%';
    if (outL) outL.style.height = peakToMeterHeight(telemetry.outL).toFixed(2) + '%';
    if (outR) outR.style.height = peakToMeterHeight(telemetry.outR).toFixed(2) + '%';

    var inDb = document.getElementById('input-db');
    var outDb = document.getElementById('output-db');
    if (inDb) inDb.textContent = peakToDb(Math.max(telemetry.inL, telemetry.inR)).toFixed(1) + ' dB';
    if (outDb) outDb.textContent = peakToDb(Math.max(telemetry.outL, telemetry.outR)).toFixed(1) + ' dB';

    var reduction = document.getElementById('clip-reduction');
    if (reduction) reduction.textContent = 'CLIP REDUCTION ' + Math.max(0, telemetry.clipDb).toFixed(1) + ' dB';
  }

  function updateDemoTelemetry() {
    if (juceAvailable) return;

    demoTick += 0.038;

    var driveNorm = (values.drive || 6) / 24;
    var shapeNorm = (values.clip_shape || 52) / 100;
    var punchNorm = (values.punch || 65) / 100;
    var mixNorm = (values.mix || 100) / 100;

    var inputBase = 0.34 + 0.11 * Math.sin(demoTick * 1.3) + 0.07 * Math.sin(demoTick * 2.8);
    var clippingForce = clamp(driveNorm * 0.72 + shapeNorm * 0.34 + punchNorm * 0.28, 0, 1.5);
    var outputBase = inputBase + clippingForce * 0.18 * mixNorm;

    telemetry.inL = clamp(inputBase + 0.04 * Math.sin(demoTick * 3.2), 0.03, 1.0);
    telemetry.inR = clamp(inputBase + 0.04 * Math.sin(demoTick * 2.7 + 1.2), 0.03, 1.0);
    telemetry.outL = clamp(outputBase + 0.06 * Math.sin(demoTick * 4.1), 0.03, 1.0);
    telemetry.outR = clamp(outputBase + 0.06 * Math.sin(demoTick * 3.5 + 0.9), 0.03, 1.0);

    telemetry.clipDb = clamp(0.25 + clippingForce * 2.8 + 0.55 * Math.sin(demoTick * 1.7), 0, 6);
    telemetry.heat = clamp(clippingForce * 0.56 + 0.18 * Math.sin(demoTick * 2.2), 0, 1);

    telemetry.drive = values.drive;
    telemetry.shape = values.clip_shape;
    telemetry.tone = values.tone;
    telemetry.punch = values.punch;
    telemetry.ceiling = values.ceiling;
    telemetry.mix = values.mix;
    telemetry.mode = values.mode;

    updateMeters();
  }

  function transfer(x) {
    var drive = (telemetry.drive || values.drive || 6) / 24;
    var shape = (telemetry.shape || values.clip_shape || 52) / 100;
    var mode = Math.round(telemetry.mode || values.mode || 0);

    var driveScale = [0.86, 1.0, 1.14, 1.28][mode] || 1.0;
    var shapeBias = [-0.18, 0.0, 0.1, 0.26][mode] || 0;

    var d = 1 + drive * 7.8 * driveScale;
    var pushed = x * d;
    var s = clamp(shape + shapeBias, 0, 1);

    var k = 1.2 + (1 - s) * 10;
    var soft = Math.tanh(k * pushed) / Math.tanh(k);
    var hard = clamp(pushed, -1, 1);

    return lerp(soft, hard, Math.pow(s, 1.35));
  }

  function drawGrid(w, h) {
    ctx.save();
    ctx.strokeStyle = 'rgba(158, 182, 220, 0.08)';
    ctx.lineWidth = 1;

    for (var i = 0; i <= 10; i++) {
      var y = (h / 10) * i;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    for (var j = 0; j <= 12; j++) {
      var x = (w / 12) * j;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }

    ctx.restore();
  }

  function drawHeatAura(w, h, state) {
    var upperGlow = ctx.createRadialGradient(w * 0.72, h * 0.24, 16, w * 0.72, h * 0.24, w * 0.72);
    upperGlow.addColorStop(0, 'rgba(255,148,38,' + (0.10 + state.bloom * 0.28).toFixed(3) + ')');
    upperGlow.addColorStop(0.45, 'rgba(255,112,24,' + (0.05 + state.bloom * 0.12).toFixed(3) + ')');
    upperGlow.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = upperGlow;
    ctx.fillRect(0, 0, w, h);

    var floorGlow = ctx.createRadialGradient(w * 0.56, h * 0.84, 12, w * 0.56, h * 0.84, w * 0.46);
    floorGlow.addColorStop(0, 'rgba(255,106,18,' + (0.06 + state.aggression * 0.18).toFixed(3) + ')');
    floorGlow.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = floorGlow;
    ctx.fillRect(0, 0, w, h);
  }

  function drawCurveWaveform(w, h, state) {
    var margin = 26;
    var plotW = w - margin * 2;
    var plotH = h * 0.66;
    var plotTop = 14;
    var midY = plotTop + plotH * 0.5;
    var ampY = plotH * (0.16 + state.punchNorm * 0.04 + state.clipNorm * 0.02);
    var scroll = phase * (0.36 + state.aggression * 0.24);

    ctx.save();
    ctx.beginPath();
    ctx.rect(margin, plotTop, plotW, plotH);
    ctx.clip();

    var wash = ctx.createLinearGradient(0, plotTop, 0, plotTop + plotH);
    wash.addColorStop(0, 'rgba(255, 132, 28, ' + (0.03 + state.bloom * 0.04).toFixed(3) + ')');
    wash.addColorStop(1, 'rgba(0, 0, 0, 0)');
    ctx.fillStyle = wash;
    ctx.fillRect(margin, plotTop, plotW, plotH);

    ctx.strokeStyle = 'rgba(214, 226, 248, ' + (0.18 + state.heatNorm * 0.12).toFixed(3) + ')';
    ctx.lineWidth = 1.15;
    ctx.beginPath();
    for (var i = 0; i < plotW; i++) {
      var t = i / Math.max(1, plotW - 1);
      var travel = t + scroll * 0.22;
      var raw = Math.sin((travel * 9.5 + scroll * 0.7) * Math.PI * 2);
      raw += 0.28 * Math.sin((travel * 21 + scroll * 1.18) * Math.PI * 2) * (0.42 + state.punchNorm * 0.34);
      raw += 0.11 * Math.sin((travel * 47 + scroll * 1.9) * Math.PI * 2) * state.clipNorm;
      var y = midY + raw * ampY;
      if (i === 0) ctx.moveTo(margin + i, y);
      else ctx.lineTo(margin + i, y);
    }
    ctx.stroke();

    var curveTrailCount = 1 + Math.round(state.clipNorm * 2);
    for (var ct = 0; ct < curveTrailCount; ct++) {
      var curveJitter = chaos(2.4 + ct * 1.73);
      var curveLag = (0.038 + ct * 0.021) * (1 + curveJitter * 0.18);
      var curveSkew = (ct % 2 === 0 ? 1 : -1) * (0.9 + curveJitter * 2.4);
      var curveDepth = curveTrailCount > 1 ? ct / (curveTrailCount - 1) : 0;
      var curveAlpha = (0.1 + state.clipNorm * 0.16) * (1 - curveDepth * 0.76) * (0.82 + curveJitter * 0.14);

      ctx.globalCompositeOperation = 'screen';
      ctx.strokeStyle = 'rgba(' + Math.round(255 - curveDepth * 38) + ', ' + Math.round(192 - curveDepth * 36) + ', ' + Math.round(98 - curveDepth * 20) + ', ' + clamp(curveAlpha, 0.03, 0.24).toFixed(3) + ')';
      ctx.shadowBlur = 2 + curveDepth * 8 + state.bloom * (4 + curveDepth * 3);
      ctx.shadowColor = 'rgba(255, 156, 44, ' + clamp(curveAlpha * (1.18 - curveDepth * 0.22), 0.05, 0.32).toFixed(3) + ')';
      ctx.lineWidth = 1.14 - curveDepth * 0.28 + state.clipNorm * 0.2;
      ctx.beginPath();
      for (var cti = 0; cti < plotW; cti++) {
        var ctt = cti / Math.max(1, plotW - 1);
        var ctTravel = ctt + (scroll - curveLag) * 0.22;
        var ctRaw = Math.sin((ctTravel * 9.5 + (scroll - curveLag) * 0.7) * Math.PI * 2);
        ctRaw += 0.28 * Math.sin((ctTravel * 21 + (scroll - curveLag) * 1.18) * Math.PI * 2) * (0.42 + state.punchNorm * 0.34);
        ctRaw += 0.11 * Math.sin((ctTravel * 47 + (scroll - curveLag) * 1.9) * Math.PI * 2) * state.clipNorm;
        var ctY = midY + ctRaw * ampY + curveSkew;
        if (cti === 0) ctx.moveTo(margin + cti, ctY);
        else ctx.lineTo(margin + cti, ctY);
      }
      ctx.stroke();
    }

    ctx.globalCompositeOperation = 'screen';
    ctx.strokeStyle = 'rgba(255, 174, 58, ' + (0.16 + state.bloom * 0.22).toFixed(3) + ')';
    ctx.shadowBlur = 10 + state.bloom * 16;
    ctx.shadowColor = 'rgba(255, 143, 32, ' + (0.22 + state.bloom * 0.32).toFixed(3) + ')';
    ctx.lineWidth = 1.5 + state.clipNorm * 0.4;
    ctx.beginPath();
    for (var j = 0; j < plotW; j++) {
      var t2 = j / Math.max(1, plotW - 1);
      var travel2 = t2 + scroll * 0.24;
      var raw2 = Math.sin((travel2 * 9.5 + scroll * 0.7) * Math.PI * 2);
      raw2 += 0.28 * Math.sin((travel2 * 21 + scroll * 1.18) * Math.PI * 2) * (0.42 + state.punchNorm * 0.34);
      raw2 += 0.11 * Math.sin((travel2 * 47 + scroll * 1.9) * Math.PI * 2) * state.clipNorm;
      var clipped = transfer(raw2 * (1.04 + state.aggression * 0.76));
      var y2 = midY + clipped * ampY;
      if (j === 0) ctx.moveTo(margin + j, y2);
      else ctx.lineTo(margin + j, y2);
    }
    ctx.stroke();

    ctx.restore();
  }

  function drawTransferCurves(w, h, state) {
    var margin = 26;
    var plotW = w - margin * 2;
    var plotH = h * 0.66;
    var plotTop = 14;
    var ceilingY = lerp(plotH * 0.82, plotH * 0.12, state.ceilingNorm);
    var glow = 10 + state.bloom * 22;
    var hotThreshold = 0.76 - state.clipNorm * 0.08;
    var hotPoints = [];

    ctx.save();
    ctx.translate(margin, plotTop);

    ctx.lineWidth = 2.2;
    ctx.strokeStyle = 'rgba(232,240,255,0.64)';
    ctx.beginPath();
    for (var i = 0; i <= 120; i++) {
      var t = i / 120;
      var x = t * plotW;
      var y = plotH - t * plotH;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    ctx.save();
    ctx.shadowBlur = 14 + state.bloom * 28;
    ctx.shadowColor = 'rgba(255,136,30,' + (0.30 + state.bloom * 0.56).toFixed(3) + ')';
    ctx.strokeStyle = 'rgba(255,172,64,' + (0.34 + state.bloom * 0.38).toFixed(3) + ')';
    ctx.lineWidth = 1.6 + state.clipNorm * 1.4;
    ctx.beginPath();
    ctx.moveTo(0, ceilingY);
    ctx.lineTo(plotW, ceilingY);
    ctx.stroke();
    ctx.restore();

    ctx.shadowBlur = glow;
    ctx.shadowColor = 'rgba(255,160,56,' + (0.38 + state.bloom * 0.5).toFixed(3) + ')';
    ctx.strokeStyle = '#ffb63d';
    ctx.lineWidth = 3.4 + state.aggression * 0.6;
    ctx.beginPath();
    for (var j = 0; j <= 240; j++) {
      var n = j / 240;
      var xin = -1 + n * 2;
      var yout = transfer(xin);
      var px = n * plotW;
      var py = plotH - ((yout + 1) * 0.5) * plotH;
      if (Math.abs(yout) > hotThreshold && j % 3 === 0) {
        hotPoints.push({ x: px, y: py, hot: smoothstep(hotThreshold, 1, Math.abs(yout)) });
      }
      if (j === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    }
    ctx.stroke();

    ctx.shadowBlur = 0;
    ctx.strokeStyle = 'rgba(255,230,168,' + (0.34 + state.bloom * 0.18).toFixed(3) + ')';
    ctx.lineWidth = 1.15;
    ctx.stroke();

    if (hotPoints.length) {
      ctx.save();
      for (var k = 0; k < hotPoints.length; k++) {
        var point = hotPoints[k];
        ctx.globalAlpha = 0.18 + point.hot * 0.6;
        ctx.fillStyle = '#ffd58a';
        ctx.shadowBlur = 8 + point.hot * 16;
        ctx.shadowColor = 'rgba(255,158,48,0.9)';
        ctx.beginPath();
        ctx.arc(point.x, point.y, 0.9 + point.hot * 2.2, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.restore();
    }

    ctx.restore();
  }

  function drawWaveforms(state) {
    if (!waveCtx || !waveCanvas) return;

    var w = waveCanvas.width;
    var h = waveCanvas.height;
    var sidePad = 16;
    var centerGap = 18;
    var y0 = h * 0.56;
    var hh = h * 0.34;
    var halfW = w * 0.5;
    var inputStart = sidePad;
    var inputSpan = Math.max(1, Math.floor(halfW - sidePad - centerGap));
    var outputStart = Math.floor(halfW + centerGap);
    var outputSpan = Math.max(1, Math.floor(w - outputStart - sidePad));
    var scroll = phase * (0.38 + state.aggression * 0.26);
    var inputDrift = phase * 0.16;
    var sweepX = outputStart + (scroll * 12 % Math.max(1, outputSpan));
    var safeVisual = state.safeMode ? 1 : 0;
    var trailCount = 2 + Math.round(state.clipNorm * 2) - (safeVisual ? 1 : 0);
    trailCount = Math.max(1, trailCount);
    var peakFlatten = 0;
    var peakX = outputStart;
    var flattenAccum = 0;

    waveCtx.save();
    waveCtx.globalCompositeOperation = 'source-over';
    waveCtx.fillStyle = 'rgba(1, 4, 11, ' + ((dragging ? 0.54 : 0.46) - state.bloom * 0.06).toFixed(3) + ')';
    waveCtx.fillRect(0, 0, w, h);
    waveCtx.restore();

    var bgGlow = waveCtx.createLinearGradient(0, 0, 0, h);
    bgGlow.addColorStop(0, 'rgba(18, 28, 44, ' + (0.14 + state.heatNorm * 0.06).toFixed(3) + ')');
    bgGlow.addColorStop(1, 'rgba(0, 0, 0, 0.0)');
    waveCtx.fillStyle = bgGlow;
    waveCtx.fillRect(0, 0, w, h);

    var orangeWash = waveCtx.createRadialGradient(w * 0.72, h * 0.48, 4, w * 0.72, h * 0.48, w * 0.34);
    orangeWash.addColorStop(0, 'rgba(255,138,30,' + (0.08 + state.bloom * 0.18).toFixed(3) + ')');
    orangeWash.addColorStop(1, 'rgba(0,0,0,0)');
    waveCtx.fillStyle = orangeWash;
    waveCtx.fillRect(0, 0, w, h);

    var atmosphere = waveCtx.createRadialGradient(w * 0.52, h * 0.58, w * 0.18, w * 0.52, h * 0.58, w * 0.76);
    atmosphere.addColorStop(0, 'rgba(255,180,86,' + (0.02 + state.bloom * 0.05).toFixed(3) + ')');
    atmosphere.addColorStop(1, 'rgba(0,0,0,' + (0.12 + state.clipNorm * 0.08).toFixed(3) + ')');
    waveCtx.fillStyle = atmosphere;
    waveCtx.fillRect(0, 0, w, h);

    waveCtx.save();
    waveCtx.globalCompositeOperation = 'screen';
    waveCtx.globalAlpha = 0.08 + state.bloom * 0.12;
    waveCtx.fillStyle = '#ffb34a';
    waveCtx.fillRect(sweepX - 3, 0, 6, h);
    waveCtx.restore();

    waveCtx.save();
    waveCtx.globalAlpha = 0.98;

    waveCtx.strokeStyle = 'rgba(226,236,255,' + (0.62 + state.heatNorm * 0.16).toFixed(3) + ')';
    waveCtx.lineWidth = 1.8 + state.heatNorm * 0.2;
    waveCtx.beginPath();
    for (var i = 0; i < inputSpan; i++) {
      var t = i / inputSpan;
      var travelT = t + scroll * 0.18;
      var amp = 0.22 + 0.07 * Math.sin(travelT * 10.5 + inputDrift) + state.punchNorm * 0.05 * Math.sin(travelT * 26 + phase * 1.4);
      var s = Math.sin((travelT * 16.5 + scroll) * Math.PI * 2);
      s += 0.34 * Math.sin((travelT * 31 + scroll * 1.35) * Math.PI * 2) * (0.38 + state.punchNorm * 0.48);
      s += 0.12 * Math.sin((travelT * 54 + scroll * 2.2) * Math.PI * 2);
      var y = y0 + (s * amp) * hh;
      if (i === 0) waveCtx.moveTo(inputStart + i, y);
      else waveCtx.lineTo(inputStart + i, y);
    }
    waveCtx.stroke();

    for (var it = 0; it < trailCount; it++) {
      var iChaos = chaos(1.1 + it * 1.37) * (safeVisual ? 0.58 : 1.0);
      var iLag = (0.03 + it * 0.017) * (1 + iChaos * 0.25);
      var iDepth = trailCount > 1 ? it / (trailCount - 1) : 0;
      var iAlpha = (0.16 + state.heatNorm * 0.18) * (1 - iDepth * 0.78) * (0.82 + iChaos * 0.16);
      var iYOffset = (it % 2 ? -1 : 1) * (0.5 + iChaos * 1.6);

      waveCtx.strokeStyle = 'rgba(' + Math.round(214 - iDepth * 26) + ', ' + Math.round(226 - iDepth * 30) + ', ' + Math.round(248 - iDepth * 44) + ', ' + clamp(iAlpha, 0.03, 0.34).toFixed(3) + ')';
      waveCtx.lineWidth = 1.08 - iDepth * 0.28 + state.heatNorm * 0.1;
      waveCtx.shadowBlur = 1 + iDepth * 3;
      waveCtx.shadowColor = 'rgba(180,198,230,' + clamp(iAlpha * 0.6, 0.02, 0.16).toFixed(3) + ')';
      waveCtx.beginPath();
      for (var iti = 0; iti < inputSpan; iti++) {
        var itT = iti / inputSpan;
        var itTravel = itT + (scroll - iLag) * 0.18;
        var itAmp = 0.22 + 0.07 * Math.sin(itTravel * 10.5 + inputDrift - iLag * 18) + state.punchNorm * 0.05 * Math.sin(itTravel * 26 + phase * 1.4);
        var itS = Math.sin((itTravel * 16.5 + (scroll - iLag)) * Math.PI * 2);
        itS += 0.34 * Math.sin((itTravel * 31 + (scroll - iLag) * 1.35) * Math.PI * 2) * (0.38 + state.punchNorm * 0.48);
        itS += 0.12 * Math.sin((itTravel * 54 + (scroll - iLag) * 2.2) * Math.PI * 2);
        var itY = y0 + (itS * itAmp) * hh + iYOffset;
        if (iti === 0) waveCtx.moveTo(inputStart + iti, itY);
        else waveCtx.lineTo(inputStart + iti, itY);
      }
      waveCtx.stroke();
    }

    waveCtx.strokeStyle = '#ffae34';
    waveCtx.shadowBlur = 16 + state.bloom * 20;
    waveCtx.shadowColor = 'rgba(255,154,44,' + (0.42 + 0.46 * state.bloom).toFixed(3) + ')';
    waveCtx.lineWidth = 2.2 + state.clipNorm * 0.8;
    waveCtx.beginPath();
    for (var j = 0; j < outputSpan; j++) {
      var t2 = j / outputSpan;
      var travelOut = t2 + scroll * 0.2;
      var raw = Math.sin((travelOut * 15.5 + scroll * 1.12) * Math.PI * 2);
      raw += 0.28 * Math.sin((travelOut * 34 + scroll * 1.7) * Math.PI * 2) * (0.55 + state.aggression * 0.4);
      raw += 0.12 * Math.sin((travelOut * 72 + scroll * 2.6) * Math.PI * 2) * state.clipNorm;
      raw *= 0.34 + state.mixNorm * 0.08 + state.aggression * 0.28;

      var clipped = transfer(raw * (1.22 + state.aggression * 1.18));
      var flatten = smoothstep(0.54, 0.94, Math.abs(clipped)) * (0.22 + state.shapeNorm * 0.24 + state.clipNorm * 0.3);
      clipped = Math.sign(clipped) * lerp(Math.abs(clipped), 0.82 + state.clipNorm * 0.14, flatten);
      clipped = lerp(clipped, transfer(clipped * (0.96 + state.shapeNorm * 0.12)), 0.14 + state.clipNorm * 0.14);
      var y2 = y0 + clipped * hh;

      flattenAccum += flatten;
      if (flatten > peakFlatten) {
        peakFlatten = flatten;
        peakX = outputStart + j;
      }

      if (j === 0) waveCtx.moveTo(outputStart + j, y2);
      else waveCtx.lineTo(outputStart + j, y2);

      if (flatten > 0.26 && j % 5 === 0) {
        waveCtx.save();
        waveCtx.globalAlpha = 0.12 + flatten * 0.34;
        waveCtx.fillStyle = '#ffc76d';
        waveCtx.fillRect(outputStart + j - 1, y2 - 2, 2, 4);
        waveCtx.restore();
      }
    }
    waveCtx.stroke();

    for (var ot = 0; ot < trailCount + 1; ot++) {
      var oChaos = chaos(4.7 + ot * 1.91) * (safeVisual ? 0.54 : 1.0);
      var oLag = (0.028 + ot * 0.019) * (1 + oChaos * 0.3);
      var oDepth = (trailCount + 1) > 1 ? ot / trailCount : 0;
      var oAlpha = (0.2 + state.clipNorm * 0.24) * (1 - oDepth * 0.8) * (0.78 + oChaos * 0.18);
      var oYOffset = (ot % 2 ? -1 : 1) * (0.7 + oChaos * 1.9);

      waveCtx.strokeStyle = 'rgba(' + Math.round(255 - oDepth * 60) + ', ' + Math.round(176 - oDepth * 52) + ', ' + Math.round(72 - oDepth * 30) + ', ' + clamp(oAlpha, 0.04, 0.42).toFixed(3) + ')';
      waveCtx.shadowBlur = 4 + oDepth * 10 + state.bloom * 6;
      waveCtx.shadowColor = 'rgba(255,150,42,' + clamp(oAlpha * (0.95 - oDepth * 0.3), 0.06, 0.34).toFixed(3) + ')';
      waveCtx.lineWidth = 1.26 - oDepth * 0.34 + state.clipNorm * 0.22;
      waveCtx.beginPath();
      for (var otj = 0; otj < outputSpan; otj++) {
        var otT = otj / outputSpan;
        var otTravel = otT + (scroll - oLag) * 0.2;
        var otRaw = Math.sin((otTravel * 15.5 + (scroll - oLag) * 1.12) * Math.PI * 2);
        otRaw += 0.28 * Math.sin((otTravel * 34 + (scroll - oLag) * 1.7) * Math.PI * 2) * (0.55 + state.aggression * 0.4);
        otRaw += 0.12 * Math.sin((otTravel * 72 + (scroll - oLag) * 2.6) * Math.PI * 2) * state.clipNorm;
        otRaw += 0.035 * oChaos * Math.sin((otTravel * 118 + (scroll - oLag) * 3.2) * Math.PI * 2);
        otRaw *= 0.34 + state.mixNorm * 0.08 + state.aggression * 0.28;

        var otClipped = transfer(otRaw * (1.22 + state.aggression * 1.18));
        var otFlatten = smoothstep(0.54, 0.94, Math.abs(otClipped)) * (0.22 + state.shapeNorm * 0.24 + state.clipNorm * 0.3);
        otClipped = Math.sign(otClipped) * lerp(Math.abs(otClipped), 0.82 + state.clipNorm * 0.14, otFlatten);
        otClipped = lerp(otClipped, transfer(otClipped * (0.96 + state.shapeNorm * 0.12)), 0.12 + state.clipNorm * 0.16);
        var otY = y0 + otClipped * hh + oYOffset;

        if (otj === 0) waveCtx.moveTo(outputStart + otj, otY);
        else waveCtx.lineTo(outputStart + otj, otY);
      }
      waveCtx.stroke();
    }

    var density = clamp(flattenAccum / Math.max(1, outputSpan), 0, 1);
    var clipBloom = clamp((peakFlatten * 0.62 + density * 0.95) * (0.35 + state.clipNorm * 0.9) * (safeVisual ? 0.86 : 1.0), 0, 1);
    if (clipBloom > 0.04) {
      var bloomGrad = waveCtx.createRadialGradient(peakX, y0, 2, peakX, y0, 22 + clipBloom * 38);
      bloomGrad.addColorStop(0, 'rgba(255,186,92,' + (0.08 + clipBloom * 0.24).toFixed(3) + ')');
      bloomGrad.addColorStop(0.4, 'rgba(255,146,42,' + (0.05 + clipBloom * 0.16).toFixed(3) + ')');
      bloomGrad.addColorStop(1, 'rgba(0,0,0,0)');
      waveCtx.fillStyle = bloomGrad;
      waveCtx.fillRect(peakX - (34 + clipBloom * 36), y0 - (24 + clipBloom * 22), 2 * (34 + clipBloom * 36), 2 * (24 + clipBloom * 22));
    }

    waveCtx.save();
    waveCtx.globalCompositeOperation = 'screen';
    waveCtx.globalAlpha = 0.18 + state.bloom * 0.2;
    waveCtx.fillStyle = '#ff8f1d';
    waveCtx.fillRect(outputStart, y0 - hh * (0.78 + state.clipNorm * 0.18), outputSpan, 3 + state.clipNorm * 2);
    waveCtx.restore();

    waveCtx.restore();
  }

  function updateSparks(w, h, state) {
    var spawnChance = 0.08 + state.clipNorm * 0.54 + state.aggression * 0.24;
    if (Math.random() < spawnChance) {
      sparks.push({
        x: w * (0.56 + Math.random() * 0.34),
        y: h * (0.12 + Math.random() * 0.16),
        vx: (Math.random() - 0.5) * (1.2 + state.clipNorm * 1.8),
        vy: -0.7 - Math.random() * (1.5 + state.aggression * 1.6),
        life: 1,
        size: 0.9 + Math.random() * (1.2 + state.aggression * 2.4),
        heat: 0.5 + Math.random() * 0.5
      });
    }

    ctx.save();
    for (var i = sparks.length - 1; i >= 0; i--) {
      var p = sparks[i];
      p.x += p.vx;
      p.y += p.vy;
      p.life -= 0.026 + state.aggression * 0.018;

      if (p.life <= 0) {
        sparks.splice(i, 1);
        continue;
      }

      ctx.globalAlpha = p.life;
      ctx.fillStyle = p.heat > 0.7 ? '#ffd18c' : '#ffbe4a';
      ctx.shadowBlur = 10 + p.heat * 10;
      ctx.shadowColor = 'rgba(255,159,42,' + (0.46 + p.heat * 0.34).toFixed(3) + ')';
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
  }

  function updateEmbers(w, h, state) {
    var emberChance = 0.06 + state.driveNorm * 0.08 + state.heatNorm * 0.12;
    if (Math.random() < emberChance) {
      embers.push({
        x: w * (0.18 + Math.random() * 0.66),
        y: h * (0.72 + Math.random() * 0.18),
        vx: (Math.random() - 0.5) * 0.45,
        vy: -0.18 - Math.random() * 0.42,
        life: 0.35 + Math.random() * 0.65,
        size: 0.8 + Math.random() * 1.8,
        heat: 0.35 + Math.random() * 0.65
      });
    }

    ctx.save();
    ctx.globalCompositeOperation = 'screen';
    for (var i = embers.length - 1; i >= 0; i--) {
      var ember = embers[i];
      ember.x += ember.vx;
      ember.y += ember.vy;
      ember.life -= 0.008 + state.heatNorm * 0.012;

      if (ember.life <= 0) {
        embers.splice(i, 1);
        continue;
      }

      ctx.globalAlpha = ember.life * 0.42;
      ctx.fillStyle = ember.heat > 0.7 ? '#ffcf92' : '#ff8f24';
      ctx.shadowBlur = 8 + ember.heat * 10;
      ctx.shadowColor = 'rgba(255,132,24,' + (0.24 + ember.heat * 0.26).toFixed(3) + ')';
      ctx.beginPath();
      ctx.arc(ember.x, ember.y, ember.size, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
  }

  function draw() {
    if (!ctx || !canvas) return;

    var w = canvas.width;
    var h = canvas.height;

    updateDemoTelemetry();
    tickKnobAnimations();

    var state = smoothAnalyzerState(getAnalyzerState());

    ctx.save();
    ctx.globalCompositeOperation = 'source-over';
    ctx.fillStyle = 'rgba(2, 5, 13, ' + ((dragging ? 0.48 : 0.40) - state.bloom * 0.05).toFixed(3) + ')';
    ctx.fillRect(0, 0, w, h);
    ctx.restore();

    drawHeatAura(w, h, state);

    canvas.style.boxShadow = '0 0 ' + (10 + state.bloom * 26).toFixed(1) + 'px rgba(255,136,28,' + (0.08 + state.bloom * 0.22).toFixed(3) + ')';
    waveCanvas.style.boxShadow = 'inset 0 0 0 1px rgba(255,175,67,0.08), 0 0 ' + (8 + state.bloom * 18).toFixed(1) + 'px rgba(255,128,24,' + (0.06 + state.bloom * 0.18).toFixed(3) + ')';

    drawGrid(w, h);
    drawCurveWaveform(w, h, state);
    drawTransferCurves(w, h, state);
    updateEmbers(w, h, state);
    updateSparks(w, h, state);
    drawWaveforms(state);

    phase += 0.010 + state.aggression * 0.018;
    frameHandle = window.requestAnimationFrame(draw);
  }

  function resizeCanvas() {
    dpr = window.devicePixelRatio || 1;

    if (canvas) {
      var rect = canvas.getBoundingClientRect();
      canvas.width = Math.max(1, Math.floor(rect.width * dpr));
      canvas.height = Math.max(1, Math.floor(rect.height * dpr));
      if (ctx) ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }

    if (waveCanvas) {
      var waveRect = waveCanvas.getBoundingClientRect();
      waveCanvas.width = Math.max(1, Math.floor(waveRect.width * dpr));
      waveCanvas.height = Math.max(1, Math.floor(waveRect.height * dpr));
      if (waveCtx) waveCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
  }

  function fitPluginViewport() {
    var plugin = document.getElementById('plugin-root');
    var viewport = document.getElementById('viewport-root');
    if (!plugin || !viewport) return;

    var vw = Math.max(1, viewport.clientWidth - 8);
    var vh = Math.max(1, viewport.clientHeight - 8);
    var scale = Math.min(vw / BASE_WIDTH, vh / BASE_HEIGHT);

    // Never upscale; keep native sharpness when host is larger than base size.
    scale = Math.min(scale, 1);
    plugin.style.transform = 'translate(-50%, -50%) scale(' + scale.toFixed(4) + ')';
  }

  function initCanvas() {
    canvas = document.getElementById('curve-canvas');
    waveCanvas = document.getElementById('wave-canvas');
    if (!canvas || !waveCanvas) return;

    ctx = canvas.getContext('2d');
    waveCtx = waveCanvas.getContext('2d');

    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
    window.addEventListener('resize', fitPluginViewport);
    if (frameHandle) window.cancelAnimationFrame(frameHandle);
    draw();
  }

  window.updateClipTelemetry = function (data) {
    if (!data) return;
    telemetry.inL = Number(data.inL || telemetry.inL || 0);
    telemetry.inR = Number(data.inR || telemetry.inR || 0);
    telemetry.outL = Number(data.outL || telemetry.outL || 0);
    telemetry.outR = Number(data.outR || telemetry.outR || 0);
    telemetry.clipDb = Number(data.clipDb || telemetry.clipDb || 0);
    telemetry.heat = Number(data.heat || telemetry.heat || 0);
    telemetry.drive = Number(data.drive || telemetry.drive || values.drive || 6);
    telemetry.shape = Number(data.shape || telemetry.shape || values.clip_shape || 52);
    telemetry.tone = Number(data.tone || telemetry.tone || values.tone || 50);
    telemetry.punch = Number(data.punch || telemetry.punch || values.punch || 65);
    telemetry.ceiling = Number(data.ceiling || telemetry.ceiling || values.ceiling || -0.3);
    telemetry.mix = Number(data.mix || telemetry.mix || values.mix || 100);
    telemetry.mode = Number(data.mode || telemetry.mode || values.mode || 0);
    telemetry.safeMode = Number(data.safeMode || telemetry.safeMode || values.safe_mode || 0);
    updateMeters();
  };

  function initBridge() {
    juceAvailable = !!(window.__JUCE__ && window.__JUCE__.backend);

    for (var param in PARAMS) {
      if (Object.prototype.hasOwnProperty.call(PARAMS, param)) {
        values[param] = PARAMS[param].def;
        displayValues[param] = PARAMS[param].def;
      }
    }

    for (var key in PARAMS) {
      if (!Object.prototype.hasOwnProperty.call(PARAMS, key)) continue;

      states[key] = createSliderState(key);
      (function (param) {
        var state = states[param];
        if (!state) return;
        state.valueChangedEvent.add(function () {
          setParam(param, state.scaledValue, false);
        });
      })(key);
    }
  }

  function init() {
    fitPluginViewport();
    initBridge();
    initKnobs();
    initModes();
    initUtility();
    initPresetUI();
    initCanvas();

    setPreset(0, false);

    for (var param in PARAMS) {
      if (!Object.prototype.hasOwnProperty.call(PARAMS, param)) continue;
      setParam(param, values[param], false);
    }

    updateMeters();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
