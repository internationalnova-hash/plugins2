(function () {
  'use strict';

  var BINS = 96;
  var KNOB_ARC_TOTAL = 226;
  var KNOB_MIN_DEG = -120;
  var KNOB_MAX_DEG = 120;

  var PRESETS = {
    'Universal Smooth': { smooth: 42, focus: 60, air_preserve: 72, body: 62, output: 50, mix: 100 },
    'Vocal':            { smooth: 50, focus: 65, air_preserve: 70, body: 65, output: 50, mix: 100 },
    'Drums':            { smooth: 35, focus: 62, air_preserve: 55, body: 40, output: 50, mix: 80  },
    'Guitar':           { smooth: 40, focus: 58, air_preserve: 68, body: 70, output: 50, mix: 90  },
    'Master':           { smooth: 28, focus: 50, air_preserve: 75, body: 72, output: 50, mix: 70  }
  };

  var ALL_PARAMS = ['smooth', 'focus', 'air_preserve', 'body', 'output', 'mix'];

  var currentValues = { smooth: 42, focus: 60, air_preserve: 72, body: 62, output: 50, mix: 100 };
  var paramStates = {};
  var juceAvailable = false;
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
  var hotspotPhase = 0;

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
    if (param === 'mix') {
      return Math.round(percent) + '%';
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
      if (param === 'smooth') filled = Math.max(12, filled);
      arc.setAttribute('stroke-dasharray', filled + ' ' + (KNOB_ARC_TOTAL - filled + 1));
      if (param === 'smooth') {
        arc.style.opacity = (0.68 + frac * 0.24).toFixed(3);
        arc.style.filter = 'drop-shadow(0 0 ' + (8 + frac * 4).toFixed(2) + 'px rgba(177,126,255,0.68)) drop-shadow(0 0 ' + (10 + frac * 4).toFixed(2) + 'px rgba(242,200,125,0.38))';
      }
    }

    if (ind) {
      var deg = KNOB_MIN_DEG + frac * (KNOB_MAX_DEG - KNOB_MIN_DEG);
      ind.style.transform = 'rotate(' + deg + 'deg)';
      if (param === 'smooth') ind.style.opacity = (0.88 + frac * 0.12).toFixed(3);
    }

    if (val && document.activeElement !== val) {
      val.textContent = formatKnob(param, percent);
    }
  }

  function setParamPercent(param, percent, opts) {
    var safe = clamp(percent, 0, 100);
    var settings = opts || {};
    currentValues[param] = safe;

    if (param === 'mix') {
      updateMixSlider(safe);
    } else {
      updateKnobVisual(param, safe);
    }

    if (!settings.push) return;

    var state = paramStates[param];
    if (!state) return;

    try {
      if (settings.gesture) state.sliderDragStarted();
      state.setNormalisedValue(safe / 100);
      if (settings.gesture) state.sliderDragEnded();
    } catch (_) {}
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
      if (preset[p] !== undefined) setParamPercent(p, preset[p], { push: shouldPush, gesture: false });
    });
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

    window.setTimeout(function () {
      Array.prototype.forEach.call(ALL_PARAMS, function (p) {
        if (paramStates[p]) {
          setParamPercent(p, paramStates[p].getNormalisedValue() * 100, { push: false });
        }
      });
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

  function updateMixSlider(percent) {
    var fill  = document.getElementById('mix-fill');
    var thumb = document.getElementById('mix-thumb');
    var label = document.getElementById('mix-value');
    var frac  = clamp(percent, 0, 100) / 100;
    if (fill)  fill.style.width = (frac * 100) + '%';
    if (thumb) thumb.style.left = (frac * 100) + '%';
    if (label) label.textContent = Math.round(percent) + '%';
  }

  function initMixSlider() {
    var track = document.getElementById('mix-slider');
    if (!track) return;

    function pctFromEvent(ev) {
      var rect = track.getBoundingClientRect();
      var clientX = ev.touches ? ev.touches[0].clientX : ev.clientX;
      return clamp(((clientX - rect.left) / rect.width) * 100, 0, 100);
    }

    var dragging = false;

    function onMove(ev) {
      if (!dragging) return;
      ev.preventDefault();
      var pct = pctFromEvent(ev);
      setParamPercent('mix', pct, { push: true, gesture: true });
    }

    track.addEventListener('mousedown', function (ev) {
      dragging = true;
      setParamPercent('mix', pctFromEvent(ev), { push: true, gesture: true });
    });
    track.addEventListener('touchstart', function (ev) {
      dragging = true;
      setParamPercent('mix', pctFromEvent(ev), { push: true, gesture: true });
    }, { passive: true });

    document.addEventListener('mousemove', onMove);
    document.addEventListener('touchmove', onMove, { passive: false });
    document.addEventListener('mouseup',  function () { dragging = false; });
    document.addEventListener('touchend', function () { dragging = false; });

    track.addEventListener('keydown', function (ev) {
      var delta = ev.shiftKey ? 10 : 1;
      if (ev.key === 'ArrowRight' || ev.key === 'ArrowUp') {
        setParamPercent('mix', clamp(currentValues.mix + delta, 0, 100), { push: true });
      } else if (ev.key === 'ArrowLeft' || ev.key === 'ArrowDown') {
        setParamPercent('mix', clamp(currentValues.mix - delta, 0, 100), { push: true });
      }
    });

    updateMixSlider(currentValues.mix);
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
        : lerp(smoothProblem[i], rawProblem[i], 0.042);

      smoothReduction[i] = rawReduction[i] > smoothReduction[i]
        ? lerp(smoothReduction[i], rawReduction[i], 0.14)
        : lerp(smoothReduction[i], rawReduction[i], 0.03);
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

  function makeParticle() {
    var clusters = [
      { x: 0.68, y: 0.31 },
      { x: 0.77, y: 0.25 },
      { x: 0.85, y: 0.33 }
    ];
    var isStray = Math.random() < 0.22;
    var c = clusters[Math.floor(Math.random() * clusters.length)];
    var depth = Math.random();
    var tierRoll = Math.random();
    var tier = tierRoll < 0.70 ? 'small' : (tierRoll < 0.95 ? 'medium' : 'large');

    var baseX = isStray ? (0.64 + Math.random() * 0.28) : (c.x + (Math.random() - 0.5) * 0.095);
    var baseY = isStray ? (0.10 + Math.random() * 0.48) : (c.y + (Math.random() - 0.5) * 0.12);

    var size;
    var blur;
    if (tier === 'small') {
      size = 0.26 + Math.random() * 0.85;
      blur = depth > 0.72 ? (1.1 + Math.random() * 1.9) : (3.8 + Math.random() * 5.3);
    } else if (tier === 'medium') {
      size = 0.9 + Math.random() * 1.35;
      blur = depth > 0.72 ? (1.4 + Math.random() * 2.2) : (4.8 + Math.random() * 6.0);
    } else {
      size = 1.9 + Math.random() * 1.6;
      blur = depth > 0.72 ? (2.0 + Math.random() * 2.8) : (5.2 + Math.random() * 6.8);
    }

    return {
      x: clamp(baseX, 0.62, 0.93),
      y: clamp(baseY, 0.06, 0.70),
      vx: 0.00006 + Math.random() * 0.00024,
      vy: -0.00030 + Math.random() * 0.00072,
      life: Math.random(),
      size: size,
      blur: blur,
      tier: tier,
      depth: depth,
      warm: Math.random() > 0.42
    };
  }

  function ensureParticles() {
    if (particles.length >= 150) return;
    while (particles.length < 150) particles.push(makeParticle());
  }

  function drawParticles(w, h) {
    ensureParticles();

    ctx.save();
    for (var i = 0; i < particles.length; i++) {
      var p = particles[i];
      p.life += 0.006;
      if (p.life > 1) {
        particles[i] = makeParticle();
        p = particles[i];
      }

      p.x += p.vx + 0.00003 * Math.sin(phase * 0.9 + i * 0.17);
      p.y += p.vy;

      if (p.x > 0.94 || p.x < 0.60 || p.y < 0.02) {
        
        particles[i] = makeParticle();
        particles[i].x = 0.63 + Math.random() * 0.06;
        p = particles[i];
      }

      var px = p.x * w;
      var py = p.y * h;
      var flicker = 0.35 + 0.65 * Math.sin((phase * 0.6 + i) * 0.35) * 0.5 + 0.5;
      var depthGain = 0.55 + p.depth * 0.55;
      var alpha = (0.13 + 0.30 * (1 - p.life)) * flicker * depthGain;
      var tierFillGain = p.tier === 'small' ? 0.82 : (p.tier === 'medium' ? 0.94 : 1.08);
      var tierGlowGain = p.tier === 'small' ? 0.78 : (p.tier === 'medium' ? 0.96 : 1.14);

      ctx.beginPath();
      ctx.arc(px, py, p.size, 0, Math.PI * 2);
      ctx.fillStyle = p.warm
        ? 'rgba(247, 201, 126, ' + (alpha * 0.82 * tierFillGain).toFixed(3) + ')'
        : 'rgba(200, 150, 255, ' + (alpha * 0.70 * tierFillGain).toFixed(3) + ')';
      ctx.shadowColor = p.warm
        ? 'rgba(248, 204, 132, ' + ((0.24 + p.depth * 0.26) * tierGlowGain).toFixed(3) + ')'
        : 'rgba(188, 133, 255, ' + ((0.22 + p.depth * 0.24) * tierGlowGain).toFixed(3) + ')';
      ctx.shadowBlur = p.blur;
      ctx.fill();
    }
    ctx.restore();
  }

  function initParticles() {
    particles = [];
    ensureParticles();
  }

  function updateParticles(energyAmount) {
    // Slightly increase sparkle activity as high-band energy rises.
    var target = 104 + Math.round(clamp(energyAmount, 0, 1) * 62);
    if (particles.length < target) {
      while (particles.length < target) {
        particles.push(makeParticle());
      }
    } else if (particles.length > target) {
      particles.length = target;
    }
  }

  function generateDemoSpectrum() {
    phase += 0.006;
    for (var i = 0; i < BINS; i++) {
      var t = i / (BINS - 1);
      var base = 0.32 + 0.18 * Math.sin(phase * 1.7 + t * 8.4);
      var shimmer = 0.14 * Math.sin(phase * 2.6 + t * 13.1);
      var sparkle = Math.max(0, t - 0.62) * (0.22 + 0.16 * Math.sin(phase * 1.1));

      rawInput[i] = clamp(base + shimmer, 0, 1);
      rawProblem[i] = clamp(0.22 + 0.26 * Math.sin(phase * 1.4 + t * 10.5) + sparkle, 0, 1);
      rawReduction[i] = clamp(0.10 + 0.24 * Math.sin(phase * 1.05 + t * 7.2) + sparkle * 0.5, 0, 1);
    }
  }

  function drawSuppressionOverlay() {
    if (!canvas || !ctx) return;

    var w = canvas.width;
    var h = canvas.height;
    var maxWaveformHeight = h * 0.40;  // Max height from bottom

    // ── All 5 knobs drive the visualization ──────────────────────────────
    var focusAmt   = currentValues.focus        / 100;  // Where suppression targets (L↔R sweep)
    var bodyAmt    = currentValues.body         / 100;  // Width of suppression band (narrow↔wide)
    var smoothAmt  = currentValues.smooth       / 100;  // Character: jittery→silky
    var airAmt     = currentValues.air_preserve / 100;  // High-freq preservation (right bars tall/short)
    var outputAmt  = currentValues.output       / 100;  // Overall bar scale + brightness

    var focusWidth = 0.06 + bodyAmt * 0.28;  // Body → how wide the dip zone is

    // Output: 0%→very dim/low, 50%→normal, 100%→bright/tall
    var outputGain = 0.18 + outputAmt * 1.64;

    ctx.save();

    var barWidth = w / BINS;

    for (var i = 0; i < BINS; i++) {
      var x = (i / (BINS - 1)) * w;
      var t = i / (BINS - 1);
      var inputLevel = smoothInput[i] || 0;
      var detectedReduction = smoothReduction[i] || 0;

      // BODY: Gaussian band — controls spread/bandwidth of the suppression zone.
      // Narrow body = tight notch; wide body = broad valley.
      var dist = Math.abs(t - focusAmt);
      var focusWeight = Math.exp(-(dist * dist) / (2 * focusWidth * focusWidth));

      // AIR PRESERVE: shield high-freq bins from suppression
      var airShield = Math.max(0, (t - 0.52) / 0.48) * airAmt * 0.92;

      // SMOOTH + BODY work in tandem:
      //   Smooth = depth  (how far bars are pushed down)
      //   Body   = spread (how many bars are affected, via focusWeight)
      //
      // Suppression is ZERO outside the Body band — bars outside stay tall,
      // making the shape of the dip clearly read the Body setting.
      // The detector data adds extra dip when the plugin is actually processing.
      var smoothDepth     = smoothAmt * 0.82;                                    // 0→no dip, 1→full dip
      var detectorBoost   = detectedReduction * smoothAmt * (0.4 + 0.6 * bodyAmt); // real signal enhances it

      // focusWeight confines everything to the Body band — outside the band, focusWeight≈0 → no suppression
      var suppressionStrength = clamp(
        (smoothDepth + detectorBoost) * focusWeight * (1 - airShield),
        0,
        0.96
      );

      var postLevel = Math.max(0, inputLevel * (1 - suppressionStrength));

      // OUTPUT: scale bar height + opacity
      var barHeight = postLevel * maxWaveformHeight * outputGain;
      var barY = h - barHeight;

      // Bar color: focused zone → more golden tint; air zone → more cyan/bright tint; default → purple
      var inFocusZone     = focusWeight > 0.35;
      var inAirZone       = t > 0.58 && airAmt > 0.2;
      var outputBrightness = 0.55 + outputAmt * 0.55;  // Output → bar alpha

      var barGradient = ctx.createLinearGradient(x, barY, x, h);
      if (inFocusZone && !inAirZone) {
        // Focus zone: golden-purple gradient
        barGradient.addColorStop(0, 'rgba(244, 186, 100, ' + (0.45 * outputBrightness).toFixed(2) + ')');
        barGradient.addColorStop(0.5, 'rgba(200, 130, 255, ' + (0.32 * outputBrightness).toFixed(2) + ')');
        barGradient.addColorStop(1, 'rgba(140, 80, 220, ' + (0.18 * outputBrightness).toFixed(2) + ')');
      } else if (inAirZone) {
        // Air zone: bright cyan-white, preserved presence
        barGradient.addColorStop(0, 'rgba(190, 240, 255, ' + (0.50 * outputBrightness).toFixed(2) + ')');
        barGradient.addColorStop(0.5, 'rgba(150, 190, 255, ' + (0.32 * outputBrightness).toFixed(2) + ')');
        barGradient.addColorStop(1, 'rgba(120, 140, 220, ' + (0.16 * outputBrightness).toFixed(2) + ')');
      } else {
        // Default: purple
        barGradient.addColorStop(0, 'rgba(180, 120, 255, ' + (0.34 * outputBrightness).toFixed(2) + ')');
        barGradient.addColorStop(0.5, 'rgba(160, 100, 240, ' + (0.26 * outputBrightness).toFixed(2) + ')');
        barGradient.addColorStop(1, 'rgba(140, 80, 220, ' + (0.16 * outputBrightness).toFixed(2) + ')');
      }

      ctx.shadowColor = inAirZone ? 'rgba(180, 220, 255, 0.2)' : 'rgba(200, 140, 255, 0.16)';
      ctx.shadowBlur = 3;
      ctx.fillStyle = barGradient;
      ctx.fillRect(x, barY, barWidth, barHeight);
    }

    ctx.shadowBlur = 0;

    // ── Focus + Body zone highlight ──────────────────────────────────────
    // Width = Body (spread); Intensity = Smooth (depth).
    // This makes the highlighted band directly mirror where/how much suppression is happening.
    var focusX = focusAmt * w;
    var regionWidthPx = focusWidth * w;
    var zoneOpacity = 0.06 + smoothAmt * 0.22 + bodyAmt * 0.10;  // brighter as smooth/body increase
    var region = ctx.createRadialGradient(focusX, h - maxWaveformHeight * 0.65, 2, focusX, h - maxWaveformHeight * 0.65, regionWidthPx * 1.15);
    region.addColorStop(0, 'rgba(255, 196, 110, ' + zoneOpacity.toFixed(2) + ')');
    region.addColorStop(0.5, 'rgba(244, 160, 80, ' + (zoneOpacity * 0.38).toFixed(2) + ')');
    region.addColorStop(1, 'rgba(244, 186, 120, 0)');
    ctx.fillStyle = region;
    ctx.fillRect(focusX - regionWidthPx, h - maxWaveformHeight, regionWidthPx * 2, maxWaveformHeight);

    // ── Air preserve zone top-edge shimmer ── (visible when air is high)
    if (airAmt > 0.1) {
      var airOpacity = airAmt * 0.28;
      var airX = w * 0.55;
      var airGrd = ctx.createLinearGradient(airX, 0, w, 0);
      airGrd.addColorStop(0, 'rgba(160, 220, 255, 0)');
      airGrd.addColorStop(0.3, 'rgba(180, 230, 255, ' + (airOpacity * 0.35).toFixed(2) + ')');
      airGrd.addColorStop(1, 'rgba(200, 240, 255, ' + airOpacity.toFixed(2) + ')');
      ctx.fillStyle = airGrd;
      ctx.fillRect(airX, h - maxWaveformHeight, w - airX, maxWaveformHeight);
    }

    ctx.restore();
  }

  function drawSilkField() {
    if (!canvas || !ctx) return;

    var w = canvas.width;
    var h = canvas.height;

    phase += 0.0045;
    hotspotPhase += 0.0026;

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
      var micro = (Math.sin(t * 57.0 + phase * 11.0) + Math.sin(t * 91.0 - phase * 7.0)) * 0.0018;
      var highNoise = Math.max(0, t - 0.72) * (0.0025 * Math.sin(t * 133.0 + phase * 17.0));

      var dspShape = smoothProblem[i] * 0.3 + smoothInput[i] * 0.2 - smoothReduction[i] * 0.15;
      var crest = 0.46 + (0.08 * (0.3 - t));
      var highSpark = Math.max(0, t - 0.64) * (0.16 + airAmt * 0.16);

      var yBase = (crest + sweep + dip + lift + micro + highNoise - dspShape * 0.18 - highSpark) * h;
      var driftA = 0.24 * Math.sin(hotspotPhase * 0.58);
      var driftB = 0.31 * Math.sin(hotspotPhase * 0.58 + 0.9);
      var driftC = 0.38 * Math.sin(hotspotPhase * 0.58 + 1.8);
      var driftD = 0.46 * Math.sin(hotspotPhase * 0.58 + 2.7);

      waveA[i] = yBase + driftA;
      waveB[i] = yBase + h * (0.105 + 0.04 * Math.sin(phase * 2 + t * 7.5) + micro * 1.1) + driftB;
      waveC[i] = yBase + h * (0.18 + 0.05 * Math.sin(phase * 1.35 + t * 6.8) + micro * 1.8) + driftC;
      waveD[i] = yBase + h * (0.25 + 0.05 * Math.cos(phase * 1.8 + t * 9.2) + micro * 2.2) + driftD;
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
      24 + (0.6 + 0.4 * Math.sin(phase * 0.9)),
      0.96
    );

    ctx.save();
    ctx.beginPath();
    for (var hs = Math.floor(BINS * 0.70); hs < BINS; hs++) {
      var xh = (hs / (BINS - 1)) * w;
      var shimmerY = waveA[hs] - h * (0.004 + 0.002 * Math.sin(phase * 8.0 + hs));
      if (hs === Math.floor(BINS * 0.70)) ctx.moveTo(xh, shimmerY);
      else ctx.lineTo(xh, shimmerY);
    }
    ctx.strokeStyle = 'rgba(255, 233, 180, 0.18)';
    ctx.lineWidth = 1.15 + 0.15 * Math.sin(phase * 5.0);
    ctx.shadowColor = 'rgba(255, 222, 162, 0.22)';
    ctx.shadowBlur = 5;
    ctx.stroke();
    ctx.restore();


    // Calculate high-frequency energy for particle system
    var highFreqSum = 0;
    for (var hf = Math.floor(BINS * 0.62); hf < BINS; hf++) {
      highFreqSum += smoothProblem[hf] || 0;
    }
    var energyLevel = Math.min(1, highFreqSum / (BINS * 0.38));
    updateParticles(energyLevel);

    // Draw suppression overlay (always visible)
    drawSuppressionOverlay();

    // Render particles (high-frequency sparkles)
    drawParticles(w, h);
    var focusCenter = clamp(0.08 + 0.84 * focusAmt, 0.06, 0.94);
    var focusBin = Math.round(focusCenter * (BINS - 1));
    var focusWindow = 6 + Math.round(bodyAmt * 12);
    var sumAtFocus = 0;
    var countAtFocus = 0;
    for (var fb = Math.max(0, focusBin - focusWindow); fb <= Math.min(BINS - 1, focusBin + focusWindow); fb++) {
      sumAtFocus += smoothReduction[fb] || 0;
      countAtFocus++;
    }
    var focusReduction = countAtFocus > 0 ? sumAtFocus / countAtFocus : 0;

    var hotspotX = w * (focusCenter + 0.006 * Math.sin(hotspotPhase * 2.9));
    var hotspotY = h * (0.56 - focusReduction * 0.22 + 0.03 * (0.5 - bodyAmt) + 0.01 * Math.cos(hotspotPhase * 1.6));
    var hotspotPulse = 0.215 + 0.065 * (0.5 + 0.5 * Math.sin(hotspotPhase * 1.8)) + 0.011 * Math.sin(hotspotPhase * 6.1);
    var bloom = ctx.createRadialGradient(hotspotX, hotspotY, 12, hotspotX, hotspotY, h * 0.54);
    bloom.addColorStop(0, 'rgba(255, 204, 130, ' + hotspotPulse.toFixed(3) + ')');
    bloom.addColorStop(0.6, 'rgba(214, 149, 255, ' + (hotspotPulse * 0.45).toFixed(3) + ')');
    bloom.addColorStop(1, 'rgba(112, 78, 180, 0)');
    ctx.fillStyle = bloom;
    ctx.fillRect(0, 0, w, h);
  }

  function startRenderLoop() {
    function frame() {
      if (!juceAvailable) {
        generateDemoSpectrum();
        updateSignalData(rawInput, rawProblem, rawReduction);
      }
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
    initParticles();
    initKnobs();
    initReadouts();
    initPresetButtons();
    initMixSlider();
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
