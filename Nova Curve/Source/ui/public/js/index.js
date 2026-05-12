const MAX_BANDS = 24;
const BINS = 96;
const FREQ_MIN = 20;
const FREQ_MAX = 20000;
const UI_BASE_WIDTH = 1700;
const UI_BASE_HEIGHT = 1000;

const FILTER_TYPES = ["Bell", "Low Shelf", "High Shelf", "High Pass", "Low Pass", "Notch", "Band Pass", "Tilt"];
const BAND_MODES = ["Static", "Dynamic", "Resonance"];
const CHANNEL_MODES = ["Stereo", "Mid", "Side", "Left", "Right"];
const USER_PRESETS_STORAGE_KEY = "novaCurve.userPresets.v1";
const PRESET_CATEGORIES = ["All Presets", "Vocal", "Mastering", "Mix Bus", "Drums", "Guitar", "Bass", "Keys", "Podcast", "Custom"];
const PRESET_CATEGORY_ICONS = {
  "All Presets": "AP",
  "Vocal": "VC",
  "Mastering": "MS",
  "Mix Bus": "MB",
  "Drums": "DR",
  "Guitar": "GT",
  "Bass": "BS",
  "Keys": "KY",
  "Podcast": "PC",
  "Custom": "CU"
};

const PRESETS = [
  { name: "Default*", ops: [] },
  { name: "Vocal Cleanup", ops: [{ i: 2, g: -3.2, f: 310 }, { i: 3, g: -2.4, f: 2800, m: 1 }, { i: 5, g: 3.8, f: 13000, t: 2 }] },
  { name: "Rap Vocal Surgical", ops: [{ i: 0, t: 3, f: 90 }, { i: 2, g: -5.8, f: 380, q: 2.2 }, { i: 3, g: -3.0, f: 4300, m: 2 }] },
  { name: "Pop Vocal Polish", ops: [{ i: 1, g: 2.0, f: 140 }, { i: 3, g: 2.8, f: 2500 }, { i: 5, g: 4.5, f: 12000, t: 2 }] },
  { name: "Mix Bus Sweetener", ops: [{ i: 1, g: 1.4, f: 120 }, { i: 3, g: 1.8, f: 1800 }, { i: 5, g: 2.0, f: 14000, t: 2 }] },
  { name: "Mastering Clean", ops: [{ i: 0, t: 3, f: 24 }, { i: 2, g: -1.7, f: 280, q: 1.4 }, { i: 5, g: 1.8, f: 14000, t: 2 }] },
  { name: "Beat Cleanup", ops: [{ i: 0, t: 3, f: 28 }, { i: 2, g: -4.0, f: 260, q: 2.4 }, { i: 4, g: -2.2, f: 5600, m: 1 }] },
  { name: "Harshness Control", ops: [{ i: 3, g: -3.0, f: 3200, q: 2.0, m: 2 }, { i: 4, g: -2.0, f: 6900, q: 2.3, m: 1 }] },
  { name: "Low-End Tighten", ops: [{ i: 0, t: 3, f: 33 }, { i: 1, g: -2.8, f: 130, q: 1.6, m: 1 }] },
  { name: "Air & Shine", ops: [{ i: 5, t: 2, g: 6.2, f: 14500 }, { i: 4, g: -1.3, f: 5200, m: 1 }] },
  { name: "Podcast / Voice", ops: [{ i: 0, t: 3, f: 75 }, { i: 2, g: -3.4, f: 250 }, { i: 3, g: 2.6, f: 2600 }, { i: 5, g: 2.8, f: 11000, t: 2 }] }
];

// Ultra-flat diagnostic mode: set to true to strip all visual effects during interaction
// This was used to isolate lag root cause (confirmed: analyzer thread contention, not visuals).
// Disabled now that root cause is fixed.
const ULTRA_FLAT_MODE = false;

const defaultBand = (i) => ({
  enabled: i < 6 ? 1 : 0,
  type: i === 0 ? 3 : i === 5 ? 2 : 0,
  mode: i === 3 || i === 4 ? 1 : 0,
  channel: 0,
  frequency: 20 * Math.pow(1000, i / (MAX_BANDS - 1)),
  gainDb: 0,
  q: 1.2,
  slope: 24,
  dynRangeDb: -6,
  thresholdMode: 1,
  thresholdDb: -22,
  attackMs: 10,
  releaseMs: 120,
  ratio: 2.2,
  solo: 0
});

const state = {
  selectedBand: 3,
  phaseMode: 1,
  qualityMode: 1,
  analyzerMode: 0,
  harmonicLink: 0,
  signalMotion: 0,
  outputGainDb: 0,
  bypassed: 0,
  resonanceAmount: 42,
  bands: Array.from({ length: MAX_BANDS }, (_, i) => defaultBand(i))
};

state.bands[0].frequency = 34;
state.bands[1].frequency = 110; state.bands[1].gainDb = 3.2;
state.bands[2].frequency = 360; state.bands[2].gainDb = -10.8; state.bands[2].type = 5;
state.bands[3].frequency = 2730; state.bands[3].gainDb = 4.8;
state.bands[4].frequency = 5000;
state.bands[5].frequency = 13000; state.bands[5].type = 2; state.bands[5].gainDb = 5.6;

let undoStack = [];
let redoStack = [];
let snapshotA = JSON.stringify(state);

let preSpectrum = new Float32Array(BINS);
let postSpectrum = new Float32Array(BINS);
let reductionSpectrum = new Float32Array(BINS);
let preSmoothed = new Float32Array(BINS);
let postSmoothed = new Float32Array(BINS);
let reductionSmoothed = new Float32Array(BINS);
let analyzerTrailA = new Float32Array(BINS);
let analyzerTrailB = new Float32Array(BINS);
let analyzerTrailC = new Float32Array(BINS);
let outputPeak = 0;
let dynActivity = 0;
let harmonicLinkVisual = 0;
let signalMotionVisual = 0;
let lastAnalyzerUpdateMs = 0;
let noisePhase = 0;
let dragPreviewActive = false;
let dragPreviewFreq = 0;
let dragPreviewGain = 0;
let bandDynamicGainDb = new Array(MAX_BANDS).fill(0); // For compression visualization

let pushTimer = 0;
let rafHandle = 0;
let draggingBand = -1;
let hoveredBand = -1;
let hoverGraphX = 0;
let hoverGraphY = 0;
let cachedCanvasRect = null;
let knobDragging = false;
let graphDragUndoPending = false;
let interactionActiveState = false;
let interactionDeactivateTimer = 0;
let activeKnobDrag = null;
let interactionUltraFast = false;

let lastFrameMs = performance.now();
let interactionEnergy = 0;
let calloutVisible = false;
let calloutTargetX = 0;
let calloutTargetY = 0;
let calloutX = 0;
let calloutY = 0;
let calloutBandIndex = -1;
let calloutHovering = false;
let calloutHideTimer = 0;
let calloutSoloPersistent = false;
let calloutContextMode = false;
let displayBands = [];
let userPresets = [];
let presetBrowserOpen = false;
let savePresetOpen = false;
let presetActiveCategory = "All Presets";
let presetSearchText = "";
let presetTypeFilterValue = "all";
let favoritesOnly = false;
let activePreviewPresetName = "";
let savePresetCategory = "Custom";
let savePresetVisibility = "User";
let savePresetFavorite = false;

let nativeGetState = async () => "";
let nativeSetState = async () => true;
let nativeSetInteractionActive = async () => true;

const graphWrap = document.getElementById("graphWrap");
const canvas = document.getElementById("graphCanvas");
const ctx = canvas.getContext("2d");
const callout = document.getElementById("callout");
const orb = document.getElementById("resonanceOrb");
const pluginRoot = document.querySelector(".plugin");
const coEdit = document.getElementById("coEdit");
const coModeSelect = document.getElementById("coModeSelect");
const coTypeSelect = document.getElementById("coTypeSelect");
const presetOverlay = document.getElementById("presetSystemOverlay");
const presetBrowserPanel = document.getElementById("presetBrowserPanel");
const presetBrowserBtn = document.getElementById("presetBrowserBtn");
const presetBrowserCurrent = document.getElementById("presetBrowserCurrent");
const presetBrowserClose = document.getElementById("presetBrowserClose");
const presetBrowserCloseBottom = document.getElementById("presetBrowserCloseBottom");
const presetSearchInput = document.getElementById("presetSearchInput");
const presetTypeFilter = document.getElementById("presetTypeFilter");
const presetFavoriteFilter = document.getElementById("presetFavoriteFilter");
const presetCategoryColumn = document.getElementById("presetCategoryColumn");
const presetList = document.getElementById("presetList");
const presetPreviewCanvas = document.getElementById("presetPreviewCanvas");
const presetPreviewName = document.getElementById("presetPreviewName");
const presetPreviewCategory = document.getElementById("presetPreviewCategory");
const presetPreviewMode = document.getElementById("presetPreviewMode");
const presetPreviewBands = document.getElementById("presetPreviewBands");
const presetPreviewRating = document.getElementById("presetPreviewRating");
const presetNewFolderBtn = document.getElementById("presetNewFolderBtn");
const presetImportBtn = document.getElementById("presetImportBtn");
const savePresetModal = document.getElementById("savePresetModal");
const savePresetClose = document.getElementById("savePresetClose");
const savePresetNameInput = document.getElementById("savePresetNameInput");
const savePresetNameCount = document.getElementById("savePresetNameCount");
const savePresetTypeSelect = document.getElementById("savePresetTypeSelect");
const saveCategoryGrid = document.getElementById("saveCategoryGrid");
const savePresetDescription = document.getElementById("savePresetDescription");
const savePresetDescCount = document.getElementById("savePresetDescCount");
const saveVisibility = document.getElementById("saveVisibility");
const saveFavoriteToggle = document.getElementById("saveFavoriteToggle");
const savePresetCancel = document.getElementById("savePresetCancel");
const savePresetConfirm = document.getElementById("savePresetConfirm");

const bandIndexSelect = document.getElementById("bandIndex");
const bandModeSelect = document.getElementById("bandMode");
const bandTypeSelect = document.getElementById("bandType");
const bandChannelSelect = document.getElementById("bandChannel");
const bandPrevBtn = document.getElementById("bandPrevBtn");
const bandNextBtn = document.getElementById("bandNextBtn");
const bandDisplay = document.getElementById("bandDisplay");
const bandPowerBtn = document.getElementById("bandPowerBtn");
const bandPanel = document.getElementById("bandPanel");

const phaseModeSelect = document.getElementById("phaseMode");
const presetSelect = document.getElementById("presetSelect");
const savePresetBtn = document.getElementById("savePresetBtn");
const bandButtons = document.getElementById("bandButtons");

const bypassBtn = document.getElementById("bypassBtn");
const soloBtn = document.getElementById("soloBtn");
const undoBtn = document.getElementById("undoBtn");
const redoBtn = document.getElementById("redoBtn");
const abBtn = document.getElementById("abBtn");
const meterSourceBtn = document.getElementById("meterSourceBtn");
const dynMeterFill = document.getElementById("dynMeterFill");
const scBtn = document.getElementById("scBtn");
const intBtn = document.getElementById("intBtn");
const linkBtn = document.getElementById("linkBtn");
const scArrowBtn = document.getElementById("scArrowBtn");
const scMiniWave = document.getElementById("scMiniWave");

const knobControllers = {};
let sidechainExternal = false;
let meterSourceExternal = true;
let knobVisualSeed = 0;

function applySignalMotionState() {
  const enabled = state.signalMotion > 0.5;
  if (scMiniWave) {
    scMiniWave.dataset.motion = enabled ? "1" : "0";
  }
  if (scArrowBtn) {
    scArrowBtn.title = enabled ? "Signal Motion: On" : "Signal Motion: Off";
    scArrowBtn.setAttribute("aria-pressed", enabled ? "true" : "false");
    scArrowBtn.classList.toggle("active", enabled);
  }
}

function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }

function applyUiScale() {
  const margin = 12;
  const sx = (window.innerWidth - margin * 2) / UI_BASE_WIDTH;
  const sy = (window.innerHeight - margin * 2) / UI_BASE_HEIGHT;
  const scale = Math.max(0.42, Math.min(1, sx, sy));
  document.documentElement.style.setProperty("--ui-scale", scale.toFixed(4));
}

function setInteractionActive(active) {
  const shouldBeActive = active === true;
  if (shouldBeActive) {
    clearTimeout(interactionDeactivateTimer);
    if (interactionActiveState) return;
    interactionActiveState = true;
    // Keep pointerdown hot path light: bridge activation is deferred one tick.
    setTimeout(() => {
      if (!interactionActiveState) return;
      try {
        nativeSetInteractionActive(true);
      } catch (_) {}
    }, 0);
    if (pluginRoot) pluginRoot.classList.add("interaction-fast");
    return;
  }

  clearTimeout(interactionDeactivateTimer);
  interactionDeactivateTimer = setTimeout(() => {
    if (draggingBand >= 0 || knobDragging) return;
    if (!interactionActiveState) return;
    interactionActiveState = false;
    try {
      nativeSetInteractionActive(false);
    } catch (_) {}
    if (pluginRoot) pluginRoot.classList.remove("interaction-fast");
  }, 120);
}

function freqToNorm(hz) {
  return clamp(Math.log(hz / FREQ_MIN) / Math.log(FREQ_MAX / FREQ_MIN), 0, 1);
}

function normToFreq(norm) {
  return FREQ_MIN * Math.pow(FREQ_MAX / FREQ_MIN, clamp(norm, 0, 1));
}

function hzToX(hz, width) { return freqToNorm(hz) * width; }
function xToHz(x, width) { return normToFreq(x / Math.max(1, width)); }

function gainToY(gain, height) { return ((24 - clamp(gain, -24, 24)) / 48) * height; }
function yToGain(y, height) { return clamp(24 - (y / Math.max(1, height)) * 48, -30, 30); }

function bandResponseDbAtFreq(freq, b) {
  const center = Math.max(FREQ_MIN, b.frequency || 1000);
  const q = Math.max(0.1, b.q || 1.2);
  const gain = b.gainDb || 0;
  const oct = Math.log2(Math.max(FREQ_MIN, freq) / center);
  const widthOct = 0.92 / q;
  const gaussian = Math.exp(-0.5 * Math.pow(oct / Math.max(0.03, widthOct), 2));

  switch (Math.round(b.type || 0)) {
    case 1: { // Low Shelf
      const slope = 1 / (1 + Math.exp(oct * (3.2 * q)));
      return gain * slope;
    }
    case 2: { // High Shelf
      const slope = 1 / (1 + Math.exp(-oct * (3.2 * q)));
      return gain * slope;
    }
    case 3: { // High Pass
      const attn = 1 / (1 + Math.exp(-oct * (4.4 * q)));
      return -24 * (1 - attn);
    }
    case 4: { // Low Pass
      const attn = 1 / (1 + Math.exp(oct * (4.4 * q)));
      return -24 * (1 - attn);
    }
    case 5: { // Notch
      const notchDepth = -Math.max(6, Math.abs(gain) || 12);
      return notchDepth * gaussian;
    }
    case 6: { // Band Pass
      return Math.max(4, Math.abs(gain) || 6) * gaussian;
    }
    case 7: { // Tilt
      const tilt = clamp(oct / 2.5, -1, 1);
      return gain * tilt;
    }
    case 0:
    default:
      return gain * gaussian;
  }
}

