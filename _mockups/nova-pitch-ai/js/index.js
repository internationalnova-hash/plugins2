import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of [
  "setParam", "getParamsMeta",
  "abGetState", "abCaptureActive", "abToggleAndRecall", "abCopyActiveToOther",
  "getPresetsMeta", "loadPreset", "savePreset",
  "jsError", "getCompact", "setCompact",
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
  if (paramId === "harmWidth") return Math.round(realValue) + "%";
  if (paramId === "harmLead") return (realValue >= 0 ? "+" : "") + realValue.toFixed(1) + " dB";
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
function initStepControl(buttonEl, paramId, names, valueElId, onPick) {
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
      if (onPick) onPick(i);
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

// ── Tracking mode (low latency for recording) ─────────────────────────
function renderTracking() {
  const on = !!currentValues.tracking;
  const b = $("trackBtn");
  b.classList.toggle("is-active", on);
  b.textContent = on ? "ON · 4 ms" : "OFF";
}
function initTracking() {
  $("trackBtn").addEventListener("click", () => {
    currentValues.tracking = currentValues.tracking ? 0 : 1;
    renderTracking();
    nf.setParam("tracking", currentValues.tracking);
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
  updateKeyboard(data);
  updateHarmonyMeters(data);

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


// ── Keyboard (C2-B5): scale, target note, sung note, harmony notes ─────
const KB_LO = 36, KB_HI = 83;
const BLACK_PC = [1, 3, 6, 8, 10];
const SCALE_MASKS = [0xFFF, 0xAB5, 0x5AD, 0x295, 0x4E9]; // Chromatic, Major, Minor, Pentatonic, Blues
const PART_COLORS = ["#6ee7a8", "#ffb86b", "#ff7eb6", "#7cd1ff"];
const kbKeys = {};
const kbDots = [];
let kbLastTarget = -1, kbLastInput = -1;

function buildKeyboard() {
  const kb = $("kb");
  if (!kb) return;
  const W = 1054;
  const whites = [];
  for (let m = KB_LO; m <= KB_HI; m++) if (!BLACK_PC.includes(m % 12)) whites.push(m);
  const ww = W / whites.length;
  const pos = {};
  whites.forEach((m, i) => {
    const d = document.createElement("div");
    d.className = "wk";
    d.style.left = (i * ww) + "px"; d.style.width = ww + "px";
    if (m % 12 === 0) d.textContent = "C" + (m / 12 - 1);
    kb.appendChild(d); kbKeys[m] = d; pos[m] = i * ww;
  });
  for (let m = KB_LO; m <= KB_HI; m++) {
    if (!BLACK_PC.includes(m % 12)) continue;
    const d = document.createElement("div");
    d.className = "bk";
    d.style.left = (pos[m - 1] + ww * 0.68) + "px"; d.style.width = (ww * 0.64) + "px";
    kb.appendChild(d); kbKeys[m] = d;
  }
  for (let p = 0; p < 4; p++) {
    const dot = document.createElement("div");
    dot.className = "hv"; dot.textContent = String(p + 1);
    dot.style.background = PART_COLORS[p]; dot.style.boxShadow = "0 0 8px " + PART_COLORS[p];
    dot.style.display = "none";
    kb.appendChild(dot); kbDots.push(dot);
  }
  renderKeyboardScale();
}

function renderKeyboardScale() {
  const key = Math.round(Number(currentValues.key ?? 0));
  const mask = SCALE_MASKS[Math.round(Number(currentValues.scale ?? 0))] ?? 0xFFF;
  for (const m in kbKeys) {
    const rel = ((Number(m) - key) % 12 + 12) % 12;
    const inKey = ((mask >> rel) & 1) === 1;
    kbKeys[m].classList.toggle("in", inKey && mask !== 0xFFF);
    kbKeys[m].classList.toggle("out", !inKey);
  }
}

function midiName(m) { return KEY_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1); }

function updateKeyboard(data) {
  const target = data.targetMidi > 0 ? Math.round(data.targetMidi) : -1;
  const input = data.inMidi > 0 ? Math.round(data.inMidi) : -1;
  if (target !== kbLastTarget) {
    if (kbKeys[kbLastTarget]) kbKeys[kbLastTarget].classList.remove("target");
    if (kbKeys[target]) kbKeys[target].classList.add("target");
    kbLastTarget = target;
  }
  if (input !== kbLastInput) {
    if (kbKeys[kbLastInput]) kbKeys[kbLastInput].classList.remove("input");
    if (kbKeys[input]) kbKeys[input].classList.add("input");
    kbLastInput = input;
  }
  if (target > 0 && data.inMidi > 0) {
    const c = Math.round((data.inMidi - target) * 100);
    $("kbReadout").textContent = midiName(target) + " · " + (c > 0 ? "+" : c < 0 ? "−" : "") + Math.abs(c) + " cents";
  } else {
    $("kbReadout").textContent = "--";
  }

  const harmOn = !!currentValues.harmOn;
  const parts = Array.isArray(data.parts) ? data.parts : [];
  kbDots.forEach((dot, p) => {
    const m = Math.round(parts[p] || 0);
    const el = kbKeys[m];
    if (!harmOn || !el) { dot.style.display = "none"; return; }
    const isBlack = el.classList.contains("bk");
    dot.style.left = (parseFloat(el.style.left) + parseFloat(el.style.width) / 2) + "px";
    dot.style.top = isBlack ? "20px" : "46px";
    dot.style.display = "grid";
  });
  $("hvLegend").style.display = harmOn ? "" : "none";
}

// ── Harmony ─────────────────────────────────────────────────────────────
const INTERVAL_NAMES = ["Unison", "2nd Above", "3rd Above", "4th Above", "5th Above", "6th Above",
                        "Octave Above", "3rd Below", "4th Below", "5th Below", "6th Below", "Octave Below"];
// Style buttons load a starting point; every control stays editable after.
const HARM_STYLES = [
  { on: [1, 1, 0, 0], iv: [2, 6, 0, 0],  voices: 6,  width: 120, hum: 25 }, // Pop
  { on: [1, 1, 1, 0], iv: [2, 7, 5, 0],  voices: 10, width: 140, hum: 40 }, // R&B
  { on: [1, 1, 0, 0], iv: [11, 6, 0, 0], voices: 8,  width: 170, hum: 30 }, // Trap
  { on: [1, 1, 1, 0], iv: [2, 4, 11, 0], voices: 16, width: 140, hum: 45 }, // Gospel
  { on: [1, 1, 1, 1], iv: [2, 4, 6, 11], voices: 24, width: 180, hum: 60 }, // Choir
  { on: [1, 1, 0, 0], iv: [6, 4, 0, 0],  voices: 32, width: 200, hum: 80 }, // Ambient
  { on: [1, 0, 0, 0], iv: [0, 2, 0, 0],  voices: 4,  width: 140, hum: 20 }, // Doubles
];

function setP(id, v) { currentValues[id] = v; nf.setParam(id, v); }

function showView(v) {
  const harm = v === "harm";
  $("tabTune").classList.toggle("is-active", !harm);
  $("tabHarm").classList.toggle("is-active", harm);
  $("tuneView").style.display = harm ? "none" : "grid";
  $("harmViewWrap").style.display = harm ? "grid" : "none";
}

function renderHarmony() {
  const on = !!currentValues.harmOn;
  $("harmDot").classList.toggle("is-on", on);
  $("harmOnBtn").classList.toggle("is-active", on);
  $("harmPower").classList.toggle("is-on", on);
  $("harmPowerLabel").textContent = on ? "HARMONY ON" : "HARMONY OFF";
  $("harmView").classList.toggle("is-off", !on);

  const midi = Math.round(Number(currentValues.harmMode ?? 0)) === 1;
  document.querySelectorAll("#modeSeg button").forEach((b) => b.classList.toggle("is-active", (b.dataset.mode === "1") === midi));
  $("modeHint").textContent = midi ? "MIDI: play the harmony notes from a keyboard routed to Nova Pitch (one note per part)."
                                   : "Scale: parts follow the Key and Scale above.";
  $("partsHint").textContent = midi ? "· played from MIDI" : "· notes follow your key";

  const style = Math.round(Number(currentValues.harmStyle ?? 0));
  document.querySelectorAll("#styleSeg button").forEach((b) => b.classList.toggle("is-active", Number(b.dataset.style) === style));

  for (let p = 0; p < 4; p++) {
    const pOn = !!currentValues["part" + (p + 1) + "On"];
    $("part" + p).classList.toggle("off", !pOn);
    $("partOnBtn" + p).classList.toggle("is-active", pOn);
    $("partIntVal" + p).textContent = midi ? "MIDI note " + (p + 1) : INTERVAL_NAMES[Math.round(Number(currentValues["part" + (p + 1) + "Interval"] ?? 0))];
    $("partIntBtn" + p).disabled = midi;
    const db = Number(currentValues["part" + (p + 1) + "Level"] ?? 0);
    $("partLev" + p).textContent = (db > 0 ? "+" : "") + db.toFixed(1) + " dB";
  }
}

function refreshHarmonyFromMeta(byId) {
  const ids = ["harmOn", "harmMode", "harmStyle"];
  for (let p = 1; p <= 4; p++) ids.push("part" + p + "On", "part" + p + "Interval", "part" + p + "Level");
  ids.forEach((id) => { if (byId[id]) currentValues[id] = byId[id].value; });
  renderHarmony();
}

function initIntervalPopover(p) {
  const btn = $("partIntBtn" + p);
  const pop = document.createElement("div");
  pop.className = "stepPopover";
  INTERVAL_NAMES.forEach((name, i) => {
    const item = document.createElement("button");
    item.type = "button"; item.className = "stepPopoverItem"; item.textContent = name;
    item.addEventListener("click", (e) => {
      e.stopPropagation();
      setP("part" + (p + 1) + "Interval", i);
      if (!currentValues["part" + (p + 1) + "On"]) setP("part" + (p + 1) + "On", 1);
      pop.classList.remove("is-open");
      renderHarmony();
    });
    pop.appendChild(item);
  });
  btn.parentElement.appendChild(pop);
  btn.addEventListener("click", (e) => {
    e.stopPropagation();
    const cur = Math.round(Number(currentValues["part" + (p + 1) + "Interval"] ?? 0));
    [...pop.children].forEach((c, i) => c.classList.toggle("is-selected", i === cur));
    pop.classList.toggle("is-open");
  });
  document.addEventListener("pointerdown", (e) => {
    if (pop.classList.contains("is-open") && !pop.contains(e.target) && !btn.contains(e.target)) pop.classList.remove("is-open");
  });
}

function initLevelDrag(p) {
  const el = $("partLev" + p);
  const id = "part" + (p + 1) + "Level";
  let drag = null;
  el.addEventListener("pointerdown", (e) => { e.preventDefault(); drag = { y: e.clientY, v: Number(currentValues[id] ?? 0) }; });
  window.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const v = Math.max(-24, Math.min(6, drag.v + (drag.y - e.clientY) / (currentStageScale || 1) * 0.12));
    setP(id, Math.round(v * 10) / 10);
    renderHarmony();
  });
  window.addEventListener("pointerup", () => { drag = null; });
  el.addEventListener("dblclick", () => { setP(id, -6); renderHarmony(); });
}

function applyStyle(i) {
  const st = HARM_STYLES[i];
  setP("harmStyle", i);
  for (let p = 0; p < 4; p++) {
    setP("part" + (p + 1) + "On", st.on[p]);
    setP("part" + (p + 1) + "Interval", st.iv[p]);
  }
  setP("harmVoices", st.voices); setP("harmWidth", st.width); setP("harmHumanize", st.hum);
  [["harmVoices", st.voices], ["harmWidth", st.width], ["harmHumanize", st.hum]].forEach(([id, v]) => setKnobVisual($(id + "Knob"), v));
  renderHarmony();
}

function setCompactView(compact, tellNative) {
  document.body.classList.toggle("compact", compact);
  $("viewValue").textContent = compact ? "Compact" : "Full";
  lastFitW = -1; fitStage();
  if (tellNative) nf.setCompact(compact);
}

function initViewToggle() {
  $("viewBtn").addEventListener("click", () => setCompactView(!document.body.classList.contains("compact"), true));
  Promise.resolve(nf.getCompact()).then((c) => setCompactView(!!c, false)).catch(() => {});
}

function initHarmony() {
  initViewToggle();
  $("tabTune").addEventListener("click", () => showView("tune"));
  $("tabHarm").addEventListener("click", () => showView("harm"));
  $("harmOnBtn").addEventListener("click", () => { setP("harmOn", currentValues.harmOn ? 0 : 1); renderHarmony(); });
  document.querySelectorAll("#modeSeg button").forEach((b) => b.addEventListener("click", () => { setP("harmMode", Number(b.dataset.mode)); renderHarmony(); }));
  document.querySelectorAll("#styleSeg button").forEach((b) => b.addEventListener("click", () => applyStyle(Number(b.dataset.style))));
  for (let p = 0; p < 4; p++) {
    initIntervalPopover(p);
    initLevelDrag(p);
    $("partOnBtn" + p).addEventListener("click", () => { setP("part" + (p + 1) + "On", currentValues["part" + (p + 1) + "On"] ? 0 : 1); renderHarmony(); });
  }
  renderHarmony();
}

function updateHarmonyMeters(data) {
  const parts = Array.isArray(data.parts) ? data.parts : [];
  for (let p = 0; p < 4; p++) {
    const m = Math.round(parts[p] || 0);
    const el = $("partNote" + p);
    if (m > 0) el.innerHTML = KEY_NAMES[m % 12] + "<span>" + (Math.floor(m / 12) - 1) + "</span>";
    else el.textContent = "--";
  }
  const levels = Array.isArray(data.voiceLevels) ? data.voiceLevels : [];
  const n = data.activeVoices || 0;
  const enabled = [];
  for (let p = 0; p < 4; p++) if (currentValues["part" + (p + 1) + "On"]) enabled.push(p);
  const want = Math.round(Number(currentValues.harmVoices ?? 8));
  const shown = n > 0 ? n : Math.max(enabled.length, Math.min(32, want));
  $("stackHead").textContent = "· " + shown + " of 32 voices";
  for (let v = 0; v < 32; v++) {
    const bar = $("vbf" + v);
    const on = v < shown && enabled.length > 0;
    $("vb" + v).classList.toggle("off", !on);
    const c = on ? PART_COLORS[enabled[v % enabled.length]] : "#3a3a46";
    const lv = levels[v] || 0;
    const db = lv > 0 ? 20 * Math.log10(lv) : -80;
    const h = on ? Math.max(6, Math.min(100, (db + 54) / 48 * 100)) : 30;
    bar.style.height = h + "%";
    bar.style.background = c;
    bar.style.boxShadow = on && lv > 0.001 ? "0 0 6px " + c : "none";
  }
}

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
  if (byId.tracking) {
    currentValues.tracking = byId.tracking.value ? 1 : 0;
    renderTracking();
  }
  if (byId.bypass) {
    const active = !!byId.bypass.value;
    currentValues.bypass = active ? 1 : 0;
    $("bypassBtn").classList.toggle("is-active", active);
    $("bypassBtn").setAttribute("aria-pressed", String(active));
  }

  refreshHarmonyFromMeta(byId);
  renderKeyboardScale();
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
  const scale = Math.min(w / 1100, h / (document.body.classList.contains("compact") ? 660 : 760));
  currentStageScale = scale || 1;
  const stage = $("stage");
  if (stage) stage.style.transform = `scale(${scale})`;
}
fitStage();
window.addEventListener("resize", fitStage);
setInterval(fitStage, 300);

(function boot() {
  document.querySelectorAll(".knob[data-param]").forEach(initKnob);
  initStepControl($("keyBtn"), "key", KEY_NAMES, "keyValue", renderKeyboardScale);
  initStepControl($("scaleBtn"), "scale", SCALE_NAMES, "scaleValue", renderKeyboardScale);
  buildKeyboard();
  initHarmony();
  initBypass();
  initTracking();
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
