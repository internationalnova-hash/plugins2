import { getSliderState, getToggleState, getComboBoxState, getNativeFunction } from "./juce/index.js";

function hasNative() {
  return typeof window.__JUCE__ !== "undefined";
}

// ---------------------------------------------------------------------
// Stage scale-to-fit (same technique as Nova Glue/Nova Glow/Nova Ride) -
// initial open size is set below 100% of this design canvas from day one
// (PluginEditor.cpp), never at full scale.
// ---------------------------------------------------------------------
(function () {
  let lastW = -1, lastH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    const scale = Math.min(w / 1080, h / 720);
    window.__novaBendScale = scale || 1;
    const stage = document.getElementById("app");
    if (stage) {
      // transform:scale() does NOT shrink an element's LAYOUT footprint for
      // its parent's purposes - only its paint - so centering this
      // (always-1080x720-in-layout) box via flex/place-content on the
      // parent breaks the moment the host resizes to an aspect ratio other
      // than 1080:720 (the unscaled 720-tall box can exceed a short-and-
      // wide window, and flex centering an oversized item produces a
      // negative/wrong offset that cuts content off screen - confirmed
      // directly at 1600x500, where a button's bottom edge rendered well
      // past the actual viewport bottom). Compute the centering offset
      // explicitly here instead, so it's correct for any aspect ratio.
      const scaledW = 1080 * scale, scaledH = 720 * scale;
      const offsetX = (w - scaledW) / 2;
      const offsetY = (h - scaledH) / 2;
      stage.style.transform = "translate(" + offsetX + "px, " + offsetY + "px) scale(" + scale + ")";
    }
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();

// ---------------------------------------------------------------------
// Runtime SVG tick-ring rendering (never baked into knob PNGs) - same
// -135..+135deg / 270deg sweep convention used catalog-wide.
// ---------------------------------------------------------------------
function addMinorTicks(svg, c, innerR, outerR, count) {
  for (let i = 0; i <= count; i++) {
    const norm = i / count;
    const angleDeg = -135 + norm * 270;
    const rad = (Math.PI / 180) * angleDeg;
    const sin = Math.sin(rad), cos = Math.cos(rad);
    const x1 = c + innerR * sin, y1 = c - innerR * cos;
    const x2 = c + outerR * sin, y2 = c - outerR * cos;
    const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#5c4a22");
    line.setAttribute("stroke-width", "1");
    svg.appendChild(line);
  }
}

function renderKnobScale(containerId, min, max, step) {
  const container = document.getElementById(containerId);
  if (!container) return;

  const box = 192, c = box / 2;
  const tickInnerR = 68, tickOuterR = 86, numberR = 102;

  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);

  addMinorTicks(svg, c, 70, 82, Math.round((max - min) / (step / 3)));

  for (let v = min; v <= max + 1e-6; v += step) {
    const norm = (v - min) / (max - min);
    const angleDeg = -135 + norm * 270;
    const rad = (Math.PI / 180) * angleDeg;
    const sin = Math.sin(rad), cos = Math.cos(rad);

    const x1 = c + tickInnerR * sin, y1 = c - tickInnerR * cos;
    const x2 = c + tickOuterR * sin, y2 = c - tickOuterR * cos;
    const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", x1); line.setAttribute("y1", y1);
    line.setAttribute("x2", x2); line.setAttribute("y2", y2);
    line.setAttribute("stroke", "#e9e4d8");
    line.setAttribute("stroke-width", "2");
    line.setAttribute("stroke-linecap", "round");
    svg.appendChild(line);

    const tx = c + numberR * sin, ty = c - numberR * cos;
    const text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", "13");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", "#e9e4d8");
    text.textContent = (v > 0 ? "+" : "") + Math.round(v);
    svg.appendChild(text);
  }

  container.innerHTML = "";
  container.appendChild(svg);
}

function renderSmallKnobScale(containerId) {
  const container = document.getElementById(containerId);
  if (!container) return;
  const box = 128, c = box / 2;
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);
  addMinorTicks(svg, c, 46, 58, 32);
  container.innerHTML = "";
  container.appendChild(svg);
}

