import { getNativeFunction } from "./juce/index.js";

const nativeSetParam = getNativeFunction("setParam");
const nativeJsError = getNativeFunction("jsError");

window.onerror = function (msg, src, line, col, err) {
  try {
    nativeJsError(String(msg) + " @" + line + ":" + col + (err && err.stack ? "\n" + err.stack : ""));
  } catch (e) { /* nothing else to do if the native bridge itself is unavailable */ }
};

(function () {
  const currentValues = { width: 56, tightness: 45, mix: 100 };
  let voicesFourMode = false;
  let bypassed = false;
  let activeDrag = null;

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, Number(value) || 0));
  }

  function pushParam(paramId, value) {
    try { nativeSetParam(paramId, value); }
    catch (error) { console.warn("setParam failed for " + paramId + ":", error && error.message ? error.message : error); }
  }

  function updateKnobVisual(paramId, value) {
    currentValues[paramId] = value;
    const knob = document.querySelector('[data-param="' + paramId + '"]');
    const readout = document.querySelector('[data-readout="' + paramId + '"]');
    if (readout) readout.textContent = Math.round(value) + "%";
    if (!knob) return;
    const min = Number(knob.dataset.min);
    const max = Number(knob.dataset.max);
    const normal = (clamp(value, min, max) - min) / ((max - min) || 1);
    const rotation = -135 + normal * 270;
    knob.style.setProperty("--turn", rotation + "deg");
  }

  function setParamValue(paramId, value, options) {
    const settings = options || {};
    updateKnobVisual(paramId, value);
    if (settings.pushToBackend) pushParam(paramId, value);
  }

  function getClientY(event) {
    if (!event) return 0;
    if (event.touches && event.touches.length > 0) return event.touches[0].clientY;
    if (event.changedTouches && event.changedTouches.length > 0) return event.changedTouches[0].clientY;
    return typeof event.clientY === "number" ? event.clientY : 0;
  }

  function startKnobDrag(knob, event) {
    if (typeof event.button === "number" && event.button !== 0) return;
    if (event.preventDefault) event.preventDefault();
    if (event.stopPropagation) event.stopPropagation();

    const paramId = knob.dataset.param;
    const min = Number(knob.dataset.min);
    const max = Number(knob.dataset.max);

    knob.classList.add("dragging");
    activeDrag = {
      paramId, knob, min, max,
      startY: getClientY(event),
      startValue: Number(currentValues[paramId] || min),
      pointerId: typeof event.pointerId !== "undefined" ? event.pointerId : null
    };

    if (typeof knob.setPointerCapture === "function" && typeof event.pointerId !== "undefined") {
      try { knob.setPointerCapture(event.pointerId); } catch (error) { /* not fatal */ }
    }
  }

  function updateActiveDrag(event) {
    if (!activeDrag) return;
    if (activeDrag.pointerId !== null && typeof event.pointerId !== "undefined" && event.pointerId !== activeDrag.pointerId) return;
    if (event.preventDefault) event.preventDefault();

    const delta = (activeDrag.startY - getClientY(event)) / (window.__novaDoublerScale || 1);
    const nextValue = activeDrag.startValue + delta * ((activeDrag.max - activeDrag.min) / 200);
    setParamValue(activeDrag.paramId, clamp(nextValue, activeDrag.min, activeDrag.max), { pushToBackend: true });
  }

  function finishActiveDrag(event) {
    if (!activeDrag) return;
    if (activeDrag.pointerId !== null && event && typeof event.pointerId !== "undefined" && event.pointerId !== activeDrag.pointerId) return;

    if (typeof activeDrag.knob.releasePointerCapture === "function" && activeDrag.pointerId !== null) {
      try { activeDrag.knob.releasePointerCapture(activeDrag.pointerId); } catch (error) { /* not fatal */ }
    }
    activeDrag.knob.classList.remove("dragging");
    activeDrag = null;
  }

  function initKnobs() {
    document.querySelectorAll(".knob[data-param], .hero-knob[data-param]").forEach((knob) => {
      const startHandler = (event) => startKnobDrag(knob, event);
      knob.addEventListener("pointerdown", startHandler);
      knob.addEventListener("touchstart", startHandler, { passive: false });
    });

    document.addEventListener("pointermove", updateActiveDrag);
    document.addEventListener("touchmove", updateActiveDrag, { passive: false });
    document.addEventListener("pointerup", finishActiveDrag);
    document.addEventListener("touchend", finishActiveDrag);
    document.addEventListener("pointercancel", finishActiveDrag);
    document.addEventListener("touchcancel", finishActiveDrag);
  }

  function setVoicesVisual(fourVoices) {
    voicesFourMode = fourVoices;
    document.querySelectorAll(".voices-btn").forEach((button) => {
      button.classList.toggle("active", (button.dataset.voices === "4") === fourVoices);
    });
    const readout = document.getElementById("voices-readout");
    if (readout) readout.textContent = (fourVoices ? "4" : "2") + " VOICES";
  }

  function initVoicesToggle() {
    document.querySelectorAll(".voices-btn").forEach((button) => {
      button.addEventListener("click", () => {
        const fourVoices = button.dataset.voices === "4";
        setVoicesVisual(fourVoices);
        pushParam("voicesFourMode", fourVoices);
      });
    });
  }

  function setBypassVisual(active) {
    bypassed = active;
    const btn = document.getElementById("bypass-btn");
    if (btn) btn.classList.toggle("active", active);
  }

  function initBypassButton() {
    const btn = document.getElementById("bypass-btn");
    if (!btn) return;
    btn.addEventListener("click", () => {
      setBypassVisual(!bypassed);
      pushParam("bypass", bypassed);
    });
  }

  // Live stereo-field ring around the hero WIDTH knob: two arcs sweep out
  // from the top, one per channel, driven by the REAL post-mix spread
  // measured by NovaStereoFieldAnalyzer (pushed ~30x/sec from the audio
  // thread's own analysis) - never a fabricated/decorative animation.
  window.updateStereoField = function (leftRms, rightRms, midRms, sideRms, correlation, spread) {
    // Square-root response, not linear: a natural-sounding doubler
    // deliberately avoids heavy decorrelation (that's what keeps it from
    // sounding like a flanger/chorus), so real spread values sit fairly
    // low/moderate most of the time. A linear length mapping left the arc
    // reading as a thin sliver even with genuine width applied - sqrt
    // gives a much bigger visual response low-to-mid range while still
    // topping out at the same full sweep for fully decorrelated content.
    const spreadNorm = clamp(Number(spread) || 0, 0, 100) / 100;
    const half = Math.sqrt(spreadNorm) * 50;
    const arcL = document.getElementById("field-arc-l");
    const arcR = document.getElementById("field-arc-r");
    if (arcL) arcL.setAttribute("stroke-dasharray", half.toFixed(1) + " 100");
    if (arcR) arcR.setAttribute("stroke-dasharray", half.toFixed(1) + " 100");

    // Always fully saturated color - length alone (above) conveys how
    // much spread is present. Dimming color by level on top of that made
    // the arc read as washed-out/pale at typical real-world signal levels
    // instead of a clean, vivid purple/gold.
    if (arcL) arcL.style.opacity = "1";
    if (arcR) arcR.style.opacity = "1";
  };

  function bootUi() {
    initKnobs();
    initVoicesToggle();
    initBypassButton();

    setParamValue("width", currentValues.width, { pushToBackend: false });
    setParamValue("tightness", currentValues.tightness, { pushToBackend: false });
    setParamValue("mix", currentValues.mix, { pushToBackend: false });
    setVoicesVisual(false);
    setBypassVisual(false);
    window.updateStereoField(0, 0, 0, 0, 1, 0);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bootUi, { once: true });
  } else {
    bootUi();
  }
}());

// ---- stage scale-to-fit ----
// Plain JS arithmetic, not CSS min()/calc() (see the #stage comment in
// index.html for why) - and polled, not just resize-event-driven, since
// this embedded WebView doesn't reliably fire resize events on a
// native host-driven setBounds(). Same technique as Nova Tape/Nova Pitch.
(function () {
  let lastFitW = -1, lastFitH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastFitW && h === lastFitH) return;
    lastFitW = w; lastFitH = h;
    const scale = Math.min(w / 820, h / 540);
    // Exposed globally (this fitStage IIFE is a separate scope from the
    // knob-drag code) so drag handlers can compensate raw pointer-pixel
    // deltas for the current visual scale - pointer events report real
    // screen pixels, unaffected by this CSS transform.
    window.__novaDoublerScale = scale || 1;
    const stage = document.getElementById("stage");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
}());