function drawEqResponsePath(ctx, bands, width, height, pointCount = 260) {
  if (!bands.length) return;
  ctx.beginPath();
  for (let i = 0; i < pointCount; i++) {
    const x = (i / (pointCount - 1)) * width;
    const freq = xToHz(x, width);
    let responseDb = 0;
    for (let j = 0; j < bands.length; j++) {
      responseDb += bandResponseDbAtFreq(freq, bands[j]);
    }
    const y = gainToY(clamp(responseDb, -24, 24), height);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
}

function fmtHz(hz) { return hz >= 1000 ? `${(hz / 1000).toFixed(2)} kHz` : `${Math.round(hz)} Hz`; }
function fmtDb(db) { return `${db >= 0 ? "+" : ""}${db.toFixed(1)} dB`; }
function fmtMs(ms) { return ms < 100 ? `${ms.toFixed(1)} ms` : `${Math.round(ms)} ms`; }

function inferPresetCategory(name) {
  const n = String(name || "").toLowerCase();
  if (n.includes("vocal") || n.includes("voice") || n.includes("rap") || n.includes("podcast")) {
    return n.includes("podcast") ? "Podcast" : "Vocal";
  }
  if (n.includes("master") || n.includes("mastering") || n.includes("clean")) return "Mastering";
  if (n.includes("mix bus") || n.includes("mix") || n.includes("sweetener")) return "Mix Bus";
  if (n.includes("drum") || n.includes("beat")) return "Drums";
  if (n.includes("guitar")) return "Guitar";
  if (n.includes("bass") || n.includes("low-end")) return "Bass";
  if (n.includes("keys") || n.includes("piano") || n.includes("synth")) return "Keys";
  return "Custom";
}

function getPresetModeLabel(ops = []) {
  let hasDynamic = false;
  let hasResonance = false;
  for (let i = 0; i < ops.length; i++) {
    const mode = Number(ops[i].m || 0);
    if (mode === 1) hasDynamic = true;
    if (mode === 2) hasResonance = true;
  }
  if (hasResonance) return "Resonance";
  if (hasDynamic) return "Dynamic";
  return "Static";
}

function getPresetEntries() {
  const factory = PRESETS.map((preset) => ({
    ...preset,
    category: inferPresetCategory(preset.name),
    type: inferPresetCategory(preset.name),
    favorite: false,
    visibility: "Global",
    description: "",
    isUser: false
  }));

  const users = userPresets.map((preset) => ({
    ...preset,
    category: PRESET_CATEGORIES.includes(preset.category) ? preset.category : inferPresetCategory(preset.name),
    type: PRESET_CATEGORIES.includes(preset.type) ? preset.type : (PRESET_CATEGORIES.includes(preset.category) ? preset.category : inferPresetCategory(preset.name)),
    favorite: preset.favorite === true,
    visibility: preset.visibility === "Global" ? "Global" : "User",
    description: typeof preset.description === "string" ? preset.description : "",
    isUser: true
  }));

  return factory.concat(users);
}

function getFilteredPresetEntries() {
  const presets = getPresetEntries();
  return presets.filter((preset) => {
    if (presetActiveCategory !== "All Presets" && preset.category !== presetActiveCategory) return false;
    if (presetTypeFilterValue !== "all" && preset.type !== presetTypeFilterValue) return false;
    if (favoritesOnly && !preset.favorite) return false;
    if (presetSearchText.length > 0) {
      const hay = `${preset.name} ${preset.category} ${preset.type} ${preset.description}`.toLowerCase();
      if (!hay.includes(presetSearchText)) return false;
    }
    return true;
  });
}

function updatePresetBrowserCurrentLabel(name) {
  if (presetBrowserCurrent) {
    presetBrowserCurrent.textContent = name || "Default*";
  }
}

function getAllPresets() {
  return getPresetEntries();
}

function rebuildPresetSelectOptions() {
  if (!presetSelect) return;
  const currentValue = presetSelect.value;
  presetSelect.innerHTML = "";
  getAllPresets().forEach((preset) => {
    const option = document.createElement("option");
    option.value = preset.name;
    option.textContent = preset.name;
    presetSelect.appendChild(option);
  });

  if (!presetSelect.options.length) return;
  const hasCurrent = Array.from(presetSelect.options).some((o) => o.value === currentValue);
  presetSelect.value = hasCurrent ? currentValue : presetSelect.options[0].value;
  updatePresetBrowserCurrentLabel(presetSelect.value);
}

function loadUserPresets() {
  try {
    if (typeof localStorage === "undefined") return;
    const raw = localStorage.getItem(USER_PRESETS_STORAGE_KEY);
    if (!raw) return;
    const parsed = JSON.parse(raw);
    if (!Array.isArray(parsed)) return;
    userPresets = parsed
      .filter((p) => p && typeof p.name === "string" && Array.isArray(p.ops))
      .map((p) => ({
        name: p.name.trim(),
        ops: p.ops,
        category: PRESET_CATEGORIES.includes(p.category) ? p.category : inferPresetCategory(p.name),
        type: PRESET_CATEGORIES.includes(p.type) ? p.type : (PRESET_CATEGORIES.includes(p.category) ? p.category : inferPresetCategory(p.name)),
        description: typeof p.description === "string" ? p.description.trim() : "",
        visibility: p.visibility === "Global" ? "Global" : "User",
        favorite: p.favorite === true
      }))
      .filter((p) => p.name.length > 0);
  } catch (_) {
    userPresets = [];
  }
}

function persistUserPresets() {
  try {
    if (typeof localStorage === "undefined") return;
    localStorage.setItem(USER_PRESETS_STORAGE_KEY, JSON.stringify(userPresets));
  } catch (_) {}
}

function captureCurrentPresetOps() {
  return state.bands
    .map((b, i) => ({ b, i }))
    .filter(({ b }) => b.enabled > 0.5)
    .map(({ b, i }) => ({
      i,
      t: Math.round(b.type || 0),
      m: Math.round(b.mode || 0),
      c: Math.round(b.channel || 0),
      f: Number((b.frequency || 1000).toFixed(2)),
      g: Number((b.gainDb || 0).toFixed(2)),
      q: Number((b.q || 1.2).toFixed(3))
    }));
}

function drawPresetPreview(preset) {
  if (!presetPreviewCanvas || !preset) return;
  const pctx = presetPreviewCanvas.getContext("2d");
  if (!pctx) return;
  const w = presetPreviewCanvas.width;
  const h = presetPreviewCanvas.height;
  pctx.clearRect(0, 0, w, h);
  pctx.fillStyle = "rgba(8, 14, 28, 0.92)";
  pctx.fillRect(0, 0, w, h);

  pctx.strokeStyle = "rgba(96, 118, 196, 0.2)";
  pctx.lineWidth = 1;
  for (let i = 0; i < 6; i++) {
    const y = (i / 5) * h;
    pctx.beginPath();
    pctx.moveTo(0, y);
    pctx.lineTo(w, y);
    pctx.stroke();
  }

  const grad = pctx.createLinearGradient(0, 0, w, 0);
  grad.addColorStop(0, "#e8efff");
  grad.addColorStop(0.4, "#bf73ff");
  grad.addColorStop(1, "#86c4ff");

  pctx.beginPath();
  const ops = Array.isArray(preset.ops) ? preset.ops : [];
  const points = 64;
  for (let i = 0; i < points; i++) {
    const x = (i / (points - 1)) * w;
    const freq = xToHz(x, w);
    let response = 0;
    for (let j = 0; j < ops.length; j++) {
      const op = ops[j];
      response += bandResponseDbAtFreq(freq, {
        frequency: op.f || 1000,
        q: op.q || 1.2,
        gainDb: op.g || 0,
        type: op.t || 0
      });
    }
    const y = gainToY(clamp(response, -24, 24), h);
    if (i === 0) pctx.moveTo(x, y);
    else pctx.lineTo(x, y);
  }
  pctx.lineWidth = 2.4;
  pctx.strokeStyle = grad;
  pctx.stroke();

  pctx.shadowColor = "rgba(170, 122, 255, 0.26)";
  pctx.shadowBlur = 12;
  pctx.stroke();
  pctx.shadowBlur = 0;
}

function setPresetPreview(preset) {
  if (!preset) return;
  if (presetPreviewName) presetPreviewName.textContent = preset.name;
  if (presetPreviewCategory) presetPreviewCategory.textContent = `${preset.category} / ${preset.type}`;
  if (presetPreviewMode) presetPreviewMode.textContent = getPresetModeLabel(preset.ops || []);
  if (presetPreviewBands) presetPreviewBands.textContent = `${(preset.ops || []).length} Bands`;
  if (presetPreviewRating) {
    const stars = preset.favorite ? "1 Star" : "No Stars";
    presetPreviewRating.textContent = stars;
  }
  activePreviewPresetName = preset.name;
  drawPresetPreview(preset);
}

function renderPresetCategoryColumn() {
  if (!presetCategoryColumn) return;
  presetCategoryColumn.innerHTML = "";
  PRESET_CATEGORIES.forEach((category) => {
    const btn = document.createElement("button");
    btn.className = `preset-cat-btn${category === presetActiveCategory ? " active" : ""}`;
    btn.innerHTML = `<span>${PRESET_CATEGORY_ICONS[category] || ".."}</span><span>${category}</span>`;
    btn.onclick = () => {
      presetActiveCategory = category;
      renderPresetCategoryColumn();
      renderPresetList();
    };
    presetCategoryColumn.appendChild(btn);
  });
}

function togglePresetFavoriteByName(name) {
  const idx = userPresets.findIndex((p) => p.name === name);
  if (idx < 0) return;
  userPresets[idx].favorite = !userPresets[idx].favorite;
  persistUserPresets();
  rebuildPresetSelectOptions();
  renderPresetList();
}

function applyPresetByName(name) {
  const preset = getAllPresets().find((p) => p.name === name);
  if (!preset) return;
  snapshotForUndo();
  (preset.ops || []).forEach((op) => {
    const b = state.bands[op.i];
    if (!b) return;
    b.enabled = 1;
    if (op.t !== undefined) b.type = op.t;
    if (op.m !== undefined) b.mode = op.m;
    if (op.c !== undefined) b.channel = op.c;
    if (op.f !== undefined) b.frequency = op.f;
    if (op.g !== undefined) b.gainDb = op.g;
    if (op.q !== undefined) b.q = op.q;
  });
  presetSelect.value = preset.name;
  updatePresetBrowserCurrentLabel(preset.name);
  syncControlsFromState();
  queuePushState();
  setPresetPreview(preset);
  if (presetBrowserOpen) closePresetBrowser();
}

function renderPresetList() {
  if (!presetList) return;
  const filtered = getFilteredPresetEntries();
  presetList.innerHTML = "";

  filtered.forEach((preset, index) => {
    const item = document.createElement("div");
    const isActive = presetSelect && presetSelect.value === preset.name;
    item.className = `preset-list-item${isActive ? " active" : ""}`;
    item.innerHTML = `
      <div class="name">${preset.name}</div>
      <button class="preset-star${preset.favorite ? " active" : ""}" type="button" title="Favorite">*</button>
      <div class="meta">${preset.category} / ${preset.type}</div>
    `;

    item.onclick = (e) => {
      const star = e.target.closest(".preset-star");
      if (star) {
        e.stopPropagation();
        togglePresetFavoriteByName(preset.name);
        return;
      }
      applyPresetByName(preset.name);
      renderPresetList();
    };

    item.onmouseenter = () => setPresetPreview(preset);
    presetList.appendChild(item);
  });

  if (!filtered.length) {
    const empty = document.createElement("div");
    empty.className = "preset-list-item";
    empty.innerHTML = `<div class="name">No Presets Found</div><div class="meta">Adjust filters or search</div>`;
    presetList.appendChild(empty);
    return;
  }

  const active = filtered.find((p) => p.name === presetSelect.value) || filtered[0];
  setPresetPreview(active);
}

function renderPresetBrowser() {
  renderPresetCategoryColumn();
  renderPresetList();
}

function updateSaveCounters() {
  if (savePresetNameCount && savePresetNameInput) savePresetNameCount.textContent = `${savePresetNameInput.value.length}/48`;
  if (savePresetDescCount && savePresetDescription) savePresetDescCount.textContent = `${savePresetDescription.value.length}/180`;
}

function renderSaveCategoryGrid() {
  if (!saveCategoryGrid) return;
  saveCategoryGrid.innerHTML = "";
  PRESET_CATEGORIES.filter((c) => c !== "All Presets").forEach((category) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = `save-category-btn${savePresetCategory === category ? " active" : ""}`;
    btn.innerHTML = `<span>${PRESET_CATEGORY_ICONS[category] || ".."}</span><span>${category}</span>`;
    btn.onclick = () => {
      savePresetCategory = category;
      if (savePresetTypeSelect) savePresetTypeSelect.value = category;
      renderSaveCategoryGrid();
    };
    saveCategoryGrid.appendChild(btn);
  });
}

function openPresetBrowser() {
  if (!presetOverlay || !presetBrowserPanel) return;
  presetBrowserOpen = true;
  savePresetOpen = false;
  presetOverlay.classList.add("visible");
  presetOverlay.setAttribute("aria-hidden", "false");
  presetBrowserPanel.classList.add("visible");
  if (savePresetModal) savePresetModal.classList.remove("visible");
  // Let overlay/panel paint first, then do heavier list rendering.
  requestAnimationFrame(() => {
    renderPresetBrowser();
    if (presetSearchInput) presetSearchInput.focus();
  });
}

function closePresetBrowser() {
  presetBrowserOpen = false;
  if (!savePresetOpen && presetOverlay) {
    presetOverlay.classList.remove("visible");
    presetOverlay.setAttribute("aria-hidden", "true");
  }
  if (presetBrowserPanel) presetBrowserPanel.classList.remove("visible");
}

function openSavePresetModal() {
  if (!presetOverlay || !savePresetModal) return;
  savePresetOpen = true;
  presetBrowserOpen = false;
  presetOverlay.classList.add("visible");
  presetOverlay.setAttribute("aria-hidden", "false");
  if (presetBrowserPanel) presetBrowserPanel.classList.remove("visible");
  savePresetModal.classList.add("visible");

  savePresetCategory = "Custom";
  savePresetVisibility = "User";
  savePresetFavorite = false;
  if (savePresetNameInput) savePresetNameInput.value = `My Preset ${userPresets.length + 1}`;
  if (savePresetDescription) savePresetDescription.value = "";
  if (savePresetTypeSelect) savePresetTypeSelect.value = savePresetCategory;
  if (saveVisibility) {
    saveVisibility.querySelectorAll("button").forEach((btn) => {
      btn.classList.toggle("active", btn.dataset.visibility === savePresetVisibility);
    });
  }
  if (saveFavoriteToggle) {
    saveFavoriteToggle.dataset.active = "0";
    saveFavoriteToggle.setAttribute("aria-pressed", "false");
  }
  renderSaveCategoryGrid();
  updateSaveCounters();
  if (savePresetNameInput) {
    savePresetNameInput.focus();
    savePresetNameInput.select();
  }
}

function closeSavePresetModal() {
  savePresetOpen = false;
  if (!presetBrowserOpen && presetOverlay) {
    presetOverlay.classList.remove("visible");
    presetOverlay.setAttribute("aria-hidden", "true");
  }
  if (savePresetModal) savePresetModal.classList.remove("visible");
}

