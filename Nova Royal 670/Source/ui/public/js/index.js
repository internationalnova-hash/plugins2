import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of ["setParam", "getParamsMeta", "jsError"]) nf[name] = getNativeFunction(name);

window.onerror = function (msg, src, line, col, err) {
  try { nf.jsError(String(msg) + " @" + line + ":" + col); } catch (e) {}
};

// Current #app CSS transform:scale() factor (kept in sync by fitStage()
// at the bottom of this file). Pointer events report clientX/clientY in
// real screen pixels, unaffected by a CSS transform on an ancestor, so
// drag handlers must divide by this to stay correctly calibrated once the
// window is no longer 1:1 with the design canvas.
let currentStageScale = 1;

let paramsMeta = {}; // id -> {type, min, max, choices, realValue, value(normalized)}
let currentRealValues = {}; // id -> real (denormalized) value

const $ = (id) => document.getElementById(id);

function formatValue(id, real) {
  switch (id) {
    case "input": case "output": case "acThreshold": case "dcThreshold": case "outputTrim":
      return (real >= 0 ? "+" : "") + real.toFixed(1) + " dB";
    case "hrmDrive": case "mix": case "link": case "weight":
    case "tubeSaturation": case "transformer": case "analogNoise":
      return Math.round(real) + " %";
    case "sidechainHpf":
      return Math.round(real) + " Hz";
    default:
      return String(real);
  }
}

function knobAngleForNormalized(v01) {
  return -135 + v01 * 270; // -135deg..+135deg, standard 270deg sweep
}

function setKnobVisual(el, min, max, real) {
  const norm = Math.max(0, Math.min(1, (real - min) / (max - min)));
  el.style.setProperty("--turn", `${knobAngleForNormalized(norm)}deg`);
}

function refreshReadout(id, real) {
  const el = document.querySelector(`[data-readout="${id}"]`);
  if (el) el.textContent = formatValue(id, real);
}

function refreshMicroLed(id) {
  const led = document.querySelector(`[data-indicator="${id}"]`);
  if (!led) return;
  const meta = paramsMeta[id];
  if (!meta) return;
  const real = currentRealValues[id] ?? meta.min;
  led.classList.toggle("active", real > (meta.min + (meta.max - meta.min) * 0.02));
}

function applyRealValue(id, real, pushToBackend) {
  currentRealValues[id] = real;
  const meta = paramsMeta[id];
  if (meta && meta.type !== "choice" && meta.type !== "bool") {
    document.querySelectorAll(`.knob[data-param="${id}"]`).forEach((el) => setKnobVisual(el, meta.min, meta.max, real));
  }
  refreshReadout(id, real);
  refreshMicroLed(id);
  if (pushToBackend) {
    if (meta && meta.type === "choice") nf.setParam(id, real);
    else if (meta && meta.type === "bool") nf.setParam(id, !!real);
    else nf.setParam(id, real);
  }
}

// ---- Continuous knob drag (vertical, standard catalog idiom) ----
function bindContinuousKnob(el) {
  const id = el.dataset.param;
  const min = parseFloat(el.dataset.min);
  const max = parseFloat(el.dataset.max);

  function getClientY(e) {
    if (e.touches && e.touches.length) return e.touches[0].clientY;
    return typeof e.clientY === "number" ? e.clientY : 0;
  }

  let dragging = null;

  el.addEventListener("pointerdown", (e) => {
    dragging = { startY: getClientY(e), startReal: currentRealValues[id] ?? min };
    if (el.setPointerCapture && e.pointerId !== undefined) {
      try { el.setPointerCapture(e.pointerId); } catch (err) {}
    }
    e.preventDefault();
  });

  window.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const dy = (dragging.startY - getClientY(e)) / (currentStageScale || 1);
    const range = max - min;
    const next = Math.max(min, Math.min(max, dragging.startReal + dy * range * 0.005));
    applyRealValue(id, next, true);
    e.preventDefault();
  }, { passive: false });

  window.addEventListener("pointerup", () => { dragging = null; });

  el.addEventListener("dblclick", () => {
    const meta = paramsMeta[id];
    if (meta && typeof meta.default === "number") applyRealValue(id, meta.default, true);
  });
}

