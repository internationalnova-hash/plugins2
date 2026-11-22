import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of [
  "setParam", "getParamsMeta", "getPresetsMeta", "loadPreset", "savePreset",
  "abGetState", "abCaptureActive", "abToggleAndRecall", "abCopyActiveToOther", "jsError",
]) nf[name] = getNativeFunction(name);

window.addEventListener("error", (e) => { try { nf.jsError(String(e.message || e)); } catch (_) {} });

const $ = (id) => document.getElementById(id);

let paramsMeta = {};   // id -> meta object from getParamsMeta
let currentValues = {}; // id -> real value
let presets = [];
let presetIndex = 0;
// Current #stage CSS transform:scale() factor (kept in sync by fitStage()
// at the bottom of this file). Pointer events report clientX/clientY in
// real screen pixels, which a CSS transform on an ancestor does NOT
// affect - so drag handlers that convert raw pixel delta into a value
// change must divide by this to stay correctly calibrated once the
// window (and therefore the visual knob size) is no longer 1:1 with the
// design canvas. Without it, dragging feels sluggish when the window is
// smaller than the design canvas (knob visually shrunk, same real-pixel
// throw still required) and twitchy when larger (knob visually grown,
// same tiny real-pixel throw still enough for full range).
let currentStageScale = 1;

function fmtValue(id, v) {
  switch (id) {
    case "input": case "output": case "trim": return v.toFixed(1) + " dB";
    case "tapeAmount": case "mix": return Math.round(v) + "%";
    case "bias": case "wowFlutter": case "noise": return v.toFixed(1);
    case "lowCut": return Math.round(v) + " Hz";
    case "highCut": return (v / 1000).toFixed(1) + " kHz";
    default: return String(v);
  }
}

// The knob photo supplies only the static outer ring; the inner cap is a
// procedural brushed-metal disc (.grainSpin) that genuinely rotates with
// value, carrying the pointer mark with it, while a separate highlight
// layer stays fixed on top. See the CSS comment above .knobWrap .rotor
// in index.html for why (photo highlight vs. procedural grain).
function angleFor(min, max, value) {
  const pct = Math.max(0, Math.min(1, (value - min) / (max - min)));
  return -135 + pct * 270;
}
// ---- TAPE knob's value ring (SVG arc, -135deg..+135deg sweep) ----
function polarPoint(cx, cy, r, angleDeg) {
  const rad = (angleDeg * Math.PI) / 180;
  return { x: cx + r * Math.sin(rad), y: cy - r * Math.cos(rad) };
}
function arcPath(cx, cy, r, startDeg, endDeg) {
  if (endDeg <= startDeg) return "";
  if (endDeg - startDeg >= 359.999) endDeg = startDeg + 359.999;
  const start = polarPoint(cx, cy, r, startDeg);
  const end = polarPoint(cx, cy, r, endDeg);
  const largeArc = endDeg - startDeg > 180 ? 1 : 0;
  return `M ${start.x} ${start.y} A ${r} ${r} 0 ${largeArc} 1 ${end.x} ${end.y}`;
}
function initTapeRing() {
  const svg = document.querySelector('.knobWrap[data-param="tapeAmount"] .tapeRing');
  if (!svg) return;
  const cx = 90.5, cy = 86.5, r = 80;
  svg.querySelector(".ringTrack").setAttribute("d", arcPath(cx, cy, r, -135, 135));
}
function updateTapeRing(value) {
  const svg = document.querySelector('.knobWrap[data-param="tapeAmount"] .tapeRing');
  if (!svg) return;
  const cx = 90.5, cy = 86.5, r = 80;
  const pct = Math.max(0, Math.min(1, value / 100));
  svg.querySelector(".ringFill").setAttribute("d", arcPath(cx, cy, r, -135, -135 + pct * 270));
}

function setKnobVisual(knobEl, value) {
  const min = parseFloat(knobEl.dataset.min);
  const max = parseFloat(knobEl.dataset.max);
  const deg = angleFor(min, max, value);
  const spin = knobEl.querySelector(".grainSpin");
  if (spin) spin.style.transform = `rotate(${deg}deg)`;
}