function saveCurrentPreset(enteredName, metadata = {}) {
  const name = String(enteredName ?? "").trim();
  if (!name) return;

  const ops = captureCurrentPresetOps();
  if (!ops.length) {
    window.alert("Enable at least one band before saving a preset.");
    return;
  }

  const existingBuiltIn = PRESETS.findIndex((p) => p.name.toLowerCase() === name.toLowerCase());
  if (existingBuiltIn >= 0) {
    window.alert("That name is used by a factory preset. Please choose a different name.");
    return;
  }

  const existingUser = userPresets.findIndex((p) => p.name.toLowerCase() === name.toLowerCase());
  const nextPreset = {
    name,
    ops,
    category: PRESET_CATEGORIES.includes(metadata.category) ? metadata.category : inferPresetCategory(name),
    type: PRESET_CATEGORIES.includes(metadata.type) ? metadata.type : (PRESET_CATEGORIES.includes(metadata.category) ? metadata.category : inferPresetCategory(name)),
    description: typeof metadata.description === "string" ? metadata.description.trim() : "",
    visibility: metadata.visibility === "Global" ? "Global" : "User",
    favorite: metadata.favorite === true
  };

  if (existingUser >= 0) {
    const ok = window.confirm(`Overwrite preset \"${name}\"?`);
    if (!ok) return;
    userPresets[existingUser] = nextPreset;
  } else {
    userPresets.push(nextPreset);
  }

  persistUserPresets();
  rebuildPresetSelectOptions();
  presetSelect.value = name;
  updatePresetBrowserCurrentLabel(name);
  renderPresetBrowser();
  closeSavePresetModal();
}

function selectedBand() { return state.bands[state.selectedBand] || state.bands[0]; }

function createDisplayBandFromState(b) {
  return {
    frequency: b.frequency,
    gainDb: b.gainDb,
    q: b.q,
    dynRangeDb: b.dynRangeDb,
    enabled: b.enabled,
    mode: b.mode,
    type: b.type,
    channel: b.channel
  };
}

function ensureDisplayBands() {
  if (displayBands.length === state.bands.length) return;
  displayBands = state.bands.map((b) => createDisplayBandFromState(b));
}

function smoothTo(current, target, speed, dtMs) {
  const a = 1 - Math.exp(-(Math.max(0.001, speed) * dtMs) / 1000);
  return current + (target - current) * a;
}

function cancelCalloutHide() {
  if (!calloutHideTimer) return;
  clearTimeout(calloutHideTimer);
  calloutHideTimer = 0;
}

function scheduleCalloutHide(delayMs = 400) {
  if (calloutContextMode) return;
  cancelCalloutHide();
  calloutHideTimer = setTimeout(() => {
    if (draggingBand < 0 && !calloutHovering && hoveredBand < 0) {
      calloutVisible = false;
      calloutBandIndex = -1;
    }
    calloutHideTimer = 0;
  }, delayMs);
}

function updateSelectedBandReadouts(readoutKey = "all") {
  const b = selectedBand();
  if (readoutKey === "all" || readoutKey === "freq") document.getElementById("freqRead").textContent = fmtHz(b.frequency);
  if (readoutKey === "all" || readoutKey === "gain") document.getElementById("gainRead").textContent = fmtDb(b.gainDb);
  if (readoutKey === "all" || readoutKey === "q") document.getElementById("qRead").textContent = b.q.toFixed(2);
  if (readoutKey === "all" || readoutKey === "range") document.getElementById("rangeRead").textContent = fmtDb(b.dynRangeDb);
  if (readoutKey === "all" || readoutKey === "threshold") document.getElementById("thresholdRead").textContent = fmtDb(b.thresholdDb);
  if (readoutKey === "all" || readoutKey === "attack") document.getElementById("attackRead").textContent = fmtMs(b.attackMs);
  if (readoutKey === "all" || readoutKey === "release") document.getElementById("releaseRead").textContent = fmtMs(b.releaseMs);
  if (readoutKey === "all" || readoutKey === "ratio") document.getElementById("ratioRead").textContent = `${b.ratio.toFixed(1)}:1`;
  if (readoutKey === "all" || readoutKey === "resonance") document.getElementById("resRead").textContent = `${Math.round(state.resonanceAmount)}%`;
  if (readoutKey === "all" || readoutKey === "output") document.getElementById("outputRead").textContent = fmtDb(state.outputGainDb);

  const shouldRefreshDyn = readoutKey === "all" || readoutKey === "range" || readoutKey === "threshold" || readoutKey === "attack" || readoutKey === "release" || readoutKey === "ratio";
  if (!shouldRefreshDyn) return;
  const dynActive = Math.round(b.mode) === 1;
  const dynGrid = document.querySelector(".dyn-grid");
  if (dynGrid) {
    dynGrid.style.opacity = dynActive ? "1" : "0.52";
  }
  const dynIndicator = document.getElementById("dynActiveIndicator");
  if (dynIndicator) {
    dynIndicator.classList.toggle("active", dynActive);
  }
  ["knobRange", "knobThreshold", "knobAttack", "knobRelease", "knobRatio"].forEach((id) => {
    const k = document.getElementById(id);
    if (!k) return;
    k.style.pointerEvents = dynActive ? "auto" : "none";
    k.style.filter = dynActive ? "none" : "saturate(0.65)";
  });
}

function snapshotForUndo() {
  undoStack.push(JSON.stringify(state));
  if (undoStack.length > 120) undoStack.shift();
  redoStack = [];
}

function queuePushState() {
  clearTimeout(pushTimer);
  pushTimer = setTimeout(async () => {
    if (draggingBand >= 0 || knobDragging || interactionActiveState) {
      queuePushState();
      return;
    }
    try { await nativeSetState(JSON.stringify(state)); } catch (_) {}
  }, 65);
}

async function setupNativeBridge() {
  if (typeof window.__JUCE__ === "undefined")
    return;

  try {
    const juce = await import("./juce/index.js");
    nativeGetState = juce.getNativeFunction("getInitialState");
    nativeSetState = juce.getNativeFunction("setUiState");
    nativeSetInteractionActive = juce.getNativeFunction("setInteractionActive");
  } catch (error) {
    console.warn("Native bridge unavailable, staying in preview mode", error);
  }
}

