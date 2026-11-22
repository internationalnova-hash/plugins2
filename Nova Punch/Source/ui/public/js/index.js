import { getSliderState, getToggleState, getComboBoxState, getNativeFunction } from "./juce/index.js";

function hasNative() {
  return typeof window.__JUCE__ !== "undefined";
}

// ---------------------------------------------------------------------
// Stage scale-to-fit (same technique used catalog-wide) - initial open
// size is set below 100% of this design canvas from day one
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
    const scale = Math.min(w / 1536, h / 1024);
    window.__novaPunchScale = scale || 1;
    const stage = document.getElementById("app");
    if (stage) {
      const scaledW = 1536 * scale, scaledH = 1024 * scale;
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
// Runtime SVG tick-ring rendering (never baked into knob PNGs).
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
    line.setAttribute("stroke", "#6a6a6a");
    line.setAttribute("stroke-width", "1");
    svg.appendChild(line);
  }
}

function renderKnobScale(containerId, min, max, step, labelFn) {
  const container = document.getElementById(containerId);
  if (!container) return;

  const box = 192, c = box / 2;
  const tickInnerR = 68, tickOuterR = 86, numberR = 100;

  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);

  addMinorTicks(svg, c, 70, 82, Math.round((max - min) / (step / 4)));

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
    line.setAttribute("stroke", "#8a8a8a");
    line.setAttribute("stroke-width", "2");
    line.setAttribute("stroke-linecap", "round");
    svg.appendChild(line);

    const tx = c + numberR * sin, ty = c - numberR * cos;
    const text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", "11");
    text.setAttribute("font-weight", "700");
    text.setAttribute("fill", "#8a8a8a");
    text.textContent = labelFn ? labelFn(v) : Math.round(v);
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
  addMinorTicks(svg, c, 46, 58, 20);
  container.innerHTML = "";
  container.appendChild(svg);
}

// Active-value arc drawn at runtime (the asset pack's own active-arc PNG
// is a static style reference, not a value indicator - see its README).
function renderArc(containerId, box, radius, norm) {
  const container = document.getElementById(containerId);
  if (!container) return;
  const c = box / 2;
  const startDeg = -135, sweepDeg = 270 * Math.max(0, Math.min(1, norm));
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 " + box + " " + box);
  if (sweepDeg > 0.5) {
    const a0 = (Math.PI / 180) * startDeg;
    const a1 = (Math.PI / 180) * (startDeg + sweepDeg);
    const x0 = c + radius * Math.sin(a0), y0 = c - radius * Math.cos(a0);
    const x1 = c + radius * Math.sin(a1), y1 = c - radius * Math.cos(a1);
    const largeArc = sweepDeg > 180 ? 1 : 0;
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", `M ${x0} ${y0} A ${radius} ${radius} 0 ${largeArc} 1 ${x1} ${y1}`);
    path.setAttribute("fill", "none");
    path.setAttribute("stroke", "#ff7a12");
    path.setAttribute("stroke-width", "4");
    path.setAttribute("stroke-linecap", "round");
    path.setAttribute("opacity", "0.9");
    svg.appendChild(path);
  }
  container.innerHTML = "";
  container.appendChild(svg);
}

// ---------------------------------------------------------------------
// Continuous knobs (Punch, Body, Sensitivity, Mix).
// ---------------------------------------------------------------------
function wireKnob(elId, opts) {
  const el = document.getElementById(elId);
  if (!el) return null;
  const dial = el.querySelector(".knobDial");

  const state = hasNative() ? getSliderState(opts.name) : null;

  function currentNorm() {
    return state ? state.getNormalisedValue() : 0.5;
  }

  function applyVisual() {
    const norm = Math.max(0, Math.min(1, currentNorm()));
    const angle = -135 + norm * 270;
    if (dial) dial.style.setProperty("--angle", angle + "deg");
    if (opts.arcId) renderArc(opts.arcId, opts.arcBox || 192, opts.arcRadius || 112, norm);
    if (opts.valueId) {
      const el2 = document.getElementById(opts.valueId);
      if (el2) el2.textContent = opts.formatValue ? opts.formatValue(norm) : Math.round(norm * 100);
    }
    if (opts.onChange) opts.onChange(state, norm);
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  let dragging = false, startY = 0, startNorm = 0;
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
    const scale = window.__novaPunchScale || 1;
    const deltaNorm = ((startY - e.clientY) / scale) / dragRangePx;
    const newNorm = Math.max(0, Math.min(1, startNorm + deltaNorm));
    state.setNormalisedValue(newNorm);
    applyVisual();
  });
  function endDrag() {
    if (!dragging) return;
    dragging = false;
    if (state && state.sliderDragEnded) state.sliderDragEnded();
  }
  el.addEventListener("pointerup", endDrag);
  el.addEventListener("pointercancel", endDrag);

  return { refresh: applyVisual, getState: () => state };
}