function applyValueToUI(id, value) {
  currentValues[id] = value;
  const knobEl = document.querySelector(`.knobWrap[data-param="${id}"]`);
  if (knobEl) setKnobVisual(knobEl, value);
  const labelEl = $("val_" + id);
  if (labelEl) labelEl.textContent = fmtValue(id, value);
  if (id === "tapeAmount") {
    $("tapeValLbl").textContent = Math.round(value) + "%";
    updateTapeRing(value);
  }
  if (id === "mix") {
    const min = 0, max = 100;
    const pct = Math.max(0, Math.min(1, (value - min) / (max - min)));
    // Track goes from x=6 to x=252 within the 258px-wide asset (leaving
    // room for the baked-in end caps); fill/handle both driven by the
    // SAME live value, independent of whatever position the asset's own
    // baked-in thumb graphic happens to show.
    const trackStart = 6, trackEnd = 252;
    const x = trackStart + pct * (trackEnd - trackStart);
    $("mixFill").style.width = (x - trackStart) + "px";
    $("mixHandle").style.left = x + "px";
  }
  if (id === "tapeType") updateChoiceGroup("tapeTypeGroup", value);
  if (id === "ips") updateChoiceGroup("ipsGroup", value);
}

function updateChoiceGroup(groupId, activeIndex) {
  const group = $(groupId);
  if (!group) return;
  group.querySelectorAll("button[data-choice]").forEach((btn) => {
    btn.classList.toggle("active", parseInt(btn.dataset.choice, 10) === activeIndex);
  });
}

async function refreshAll() {
  const meta = await nf.getParamsMeta();
  paramsMeta = {};
  for (const m of meta) {
    paramsMeta[m.id] = m;
    applyValueToUI(m.id, m.value);
  }
}

// ---- generic rotary knob drag (vertical throw, direct 1:1 visual follow) ----
function initKnob(knobEl) {
  const paramId = knobEl.dataset.param;
  const min = parseFloat(knobEl.dataset.min);
  const max = parseFloat(knobEl.dataset.max);
  const range = max - min;
  let dragState = null;

  const getClientY = (e) => (e.touches ? e.touches[0].clientY : e.clientY);

  const startDrag = (event) => {
    if (typeof event.button === "number" && event.button !== 0) return;
    event.preventDefault();
    dragState = { startY: getClientY(event), startVal: currentValues[paramId] ?? min };
    if (knobEl.setPointerCapture && event.pointerId !== undefined) {
      try { knobEl.setPointerCapture(event.pointerId); } catch (e) {}
    }
  };
  const moveDrag = (event) => {
    if (!dragState) return;
    event.preventDefault();
    const delta = (dragState.startY - getClientY(event)) / (currentStageScale || 1);
    const next = Math.max(min, Math.min(max, dragState.startVal + delta * range * 0.006));
    applyValueToUI(paramId, next);
    nf.setParam(paramId, next);
  };
  const endDrag = () => { dragState = null; };

  knobEl.addEventListener("pointerdown", startDrag);
  window.addEventListener("pointermove", moveDrag);
  window.addEventListener("pointerup", endDrag);
  knobEl.addEventListener("dblclick", () => {
    const meta = paramsMeta[paramId];
    if (meta) { applyValueToUI(paramId, meta.value); nf.setParam(paramId, meta.value); }
  });
}

function initChoiceGroup(groupId, paramId) {
  const group = $(groupId);
  group.querySelectorAll("button[data-choice]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const idx = parseInt(btn.dataset.choice, 10);
      applyValueToUI(paramId, idx);
      nf.setParam(paramId, idx);
    });
  });
}

function initMixSlider() {
  const track = $("mixTrack");
  let dragging = false;
  const setFromClientX = (clientX) => {
    const rect = track.getBoundingClientRect();
    const pct = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
    const value = pct * 100;
    applyValueToUI("mix", value);
    nf.setParam("mix", value);
  };
  track.addEventListener("pointerdown", (e) => { dragging = true; setFromClientX(e.clientX); });
  window.addEventListener("pointermove", (e) => { if (dragging) setFromClientX(e.clientX); });
  window.addEventListener("pointerup", () => { dragging = false; });
}

// ---- presets ----
async function loadPresetsMeta() {
  presets = await nf.getPresetsMeta();
}
function showPresetAt(index) {
  if (presets.length === 0) return;
  presetIndex = ((index % presets.length) + presets.length) % presets.length;
  $("presetName").textContent = presets[presetIndex].name;
}

// ---- LED meters (real lamp sprites over the led_meter_vertical.png housing) ----
function buildLedMeter(el, count) {
  el.innerHTML = "";
  const usableTop = 14, usableBottom = 14; // inset from the housing's own end caps
  const span = 197 - usableTop - usableBottom;
  for (let i = 0; i < count; ++i) {
    // i=0 is the BOTTOM (green) lamp, i=count-1 is the TOP (red) lamp.
    const color = i >= count - 2 ? "red" : (i >= count - 5 ? "amber" : "green");
    const img = document.createElement("img");
    img.className = "lamp";
    img.dataset.color = color;
    img.src = `images/lamp_${color}.png`;
    const yFromBottom = usableTop + (span * i) / (count - 1);
    img.style.top = (197 - yFromBottom - 10) + "px";
    el.appendChild(img);
  }
}
function updateLedMeter(el, level0to1) {
  const lamps = el.children;
  const lit = Math.round(level0to1 * lamps.length);
  for (let i = 0; i < lamps.length; ++i)
    lamps[i].classList.toggle("lit", i < lit);
}