function buildKnob(el, options) {
  const visualId = ++knobVisualSeed;
  const gradientId = `arcGradient-${visualId}`;
  const glowId = `arcGlow-${visualId}`;
  const bloomId = `arcBloom-${visualId}`;
  const coreGradientId = `arcCoreGradient-${visualId}`;
  const headGradientId = `arcHeadGradient-${visualId}`;
  const r = 46;
  const fullArcLength = 2 * Math.PI * r;
  const trackSweep = 0.78;
  const trackLength = fullArcLength * trackSweep;

  const ring = document.createElement("div");
  ring.className = "knob-ring";
  const ambientRing = document.createElement("div");
  ambientRing.className = "knob-ambient-ring";
  const outerRing = document.createElement("div");
  outerRing.className = "knob-outer-ring";
  const indicator = document.createElement("div");
  indicator.className = "knob-indicator";
  const imperfection = ((visualId % 9) - 4) * 0.55;
  const sweepBias = 0.88 + (((visualId * 37) % 17) / 100);
  el.style.setProperty("--knob-imperfection", `${imperfection}deg`);
  el.style.setProperty("--knob-sweep", `${sweepBias}`);
  
  // Create arc container with SVG
  const arcContainer = document.createElement("div");
  arcContainer.className = "knob-arc-container";
  const arcDiv = document.createElement("div");
  arcDiv.className = "knob-arc";
  
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", "0 0 100 100");
  svg.setAttribute("style", "pointer-events: none;");
  
  // Outer subtle arc (background reference track)
  const bgArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  bgArc.setAttribute("cx", "50");
  bgArc.setAttribute("cy", "50");
  bgArc.setAttribute("r", `${r}`);
  bgArc.setAttribute("fill", "none");
  bgArc.setAttribute("stroke", "rgba(98, 118, 222, 0.16)");
  bgArc.setAttribute("stroke-width", "1.2");
  bgArc.setAttribute("stroke-dasharray", `${trackLength} ${fullArcLength}`);
  bgArc.setAttribute("stroke-dashoffset", "0");
  bgArc.setAttribute("stroke-linecap", "round");
  bgArc.setAttribute("transform", "rotate(-135 50 50)");
  svg.appendChild(bgArc);

  const bgArcSoft = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  bgArcSoft.setAttribute("cx", "50");
  bgArcSoft.setAttribute("cy", "50");
  bgArcSoft.setAttribute("r", `${r}`);
  bgArcSoft.setAttribute("fill", "none");
  bgArcSoft.setAttribute("stroke", "rgba(132, 112, 246, 0.07)");
  bgArcSoft.setAttribute("stroke-width", "3.6");
  bgArcSoft.setAttribute("stroke-dasharray", `${trackLength} ${fullArcLength}`);
  bgArcSoft.setAttribute("stroke-dashoffset", "0");
  bgArcSoft.setAttribute("stroke-linecap", "round");
  bgArcSoft.setAttribute("transform", "rotate(-135 50 50)");
  svg.appendChild(bgArcSoft);

  // Outer bloom arc — soft wide halo behind the main arc
  const outerBloomArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  outerBloomArc.setAttribute("cx", "50");
  outerBloomArc.setAttribute("cy", "50");
  outerBloomArc.setAttribute("r", `${r}`);
  outerBloomArc.setAttribute("fill", "none");
  outerBloomArc.setAttribute("stroke", `url(#${gradientId})`);
  outerBloomArc.setAttribute("stroke-width", "5.2");
  outerBloomArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
  outerBloomArc.setAttribute("stroke-dashoffset", "0");
  outerBloomArc.setAttribute("stroke-linecap", "round");
  outerBloomArc.setAttribute("transform", "rotate(-135 50 50)");
  outerBloomArc.setAttribute("filter", `url(#${bloomId})`);
  outerBloomArc.setAttribute("opacity", "0");
  svg.appendChild(outerBloomArc);

  // Dense body arc to make the band read as illuminated material, not a thin outline
  const bandMassArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  bandMassArc.setAttribute("cx", "50");
  bandMassArc.setAttribute("cy", "50");
  bandMassArc.setAttribute("r", `${r}`);
  bandMassArc.setAttribute("fill", "none");
  bandMassArc.setAttribute("stroke", `url(#${gradientId})`);
  bandMassArc.setAttribute("stroke-width", "4.4");
  bandMassArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
  bandMassArc.setAttribute("stroke-dashoffset", "0");
  bandMassArc.setAttribute("stroke-linecap", "round");
  bandMassArc.setAttribute("transform", "rotate(-135 50 50)");
  bandMassArc.setAttribute("opacity", "0");
  svg.appendChild(bandMassArc);

  // Feather arc extends slightly beyond the body to smooth end rolloff
  const taperFeatherArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  taperFeatherArc.setAttribute("cx", "50");
  taperFeatherArc.setAttribute("cy", "50");
  taperFeatherArc.setAttribute("r", `${r}`);
  taperFeatherArc.setAttribute("fill", "none");
  taperFeatherArc.setAttribute("stroke", `url(#${gradientId})`);
  taperFeatherArc.setAttribute("stroke-width", "4.6");
  taperFeatherArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
  taperFeatherArc.setAttribute("stroke-dashoffset", "0");
  taperFeatherArc.setAttribute("stroke-linecap", "round");
  taperFeatherArc.setAttribute("transform", "rotate(-135 50 50)");
  taperFeatherArc.setAttribute("filter", `url(#${bloomId})`);
  taperFeatherArc.setAttribute("opacity", "0");
  svg.appendChild(taperFeatherArc);

  // Main parameter arc (illuminated body)
  const paramArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  paramArc.setAttribute("cx", "50");
  paramArc.setAttribute("cy", "50");
  paramArc.setAttribute("r", `${r}`);
  paramArc.setAttribute("fill", "none");
  paramArc.setAttribute("stroke", `url(#${gradientId})`);
  paramArc.setAttribute("stroke-width", "2.9");
  paramArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
  paramArc.setAttribute("stroke-dashoffset", "0");
  paramArc.setAttribute("stroke-linecap", "round");
  paramArc.setAttribute("transform", "rotate(-135 50 50)");
  paramArc.setAttribute("filter", `url(#${glowId})`);
  svg.appendChild(paramArc);

  // Inner core arc — crisp bright spine running on top of the main arc
  const innerCoreArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  innerCoreArc.setAttribute("cx", "50");
  innerCoreArc.setAttribute("cy", "50");
  innerCoreArc.setAttribute("r", `${r}`);
  innerCoreArc.setAttribute("fill", "none");
  innerCoreArc.setAttribute("stroke", `url(#${coreGradientId})`);
  innerCoreArc.setAttribute("stroke-width", "0.72");
  innerCoreArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
  innerCoreArc.setAttribute("stroke-dashoffset", "0");
  innerCoreArc.setAttribute("stroke-linecap", "round");
  innerCoreArc.setAttribute("transform", "rotate(-135 50 50)");
  innerCoreArc.setAttribute("opacity", "0");
  svg.appendChild(innerCoreArc);

  // Active head highlight for readable end-position definition
  const headArc = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  headArc.setAttribute("cx", "50");
  headArc.setAttribute("cy", "50");
  headArc.setAttribute("r", `${r}`);
  headArc.setAttribute("fill", "none");
  headArc.setAttribute("stroke", `url(#${headGradientId})`);
  headArc.setAttribute("stroke-width", "1.55");
  headArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
  headArc.setAttribute("stroke-dashoffset", "0");
  headArc.setAttribute("stroke-linecap", "round");
  headArc.setAttribute("transform", "rotate(-135 50 50)");
  headArc.setAttribute("opacity", "0");
  headArc.setAttribute("filter", `url(#${glowId})`);
  svg.appendChild(headArc);
  
  // Define gradients and filters
  const defs = document.createElementNS("http://www.w3.org/2000/svg", "defs");
  
  // Gradient for arc - active point brighter, soft trailing body
  const grad = document.createElementNS("http://www.w3.org/2000/svg", "linearGradient");
  grad.setAttribute("id", gradientId);
  grad.setAttribute("x1", "0%");
  grad.setAttribute("y1", "0%");
  grad.setAttribute("x2", "100%");
  grad.setAttribute("y2", "100%");
  const stop1 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  stop1.setAttribute("offset", "0%");
  stop1.setAttribute("stop-color", "rgba(94, 136, 255, 0.94)");
  const stop2 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  stop2.setAttribute("offset", "46%");
  stop2.setAttribute("stop-color", "rgba(162, 78, 255, 1)");
  const stopMid = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  stopMid.setAttribute("offset", "78%");
  stopMid.setAttribute("stop-color", "rgba(208, 94, 255, 0.98)");
  const stop3 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  stop3.setAttribute("offset", "100%");
  stop3.setAttribute("stop-color", "rgba(238, 214, 255, 0.96)");
  grad.appendChild(stop1);
  grad.appendChild(stop2);
  grad.appendChild(stopMid);
  grad.appendChild(stop3);
  defs.appendChild(grad);

  // Inner core gradient — near-white bright spine
  const coreGrad = document.createElementNS("http://www.w3.org/2000/svg", "linearGradient");
  coreGrad.setAttribute("id", coreGradientId);
  coreGrad.setAttribute("x1", "0%");
  coreGrad.setAttribute("y1", "0%");
  coreGrad.setAttribute("x2", "100%");
  coreGrad.setAttribute("y2", "100%");
  const coreStop1 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  coreStop1.setAttribute("offset", "0%");
  coreStop1.setAttribute("stop-color", "rgba(170, 192, 255, 0.06)");
  const coreStop2 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  coreStop2.setAttribute("offset", "62%");
  coreStop2.setAttribute("stop-color", "rgba(230, 198, 255, 0.86)");
  const coreStop3 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  coreStop3.setAttribute("offset", "100%");
  coreStop3.setAttribute("stop-color", "rgba(246, 232, 255, 0.97)");
  coreGrad.appendChild(coreStop1);
  coreGrad.appendChild(coreStop2);
  coreGrad.appendChild(coreStop3);
  defs.appendChild(coreGrad);

  const headGrad = document.createElementNS("http://www.w3.org/2000/svg", "linearGradient");
  headGrad.setAttribute("id", headGradientId);
  headGrad.setAttribute("x1", "0%");
  headGrad.setAttribute("y1", "0%");
  headGrad.setAttribute("x2", "100%");
  headGrad.setAttribute("y2", "100%");
  const headStop1 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  headStop1.setAttribute("offset", "0%");
  headStop1.setAttribute("stop-color", "rgba(168, 104, 255, 0.12)");
  const headStop2 = document.createElementNS("http://www.w3.org/2000/svg", "stop");
  headStop2.setAttribute("offset", "100%");
  headStop2.setAttribute("stop-color", "rgba(242, 224, 255, 0.94)");
  headGrad.appendChild(headStop1);
  headGrad.appendChild(headStop2);
  defs.appendChild(headGrad);
  
  // Tight focused glow filter for main arc with boosted overlap intensity
  const filter = document.createElementNS("http://www.w3.org/2000/svg", "filter");
  filter.setAttribute("id", glowId);
  filter.setAttribute("x", "-50%");
  filter.setAttribute("y", "-50%");
  filter.setAttribute("width", "200%");
  filter.setAttribute("height", "200%");
  const feGaussianBlur = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
  feGaussianBlur.setAttribute("stdDeviation", "0.96");
  feGaussianBlur.setAttribute("result", "coloredBlur");
  filter.appendChild(feGaussianBlur);
  const feGaussianBlur2 = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
  feGaussianBlur2.setAttribute("in", "SourceGraphic");
  feGaussianBlur2.setAttribute("stdDeviation", "0.08");
  feGaussianBlur2.setAttribute("result", "softBlur");
  filter.appendChild(feGaussianBlur2);
  const feMerge = document.createElementNS("http://www.w3.org/2000/svg", "feMerge");
  const feMergeNode1 = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
  feMergeNode1.setAttribute("in", "coloredBlur");
  const feMergeNode2 = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
  feMergeNode2.setAttribute("in", "softBlur");
  const feMergeNode3 = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
  feMergeNode3.setAttribute("in", "SourceGraphic");
  feMerge.appendChild(feMergeNode1);
  feMerge.appendChild(feMergeNode2);
  feMerge.appendChild(feMergeNode3);
  filter.appendChild(feMerge);
  defs.appendChild(filter);

  // Soft wide bloom filter for outer bloom arc
  const bloomFilter = document.createElementNS("http://www.w3.org/2000/svg", "filter");
  bloomFilter.setAttribute("id", bloomId);
  bloomFilter.setAttribute("x", "-60%");
  bloomFilter.setAttribute("y", "-60%");
  bloomFilter.setAttribute("width", "220%");
  bloomFilter.setAttribute("height", "220%");
  const bloomBlur = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
  bloomBlur.setAttribute("stdDeviation", "1.55");
  bloomBlur.setAttribute("result", "bloom");
  bloomFilter.appendChild(bloomBlur);
  const bloomMerge = document.createElementNS("http://www.w3.org/2000/svg", "feMerge");
  const bloomNode1 = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
  bloomNode1.setAttribute("in", "bloom");
  bloomMerge.appendChild(bloomNode1);
  bloomFilter.appendChild(bloomMerge);
  defs.appendChild(bloomFilter);
  
  svg.insertBefore(defs, svg.firstChild);
  arcDiv.appendChild(svg);
  arcContainer.appendChild(arcDiv);
  
  el.appendChild(arcContainer);
  el.appendChild(outerRing);
  el.appendChild(ambientRing);
  el.appendChild(ring);
  el.appendChild(indicator);

  const ctrl = {
    value: options.get(),
    arc: paramArc,
    massArc: bandMassArc,
    taperArc: taperFeatherArc,
    headArc: headArc,
    coreArc: innerCoreArc,
    bloomArc: outerBloomArc,
    bgArc: bgArc,
    set(v, push) {
      const clamped = clamp(v, options.min, options.max);
      if (push !== false) {
        options.set(clamped, true);
      }
      this.value = options.get();
      const norm = clamp(options.toNorm ? options.toNorm(this.value) : (this.value - options.min) / (options.max - options.min), 0, 1);
      const deg = -135 + norm * 270;
      indicator.style.transform = `translateX(-50%) rotate(${deg}deg)`;
      
      // Update illuminated value arc over a fixed 270-degree track
      const progressLength = Math.max(0, norm * trackLength);
      this.arc.setAttribute("stroke-dasharray", `${progressLength} ${fullArcLength}`);
      this.bloomArc.setAttribute("stroke-dasharray", `${progressLength} ${fullArcLength}`);
      this.massArc.setAttribute("stroke-dasharray", `${progressLength} ${fullArcLength}`);
      this.coreArc.setAttribute("stroke-dasharray", `${progressLength} ${fullArcLength}`);
      const taperCurve = Math.pow(norm, 0.88);
      const featherLength = Math.min(trackLength, Math.max(0, progressLength + 8.0));
      this.taperArc.setAttribute("stroke-dasharray", `${featherLength} ${fullArcLength}`);
      this.taperArc.setAttribute("stroke-dashoffset", `${-4.3}`);
      const interactionBoost = 0.72 + interactionEnergy * 0.62;
      this.bloomArc.setAttribute("opacity", progressLength > 0.8 ? `${(0.22 + 0.31 * Math.pow(norm, 1.18)) * interactionBoost}` : "0");
      this.massArc.setAttribute("opacity", progressLength > 0.8 ? `${Math.min(1, (0.86 + 0.24 * Math.pow(norm, 0.92)) * interactionBoost)}` : "0");
      this.coreArc.setAttribute("opacity", progressLength > 0.8 ? `${0.34 + 0.2 * taperCurve}` : "0");
      this.taperArc.setAttribute("opacity", progressLength > 0.8 ? `${0.28 + 0.2 * Math.pow(norm, 1.0)}` : "0");

      // Active point head segment with subtle taper-like behavior
      const headLength = Math.min(8.9, Math.max(1.9, progressLength * 0.14));
      const headStart = Math.max(0, progressLength - headLength);
      this.headArc.setAttribute("stroke-dasharray", `${headLength} ${fullArcLength}`);
      this.headArc.setAttribute("stroke-dashoffset", `${-headStart}`);
      this.headArc.setAttribute("opacity", progressLength > 1.1 ? `${0.5 + 0.24 * norm}` : "0");

      // Arc intensity + reflection coupling
      const glowIntensity = (0.9 + 0.36 * taperCurve) * (0.98 + interactionEnergy * 0.32);
      this.arc.style.opacity = glowIntensity;
      el.style.setProperty("--arc-reflect", `${0.05 + 0.28 * Math.pow(norm, 0.92)}`);
      el.style.setProperty("--arc-angle", `${deg}deg`);
    }
  };


  const valueToNorm = (v) => {
    if (options.toNorm) return clamp(options.toNorm(v), 0, 1);
    return clamp((v - options.min) / (options.max - options.min), 0, 1);
  };

  const normToValue = (n) => {
    const norm = clamp(n, 0, 1);
    if (options.fromNorm) return options.fromNorm(norm);
    return options.min + norm * (options.max - options.min);
  };

  const setDragVisual = (value) => {
    const norm = clamp(options.toNorm ? options.toNorm(value) : (value - options.min) / (options.max - options.min), 0, 1);
    const deg = -135 + norm * 270;
    indicator.style.transform = `translateX(-50%) rotate(${deg}deg)`;
    const progressLength = Math.max(0, norm * trackLength);
    ctrl.arc.setAttribute("stroke-dasharray", `${progressLength} ${fullArcLength}`);
    // Keep expensive blur/filter layers static while dragging for smoother interaction.
    ctrl.bloomArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
    ctrl.massArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
    ctrl.coreArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
    ctrl.taperArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
    ctrl.headArc.setAttribute("stroke-dasharray", `0 ${fullArcLength}`);
    ctrl.headArc.setAttribute("opacity", "0");
    ctrl.arc.style.opacity = 1;
    el.style.setProperty("--arc-reflect", `${0.09 + 0.24 * Math.pow(norm, 0.92)}`);
    el.style.setProperty("--arc-angle", `${deg}deg`);
  };

  const updateKnobDragFromPointer = (e) => {
    if (!activeKnobDrag || activeKnobDrag.ctrl !== ctrl) return;
    if (activeKnobDrag.pointerId !== null && typeof e.pointerId !== "undefined" && e.pointerId !== activeKnobDrag.pointerId) return;
    if (e.cancelable) e.preventDefault();

    const coalesced = typeof e.getCoalescedEvents === "function" ? e.getCoalescedEvents() : null;
    const latest = (coalesced && coalesced.length > 0) ? coalesced[coalesced.length - 1] : e;
    const delta = activeKnobDrag.lastY - latest.clientY;
    activeKnobDrag.lastY = latest.clientY;
    const fine = (latest.shiftKey || latest.metaKey) ? 0.2 : 1;
    activeKnobDrag.norm = clamp(activeKnobDrag.norm + delta * 0.0031 * fine, 0, 1);

    const newValue = activeKnobDrag.normToValue(activeKnobDrag.norm);
    activeKnobDrag.options.set(clamp(newValue, activeKnobDrag.options.min, activeKnobDrag.options.max), false);
    activeKnobDrag.setDragVisual(activeKnobDrag.options.get());
    interactionEnergy = Math.min(1, interactionEnergy + 0.05);
  };

  const finishKnobDrag = (e, pushState = true) => {
    if (!activeKnobDrag || activeKnobDrag.ctrl !== ctrl) return;
    if (activeKnobDrag.pointerId !== null && e && typeof e.pointerId !== "undefined" && e.pointerId !== activeKnobDrag.pointerId) return;

    if (activeKnobDrag.pointerId !== null) {
      try {
        el.releasePointerCapture(activeKnobDrag.pointerId);
      } catch (_) {}
    }

    const drag = activeKnobDrag;
    activeKnobDrag = null;
    knobDragging = false;
    interactionUltraFast = false;
    setInteractionActive(false);
    drag.ctrl.set(drag.options.get(), false);
    updateSelectedBandReadouts(drag.options.readoutKey || "all");
    if (pushState) queuePushState();
  };

  // Pointerdown seeds drag and captures movement on this knob only.
  el.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    e.preventDefault();

    activeKnobDrag = {
      pointerId: typeof e.pointerId !== "undefined" ? e.pointerId : null,
      lastY: e.clientY,
      norm: valueToNorm(options.get()),
      options,
      ctrl,
      setDragVisual,
      normToValue
    };

    knobDragging = true;
    interactionUltraFast = true;
    try {
      if (typeof e.pointerId !== "undefined") el.setPointerCapture(e.pointerId);
    } catch (_) {}
    setInteractionActive(true);
  });

  el.addEventListener("pointermove", updateKnobDragFromPointer, { passive: false });
  el.addEventListener("pointerup", (e) => finishKnobDrag(e, true));
  el.addEventListener("pointercancel", (e) => finishKnobDrag(e, true));
  el.addEventListener("lostpointercapture", (e) => finishKnobDrag(e, true));

  el.addEventListener("wheel", (e) => {
    e.preventDefault();
    const fine = (e.shiftKey || e.metaKey) ? 0.24 : 1;
    const dir = e.deltaY > 0 ? -1 : 1;
    const currentNorm = valueToNorm(options.get());
    const stepNorm = 0.007 * fine;
    ctrl.set(normToValue(currentNorm + dir * stepNorm), true);
    lastDragValue = options.get();
    updateSelectedBandReadouts(options.readoutKey || "all");
    interactionEnergy = Math.min(1, interactionEnergy + 0.035);
  }, { passive: false });

  el.addEventListener("dblclick", () => {
    if (typeof options.defaultValue !== "number") return;
    ctrl.set(options.defaultValue, true);
    lastDragValue = options.get();
    updateSelectedBandReadouts(options.readoutKey || "all");
    interactionEnergy = Math.min(1, interactionEnergy + 0.12);
  });

  el.addEventListener("mouseenter", () => {
    const isDraggingThisKnob = !!activeKnobDrag && activeKnobDrag.ctrl === ctrl;
    if (!isDraggingThisKnob) {
      el.style.transition = "all 240ms cubic-bezier(0.22, 1, 0.36, 1)";
      ctrl.arc.style.filter = "drop-shadow(0 0 4px rgba(148, 104, 245, 0.48))";
      ctrl.massArc.style.filter = "drop-shadow(0 0 3px rgba(164, 108, 255, 0.28))";
      ctrl.headArc.style.filter = "drop-shadow(0 0 5px rgba(176, 142, 255, 0.52))";
      ambientRing.style.filter = "drop-shadow(0 0 2px rgba(110, 126, 248, 0.2))";
      ring.style.filter = "drop-shadow(0 0 1.2px rgba(124, 106, 238, 0.22))";
      indicator.style.filter = "drop-shadow(0 0 1.2px rgba(210, 220, 255, 0.26))";
    }
  });

  el.addEventListener("mouseleave", () => {
    const isDraggingThisKnob = !!activeKnobDrag && activeKnobDrag.ctrl === ctrl;
    if (!isDraggingThisKnob) {
      el.style.transition = "all 380ms ease-out";
      ctrl.arc.style.filter = "drop-shadow(0 0 1.2px rgba(126, 100, 226, 0.28))";
      ctrl.massArc.style.filter = "none";
      ctrl.headArc.style.filter = "drop-shadow(0 0 2.2px rgba(168, 144, 244, 0.34))";
      ambientRing.style.filter = "drop-shadow(0 0 1px rgba(106, 128, 248, 0.14))";
      ring.style.filter = "drop-shadow(0 0 1px rgba(106, 96, 208, 0.12))";
      indicator.style.filter = "drop-shadow(0 0 0.6px rgba(196, 212, 255, 0.18))";
    }
  });

  ctrl.set(options.get(), false);
  return ctrl;
}

