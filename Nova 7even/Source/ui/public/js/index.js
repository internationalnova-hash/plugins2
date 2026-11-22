import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of [
  "setParam", "getParam", "getParamsMeta",
  "abGetState", "abCaptureActive", "abToggleAndRecall", "abCopyActiveToOther",
  "jsError",
]) nf[name] = getNativeFunction(name);

window.onerror = function (msg, src, line, col, err) {
  try { nf.jsError(String(msg) + " @" + line + ":" + col); } catch (e) {}
};

const SWEEP_DEG = 270; // -135deg..+135deg, matches .arc default transform

// Current #app CSS transform:scale() factor (kept in sync by fitStage()
// at the bottom of this file). Pointer events report clientX/clientY in
// real screen pixels, unaffected by a CSS transform on an ancestor, so
// drag handlers must divide by this to stay correctly calibrated once the
// window is no longer 1:1 with the design canvas.
let currentStageScale = 1;

function $(id) { return document.getElementById(id); }

// ── VU meter ──────────────────────────────────────────────────────────
window.updateVu = function (vuDb) {
  const needle = $("vuNeedle");
  if (!needle) return;
  // Real VU meters aren't linear across their printed scale, but a
  // clamped linear map from -20..+3 VU onto a -50..+50 degree sweep is a
  // reasonable visual approximation for a needle indicator.
  const clamped = Math.max(-20, Math.min(3, vuDb));
  const deg = ((clamped + 20) / 23) * 100 - 50;
  needle.style.transform = `rotate(${deg}deg)`;
};

// ── Continuous knobs (drag-to-change, vertical) ──────────────────────
function setContinuousKnobVisual(knobEl, value, min, max) {
  const norm = Math.max(0, Math.min(1, (value - min) / (max - min)));
  const deg = -135 + norm * SWEEP_DEG;
  knobEl.style.setProperty("--turn", `${deg}deg`);
}

function formatKnobValue(paramId, value) {
  if (paramId === "input_trim") return Math.round(((value + 20) / 44) * 80).toString();
  if (paramId === "mix") return `${Math.round(value)}%`;
  return (value >= 0 ? "+" : "") + value.toFixed(1);
}

const currentValues = {};

function initContinuousKnob(knobEl) {
  const paramId = knobEl.dataset.param;
  const min = Number(knobEl.dataset.min);
  const max = Number(knobEl.dataset.max);

  let dragState = null;

  const startDrag = (event) => {
    if (typeof event.button === "number" && event.button !== 0) return;
    event.preventDefault();
    knobEl.classList.add("dragging");
    dragState = { startY: getClientY(event), startValue: Number(currentValues[paramId] ?? min) };
    if (knobEl.setPointerCapture && event.pointerId !== undefined) {
      try { knobEl.setPointerCapture(event.pointerId); } catch (e) {}
    }
  };

  const moveDrag = (event) => {
    if (!dragState) return;
    event.preventDefault();
    const delta = (dragState.startY - getClientY(event)) / (currentStageScale || 1);
    const next = Math.max(min, Math.min(max, dragState.startValue + delta * (max - min) * 0.005));
    currentValues[paramId] = next;
    // A parameter can appear as more than one physical knob (e.g.
    // output_trim on both the main OUTPUT knob and the footer's compact
    // "OUT TRIM" knob) - keep every instance visually in sync, not just
    // the one being dragged.
    document.querySelectorAll(`.knob[data-param="${paramId}"]:not([data-choice])`).forEach((el) => {
      setContinuousKnobVisual (el, next, Number(el.dataset.min), Number(el.dataset.max));
    });
    const disp = $("disp_" + paramId);
    if (disp) disp.textContent = formatKnobValue(paramId, next);
    nf.setParam(paramId, next);
  };

  const endDrag = () => { knobEl.classList.remove("dragging"); dragState = null; };

  knobEl.addEventListener("pointerdown", startDrag);
  window.addEventListener("pointermove", moveDrag, { passive: false });
  window.addEventListener("pointerup", endDrag);
}

function getClientY(event) {
  if (event.touches && event.touches.length) return event.touches[0].clientY;
  return typeof event.clientY === "number" ? event.clientY : 0;
}