buildLedMeter($("ledIn"), 6);
buildLedMeter($("ledOut"), 6);

// Shared SVG element builder (used by the procedural VU gauge below).
const SVG_NS = "http://www.w3.org/2000/svg";
function svgEl(tag, attrs) {
  const el = document.createElementNS(SVG_NS, tag);
  for (const k in attrs) el.setAttribute(k, attrs[k]);
  return el;
}

// ---- procedural VU gauge face (replaces the photo's baked face+needle
// entirely - see the .vuGauge CSS comment for why) ----
const VU_PIVOT = { x: 107.5, y: 150 };
const VU_TICKS = [
  { v: -20, deg: -50, major: true }, { v: -10, deg: -32, major: true },
  { v: -7, deg: -24, major: true }, { v: -5, deg: -18, major: true },
  { v: -3, deg: -11, major: true }, { v: -1, deg: -4, major: true },
  { v: 0, deg: 0, major: true }, { v: 1, deg: 8, major: true, red: true },
  { v: 2, deg: 16, major: true, red: true }, { v: 3, deg: 24, major: true, red: true },
];
function buildVuGauge(svg) {
  svg.innerHTML = "";
  const gradId = svg.id + "_grad";
  const defs = svgEl("defs", {});
  const grad = svgEl("radialGradient", { id: gradId, cx: "50%", cy: "35%", r: "75%" });
  grad.appendChild(svgEl("stop", { offset: "0%", "stop-color": "#e9c98e" }));
  grad.appendChild(svgEl("stop", { offset: "100%", "stop-color": "#c99c5e" }));
  defs.appendChild(grad);
  svg.appendChild(defs);

  svg.appendChild(svgEl("rect", {
    x: 1, y: 1, width: 213, height: 124, rx: 5,
    fill: `url(#${gradId})`, stroke: "#3a2a12", "stroke-width": 2,
  }));

  const arcPts = [];
  for (let d = -52; d <= 26; d += 2) arcPts.push(polarPoint(VU_PIVOT.x, VU_PIVOT.y, 96, d));
  svg.appendChild(svgEl("polyline", {
    points: arcPts.map((p) => `${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(" "),
    fill: "none", stroke: "#2a2016", "stroke-width": 1.6,
  }));
  const redPts = [];
  for (let d = 8; d <= 26; d += 2) redPts.push(polarPoint(VU_PIVOT.x, VU_PIVOT.y, 96, d));
  svg.appendChild(svgEl("polyline", {
    points: redPts.map((p) => `${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(" "),
    fill: "none", stroke: "#a8362a", "stroke-width": 2.6,
  }));

  for (const t of VU_TICKS) {
    const inner = polarPoint(VU_PIVOT.x, VU_PIVOT.y, 88, t.deg);
    const outer = polarPoint(VU_PIVOT.x, VU_PIVOT.y, 100, t.deg);
    svg.appendChild(svgEl("line", {
      x1: inner.x, y1: inner.y, x2: outer.x, y2: outer.y,
      stroke: t.red ? "#a8362a" : "#2a2016", "stroke-width": t.v === 0 ? 2.2 : 1.4,
    }));
    const lp = polarPoint(VU_PIVOT.x, VU_PIVOT.y, 111, t.deg);
    const label = svgEl("text", {
      x: lp.x, y: lp.y + 3, "text-anchor": "middle",
      "font-size": 10, "font-family": "Helvetica, Arial, sans-serif",
      fill: t.red ? "#a8362a" : "#2a2016",
    });
    label.textContent = (t.v > 0 ? "+" : "") + t.v;
    svg.appendChild(label);
  }

  const vuText = svgEl("text", {
    x: VU_PIVOT.x, y: 88, "text-anchor": "middle",
    "font-size": 22, "font-family": "Georgia, 'Times New Roman', serif",
    "font-weight": "bold", fill: "#2a2016",
  });
  vuText.textContent = "VU";
  svg.appendChild(vuText);

  svg.appendChild(svgEl("circle", { cx: VU_PIVOT.x, cy: VU_PIVOT.y, r: 5, fill: "#2a2016" }));
  const needle = svgEl("line", {
    x1: VU_PIVOT.x, y1: VU_PIVOT.y, x2: VU_PIVOT.x, y2: VU_PIVOT.y - 100,
    stroke: "#1a130c", "stroke-width": 2.2, "stroke-linecap": "round",
  });
  svg.appendChild(needle);
  return needle;
}
function setVuNeedle(needle, deg) {
  const tip = polarPoint(VU_PIVOT.x, VU_PIVOT.y, 100, deg);
  needle.setAttribute("x2", tip.x.toFixed(2));
  needle.setAttribute("y2", tip.y.toFixed(2));
}
const vuNeedleL = buildVuGauge($("vuGaugeL"));
const vuNeedleR = buildVuGauge($("vuGaugeR"));

// ---- telemetry from C++ (30Hz) ----
window.updateTelemetry = function (inLevel, outLevel, reelAngleDeg) {
  const inNorm = Math.max(0, Math.min(1, inLevel * 1.6));
  const outNorm = Math.max(0, Math.min(1, outLevel * 1.6));
  updateLedMeter($("ledIn"), inNorm);
  updateLedMeter($("ledOut"), outNorm);

  // -50deg pegs at "-20" (silence rest position), +24deg pegs at "+3" -
  // matches VU_TICKS above so the needle tip lands right on the printed
  // scale it's drawn against.
  const needleDeg = (lvl) => -50 + Math.max(0, Math.min(1, lvl * 1.4)) * 74;
  setVuNeedle(vuNeedleL, needleDeg(inLevel));
  setVuNeedle(vuNeedleR, needleDeg(outLevel));

  // The reel photo is a genuine straight-on orthographic shot (see the
  // #reelL/R CSS comment), so a plain whole-image rotation is correct.
  $("reelL").style.transform = `rotate(${reelAngleDeg}deg)`;
  $("reelR").style.transform = `rotate(${-reelAngleDeg}deg)`;
};

// ---- stage scale-to-fit ----
// Plain JS arithmetic, not CSS min()/calc() (see the #stage comment in
// index.html for why) - and polled, not just resize-event-driven, since
// this embedded WebView doesn't reliably fire resize events on a
// native host-driven setBounds().
let lastFitW = -1, lastFitH = -1;
function fitStage() {
  const w = document.documentElement.clientWidth || window.innerWidth || 0;
  const h = document.documentElement.clientHeight || window.innerHeight || 0;
  if (w <= 0 || h <= 0) return; // not laid out yet - leave lastFit unset so the next poll retries
  if (w === lastFitW && h === lastFitH) return;
  lastFitW = w; lastFitH = h;
  const scale = Math.min(w / 1536, h / 1024);
  currentStageScale = scale || 1;
  const stage = $("stage");
  if (stage) stage.style.transform = `scale(${scale})`;
}
fitStage();
window.addEventListener("resize", fitStage);
setInterval(fitStage, 300);

// ---- wire up ----
document.querySelectorAll(".knobWrap[data-param]").forEach(initKnob);
initChoiceGroup("tapeTypeGroup", "tapeType");
initChoiceGroup("ipsGroup", "ips");
initMixSlider();
initTapeRing();

$("presetPrev").addEventListener("click", () => { showPresetAt(presetIndex - 1); nf.loadPreset(presetIndex); refreshAll(); });
$("presetNext").addEventListener("click", () => { showPresetAt(presetIndex + 1); nf.loadPreset(presetIndex); refreshAll(); });

$("saveBtn").addEventListener("click", () => {
  const pop = $("savePopover");
  pop.style.display = pop.style.display === "flex" ? "none" : "flex";
});
$("saveConfirm").addEventListener("click", async () => {
  const name = $("saveNameInput").value.trim() || "User Preset";
  const newIndex = await nf.savePreset(name);
  await loadPresetsMeta();
  showPresetAt(newIndex);
  $("savePopover").style.display = "none";
  $("saveNameInput").value = "";
});

async function refreshAbButtons() {
  const state = await nf.abGetState();
  $("abA").classList.toggle("active", state.activeSlot === "A");
  $("abB").classList.toggle("active", state.activeSlot === "B");
}
$("abA").addEventListener("click", async () => {
  const state = await nf.abGetState();
  if (state.activeSlot !== "A") { await nf.abCaptureActive(); await nf.abToggleAndRecall(); }
  await refreshAbButtons(); await refreshAll();
});
$("abB").addEventListener("click", async () => {
  const state = await nf.abGetState();
  if (state.activeSlot !== "B") { await nf.abCaptureActive(); await nf.abToggleAndRecall(); }
  await refreshAbButtons(); await refreshAll();
});

(async function init() {
  await refreshAll();
  await loadPresetsMeta();
  showPresetAt(0);
  await refreshAbButtons();
})();