function createKnobs() {
  knobControllers.freq = buildKnob(document.getElementById("knobFreq"), {
    min: FREQ_MIN,
    max: FREQ_MAX,
    get: () => selectedBand().frequency,
    set: (v, push) => { selectedBand().frequency = clamp(v, FREQ_MIN, FREQ_MAX); selectedBand().enabled = 1; if (push) queuePushState(); },
    sensitivity: 85,
    wheelStep: 0.015,
    toNorm: (v) => freqToNorm(v),
    fromNorm: (n) => normToFreq(n),
    defaultValue: 1000,
    readoutKey: "freq"
  });

  knobControllers.gain = buildKnob(document.getElementById("knobGain"), {
    min: -30,
    max: 30,
    get: () => selectedBand().gainDb,
    set: (v, push) => { selectedBand().gainDb = clamp(v, -30, 30); selectedBand().enabled = 1; if (push) queuePushState(); },
    sensitivity: 0.18,
    wheelStep: 0.2,
    defaultValue: 0,
    readoutKey: "gain"
  });

  knobControllers.q = buildKnob(document.getElementById("knobQ"), {
    min: 0.1,
    max: 10,
    get: () => selectedBand().q,
    set: (v, push) => { selectedBand().q = clamp(v, 0.1, 10); selectedBand().enabled = 1; if (push) queuePushState(); },
    sensitivity: 0.035,
    wheelStep: 0.08,
    defaultValue: 1.2,
    readoutKey: "q"
  });

  knobControllers.range = buildKnob(document.getElementById("knobRange"), {
    min: -30,
    max: 30,
    get: () => selectedBand().dynRangeDb,
    set: (v, push) => { selectedBand().dynRangeDb = clamp(v, -30, 30); selectedBand().mode = 1; if (push) queuePushState(); },
    sensitivity: 0.16,
    wheelStep: 0.2,
    defaultValue: -6,
    readoutKey: "range"
  });

  knobControllers.threshold = buildKnob(document.getElementById("knobThreshold"), {
    min: -60,
    max: 0,
    get: () => selectedBand().thresholdDb,
    set: (v, push) => { selectedBand().thresholdDb = clamp(v, -60, 0); selectedBand().mode = 1; if (push) queuePushState(); },
    sensitivity: 0.2,
    wheelStep: 0.2,
    defaultValue: -22,
    readoutKey: "threshold"
  });

  knobControllers.attack = buildKnob(document.getElementById("knobAttack"), {
    min: 0.1,
    max: 200,
    get: () => selectedBand().attackMs,
    set: (v, push) => { selectedBand().attackMs = clamp(v, 0.1, 200); selectedBand().mode = 1; if (push) queuePushState(); },
    sensitivity: 0.6,
    wheelStep: 1,
    defaultValue: 10,
    readoutKey: "attack"
  });

  knobControllers.release = buildKnob(document.getElementById("knobRelease"), {
    min: 10,
    max: 1000,
    get: () => selectedBand().releaseMs,
    set: (v, push) => { selectedBand().releaseMs = clamp(v, 10, 1000); selectedBand().mode = 1; if (push) queuePushState(); },
    sensitivity: 2.9,
    wheelStep: 8,
    defaultValue: 120,
    readoutKey: "release"
  });

  knobControllers.ratio = buildKnob(document.getElementById("knobRatio"), {
    min: 1,
    max: 10,
    get: () => selectedBand().ratio,
    set: (v, push) => { selectedBand().ratio = clamp(v, 1, 10); selectedBand().mode = 1; if (push) queuePushState(); },
    sensitivity: 0.03,
    wheelStep: 0.1,
    defaultValue: 2.2,
    readoutKey: "ratio"
  });

  knobControllers.resonance = buildKnob(document.getElementById("knobResonance"), {
    min: 0,
    max: 100,
    get: () => state.resonanceAmount,
    set: (v, push) => { state.resonanceAmount = clamp(v, 0, 100); if (push) queuePushState(); },
    sensitivity: 0.35,
    wheelStep: 1,
    defaultValue: 42,
    readoutKey: "resonance"
  });

  knobControllers.output = buildKnob(document.getElementById("knobOutput"), {
    min: -24,
    max: 24,
    get: () => state.outputGainDb,
    set: (v, push) => { state.outputGainDb = clamp(v, -24, 24); if (push) queuePushState(); },
    sensitivity: 0.08,
    wheelStep: 0.1,
    defaultValue: 0,
    readoutKey: "output"
  });
}

function populateUi() {
  loadUserPresets();

  for (let i = 0; i < MAX_BANDS; i++) {
    const option = document.createElement("option");
    option.value = String(i);
    option.textContent = `Band ${i + 1}`;
    bandIndexSelect.appendChild(option);
  }

  BAND_MODES.forEach((mode, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = mode;
    bandModeSelect.appendChild(option);
  });

  FILTER_TYPES.forEach((type, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = type;
    bandTypeSelect.appendChild(option);
  });

  CHANNEL_MODES.forEach((ch, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = ch;
    bandChannelSelect.appendChild(option);
  });

  rebuildPresetSelectOptions();
  updatePresetBrowserCurrentLabel(presetSelect ? presetSelect.value : "Default*");
  renderPresetCategoryColumn();
  renderPresetList();
  renderSaveCategoryGrid();
  updateSaveCounters();

  if (coModeSelect && !coModeSelect.options.length) {
    BAND_MODES.forEach((mode, index) => {
      const option = document.createElement("option");
      option.value = String(index);
      option.textContent = mode;
      coModeSelect.appendChild(option);
    });
  }

  if (coTypeSelect && !coTypeSelect.options.length) {
    FILTER_TYPES.forEach((type, index) => {
      const option = document.createElement("option");
      option.value = String(index);
      option.textContent = type;
      coTypeSelect.appendChild(option);
    });
  }

  for (let i = 0; i < 8; i++) {
    const button = document.createElement("button");
    button.className = "band-chip";
    button.textContent = i < 7 ? String(i + 1) : "+";
    button.onclick = () => {
      if (i === 7) {
        const idx = state.bands.findIndex((b) => b.enabled < 0.5);
        if (idx < 0) return;
        snapshotForUndo();
        state.selectedBand = idx;
        state.bands[idx].enabled = 1;
      } else {
        state.selectedBand = i;
      }
      syncControlsFromState();
      queuePushState();
    };
    bandButtons.appendChild(button);
  }
}

function syncControlsFromState(refreshKnobs = true) {
  const b = selectedBand();
  ensureDisplayBands();

  bandIndexSelect.value = String(state.selectedBand);
  if (bandDisplay) bandDisplay.textContent = `${state.selectedBand + 1}`;
  bandModeSelect.value = String(Math.round(b.mode));
  bandTypeSelect.value = String(Math.round(b.type));
  bandChannelSelect.value = String(Math.round(b.channel));
  phaseModeSelect.value = String(Math.round(state.phaseMode));

  updateSelectedBandReadouts();

  state.bands.forEach((src, i) => {
    if (!displayBands[i]) {
      displayBands[i] = createDisplayBandFromState(src);
      return;
    }
    displayBands[i].enabled = src.enabled;
    displayBands[i].mode = src.mode;
    displayBands[i].type = src.type;
    displayBands[i].channel = src.channel;
  });

  if (refreshKnobs) {
    Object.values(knobControllers).forEach((k) => k.set(k.value, false));
  }

  bypassBtn.style.opacity = state.bypassed > 0.5 ? "1" : "0.74";
  bypassBtn.classList.toggle("active", state.bypassed > 0.5);
  soloBtn.style.opacity = b.solo > 0.5 ? "1" : "0.74";
  soloBtn.classList.toggle("active", b.solo > 0.5);
  soloBtn.setAttribute("aria-pressed", b.solo > 0.5 ? "true" : "false");
  if (linkBtn) {
    const linkEnabled = state.harmonicLink > 0.5;
    linkBtn.classList.toggle("active", linkEnabled);
    linkBtn.setAttribute("aria-pressed", linkEnabled ? "true" : "false");
    linkBtn.title = linkEnabled ? "Harmonic Link: On" : "Harmonic Link: Off";
  }
  applySignalMotionState();
  const bandEnabled = b.enabled > 0.5;
  if (bandPanel) {
    bandPanel.classList.toggle("active", bandEnabled);
  }
  if (bandPowerBtn) {
    bandPowerBtn.classList.toggle("active", bandEnabled);
    bandPowerBtn.setAttribute("aria-pressed", bandEnabled ? "true" : "false");
    bandPowerBtn.title = bandEnabled ? "Disable selected band" : "Enable selected band";
  }

  Array.from(bandButtons.children).forEach((btn, i) => {
    if (i < 7)
      btn.classList.toggle("active", i === state.selectedBand);
  });

  document.querySelectorAll("#qualitySwitch button").forEach((btn) => {
    btn.classList.toggle("active", Number(btn.dataset.q) === Math.round(state.qualityMode));
  });

  document.querySelectorAll("#anSwitch button").forEach((btn) => {
    btn.classList.toggle("active", Number(btn.dataset.an) === Math.round(state.analyzerMode));
  });
}

function bindEvents() {
  bandIndexSelect.onchange = () => {
    state.selectedBand = clamp(Number(bandIndexSelect.value) || 0, 0, MAX_BANDS - 1);
    syncControlsFromState();
    queuePushState();
  };

  if (bandPrevBtn) {
    bandPrevBtn.onclick = () => {
      state.selectedBand = clamp(state.selectedBand - 1, 0, MAX_BANDS - 1);
      syncControlsFromState();
      queuePushState();
    };
  }

  if (bandNextBtn) {
    bandNextBtn.onclick = () => {
      state.selectedBand = clamp(state.selectedBand + 1, 0, MAX_BANDS - 1);
      syncControlsFromState();
      queuePushState();
    };
  }

  bandModeSelect.onchange = () => { selectedBand().mode = Number(bandModeSelect.value) || 0; selectedBand().enabled = 1; syncControlsFromState(); queuePushState(); };
  bandTypeSelect.onchange = () => { selectedBand().type = Number(bandTypeSelect.value) || 0; selectedBand().enabled = 1; syncControlsFromState(); queuePushState(); };
  bandChannelSelect.onchange = () => { selectedBand().channel = Number(bandChannelSelect.value) || 0; queuePushState(); };

  if (bandPowerBtn) {
    bandPowerBtn.onclick = () => {
      const b = selectedBand();
      b.enabled = b.enabled > 0.5 ? 0 : 1;
      syncControlsFromState();
      queuePushState();
    };
  }

  phaseModeSelect.onchange = () => { state.phaseMode = Number(phaseModeSelect.value) || 1; queuePushState(); };

  bypassBtn.onclick = () => { state.bypassed = state.bypassed > 0.5 ? 0 : 1; syncControlsFromState(); queuePushState(); };
  soloBtn.onclick = () => { selectedBand().solo = selectedBand().solo > 0.5 ? 0 : 1; syncControlsFromState(); queuePushState(); };

  meterSourceBtn.onclick = () => {
    meterSourceExternal = ! meterSourceExternal;
    meterSourceBtn.textContent = meterSourceExternal ? "Ext" : "Int";
  };

  scBtn.onclick = () => {
    sidechainExternal = true;
    scBtn.classList.add("active");
    intBtn.classList.remove("active");
  };

  intBtn.onclick = () => {
    sidechainExternal = false;
    intBtn.classList.add("active");
    scBtn.classList.remove("active");
  };

  if (scArrowBtn) {
    scArrowBtn.onclick = () => {
      state.signalMotion = state.signalMotion > 0.5 ? 0 : 1;
      applySignalMotionState();
      queuePushState();
    };
  }

  linkBtn.onclick = () => {
    state.harmonicLink = state.harmonicLink > 0.5 ? 0 : 1;
    syncControlsFromState(false);
    queuePushState();
  };

  undoBtn.onclick = () => {
    if (!undoStack.length) return;
    redoStack.push(JSON.stringify(state));
    Object.assign(state, JSON.parse(undoStack.pop()));
    syncControlsFromState();
    queuePushState();
  };

  redoBtn.onclick = () => {
    if (!redoStack.length) return;
    undoStack.push(JSON.stringify(state));
    Object.assign(state, JSON.parse(redoStack.pop()));
    syncControlsFromState();
    queuePushState();
  };

  abBtn.onclick = () => {
    const now = JSON.stringify(state);
    Object.assign(state, JSON.parse(snapshotA));
    snapshotA = now;
    syncControlsFromState();
    queuePushState();
  };

  presetSelect.onchange = () => {
    applyPresetByName(presetSelect.value);
    renderPresetList();
  };

  if (presetBrowserBtn) {
    presetBrowserBtn.onclick = () => {
      if (presetBrowserOpen) closePresetBrowser();
      else openPresetBrowser();
    };
  }

  if (presetBrowserClose) {
    presetBrowserClose.onclick = () => closePresetBrowser();
  }

  if (presetBrowserCloseBottom) {
    presetBrowserCloseBottom.onclick = () => closePresetBrowser();
  }

  if (presetSearchInput) {
    presetSearchInput.oninput = () => {
      presetSearchText = presetSearchInput.value.trim().toLowerCase();
      renderPresetList();
    };
  }

  if (presetTypeFilter) {
    presetTypeFilter.onchange = () => {
      presetTypeFilterValue = presetTypeFilter.value;
      renderPresetList();
    };
  }

  if (presetFavoriteFilter) {
    presetFavoriteFilter.onclick = () => {
      favoritesOnly = !favoritesOnly;
      presetFavoriteFilter.dataset.active = favoritesOnly ? "1" : "0";
      presetFavoriteFilter.setAttribute("aria-pressed", favoritesOnly ? "true" : "false");
      renderPresetList();
    };
  }

  if (presetNewFolderBtn) {
    presetNewFolderBtn.onclick = () => {
      window.alert("Folder creation will be enabled in an upcoming update.");
    };
  }

  if (presetImportBtn) {
    presetImportBtn.onclick = () => {
      window.alert("Preset import will be enabled in an upcoming update.");
    };
  }

  if (savePresetBtn) {
    savePresetBtn.onclick = () => openSavePresetModal();
  }

  if (savePresetClose) {
    savePresetClose.onclick = () => closeSavePresetModal();
  }

  if (savePresetCancel) {
    savePresetCancel.onclick = () => closeSavePresetModal();
  }

  if (savePresetNameInput) {
    savePresetNameInput.addEventListener("input", updateSaveCounters);
    savePresetNameInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        savePresetConfirm?.click();
      } else if (e.key === "Escape") {
        e.preventDefault();
        closeSavePresetModal();
      }
    });
  }

  if (savePresetDescription) {
    savePresetDescription.addEventListener("input", updateSaveCounters);
  }

  if (savePresetTypeSelect) {
    savePresetTypeSelect.onchange = () => {
      if (PRESET_CATEGORIES.includes(savePresetTypeSelect.value)) {
        savePresetCategory = savePresetTypeSelect.value;
        renderSaveCategoryGrid();
      }
    };
  }

  if (saveVisibility) {
    saveVisibility.querySelectorAll("button").forEach((btn) => {
      btn.onclick = () => {
        savePresetVisibility = btn.dataset.visibility === "Global" ? "Global" : "User";
        saveVisibility.querySelectorAll("button").forEach((x) => x.classList.toggle("active", x === btn));
      };
    });
  }

  if (saveFavoriteToggle) {
    saveFavoriteToggle.onclick = () => {
      savePresetFavorite = !savePresetFavorite;
      saveFavoriteToggle.dataset.active = savePresetFavorite ? "1" : "0";
      saveFavoriteToggle.setAttribute("aria-pressed", savePresetFavorite ? "true" : "false");
    };
  }

  if (savePresetConfirm) {
    savePresetConfirm.onclick = () => {
      saveCurrentPreset(
        savePresetNameInput ? savePresetNameInput.value : "",
        {
          category: savePresetCategory,
          type: savePresetTypeSelect ? savePresetTypeSelect.value : savePresetCategory,
          description: savePresetDescription ? savePresetDescription.value : "",
          visibility: savePresetVisibility,
          favorite: savePresetFavorite
        }
      );
    };
  }

  document.addEventListener("pointerdown", (e) => {
    if (!presetOverlay || !presetOverlay.classList.contains("visible")) return;
    if (presetBrowserPanel && presetBrowserPanel.classList.contains("visible") && presetBrowserPanel.contains(e.target)) return;
    if (savePresetModal && savePresetModal.classList.contains("visible") && savePresetModal.contains(e.target)) return;
    if (e.target === presetBrowserBtn || (presetBrowserBtn && presetBrowserBtn.contains(e.target))) return;
    if (e.target === savePresetBtn || (savePresetBtn && savePresetBtn.contains(e.target))) return;
    closePresetBrowser();
    closeSavePresetModal();
  });

  document.addEventListener("keydown", (e) => {
    if (e.key !== "Escape") return;
    if (savePresetOpen) {
      closeSavePresetModal();
      return;
    }
    if (presetBrowserOpen) closePresetBrowser();
  });

  document.querySelectorAll("#qualitySwitch button").forEach((btn) => {
    btn.onclick = () => { state.qualityMode = Number(btn.dataset.q) || 1; syncControlsFromState(); queuePushState(); };
  });

  document.querySelectorAll("#anSwitch button").forEach((btn) => {
    btn.onclick = () => { state.analyzerMode = Number(btn.dataset.an) || 0; syncControlsFromState(); queuePushState(); };
  });

  canvas.addEventListener("pointerdown", onGraphDown);
  canvas.addEventListener("pointermove", onGraphMove);
  canvas.addEventListener("pointerup", onGraphUp);
  canvas.addEventListener("pointercancel", onGraphUp);
  canvas.addEventListener("lostpointercapture", onGraphUp);
  canvas.addEventListener("contextmenu", onGraphContextMenu);
  // Invalidate cached canvas rect on window resize so coordinates remain accurate.
  window.addEventListener("resize", () => {
    cachedCanvasRect = null;
    applyUiScale();
  });
  // When clicking on callout, disable solo persistence so it can hide normally
  callout.addEventListener("click", () => {
    calloutSoloPersistent = false;
  });

  graphWrap.addEventListener("mouseleave", () => {
    if (draggingBand < 0 && !calloutHovering) {
      hoveredBand = -1;
      if (!calloutSoloPersistent) {
        scheduleCalloutHide(450);
      }
    }
  });

  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    const b = selectedBand();
    const fine = (e.shiftKey || e.metaKey) ? 0.33 : 1;
    b.q = clamp(b.q + (e.deltaY > 0 ? -0.07 : 0.07) * fine, 0.1, 10);
    interactionEnergy = Math.min(1, interactionEnergy + 0.035);
    syncControlsFromState();
    queuePushState();
  }, { passive: false });

  const coSoloBtn = document.getElementById("coSoloBtn");
  if (coSoloBtn) {
    coSoloBtn.onclick = (e) => {
      e.preventDefault();
      e.stopPropagation();
      if (calloutBandIndex < 0 || calloutBandIndex >= state.bands.length) return;
      const b = state.bands[calloutBandIndex];
      b.solo = b.solo > 0.5 ? 0 : 1;
      state.selectedBand = calloutBandIndex;
      syncControlsFromState(false);
      queuePushState();
      calloutVisible = true;
      calloutSoloPersistent = (b.solo > 0.5);
      cancelCalloutHide();
    };
  }

  if (coModeSelect) {
    coModeSelect.onchange = () => {
      if (calloutBandIndex < 0 || calloutBandIndex >= state.bands.length) return;
      const b = state.bands[calloutBandIndex];
      b.mode = Number(coModeSelect.value) || 0;
      b.enabled = 1;
      syncControlsFromState(false);
      queuePushState();
    };
  }

  if (coTypeSelect) {
    coTypeSelect.onchange = () => {
      if (calloutBandIndex < 0 || calloutBandIndex >= state.bands.length) return;
      const b = state.bands[calloutBandIndex];
      b.type = Number(coTypeSelect.value) || 0;
      b.enabled = 1;
      syncControlsFromState(false);
      queuePushState();
    };
  }

  callout.addEventListener("mouseenter", () => {
    calloutHovering = true;
    calloutVisible = true;
    cancelCalloutHide();
  });

  callout.addEventListener("mouseleave", () => {
    calloutHovering = false;
    if (calloutContextMode) return;
    if (!calloutSoloPersistent) {
      scheduleCalloutHide(500);
    }
  });

  // Dismiss persistent solo callout when clicking elsewhere
  document.addEventListener("mousedown", (e) => {
    if ((calloutSoloPersistent || calloutContextMode) && !callout.contains(e.target) && !canvas.contains(e.target)) {
      calloutSoloPersistent = false;
      calloutContextMode = false;
      if (coEdit) coEdit.classList.remove("visible");
      scheduleCalloutHide(150);
    }
  });

}

