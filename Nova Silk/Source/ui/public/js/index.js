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

  // ── Animation loop ─────────────────────────────────────────────────────────
  function startRenderLoop() {
    function frame() {
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
