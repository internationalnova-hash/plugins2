import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of [
  "setParam", "getParamsMeta",
  "abGetState", "abCaptureActive", "abToggleAndRecall", "abCopyActiveToOther",
  "getPresetsMeta", "loadPreset", "savePreset",
  "jsError",
]) nf[name] = getNativeFunction(name);

window.onerror = function (msg, src, line, col, err) {
  try { nf.jsError(String(msg) + " @" + line + ":" + col); } catch (e) {}
};

function $(id) { return document.getElementById(id); }

const KEY_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const SCALE_NAMES = ["Chromatic", "Major", "Minor", "Pentatonic", "Blues"];

const currentValues = {}; // paramId -> real value (as currently reflected in the UI)
let presetsMeta = [];
let currentPresetIndex = 0;

// ── Knobs (rotary, vertical-drag, immediate visual follow) ───────────
function paramRange(el) {
  return { min: Number(el.dataset.min), max: Number(el.dataset.max) };
}

function displayValue(el, realValue) {
  // "Retune Speed" is stored internally as "tolerance" (0=fast,100=slow)
  // but reads on-screen as a speed dial (low number=slow, high=fast) -
  // the DSP semantics stay stable under the same param id; only the
  // on-screen number/knob position are flipped.
  if (el.dataset.invert === "1") {
    const { min, max } = paramRange(el);
    return min + max - realValue;
  }
  return realValue;
}

function realFromDisplay(el, dispValue) {
  if (el.dataset.invert === "1") {
    const { min, max } = paramRange(el);
    return min + max - dispValue;
  }
  return dispValue;
}

function formatKnobValue(paramId, realValue) {
  if (paramId === "vibratoRate") return realValue.toFixed(1) + " Hz";
  if (paramId === "outputGain") return (realValue >= 0 ? "+" : "") + realValue.toFixed(1) + " dB";
  if (paramId === "formantWarp") return (realValue >= 0 ? "+" : "") + Math.round(realValue);
  return Math.round(realValue).toString();
}

function knobValueLabelEl(knobEl) {
  const id = knobEl.id.replace(/Knob$/, "");
  return $(id + "Value");
}

function setKnobVisual(knobEl, realValue) {
  const { min, max } = paramRange(knobEl);
  const disp = displayValue(knobEl, realValue);
  const dispMin = Math.min(displayValue(knobEl, min), displayValue(knobEl, max));
  const dispMax = Math.max(displayValue(knobEl, min), displayValue(knobEl, max));
  const pct = (disp - dispMin) / (dispMax - dispMin || 1);
  knobEl.style.setProperty("--pct", String(Math.max(0, Math.min(1, pct))));

  if (knobEl.dataset.bipolar === "1") {
    const zeroPct = (0 - min) / (max - min);
    knobEl.style.setProperty("--zeroPct", String(zeroPct));
    knobEl.classList.toggle("pos", realValue >= 0);
    knobEl.classList.toggle("neg", realValue < 0);
  }

  const pointer = knobEl.querySelector(".knobPointer");
  if (pointer) pointer.style.transform = `rotate(${-135 + pct * 270}deg)`;

  const label = knobValueLabelEl(knobEl);
  if (label) label.textContent = formatKnobValue(knobEl.dataset.param, disp);
}

