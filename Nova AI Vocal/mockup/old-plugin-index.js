import { getNativeFunction, getSliderState, getComboBoxState, getToggleState } from "./juce/index.js";

const nativeToggleListen = getNativeFunction("toggleListen");

// ── Knobs ────────────────────────────────────────────────────────────────
// Vertical drag (up = more), Shift for fine, wheel steps, double-click resets.
// Same feel as Nova Silk: 0.45 % per pixel.

const ARC_START = -135, ARC_END = 135;
const DEFAULTS = { clean: 0.6, deess: 0.55, punch: 0.7, air: 0.75, warmth: 0.25, space: 0.55, tune: 0.95, mix: 1, output: 0.5 };

function polar(cx, cy, r, deg) {
  const a = (deg - 90) * Math.PI / 180;
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
}
function arcPath(cx, cy, r, a0, a1) {
  if (Math.abs(a1 - a0) < 0.01) return "";
  const [x0, y0] = polar(cx, cy, r, a0);
  const [x1, y1] = polar(cx, cy, r, a1);
  const large = Math.abs(a1 - a0) > 180 ? 1 : 0;
  const sweep = a1 > a0 ? 1 : 0;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} ${sweep} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

function formatValue(param, state) {
  if (param === "output") {
    const db = state.getScaledValue();
    return (db > 0 ? "+" : "") + db.toFixed(1) + " dB";
  }
  return Math.round(state.getNormalisedValue() * 100) + "%";
}

const knobs = [];

function setupKnob(el) {
  const param = el.dataset.param;
  const bipolar = el.dataset.bipolar === "1";
  const state = getSliderState(param);
  const valueEl = document.querySelector(`[data-value="${param}"]`);

  el.innerHTML = `
    <svg viewBox="0 0 74 74">
      <path class="arc-track" d="${arcPath(37, 37, 31, ARC_START, ARC_END)}"></path>
      <path class="arc-value"></path>
      <circle class="cap" cx="37" cy="37" r="22"></circle>
      <line class="pointer" x1="37" y1="37" x2="37" y2="21"></line>
    </svg>`;
  const arcValue = el.querySelector(".arc-value");
  const pointer = el.querySelector(".pointer");

  function render() {
    const v = state.getNormalisedValue();
    const ang = ARC_START + v * (ARC_END - ARC_START);
    const from = bipolar ? 0 : ARC_START;
    arcValue.setAttribute("d", arcPath(37, 37, 31, Math.min(from, ang), Math.max(from, ang)));
    pointer.setAttribute("transform", `rotate(${ang} 37 37)`);
    if (valueEl) valueEl.textContent = formatValue(param, state);
  }

  let drag = null;
  el.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    drag = { y: e.clientY, start: state.getNormalisedValue(), id: e.pointerId };
    try { el.setPointerCapture(e.pointerId); } catch (_) {}
    state.sliderDragStarted();
  });
  el.addEventListener("pointermove", (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    const scale = window.__novaVocalScale || 1;
    const dy = (drag.y - e.clientY) / scale;
    const perPx = e.shiftKey ? 0.001 : 0.0045;
    state.setNormalisedValue(Math.max(0, Math.min(1, drag.start + dy * perPx)));
    render();
  });
  const end = (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    drag = null;
    try { el.releasePointerCapture(e.pointerId); } catch (_) {}
    state.sliderDragEnded();
  };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);

  el.addEventListener("wheel", (e) => {
    e.preventDefault();
    const step = (e.shiftKey ? 0.0015 : 0.01) * (e.deltaY > 0 ? -1 : 1);
    state.sliderDragStarted();
    state.setNormalisedValue(Math.max(0, Math.min(1, state.getNormalisedValue() + step)));
    state.sliderDragEnded();
    render();
  }, { passive: false });

  el.addEventListener("dblclick", () => {
    state.sliderDragStarted();
    state.setNormalisedValue(DEFAULTS[param] ?? 0.5);
    state.sliderDragEnded();
    render();
  });

  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();
  knobs.push(render);
}

document.querySelectorAll(".knob[data-param]").forEach(setupKnob);

// ── Dropdowns (vibe, key) ────────────────────────────────────────────────
function setupDropdown(paramName, ddId, menuId, valId, format) {
  const state = getComboBoxState(paramName);
  const dd = document.getElementById(ddId);
  const menu = document.getElementById(menuId);
  const val = document.getElementById(valId);
  function render() {
    const choices = state.properties.choices || [];
    const idx = state.getChoiceIndex();
    menu.innerHTML = "";
    choices.forEach((name, i) => {
      const opt = document.createElement("div");
      opt.className = "opt" + (i === idx ? " selected" : "");
      opt.textContent = name;
      opt.addEventListener("click", (e) => {
        e.stopPropagation();
        menu.classList.remove("open");
        state.setChoiceIndex(i);
        render();
      });
      menu.appendChild(opt);
    });
    if (choices[idx]) val.textContent = format(choices[idx]);
  }
  dd.addEventListener("click", (e) => {
    e.stopPropagation();
    document.querySelectorAll(".dropdown-menu").forEach((m) => { if (m !== menu) m.classList.remove("open"); });
    menu.classList.toggle("open");
  });
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();
}
document.addEventListener("click", () => document.querySelectorAll(".dropdown-menu").forEach((m) => m.classList.remove("open")));
setupDropdown("vibe", "vibe-dropdown", "vibe-menu", "vibe-val", (t) => t.toUpperCase());
setupDropdown("key", "key-dropdown", "key-menu", "key-val", (t) => t.toUpperCase());

