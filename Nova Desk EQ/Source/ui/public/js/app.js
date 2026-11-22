import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of [
  "setParam", "getParam", "getParamsMeta",
  "listPresets", "getCurrentPresetIndex", "loadPreset", "loadNextPreset", "loadPreviousPreset", "savePreset",
  "getEqCurve",
  "abGetState", "abCaptureActive", "abToggleAndRecall", "abCopyActiveToOther",
  "jsError",
]) nf[name] = getNativeFunction(name);

window.addEventListener("error", (e) => { try { nf.jsError(String(e.message || e)); } catch (_) {} });

// ── Knob geometry ────────────────────────────────────────────────────────
// Knobs are procedural (CSS radial-gradient face + a ::after pointer stripe
// fixed at 12 o'clock in the element's unrotated state), matching Nova
// 7even's real technique - not a photo. At v=0.5 the pointer sits straight
// up (0deg); the full range sweeps -135deg..+135deg (standard 270deg pot).
const SWEEP_DEG = 270;

// Current #app CSS transform:scale() factor (kept in sync by fitStage()
// at the bottom of this file). Pointer events report clientX/clientY in
// real screen pixels, unaffected by a CSS transform on an ancestor, so
// drag handlers must divide by this to stay correctly calibrated once the
// window is no longer 1:1 with the design canvas.
let currentStageScale = 1;

function turnForValue01 (v) {
  return v * SWEEP_DEG - (SWEEP_DEG / 2);
}

// ── Parameter metadata (fetched once) ───────────────────────────────────
let paramMeta = {};   // id -> {type, min, max, choices, text, skew}
let currentValues = {}; // id -> normalized 0..1
const draggingIds = new Set();

// Replicates JUCE's own NormalisableRange::convertFrom0to1() skew formula
// (value = min + (max-min) * pow(proportion, skew)) so live-drag readouts
// and curve-marker placement match the real (often skewed, for frequency
// knobs) parameter mapping exactly, without a native round-trip per pixel
// of drag.
function rawFloatValue (id) {
  const meta = paramMeta[id];
  if (!meta) return null;
  const normalized = currentValues[id] ?? 0;
  const skew = meta.skew ?? 1;
  const proportion = skew !== 1 ? Math.pow(normalized, skew) : normalized;
  return meta.min + proportion * (meta.max - meta.min);
}

function fmtReadout (id, meta, normalized) {
  if (!meta) return "";
  if (meta.type === "bool") return normalized > 0.5 ? "On" : "Off";
  if (meta.type === "choice") {
    const idx = Math.round(normalized * (meta.choices.length - 1));
    return meta.choices[idx] ?? "";
  }
  const raw = rawFloatValue(id);
  if (id.includes("freq") && !id.includes("index")) {
    return raw >= 1000 ? (raw / 1000).toFixed(2) + " kHz" : Math.round(raw) + " Hz";
  }
  if (id.includes("gain") || id === "input" || id === "output") return raw.toFixed(1) + " dB";
  if (id === "mix") return Math.round(raw) + "%";
  if (id.endsWith("_q")) return raw.toFixed(2);
  if (id === "drive") return raw.toFixed(1);
  return raw.toFixed(2);
}

// ── Tick-mark rings + reference numbers printed around each knob ────────
// Exact per-band values transcribed from the locked mockup's own printed
// DSP-requirements panel (HPF/LOW/LOW MID/HIGH MID/HIGH/LPF sections).
const TICK_SETS = {
  input:      ["-24", "-12", "0", "+12", "+24"],
  lowfreq:    ["30", "60", "100", "200", "300"],
  lowmidfreq: ["100", "200", "400", "800", "1.5k", "2k"],
  highmidfreq:["600", "1k", "2k", "4k", "6k", "8k"],
  highfreq:   ["3k", "5k", "8k", "12k", "16k", "20k"],
  gain15:     ["-15", "0", "+15"],
  q:          ["0.3", "0.8", "2.5", "4"],
  drive:      ["0", "5", "10"],
};
// hpf/lpf ticks are populated from the parameter's real stepped choices
// (getParamsMeta) rather than a hardcoded guess - see renderTicksFor.
const TICK_FROM_CHOICES = new Set(["hpf", "lpf"]);