// ---- Time Constant selector (knob visual + LED list, both drive the
// same choice param) ----
const TC_LABELS = ["0.1 SEC", "0.2 SEC", "0.6 SEC", "1.2 SEC", "2.0 SEC", "4.0 SEC"];

function buildTimeConstantUI() {
  const list = $("tcLedList");
  list.innerHTML = "";
  TC_LABELS.forEach((label, i) => {
    const row = document.createElement("div");
    row.className = "tcLedRow";
    row.dataset.index = String(i);
    row.innerHTML = `<div class="led"></div><div class="txt">${label}</div>`;
    row.addEventListener("click", () => selectTimeConstant(i));
    list.appendChild(row);
  });
}

// The knob visual has no data-param (it's a discrete 6-position selector,
// not a continuous knob), so it's excluded from bindContinuousKnob's
// generic .knob[data-param] loop and needs its own drag handler - without
// this it was visual-only, only updatable by clicking the LED list rows.
function bindTimeConstantKnobDrag(el) {
  function getClientY(e) {
    if (e.touches && e.touches.length) return e.touches[0].clientY;
    return typeof e.clientY === "number" ? e.clientY : 0;
  }

  const kStepPx = 26; // px of drag per discrete step, matches other knobs' feel
  let dragging = null;

  el.addEventListener("pointerdown", (e) => {
    dragging = { startY: getClientY(e), startIndex: currentRealValues.timeConstant ?? 0 };
    if (el.setPointerCapture && e.pointerId !== undefined) {
      try { el.setPointerCapture(e.pointerId); } catch (err) {}
    }
    e.preventDefault();
  });

  window.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const dy = (dragging.startY - getClientY(e)) / (currentStageScale || 1);
    const steps = Math.round(dy / kStepPx);
    const next = Math.max(0, Math.min(TC_LABELS.length - 1, dragging.startIndex + steps));
    if (next !== currentRealValues.timeConstant) selectTimeConstant(next);
    e.preventDefault();
  }, { passive: false });

  window.addEventListener("pointerup", () => { dragging = null; });
}

function selectTimeConstant(index) {
  currentRealValues.timeConstant = index;
  document.querySelectorAll(".tcLedRow").forEach((row) => {
    row.classList.toggle("active", parseInt(row.dataset.index, 10) === index);
  });
  const tcKnob = $("tcKnob");
  if (tcKnob) {
    const norm = index / (TC_LABELS.length - 1);
    setKnobVisual(tcKnob, 0, 1, norm);
  }
  nf.setParam("timeConstant", index);
}

// ---- Discrete toggle groups ----
function bindToggleGroup(containerEl, paramId, onSelect) {
  containerEl.querySelectorAll("button").forEach((btn) => {
    btn.addEventListener("click", () => {
      const value = parseInt(btn.dataset.value, 10);
      containerEl.querySelectorAll("button").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      currentRealValues[paramId] = value;
      nf.setParam(paramId, value);
      if (onSelect) onSelect(value);
    });
  });
}

function bindOnOffButton(btn, paramId, initial) {
  let state = initial;
  const render = () => {
    btn.classList.toggle("active", state);
    btn.textContent = state ? "ON" : "OFF";
  };
  render();
  btn.addEventListener("click", () => {
    state = !state;
    currentRealValues[paramId] = state;
    nf.setParam(paramId, state);
    render();
  });
}

function bindLinkButton() {
  const btn = $("linkBtn");
  let active = true;
  btn.addEventListener("click", () => {
    active = !active;
    btn.classList.toggle("active", active);
    const real = active ? 100 : 0;
    applyRealValue("link", real, true);
  });
}