// ── Bypass ───────────────────────────────────────────────────────────────
const bypassState = getToggleState("bypass");
const bypassBtn = document.getElementById("bypass-btn");
function renderBypass() { bypassBtn.classList.toggle("bypassed", !!bypassState.getValue()); }
bypassBtn.addEventListener("click", () => { bypassState.setValue(!bypassState.getValue()); renderBypass(); });
bypassState.valueChangedEvent.addListener(renderBypass);
renderBypass();

// ── Listen ring + status (pushed from C++ at 30 Hz) ──────────────────────
const RING_CIRC = 2 * Math.PI * 72;
const ringBtn = document.getElementById("ring-btn");
const ringProgress = document.getElementById("ring-progress");
const ringLabel = document.getElementById("ring-label");
const ringSub = document.getElementById("ring-sub");
const statusText = document.getElementById("status-text");
const chips = document.getElementById("chips");
ringProgress.setAttribute("stroke-dasharray", RING_CIRC.toFixed(2));

function setRing(fraction) {
  ringProgress.setAttribute("stroke-dashoffset", (RING_CIRC * (1 - fraction)).toFixed(2));
}

let lastSummary = null, lastState = null;

const NOTE_NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
const tuneNote = document.getElementById("tune-note");
const keyVal = document.getElementById("key-val");
function noteName(m) { const n = Math.round(m); return NOTE_NAMES[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1); }

function renderTuner(s) {
  if (s.inMidi > 0 && s.targetMidi > 0) {
    const cents = Math.round((s.inMidi - s.targetMidi) * 100);
    tuneNote.textContent = noteName(s.targetMidi) + "  " + (cents >= 0 ? "+" : "") + cents + "c";
  } else {
    tuneNote.textContent = "";
  }
  if (s.keyName && keyVal.textContent.startsWith("AUTO")) keyVal.textContent = "AUTO: " + s.keyName.replace(" (auto)", "").toUpperCase();
}

function renderStatus(s) {
  ringBtn.classList.toggle("listening", s.state === "Listening" || s.state === "Analyzing");

  switch (s.state) {
    case "Idle":
      ringLabel.textContent = "LISTEN";
      ringSub.textContent = "auto mix";
      statusText.textContent = s.summary || "Pick a vibe, press LISTEN and play 10-30 seconds of your vocal.";
      setRing(0);
      break;
    case "Listening":
      ringLabel.textContent = "LISTENING";
      ringSub.textContent = s.captured.toFixed(1) + "s  |  tap to mix";
      statusText.textContent = s.captured < 2
        ? "Play your vocal now..."
        : "Keep going for a few more seconds, then tap the ring to mix.";
      setRing(Math.min(1, s.captured / 30));
      break;
    case "Analyzing":
      ringLabel.textContent = "MIXING";
      ringSub.textContent = "";
      statusText.textContent = "Analyzing your voice...";
      setRing(1);
      break;
    case "Mixed":
      ringLabel.textContent = "MIXED";
      ringSub.textContent = "tap to re-listen";
      statusText.textContent = "Mixed for your voice. Switch vibes to re-mix, or tweak the knobs.";
      setRing(1);
      break;
  }

  if (s.summary !== lastSummary || s.state !== lastState) {
    lastSummary = s.summary;
    lastState = s.state;
    chips.innerHTML = "";
    if (s.state === "Mixed" && s.summary) {
      s.summary.split(" | ").forEach((t) => {
        const c = document.createElement("span");
        c.className = "chip";
        c.textContent = t;
        chips.appendChild(c);
      });
    }
  }
}

ringBtn.addEventListener("click", () => nativeToggleListen());

// ── Meters ───────────────────────────────────────────────────────────────
function setLevel(id, valId, db) {
  const pct = Math.max(0, Math.min(1, (db + 60) / 60)) * 100;
  document.querySelector(`#${id} .fill`).style.width = pct.toFixed(1) + "%";
  document.getElementById(valId).textContent = db <= -99 ? "-inf" : db.toFixed(1);
}
function setGr(id, valId, db) {
  const pct = Math.max(0, Math.min(1, db / 20)) * 100;
  document.querySelector(`#${id} .fill`).style.width = pct.toFixed(1) + "%";
  document.getElementById(valId).textContent = "-" + Math.max(0, db).toFixed(1);
}

// Peak-hold-ish smoothing so meters are readable at 30 Hz
const meter = { inDb: -100, outDb: -100, compGr: 0, deessGr: 0 };
function smooth(key, v, fall) { meter[key] = v > meter[key] ? v : meter[key] + (v - meter[key]) * fall; return meter[key]; }

if (window.__JUCE__ && window.__JUCE__.backend) {
  window.__JUCE__.backend.addEventListener("novaStatus", (s) => {
    renderStatus(s);
    renderTuner(s);
    setLevel("m-in", "v-in", smooth("inDb", s.inDb, 0.15));
    setLevel("m-out", "v-out", smooth("outDb", s.outDb, 0.15));
    setGr("m-deess", "v-deess", smooth("deessGr", s.deessGr, 0.25));
    setGr("m-comp", "v-comp", smooth("compGr", s.compGr, 0.25));
  });
}

renderStatus({ state: "Idle", summary: "", captured: 0 });

// ── Stage scale-to-fit (same technique as Nova AI Master) ────────────────
(function () {
  let lastW = -1, lastH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0 || (w === lastW && h === lastH)) return;
    lastW = w; lastH = h;
    const scale = Math.min(w / 500, h / 600);
    window.__novaVocalScale = scale;
    const stage = document.getElementById("stage");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
}());
