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
// Stage scale-to-fit (same technique as Nova Bass/Nova Tape) - #app is a
// fixed 1280x800 design canvas, uniformly scaled via CSS transform to
// whatever size the actual window is, instead of reflowing/cutting off
// fixed-px controls. window.__novaRideScale is read by knob-drag math
// below to normalize pointer deltas back into design-space pixels.
(function () {
  var lastW = -1, lastH = -1;
  function fitStage() {
    var w = document.documentElement.clientWidth || window.innerWidth || 0;
    var h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    var scale = Math.min(w / 1280, h / 800);
    window.__novaRideScale = scale || 1;
    var stage = document.getElementById("app");
    if (stage) stage.style.transform = "scale(" + scale + ")";
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();

// ---------------------------------------------------------------------
// Generic rotary knob wiring - drag vertically to change. Works against
// either a SliderState (continuous) or a ComboBoxState (discrete stops).
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
    var scale = window.__novaRideScale || 1;
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
// Toggle switches
// ---------------------------------------------------------------------
function wireToggle(elId, paramName, onChangeCb) {
  var el = document.getElementById(elId);
  if (!el) return;
  var state = hasNative() ? getToggleState(paramName) : null;

  function applyVisual() {
    var on = state ? state.getValue() : false;
    el.classList.toggle("on", !!on);
    if (onChangeCb) onChangeCb(!!on);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  }

  el.addEventListener("click", function () {
    if (!state) return;
    state.setValue(!state.getValue());
    applyVisual();
  });
}

// ---------------------------------------------------------------------
// Mode buttons (3-way, backed by a ComboBoxState)
// ---------------------------------------------------------------------
function wireModeButtons() {
  var buttons = Array.prototype.slice.call(document.querySelectorAll(".modeBtn"));
  var state = hasNative() ? getComboBoxState("mode") : null;

  function applyVisual() {
    var idx = state ? state.getChoiceIndex() : 0;
    buttons.forEach(function (b) {
      b.classList.toggle("active", parseInt(b.getAttribute("data-mode"), 10) === idx);
    });
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else {
    applyVisual();
  }

  buttons.forEach(function (b) {
    b.addEventListener("click", function () {
      if (!state) return;
      state.setChoiceIndex(parseInt(b.getAttribute("data-mode"), 10));
      applyVisual();
    });
  });

  return { getState: function () { return state; }, refresh: applyVisual };
}

// ---------------------------------------------------------------------
// Fader / ride-history / meters - driven by real telemetry only.
//
// The fader sits on a FIXED absolute dB scale matching the TARGET knob's
// own range, not a scale centered on "zero correction". A dedicated
// faderTargetLine marks TARGET's current position on that scale, and the
// faderCap rides above/below it by the live correction amount - so pulling
// TARGET down visually pulls the fader's resting point down too, like a
// physical fader someone rode down to a quieter target.
// ---------------------------------------------------------------------
var TARGET_MIN_DB = -24.0;
var TARGET_MAX_DB = 6.0;
var ABSOLUTE_TICKS = [6, 0, -6, -12, -18, -24];
var currentTargetDb = 0.0;

var RIDE_HISTORY_SIZE = 240; // ~8s @ 30Hz
var rideHistory = new Float32Array(RIDE_HISTORY_SIZE);
var rideHistoryWritePos = 0;
var rideHistoryFilled = 0;

function topPercentForAbsoluteDb(db) {
  var clamped = Math.max(TARGET_MIN_DB, Math.min(TARGET_MAX_DB, db));
  return (TARGET_MAX_DB - clamped) / (TARGET_MAX_DB - TARGET_MIN_DB) * 100;
}

function updateTargetLine() {
  var line = document.getElementById("faderTargetLine");
  if (line) line.style.top = topPercentForAbsoluteDb(currentTargetDb) + "%";
}

function renderTicks() {
  var layer = document.getElementById("tickLayer");
  if (!layer) return;
  layer.innerHTML = "";
  ABSOLUTE_TICKS.forEach(function (t) {
    var topPct = topPercentForAbsoluteDb(t);
    var div = document.createElement("div");
    div.className = "tick" + (t === 0 ? " zero" : "");
    div.style.top = topPct + "%";
    div.textContent = (t > 0 ? "+" + t : t);
    layer.appendChild(div);
  });
}

function updateFaderPosition(correctionDb) {
  var cap = document.getElementById("faderCap");
  if (!cap) return;
  cap.style.top = topPercentForAbsoluteDb(currentTargetDb + correctionDb) + "%";
}

function drawRideHistory() {
  var canvas = document.getElementById("rideHistoryCanvas");
  if (!canvas) return;
  var track = document.getElementById("faderTrack");
  var dpr = window.devicePixelRatio || 1;
  var w = track.clientWidth, h = track.clientHeight;
  if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
    canvas.width = w * dpr;
    canvas.height = h * dpr;
  }
  var ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);

  var targetY = (topPercentForAbsoluteDb(currentTargetDb) / 100) * h;
  var n = rideHistoryFilled;
  if (n < 2) { requestAnimationFrame(drawRideHistory); return; }

  ctx.beginPath();
  for (var i = 0; i < n; i++) {
    var idx = (rideHistoryWritePos - n + i + RIDE_HISTORY_SIZE * 4) % RIDE_HISTORY_SIZE;
    var db = rideHistory[idx];
    var x = (i / (RIDE_HISTORY_SIZE - 1)) * w;
    var y = (topPercentForAbsoluteDb(currentTargetDb + db) / 100) * h;
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.strokeStyle = "rgba(212,175,106,0.85)";
  ctx.lineWidth = 1.5;
  ctx.stroke();

  // Bipolar fill toward the target line.
  ctx.lineTo((n - 1) / (RIDE_HISTORY_SIZE - 1) * w, targetY);
  ctx.lineTo(0, targetY);
  ctx.closePath();
  ctx.fillStyle = "rgba(212,175,106,0.10)";
  ctx.fill();

  requestAnimationFrame(drawRideHistory);
}

function pushRideHistory(db) {
  rideHistory[rideHistoryWritePos] = db;
  rideHistoryWritePos = (rideHistoryWritePos + 1) % RIDE_HISTORY_SIZE;
  rideHistoryFilled = Math.min(RIDE_HISTORY_SIZE, rideHistoryFilled + 1);
}

function dbLabel(db) {
  if (db <= -60) return "-inf";
  return (db >= 0 ? "+" : "") + db.toFixed(1);
}

// LED-segment meters: a fixed stack of discrete lens elements (bottom to
// top, via flex-direction:column-reverse) rather than a continuous fill -
// matches the physical meter_lens_off/on asset design (modular, repeatable).
var METER_SEGMENT_COUNT = 18;
var METER_HOT_SEGMENTS = 3; // topmost segments render as the red "hot" lens

function buildMeterSegments(shellId) {
  var shell = document.getElementById(shellId);
  if (!shell || shell.children.length > 0) return;
  for (var i = 0; i < METER_SEGMENT_COUNT; i++) {
    var lens = document.createElement("div");
    lens.className = "meterLens";
    shell.appendChild(lens);
  }
}

function updateMeter(shellId, valueId, db) {
  var shell = document.getElementById(shellId);
  var valueEl = document.getElementById(valueId);
  // Meter scale: -48dB .. +6dB mapped across the segment stack.
  var pct = Math.max(0, Math.min(1, (db + 48) / 54));
  var litCount = Math.round(pct * METER_SEGMENT_COUNT);
  if (shell) {
    var lenses = shell.children;
    for (var i = 0; i < lenses.length; i++) {
      var lit = i < litCount;
      lenses[i].classList.toggle("on", lit);
      lenses[i].classList.toggle("hot", lit && i >= METER_SEGMENT_COUNT - METER_HOT_SEGMENTS);
    }
  }
  if (valueEl) valueEl.textContent = dbLabel(db);
}

window.updateRideTelemetry = function (data) {
  if (!data) return;

  updateFaderPosition(data.correctionDb);
  document.getElementById("faderCap").classList.toggle("riding", !!data.riding);

  var rideValueEl = document.getElementById("rideValue");
  var rideStateEl = document.getElementById("rideState");
  if (rideValueEl) rideValueEl.textContent = (data.correctionDb >= 0 ? "+" : "") + data.correctionDb.toFixed(1) + " dB";
  if (rideStateEl) {
    rideStateEl.textContent = data.riding ? "RIDING" : "HOLD";
    rideStateEl.classList.toggle("riding", !!data.riding);
  }

  updateMeter("inMeterShell", "inMeterValue", data.inDb);
  updateMeter("outMeterShell", "outMeterValue", data.outDb);

  pushRideHistory(data.correctionDb);

  var statusEl = document.getElementById("sidechainStatus");
  if (statusEl) {
    var text = "Not Connected";
    var active = false;
    if (data.sidechainConnected) {
      if (data.sidechainPresence > 0.5) { text = "Connected – Active"; active = true; }
      else { text = "Connected – Silent"; }
    }
    statusEl.textContent = text;
    statusEl.classList.toggle("active", active);
  }
};

// ---------------------------------------------------------------------
// Presets
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
    presetIndex = (presetIndex - 1 + presetNames.length) % presetNames.length;
    callNative("applyPreset", presetIndex).then(function (result) {
      if (result && result.success) refreshPresetLabel();
    });
  });
  document.getElementById("presetNext").addEventListener("click", function () {
    presetIndex = (presetIndex + 1) % presetNames.length;
    callNative("applyPreset", presetIndex).then(function (result) {
      if (result && result.success) refreshPresetLabel();
    });
  });
}