function initKnob(knobEl) {
  const paramId = knobEl.dataset.param;
  const { min, max } = paramRange(knobEl);
  let dragState = null;

  const startDrag = (event) => {
    if (typeof event.button === "number" && event.button !== 0) return;
    event.preventDefault();
    const startValue = Number(currentValues[paramId] ?? min);
    dragState = { startY: getClientY(event), startDisp: displayValue(knobEl, startValue) };
    if (knobEl.setPointerCapture && event.pointerId !== undefined) {
      try { knobEl.setPointerCapture(event.pointerId); } catch (e) {}
    }
  };

  const moveDrag = (event) => {
    if (!dragState) return;
    event.preventDefault();
    const delta = (dragState.startY - getClientY(event)) / (currentStageScale || 1);
    // Full knob throw (~220px) covers the full range - direct, unsmoothed
    // 1:1 visual follow (DSP-side smoothing happens in C++, invisibly).
    // Dragging happens in on-screen "display" space so up always turns the
    // knob right, even for inverted params like Retune Speed.
    const range = max - min;
    const dispMin = Math.min(displayValue(knobEl, min), displayValue(knobEl, max));
    const dispMax = Math.max(displayValue(knobEl, min), displayValue(knobEl, max));
    const nextDisp = Math.max(dispMin, Math.min(dispMax, dragState.startDisp + delta * range * 0.006));
    const next = realFromDisplay(knobEl, nextDisp);
    currentValues[paramId] = next;
    setKnobVisual(knobEl, next);
    nf.setParam(paramId, next);
  };

  const endDrag = () => { dragState = null; };

  knobEl.addEventListener("pointerdown", startDrag);
  window.addEventListener("pointermove", moveDrag, { passive: false });
  window.addEventListener("pointerup", endDrag);
}

function getClientY(event) {
  if (event.touches && event.touches.length) return event.touches[0].clientY;
  return typeof event.clientY === "number" ? event.clientY : 0;
}

// ── Key / Scale (click-to-open dropdown popovers) ──────────────────────
function initStepControl(buttonEl, paramId, names, valueElId) {
  const container = buttonEl.closest(".stepControl");
  const popover = document.createElement("div");
  popover.className = "stepPopover";

  const items = names.map((name, i) => {
    const item = document.createElement("button");
    item.type = "button";
    item.className = "stepPopoverItem";
    item.textContent = name;
    item.addEventListener("click", (e) => {
      e.stopPropagation();
      currentValues[paramId] = i;
      $(valueElId).textContent = name;
      nf.setParam(paramId, i);
      updateSelected();
      close();
    });
    popover.appendChild(item);
    return item;
  });
  container.appendChild(popover);

  const updateSelected = () => {
    const cur = Math.round(Number(currentValues[paramId] ?? 0));
    items.forEach((item, i) => item.classList.toggle("is-selected", i === cur));
  };

  const open = () => { updateSelected(); popover.classList.add("is-open"); };
  const close = () => popover.classList.remove("is-open");

  buttonEl.addEventListener("click", (e) => {
    e.stopPropagation();
    if (popover.classList.contains("is-open")) close();
    else open();
  });
  document.addEventListener("pointerdown", (e) => {
    if (popover.classList.contains("is-open") && !popover.contains(e.target) && e.target !== buttonEl)
      close();
  });
}

// ── Bypass ─────────────────────────────────────────────────────────────
function initBypass() {
  const btn = $("bypassBtn");
  btn.addEventListener("click", () => {
    const next = !btn.classList.contains("is-active");
    btn.classList.toggle("is-active", next);
    btn.setAttribute("aria-pressed", String(next));
    currentValues.bypass = next ? 1 : 0;
    nf.setParam("bypass", next ? 1 : 0);
  });
}

// ── A/B ────────────────────────────────────────────────────────────────
async function refreshAbButtons() {
  const state = await nf.abGetState();
  if (!state) return;
  $("abBtnA").classList.toggle("is-active", state.activeSlot === "A");
  $("abBtnB").classList.toggle("is-active", state.activeSlot === "B");
}

function initAbButtons() {
  $("abBtnA").addEventListener("click", async () => {
    const state = await nf.abGetState();
    if (state && state.activeSlot !== "A") {
      await nf.abCaptureActive();
      await nf.abToggleAndRecall();
      await refreshAll();
    }
  });
  $("abBtnB").addEventListener("click", async () => {
    const state = await nf.abGetState();
    if (state && state.activeSlot !== "B") {
      await nf.abCaptureActive();
      await nf.abToggleAndRecall();
      await refreshAll();
    }
  });
}