function findNearestBandIndex(x, y, width, height, radius = 20) {
  let nearest = -1;
  let best = Number.POSITIVE_INFINITY;
  for (let i = 0; i < state.bands.length; i++) {
    const b = state.bands[i];
    if (b.enabled < 0.5) continue;
    const nx = hzToX(b.frequency, width);
    const ny = gainToY(b.gainDb, height);
    const d = Math.hypot(nx - x, ny - y);
    if (d < best) {
      best = d;
      nearest = i;
    }
  }
  return best <= radius ? nearest : -1;
}

function createBandAtGraphPosition(x, y, rect, beginDrag = false) {
  const idx = state.bands.findIndex((b) => b.enabled < 0.5);
  if (idx < 0) return false;

  snapshotForUndo();
  state.selectedBand = idx;
  const b = state.bands[idx];
  b.enabled = 1;
  b.type = 0;
  b.mode = 0;
  b.channel = 0;
  b.frequency = clamp(xToHz(x, rect.width), FREQ_MIN, FREQ_MAX);
  b.gainDb = clamp(yToGain(y, rect.height), -30, 30);
  b.q = 1.2;
  hoveredBand = idx;
  calloutVisible = true;
  calloutTargetX = x;
  calloutTargetY = y;
  interactionEnergy = Math.min(1, interactionEnergy + 0.22);
  if (beginDrag) draggingBand = idx;
  if (!beginDrag) {
    syncControlsFromState();
    queuePushState();
  } else {
    // Dragging immediately — defer full sync and state push to onGraphUp.
    if (bandDisplay) bandDisplay.textContent = `${idx + 1}`;
    bandIndexSelect.value = String(idx);
  }
  return true;
}

function onGraphDown(e) {
  e.preventDefault();
  if (e.button !== 0) return;
  calloutContextMode = false;
  if (coEdit) coEdit.classList.remove("visible");
  canvas.setPointerCapture(e.pointerId);
  setInteractionActive(true);
  cachedCanvasRect = canvas.getBoundingClientRect();
  const rect = cachedCanvasRect;
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  const nearest = findNearestBandIndex(x, y, rect.width, rect.height, 18);

  if (nearest >= 0) {
    snapshotForUndo();
    graphDragUndoPending = false;
    draggingBand = nearest;
    interactionUltraFast = true;
    hoveredBand = nearest;
    state.selectedBand = nearest;
    const b = state.bands[nearest];
    // Snap immediately on pickup so the node follows from the first click.
    b.frequency = clamp(xToHz(x, rect.width), FREQ_MIN, FREQ_MAX);
    b.gainDb = clamp(yToGain(y, rect.height), -30, 30);
    b.enabled = 1;
    calloutVisible = !interactionUltraFast;
    calloutTargetX = x;
    calloutTargetY = y;
    interactionEnergy = Math.min(1, interactionEnergy + 0.18);
    // Skip full syncControlsFromState during drag — too expensive to fire per frame.
    // Only update the band selector readout so the UI label stays correct.
    if (bandDisplay) bandDisplay.textContent = `${nearest + 1}`;
    bandIndexSelect.value = String(nearest);
    return;
  }

  // Single-click on empty graph creates a new band and allows immediate drag.
  const created = createBandAtGraphPosition(x, y, rect, true);
  if (created) interactionUltraFast = true;
}

function onGraphMove(e) {
  const rect = cachedCanvasRect || (cachedCanvasRect = canvas.getBoundingClientRect());
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;
  hoverGraphX = x;
  hoverGraphY = y;

  if (calloutContextMode && draggingBand < 0) {
    const idx = calloutBandIndex >= 0 ? calloutBandIndex : state.selectedBand;
    const bCtx = state.bands[idx] || selectedBand();
    hoveredBand = idx;
    calloutVisible = true;
    calloutTargetX = hzToX(bCtx.frequency, rect.width);
    calloutTargetY = gainToY(bCtx.gainDb, rect.height);
    if (coEdit) coEdit.classList.add("visible");
  } else if (draggingBand < 0) {
    if (calloutHovering && calloutBandIndex >= 0) {
      hoveredBand = calloutBandIndex;
      calloutVisible = true;
      cancelCalloutHide();
    } else {
      const newHovered = findNearestBandIndex(x, y, rect.width, rect.height, 22);
      if (newHovered >= 0) {
        hoveredBand = newHovered;
        calloutBandIndex = newHovered;
        calloutVisible = true;
        cancelCalloutHide();
      } else {
        hoveredBand = -1;
        if (!calloutSoloPersistent && !calloutHovering) {
          scheduleCalloutHide(420);
        }
      }
    }
  }

  if (draggingBand >= 0) {
    const dragFine = (e.shiftKey || e.metaKey) ? 0.24 : 1;
    interactionUltraFast = true;
    dragPreviewActive = true;
    calloutVisible = false;
    calloutBandIndex = -1;

    const b = state.bands[draggingBand];
    const targetFreq = clamp(xToHz(x, rect.width), FREQ_MIN, FREQ_MAX);
    const targetGain = clamp(yToGain(y, rect.height), -30, 30);
    // Keep tiny damping only for fine-control drag; normal drag follows immediately.
    const dragResponse = dragFine < 1 ? 0.42 : 1.0;
    const newFreq = clamp(b.frequency + (targetFreq - b.frequency) * dragResponse, FREQ_MIN, FREQ_MAX);
    const newGain = clamp(b.gainDb + (targetGain - b.gainDb) * dragResponse, -30, 30);
    dragPreviewFreq = newFreq;
    dragPreviewGain = newGain;
    b.frequency = newFreq;
    b.gainDb = newGain;
    b.enabled = 1;
    interactionEnergy = Math.min(1, interactionEnergy + 0.06);
    return;
  }

  const activeIdx = draggingBand >= 0 ? draggingBand : (hoveredBand >= 0 ? hoveredBand : state.selectedBand);
  const b = state.bands[activeIdx] || selectedBand();
  calloutBandIndex = activeIdx;
  const anchorX = draggingBand >= 0 ? x : (calloutBandIndex >= 0 ? hzToX(b.frequency, rect.width) : x);
  const anchorY = draggingBand >= 0 ? y : (calloutBandIndex >= 0 ? gainToY(b.gainDb, rect.height) : y);
  calloutTargetX = anchorX;
  calloutTargetY = anchorY;
  if (calloutVisible) callout.classList.add("visible");
  else callout.classList.remove("visible");
  const coFreq = document.getElementById("coFreq");
  const coGain = document.getElementById("coGain");
  if (coFreq) coFreq.textContent = fmtHz(b.frequency);
  if (coGain) coGain.textContent = fmtDb(b.gainDb);

  // Avoid expensive, unchanged DOM churn while dragging.
  if (draggingBand < 0) {
    const coType = document.getElementById("coType");
    const coQ = document.getElementById("coQ");
    if (coType) coType.textContent = FILTER_TYPES[Math.round(b.type)] || "Bell";
    if (coQ) coQ.textContent = b.q.toFixed(2);

    const coSoloBtn = document.getElementById("coSoloBtn");
    if (coSoloBtn) {
      const soloActive = b.solo > 0.5;
      coSoloBtn.innerHTML = `<span class="icon">🎧</span><span>${soloActive ? "Unsolo Band" : "Band Solo"}</span>`;
      coSoloBtn.classList.toggle("active", soloActive);
    }
  }
}

function onGraphContextMenu(e) {
  e.preventDefault();
  const rect = cachedCanvasRect || (cachedCanvasRect = canvas.getBoundingClientRect());
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;
  const nearest = findNearestBandIndex(x, y, rect.width, rect.height, 22);
  if (nearest < 0) return;

  state.selectedBand = nearest;
  hoveredBand = nearest;
  calloutBandIndex = nearest;
  calloutVisible = true;
  calloutContextMode = true;
  calloutSoloPersistent = true;
  calloutTargetX = hzToX(state.bands[nearest].frequency, rect.width);
  calloutTargetY = gainToY(state.bands[nearest].gainDb, rect.height);
  cancelCalloutHide();

  if (bandDisplay) bandDisplay.textContent = `${nearest + 1}`;
  bandIndexSelect.value = String(nearest);

  const b = state.bands[nearest];
  const coType = document.getElementById("coType");
  const coQ = document.getElementById("coQ");
  const coFreq = document.getElementById("coFreq");
  const coGain = document.getElementById("coGain");
  if (coType) coType.textContent = FILTER_TYPES[Math.round(b.type)] || "Bell";
  if (coQ) coQ.textContent = b.q.toFixed(2);
  if (coFreq) coFreq.textContent = fmtHz(b.frequency);
  if (coGain) coGain.textContent = fmtDb(b.gainDb);
  if (coEdit) coEdit.classList.add("visible");
  if (coModeSelect) coModeSelect.value = String(Math.round(b.mode || 0));
  if (coTypeSelect) coTypeSelect.value = String(Math.round(b.type || 0));

  const coSoloBtn = document.getElementById("coSoloBtn");
  if (coSoloBtn) {
    const soloActive = b.solo > 0.5;
    coSoloBtn.innerHTML = `<span class="icon">🎧</span><span>${soloActive ? "Unsolo Band" : "Band Solo"}</span>`;
    coSoloBtn.classList.toggle("active", soloActive);
  }
}

function onGraphUp() {
  const releasedBand = draggingBand;
  draggingBand = -1;
  graphDragUndoPending = false;
  interactionUltraFast = false;
  setInteractionActive(false);

  // Commit once after drag ends to avoid per-frame sync lag.
  if (releasedBand >= 0) {
    state.selectedBand = releasedBand;
    if (bandDisplay) bandDisplay.textContent = `${releasedBand + 1}`;
    if (bandIndexSelect) bandIndexSelect.value = String(releasedBand);
    updateSelectedBandReadouts("all");
    const rect = cachedCanvasRect || canvas.getBoundingClientRect();
    const released = state.bands[releasedBand];
    if (released) {
      calloutBandIndex = releasedBand;
      calloutTargetX = hzToX(released.frequency, rect.width);
      calloutTargetY = gainToY(released.gainDb, rect.height);
      calloutVisible = true;
    }
    requestAnimationFrame(() => {
      syncControlsFromState(false);
    });
    queuePushState();
  }

  if (hoveredBand < 0) calloutVisible = false;
}