// ── Stepped knobs (drag or click cycles through discrete choices) ────
function setSteppedKnobVisual(knobEl, index, numChoices) {
  const norm = numChoices > 1 ? index / (numChoices - 1) : 0;
  const deg = -135 + norm * SWEEP_DEG;
  knobEl.style.setProperty("--turn", `${deg}deg`);

  const labelsContainer = $("labels_" + knobEl.dataset.param);
  if (labelsContainer) {
    labelsContainer.querySelectorAll("[data-i]").forEach((el) => {
      el.classList.toggle("active", Number(el.dataset.i) === index);
    });
  }
}

function initSteppedKnob(knobEl) {
  const paramId = knobEl.dataset.param;
  const choices = knobEl.dataset.choices.split(",");
  const numChoices = choices.length;

  let dragState = null;

  const startDrag = (event) => {
    if (typeof event.button === "number" && event.button !== 0) return;
    event.preventDefault();
    knobEl.classList.add("dragging");
    dragState = { startY: getClientY(event), startIndex: Number(currentValues[paramId] ?? 0) };
    if (knobEl.setPointerCapture && event.pointerId !== undefined) {
      try { knobEl.setPointerCapture(event.pointerId); } catch (e) {}
    }
  };

  const moveDrag = (event) => {
    if (!dragState) return;
    event.preventDefault();
    const delta = (dragState.startY - getClientY(event)) / (currentStageScale || 1);
    const rawIndex = dragState.startIndex + delta * (numChoices - 1) * 0.01;
    const index = Math.max(0, Math.min(numChoices - 1, Math.round(rawIndex)));
    if (index !== currentValues[paramId]) {
      currentValues[paramId] = index;
      setSteppedKnobVisual(knobEl, index, numChoices);
      nf.setParam(paramId, index);
    }
  };

  const endDrag = () => { knobEl.classList.remove("dragging"); dragState = null; };

  knobEl.addEventListener("pointerdown", startDrag);
  window.addEventListener("pointermove", moveDrag, { passive: false });
  window.addEventListener("pointerup", endDrag);

  // Clicking a printed frequency label jumps straight to that step.
  const labelsContainer = $("labels_" + paramId);
  if (labelsContainer) {
    labelsContainer.querySelectorAll("[data-i]").forEach((el) => {
      el.style.cursor = "pointer";
      el.addEventListener("click", () => {
        const index = Number(el.dataset.i);
        currentValues[paramId] = index;
        setSteppedKnobVisual(knobEl, index, numChoices);
        nf.setParam(paramId, index);
      });
    });
  }
}

// ── Character mode (physical 3-position slide switch) ────────────────
const CHARACTER_NUB_POSITIONS = [2, 42, 82]; // px, matches switchTrack(120)/switchNub(36) geometry

function setCharacterVisual(index) {
  document.querySelectorAll("#characterLabels [data-i]").forEach((el) => {
    el.classList.toggle("active", Number(el.dataset.i) === index);
  });
  const nub = $("characterNub");
  if (nub) nub.style.left = CHARACTER_NUB_POSITIONS[Math.max(0, Math.min(2, index))] + "px";
}

function selectCharacter(index) {
  currentValues.character_mode = index;
  setCharacterVisual(index);
  nf.setParam("character_mode", index);
}

function initCharacterTrack() {
  document.querySelectorAll("#characterLabels .lbl[data-i]").forEach((el) => {
    el.addEventListener("click", () => selectCharacter(Number(el.dataset.i)));
  });

  const track = $("characterSwitch");
  const nub = $("characterNub");

  const positionToIndex = (clientX) => {
    const rect = track.getBoundingClientRect();
    const relativeX = clientX - rect.left;
    // 3 roughly-equal zones across the track width.
    if (relativeX < rect.width / 3) return 0;
    if (relativeX < (rect.width * 2) / 3) return 1;
    return 2;
  };

  track.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    selectCharacter(positionToIndex(event.clientX));
    const onMove = (moveEvent) => selectCharacter(positionToIndex(moveEvent.clientX));
    const onUp = () => {
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
    };
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
  });
}

// ── Oversample button (click cycles Off -> 2x -> 4x -> Off) ──────────
const OVERSAMPLE_LABELS = ["Off", "2x", "4x"];

