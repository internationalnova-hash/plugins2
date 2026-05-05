(function () {
  'use strict';

  var BINS = 96;
  var KNOB_ARC_TOTAL = 226;
  var KNOB_MIN_DEG = -120;
  var KNOB_MAX_DEG = 120;

  var PRESETS = {
    'Universal Smooth': { smooth: 42, focus: 60, air_preserve: 72, body: 62, output: 50, magic: true },
    'Vocal':            { smooth: 50, focus: 65, air_preserve: 70, body: 65, output: 50, magic: true },
    'Drums':            { smooth: 35, focus: 62, air_preserve: 55, body: 40, output: 50, magic: false },
    'Guitar':           { smooth: 40, focus: 58, air_preserve: 68, body: 70, output: 50, magic: true },
    'Master':           { smooth: 28, focus: 50, air_preserve: 75, body: 72, output: 50, magic: true }
  };

  var ALL_PARAMS = ['smooth', 'focus', 'air_preserve', 'body', 'output'];

  var currentValues = { smooth: 42, focus: 60, air_preserve: 72, body: 62, output: 50 };
  var paramStates = {};
  var juceAvailable = false;
  var magicOn = true;
  var activeDrag = null;
  var activePreset = 'Universal Smooth';

  var rawInput = new Float32Array(BINS);
  var rawProblem = new Float32Array(BINS);
  var rawReduction = new Float32Array(BINS);

  var smoothInput = new Float32Array(BINS);
  var smoothProblem = new Float32Array(BINS);
  var smoothReduction = new Float32Array(BINS);

  var phase = 0;
  var particles = [];

  var canvas = null;
  var ctx = null;
  var frameHandle = 0;

  function clamp(v, lo, hi) {
    return Math.min(hi, Math.max(lo, v));
  }

  function lerp(a, b, t) {
    return a + (b - a) * t;
  }

  function getAttr(el, name) {
    if (!el) return '';
    if (el.dataset && Object.prototype.hasOwnProperty.call(el.dataset, name)) return el.dataset[name];
    return el.getAttribute('data-' + name) || '';
  }

  function createListenerList() {
    return {
      _list: [],
      add: function (fn) { this._list.push(fn); },
      fire: function (value) {
        for (var i = 0; i < this._list.length; i++) this._list[i](value);
      }
    };
  }

  function createSliderState(name) {
    if (!juceAvailable || !window.__JUCE__ || !window.__JUCE__.backend) return null;

    var backend = window.__JUCE__.backend;
    var id = '__juce__slider' + name;

    var state = {
      name: name,
      scaledValue: 0,
      properties: { start: 0, end: 1, skew: 1, interval: 0, numSteps: 100 },
      valueChangedEvent: createListenerList(),
      getNormalisedValue: function () {
        var range = this.properties.end - this.properties.start;
        if (range === 0) return 0;
        return clamp((this.scaledValue - this.properties.start) / range, 0, 1);
      },
      setNormalisedValue: function (norm) {
        var v = clamp(norm, 0, 1);
        var start = this.properties.start;
        var end = this.properties.end;
        var interval = this.properties.interval;
        var scaled = start + v * (end - start);
        if (interval > 0) scaled = Math.round(scaled / interval) * interval;
        this.scaledValue = scaled;
        backend.emitEvent(id, { eventType: 'valueChanged', value: scaled });
      },
      sliderDragStarted: function () {
        backend.emitEvent(id, { eventType: 'sliderDragStarted' });
      },
      sliderDragEnded: function () {
        backend.emitEvent(id, { eventType: 'sliderDragEnded' });
      }
    };

    backend.addEventListener(id, function (ev) {
      if (!ev) return;
      if (ev.eventType === 'valueChanged' && typeof ev.value === 'number') {
        state.scaledValue = ev.value;
        state.valueChangedEvent.fire(ev);
      }
      if (ev.eventType === 'propertiesChanged') {
        for (var key in ev) {
          if (Object.prototype.hasOwnProperty.call(ev, key) && key !== 'eventType') {
            state.properties[key] = ev[key];
          }
        }
      }
    });

    backend.emitEvent(id, { eventType: 'requestInitialUpdate' });
    return state;
  }

  function formatKnob(param, percent) {
    if (param === 'output') {
      var db = -12 + (percent / 100) * 24;
      return (db >= 0 ? '+' : '') + db.toFixed(1) + ' dB';
    }
    return (percent / 10).toFixed(1);
  }

  function moveCaretToEnd(el) {
    if (!window.getSelection || !document.createRange) return;
    var sel = window.getSelection();
    var range = document.createRange();
    range.selectNodeContents(el);
    range.collapse(false);
    sel.removeAllRanges();
    sel.addRange(range);
  }

  function updateKnobVisual(param, percent) {
    var arc = document.getElementById(param + '-arc');
    var ind = document.getElementById(param + '-ind');
    var val = document.getElementById(param + '-value');

    var frac = clamp(percent, 0, 100) / 100;

    if (arc) {
      var filled = frac * KNOB_ARC_TOTAL;
      arc.setAttribute('stroke-dasharray', filled + ' ' + (KNOB_ARC_TOTAL - filled + 1));
    }

    if (ind) {
      var deg = KNOB_MIN_DEG + frac * (KNOB_MAX_DEG - KNOB_MIN_DEG);
      ind.style.transform = 'rotate(' + deg + 'deg)';
    }

    if (val && document.activeElement !== val) {
      val.textContent = formatKnob(param, percent);
    }
  }

  function setParamPercent(param, percent, opts) {
    var safe = clamp(percent, 0, 100);
    var settings = opts || {};
    currentValues[param] = safe;
    updateKnobVisual(param, safe);

    if (!settings.push) return;

    var state = paramStates[param];
    if (!state) return;

    try {
      if (settings.gesture) state.sliderDragStarted();
      state.setNormalisedValue(safe / 100);
      if (settings.gesture) state.sliderDragEnded();
    } catch (_) {}
  }

  function setMagic(on, push) {
    magicOn = !!on;
    var ctrl = document.getElementById('magic-control');
    if (ctrl) ctrl.classList.toggle('on', magicOn);

    if (push) {
      var state = paramStates.magic;
      if (state) {
        try {
          state.setNormalisedValue(magicOn ? 1 : 0);
        } catch (_) {}
      }
    }
  }

  function applyPreset(name, push) {
    var preset = PRESETS[name];
    if (!preset) return;

    activePreset = name;

    var buttons = document.querySelectorAll('.preset-btn');
    Array.prototype.forEach.call(buttons, function (btn) {
      btn.classList.toggle('active', getAttr(btn, 'preset') === name);
    });

    var shouldPush = push !== false;
    Array.prototype.forEach.call(ALL_PARAMS, function (p) {
      setParamPercent(p, preset[p], { push: shouldPush, gesture: false });
    });
    setMagic(preset.magic, shouldPush);
  }

  function initParamStates() {
    if (!juceAvailable) return;

    Array.prototype.forEach.call(ALL_PARAMS, function (p) {
      paramStates[p] = createSliderState(p);
      var state = paramStates[p];
      if (!state) return;

      state.valueChangedEvent.add(function () {
        setParamPercent(p, state.getNormalisedValue() * 100, { push: false });
      });
    });

    paramStates.magic = createSliderState('magic');
    if (paramStates.magic) {
      paramStates.magic.valueChangedEvent.add(function () {
        setMagic(paramStates.magic.getNormalisedValue() > 0.5, false);
      });
    }

    window.setTimeout(function () {
      Array.prototype.forEach.call(ALL_PARAMS, function (p) {
        if (paramStates[p]) {
          setParamPercent(p, paramStates[p].getNormalisedValue() * 100, { push: false });
        }
      });
      if (paramStates.magic) {
        setMagic(paramStates.magic.getNormalisedValue() > 0.5, false);
      }
    }, 100);
  }

  function getClientY(ev) {
    if (ev && ev.touches && ev.touches.length) return ev.touches[0].clientY;
    if (ev && ev.changedTouches && ev.changedTouches.length) return ev.changedTouches[0].clientY;
    return ev && typeof ev.clientY === 'number' ? ev.clientY : 0;
  }

  function startDrag(param, knob, ev, defaultPct) {
    if (ev && typeof ev.button === 'number' && ev.button !== 0) return;
    if (ev && ev.preventDefault) ev.preventDefault();

    activeDrag = {
      param: param,
      knob: knob,
      pointerId: ev && ev.pointerId != null ? ev.pointerId : null,
      startY: getClientY(ev),
      startValue: typeof currentValues[param] === 'number' ? currentValues[param] : defaultPct
    };

    if (activeDrag.pointerId != null && typeof knob.setPointerCapture === 'function') {
      try { knob.setPointerCapture(activeDrag.pointerId); } catch (_) {}
    }

    var state = paramStates[param];
    if (state) {
      try { state.sliderDragStarted(); } catch (_) {}
    }
  }

  function moveDrag(ev) {
    if (!activeDrag) return;
    if (activeDrag.pointerId != null && ev && ev.pointerId != null && ev.pointerId !== activeDrag.pointerId) return;
    if (ev && ev.preventDefault) ev.preventDefault();

    var delta = activeDrag.startY - getClientY(ev);
    var nextValue = clamp(activeDrag.startValue + delta * 0.45, 0, 100);
    setParamPercent(activeDrag.param, nextValue, { push: true });
  }

  function endDrag(ev) {
    if (!activeDrag) return;
    if (activeDrag.pointerId != null && ev && ev.pointerId != null && ev.pointerId !== activeDrag.pointerId) return;

    var state = paramStates[activeDrag.param];
    if (state) {
      try { state.sliderDragEnded(); } catch (_) {}
    }

    if (activeDrag.pointerId != null && typeof activeDrag.knob.releasePointerCapture === 'function') {
      try { activeDrag.knob.releasePointerCapture(activeDrag.pointerId); } catch (_) {}
    }

    activeDrag = null;
  }

  function handleScroll(param, ev) {
    if (ev && ev.preventDefault) ev.preventDefault();
    var amount = ev && ev.shiftKey ? 0.15 : 1;
    var sign = ev && ev.deltaY > 0 ? -1 : 1;
    setParamPercent(param, clamp(currentValues[param] + sign * amount, 0, 100), { push: true, gesture: true });
  }

  function initKnobs() {
    var knobs = document.querySelectorAll('.macro-knob');
    Array.prototype.forEach.call(knobs, function (knob) {
      var param = getAttr(knob, 'param');
      var def = parseFloat(getAttr(knob, 'default') || '0');

      updateKnobVisual(param, currentValues[param] || def);

      knob.addEventListener('pointerdown', function (ev) { startDrag(param, knob, ev, def); });
      knob.addEventListener('mousedown', function (ev) { startDrag(param, knob, ev, def); });
      knob.addEventListener('touchstart', function (ev) { startDrag(param, knob, ev, def); }, { passive: false });
      knob.addEventListener('wheel', function (ev) { handleScroll(param, ev); }, { passive: false });
      knob.addEventListener('dblclick', function () { setParamPercent(param, def, { push: true, gesture: true }); });
    });

    document.addEventListener('pointermove', moveDrag);
    document.addEventListener('mousemove', moveDrag);
    document.addEventListener('touchmove', moveDrag, { passive: false });
    document.addEventListener('pointerup', endDrag);
    document.addEventListener('mouseup', endDrag);
    document.addEventListener('touchend', endDrag);
    document.addEventListener('pointercancel', endDrag);
    document.addEventListener('touchcancel', endDrag);
    document.addEventListener('mouseleave', endDrag);
  }

  function initReadouts() {
    var readouts = document.querySelectorAll('.editable-value');
    Array.prototype.forEach.call(readouts, function (el) {
      var param = getAttr(el, 'param');

      el.addEventListener('focus', function () {
        if (param === 'output') {
          var db = -12 + (currentValues[param] / 100) * 24;
          el.textContent = db.toFixed(1);
        } else {
          el.textContent = (currentValues[param] / 10).toFixed(1);
        }
        moveCaretToEnd(el);
      });

      el.addEventListener('input', function () {
        var clean = el.textContent.replace(/[^0-9.\-]/g, '');
        if (clean !== el.textContent) {
          el.textContent = clean;
          moveCaretToEnd(el);
        }
      });

      el.addEventListener('keydown', function (ev) {
        if (ev.key === 'Enter') {
          ev.preventDefault();
          el.blur();
        }
        if (ev.key === 'Escape') {
          ev.preventDefault();
          updateKnobVisual(param, currentValues[param]);
          el.blur();
        }
      });

      el.addEventListener('blur', function () {
        var value = parseFloat(el.textContent);
        var pct;

        if (isNaN(value)) {
          pct = currentValues[param];
        } else if (param === 'output') {
          pct = clamp(((value + 12) / 24) * 100, 0, 100);
        } else {
          pct = clamp(value * 10, 0, 100);
        }

        setParamPercent(param, pct, { push: true, gesture: false });
      });
    });
  }

  function initPresetButtons() {
    var buttons = document.querySelectorAll('.preset-btn');
    Array.prototype.forEach.call(buttons, function (btn) {
      btn.addEventListener('click', function () {
        applyPreset(getAttr(btn, 'preset'), true);
      });
    });
  }

  function initMagicToggle() {
    var ctrl = document.getElementById('magic-control');
    if (!ctrl) return;

    ctrl.addEventListener('click', function () {
      setMagic(!magicOn, true);
    });
  }

  function initCanvas() {
    canvas = document.getElementById('silk-canvas');
    if (!canvas) return;

    ctx = canvas.getContext('2d');
    var wrap = canvas.parentElement;

    function resize() {
      canvas.width = wrap.clientWidth;
      canvas.height = wrap.clientHeight - 44;
    }

    resize();
    window.addEventListener('resize', resize);
  }

  function updateSignalData(inputArr, problemArr, reductionArr) {
    if (!inputArr || !problemArr || !reductionArr) return;

    for (var i = 0; i < BINS; i++) {
      rawInput[i] = typeof inputArr[i] === 'number' ? inputArr[i] : 0;
      rawProblem[i] = typeof problemArr[i] === 'number' ? problemArr[i] : 0;
      rawReduction[i] = typeof reductionArr[i] === 'number' ? reductionArr[i] : 0;

      smoothInput[i] = rawInput[i] > smoothInput[i]
        ? lerp(smoothInput[i], rawInput[i], 0.22)
        : lerp(smoothInput[i], rawInput[i], 0.07);

      smoothProblem[i] = rawProblem[i] > smoothProblem[i]
        ? lerp(smoothProblem[i], rawProblem[i], 0.19)
        : lerp(smoothProblem[i], rawProblem[i], magicOn ? 0.042 : 0.055);

      smoothReduction[i] = rawReduction[i] > smoothReduction[i]
        ? lerp(smoothReduction[i], rawReduction[i], 0.14)
        : lerp(smoothReduction[i], rawReduction[i], magicOn ? 0.03 : 0.05);
    }
  }

  window.updateSilkSpectrum = updateSignalData;

  function drawVerticalGrid(w, h) {
    var marks = [0.0, 0.18, 0.34, 0.5, 0.65, 0.82, 1.0];
    ctx.save();
    ctx.strokeStyle = 'rgba(179, 150, 236, 0.14)';
    ctx.lineWidth = 1;

    for (var i = 0; i < marks.length; i++) {
      var x = Math.round(marks[i] * w);
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }

    ctx.restore();
  }

  function drawBand(points, fillTop, fillBottom, stroke, blur, alpha) {
    var w = canvas.width;
    var h = canvas.height;

    ctx.save();
    ctx.globalAlpha = alpha;
    ctx.beginPath();
    for (var i = 0; i < BINS; i++) {
      var x = (i / (BINS - 1)) * w;
      var y = points[i];
      if (i === 0) ctx.moveTo(x, y);
      else {
        var prevX = ((i - 1) / (BINS - 1)) * w;
        var prevY = points[i - 1];
        var cpX = (prevX + x) * 0.5;
        ctx.bezierCurveTo(cpX, prevY, cpX, y, x, y);
      }
    }
    ctx.lineTo(w, h);
    ctx.lineTo(0, h);
    ctx.closePath();

    var fill = ctx.createLinearGradient(0, 0, 0, h);
    fill.addColorStop(0, fillTop);
    fill.addColorStop(1, fillBottom);
    ctx.fillStyle = fill;
    ctx.fill();

    ctx.beginPath();
    for (var j = 0; j < BINS; j++) {
      var lx = (j / (BINS - 1)) * w;
      var ly = points[j];
      if (j === 0) ctx.moveTo(lx, ly);
      else {
        var ppx = ((j - 1) / (BINS - 1)) * w;
        var ppy = points[j - 1];
        var cpx = (ppx + lx) * 0.5;
        ctx.bezierCurveTo(cpx, ppy, cpx, ly, lx, ly);
      }
    }

    ctx.shadowColor = stroke;
    ctx.shadowBlur = blur;
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 2.2;
    ctx.stroke();
    ctx.restore();
  }

  function ensureParticles() {
    if (particles.length >= 150) return;

    while (particles.length < 150) {
      particles.push({
        x: 0.66 + Math.random() * 0.34,
        y: 0.12 + Math.random() * 0.46,
        vx: -0.0002 + Math.random() * 0.00055,
        vy: -0.00035 + Math.random() * 0.0008,
        life: Math.random(),
        size: 0.4 + Math.random() * 1.9,
        warm: Math.random() > 0.45
      });
    }
  }

  function drawParticles(w, h) {
    ensureParticles();

    ctx.save();
    for (var i = 0; i < particles.length; i++) {
      var p = particles[i];
      p.life += 0.004 + (magicOn ? 0.002 : 0);
      if (p.life > 1) {
        p.life = 0;
        p.x = 0.66 + Math.random() * 0.34;
        p.y = 0.12 + Math.random() * 0.46;
        p.vx = -0.0002 + Math.random() * 0.00055;
        p.vy = -0.00035 + Math.random() * 0.0008;
        p.size = 0.4 + Math.random() * 1.9;
        p.warm = Math.random() > 0.45;
      }

      p.x += p.vx;
      p.y += p.vy;

      if (p.x < 0.62 || p.y < 0.02) {
        p.x = 0.66 + Math.random() * 0.34;
        p.y = 0.2 + Math.random() * 0.36;
      }

      var px = p.x * w;
      var py = p.y * h;
      var flicker = 0.35 + 0.65 * Math.sin((phase * 0.6 + i) * 0.35) * 0.5 + 0.5;
      var alpha = (0.18 + 0.42 * (1 - p.life)) * flicker;

      ctx.beginPath();
      ctx.arc(px, py, p.size, 0, Math.PI * 2);
      ctx.fillStyle = p.warm
        ? 'rgba(247, 201, 126, ' + alpha.toFixed(3) + ')'
        : 'rgba(200, 150, 255, ' + (alpha * 0.85).toFixed(3) + ')';
      ctx.shadowColor = p.warm ? 'rgba(248, 204, 132, 0.62)' : 'rgba(188, 133, 255, 0.58)';
      ctx.shadowBlur = 6;
      ctx.fill();
    }
    ctx.restore();
  }

  function drawSilkField() {
    if (!canvas || !ctx) return;

    var w = canvas.width;
    var h = canvas.height;

    phase += 0.0045;

    ctx.clearRect(0, 0, w, h);

    var bg = ctx.createLinearGradient(0, 0, 0, h);
    bg.addColorStop(0, 'rgba(11, 17, 44, 0.96)');
    bg.addColorStop(1, 'rgba(5, 9, 22, 0.99)');
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, w, h);

    drawVerticalGrid(w, h);

    var waveA = new Float32Array(BINS);
    var waveB = new Float32Array(BINS);
    var waveC = new Float32Array(BINS);
    var waveD = new Float32Array(BINS);

    var smoothAmt = currentValues.smooth / 100;
    var focusAmt = currentValues.focus / 100;
    var airAmt = currentValues.air_preserve / 100;
    var bodyAmt = currentValues.body / 100;

    for (var i = 0; i < BINS; i++) {
      var t = i / (BINS - 1);
      var sweep = Math.sin(t * 8.8 + phase * 4.2) * (0.055 + smoothAmt * 0.05);
      var dip = Math.sin(t * 12.5 - phase * 2.3) * (0.028 + focusAmt * 0.03);
      var lift = Math.sin(t * 5.7 + phase * 1.7) * (0.038 + bodyAmt * 0.03);

      var dspShape = smoothProblem[i] * 0.3 + smoothInput[i] * 0.2 - smoothReduction[i] * 0.15;
      var crest = 0.46 + (0.08 * (0.3 - t));
      var highSpark = Math.max(0, t - 0.64) * (0.16 + airAmt * 0.16);

      var yBase = (crest + sweep + dip + lift - dspShape * 0.18 - highSpark) * h;

      waveA[i] = yBase;
      waveB[i] = yBase + h * (0.105 + 0.04 * Math.sin(phase * 2 + t * 7.5));
      waveC[i] = yBase + h * (0.18 + 0.05 * Math.sin(phase * 1.35 + t * 6.8));
      waveD[i] = yBase + h * (0.25 + 0.05 * Math.cos(phase * 1.8 + t * 9.2));
    }

    drawBand(
      waveD,
      'rgba(105, 72, 198, 0.34)',
      'rgba(55, 34, 112, 0.02)',
      'rgba(144, 105, 222, 0.35)',
      10,
      0.58
    );

    drawBand(
      waveC,
      'rgba(166, 114, 243, 0.36)',
      'rgba(74, 45, 138, 0.03)',
      'rgba(196, 143, 255, 0.52)',
      14,
      0.72
    );

    drawBand(
      waveB,
      'rgba(236, 173, 255, 0.36)',
      'rgba(95, 62, 148, 0.03)',
      'rgba(242, 194, 255, 0.62)',
      16,
      0.82
    );

    drawBand(
      waveA,
      'rgba(255, 214, 146, 0.32)',
      'rgba(112, 76, 34, 0.02)',
      'rgba(255, 224, 156, 0.94)',
      24,
      0.96
    );

    var bloom = ctx.createRadialGradient(w * 0.67, h * 0.53, 12, w * 0.67, h * 0.53, h * 0.54);
    bloom.addColorStop(0, 'rgba(255, 204, 130, 0.3)');
    bloom.addColorStop(0.6, 'rgba(214, 149, 255, 0.12)');
    bloom.addColorStop(1, 'rgba(112, 78, 180, 0)');
    ctx.fillStyle = bloom;
    ctx.fillRect(0, 0, w, h);

    drawParticles(w, h);
  }

  function startRenderLoop() {
    function frame() {
      drawSilkField();
      frameHandle = window.requestAnimationFrame(frame);
    }

    if (frameHandle) window.cancelAnimationFrame(frameHandle);
    frame();
  }

  function boot() {
    juceAvailable = !!(
      window.__JUCE__ &&
      window.__JUCE__.backend &&
      typeof window.__JUCE__.backend.emitEvent === 'function'
    );

    initCanvas();
    initKnobs();
    initReadouts();
    initPresetButtons();
    initMagicToggle();
    startRenderLoop();

    applyPreset(activePreset, false);

    if (juceAvailable) {
      try {
        initParamStates();
      } catch (err) {
        juceAvailable = false;
        console.warn('Nova Silk UI: JUCE bridge unavailable, running preview mode.', err);
      }
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot, { once: true });
  } else {
    boot();
  }
}());
(function () {
  'use strict';

  // ── Constants ──────────────────────────────────────────────────────────────
  var BINS = 96;
  var KNOB_ARC_TOTAL = 113.1;   // px for 240° sweep on r=27 circle (circumference * 240/360)
  var KNOB_MIN_DEG   = -120;    // rotation at 0 %
  var KNOB_MAX_DEG   =  120;    // rotation at 100 %

  var PRESETS = {
    'Universal Smooth': { smooth: 42, focus: 60, air_preserve: 72, body: 62, output: 50, magic: true },
    'Vocal':            { smooth: 50, focus: 65, air_preserve: 70, body: 65, output: 50, magic: true },
    'Drums':            { smooth: 35, focus: 62, air_preserve: 55, body: 40, output: 50, magic: false },
    'Guitar':           { smooth: 40, focus: 58, air_preserve: 68, body: 70, output: 50, magic: true },
    'Master':           { smooth: 28, focus: 50, air_preserve: 75, body: 72, output: 50, magic: true }
  };

  var DISPLAY_PARAMS   = ['smooth', 'focus', 'air_preserve', 'body'];
  var ALL_PARAMS       = ['smooth', 'focus', 'air_preserve', 'body', 'output'];

  // ── State ──────────────────────────────────────────────────────────────────
  var currentValues    = { smooth: 42, focus: 60, air_preserve: 72, body: 62, output: 50 };
  var magicOn          = true;
  var paramStates      = {};
  var juceAvailable    = false;
  var activeDrag       = null;
  var activePreset     = 'Universal Smooth';

  // Display buffers — exponentially smoothed for "silk" feel
  var smoothedInput     = new Float32Array(BINS);
  var smoothedProblem   = new Float32Array(BINS);
  var smoothedReduction = new Float32Array(BINS);

  // Raw incoming from C++
  var rawInput     = new Float32Array(BINS);
  var rawProblem   = new Float32Array(BINS);
  var rawReduction = new Float32Array(BINS);

  var animFrameId = null;
  var canvas = null;
  var ctx    = null;

  // ── Helpers ────────────────────────────────────────────────────────────────
  function clamp(v, lo, hi) {
    var mn = (typeof lo === 'number') ? lo : 0;
    var mx = (typeof hi === 'number') ? hi : 100;
    return Math.min(mx, Math.max(mn, v));
  }

  function lerp(a, b, t) { return a + (b - a) * t; }

  function getAttr(el, name) {
    if (!el) return '';
    if (el.dataset && Object.prototype.hasOwnProperty.call(el.dataset, name)) return el.dataset[name];
    return el.getAttribute('data-' + name) || '';
  }

  function moveCaretToEnd(el) {
    if (!window.getSelection || !document.createRange) return;
    var sel = window.getSelection();
    var r   = document.createRange();
    r.selectNodeContents(el);
    r.collapse(false);
    sel.removeAllRanges();
    sel.addRange(r);
  }

  // ── Value display formatters ───────────────────────────────────────────────
  function formatKnob(param, percent) {
    if (param === 'output') {
      var db = -12 + (percent / 100) * 24;
      return (db >= 0 ? '+' : '') + db.toFixed(1) + ' dB';
    }
    // Smooth / Focus / Air / Body: display as 0–10 scale
    return (percent / 10).toFixed(1);
  }

  // ── Knob visual update ─────────────────────────────────────────────────────
  function updateKnobVisual(param, percent) {
    var arc = document.getElementById(param + '-arc');
    var ind = document.getElementById(param + '-ind');
    var val = document.getElementById(param + '-value');

    var frac = clamp(percent, 0, 100) / 100;

    // Arc: stroke-dasharray="filled gap" — filled portion of KNOB_ARC_TOTAL
    if (arc) {
      var filled = frac * KNOB_ARC_TOTAL;
      arc.setAttribute('stroke-dasharray', filled + ' ' + (KNOB_ARC_TOTAL - filled + 1));
    }

    // Indicator dot rotation: -120° at 0%, +120° at 100%
    if (ind) {
      var deg = KNOB_MIN_DEG + frac * (KNOB_MAX_DEG - KNOB_MIN_DEG);
      ind.style.transform = 'rotate(' + deg + 'deg)';
    }

    // Value readout (skip when focused / being edited)
    if (val && document.activeElement !== val) {
      val.textContent = formatKnob(param, percent);
    }
  }

  // ── Parameter value update (visual + optional JUCE push) ──────────────────
  function setParamPercent(param, percent, opts) {
    var safe = clamp(percent, 0, 100);
    var settings = opts || {};

    currentValues[param] = safe;
    updateKnobVisual(param, safe);

    if (settings.push) {
      var state = paramStates[param];
      if (state) {
        try {
          if (settings.gesture) state.sliderDragStarted();
          state.setNormalisedValue(safe / 100);
          if (settings.gesture) state.sliderDragEnded();
        } catch (_) {}
      }
    }
  }

  function setMagic(on, push) {
    magicOn = on;
    var ctrl = document.getElementById('magic-control');
    if (ctrl) ctrl.classList.toggle('on', on);

    if (push) {
      var state = paramStates['magic'];
      if (state) {
        try {
          state.setNormalisedValue(on ? 1 : 0);
        } catch (_) {}
      }
    }
  }

  // ── Preset application ─────────────────────────────────────────────────────
  function applyPreset(name, push) {
    var p = PRESETS[name];
    if (!p) return;

    activePreset = name;

    // Highlight active preset button
    var btns = document.querySelectorAll('.preset-btn');
    Array.prototype.forEach.call(btns, function (b) {
      b.classList.toggle('active', getAttr(b, 'preset') === name);
    });

    var shouldPush = (push !== false);
    Array.prototype.forEach.call(ALL_PARAMS, function (param) {
      setParamPercent(param, p[param], { push: shouldPush, gesture: false });
    });
    setMagic(p.magic, shouldPush);
  }

  // ── JUCE parameter state factory ───────────────────────────────────────────
  function createListenerList() {
    return {
      _list: [],
      add: function (fn) { this._list.push(fn); },
      fire: function (data) { for (var i = 0; i < this._list.length; i++) this._list[i](data); }
    };
  }

  function createSliderState(name) {
    if (!juceAvailable || !window.__JUCE__ || !window.__JUCE__.backend) return null;

    var backend = window.__JUCE__.backend;
    var id      = '__juce__slider' + name;

    var state = {
      name:               name,
      scaledValue:        0,
      properties:         { start: 0, end: 1, skew: 1, interval: 0, numSteps: 100 },
      valueChangedEvent:  createListenerList(),

      getNormalisedValue: function () {
        var r = this.properties.end - this.properties.start;
        if (r === 0) return 0;
        return clamp((this.scaledValue - this.properties.start) / r, 0, 1);
      },

      setNormalisedValue: function (n) {
        var normed   = clamp(n, 0, 1);
        var start    = this.properties.start;
        var end      = this.properties.end;
        var interval = this.properties.interval;
        var scaled   = start + normed * (end - start);
        if (interval > 0) scaled = Math.round(scaled / interval) * interval;
        this.scaledValue = scaled;
        backend.emitEvent(id, { eventType: 'valueChanged', value: scaled });
      },

      sliderDragStarted: function () { backend.emitEvent(id, { eventType: 'sliderDragStarted' }); },
      sliderDragEnded:   function () { backend.emitEvent(id, { eventType: 'sliderDragEnded'   }); }
    };

    backend.addEventListener(id, function (ev) {
      if (!ev) return;
      if (ev.eventType === 'valueChanged' && typeof ev.value === 'number') {
        state.scaledValue = ev.value;
        state.valueChangedEvent.fire(ev);
      }
      if (ev.eventType === 'propertiesChanged') {
        for (var k in ev) {
          if (Object.prototype.hasOwnProperty.call(ev, k) && k !== 'eventType') {
            state.properties[k] = ev[k];
          }
        }
      }
    });

    backend.emitEvent(id, { eventType: 'requestInitialUpdate' });
    return state;
  }

  function initParamStates() {
    if (!juceAvailable) return;

    Array.prototype.forEach.call(ALL_PARAMS, function (p) {
      paramStates[p] = createSliderState(p);
      if (paramStates[p]) {
        (function (param, st) {
          st.valueChangedEvent.add(function () {
            setParamPercent(param, st.getNormalisedValue() * 100, { push: false });
          });
        }(p, paramStates[p]));
      }
    });

    // Magic boolean relay
    paramStates['magic'] = createSliderState('magic');
    if (paramStates['magic']) {
      paramStates['magic'].valueChangedEvent.add(function () {
        setMagic(paramStates['magic'].getNormalisedValue() > 0.5, false);
      });
    }

    // Sync initial visual from JUCE after a short delay
    window.setTimeout(function () {
      Array.prototype.forEach.call(ALL_PARAMS, function (p) {
        if (paramStates[p]) setParamPercent(p, paramStates[p].getNormalisedValue() * 100, { push: false });
      });
      if (paramStates['magic']) setMagic(paramStates['magic'].getNormalisedValue() > 0.5, false);
    }, 80);
  }

  // ── Knob drag ──────────────────────────────────────────────────────────────
  function getClientY(ev) {
    if (ev && ev.touches && ev.touches.length)       return ev.touches[0].clientY;
    if (ev && ev.changedTouches && ev.changedTouches.length) return ev.changedTouches[0].clientY;
    return (ev && typeof ev.clientY === 'number') ? ev.clientY : 0;
  }

  function startDrag(param, knob, ev, defaultPct) {
    if (ev && typeof ev.button === 'number' && ev.button !== 0) return;
    if (ev && ev.preventDefault) ev.preventDefault();
    knob.classList.add('dragging');

    activeDrag = {
      param:     param,
      knob:      knob,
      pid:       (ev && ev.pointerId != null) ? ev.pointerId : null,
      startY:    getClientY(ev),
      startVal:  typeof currentValues[param] === 'number' ? currentValues[param] : defaultPct
    };

    if (typeof knob.setPointerCapture === 'function' && activeDrag.pid != null) {
      try { knob.setPointerCapture(activeDrag.pid); } catch (_) {}
    }

    var st = paramStates[param];
    if (st) try { st.sliderDragStarted(); } catch (_) {}
  }

  function moveDrag(ev) {
    if (!activeDrag) return;
    if (activeDrag.pid != null && ev && ev.pointerId != null && ev.pointerId !== activeDrag.pid) return;
    if (ev && ev.preventDefault) ev.preventDefault();

    var delta = activeDrag.startY - getClientY(ev);
    setParamPercent(activeDrag.param, clamp(activeDrag.startVal + delta * 0.45, 0, 100), { push: true });
  }

  function endDrag(ev) {
    if (!activeDrag) return;
    if (activeDrag.pid != null && ev && ev.pointerId != null && ev.pointerId !== activeDrag.pid) return;

    var st = paramStates[activeDrag.param];
    if (st) try { st.sliderDragEnded(); } catch (_) {}

    if (typeof activeDrag.knob.releasePointerCapture === 'function' && activeDrag.pid != null) {
      try { activeDrag.knob.releasePointerCapture(activeDrag.pid); } catch (_) {}
    }
    activeDrag.knob.classList.remove('dragging');
    activeDrag = null;
  }

  // ── Knob scroll ────────────────────────────────────────────────────────────
  function handleScroll(param, ev) {
    if (ev && ev.preventDefault) ev.preventDefault();
    var delta = ev ? (ev.deltaY || 0) : 0;
    var step  = (ev && ev.shiftKey) ? 0.1 : 1;
    setParamPercent(param, clamp(currentValues[param] - Math.sign(delta) * step, 0, 100), { push: true, gesture: true });
  }

  // ── Canvas setup ───────────────────────────────────────────────────────────
  function initCanvas() {
    canvas = document.getElementById('silk-canvas');
    if (!canvas) return;

    ctx = canvas.getContext('2d');
    var frame = canvas.parentElement;

    function resize() {
      canvas.width  = frame.clientWidth;
      canvas.height = frame.clientHeight;
    }
    resize();
    window.addEventListener('resize', resize);
  }

  // ── Spectrum data ingestion (called from C++ timerCallback) ───────────────
  window.updateSilkSpectrum = function (inputArr, problemArr, reductionArr) {
    if (!inputArr || !problemArr || !reductionArr) return;

    // Time-smoothing rates (controls the "silk" / melting feel)
    // Faster for input (reacts quickly), slower for glow/reduction (floats down)
    var iAlpha = 0.78;
    var pAlpha = magicOn ? 0.88 : 0.82;  // glow holds longer with Magic ON
    var rAlpha = magicOn ? 0.92 : 0.86;  // reduction dip releases even slower

    for (var i = 0; i < BINS; i++) {
      rawInput[i]     = (typeof inputArr[i]     === 'number') ? inputArr[i]     : 0;
      rawProblem[i]   = (typeof problemArr[i]   === 'number') ? problemArr[i]   : 0;
      rawReduction[i] = (typeof reductionArr[i] === 'number') ? reductionArr[i] : 0;

      // Attack: fast rise; Release: controlled by alpha (higher = slower decay)
      var si = rawInput[i];
      smoothedInput[i] = si > smoothedInput[i]
        ? lerp(smoothedInput[i], si, 1 - iAlpha)   // rise
        : lerp(smoothedInput[i], si, 1 - iAlpha * 1.4); // faster fall for input line

      var sp = rawProblem[i];
      smoothedProblem[i] = sp > smoothedProblem[i]
        ? lerp(smoothedProblem[i], sp, 0.40)        // fast glow attack
        : lerp(smoothedProblem[i], sp, 1 - pAlpha); // slow glow release (melting)

      var sr = rawReduction[i];
      smoothedReduction[i] = sr > smoothedReduction[i]
        ? lerp(smoothedReduction[i], sr, 0.35)       // dip appears quickly
        : lerp(smoothedReduction[i], sr, 1 - rAlpha); // dip fades slowly (elastic)
    }
  };

  // ── Spectrum draw ──────────────────────────────────────────────────────────
  function drawSpectrum() {
    if (!canvas || !ctx) return;

    var W = canvas.width;
    var H = canvas.height;

    ctx.clearRect(0, 0, W, H);

    // Subtle horizontal grid lines
    ctx.strokeStyle = 'rgba(184, 164, 216, 0.06)';
    ctx.lineWidth = 1;
    for (var g = 0.25; g < 1.0; g += 0.25) {
      var gy = H * g;
      ctx.beginPath();
      ctx.moveTo(0, gy);
      ctx.lineTo(W, gy);
      ctx.stroke();
    }

    var xStep = W / (BINS - 1);

    // ── Layer 3: Reduction dip shape (rendered below glow so glow overlaps) ──
    // Soft U-shaped violet dips — "where Silk is cutting"
    ctx.beginPath();
    for (var i = 0; i < BINS; i++) {
      var x = i * xStep;
      var dip = smoothedReduction[i];          // 0–1, 1 = max reduction
      var y   = H * (1 - dip * 0.72);         // max dip = 72% of canvas height
      if (i === 0) ctx.moveTo(x, H);
      ctx.lineTo(x, y);
    }
    ctx.lineTo((BINS - 1) * xStep, H);
    ctx.closePath();
    ctx.fillStyle = 'rgba(130, 100, 190, 0.12)';
    ctx.fill();

    // Reduction outline — smooth dark violet curve
    ctx.beginPath();
    for (var i2 = 0; i2 < BINS; i2++) {
      var x2  = i2 * xStep;
      var dip2 = smoothedReduction[i2];
      var y2   = H * (1 - dip2 * 0.72);
      if (i2 === 0) ctx.moveTo(x2, y2);
      else {
        var px = (i2 - 1) * xStep;
        var py = H * (1 - smoothedReduction[i2 - 1] * 0.72);
        var cpx = (px + x2) / 2;
        ctx.bezierCurveTo(cpx, py, cpx, y2, x2, y2);
      }
    }
    ctx.strokeStyle = 'rgba(150, 110, 210, 0.35)';
    ctx.lineWidth   = 1.5;
    ctx.stroke();

    // ── Layer 2: Silk activity glow (lavender fill) ───────────────────────────
    // This is the signature: where suppression is happening, a soft lavender glow appears
    ctx.beginPath();
    for (var i3 = 0; i3 < BINS; i3++) {
      var x3   = i3 * xStep;
      var glow = smoothedProblem[i3];          // 0–1
      // The glow fills upward from a "base" line at middle height
      var base = H * 0.70;                     // glow radiates from lower area
      var top3 = base - glow * H * 0.62;
      if (i3 === 0) ctx.moveTo(x3, base);
      ctx.lineTo(x3, top3);
    }
    // Trace back along bottom
    for (var i4 = BINS - 1; i4 >= 0; i4--) {
      ctx.lineTo(i4 * xStep, H * 0.70);
    }
    ctx.closePath();

    // Lavender gradient fill — bright at top, transparent at base
    var glowGrad = ctx.createLinearGradient(0, 0, 0, H);
    glowGrad.addColorStop(0.0, 'rgba(200, 175, 240, 0.55)');
    glowGrad.addColorStop(0.4, 'rgba(184, 160, 228, 0.30)');
    glowGrad.addColorStop(1.0, 'rgba(140, 120, 200, 0.00)');
    ctx.fillStyle = glowGrad;
    ctx.fill();

    // Soft top edge of glow (the "luminous rim")
    ctx.beginPath();
    for (var i5 = 0; i5 < BINS; i5++) {
      var x5   = i5 * xStep;
      var glow5 = smoothedProblem[i5];
      var base5 = H * 0.70;
      var top5  = base5 - glow5 * H * 0.62;
      if (i5 === 0) ctx.moveTo(x5, top5);
      else {
        var px5  = (i5 - 1) * xStep;
        var py5  = H * 0.70 - smoothedProblem[i5 - 1] * H * 0.62;
        var cpx5 = (px5 + x5) / 2;
        ctx.bezierCurveTo(cpx5, py5, cpx5, top5, x5, top5);
      }
    }
    ctx.strokeStyle = 'rgba(210, 188, 248, 0.55)';
    ctx.lineWidth   = 2.0;
    ctx.shadowColor = 'rgba(200, 175, 240, 0.60)';
    ctx.shadowBlur  = magicOn ? 10 : 6;
    ctx.stroke();
    ctx.shadowBlur = 0;

    // ── Layer 1: Input spectrum line (topmost, gray) ──────────────────────────
    ctx.beginPath();
    for (var i6 = 0; i6 < BINS; i6++) {
      var x6 = i6 * xStep;
      var y6 = H * (1 - smoothedInput[i6] * 0.88);
      if (i6 === 0) ctx.moveTo(x6, y6);
      else ctx.lineTo(x6, y6);
    }
    ctx.strokeStyle = 'rgba(192, 186, 210, 0.50)';
    ctx.lineWidth   = 1.2;
    ctx.stroke();
  }

  // ── Demo spectrum generator (for preview mode without JUCE) ────────────────
  var demoPhase = 0;
  function generateDemoSpectrum() {
    demoPhase += 0.008;
    
    for (var i = 0; i < BINS; i++) {
      var freqPos = i / BINS;  // 0 = 20Hz, 1 = 20kHz
      
      // Create a musical fundamental with harmonics
      var fundamental = Math.sin(demoPhase * 2.3 + freqPos * 3) * 0.4;
      var harmonic1 = Math.sin(demoPhase * 3.1 + freqPos * 6) * 0.25;
      var harmonic2 = Math.sin(demoPhase * 2.7 + freqPos * 9) * 0.15;
      var noise = (Math.random() - 0.5) * 0.08;
      
      // Base spectrum (input)
      rawInput[i] = Math.max(0, 0.35 + fundamental + harmonic1 + harmonic2 + noise) * 0.7;
      
      // Problem detection - show activity mid-high range
      var midHighFocus = freqPos > 0.35 && freqPos < 0.8;
      var problemAmount = midHighFocus ? Math.abs(Math.sin(demoPhase * 1.4 + freqPos * 12)) * 0.6 : 0;
      rawProblem[i] = problemAmount;
      
      // Reduction dip - smooth, peaky in mid-highs
      var band = Math.abs(Math.sin(demoPhase * 0.7 + freqPos * 8));
      var dip = midHighFocus ? band * 0.35 : band * 0.08;
      rawReduction[i] = dip;
    }
  }

  // ── Animation loop ─────────────────────────────────────────────────────────
  function startRenderLoop() {
    function frame() {
      // Generate demo data if not connected to JUCE
      if (!juceAvailable) {
        generateDemoSpectrum();
        updateSignalData(rawInput, rawProblem, rawReduction);
      }
      drawSpectrum();
      animFrameId = window.requestAnimationFrame(frame);
    }
    frame();
  }

  // ── Editable readout (inline text field on knob value) ────────────────────
  function initReadouts() {
    var readouts = document.querySelectorAll('.editable-value');
    Array.prototype.forEach.call(readouts, function (el) {
      var param = getAttr(el, 'param');

      el.addEventListener('focus', function () {
        // Show raw numeric for editing
        if (param === 'output') {
          var db = -12 + (currentValues[param] / 100) * 24;
          el.textContent = db.toFixed(1);
        } else {
          el.textContent = (currentValues[param] / 10).toFixed(1);
        }
        moveCaretToEnd(el);
      });

      el.addEventListener('input', function () {
        var cleaned = el.textContent.replace(/[^0-9.\-]/g, '');
        if (el.textContent !== cleaned) { el.textContent = cleaned; moveCaretToEnd(el); }
      });

      el.addEventListener('keydown', function (ev) {
        if (ev.key === 'Enter') { ev.preventDefault(); el.blur(); }
        if (ev.key === 'Escape') { ev.preventDefault(); updateKnobVisual(param, currentValues[param]); el.blur(); }
      });

      el.addEventListener('blur', function () {
        var raw  = parseFloat(el.textContent);
        var pct;
        if (!isNaN(raw)) {
          if (param === 'output') {
            // user typed dB value
            pct = clamp(((raw + 12) / 24) * 100, 0, 100);
          } else {
            // user typed 0–10 scale
            pct = clamp(raw * 10, 0, 100);
          }
        } else {
          pct = currentValues[param];
        }
        setParamPercent(param, pct, { push: true, gesture: false });
      });
    });
  }

  // ── Knob init ──────────────────────────────────────────────────────────────
  function initKnobs() {
    var knobs = document.querySelectorAll('.macro-knob');
    Array.prototype.forEach.call(knobs, function (knob) {
      var param      = getAttr(knob, 'param');
      var defaultPct = parseFloat(getAttr(knob, 'default') || '0');

      updateKnobVisual(param, currentValues[param] || defaultPct);

      knob.addEventListener('pointerdown', function (ev) { startDrag(param, knob, ev, defaultPct); });
      knob.addEventListener('mousedown',   function (ev) { startDrag(param, knob, ev, defaultPct); });
      knob.addEventListener('touchstart',  function (ev) { startDrag(param, knob, ev, defaultPct); }, { passive: false });

      knob.addEventListener('dblclick', function () {
        setParamPercent(param, defaultPct, { push: true, gesture: true });
      });

      knob.addEventListener('wheel', function (ev) { handleScroll(param, ev); }, { passive: false });
    });

    document.addEventListener('pointermove',  moveDrag);
    document.addEventListener('mousemove',    moveDrag);
    document.addEventListener('touchmove',    moveDrag, { passive: false });
    document.addEventListener('pointerup',    endDrag);
    document.addEventListener('mouseup',      endDrag);
    document.addEventListener('touchend',     endDrag);
    document.addEventListener('pointercancel',endDrag);
    document.addEventListener('touchcancel',  endDrag);
    document.addEventListener('mouseleave',   endDrag);
  }

  // ── Preset buttons ─────────────────────────────────────────────────────────
  function initPresets() {
    var btns = document.querySelectorAll('.preset-btn');
    Array.prototype.forEach.call(btns, function (btn) {
      btn.addEventListener('click', function (ev) {
        if (ev && ev.preventDefault) ev.preventDefault();
        applyPreset(getAttr(btn, 'preset'), true);
      });
    });
  }

  // ── Magic toggle ───────────────────────────────────────────────────────────
  function initMagic() {
    var ctrl = document.getElementById('magic-control');
    if (!ctrl) return;
    ctrl.addEventListener('click', function () {
      setMagic(!magicOn, true);
    });
  }

  // ── Boot ────────────────────────────────────────────────────────────────────
  function boot() {
    juceAvailable = !!(
      window.__JUCE__ &&
      window.__JUCE__.backend &&
      typeof window.__JUCE__.backend.emitEvent === 'function'
    );

    initCanvas();
    initKnobs();
    initReadouts();
    initPresets();
    initMagic();
    startRenderLoop();

    // Apply default preset visuals before JUCE sync
    applyPreset('Universal Smooth', false);

    if (juceAvailable) {
      try {
        initParamStates();
      } catch (err) {
        juceAvailable = false;
        console.warn('Nova Silk: JUCE sync unavailable, running in standalone preview mode.', err);
      }
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot, { once: true });
  } else {
    boot();
  }

}());