// Same conic-gradient tick-ring technique as Nova 7even's knob::before
// (a masked ring of short radial dashes spanning the -135deg..+135deg
// sweep), but built here as a real element with a JS-computed gradient
// so it generalizes to any tick count instead of Nova 7even's per-count
// hardcoded CSS classes (Desk EQ has many more distinct knob instances).
function tickRingGradient (n) {
  const dashDeg = 3;
  const stops = [];
  for (let i = 0; i < n; i++) {
    const center = n === 1 ? 0 : (i / (n - 1)) * 270;
    const start = Math.max(0, center - dashDeg / 2);
    const end = Math.min(270, center + dashDeg / 2);
    const prevEnd = stops.length ? stops[stops.length - 1].end : 0;
    if (start > prevEnd) stops.push({ end: start, color: "transparent" });
    stops.push({ end, color: "rgba(216,196,160,0.85)" });
  }
  stops.push({ end: 270, color: "transparent" });
  stops.push({ end: 360, color: "transparent" });
  let prev = 0;
  const parts = stops.map((s) => {
    const seg = `${s.color} ${prev}deg ${s.end}deg`;
    prev = s.end;
    return seg;
  });
  return `conic-gradient(from 225deg, ${parts.join(", ")})`;
}

function renderTicksFor (wrapEl) {
  const key = wrapEl.dataset.ticks;
  const dialFace = wrapEl.closest(".dialFace");
  const paramEl = dialFace && dialFace.querySelector(".knob[data-param]");
  const id = paramEl ? paramEl.dataset.param : null;

  let labels = null;
  if (key) {
    if (id && TICK_FROM_CHOICES.has(key) && paramMeta[id] && paramMeta[id].type === "choice") {
      labels = paramMeta[id].choices;
    } else {
      labels = TICK_SETS[key];
    }
  }
  if (!labels || !labels.length) return;

  wrapEl.innerHTML = "";
  const ring = document.createElement("div");
  ring.className = "tickRing";
  ring.style.background = tickRingGradient(labels.length);
  wrapEl.appendChild(ring);

  const n = labels.length;
  labels.forEach((label, i) => {
    const angleDeg = -135 + (n === 1 ? 0 : (i / (n - 1)) * 270);
    const rad = (angleDeg * Math.PI) / 180;
    const x = 50 + 44 * Math.sin(rad);
    const y = 50 - 44 * Math.cos(rad);
    const span = document.createElement("span");
    span.className = "tickLabel";
    span.textContent = label;
    span.style.left = x + "%";
    span.style.top = y + "%";
    wrapEl.appendChild(span);
  });
}

function renderAllTicks () {
  document.querySelectorAll(".tickWrap").forEach(renderTicksFor);
}

function applyKnobVisual (el, id, normalized) {
  const stepped = el.dataset.stepped;
  let v = normalized;
  if (stepped) {
    const n = parseInt(stepped, 10);
    v = Math.round(normalized * (n - 1)) / (n - 1);
  }
  el.style.setProperty("--turn", `${turnForValue01(v)}deg`);
  const readout = document.querySelector(`[data-readout="${id}"]`);
  if (readout) readout.textContent = fmtReadout(id, paramMeta[id], normalized);
}

function refreshAllVisuals () {
  document.querySelectorAll(".knob[data-param]").forEach((el) => {
    const id = el.dataset.param;
    if (currentValues[id] !== undefined) applyKnobVisual(el, id, currentValues[id]);
  });
  document.querySelectorAll("[data-toggle]").forEach((el) => {
    const id = el.dataset.toggle;
    if (currentValues[id] !== undefined) el.classList.toggle("active", currentValues[id] > 0.5);
  });
  document.querySelectorAll("[data-choiceToggle]").forEach((el) => {
    const id = el.dataset.choicetoggle || el.getAttribute("data-choiceToggle");
    const meta = paramMeta[id];
    if (!meta || currentValues[id] === undefined) return;
    const idx = Math.round(currentValues[id] * (meta.choices.length - 1));
    el.querySelectorAll(".typeRow").forEach((row) => {
      const lbl = row.querySelector(".typeLabel");
      row.classList.toggle("active", lbl && parseInt(lbl.dataset.choice, 10) === idx);
    });
  });
  document.querySelectorAll("[data-choiceParam]").forEach((el) => {
    const id = el.dataset.choiceparam || el.getAttribute("data-choiceParam");
    const meta = paramMeta[id];
    if (!meta || currentValues[id] === undefined) return;
    const idx = Math.round(currentValues[id] * (meta.choices.length - 1));
    el.classList.toggle("active", parseInt(el.dataset.choice, 10) === idx);
  });
}

// ── Continuous / stepped knob drag ──────────────────────────────────────
let abCaptureTimer = null;
function scheduleAbCapture () {
  if (abCaptureTimer) clearTimeout(abCaptureTimer);
  abCaptureTimer = setTimeout(() => { nf.abCaptureActive(); }, 300);
}