// ── Save popover ───────────────────────────────────────────────────────
function initSavePopover() {
  const popover = $("savePopover");
  const input = $("saveNameInput");

  const open = () => {
    input.value = $("presetName").textContent;
    popover.classList.add("is-open");
    setTimeout(() => { input.focus(); input.select(); }, 0);
  };
  const close = () => popover.classList.remove("is-open");
  const confirmSave = async () => {
    const name = input.value.trim() || "User Preset";
    const newIndex = await nf.savePreset(name);
    close();
    presetsMeta = (await nf.getPresetsMeta()) || presetsMeta;
    if (typeof newIndex === "number") {
      currentPresetIndex = newIndex;
      $("presetName").textContent = presetsMeta[currentPresetIndex]?.name ?? name;
    }
  };

  $("saveBtn").addEventListener("click", open);
  $("saveCancel").addEventListener("click", close);
  $("saveConfirm").addEventListener("click", confirmSave);
  input.addEventListener("keydown", (e) => {
    if (e.key === "Enter") confirmSave();
    if (e.key === "Escape") close();
  });
  document.addEventListener("pointerdown", (e) => {
    if (popover.classList.contains("is-open") && !popover.contains(e.target) && e.target !== $("saveBtn"))
      close();
  });
}

// ── Settings / menu popover ───────────────────────────────────────────
function initSettingsPopover() {
  const popover = $("settingsPopover");
  const toggle = () => popover.classList.toggle("is-open");
  $("settingsBtn").addEventListener("click", toggle);
  $("menuBtn").addEventListener("click", toggle);
  document.addEventListener("pointerdown", (e) => {
    if (popover.classList.contains("is-open") && !popover.contains(e.target)
        && e.target !== $("settingsBtn") && e.target !== $("menuBtn"))
      popover.classList.remove("is-open");
  });
}

// ── Presets ────────────────────────────────────────────────────────────
async function applyPresetToUI(index) {
  currentPresetIndex = index;
  const meta = presetsMeta[index];
  if (meta) $("presetName").textContent = meta.name;
  await nf.loadPreset(index);
  await refreshAll();
}

function initPresetNav() {
  $("presetPrev").addEventListener("click", () => {
    if (!presetsMeta.length) return;
    applyPresetToUI((currentPresetIndex - 1 + presetsMeta.length) % presetsMeta.length);
  });
  $("presetNext").addEventListener("click", () => {
    if (!presetsMeta.length) return;
    applyPresetToUI((currentPresetIndex + 1) % presetsMeta.length);
  });
}

// ── Hero display (live pitch data) ────────────────────────────────────
const waveHistory = new Array(96).fill(0);
let waveCtx = null;

function drawWaveform() {
  const canvas = $("waveCanvas");
  if (!canvas) return;
  if (!waveCtx) waveCtx = canvas.getContext("2d");
  const w = canvas.width, h = canvas.height;
  waveCtx.clearRect(0, 0, w, h);

  const n = waveHistory.length;
  const barW = w / n;
  const grad = waveCtx.createLinearGradient(0, 0, w, 0);
  grad.addColorStop(0, "#9c7dff");
  grad.addColorStop(1, "#4ea9ff");
  waveCtx.fillStyle = grad;

  for (let i = 0; i < n; i++) {
    const level = Math.max(0, Math.min(1, waveHistory[i] * 4.0));
    // A flat, low baseline at idle/near-silence is the honest reading (no
    // signal in), not a decorative animation - keep it visible enough to
    // read as "live and listening" rather than "broken/blank" though.
    const barH = Math.max(3, level * h);
    const x = i * barW;
    const y = (h - barH) / 2;
    waveCtx.globalAlpha = 0.45 + level * 0.55;
    waveCtx.fillRect(x, y, Math.max(1, barW - 1.5), barH);
  }
  waveCtx.globalAlpha = 1;
}

