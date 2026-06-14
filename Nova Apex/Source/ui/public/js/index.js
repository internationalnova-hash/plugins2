(function () {
  'use strict';

  // ── Parameter definitions ──────────────────────────────────────────────────
  var PARAMS = {
    gain:             { min: 0,     max: 24,   def: 0,    kind: 'db',      label: 'dB' },
    ceiling:          { min: -12,   max: 0,    def: -0.1, kind: 'dbSigned', label: 'dBFS' },
    outputGain:       { min: -12,   max: 0,    def: 0,    kind: 'dbSigned', label: 'dB' },
    drive:            { min: 0,     max: 10,   def: 0,    kind: 'float',   label: '' },
    style:            { min: 0,     max: 5,    def: 0,    kind: 'int',     label: '' },
    attack:           { min: 0.1,   max: 50,   def: 1.0,  kind: 'ms',      label: 'ms' },
    release:          { min: 10,    max: 1000, def: 100,  kind: 'ms',      label: 'ms' },
    lookahead:        { min: 0,     max: 20,   def: 5,    kind: 'ms',      label: 'ms' },
    oversample:       { min: 0,     max: 3,    def: 1,    kind: 'int',     label: '' },
    link:             { min: 0,     max: 1,    def: 1,    kind: 'bool',    label: '' },
    dither:           { min: 0,     max: 1,    def: 0,    kind: 'bool',    label: '' },
    transientPreserve:{ min: 0,     max: 1,    def: 0,    kind: 'float',   label: '' },
    lowEndProtect:    { min: 0,     max: 1,    def: 0,    kind: 'float',   label: '' },
    loudnessTarget:   { min: -23,   max: -6,   def: -14,  kind: 'dbSigned', label: 'LUFS' }
  };

  var OVERSAMPLE_LABELS = ['1x', '2x', '4x', '8x'];
  var STYLE_NAMES = ['Clean', 'Punch', 'Smooth', 'Loud', 'Analog', 'Master'];

  // ── State ──────────────────────────────────────────────────────────────────
  var values = {};
  var juceAvailable = false;

  // GR display history
  var GR_HISTORY = 512;
  var grHistory = new Float32Array(GR_HISTORY);
  var grWritePos = 0;

  // Telemetry from C++
  var telem = {
    inputL: 0, inputR: 0,
    outputL: 0, outputR: 0,
    gainReductionDb: 0,
    lufs: -70,
    truePeak: -70
  };

  // ── Init defaults ──────────────────────────────────────────────────────────
  Object.keys(PARAMS).forEach(function (id) {
    values[id] = PARAMS[id].def;
  });

  // ── JUCE integration ───────────────────────────────────────────────────────
  function initJUCE() {
    if (typeof window.__JUCE__ === 'undefined') return;
    juceAvailable = true;

    Object.keys(PARAMS).forEach(function (id) {
      var slider = window.__JUCE__.getSliderState(id);
      if (!slider) return;

      slider.valueChangedEvent.addListener(function () {
        var p = PARAMS[id];
        var norm = slider.getScaledValue();
        values[id] = p.min + norm * (p.max - p.min);
        onParamChanged(id);
      });
    });
  }

  function setParam(id, value) {
    var p = PARAMS[id];
    values[id] = Math.max(p.min, Math.min(p.max, value));
    onParamChanged(id);

    if (juceAvailable) {
      var slider = window.__JUCE__.getSliderState(id);
      if (slider) {
        var norm = (values[id] - p.min) / (p.max - p.min);
        slider.setScaledValue(norm);
      }
    }
  }

  function onParamChanged(id) {
    updateKnobDisplay(id);
    updateValueLabel(id);

    if (id === 'style') {
      document.querySelectorAll('.style-btn').forEach(function (btn) {
        btn.classList.toggle('active', parseInt(btn.dataset.style) === Math.round(values.style));
      });
    }
    if (id === 'link') {
      document.getElementById('btnLink').classList.toggle('active', values.link > 0.5);
    }
    if (id === 'dither') {
      document.getElementById('btnDither').classList.toggle('active', values.dither > 0.5);
    }
    if (id === 'oversample') {
      var idx = Math.round(values.oversample);
      document.getElementById('btnOversample').textContent = OVERSAMPLE_LABELS[idx] || '2x';
    }
  }

  // ── Knob drawing ───────────────────────────────────────────────────────────
  var START_ANGLE = Math.PI * 0.75;  // 135°
  var END_ANGLE   = Math.PI * 2.25;  // 405°

  function drawKnob(canvas, norm) {
    var dpr = window.devicePixelRatio || 1;
    var w   = canvas.offsetWidth  || 64;
    var h   = canvas.offsetHeight || 64;
    canvas.width  = w * dpr;
    canvas.height = h * dpr;

    var ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    var cx = w / 2, cy = h / 2;
    var r  = Math.min(w, h) / 2 - 5;

    // Body
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.fillStyle = '#151518';
    ctx.fill();

    // Outer ring
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.strokeStyle = '#252530';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Track arc
    var arcR = r - 4;
    ctx.beginPath();
    ctx.arc(cx, cy, arcR, START_ANGLE, END_ANGLE);
    ctx.strokeStyle = 'rgba(37,37,48,0.7)';
    ctx.lineWidth = 2.5;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Value arc
    var angle = START_ANGLE + norm * (END_ANGLE - START_ANGLE);
    ctx.beginPath();
    ctx.arc(cx, cy, arcR, START_ANGLE, angle);
    ctx.strokeStyle = '#C8A97E';
    ctx.lineWidth = 2.5;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Pointer dot
    var px = cx + Math.cos(angle) * (r * 0.55);
    var py = cy + Math.sin(angle) * (r * 0.55);
    ctx.beginPath();
    ctx.arc(px, py, 3, 0, Math.PI * 2);
    ctx.fillStyle = '#E8E4DC';
    ctx.fill();

    // Centre
    ctx.beginPath();
    ctx.arc(cx, cy, r * 0.2, 0, Math.PI * 2);
    ctx.fillStyle = '#0D0D10';
    ctx.fill();
  }

  function normForParam(id) {
    var p = PARAMS[id];
    return (values[id] - p.min) / (p.max - p.min);
  }

  function updateKnobDisplay(id) {
    var canvas = document.querySelector('[data-param="' + id + '"]');
    if (!canvas) return;
    drawKnob(canvas, normForParam(id));
  }

  function formatValue(id) {
    var p = PARAMS[id];
    var v = values[id];
    switch (p.kind) {
      case 'db':       return v.toFixed(1) + ' dB';
      case 'dbSigned': return (v >= 0 ? '+' : '') + v.toFixed(1) + ' ' + p.label;
      case 'ms':       return v < 10 ? v.toFixed(1) + ' ms' : Math.round(v) + ' ms';
      case 'bool':     return v > 0.5 ? 'On' : 'Off';
      case 'int':      return String(Math.round(v));
      default:         return v.toFixed(1) + (p.label ? ' ' + p.label : '');
    }
  }

  function updateValueLabel(id) {
    var el = document.getElementById('val-' + id);
    if (el) el.textContent = formatValue(id);
  }

  // ── Knob drag interaction ──────────────────────────────────────────────────
  var dragging = null;

  function setupKnobDrag(canvas) {
    var id    = canvas.dataset.param;
    var startY = 0;
    var startV = 0;

    function onMove(e) {
      if (!dragging || dragging !== canvas) return;
      var dy  = startY - (e.touches ? e.touches[0].clientY : e.clientY);
      var p   = PARAMS[id];
      var rng = p.max - p.min;
      var sensitivity = (e.shiftKey || (e.touches && e.touches.length > 1)) ? 0.002 : 0.007;
      var newV = startV + dy * sensitivity * rng;
      setParam(id, newV);
    }

    function onUp() {
      dragging = null;
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      window.removeEventListener('touchmove', onMove);
      window.removeEventListener('touchend', onUp);
    }

    canvas.addEventListener('mousedown', function (e) {
      e.preventDefault();
      dragging = canvas;
      startY   = e.clientY;
      startV   = values[id];
      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup',   onUp);
    });

    canvas.addEventListener('touchstart', function (e) {
      e.preventDefault();
      dragging = canvas;
      startY   = e.touches[0].clientY;
      startV   = values[id];
      window.addEventListener('touchmove', onMove, { passive: false });
      window.addEventListener('touchend',  onUp);
    }, { passive: false });

    canvas.addEventListener('dblclick', function () {
      setParam(id, PARAMS[id].def);
    });

    canvas.addEventListener('wheel', function (e) {
      e.preventDefault();
      var p   = PARAMS[id];
      var rng = p.max - p.min;
      var delta = -e.deltaY * 0.003 * rng;
      setParam(id, values[id] + delta);
    }, { passive: false });
  }

  // ── GR display canvas ─────────────────────────────────────────────────────
  var grCanvas = null;
  var grCtx    = null;

  function initGRCanvas() {
    grCanvas = document.getElementById('grCanvas');
    if (!grCanvas) return;
    grCtx = grCanvas.getContext('2d');
  }

  function resizeGRCanvas() {
    if (!grCanvas) return;
    var dpr  = window.devicePixelRatio || 1;
    var w    = grCanvas.offsetWidth;
    var h    = grCanvas.offsetHeight;
    grCanvas.width  = w * dpr;
    grCanvas.height = h * dpr;
  }

  function drawGRDisplay() {
    if (!grCtx || !grCanvas) return;
    var dpr = window.devicePixelRatio || 1;
    var w   = grCanvas.width  / dpr;
    var h   = grCanvas.height / dpr;
    grCtx.save();
    grCtx.scale(dpr, dpr);

    // Background
    grCtx.fillStyle = '#0D0D10';
    grCtx.fillRect(0, 0, w, h);

    // Grid lines
    grCtx.strokeStyle = 'rgba(37,37,48,0.5)';
    grCtx.lineWidth = 0.5;
    for (var db = -10; db >= -40; db -= 10) {
      var yLine = h * (-db) / 40;
      grCtx.beginPath();
      grCtx.moveTo(0, yLine);
      grCtx.lineTo(w, yLine);
      grCtx.stroke();

      grCtx.fillStyle = '#555560';
      grCtx.font = '8px system-ui';
      grCtx.textAlign = 'right';
      grCtx.fillText(db + ' dB', w - 4, yLine - 2);
    }

    // Clipping line at top
    grCtx.strokeStyle = 'rgba(214,76,58,0.5)';
    grCtx.lineWidth = 1;
    grCtx.beginPath();
    grCtx.moveTo(0, 1);
    grCtx.lineTo(w, 1);
    grCtx.stroke();

    // GR filled shape
    if (w > 0) {
      grCtx.beginPath();
      grCtx.moveTo(0, h);

      for (var i = 0; i < GR_HISTORY; i++) {
        var idx  = (grWritePos + i) % GR_HISTORY;
        var xPos = (i / (GR_HISTORY - 1)) * w;
        var grDb = grHistory[idx]; // 0..-40
        var yPos = h * (-grDb) / 40;
        if (i === 0) grCtx.moveTo(xPos, h - yPos);
        else         grCtx.lineTo(xPos, h - yPos);
      }
      grCtx.lineTo(w, h);
      grCtx.closePath();

      grCtx.fillStyle = 'rgba(200,169,126,0.30)';
      grCtx.fill();

      // Outline
      grCtx.beginPath();
      for (var j = 0; j < GR_HISTORY; j++) {
        var jdx  = (grWritePos + j) % GR_HISTORY;
        var jx   = (j / (GR_HISTORY - 1)) * w;
        var jDb  = grHistory[jdx];
        var jy   = h * (-jDb) / 40;
        if (j === 0) grCtx.moveTo(jx, h - jy);
        else         grCtx.lineTo(jx, h - jy);
      }
      grCtx.strokeStyle = 'rgba(200,169,126,0.65)';
      grCtx.lineWidth = 1;
      grCtx.stroke();
    }

    // Input level bar (left edge)
    var inDbL  = 20 * Math.log10(telem.inputL + 1e-9);
    var inNorm = Math.max(0, (inDbL + 60) / 60);
    var barH   = inNorm * h;
    grCtx.fillStyle = 'rgba(200,169,126,0.45)';
    grCtx.fillRect(0, h - barH, 4, barH);

    // Label
    grCtx.fillStyle = '#555560';
    grCtx.font = '9px system-ui';
    grCtx.textAlign = 'left';
    grCtx.fillText('GR', 8, 14);

    // Border
    grCtx.strokeStyle = '#252530';
    grCtx.lineWidth = 1;
    grCtx.strokeRect(0.5, 0.5, w - 1, h - 1);

    grCtx.restore();
  }

  // ── True-peak meter ────────────────────────────────────────────────────────
  var TP_SEGS = 20;

  function initTPMeter() {
    var meter = document.getElementById('tpMeter');
    if (!meter) return;
    meter.innerHTML = '';
    for (var i = 0; i < TP_SEGS; i++) {
      var seg = document.createElement('div');
      seg.className = 'tp-seg';
      seg.id = 'tp-seg-' + i;
      meter.appendChild(seg);
    }
  }

  function updateTPMeter(levelLinear) {
    var levelDb  = 20 * Math.log10(levelLinear + 1e-9);
    var normLvl  = Math.max(0, Math.min(1, (levelDb + 40) / 40));
    var activeN  = Math.round(normLvl * TP_SEGS);

    for (var i = 0; i < TP_SEGS; i++) {
      var seg = document.getElementById('tp-seg-' + i);
      if (!seg) continue;
      var t = i / (TP_SEGS - 1);
      seg.classList.toggle('clip',   i >= activeN ? false : t > 0.85);
      seg.classList.toggle('active', i < activeN && t <= 0.85);
    }

    var valEl = document.getElementById('tpMeterVal');
    if (valEl) valEl.textContent = levelDb > -70 ? levelDb.toFixed(1) : '—';
  }

  // ── Header readout updates ─────────────────────────────────────────────────
  function updateHeaderReadouts() {
    var lufsEl = document.getElementById('lufsVal');
    var tpEl   = document.getElementById('tpVal');
    var grEl   = document.getElementById('grVal');

    if (lufsEl) {
      lufsEl.textContent = telem.lufs <= -69 ? '—' : telem.lufs.toFixed(1) + ' LUFS';
    }
    if (tpEl) {
      tpEl.textContent = telem.truePeak <= -69 ? '—' : telem.truePeak.toFixed(1) + ' dBFS';
    }
    if (grEl) {
      var gr = telem.gainReductionDb;
      grEl.textContent = gr.toFixed(1) + ' dB';
      grEl.classList.toggle('gr-active', gr < -0.2);
    }
  }

  // ── Plugin layout scaling ─────────────────────────────────────────────────
  function scalePlugin() {
    var vw = window.innerWidth;
    var vh = window.innerHeight;
    var plugin = document.getElementById('plugin');
    if (!plugin) return;
    var scale = Math.min(vw / 900, vh / 540);
    plugin.style.transform = 'translate(-50%,-50%) scale(' + scale + ')';
    resizeGRCanvas();
  }

  // ── Telemetry from C++ ─────────────────────────────────────────────────────
  window.updateApexTelemetry = function (data) {
    if (data.inputL   !== undefined) telem.inputL          = data.inputL;
    if (data.inputR   !== undefined) telem.inputR          = data.inputR;
    if (data.outputL  !== undefined) telem.outputL         = data.outputL;
    if (data.outputR  !== undefined) telem.outputR         = data.outputR;
    if (data.grDb     !== undefined) telem.gainReductionDb = data.grDb;
    if (data.lufs     !== undefined) telem.lufs            = data.lufs;
    if (data.truePeak !== undefined) telem.truePeak        = data.truePeak;

    // Push GR value to history
    grHistory[grWritePos] = Math.max(-40, Math.min(0, telem.gainReductionDb));
    grWritePos = (grWritePos + 1) % GR_HISTORY;

    updateTPMeter(telem.outputL);
    updateHeaderReadouts();
  };

  // ── Render loop ────────────────────────────────────────────────────────────
  function renderFrame() {
    drawGRDisplay();
    requestAnimationFrame(renderFrame);
  }

  // ── Style button logic ────────────────────────────────────────────────────
  function setupStyleButtons() {
    document.querySelectorAll('.style-btn').forEach(function (btn) {
      btn.addEventListener('click', function () {
        setParam('style', parseInt(btn.dataset.style));
      });
    });
  }

  // ── Toggle buttons ─────────────────────────────────────────────────────────
  function setupToggleButtons() {
    var btnLink = document.getElementById('btnLink');
    var btnDither = document.getElementById('btnDither');
    var btnOversample = document.getElementById('btnOversample');
    var btnMatch = document.getElementById('btnMatch');

    if (btnLink) {
      btnLink.addEventListener('click', function () {
        setParam('link', values.link > 0.5 ? 0 : 1);
      });
      btnLink.classList.toggle('active', values.link > 0.5);
    }

    if (btnDither) {
      btnDither.addEventListener('click', function () {
        setParam('dither', values.dither > 0.5 ? 0 : 1);
      });
      btnDither.classList.toggle('active', values.dither > 0.5);
    }

    if (btnOversample) {
      btnOversample.addEventListener('click', function () {
        var next = (Math.round(values.oversample) + 1) % 4;
        setParam('oversample', next);
        btnOversample.textContent = OVERSAMPLE_LABELS[next];
      });
      var osi = Math.round(values.oversample);
      btnOversample.textContent = OVERSAMPLE_LABELS[osi] || '2x';
    }

    if (btnMatch) {
      btnMatch.addEventListener('click', function () {
        var delta  = values.loudnessTarget - telem.lufs;
        var newGain = Math.max(0, Math.min(24, values.gain + delta));
        setParam('gain', newGain);
      });
    }
  }

  // ── A/B buttons ───────────────────────────────────────────────────────────
  function setupABButtons() {
    var abState = {};
    Object.keys(PARAMS).forEach(function (id) { abState[id] = values[id]; });

    var btnA = document.getElementById('btnA');
    var btnB = document.getElementById('btnB');
    var bState = {};

    if (btnA) {
      btnA.addEventListener('click', function () {
        btnA.classList.add('active');
        btnB.classList.remove('active');
      });
    }
    if (btnB) {
      btnB.addEventListener('click', function () {
        btnB.classList.add('active');
        btnA.classList.remove('active');
        // Store A, load B (or swap)
        var tmp = {};
        Object.keys(PARAMS).forEach(function (id) { tmp[id] = values[id]; });
        Object.keys(bState).forEach(function (id) { if (bState[id] !== undefined) setParam(id, bState[id]); });
        bState = tmp;
      });
    }
  }

  // ── Init ──────────────────────────────────────────────────────────────────
  function init() {
    // Draw all initial knobs
    document.querySelectorAll('[data-param]').forEach(function (canvas) {
      if (canvas.tagName === 'CANVAS') {
        setupKnobDrag(canvas);
        drawKnob(canvas, normForParam(canvas.dataset.param));
      }
    });

    // Set initial value labels
    Object.keys(PARAMS).forEach(function (id) {
      updateValueLabel(id);
    });

    initGRCanvas();
    initTPMeter();
    setupStyleButtons();
    setupToggleButtons();
    setupABButtons();

    // Initial button states
    var osi = Math.round(values.oversample);
    var btnOs = document.getElementById('btnOversample');
    if (btnOs) btnOs.textContent = OVERSAMPLE_LABELS[osi] || '2x';

    document.getElementById('btnLink')   && document.getElementById('btnLink').classList.toggle('active', values.link > 0.5);
    document.getElementById('btnDither') && document.getElementById('btnDither').classList.toggle('active', values.dither > 0.5);

    // Style buttons
    document.querySelectorAll('.style-btn').forEach(function (btn) {
      btn.classList.toggle('active', parseInt(btn.dataset.style) === Math.round(values.style));
    });

    initJUCE();
    scalePlugin();
    window.addEventListener('resize', scalePlugin);
    renderFrame();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