function setParamLive (id, normalized) {
  normalized = Math.max(0, Math.min(1, normalized));
  currentValues[id] = normalized;
  const el = document.querySelector(`.knob[data-param="${id}"]`);
  if (el) applyKnobVisual(el, id, normalized);
  nf.setParam(id, normalized);
  scheduleAbCapture();
  if (isEqRelevant(id)) refreshEqCurve();
}

function bindKnobDrag (el) {
  const id = el.dataset.param;
  let startY = 0, startVal = 0, active = false;

  const onMove = (e) => {
    if (!active) return;
    const y = e.touches ? e.touches[0].clientY : e.clientY;
    const delta = (startY - y) / (currentStageScale || 1);
    setParamLive(id, startVal + delta * 0.005);
  };
  const onUp = () => {
    if (!active) return;
    active = false;
    draggingIds.delete(id);
    window.removeEventListener("pointermove", onMove);
    window.removeEventListener("pointerup", onUp);
  };
  el.addEventListener("pointerdown", (e) => {
    active = true;
    draggingIds.add(id);
    startY = e.clientY;
    startVal = currentValues[id] ?? 0;
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
    e.preventDefault();
  });
  // Deliberately no dblclick-to-reset: two quick successive drag/release
  // gestures (a normal way to nudge a knob) get counted as a dblclick by
  // the browser even with real movement in between, silently snapping
  // the value back to default mid-adjustment.
}

function isEqRelevant (id) {
  return id.startsWith("low") || id.startsWith("high") || id.startsWith("hpf") || id.startsWith("lpf") || id === "eg_mode";
}

// ── EQ curve (real analytic response from C++, log-freq grid + axes) ────
const FREQ_MIN = 20, FREQ_MAX = 20000;
const PLOT_X0 = 40, PLOT_X1 = 590, PLOT_Y_TOP = 10, PLOT_Y_MID = 70, PLOT_Y_BOT = 130;

function freqToX (f) {
  const t = (Math.log10(f) - Math.log10(FREQ_MIN)) / (Math.log10(FREQ_MAX) - Math.log10(FREQ_MIN));
  return PLOT_X0 + t * (PLOT_X1 - PLOT_X0);
}
function dbToYPlot (db) {
  return Math.max(PLOT_Y_TOP, Math.min(PLOT_Y_BOT, PLOT_Y_MID - (db / 15) * (PLOT_Y_MID - PLOT_Y_TOP)));
}

function initEqCurveChrome () {
  const svg = document.getElementById("eqCurve");
  if (!svg) return;
  const freqTicks = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
  const freqLabels = ["20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"];
  const dbTicks = [15, 0, -15];
  let grid = "";
  freqTicks.forEach((f, i) => {
    const x = freqToX(f).toFixed(1);
    grid += `<line class="curveGrid" x1="${x}" y1="${PLOT_Y_TOP}" x2="${x}" y2="${PLOT_Y_BOT}"/>`;
    grid += `<text class="curveAxisLabel" x="${x}" y="${PLOT_Y_BOT + 11}" text-anchor="middle">${freqLabels[i]}</text>`;
  });
  dbTicks.forEach((db) => {
    const y = dbToYPlot(db).toFixed(1);
    grid += `<line class="curveGrid" x1="${PLOT_X0}" y1="${y}" x2="${PLOT_X1}" y2="${y}"/>`;
    grid += `<text class="curveAxisLabel" x="${PLOT_X0 - 6}" y="${(parseFloat(y) + 3).toFixed(1)}" text-anchor="end">${db > 0 ? "+" + db : db}</text>`;
  });
  svg.innerHTML = grid
    + `<path id="eqCurvePath" d="" fill="none" stroke="#e0562e" stroke-width="2"/>`
    + `<g id="curveMarkers"></g>`;
}

const BAND_MARKER_IDS = ["low_freq", "low_mid_freq", "high_mid_freq", "high_freq"];

function updateCurveMarkers (points) {
  const g = document.getElementById("curveMarkers");
  if (!g) return;
  let html = "";
  BAND_MARKER_IDS.forEach((id, i) => {
    const f = rawFloatValue(id);
    if (f == null) return;
    const t = (Math.log10(f) - Math.log10(FREQ_MIN)) / (Math.log10(FREQ_MAX) - Math.log10(FREQ_MIN));
    const idx = Math.max(0, Math.min(points.length - 1, Math.round(t * (points.length - 1))));
    const x = freqToX(f);
    const y = dbToYPlot(points[idx]);
    html += `<circle class="curveBandMarker" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="7"/>`;
    html += `<text class="curveBandMarkerText" x="${x.toFixed(1)}" y="${(y + 3).toFixed(1)}">${i + 1}</text>`;
  });
  g.innerHTML = html;
}