// ---------------------------------------------------------------------
// Button groups (bool or choice-backed).
// ---------------------------------------------------------------------
function wireBoolButtonGroup(containerId, name) {
  const container = document.getElementById(containerId);
  if (!container) return;
  const buttons = Array.prototype.slice.call(container.querySelectorAll("[data-idx]"));
  const state = hasNative() ? getToggleState(name) : null;

  function applyVisual() {
    const on = state ? state.getValue() : false;
    buttons.forEach((b) => b.classList.toggle("active", (parseInt(b.dataset.idx, 10) === 1) === on));
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
  const buttons = Array.prototype.slice.call(container.querySelectorAll("[data-idx]"));
  const state = hasNative() ? getComboBoxState(name) : null;

  function applyVisual() {
    const idx = state ? state.getChoiceIndex() : 0;
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

// ---------------------------------------------------------------------
// Tabs (UI-only state, not an automatable parameter - per spec).
// ---------------------------------------------------------------------
let currentTab = "transient";
function bindTabs() {
  const buttons = Array.prototype.slice.call(document.querySelectorAll(".tabbtn"));
  buttons.forEach((b) => {
    b.addEventListener("click", () => {
      currentTab = b.dataset.tab;
      buttons.forEach((x) => x.classList.toggle("active", x === b));
      renderAnalysis(lastTelemetry);
    });
  });
}

// ---------------------------------------------------------------------
// Real (not decorative) telemetry-driven displays.
// ---------------------------------------------------------------------
let lastTelemetry = null;
const REGION_NAMES = ["IDLE", "ATTACK", "DECAY", "SUSTAIN"];

function renderMeter(barId, peak) {
  const bar = document.getElementById(barId);
  if (!bar) return;
  const db = peak > 1e-6 ? 20 * Math.log10(peak) : -80;
  const norm = Math.max(0, Math.min(1, (db + 60) / 60)); // -60..0dB -> 0..1
  bar.style.height = (norm * 100).toFixed(1) + "%";
}

function svgEl(tag, attrs) {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const k in attrs) el.setAttribute(k, attrs[k]);
  return el;
}

function renderAnalysis(t) {
  const svg = document.getElementById("analysisSvg");
  if (!svg || !t) return;
  svg.innerHTML = "";

  const W = 480, H = 246, mid = H / 2;

  if (currentTab === "spectrum" || currentTab === "both") {
    const bands = [
      { label: "LOW", v: t.lowActivity, x: currentTab === "both" ? 20 : 40 },
      { label: "MID", v: t.midActivity, x: currentTab === "both" ? 220 : 220 },
      { label: "HIGH", v: t.highActivity, x: currentTab === "both" ? 420 : 400 },
    ];
    const barW = currentTab === "both" ? 60 : 90;
    const areaH = currentTab === "both" ? H * 0.45 : H - 30;
    const baseY = currentTab === "both" ? areaH + 10 : H - 10;
    bands.forEach((b) => {
      const h = Math.max(2, b.v * (areaH - 10));
      svg.appendChild(svgEl("rect", {
        x: b.x, y: baseY - h, width: barW, height: h,
        fill: "#ff7a12", opacity: "0.85", rx: 3,
      }));
      svg.appendChild(svgEl("text", {
        x: b.x + barW / 2, y: baseY + 16, "text-anchor": "middle",
        "font-size": "11", "font-weight": "700", fill: "#8a8a8a",
      })).textContent = b.label;
    });
  }

  if (currentTab === "transient" || currentTab === "both") {
    const yOffset = currentTab === "both" ? H * 0.5 : 0;
    const areaH = currentTab === "both" ? H * 0.42 : H - 20;
    const localMid = yOffset + areaH / 2 + 10;

    const hist = t.punchHistory || [];
    const n = hist.length || 1;
    let pathD = "";
    for (let i = 0; i < n; i++) {
      const x = (i / (n - 1)) * W;
      const amp = Math.max(0, Math.min(1, hist[i])) * (areaH * 0.42);
      const y = localMid - amp;
      pathD += (i === 0 ? "M" : "L") + x.toFixed(1) + " " + y.toFixed(1) + " ";
    }
    for (let i = n - 1; i >= 0; i--) {
      const x = (i / (n - 1)) * W;
      const amp = Math.max(0, Math.min(1, hist[i])) * (areaH * 0.42);
      const y = localMid + amp;
      pathD += "L" + x.toFixed(1) + " " + y.toFixed(1) + " ";
    }
    svg.appendChild(svgEl("path", { d: pathD, fill: "#ff7a12", opacity: "0.55" }));

    // Region marker (ATTACK/PEAK/SUSTAIN) driven by real telemetry.
    const regionColor = t.region === 1 ? "#ff7a12" : (t.region === 3 ? "#ff7a12" : "#9a9a9a");
    svg.appendChild(svgEl("line", {
      x1: W - 30, y1: yOffset + 6, x2: W - 30, y2: yOffset + areaH,
      stroke: regionColor, "stroke-width": "2",
    }));
    svg.appendChild(svgEl("text", {
      x: W - 30, y: yOffset + 6, "text-anchor": "middle",
      "font-size": "10", "font-weight": "800", fill: regionColor,
    })).textContent = REGION_NAMES[t.region] || "IDLE";
  }
}

function renderActivity(t) {
  const svg = document.getElementById("activitySvg");
  if (!svg || !t) return;
  svg.innerHTML = "";

  const W = 560, H = 118;
  const punchHist = t.punchHistory || [];
  const bodyHist = t.bodyHistory || [];
  const n = Math.max(punchHist.length, bodyHist.length, 1);
  const barW = W / n;

  for (let i = 0; i < n; i++) {
    const x = i * barW;
    const pv = Math.max(0, Math.min(1, punchHist[i] || 0));
    const bv = Math.max(0, Math.min(1, bodyHist[i] || 0));
    const ph = pv * (H * 0.85);
    const bh = bv * (H * 0.85);
    svg.appendChild(svgEl("rect", { x: x + 1, y: H - ph, width: Math.max(1, barW - 2), height: ph, fill: "#ff7a12", opacity: "0.9" }));
    svg.appendChild(svgEl("rect", { x: x + 1, y: H - Math.max(ph, bh) - (bh > ph ? 0 : 0), width: Math.max(1, barW - 2), height: bh, fill: "#9a9a9a", opacity: "0.55" }));
  }
}

window.updateTelemetry = function (t) {
  lastTelemetry = t;

  renderMeter("meterInL", t.inputPeakL);
  renderMeter("meterInR", t.inputPeakR);
  renderMeter("meterOutL", t.outputPeakL);
  renderMeter("meterOutR", t.outputPeakR);

  const attackEl = document.getElementById("rvAttack");
  const decayEl = document.getElementById("rvDecay");
  const sustainEl = document.getElementById("rvSustain");
  if (attackEl) attackEl.textContent = (t.attackGainDb >= 0 ? "+" : "") + t.attackGainDb.toFixed(1) + " dB";
  if (decayEl) decayEl.textContent = (t.decayGainDb >= 0 ? "+" : "") + t.decayGainDb.toFixed(1) + " dB";
  if (sustainEl) sustainEl.textContent = (t.sustainGainDb >= 0 ? "+" : "") + t.sustainGainDb.toFixed(1) + " dB";

  renderAnalysis(t);
  renderActivity(t);
};

// ---------------------------------------------------------------------
// Preset browser.
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
  renderKnobScale("ticksPunch", -100, 100, 50, (v) => (v > 0 ? "+" : "") + v);
  renderKnobScale("ticksBody", -100, 100, 50, (v) => (v > 0 ? "+" : "") + v);
  renderSmallKnobScale("ticksSensitivity");
  renderSmallKnobScale("ticksMix");

  wireKnob("knobPunch", {
    name: "punch", dragRangePx: 160, arcId: "arcPunch", arcBox: 230, arcRadius: 128,
    valueId: "valPunch", formatValue: (norm) => Math.round(norm * 100),
  });
  wireKnob("knobBody", {
    name: "body", dragRangePx: 160, arcId: "arcBody", arcBox: 230, arcRadius: 128,
    valueId: "valBody", formatValue: (norm) => Math.round(norm * 100),
  });
  wireKnob("knobSensitivity", {
    name: "sensitivity", dragRangePx: 130, arcId: "arcSensitivity", arcBox: 78, arcRadius: 43,
    valueId: "valSensitivity", formatValue: (norm) => Math.round(norm * 100),
  });
  wireKnob("knobMix", {
    name: "mix", dragRangePx: 130, arcId: "arcMix", arcBox: 78, arcRadius: 43,
    valueId: "valMix", formatValue: (norm) => Math.round(norm * 100),
  });

  wireChoiceButtonGroup("focusGroup", "focus");
  wireBoolButtonGroup("clipSafeGroup", "clip_safe");

  bindTabs();
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
