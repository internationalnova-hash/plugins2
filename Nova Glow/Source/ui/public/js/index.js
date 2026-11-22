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
// Stage scale-to-fit (same technique as Nova Ride/Nova Bass) - #app is a
// fixed 1600x640 design canvas, uniformly scaled via CSS transform to
// whatever size the actual window is.
// ---------------------------------------------------------------------
(function () {
  var lastW = -1, lastH = -1;
  function fitStage() {
    var w = document.documentElement.clientWidth || window.innerWidth || 0;
    var h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    var scale = Math.min(w / 1600, h / 640);
    window.__novaGlowScale = scale || 1;
    var stage = document.getElementById("app");
    if (stage) stage.style.transform = "scale(" + scale + ")";
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();

// ---------------------------------------------------------------------
// Rotary knobs - drag vertically to change. Works against either a
// SliderState (continuous) or a ComboBoxState (discrete stops).
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
    dragging = true;
    startY = e.clientY;
    startNorm = currentNorm();
    try { el.setPointerCapture(e.pointerId); } catch (err) {}
    if (state && state.sliderDragStarted) state.sliderDragStarted();
  });
  el.addEventListener("pointermove", function (e) {
    if (!dragging || !state) return;
    var scale = window.__novaGlowScale || 1;
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
// Radial tick + number ring around the two big knobs (Peak Reduction,
// Gain) - matches the mockup's engraved-scale look. Ticks are drawn in
// the same -135deg..+135deg / 270deg sweep convention wireKnob uses for
// --angle, so a given value's number always sits under the pointer's
// resting position for that value.
// ---------------------------------------------------------------------
function renderKnobScale(containerId, min, max, step) {
  var container = document.getElementById(containerId);
  if (!container) return;

  var box = 232, c = box / 2;
  var tickInnerR = 88, tickOuterR = 98, numberR = 106;

  var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);

  for (var v = min; v <= max; v += step) {
    var norm = (v - min) / (max - min);
    var angleDeg = -135 + norm * 270;
    var rad = (Math.PI / 180) * angleDeg;
    var sin = Math.sin(rad), cos = Math.cos(rad);

    var x1 = c + tickInnerR * sin, y1 = c - tickInnerR * cos;
    var x2 = c + tickOuterR * sin, y2 = c - tickOuterR * cos;
    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#3a2f18");
    line.setAttribute("stroke-width", "2");
    line.setAttribute("stroke-linecap", "round");
    svg.appendChild(line);

    var tx = c + numberR * sin, ty = c - numberR * cos;
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", "11");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", "#3a2f18");
    text.textContent = String(v);
    svg.appendChild(text);
  }

  container.innerHTML = "";
  container.appendChild(svg);
}

// ---------------------------------------------------------------------
// Two-state image switch (Compress/Limit) - backed by a 2-choice ComboBox.
// ---------------------------------------------------------------------
function wireTwoStateSwitch(elId, name) {
  var el = document.getElementById(elId);
  if (!el) return;
  var state = hasNative() ? getComboBoxState(name) : null;

  function applyVisual() {
    var idx = state ? state.getChoiceIndex() : 0;
    el.classList.toggle("limit", idx === 1);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  el.addEventListener("click", function () {
    if (!state) return;
    var idx = state.getChoiceIndex();
    state.setChoiceIndex(idx === 0 ? 1 : 0);
    applyVisual();
  });
}

// ---------------------------------------------------------------------
// 3-button mode groups (Analog Mode, Oversampling) - backed by a 3-choice
// ComboBox. :hover is CSS-only; .active is JS-toggled.
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
// Binary-image toggle backed by a continuous 0-100 slider (Stereo Link) -
// the mockup presents it as a simple on/off switch; clicking snaps the
// underlying parameter to 0 or 100, while automation to any intermediate
// value still displays as whichever side of 50 it's closest to.
// ---------------------------------------------------------------------
function wireSliderAsToggle(elId, name) {
  var el = document.getElementById(elId);
  if (!el) return;
  var state = hasNative() ? getSliderState(name) : null;

  function applyVisual() {
    var on = state ? state.getScaledValue() >= 50 : true;
    el.classList.toggle("on", on);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  el.addEventListener("click", function () {
    if (!state) return;
    var on = state.getScaledValue() >= 50;
    state.setNormalisedValue(on ? 0 : 1);
    applyVisual();
  });
}

// ---------------------------------------------------------------------
// Power jewel - inverted bypass (lit = processing active, i.e. NOT bypassed).
// ---------------------------------------------------------------------
function wirePower() {
  var el = document.getElementById("powerJewel");
  if (!el) return;
  var state = hasNative() ? getToggleState("bypass") : null;

  function applyVisual() {
    var bypassed = state ? state.getValue() : false;
    el.classList.toggle("on", !bypassed);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  el.addEventListener("click", function () {
    if (!state) return;
    state.setValue(!state.getValue());
    applyVisual();
  });
}

// ---------------------------------------------------------------------
// VU scale (drawn per meter mode) + needle, driven by real DSP telemetry.
// ---------------------------------------------------------------------
var currentMeterMode = 0; // 0=GR 1=Output 2=Input
var METER_LABELS = ["GR", "OUTPUT", "INPUT"];

var SCALES = {
  0: { min: 0, max: 15, ticks: [0, 3, 6, 9, 12, 15], redFrom: 10, unit: "dB", reverse: true },
  1: { min: -20, max: 3, ticks: [-20, -10, -7, -5, -3, -1, 0, 1, 2, 3], redFrom: 0, unit: "VU", reverse: false },
  2: { min: -20, max: 3, ticks: [-20, -10, -7, -5, -3, -1, 0, 1, 2, 3], redFrom: 0, unit: "VU", reverse: false }
};

var NEEDLE_MIN_ANGLE = -45, NEEDLE_MAX_ANGLE = 45;

function angleForValue(mode, value) {
  var s = SCALES[mode];
  var t = (value - s.min) / (s.max - s.min);
  t = Math.max(0, Math.min(1, t));
  if (s.reverse) t = 1 - t;
  return NEEDLE_MIN_ANGLE + t * (NEEDLE_MAX_ANGLE - NEEDLE_MIN_ANGLE);
}

function renderVuScale() {
  var container = document.getElementById("vuScale");
  if (!container) return;
  var s = SCALES[currentMeterMode];
  var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 300 120");

  var cx = 150, cy = 118, r = 100;
  s.ticks.forEach(function (t) {
    var value = s.reverse ? (s.max - (t - s.min)) : t;
    var norm = (t - s.min) / (s.max - s.min);
    var angleDeg = s.reverse ? (180 * (1 - norm)) : (180 * norm);
    var rad = (Math.PI / 180) * (180 - angleDeg);
    var x1 = cx + r * Math.cos(rad), y1 = cy - r * Math.sin(rad);
    var x2 = cx + (r - 10) * Math.cos(rad), y2 = cy - (r - 10) * Math.sin(rad);
    var isRed = s.reverse ? (t <= (s.max - s.redFrom)) : (t >= s.redFrom);

    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", isRed ? "#c23b2e" : "#241d10");
    line.setAttribute("stroke-width", "2");
    svg.appendChild(line);

    var tx = cx + (r - 24) * Math.cos(rad), ty = cy - (r - 24) * Math.sin(rad);
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("font-size", "11");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", isRed ? "#c23b2e" : "#241d10");
    text.textContent = (t > 0 && !s.reverse ? "+" : "") + t;
    svg.appendChild(text);

    void value;
  });

  container.innerHTML = "";
  container.appendChild(svg);
}

window.updateGlowTelemetry = function (data) {
  if (!data) return;
  var needle = document.getElementById("vuNeedle");
  var value = data.grDb;
  if (currentMeterMode === 1) value = data.outputVu;
  else if (currentMeterMode === 2) value = data.inputVu;

  if (needle) needle.style.setProperty("--needleAngle", angleForValue(currentMeterMode, value) + "deg");
};

function wireMeterModeKnob() {
  var el = document.getElementById("knobMeterMode");
  var modeLabelEl = document.getElementById("vuModeLabel");
  var items = [
    document.getElementById("meterItem0"),
    document.getElementById("meterItem1"),
    document.getElementById("meterItem2"),
  ];
  if (!el) return;
  var dial = el.querySelector(".meterthumb");
  var state = hasNative() ? getComboBoxState("meterMode") : null;
  var METER_ANGLES = [-26, 0, 26];

  function applyVisual() {
    var idx = state ? state.getChoiceIndex() : 0;
    currentMeterMode = idx;
    if (dial) dial.style.setProperty("--angle", METER_ANGLES[idx] + "deg");
    var label = METER_LABELS[idx] || "GR";
    items.forEach(function (it, i) { if (it) it.classList.toggle("active", i === idx); });
    if (modeLabelEl) modeLabelEl.textContent = label;
    renderVuScale();
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  var dragging = false, startY = 0, startNorm = 0;
  el.addEventListener("pointerdown", function (e) {
    dragging = true; startY = e.clientY; startNorm = state ? state.getChoiceIndex() / 2 : 0;
    try { el.setPointerCapture(e.pointerId); } catch (err) {}
  });
  el.addEventListener("pointermove", function (e) {
    if (!dragging || !state) return;
    var scale = window.__novaGlowScale || 1;
    var deltaNorm = ((startY - e.clientY) / scale) / 100;
    var newNorm = Math.max(0, Math.min(1, startNorm + deltaNorm));
    state.setChoiceIndex(Math.round(newNorm * 2));
    applyVisual();
  });
  el.addEventListener("pointerup", function () { dragging = false; });
  el.addEventListener("pointercancel", function () { dragging = false; });
}

// ---------------------------------------------------------------------
// Presets - factory + user (saved to disk natively), browsed via
// prev/next, created via the Save modal.
// ---------------------------------------------------------------------
var presetNames = ["Default"];
var presetIndex = 0;

function refreshPresetLabel() {
  var el = document.getElementById("presetName");
  if (el) el.textContent = presetNames[presetIndex] || "Default";
}

function initPresets() {
  callNative("getPresets").then(function (result) {
    if (result && result.names) {
      presetNames = result.names;
      presetIndex = result.currentIndex || 0;
      refreshPresetLabel();
    }
  });

  document.getElementById("presetPrev").addEventListener("click", function () {
    if (presetNames.length === 0) return;
    presetIndex = (presetIndex - 1 + presetNames.length) % presetNames.length;
    callNative("applyPreset", presetIndex).then(function (result) {
      if (result && result.success) refreshPresetLabel();
    });
  });
  document.getElementById("presetNext").addEventListener("click", function () {
    if (presetNames.length === 0) return;
    presetIndex = (presetIndex + 1) % presetNames.length;
    callNative("applyPreset", presetIndex).then(function (result) {
      if (result && result.success) refreshPresetLabel();
    });
  });

  var saveBtn = document.getElementById("saveBtn");
  var modal = document.getElementById("saveModal");
  var input = document.getElementById("saveModalInput");
  var errorEl = document.getElementById("saveModalError");
  var cancelBtn = document.getElementById("saveModalCancel");
  var confirmBtn = document.getElementById("saveModalConfirm");

  function openModal() {
    errorEl.textContent = "";
    input.value = "";
    modal.hidden = false;
    setTimeout(function () { input.focus(); }, 20);
  }
  function closeModal() { modal.hidden = true; }

  function doSave() {
    var name = input.value.trim();
    if (!name) { errorEl.textContent = "Enter a preset name."; return; }
    callNative("savePreset", name).then(function (result) {
      if (result && result.success) {
        presetNames = result.names || presetNames;
        presetIndex = result.currentIndex || (presetNames.length - 1);
        refreshPresetLabel();
        closeModal();
      } else {
        errorEl.textContent = (result && result.error) || "Could not save preset.";
      }
    });
  }

  saveBtn.addEventListener("click", openModal);
  cancelBtn.addEventListener("click", closeModal);
  confirmBtn.addEventListener("click", doSave);
  input.addEventListener("keydown", function (e) {
    if (e.key === "Enter") doSave();
    else if (e.key === "Escape") closeModal();
  });
  modal.addEventListener("click", function (e) {
    if (e.target === modal) closeModal();
  });
}

// ---------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------
function init() {
  renderKnobScale("ticksPeakReduction", 0, 100, 10);
  renderKnobScale("ticksGain", 0, 100, 10);

  wireKnob("knobPeakReduction", { kind: "slider", name: "peakReduction", dragRangePx: 150,
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 30); } });
  wireKnob("knobGain", { kind: "slider", name: "gain", dragRangePx: 150,
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 50); } });
  wireKnob("knobMix", { kind: "slider", name: "mix",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 100); } });
  wireKnob("knobScHpf", { kind: "slider", name: "scHpf",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 30); } });
  wireKnob("knobEmphasis", { kind: "slider", name: "emphasis",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 0); } });
  wireKnob("knobGlow", { kind: "slider", name: "glow",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 45); } });
  wireKnob("knobNoise", { kind: "slider", name: "noise",
    formatValue: function (s) { return Math.round(s ? s.getScaledValue() : 0); } });

  wireTwoStateSwitch("clSwitch", "compressLimit");
  wireButtonGroup("analogGroup", "analogMode");
  wireButtonGroup("oversampleGroup", "oversampling");
  wireSliderAsToggle("stereoLinkToggle", "stereoLink");
  wirePower();
  wireMeterModeKnob();

  renderVuScale();
  initPresets();
}

if (document.readyState === "loading")
  document.addEventListener("DOMContentLoaded", init);
else
  init();