function setOversampleVisual(index) {
  const btn = $("osBtn");
  if (btn) btn.textContent = OVERSAMPLE_LABELS[Math.max(0, Math.min(2, index))];
}

function selectOversample(index) {
  currentValues.oversampling_mode = index;
  setOversampleVisual(index);
  nf.setParam("oversampling_mode", index);
}

function initOversampleButton() {
  const btn = $("osBtn");
  if (!btn) return;
  btn.addEventListener("click", () => {
    const next = ((currentValues.oversampling_mode ?? 1) + 1) % 3;
    selectOversample(next);
  });
}

// ── Phase / Bypass toggle buttons ────────────────────────────────────
function initToggleButton(buttonEl, paramId) {
  buttonEl.addEventListener("click", () => {
    const next = !buttonEl.classList.contains("active");
    buttonEl.classList.toggle("active", next);
    nf.setParam(paramId, next);
  });
}

// ── A/B header buttons ────────────────────────────────────────────────
async function refreshAbButtons() {
  const state = await nf.abGetState();
  if (!state) return;
  $("abBtnA").classList.toggle("active", state.activeSlot === "A");
  $("abBtnB").classList.toggle("active", state.activeSlot === "B");
}

function initAbButtons() {
  $("abBtnA").addEventListener("click", async () => {
    const state = await nf.abGetState();
    if (state && state.activeSlot !== "A") {
      await nf.abCaptureActive(); // persist edits made while on B
      await nf.abToggleAndRecall(); // -> A
      await refreshAll();
    }
  });
  $("abBtnB").addEventListener("click", async () => {
    const state = await nf.abGetState();
    if (state && state.activeSlot !== "B") {
      await nf.abCaptureActive(); // persist edits made while on A
      await nf.abToggleAndRecall(); // -> B
      await refreshAll();
    }
  });
}

// ── Full refresh from the real processor state ───────────────────────
async function refreshAll() {
  const metas = (await nf.getParamsMeta()) || [];
  const byId = {};
  metas.forEach((m) => { byId[m.id] = m; });

  document.querySelectorAll(".knob[data-param]:not([data-choice])").forEach((knobEl) => {
    const id = knobEl.dataset.param;
    if (!byId[id]) return;
    currentValues[id] = byId[id].value;
    setContinuousKnobVisual(knobEl, byId[id].value, Number(knobEl.dataset.min), Number(knobEl.dataset.max));
    const disp = $("disp_" + id);
    if (disp) disp.textContent = formatKnobValue(id, byId[id].value);
  });

  document.querySelectorAll(".knob[data-param][data-choice]").forEach((knobEl) => {
    const id = knobEl.dataset.param;
    if (!byId[id]) return;
    const numChoices = knobEl.dataset.choices.split(",").length;
    currentValues[id] = byId[id].value;
    setSteppedKnobVisual(knobEl, byId[id].value, numChoices);
  });

  if (byId.character_mode) {
    currentValues.character_mode = byId.character_mode.value;
    setCharacterVisual(byId.character_mode.value);
  }

  if (byId.phase_invert) $("phaseBtn").classList.toggle("active", !!byId.phase_invert.value);
  if (byId.bypass) $("bypassBtn").classList.toggle("active", !!byId.bypass.value);

  if (byId.oversampling_mode) {
    currentValues.oversampling_mode = byId.oversampling_mode.value;
    setOversampleVisual(byId.oversampling_mode.value);
  }

  await refreshAbButtons();
}

(function () {
  document.querySelectorAll(".knob[data-param]:not([data-choice])").forEach(initContinuousKnob);
  document.querySelectorAll(".knob[data-param][data-choice]").forEach(initSteppedKnob);
  initCharacterTrack();
  initToggleButton($("phaseBtn"), "phase_invert");
  initToggleButton($("bypassBtn"), "bypass");
  initOversampleButton();
  initAbButtons();
  refreshAll();
})();

// ---- stage scale-to-fit (same technique as Nova Tape/Nova Pitch) ----
(function () {
  let lastW = -1, lastH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    const scale = Math.min(w / 1000, h / 585);
    currentStageScale = scale || 1;
    const stage = document.getElementById("app");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();