// ---------------------------------------------------------------------
// Continuous knobs (Pitch, Formant, Mix).
// ---------------------------------------------------------------------
function wireKnob(elId, opts) {
  const el = document.getElementById(elId);
  if (!el) return null;
  const dial = el.querySelector(".knobDial");

  const state = hasNative() ? getSliderState(opts.name) : null;
  let dragging = false, startY = 0, startNorm = 0;

  function currentNorm() {
    return state ? state.getNormalisedValue() : 0.5;
  }

  // displayOverride (Formant only, see below) shows where the knob is
  // ACTUALLY acting once Link folds the live pitch shift into it - the
  // underlying parameter value (what dragging manipulates) never changes
  // because of this, only the on-screen angle. Suppressed mid-drag so what
  // you see always matches what your own motion is currently setting -
  // otherwise the knob would visibly jump the instant you press down.
  function applyVisual() {
    const base = currentNorm();
    const displayed = (!dragging && opts.displayOverride) ? opts.displayOverride(base) : base;
    const norm = Math.max(0, Math.min(1, displayed));
    const angle = -135 + norm * 270;
    if (dial) dial.style.setProperty("--angle", angle + "deg");
    if (opts.onChange) opts.onChange(state, norm);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  const dragRangePx = opts.dragRangePx || 150;

  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    dragging = true;
    startY = e.clientY;
    startNorm = currentNorm();
    try { el.setPointerCapture(e.pointerId); } catch (err) {}
    if (state && state.sliderDragStarted) state.sliderDragStarted();
  });
  el.addEventListener("pointermove", (e) => {
    if (!dragging || !state) return;
    const scale = window.__novaBendScale || 1;
    const deltaNorm = ((startY - e.clientY) / scale) / dragRangePx;
    const newNorm = Math.max(0, Math.min(1, startNorm + deltaNorm));
    state.setNormalisedValue(newNorm);
    applyVisual();
  });
  function endDrag() {
    if (!dragging) return;
    dragging = false;
    if (state && state.sliderDragEnded) state.sliderDragEnded();
    applyVisual(); // resume showing the Link-tracked position, if any
  }
  el.addEventListener("pointerup", endDrag);
  el.addEventListener("pointercancel", endDrag);

  return { refresh: applyVisual, getState: () => state };
}

// ---------------------------------------------------------------------
// 2/3-button mode groups (bool or choice-backed).
// ---------------------------------------------------------------------
function wireBoolButtonGroup(containerId, name, onChangeCb) {
  const container = document.getElementById(containerId);
  if (!container) return;
  const buttons = Array.prototype.slice.call(container.querySelectorAll(".modebtn"));
  const state = hasNative() ? getToggleState(name) : null;

  function applyVisual() {
    const on = state ? state.getValue() : false;
    buttons.forEach((b) => b.classList.toggle("active", (parseInt(b.dataset.idx, 10) === 1) === on));
    if (onChangeCb) onChangeCb(on);
  }

  if (state) { state.valueChangedEvent.addListener(applyVisual); setTimeout(applyVisual, 60); }
  else applyVisual();

  buttons.forEach((b) => {
    b.addEventListener("click", () => {
      if (!state) return;
      state.setValue(parseInt(b.dataset.idx, 10) === 1);
      applyVisual();
    });
  });
}

function wireChoiceButtonGroup(containerId, name) {
  const container = document.getElementById(containerId);
  if (!container) return;
  const buttons = Array.prototype.slice.call(container.querySelectorAll(".modebtn"));
  const state = hasNative() ? getComboBoxState(name) : null;

  function applyVisual() {
    const idx = state ? state.getChoiceIndex() : 1;
    buttons.forEach((b) => b.classList.toggle("active", parseInt(b.dataset.idx, 10) === idx));
  }

  if (state) { state.valueChangedEvent.addListener(applyVisual); setTimeout(applyVisual, 60); }
  else applyVisual();

  buttons.forEach((b) => {
    b.addEventListener("click", () => {
      if (!state) return;
      state.setChoiceIndex(parseInt(b.dataset.idx, 10));
      applyVisual();
    });
  });
}

function wireSelect(selectId, name) {
  const el = document.getElementById(selectId);
  if (!el) return;
  const state = hasNative() ? getComboBoxState(name) : null;

  function applyVisual() {
    el.selectedIndex = state ? state.getChoiceIndex() : 0;
  }

  if (state) { state.valueChangedEvent.addListener(applyVisual); setTimeout(applyVisual, 60); }
  else applyVisual();

  el.addEventListener("change", () => {
    if (!state) return;
    state.setChoiceIndex(el.selectedIndex);
  });
}

// ---------------------------------------------------------------------
// Real (not decorative) waveform telemetry - pushed from PluginEditor's
// Timer via evaluateJavascript with actual output-level history.
// ---------------------------------------------------------------------
window.updateWaveform = function (samples) {
  const poly = document.getElementById("wavePoly");
  if (!poly || !samples || !samples.length) return;
  const w = 130, h = 34, mid = h / 2;
  const n = samples.length;
  let pts = "";
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * w;
    const amp = Math.max(0, Math.min(1, samples[i] * 7));
    const y = mid - amp * mid;
    pts += x.toFixed(1) + "," + y.toFixed(1) + " ";
  }
  poly.setAttribute("points", pts.trim());
};

