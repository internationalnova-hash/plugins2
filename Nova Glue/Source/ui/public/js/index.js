import { getSliderState, getToggleState, getComboBoxState, getNativeFunction } from "./juce/index.js";

function hasNative() {
  return typeof window.__JUCE__ !== "undefined";
}

function callNative(name) {
  var args = Array.prototype.slice.call(arguments, 1);
  if (!hasNative()) return Promise.resolve(null);
  var fn = getNativeFunction(name);
  return fn.apply(null, args);
}

// ---------------------------------------------------------------------
// Stage scale-to-fit (same technique as Nova Glow/Nova Ride) - #app is a
// fixed-px design canvas nested inside #stageOuter, scaled to fit whatever
// window size the host gives the editor.
// ---------------------------------------------------------------------
(function () {
  var lastW = -1, lastH = -1;
  function fitStage() {
    var w = document.documentElement.clientWidth || window.innerWidth || 0;
    var h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    var scale = Math.min(w / 1080, h / 1126);
    window.__novaGlueScale = scale || 1;
    var stage = document.getElementById("app");
    if (stage) stage.style.transform = "scale(" + scale + ")";
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();

// ---------------------------------------------------------------------
// Knob wiring - continuous sliders and discrete comboboxes share the same
// -135deg..+135deg / 270deg sweep convention.
// ---------------------------------------------------------------------
function wireKnob(elId, opts) {
  var el = document.getElementById(elId);
  if (!el) return null;
  var dial = el.querySelector(".knobDial");
  var valueEl = document.getElementById(elId + "Value");

  var state = hasNative() ? (opts.kind === "combobox" ? getComboBoxState(opts.name) : getSliderState(opts.name)) : null;

  function numChoices() {
    return state && state.properties && state.properties.choices ? state.properties.choices.length : 1;
  }

  function currentNorm() {
    if (!state) return 0.5;
    if (opts.kind === "combobox") {
      var n = numChoices();
      return n > 1 ? state.getChoiceIndex() / (n - 1) : 0;
    }
    return state.getNormalisedValue();
  }

  function applyVisual() {
    var norm = Math.max(0, Math.min(1, currentNorm()));
    var angle = -135 + norm * 270;
    if (dial) dial.style.setProperty("--angle", angle + "deg");
    if (valueEl && opts.formatValue) valueEl.textContent = opts.formatValue(state, norm);
    if (opts.onChange) opts.onChange(state, norm);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else {
    applyVisual();
  }

  var dragging = false, startY = 0, startNorm = 0;
  var dragRangePx = opts.dragRangePx || 140;

  el.addEventListener("pointerdown", function (e) {
    e.preventDefault();
    dragging = true;
    startY = e.clientY;
    startNorm = currentNorm();
    try { el.setPointerCapture(e.pointerId); } catch (err) {}
    if (state && state.sliderDragStarted) state.sliderDragStarted();
  });
  el.addEventListener("pointermove", function (e) {
    if (!dragging || !state) return;
    var scale = window.__novaGlueScale || 1;
    var deltaNorm = ((startY - e.clientY) / scale) / dragRangePx;
    var newNorm = Math.max(0, Math.min(1, startNorm + deltaNorm));
    if (opts.kind === "combobox") {
      var n = numChoices();
      state.setChoiceIndex(Math.round(newNorm * (n - 1)));
    } else {
      state.setNormalisedValue(newNorm);
    }
    applyVisual();
  });
  function endDrag() {
    if (!dragging) return;
    dragging = false;
    if (state && state.sliderDragEnded) state.sliderDragEnded();
  }
  el.addEventListener("pointerup", endDrag);
  el.addEventListener("pointercancel", endDrag);

  return { refresh: applyVisual, getState: function () { return state; } };
}

// ---------------------------------------------------------------------
// A dense ring of short, unlabeled minor ticks spanning the full
// -135deg..+135deg sweep - matches the mockup's engraved-scale look
// (a knurled-looking ring of fine marks behind the numbered major ticks).
// ---------------------------------------------------------------------
function addMinorTicks(svg, c, innerR, outerR, count) {
  for (var i = 0; i <= count; i++) {
    var norm = i / count;
    var angleDeg = -135 + norm * 270;
    var rad = (Math.PI / 180) * angleDeg;
    var sin = Math.sin(rad), cos = Math.cos(rad);
    var x1 = c + innerR * sin, y1 = c - innerR * cos;
    var x2 = c + outerR * sin, y2 = c - outerR * cos;
    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#8c8065");
    line.setAttribute("stroke-width", "1");
    svg.appendChild(line);
  }
}

// ---------------------------------------------------------------------
// Radial tick + number ring for continuous big knobs (Threshold, Makeup).
// ---------------------------------------------------------------------
function renderKnobScale(containerId, min, max, step) {
  var container = document.getElementById(containerId);
  if (!container) return;

  var box = 192, c = box / 2;
  var tickInnerR = 68, tickOuterR = 86, numberR = 100;

  var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);

  addMinorTicks(svg, c, 70, 82, Math.round((max - min) / (step / 5)));

  for (var v = min; v <= max + 1e-6; v += step) {
    var norm = (v - min) / (max - min);
    var angleDeg = -135 + norm * 270;
    var rad = (Math.PI / 180) * angleDeg;
    var sin = Math.sin(rad), cos = Math.cos(rad);

    var x1 = c + tickInnerR * sin, y1 = c - tickInnerR * cos;
    var x2 = c + tickOuterR * sin, y2 = c - tickOuterR * cos;
    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#e4d6b0");
    line.setAttribute("stroke-width", "1.8");
    line.setAttribute("stroke-linecap", "round");
    svg.appendChild(line);

    var tx = c + numberR * sin, ty = c - numberR * cos;
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", "10.5");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", "#e4d6b0");
    text.textContent = String(Math.round(v));
    svg.appendChild(text);
  }

  container.innerHTML = "";
  container.appendChild(svg);
}

// ---------------------------------------------------------------------
// Radial tick + label ring for discrete-choice big knobs (Attack,
// Release) - ticks are evenly spaced by index rather than by value, since
// choice steps aren't linear (0.1/0.3/1/3/10/30ms).
// ---------------------------------------------------------------------
function renderChoiceKnobScale(containerId, labels) {
  var container = document.getElementById(containerId);
  if (!container) return;

  var box = 192, c = box / 2;
  var tickInnerR = 68, tickOuterR = 86, numberR = 102;
  var n = labels.length;

  var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);

  addMinorTicks(svg, c, 70, 82, (n - 1) * 4);

  for (var i = 0; i < n; i++) {
    var norm = n > 1 ? i / (n - 1) : 0;
    var angleDeg = -135 + norm * 270;
    var rad = (Math.PI / 180) * angleDeg;
    var sin = Math.sin(rad), cos = Math.cos(rad);

    var x1 = c + tickInnerR * sin, y1 = c - tickInnerR * cos;
    var x2 = c + tickOuterR * sin, y2 = c - tickOuterR * cos;
    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#e4d6b0");
    line.setAttribute("stroke-width", "1.8");
    line.setAttribute("stroke-linecap", "round");
    svg.appendChild(line);

    var tx = c + numberR * sin, ty = c - numberR * cos;
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", "9.5");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", "#e4d6b0");
    text.textContent = labels[i];
    svg.appendChild(text);
  }

  container.innerHTML = "";
  container.appendChild(svg);
}