function bindListenButton() {
  const btn = $("listenBtn");
  let active = false;
  btn.addEventListener("click", () => {
    active = !active;
    btn.classList.toggle("active", active);
    currentRealValues.sidechainListen = active;
    nf.setParam("sidechainListen", active);
  });
}

// ---- Power/bypass LED ----
function bindPowerLed() {
  const led = $("powerLed");
  led.addEventListener("click", () => {
    const bypassed = led.classList.contains("on"); // currently on = active = not bypassed
    led.classList.toggle("on", !bypassed ? true : false);
    const nowOn = led.classList.contains("on");
    currentRealValues.bypass = !nowOn;
    nf.setParam("bypass", !nowOn);
  });
}

// ---- GR ladder ----
const GR_STEPS_DB = [0, 2, 4, 6, 8, 10, 12, 16, 20]; // dB thresholds each of the 9 LEDs lights at, matching the printed scale

function updateGrLadder(gainReductionDb) {
  document.querySelectorAll(".grLed").forEach((led) => {
    const step = parseInt(led.dataset.step, 10);
    led.classList.toggle("active", gainReductionDb >= GR_STEPS_DB[step]);
  });
}

// ---- VU needles ----
// Real VU meters aren't linear across their printed scale, but a clamped
// linear map from -20..+3 VU onto a -50..+50 degree sweep (Nova 7even's
// proven approximation) reads correctly against this SVG face's printed
// tick positions.
function vuToAngle(vuDb) {
  const clamped = Math.max(-20, Math.min(3, vuDb));
  return ((clamped + 20) / 23) * 100 - 50;
}

// Forces a synchronous style recalc/repaint on an element right after a
// direct style/attribute mutation. Confirmed necessary in the real host's
// WKWebView (not reproducible in this session's Chromium-based test tool):
// classList.toggle()-driven changes (see updateGrLadder, proven working
// live in FL Studio) always repainted correctly, but direct style/
// attribute mutations on elements that force their own compositing layer
// - .tubeWindow (overflow:hidden + border-radius) and the VU needle <g>
// (nested inside a clip-path) - silently never repainted despite the
// mutation genuinely happening (verified: the values reaching this
// function are correct and change with real audio; only the on-screen
// result stayed frozen).
//
// offsetHeight (confirmed fixed the tube brightness) is an HTML box-model
// property and isn't meaningful on an SVG <g>, which is why it did NOT
// fix the needle. For SVG elements, force layout via getBBox() (the
// SVG-native equivalent) AND piggyback on the one repaint pathway proven
// to always work here - a classList no-op toggle, exploiting the same
// style-recalc trigger updateGrLadder already relies on - as a second,
// independent way to force the same result.
function forceRepaint(el) {
  if (!el) return;
  if (typeof el.getBBox === "function") { try { el.getBBox(); } catch (e) {} }
  else { void el.offsetHeight; }
  el.classList.add("_repaint");
  el.classList.remove("_repaint");
}