window.receiveDSP = function (data) {
  if (!data) return;

  if (Array.isArray(data.levels) && data.levels.length) {
    const n = waveHistory.length;
    const src = data.levels;
    for (let i = 0; i < n; i++) {
      const srcIdx = Math.floor((i / n) * src.length);
      waveHistory[i] = src[srcIdx] ?? 0;
    }
    drawWaveform();
  }

  const confidence = Math.max(0, Math.min(1, data.confidence ?? 0));
  const fillPath = $("heroRingFillPath");
  if (fillPath) fillPath.style.opacity = String(0.55 + confidence * 0.45);

  if (data.voiced && data.noteName) {
    $("heroNoteName").textContent = data.noteName;
    $("heroOctave").textContent = String(data.octave ?? "");
    const cents = Math.round(data.cents ?? 0);
    const centsEl = $("heroCents");
    centsEl.textContent = (cents > 0 ? "+" : "") + cents;
    centsEl.classList.toggle("in-tune", Math.abs(cents) <= 5);
  }
};

// ── Full refresh from real processor state ────────────────────────────
async function refreshAll() {
  const metas = (await nf.getParamsMeta()) || [];
  const byId = {};
  metas.forEach((m) => { byId[m.id] = m; });

  document.querySelectorAll(".knob[data-param]").forEach((knobEl) => {
    const id = knobEl.dataset.param;
    if (!byId[id]) return;
    currentValues[id] = byId[id].value;
    setKnobVisual(knobEl, byId[id].value);
  });

  if (byId.key) {
    currentValues.key = byId.key.value;
    $("keyValue").textContent = KEY_NAMES[Math.round(byId.key.value)] ?? "C";
  }
  if (byId.scale) {
    currentValues.scale = byId.scale.value;
    $("scaleValue").textContent = SCALE_NAMES[Math.round(byId.scale.value)] ?? "Major";
  }
  if (byId.bypass) {
    const active = !!byId.bypass.value;
    currentValues.bypass = active ? 1 : 0;
    $("bypassBtn").classList.toggle("is-active", active);
    $("bypassBtn").setAttribute("aria-pressed", String(active));
  }

  await refreshAbButtons();
}

// ---- stage scale-to-fit ----
// Plain JS arithmetic, not CSS min()/calc() (see the #stage comment in
// index.html for why) - and polled, not just resize-event-driven, since
// this embedded WebView doesn't reliably fire resize events on a
// native host-driven setBounds(). Same technique as Nova Tape.
let lastFitW = -1, lastFitH = -1;
let currentStageScale = 1;
function fitStage() {
  const w = document.documentElement.clientWidth || window.innerWidth || 0;
  const h = document.documentElement.clientHeight || window.innerHeight || 0;
  if (w <= 0 || h <= 0) return;
  if (w === lastFitW && h === lastFitH) return;
  lastFitW = w; lastFitH = h;
  const scale = Math.min(w / 1100, h / 660);
  currentStageScale = scale || 1;
  const stage = $("stage");
  if (stage) stage.style.transform = `scale(${scale})`;
}
fitStage();
window.addEventListener("resize", fitStage);
setInterval(fitStage, 300);

(function boot() {
  document.querySelectorAll(".knob[data-param]").forEach(initKnob);
  initStepControl($("keyBtn"), "key", KEY_NAMES, "keyValue");
  initStepControl($("scaleBtn"), "scale", SCALE_NAMES, "scaleValue");
  initBypass();
  initAbButtons();
  initSavePopover();
  initSettingsPopover();
  initPresetNav();
  drawWaveform(); // idle baseline, before the first live telemetry tick arrives

  (async () => {
    presetsMeta = (await nf.getPresetsMeta()) || [];
    if (presetsMeta.length) $("presetName").textContent = presetsMeta[0].name;
    await refreshAll();
  })();
})();