// ---------------------------------------------------------------------
// Preset browser - a preset is just a set of APVTS values applied
// natively (NovaBendPresetManager::applyToApvts); once applied, every
// knob/button already refreshes itself via its own relay's
// valueChangedEvent listener (wired in wireKnob/wireBoolButtonGroup/
// wireSelect above), so no separate visual-refresh pass is needed here -
// only the preset name field needs an explicit update.
// ---------------------------------------------------------------------
const nf = hasNative() ? {
  listPresets: getNativeFunction("listPresets"),
  getCurrentPresetIndex: getNativeFunction("getCurrentPresetIndex"),
  loadPreset: getNativeFunction("loadPreset"),
  loadNextPreset: getNativeFunction("loadNextPreset"),
  loadPreviousPreset: getNativeFunction("loadPreviousPreset"),
  savePreset: getNativeFunction("savePreset"),
} : null;

let presets = [];

async function refreshPresetDisplay() {
  if (!nf) return;
  const index = await nf.getCurrentPresetIndex();
  const preset = presets[index];
  const nameEl = document.getElementById("presetName");
  if (nameEl) nameEl.textContent = preset ? preset.name : "Default";
}

function bindPresetBrowser() {
  if (!nf) return;
  document.getElementById("presetPrev").addEventListener("click", async () => {
    await nf.loadPreviousPreset();
    await refreshPresetDisplay();
  });
  document.getElementById("presetNext").addEventListener("click", async () => {
    await nf.loadNextPreset();
    await refreshPresetDisplay();
  });
}

function bindSaveModal() {
  const modal = document.getElementById("saveModal");
  const nameInput = document.getElementById("presetNameInput");

  document.getElementById("saveBtn").addEventListener("click", () => {
    nameInput.value = "";
    modal.classList.add("open");
    nameInput.focus();
  });
  document.getElementById("saveCancel").addEventListener("click", () => {
    modal.classList.remove("open");
  });
  document.getElementById("saveConfirm").addEventListener("click", async () => {
    const name = nameInput.value.trim();
    if (!name || !nf) { modal.classList.remove("open"); return; }
    const newIndex = await nf.savePreset(name);
    modal.classList.remove("open");
    if (newIndex >= 0) {
      presets = (await nf.listPresets()) || [];
      await refreshPresetDisplay();
    }
  });
}

function initControls() {
  renderKnobScale("ticksPitch", -12, 12, 6);
  renderKnobScale("ticksFormant", -12, 12, 6);
  renderSmallKnobScale("ticksMix");

  const pitchKnob = wireKnob("knobPitch", { name: "pitch", dragRangePx: 160 });
  // Link folds the current pitch shift into the formant warp the DSP
  // actually applies (see NovaBendEngine::process) without touching the
  // Formant parameter's own value - this display-only offset is the
  // knob's only visible sign that Link is doing anything, since dragging
  // Formant always still edits its raw value regardless of Link state.
  const formantKnob = wireKnob("knobFormant", {
    name: "formant", dragRangePx: 160,
    displayOverride: (base) => {
      if (!hasNative() || !pitchKnob) return base;
      const linkState = getToggleState("link");
      if (!linkState.getValue()) return base;
      return base + (pitchKnob.getState().getNormalisedValue() - 0.5);
    },
  });
  wireKnob("knobMix", { name: "mix", dragRangePx: 130 });

  if (hasNative() && pitchKnob && formantKnob) {
    pitchKnob.getState().valueChangedEvent.addListener(formantKnob.refresh);
    getToggleState("link").valueChangedEvent.addListener(formantKnob.refresh);
  }

  wireBoolButtonGroup("flattenGroup", "flatten", (on) => {
    document.getElementById("noteGrp").classList.toggle("dim", !on);
  });
  wireBoolButtonGroup("linkGroup", "link");
  wireChoiceButtonGroup("qualityGroup", "quality");
  wireBoolButtonGroup("bypassGroup", "bypass");

  wireSelect("noteSelect", "flatten_note");
  wireSelect("octaveSelect", "flatten_octave");

  // Presets: real factory + user library via NovaBendPresetManager.
  // Settings icon remains a placeholder (no settings surface in V1).
  bindPresetBrowser();
  bindSaveModal();

  if (nf) {
    (async () => {
      presets = (await nf.listPresets()) || [];
      await refreshPresetDisplay();
    })();
  }
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initControls, { once: true });
} else {
  initControls();
}