function drawFallbackSpectrum(now) {
  if (now - lastAnalyzerUpdateMs < 380)
    return;

  noisePhase += 0.025;
  const localDyn = 0.14 + 0.11 * Math.sin(now * 0.0018);

  for (let i = 0; i < BINS; i++) {
    const t = i / (BINS - 1);
    let base = 0.18 + 0.28 * (1 - t) + 0.07 * Math.sin(noisePhase + t * 16.0) + 0.04 * Math.sin(noisePhase * 2.2 + t * 63.0);
    base = clamp(base, 0.03, 0.95);

    let eqShape = 0;
    for (let b = 0; b < state.bands.length; b++) {
      const band = state.bands[b];
      if (band.enabled < 0.5) continue;
      const fNorm = freqToNorm(band.frequency);
      const width = 0.03 + (1.0 / Math.max(0.2, band.q)) * 0.06;
      const dist = (t - fNorm) / width;
      const weight = Math.exp(-dist * dist * 0.5);
      eqShape += (band.gainDb / 30.0) * weight;
    }

    const pre = clamp(base + eqShape * 0.02, 0.02, 0.95);
    const post = clamp(base + eqShape * 0.16, 0.02, 0.96);

    preSpectrum[i] = preSpectrum[i] * 0.79 + pre * 0.21;
    postSpectrum[i] = postSpectrum[i] * 0.77 + post * 0.23;
    reductionSpectrum[i] = reductionSpectrum[i] * 0.84 + localDyn * (0.2 + 0.55 * (1 - t)) * 0.16;
  }

  dynActivity = localDyn;
  outputPeak = 0.18 + 0.2 * (0.5 + 0.5 * Math.sin(now * 0.003));

  // Simulate dynamic gain reduction for visualization
  for (let band = 0; band < state.bands.length; band++) {
    const b = state.bands[band];
    if (b.enabled < 0.5 || b.mode < 0.5) {
      bandDynamicGainDb[band] = 0;
      continue;
    }

    // Simulate detection based on local dynamics
    const detectionSim = localDyn * 0.8 + Math.sin(now * 0.0015 + band) * 0.2;
    const threshold = -22; // dB equivalent
    const detectionDb = 20 * Math.log10(Math.max(0.001, detectionSim));
    const gainReduction = b.dynRangeDb < 0 ?
      Math.min(Math.abs(b.dynRangeDb), Math.max(0, detectionDb - threshold) * 0.4) : 0;
    
    bandDynamicGainDb[band] = -gainReduction * (0.6 + 0.4 * Math.sin(now * 0.003));
  }
}