// ---------------------------------------------------------------------
// Plain unlabeled minor-tick ring for the lower panel's small knobs (SC
// HPF, Mix, Glue, Drive) - the mockup shows these with a decorative tick
// ring and no numbers directly on the knob (min/max + unit are captioned
// below instead).
// ---------------------------------------------------------------------
function renderSmallKnobScale(containerId) {
  var container = document.getElementById(containerId);
  if (!container) return;
  var box = 128, c = box / 2;
  var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);
  addMinorTicks(svg, c, 48, 60, 32);
  container.innerHTML = "";
  container.appendChild(svg);
}

// ---------------------------------------------------------------------
// 2/3-button mode groups backed by a ComboBox (Ratio, Oversample). :hover
// is CSS-only; .active is JS-toggled.
// ---------------------------------------------------------------------
function wireButtonGroup(containerId, name) {
  var container = document.getElementById(containerId);
  if (!container) return;
  var buttons = Array.prototype.slice.call(container.querySelectorAll(".modebtn"));
  var state = hasNative() ? getComboBoxState(name) : null;

  function applyVisual() {
    var idx = state ? state.getChoiceIndex() : 0;
    buttons.forEach(function (b) {
      b.classList.toggle("active", parseInt(b.getAttribute("data-idx"), 10) === idx);
    });
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  buttons.forEach(function (b) {
    b.addEventListener("click", function () {
      if (!state) return;
      state.setChoiceIndex(parseInt(b.getAttribute("data-idx"), 10));
      applyVisual();
    });
  });
}

// ---------------------------------------------------------------------
// 2-button OFF/ON groups backed by a plain Toggle (Auto Gain, Stereo
// Link) - idx 0 = off/false, idx 1 = on/true.
// ---------------------------------------------------------------------
function wireBoolButtonGroup(containerId, name) {
  var container = document.getElementById(containerId);
  if (!container) return;
  var buttons = Array.prototype.slice.call(container.querySelectorAll(".modebtn"));
  var state = hasNative() ? getToggleState(name) : null;

  function applyVisual() {
    var on = state ? state.getValue() : false;
    buttons.forEach(function (b) {
      var idx = parseInt(b.getAttribute("data-idx"), 10);
      b.classList.toggle("active", (idx === 1) === on);
    });
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  buttons.forEach(function (b) {
    b.addEventListener("click", function () {
      if (!state) return;
      var idx = parseInt(b.getAttribute("data-idx"), 10);
      state.setValue(idx === 1);
      applyVisual();
    });
  });
}

// ---------------------------------------------------------------------
// Compressor IN/BYPASS - backed by the plugin's own "bypass" toggle, but
// inverted (idx 0 = IN = not bypassed, idx 1 = BYPASS = bypassed).
// ---------------------------------------------------------------------
function wireBypassGroup(containerId) {
  var container = document.getElementById(containerId);
  if (!container) return;
  var buttons = Array.prototype.slice.call(container.querySelectorAll(".modebtn"));
  var state = hasNative() ? getToggleState("bypass") : null;

  function applyVisual() {
    var bypassed = state ? state.getValue() : false;
    buttons.forEach(function (b) {
      var idx = parseInt(b.getAttribute("data-idx"), 10);
      b.classList.toggle("active", (idx === 1) === bypassed);
    });
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  buttons.forEach(function (b) {
    b.addEventListener("click", function () {
      if (!state) return;
      var idx = parseInt(b.getAttribute("data-idx"), 10);
      state.setValue(idx === 1);
      applyVisual();
    });
  });
}

// ---------------------------------------------------------------------
// GR meter scale (fixed 0-20dB) + needle, driven by real DSP telemetry.
// Needle sweeps -50deg (0dB GR) .. +50deg (20dB GR).
// ---------------------------------------------------------------------
var METER_TICKS = [0, 4, 8, 12, 16, 20];
var METER_MIN_ANGLE = -50, METER_MAX_ANGLE = 50;

function angleForGr(grDb) {
  var clamped = Math.max(0, Math.min(20, grDb));
  return METER_MIN_ANGLE + (clamped / 20) * (METER_MAX_ANGLE - METER_MIN_ANGLE);
}

function renderMeterScale() {
  var container = document.getElementById("meterScale");
  if (!container) return;
  var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 300 110");

  var cx = 150, cy = 108, r = 96;

  // Dense minor ticks (every 1dB) behind the numbered major ticks, for
  // the engraved-scale look the mockup shows.
  for (var m = 0; m <= 20; m += 1) {
    var mAngle = angleForGr(m);
    var mRad = (Math.PI / 180) * mAngle;
    var mSin = Math.sin(mRad), mCos = Math.cos(mRad);
    var isMajor = m % 4 === 0;
    if (isMajor) continue; // major ticks drawn below, with numbers
    var mx1 = cx + r * mSin, my1 = cy - r * mCos;
    var mx2 = cx + (r - 6) * mSin, my2 = cy - (r - 6) * mCos;
    var mline = document.createElementNS("http://www.w3.org/2000/svg", "line");
    mline.setAttribute("x1", mx1); mline.setAttribute("y1", my1);
    mline.setAttribute("x2", mx2); mline.setAttribute("y2", my2);
    mline.setAttribute("stroke", "#5c4a22");
    mline.setAttribute("stroke-width", "1.2");
    svg.appendChild(mline);
  }

  METER_TICKS.forEach(function (t) {
    var angleDeg = angleForGr(t);
    var rad = (Math.PI / 180) * angleDeg;
    var sin = Math.sin(rad), cos = Math.cos(rad);
    var x1 = cx + r * sin, y1 = cy - r * cos;
    var x2 = cx + (r - 10) * sin, y2 = cy - (r - 10) * cos;

    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#3a2b12");
    line.setAttribute("stroke-width", "2");
    svg.appendChild(line);

    var tx = cx + (r - 24) * sin, ty = cy - (r - 24) * cos;
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("font-size", "11");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", "#3a2b12");
    text.textContent = String(t);
    svg.appendChild(text);
  });

  // Needle lives in the scale's own coordinates so it always lines up with the ticks.
  var NS = "http://www.w3.org/2000/svg";
  var defs = document.createElementNS(NS, "defs");
  defs.innerHTML = '<linearGradient id="grNeedleGrad" x1="0" y1="0" x2="1" y2="0"><stop stop-color="#6d2d16"/><stop offset=".5" stop-color="#d8702e"/><stop offset="1" stop-color="#7a3117"/></linearGradient>';
  svg.appendChild(defs);
  var g = document.createElementNS(NS, "g");
  g.setAttribute("id", "meterNeedleG");
  g.setAttribute("transform", "rotate(" + METER_MIN_ANGLE + " " + cx + " " + cy + ")");
  g.innerHTML =
    '<path d="M' + (cx - 1.8) + ' ' + (cy - 34) + ' L' + (cx - 1.6) + ' ' + (cy - r + 2) + ' L' + (cx + 1.6) + ' ' + (cy - r + 2) + ' L' + (cx + 1.8) + ' ' + (cy - 34) + ' Z" fill="#000" opacity=".25" transform="translate(1.5 1)"/>' +
    '<path d="M' + (cx - 1.3) + ' ' + (cy - 34) + ' L' + (cx - 1.1) + ' ' + (cy - r + 2) + ' L' + (cx + 1.1) + ' ' + (cy - r + 2) + ' L' + (cx + 1.3) + ' ' + (cy - 34) + ' Z" fill="url(#grNeedleGrad)"/>';
  svg.appendChild(g);

  container.innerHTML = "";
  container.appendChild(svg);
}

window.updateGlueTelemetry = function (data) {
  if (!data) return;
  var g = document.getElementById("meterNeedleG");
  if (g) g.setAttribute("transform", "rotate(" + angleForGr(data.grDb).toFixed(2) + " 150 108)");
};

// ---------------------------------------------------------------------
// Preset system - getPresets/applyPreset/savePreset native functions.
// ---------------------------------------------------------------------
var presetNames = [];
var presetIndex = 0;

function refreshPresetLabel() {
  var el = document.getElementById("presetName");
  if (el) el.textContent = presetNames[presetIndex] || "Default";
}

function initPresets() {
  callNative("getPresets").then(function (result) {
    if (!result) return;
    presetNames = result.names || [];
    presetIndex = result.currentIndex || 0;
    refreshPresetLabel();
  });

  var prevBtn = document.getElementById("presetPrev");
  var nextBtn = document.getElementById("presetNext");
  if (prevBtn) prevBtn.addEventListener("click", function () {
    if (presetNames.length === 0) return;
    var newIndex = (presetIndex - 1 + presetNames.length) % presetNames.length;
    callNative("applyPreset", newIndex).then(function (result) {
      if (result && result.success) { presetIndex = result.currentIndex; refreshPresetLabel(); }
    });
  });
  if (nextBtn) nextBtn.addEventListener("click", function () {
    if (presetNames.length === 0) return;
    var newIndex = (presetIndex + 1) % presetNames.length;
    callNative("applyPreset", newIndex).then(function (result) {
      if (result && result.success) { presetIndex = result.currentIndex; refreshPresetLabel(); }
    });
  });

  var saveBtn = document.getElementById("saveBtn");
  var modal = document.getElementById("saveModal");
  var input = document.getElementById("saveModalInput");
  var errorEl = document.getElementById("saveModalError");
  var cancelBtn = document.getElementById("saveModalCancel");
  var confirmBtn = document.getElementById("saveModalConfirm");

  function closeModal() { if (modal) modal.hidden = true; if (errorEl) errorEl.textContent = ""; }
  function openModal() {
    if (!modal) return;
    modal.hidden = false;
    if (errorEl) errorEl.textContent = "";
    if (input) { input.value = ""; input.focus(); }
  }
  function doSave() {
    var name = input ? input.value.trim() : "";
    if (!name) { if (errorEl) errorEl.textContent = "Enter a preset name."; return; }
    callNative("savePreset", name).then(function (result) {
      if (result && result.success) {
        presetNames = result.names || presetNames;
        presetIndex = result.currentIndex;
        refreshPresetLabel();
        closeModal();
      } else if (errorEl) {
        errorEl.textContent = (result && result.error) || "Could not save preset.";
      }
    });
  }

  if (saveBtn) saveBtn.addEventListener("click", openModal);
  if (cancelBtn) cancelBtn.addEventListener("click", closeModal);
  if (confirmBtn) confirmBtn.addEventListener("click", doSave);
  if (input) input.addEventListener("keydown", function (e) {
    if (e.key === "Enter") doSave();
    if (e.key === "Escape") closeModal();
  });
  if (modal) modal.addEventListener("click", function (e) { if (e.target === modal) closeModal(); });
}

// ---------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------
function init() {
  renderKnobScale("ticksThreshold", -30, 10, 10);
  renderChoiceKnobScale("ticksAttack", ["0.1", "0.3", "1", "3", "10", "30"]);
  renderChoiceKnobScale("ticksRelease", ["0.1", "0.3", "0.6", "1.2", "Auto"]);
  renderKnobScale("ticksMakeup", -20, 20, 10);
  renderSmallKnobScale("ticksScHpf");
  renderSmallKnobScale("ticksMix");
  renderSmallKnobScale("ticksGlue");
  renderSmallKnobScale("ticksDrive");
  renderMeterScale();

  wireKnob("knobThreshold", { kind: "slider", name: "threshold", dragRangePx: 150,
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : -10) + " dB"; } });

  wireKnob("knobAttack", { kind: "combobox", name: "attack", dragRangePx: 130,
    formatValue: function (s) {
      var choices = s && s.properties.choices ? s.properties.choices : ["10 ms"];
      var idx = s ? s.getChoiceIndex() : 4;
      return choices[idx] || "10 ms";
    } });

  wireKnob("knobRelease", { kind: "combobox", name: "release", dragRangePx: 130,
    formatValue: function (s) {
      var choices = s && s.properties.choices ? s.properties.choices : ["Auto"];
      var idx = s ? s.getChoiceIndex() : 4;
      return choices[idx] || "Auto";
    } });

  wireKnob("knobMakeup", { kind: "slider", name: "makeup", dragRangePx: 150,
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 0) + " dB"; } });

  wireKnob("knobScHpf", { kind: "slider", name: "scHpf",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 20) + " Hz"; } });

  wireKnob("knobMix", { kind: "slider", name: "mix",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 100) + "%"; } });

  wireKnob("knobGlue", { kind: "slider", name: "glue",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 45) + "%"; },
    onChange: function (s, norm) {
      var ring = document.getElementById("glueRingActive");
      if (ring) ring.style.opacity = String(Math.max(0, Math.min(1, norm)));
    } });

  wireKnob("knobDrive", { kind: "slider", name: "drive",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 0) + "%"; } });

  wireButtonGroup("ratioGroup", "ratio");
  wireButtonGroup("oversampleGroup", "oversampling");
  wireBoolButtonGroup("autoGainGroup", "autoGain");
  wireBoolButtonGroup("stereoLinkGroup", "stereoLink");
  wireBypassGroup("bypassGroup");

  initPresets();
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", init);
} else {
  init();
}