async function refreshEqCurve () {
  const points = await nf.getEqCurve();
  if (!points || !points.length) return;
  let d = "";
  points.forEach((db, i) => {
    const t = i / (points.length - 1);
    const f = FREQ_MIN * Math.pow(FREQ_MAX / FREQ_MIN, t);
    const x = freqToX(f);
    const y = dbToYPlot(db);
    d += (i === 0 ? "M" : "L") + x.toFixed(1) + "," + y.toFixed(1) + " ";
  });
  const path = document.getElementById("eqCurvePath");
  if (path) path.setAttribute("d", d.trim());
  updateCurveMarkers(points);
}

// ── Meters ────────────────────────────────────────────────────────────
function dbFromLinear (lin) { return lin > 0.00003 ? 20 * Math.log10(lin) : -120; }

function updateMeterColumn (prefix, db) {
  const thresholds = [-24, -18, -12, -6, 0];
  for (let i = 0; i < 5; i++) {
    const led = document.querySelector(`[data-meter="${prefix}"] [data-led="${i}"]`);
    if (!led) continue;
    const lit = db >= thresholds[i];
    led.classList.toggle("lit-green", lit && i < 3);
    led.classList.toggle("lit-amber", lit && i === 3);
    led.classList.toggle("lit-red", lit && i === 4);
  }
  const ol = document.querySelector(`[data-meter="${prefix}"] [data-led="5"]`);
  if (ol) ol.classList.toggle("lit-red", db >= 0.0);
}

window.updateMeters = function (inLevel, outLevel) {
  updateMeterColumn("input", dbFromLinear(inLevel));
  updateMeterColumn("output", dbFromLinear(outLevel));
};

// ── Live automation reflection ──────────────────────────────────────────
window.onParamsPushed = function (values) {
  let changed = false;
  for (const id in values) {
    if (draggingIds.has(id)) continue; // never fight an in-progress local drag
    if (currentValues[id] !== values[id]) {
      currentValues[id] = values[id];
      changed = true;
      const el = document.querySelector(`.knob[data-param="${id}"]`);
      if (el) applyKnobVisual(el, id, values[id]);
    }
  }
  if (changed) refreshAllVisuals();
};

// ── Toggle buttons (IN / Bell-Shelf / E-G) ──────────────────────────────
function bindToggleButtons () {
  document.querySelectorAll("[data-toggle]").forEach((el) => {
    el.addEventListener("click", () => {
      const id = el.dataset.toggle;
      const next = (currentValues[id] ?? 0) > 0.5 ? 0 : 1;
      currentValues[id] = next;
      el.classList.toggle("active", next > 0.5);
      nf.setParam(id, next);
      scheduleAbCapture();
      if (isEqRelevant(id)) refreshEqCurve();
    });
  });

  document.querySelectorAll("[data-choiceToggle]").forEach((el) => {
    el.addEventListener("click", () => {
      const id = el.getAttribute("data-choiceToggle");
      const meta = paramMeta[id];
      if (!meta) return;
      const curIdx = Math.round((currentValues[id] ?? 0) * (meta.choices.length - 1));
      const nextIdx = (curIdx + 1) % meta.choices.length;
      const normalized = nextIdx / (meta.choices.length - 1);
      currentValues[id] = normalized;
      nf.setParam(id, normalized);
      scheduleAbCapture();
      refreshAllVisuals();
      refreshEqCurve();
    });
  });

  document.querySelectorAll("[data-choiceParam]").forEach((el) => {
    el.addEventListener("click", () => {
      const id = el.getAttribute("data-choiceParam");
      const meta = paramMeta[id];
      if (!meta) return;
      const idx = parseInt(el.dataset.choice, 10);
      const normalized = idx / (meta.choices.length - 1);
      currentValues[id] = normalized;
      nf.setParam(id, normalized);
      scheduleAbCapture();
      refreshAllVisuals();
      refreshEqCurve();
    });
  });
}

// ── Preset browser ───────────────────────────────────────────────────────
let presets = [];
async function refreshPresetDisplay () {
  const index = await nf.getCurrentPresetIndex();
  const preset = presets[index];
  if (preset) {
    document.getElementById("presetName").textContent = preset.name;
    document.getElementById("presetCategory").textContent = preset.category;
  }
  await refreshAllParamsFromHost();
}