function drawGraph() {
  const now = performance.now();
  const dt = Math.min(64, Math.max(0, now - lastFrameMs || 16));
  lastFrameMs = now;
  interactionEnergy = Math.max(0, interactionEnergy * Math.exp(-dt / 220));
  const signalMotionTarget = state.signalMotion > 0.5 ? 1 : 0;
  signalMotionVisual = signalMotionVisual * 0.9 + signalMotionTarget * 0.1;
  const signalMotionAmt = signalMotionVisual;
  const reactiveDyn = dynActivity * (0.18 + 0.82 * signalMotionAmt);
  const reactivePeak = outputPeak * (0.2 + 0.8 * signalMotionAmt);
  const fastInteraction = draggingBand >= 0 || knobDragging;
  const ultraFast = interactionUltraFast || fastInteraction;

  ensureDisplayBands();
  for (let i = 0; i < state.bands.length; i++) {
    const src = state.bands[i];
    const dst = displayBands[i];
    if (!dst) continue;
    dst.frequency = src.frequency;
    dst.gainDb = src.gainDb;
    dst.q = src.q;
    dst.dynRangeDb = src.dynRangeDb;
    dst.enabled = src.enabled;
    dst.mode = src.mode;
    dst.type = src.type;
    dst.channel = src.channel;
  }

  drawFallbackSpectrum(now);

  const rect = graphWrap.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  const targetW = Math.floor(rect.width * dpr);
  const targetH = Math.floor(rect.height * dpr);

  if (canvas.width !== targetW || canvas.height !== targetH) {
    canvas.width = targetW;
    canvas.height = targetH;
  }

  const w = rect.width;
  const h = rect.height;

  if (ultraFast) {
    callout.classList.remove("visible");
  } else if (calloutVisible) {
    const targetX = clamp(calloutTargetX, 0, w);
    const targetY = clamp(calloutTargetY, 0, h);
    calloutX = smoothTo(calloutX, targetX, 26, dt);
    calloutY = smoothTo(calloutY, targetY, 26, dt);

    const cw = Math.max(180, callout.offsetWidth || 0);
    const ch = Math.max(110, callout.offsetHeight || 0);
    const edgePad = 8;
    const left = clamp(calloutX - cw * 0.5, edgePad, Math.max(edgePad, w - cw - edgePad));
    const preferAboveTop = calloutY - ch - 14;
    const top = preferAboveTop < edgePad ? clamp(calloutY + 14, edgePad, Math.max(edgePad, h - ch - edgePad)) : clamp(preferAboveTop, edgePad, Math.max(edgePad, h - ch - edgePad));

    callout.style.left = `${left}px`;
    callout.style.top = `${top}px`;
    callout.classList.add("visible");
  } else {
    callout.classList.remove("visible");
  }

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  ctx.shadowBlur = 0;
  ctx.shadowColor = "transparent";

  if (!fastInteraction) {
    // Deep-space atmospheric layering behind all graph content.
    const rearFog = ctx.createRadialGradient(w * 0.5, h * 1.18, 0, w * 0.5, h * 1.18, h * 1.25);
    rearFog.addColorStop(0, "rgba(0, 0, 0, 0.18)");
    rearFog.addColorStop(0.56, "rgba(10, 18, 40, 0.10)");
    rearFog.addColorStop(1, "rgba(0, 0, 0, 0)");
    ctx.fillStyle = rearFog;
    ctx.fillRect(0, 0, w, h);

    const centerLift = ctx.createRadialGradient(w * 0.5, h * 0.52, 0, w * 0.5, h * 0.52, h * 0.75);
    centerLift.addColorStop(0, "rgba(188, 208, 255, 0.085)");
    centerLift.addColorStop(0.42, "rgba(154, 172, 244, 0.048)");
    centerLift.addColorStop(1, "rgba(0, 0, 0, 0)");
    ctx.fillStyle = centerLift;
    ctx.fillRect(0, 0, w, h);

    const cornerDarkness = ctx.createRadialGradient(w * 0.5, h * 0.5, Math.min(w, h) * 0.35, w * 0.5, h * 0.5, Math.min(w, h) * 0.8);
    cornerDarkness.addColorStop(0, "rgba(0, 0, 0, 0)");
    cornerDarkness.addColorStop(0.7, "rgba(0, 0, 0, 0)");
    cornerDarkness.addColorStop(1, "rgba(0, 0, 0, 0.13)");
    ctx.fillStyle = cornerDarkness;
    ctx.fillRect(0, 0, w, h);
  }

  ctx.strokeStyle = "rgba(108, 132, 206, 0.15)";
  ctx.lineWidth = 0.9;

  if (knobDragging) {
    // Keep knob-driven interaction ultra-responsive: draw only anchor guides.
    [20, 1000, 20000].forEach((hz) => {
      const x = hzToX(hz, w);
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    });
    const y = gainToY(0, h);
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  } else {
    [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000].forEach((hz) => {
      const x = hzToX(hz, w);
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    });

    for (let g = -24; g <= 24; g += 6) {
      const y = gainToY(g, h);
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }
  }

  if (fastInteraction) {
    const active = displayBands
      .map((b, i) => ({ b, i }))
      .filter((entry) => entry.b.enabled > 0.5)
      .sort((a, b) => a.b.frequency - b.b.frequency);
    const activeBands = active.map((entry) => entry.b);

    // Normal fast interaction (not ultra-flat):
    if (active.length > 0) {
      ctx.shadowBlur = 0;
      ctx.shadowColor = "transparent";
      ctx.beginPath();
      ctx.lineWidth = 3.25;
      drawEqResponsePath(ctx, activeBands, w, h, 84);
      const lineGrad = ctx.createLinearGradient(0, 0, w, 0);
      lineGrad.addColorStop(0, "#ffffff");
      lineGrad.addColorStop(0.15, "#faf6ff");
      lineGrad.addColorStop(0.35, "#e86dff");
      lineGrad.addColorStop(0.62, "#a888ff");
      lineGrad.addColorStop(1, "#88c8ff");
      ctx.strokeStyle = lineGrad;
      ctx.stroke();
    }

    displayBands.forEach((b, i) => {
      if (b.enabled < 0.5) return;
      const x = hzToX(b.frequency, w);
      const y = gainToY(b.gainDb, h);
      const selected = i === state.selectedBand;
      const activeDrag = i === draggingBand;
      const dynamic = b.mode > 0.5;
      const notch = b.type === 5;

      ctx.beginPath();
      ctx.arc(x, y, 11.5, 0, Math.PI * 2);
      ctx.fillStyle = "rgba(6, 10, 24, 0.94)";
      ctx.fill();
      ctx.strokeStyle = notch ? "#89b4ff" : selected ? "#c099ff" : dynamic ? "#7fa8ff" : "#d6e4ff";
      ctx.lineWidth = activeDrag ? 2.5 : selected ? 2.2 : 1.9;
      ctx.stroke();

      ctx.fillStyle = "#edf3ff";
      ctx.font = "600 13px Avenir Next";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(String(i + 1), x, y + 0.5);
    });

    rafHandle = requestAnimationFrame(drawGraph);
    return;
  }

  for (let i = 0; i < BINS; i++) {
    preSmoothed[i] = preSmoothed[i] * 0.88 + preSpectrum[i] * 0.12;
    postSmoothed[i] = postSmoothed[i] * 0.86 + postSpectrum[i] * 0.14;
    reductionSmoothed[i] = reductionSmoothed[i] * 0.9 + reductionSpectrum[i] * 0.1;
    analyzerTrailA[i] = analyzerTrailA[i] * 0.93 + showSafe(preSmoothed[i], postSmoothed[i], state.analyzerMode) * 0.07;
    analyzerTrailB[i] = analyzerTrailB[i] * 0.96 + analyzerTrailA[i] * 0.04;
    analyzerTrailC[i] = analyzerTrailC[i] * 0.98 + analyzerTrailB[i] * 0.02;
  }

  const show = state.analyzerMode > 0.5 ? postSmoothed : preSmoothed;

  ctx.beginPath();
  ctx.moveTo(0, h);
  for (let i = 0; i < BINS; i++) {
    const x = (i / (BINS - 1)) * w;
    const y = (1 - show[i]) * h;
    ctx.lineTo(x, y);
  }
  ctx.lineTo(w, h);
  ctx.closePath();

  const fillGrad = ctx.createLinearGradient(0, 0, 0, h);
  fillGrad.addColorStop(0, "rgba(184, 202, 255, 0.036)");
  fillGrad.addColorStop(0.45, "rgba(122, 126, 222, 0.016)");
  fillGrad.addColorStop(1, "rgba(50, 70, 132, 0.014)");
  ctx.fillStyle = fillGrad;
  ctx.fill();

  if (!fastInteraction) {
    // Soft volumetric haze between analyzer trails
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(0, h);
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - analyzerTrailC[i]) * h;
      ctx.lineTo(x, y);
    }
    ctx.lineTo(w, h);
    ctx.closePath();
    const hazeC = ctx.createLinearGradient(0, 0, 0, h);
    hazeC.addColorStop(0, "rgba(108, 126, 220, 0.017)");
    hazeC.addColorStop(1, "rgba(108, 126, 220, 0)");
    ctx.fillStyle = hazeC;
    ctx.fill();
    ctx.restore();

    ctx.save();
    ctx.beginPath();
    ctx.moveTo(0, h);
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - analyzerTrailB[i]) * h;
      ctx.lineTo(x, y);
    }
    ctx.lineTo(w, h);
    ctx.closePath();
    const hazeB = ctx.createLinearGradient(0, 0, 0, h);
    hazeB.addColorStop(0, "rgba(150, 140, 242, 0.013)");
    hazeB.addColorStop(1, "rgba(150, 140, 242, 0)");
    ctx.fillStyle = hazeB;
    ctx.fill();
    ctx.restore();

    ctx.save();
    ctx.beginPath();
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - analyzerTrailC[i]) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "rgba(108, 126, 220, 0.006)";
    ctx.lineWidth = 5.4;
    ctx.shadowColor = "rgba(98, 112, 208, 0.045)";
    ctx.shadowBlur = 12;
    ctx.stroke();
    ctx.restore();

    ctx.save();
    ctx.beginPath();
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - analyzerTrailB[i]) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "rgba(132, 156, 242, 0.014)";
    ctx.lineWidth = 3.9;
    ctx.shadowColor = "rgba(124, 146, 233, 0.058)";
    ctx.shadowBlur = 10;
    ctx.stroke();
    ctx.restore();
  }

  if (!fastInteraction) {
    ctx.save();
    ctx.beginPath();
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - analyzerTrailA[i]) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "rgba(170, 168, 255, 0.018)";
    ctx.lineWidth = 0.9;
    ctx.shadowColor = "rgba(156, 130, 255, 0.045)";
    ctx.shadowBlur = 7;
    ctx.stroke();
    ctx.restore();

    ctx.save();
    ctx.globalAlpha = 0.34;
    ctx.beginPath();
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - show[i]) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "rgba(182, 204, 255, 0.16)";
    ctx.lineWidth = 1.5;
    ctx.shadowColor = "rgba(134, 154, 255, 0.12)";
    ctx.shadowBlur = 8;
    ctx.stroke();
    ctx.restore();

    ctx.beginPath();
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - preSmoothed[i]) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "rgba(188, 208, 255, 0.05)";
    ctx.lineWidth = 1.1;
    ctx.stroke();

    ctx.save();
    ctx.beginPath();
    for (let i = 0; i < BINS; i++) {
      const x = (i / (BINS - 1)) * w;
      const y = (1 - reductionSmoothed[i] * 0.9) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = "rgba(166, 118, 255, 0.14)";
    ctx.lineWidth = 1.0;
    ctx.stroke();
    ctx.restore();
  }

  const active = displayBands
    .map((b, i) => ({ b, i }))
    .filter((entry) => entry.b.enabled > 0.5)
    .sort((a, b) => a.b.frequency - b.b.frequency);
  const activeBands = active.map((entry) => entry.b);

  if (active.length > 0) {
    if (fastInteraction) {
      ctx.beginPath();
      ctx.lineWidth = 2.8;
      drawEqResponsePath(ctx, activeBands, w, h);
      const lineGrad = ctx.createLinearGradient(0, 0, w, 0);
      lineGrad.addColorStop(0, "#ffffff");
      lineGrad.addColorStop(0.2, "#f0f4ff");
      lineGrad.addColorStop(0.4, "#e858ff");
      lineGrad.addColorStop(0.65, "#9f72ff");
      lineGrad.addColorStop(1, "#88c8ff");
      ctx.strokeStyle = lineGrad;
      const reactiveBloom = 4 + reactiveDyn * 3;
      ctx.shadowColor = `rgba(255, 186, 255, ${0.84 + reactiveDyn * 0.24})`;
      ctx.shadowBlur = reactiveBloom;
      ctx.stroke();
      ctx.shadowBlur = 0;
    } else {
    const bandThree = displayBands[2];
    if (bandThree && bandThree.enabled > 0.5) {
      const cx = hzToX(bandThree.frequency, w);
      const cy = gainToY(bandThree.gainDb, h);
      const valleyGlow = ctx.createRadialGradient(cx, cy, 0, cx, cy, 82);
      valleyGlow.addColorStop(0, "rgba(172, 128, 255, 0.17)");
      valleyGlow.addColorStop(0.48, "rgba(118, 110, 240, 0.08)");
      valleyGlow.addColorStop(1, "rgba(0, 0, 0, 0)");
      ctx.save();
      ctx.fillStyle = valleyGlow;
      ctx.beginPath();
      ctx.arc(cx, cy, 82, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    }

    ctx.beginPath();
    ctx.lineWidth = 3.25;
    drawEqResponsePath(ctx, activeBands, w, h);

    const lineGrad = ctx.createLinearGradient(0, 0, w, 0);
    lineGrad.addColorStop(0, "#ffffff");
    lineGrad.addColorStop(0.15, "#faf6ff");
    lineGrad.addColorStop(0.35, "#f056ff");
    lineGrad.addColorStop(0.62, "#a56fff");
    lineGrad.addColorStop(1, "#88c8ff");
    ctx.strokeStyle = lineGrad;
    const reactiveBloom2 = 8 + reactiveDyn * 5;
    ctx.shadowColor = `rgba(255, 138, 255, ${1 + reactiveDyn * 0.16})`;
    ctx.shadowBlur = reactiveBloom2;
    ctx.stroke();
    ctx.beginPath();
    ctx.lineWidth = 5.6;
    drawEqResponsePath(ctx, activeBands, w, h);
    ctx.strokeStyle = "rgba(214, 160, 255, 0.2)";
    ctx.shadowColor = "rgba(190, 146, 255, 0.28)";
    ctx.shadowBlur = 13 + reactiveDyn * 4;
    ctx.stroke();
    ctx.shadowBlur = 0;
    }
  }

  const pulse = 1 + Math.sin(now * 0.003) * 0.08;
  const breathe = 1 + Math.sin(now * 0.0021) * 0.04;

  displayBands.forEach((b, i) => {
    if (b.enabled < 0.5) return;
    const x = hzToX(b.frequency, w);
    const y = gainToY(b.gainDb, h);

    const selected = i === state.selectedBand;
    const hovered = i === hoveredBand;
    const activeDrag = i === draggingBand;
    const dynamic = b.mode > 0.5;
    const notch = b.type === 5;

    if (fastInteraction) {
      ctx.beginPath();
      ctx.arc(x, y, 11.5, 0, Math.PI * 2);
      ctx.fillStyle = "rgba(6, 10, 24, 0.94)";
      ctx.fill();
      ctx.strokeStyle = notch ? "#89b4ff" : selected ? "#c099ff" : dynamic ? "#7fa8ff" : "#d6e4ff";
      ctx.lineWidth = activeDrag ? 2.5 : selected ? 2.2 : 1.9;
      ctx.stroke();

      ctx.fillStyle = "#edf3ff";
      ctx.font = "600 13px Avenir Next";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(String(i + 1), x, y + 0.5);
      return;
    }

    const focusBoost = activeDrag ? 1.46 : hovered ? 1.2 : selected ? 1.16 : 1.04;
    const gainIntensity = Math.abs(b.gainDb) / 30;
    const qIntensity = Math.min(1, (b.q || 1) / 6);
    const contourEnergy = 0.76 + gainIntensity * 0.44 + qIntensity * 0.22 + (dynamic ? 0.08 : 0);
    const selectedHaloBoost = selected ? 1.16 : 1;
    const energyBoost = contourEnergy * selectedHaloBoost;
    const reactiveSync = 1 + Math.sin(now * 0.004 + x * 0.02) * 0.06 * (reactiveDyn + 0.3);
    const halo = (selected ? (18 * pulse * breathe + reactiveDyn * 8) : dynamic ? (11 + reactiveDyn * 12) : 7.5) * focusBoost * energyBoost * reactiveSync;
    
    // Outer diffuse glow (largest)
    const g1 = ctx.createRadialGradient(x, y, 0, x, y, halo * 1.06);
    const outerIntensity = 0.11 + 0.1 * gainIntensity + qIntensity * 0.05;
    g1.addColorStop(0, selected ? `rgba(200, 132, 255, ${0.3 + 0.14 * gainIntensity})` : dynamic ? `rgba(116, 140, 255, ${0.16 + 0.1 * gainIntensity})` : `rgba(194, 214, 255, ${outerIntensity})`);
    g1.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = g1;
    ctx.beginPath();
    ctx.arc(x, y, halo * 1.06, 0, Math.PI * 2);
    ctx.fill();

    // Primary glow halo
    const g2 = ctx.createRadialGradient(x, y, 0, x, y, halo * 0.62);
    const haloIntensity = 0.15 + 0.22 * gainIntensity + qIntensity * 0.08;
    g2.addColorStop(0, selected ? `rgba(236, 176, 255, 1)` : dynamic ? `rgba(146, 166, 255, ${0.96 + 0.04 * gainIntensity})` : `rgba(246, 249, 255, ${0.78 + haloIntensity})`);
    g2.addColorStop(0.32, selected ? "rgba(196, 120, 255, 0.66)" : dynamic ? "rgba(128, 156, 255, 0.46)" : `rgba(226, 236, 255, ${0.34 + haloIntensity * 0.5})`);
    g2.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = g2;
    ctx.beginPath();
    ctx.arc(x, y, halo * 0.62, 0, Math.PI * 2);
    ctx.fill();

    // Node core
    const nodeCore = ctx.createRadialGradient(x - 2.2, y - 3, 0, x, y, 13.5);
    nodeCore.addColorStop(0, selected ? "rgba(255, 255, 255, 0.4)" : "rgba(244, 250, 255, 0.2)");
    nodeCore.addColorStop(0.14, selected ? "rgba(104, 42, 164, 0.96)" : "rgba(32, 40, 102, 0.9)");
    nodeCore.addColorStop(0.72, "rgba(8, 10, 24, 0.96)");
    nodeCore.addColorStop(1, "rgba(2, 4, 12, 1)");
    ctx.fillStyle = nodeCore;
    ctx.beginPath();
    ctx.arc(x, y, 13.9, 0, Math.PI * 2);
    ctx.strokeStyle = "rgba(4, 6, 16, 0.94)";
    ctx.lineWidth = 3.2;
    ctx.stroke();
    ctx.strokeStyle = notch ? "#89b4ff" : selected ? "#c099ff" : dynamic ? "#7fa8ff" : "#d6e4ff";
    ctx.lineWidth = activeDrag ? 2.95 : selected ? 2.6 : hovered ? 2.35 : 2;
    ctx.beginPath();
    ctx.arc(x, y, 13, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(x, y, 11.4, 0, Math.PI * 2);
    ctx.strokeStyle = "rgba(220, 234, 255, 0.16)";
    ctx.lineWidth = 0.9;
    ctx.stroke();

    const coreDot = ctx.createRadialGradient(x - 1.2, y - 1.6, 0, x, y, 3.4);
    coreDot.addColorStop(0, selected ? "rgba(255, 255, 255, 1)" : "rgba(244, 250, 255, 0.72)");
    coreDot.addColorStop(1, "rgba(255, 255, 255, 0)");
    ctx.fillStyle = coreDot;
    ctx.beginPath();
    ctx.arc(x - 0.2, y - 0.3, 3.4, 0, Math.PI * 2);
    ctx.fill();

    if (selected) {
      // Inner breathing glass reflection
      const glassHalo = 3.8 + Math.sin(now * 0.0056) * 0.7;
      const gGlass = ctx.createRadialGradient(x - 4, y - 4, 0, x - 4, y - 4, glassHalo);
      gGlass.addColorStop(0, "rgba(255, 255, 255, 0.26)");
      gGlass.addColorStop(1, "rgba(255, 255, 255, 0)");
      ctx.fillStyle = gGlass;
      ctx.beginPath();
      ctx.arc(x - 4, y - 4, glassHalo, 0, Math.PI * 2);
      ctx.fill();

      // Breathing inner ring
      ctx.beginPath();
      ctx.arc(x, y, 6.6 + reactiveDyn * 5.8 + Math.sin(now * 0.0042) * (0.55 + 1.15 * signalMotionAmt), 0, Math.PI * 2);
      ctx.fillStyle = "rgba(220, 168, 255, 0.42)";
      ctx.fill();

      // Reactive ambient ring
      ctx.beginPath();
      ctx.arc(x, y, 15.4 + Math.sin(now * 0.003) * 1.3, 0, Math.PI * 2);
      ctx.strokeStyle = "rgba(188, 134, 255, 0.34)";
      ctx.lineWidth = 1.3;
      ctx.stroke();
    }

    // Dynamic activity indicator with animated compression halo
    if (dynamic && reactiveDyn > 0.08) {
      const compressionBreath = 0.8 + 0.2 * Math.sin(now * 0.004);
      const dynRing = 14 + reactiveDyn * 7;
      const gDyn = ctx.createRadialGradient(x, y, 13, x, y, dynRing);
      gDyn.addColorStop(0, `rgba(200, 130, 255, ${0.18 * reactiveDyn * compressionBreath})`);
      gDyn.addColorStop(0.6, `rgba(150, 110, 255, ${0.08 * reactiveDyn})`);
      gDyn.addColorStop(1, "rgba(0,0,0,0)");
      ctx.fillStyle = gDyn;
      ctx.beginPath();
      ctx.arc(x, y, dynRing, 0, Math.PI * 2);
      ctx.fill();

      // Animated compression reduction ring (visible feedback)
      if (bandDynamicGainDb && bandDynamicGainDb[i] < -0.3) {
        const reductionRing = 8 + Math.abs(bandDynamicGainDb[i]) * 3.5;
        ctx.beginPath();
        ctx.arc(x, y, reductionRing, 0, Math.PI * 2);
        ctx.strokeStyle = `rgba(255, 120, 140, ${0.3 * Math.abs(bandDynamicGainDb[i]) / 30})`;
        ctx.lineWidth = 1.5;
        ctx.stroke();
      }
    }

    // Node label
    ctx.fillStyle = "#edf3ff";
    ctx.font = "600 14px Avenir Next";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(String(i + 1), x, y + 0.5);
  });

  const orbEnergy = clamp(0.28 + reactiveDyn * 1.05 + reactivePeak * 0.82 + (state.resonanceAmount / 100) * 0.22, 0, 1);
  const orbDrift = Math.sin(now * 0.0017) * 11 + Math.cos(now * 0.0012) * 4;
  const orbShimmer = Math.sin(now * 0.00135) * 24 + Math.cos(now * 0.0019) * 9;
  const orbRot = Math.sin(now * 0.00042) * 7;
  const orbCoreX = Math.sin(now * 0.0021) * (1.2 + orbEnergy * 1.7);
  const orbCoreY = Math.cos(now * 0.0027) * (1.0 + orbEnergy * 1.5);
  const orbParallaxX = Math.sin(now * 0.0011) * (1.3 + orbEnergy * 1.9);
  const orbParallaxY = Math.cos(now * 0.00145) * (1.1 + orbEnergy * 1.5);
  orb.style.setProperty("--orb-energy", `${orbEnergy.toFixed(3)}`);
  orb.style.setProperty("--orb-drift", `${orbDrift.toFixed(3)}deg`);
  orb.style.setProperty("--orb-shimmer", `${orbShimmer.toFixed(3)}deg`);
  orb.style.setProperty("--orb-rot", `${orbRot.toFixed(3)}deg`);
  orb.style.setProperty("--orb-core-x", `${orbCoreX.toFixed(3)}px`);
  orb.style.setProperty("--orb-core-y", `${orbCoreY.toFixed(3)}px`);
  orb.style.setProperty("--orb-parallax-x", `${orbParallaxX.toFixed(3)}px`);
  orb.style.setProperty("--orb-parallax-y", `${orbParallaxY.toFixed(3)}px`);
  orb.style.boxShadow = `inset 0 -10px ${46 + reactiveDyn * 32}px rgba(0,0,0,0.66), inset 0 0 ${44 + reactiveDyn * 42}px rgba(198,118,255,0.82), inset 0 0 ${98 + reactiveDyn * 38}px rgba(72,96,228,0.58), inset 0 0 ${156 + reactiveDyn * 30}px rgba(2,8,28,0.94), 0 0 ${5 + reactivePeak * 10}px rgba(128,124,255,0.28), 0 0 ${14 + reactivePeak * 16}px rgba(92,98,250,0.2)`;

  if (pluginRoot) {
    const envGraphGlow = clamp(0.028 + reactivePeak * 0.11 + reactiveDyn * 0.08, 0, 0.17);
    const envOrbGlow = clamp(0.02 + orbEnergy * 0.1, 0, 0.16);
    const ambientBreathe = 0.5 + Math.sin(now * 0.00085) * 0.5;
    const ambientDrift = Math.sin(now * 0.00037) * 1.6;
    pluginRoot.style.setProperty("--env-graph-glow", `${envGraphGlow.toFixed(3)}`);
    pluginRoot.style.setProperty("--env-orb-glow", `${envOrbGlow.toFixed(3)}`);
    pluginRoot.style.setProperty("--ambient-breathe", `${ambientBreathe.toFixed(3)}`);
    pluginRoot.style.setProperty("--ambient-drift", `${ambientDrift.toFixed(3)}`);
  }

  if (dynMeterFill) {
    const meterValue = clamp((meterSourceExternal ? dynActivity : outputPeak) * 100, 6, 100);
    dynMeterFill.style.height = `${meterValue}%`;
  }

  if (scMiniWave) {
    const baseEnergy = clamp(0.08 + reactiveDyn * 0.75 + reactivePeak * 0.5, 0, 1);
    const waveEnergy = signalMotionAmt > 0.5 ? baseEnergy : 0.08;
    const waveSpeed = 0.65 + signalMotionAmt * 0.7;
    const waveShift = Math.sin(now * 0.0036 * waveSpeed) * (3 + waveEnergy * 5);
    scMiniWave.style.setProperty("--wave-energy", `${waveEnergy.toFixed(3)}`);
    scMiniWave.style.setProperty("--wave-shift", `${waveShift.toFixed(3)}`);
  }

  if (linkBtn) {
    const target = state.harmonicLink > 0.5 ? 1 : 0;
    harmonicLinkVisual = harmonicLinkVisual * 0.88 + target * 0.12;
    const pulse = 0.5 + 0.5 * Math.sin(now * 0.0042);
    const energy = harmonicLinkVisual * (0.16 + 0.42 * clamp(dynActivity * 0.8 + outputPeak * 0.6, 0, 1) + 0.2 * pulse);
    linkBtn.style.setProperty("--link-energy", `${energy.toFixed(3)}`);
  }

  rafHandle = requestAnimationFrame(drawGraph);
}

window.updateCurveAnalyzer = function (pre, post, reduction, peak, activity) {
  // Skip heavy array parsing while user is actively dragging to keep interaction instant.
  if (draggingBand >= 0 || knobDragging) {
    outputPeak = Number(peak) || 0;
    dynActivity = Number(activity) || 0;
    return;
  }
  if (Array.isArray(pre)) preSpectrum = Float32Array.from(pre.slice(0, BINS));
  if (Array.isArray(post)) postSpectrum = Float32Array.from(post.slice(0, BINS));
  if (Array.isArray(reduction)) reductionSpectrum = Float32Array.from(reduction.slice(0, BINS));
  outputPeak = Number(peak) || 0;
  dynActivity = Number(activity) || 0;
  lastAnalyzerUpdateMs = performance.now();
};

function showSafe(pre, post, mode) {
  return mode > 0.5 ? post : pre;
}

async function loadInitialState() {
  try {
    const response = await nativeGetState();
    if (typeof response !== "string" || response.length < 3)
      return;

    const parsed = JSON.parse(response);
    if (parsed && Array.isArray(parsed.bands))
      Object.assign(state, parsed);
  } catch (_) {}
}

async function start() {
  applyUiScale();
  await setupNativeBridge();
  populateUi();
  createKnobs();
  bindEvents();
  applySignalMotionState();
  await loadInitialState();
  syncControlsFromState();
  queuePushState();
  if (!rafHandle) rafHandle = requestAnimationFrame(drawGraph);
}

start();