// ---------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------
function init() {
  buildMeterSegments("inMeterShell");
  buildMeterSegments("outMeterShell");

  wireKnob("knobTarget", {
    kind: "slider", name: "target",
    formatValue: function (state) { return (state ? state.getScaledValue() : 0).toFixed(1) + " dB"; },
    onChange: function (state) {
      currentTargetDb = state ? state.getScaledValue() : 0;
      updateTargetLine();
    }
  });

  wireKnob("knobRange", {
    kind: "combobox", name: "range",
    formatValue: function (state, norm) {
      var choices = state && state.properties.choices ? state.properties.choices : ["±6 dB"];
      var idx = state ? state.getChoiceIndex() : 1;
      return choices[idx] || "±6 dB";
    }
  });

  wireKnob("knobSpeed", {
    kind: "slider", name: "speed",
    formatValue: function (state) {
      var pct = state ? state.getScaledValue() : 50;
      if (pct < 40) return "Slow";
      if (pct > 60) return "Fast";
      return "Natural";
    }
  });

  wireKnob("knobSensitivity", {
    kind: "slider", name: "sensitivity",
    formatValue: function (state) { return Math.round(state ? state.getScaledValue() : 50) + "%"; }
  });

  wireKnob("knobOutput", {
    kind: "slider", name: "output",
    formatValue: function (state) { var v = state ? state.getScaledValue() : 0; return (v >= 0 ? "+" : "") + v.toFixed(1) + " dB"; }
  });

  wireModeButtons();

  wireToggle("musicToggle", "music");
  wireToggle("writeToggle", "write", function (on) {
    document.getElementById("writeExportRow").style.display = on ? "block" : "none";
  });
  var bypassBtn = document.getElementById("bypassBtn");
  var bypassState = hasNative() ? getToggleState("bypass") : null;
  function refreshBypassVisual() {
    var on = bypassState ? bypassState.getValue() : false;
    bypassBtn.classList.toggle("on", !!on);
  }
  if (bypassState) {
    bypassState.valueChangedEvent.addListener(refreshBypassVisual);
    setTimeout(refreshBypassVisual, 60);
  }
  bypassBtn.addEventListener("click", function () {
    if (!bypassState) return;
    bypassState.setValue(!bypassState.getValue());
    refreshBypassVisual();
  });

  document.getElementById("exportRideBtn").addEventListener("click", function () {
    callNative("exportRideCsv");
  });

  initPresets();
  renderTicks();
  requestAnimationFrame(drawRideHistory);

  window.addEventListener("resize", renderTicks);
}

if (document.readyState === "loading")
  document.addEventListener("DOMContentLoaded", init);
else
  init();