window.updateStatus = function (vuLeftOrMid, vuRightOrSide, gainReductionDb) {
  const leftNeedle = $("vuNeedleLeft");
  const rightNeedle = $("vuNeedleRight");
  // Plain HTML div + CSS style.transform now (see .vuNeedleDiv comment) -
  // no longer SVG geometry, so no getBBox()/attribute path needed here,
  // but forceRepaint is kept as a defensive no-cost safety net.
  if (leftNeedle) { leftNeedle.style.transform = `rotate(${vuToAngle(vuLeftOrMid).toFixed(2)}deg)`; forceRepaint(leftNeedle); }
  if (rightNeedle) { rightNeedle.style.transform = `rotate(${vuToAngle(vuRightOrSide).toFixed(2)}deg)`; forceRepaint(rightNeedle); }
  updateGrLadder(gainReductionDb);

  const tubes = $("tubeStrip");
  if (tubes) {
    // The tube photo itself shows already-lit filaments, so a translucent
    // glow layered on top of it (the old approach) can only ever look
    // marginally brighter - it can never look dim/off, which is why the
    // tubes read as "always lit regardless of audio." Modulating the
    // photo's own brightness via filter is what actually makes it look
    // idle at silence and hot under heavy gain reduction.
    // A real tube's brightness depends on both how hard it's being driven
    // (signal level - a starved input never lights it up, regardless of
    // saturation setting) and its own bias/gain (the Tube Saturation
    // knob) - gain reduction alone (the old formula) ignored both of
    // those, so a low-input/high-saturation signal looked identical to a
    // loud one. levelFactor reuses vuToAngle's own -20..+3 VU normalized
    // range so "how loud" reads consistently with the VU needles.
    const levelFactor = Math.max(0, Math.min(1, (vuLeftOrMid + 20) / 23));
    const saturationFactor = (currentRealValues.tubeSaturation ?? 35) / 100;
    const grFactor = Math.max(0, Math.min(1, gainReductionDb / 12));
    const drive = Math.max(0, Math.min(1, levelFactor * 0.45 + saturationFactor * 0.35 + grFactor * 0.20));
    tubes.style.filter = `brightness(${(0.45 + drive * 0.85).toFixed(2)}) saturate(${(0.6 + drive * 0.6).toFixed(2)})`;
    forceRepaint(tubes);
  }
  const glow = $("tubeGlow");
  if (glow) { glow.style.opacity = String(Math.max(0.15, Math.min(1, 0.15 + gainReductionDb / 14))); forceRepaint(glow); }
};

// ---- Boot ----
async function boot() {
  buildTimeConstantUI();
  const tcKnobEl = $("tcKnob");
  if (tcKnobEl) bindTimeConstantKnobDrag(tcKnobEl);

  const meta = await nf.getParamsMeta();
  (meta || []).forEach((m) => { paramsMeta[m.id] = m; });

  document.querySelectorAll(".knob[data-param]").forEach((el) => {
    bindContinuousKnob(el);
    const id = el.dataset.param;
    const m = paramsMeta[id];
    if (m) applyRealValue(id, m.realValue, false);
  });

  if (paramsMeta.timeConstant) selectTimeConstant(paramsMeta.timeConstant.realValue | 0);
  if (paramsMeta.stereoMode) {
    const idx = paramsMeta.stereoMode.realValue | 0;
    $("stereoModeToggle").querySelectorAll("button").forEach((b) => b.classList.toggle("active", parseInt(b.dataset.value, 10) === idx));
    currentRealValues.stereoMode = idx;
  }
  if (paramsMeta.oversampling) {
    const idx = paramsMeta.oversampling.realValue | 0;
    $("ovsGroup").querySelectorAll("button").forEach((b) => b.classList.toggle("active", parseInt(b.dataset.value, 10) === idx));
  }
  if (paramsMeta.autoGain) bindOnOffButton($("autoGainBtn"), "autoGain", !!paramsMeta.autoGain.realValue);
  if (paramsMeta.link) { currentRealValues.link = paramsMeta.link.realValue; $("linkBtn").classList.toggle("active", paramsMeta.link.realValue >= 50); }
  if (paramsMeta.bypass) $("powerLed").classList.toggle("on", !paramsMeta.bypass.realValue);
  if (paramsMeta.sidechainListen) $("listenBtn").classList.toggle("active", !!paramsMeta.sidechainListen.realValue);

  bindToggleGroup($("stereoModeToggle"), "stereoMode");
  bindToggleGroup($("ovsGroup"), "oversampling");
  bindLinkButton();
  bindListenButton();
  bindPowerLed();
}

boot();

// ---- stage scale-to-fit (same technique as Nova Tape/Nova Pitch) ----
(function () {
  let lastW = -1, lastH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    const scale = Math.min(w / 1580, h / 980);
    currentStageScale = scale || 1;
    const stage = document.getElementById("app");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();