async function refreshAllParamsFromHost () {
  const metas = await nf.getParamsMeta();
  (metas || []).forEach((m) => {
    paramMeta[m.id] = m;
    currentValues[m.id] = m.type === "choice"
      ? (m.choices.length > 1 ? m.value / (m.choices.length - 1) : 0)
      : m.type === "bool" ? (m.value ? 1 : 0)
      : (m.value - m.min) / (m.max - m.min);
  });
  refreshAllVisuals();
  refreshEqCurve();
}

function bindPresetBrowser () {
  document.getElementById("presetPrev").addEventListener("click", async () => { await nf.loadPreviousPreset(); await refreshPresetDisplay(); });
  document.getElementById("presetNext").addEventListener("click", async () => { await nf.loadNextPreset(); await refreshPresetDisplay(); });
}

// ── A/B ───────────────────────────────────────────────────────────────
async function refreshAbDisplay () {
  const state = await nf.abGetState();
  if (!state) return;
  document.getElementById("abA").classList.toggle("active", state.activeSlot === "A");
  document.getElementById("abB").classList.toggle("active", state.activeSlot === "B");
}

function bindAb () {
  document.getElementById("abA").addEventListener("click", async () => {
    const state = await nf.abGetState();
    if (state && state.activeSlot !== "A") { await nf.abToggleAndRecall(); await refreshAbDisplay(); await refreshAllParamsFromHost(); }
  });
  document.getElementById("abB").addEventListener("click", async () => {
    const state = await nf.abGetState();
    if (state && state.activeSlot !== "B") { await nf.abToggleAndRecall(); await refreshAbDisplay(); await refreshAllParamsFromHost(); }
  });
}

// ── Settings menu (oversampling, bypass) ────────────────────────────────
function bindSettingsMenu () {
  const menu = document.getElementById("settingsMenu");
  document.getElementById("menuBtn").addEventListener("click", () => menu.classList.toggle("hidden"));
  document.addEventListener("click", (e) => {
    if (!menu.contains(e.target) && e.target.id !== "menuBtn") menu.classList.add("hidden");
  });

  const ovs = document.getElementById("oversamplingSelect");
  ovs.addEventListener("change", () => {
    const meta = paramMeta["oversampling_mode"];
    if (!meta) return;
    const idx = parseInt(ovs.value, 10);
    nf.setParam("oversampling_mode", idx / (meta.choices.length - 1));
  });

  const bypassCb = document.getElementById("bypassToggle");
  bypassCb.addEventListener("change", () => {
    nf.setParam("bypass", bypassCb.checked ? 1 : 0);
  });
}

// ── Save preset ──────────────────────────────────────────────────────
function bindSavePresetMenu () {
  const menu = document.getElementById("savePresetMenu");
  const nameInput = document.getElementById("savePresetName");

  const openMenu = () => {
    menu.classList.remove("hidden");
    nameInput.value = "";
    nameInput.focus();
  };
  const closeMenu = () => menu.classList.add("hidden");

  const confirmSave = async () => {
    const name = nameInput.value.trim();
    if (!name) { nameInput.focus(); return; }
    const newIndex = await nf.savePreset(name);
    closeMenu();
    if (newIndex >= 0) {
      presets = (await nf.listPresets()) || [];
      await refreshPresetDisplay();
    }
  };

  document.getElementById("saveBtn").addEventListener("click", openMenu);
  document.getElementById("savePresetCancel").addEventListener("click", closeMenu);
  document.getElementById("savePresetConfirm").addEventListener("click", confirmSave);
  nameInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") confirmSave();
    else if (e.key === "Escape") closeMenu();
  });
  document.addEventListener("click", (e) => {
    if (!menu.contains(e.target) && e.target.id !== "saveBtn") closeMenu();
  });
}

// ── Boot ─────────────────────────────────────────────────────────────
(async () => {
  initEqCurveChrome();
  await refreshAllParamsFromHost();
  renderAllTicks();

  presets = (await nf.listPresets()) || [];

  document.querySelectorAll(".knob[data-param]").forEach(bindKnobDrag);
  bindToggleButtons();
  bindPresetBrowser();
  bindAb();
  bindSettingsMenu();
  bindSavePresetMenu();

  await refreshPresetDisplay();
  await refreshAbDisplay();

  const ovsMeta = paramMeta["oversampling_mode"];
  if (ovsMeta) document.getElementById("oversamplingSelect").value = String(Math.round(currentValues["oversampling_mode"] * (ovsMeta.choices.length - 1)));
  document.getElementById("bypassToggle").checked = (currentValues["bypass"] ?? 0) > 0.5;
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
    const scale = Math.min(w / 1180, h / 530);
    currentStageScale = scale || 1;
    const stage = document.getElementById("app");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();
