import { getNativeFunction } from "./juce/index.js";

const STYLES = ["CLEAN", "WARM", "PUNCHY", "LOUD", "OPEN"];
const LOUDNESS_MODES = ["STREAMING", "BALANCED", "LOUD"];

const state = {
  styleIndex: 0,
  intensity: 0.5,
  loudnessIndex: 0,
  hasImport: false,
  hasMaster: false,
  busy: false,
  isPlaying: false,
  mode: "idle", // idle | analyzing | ready | complete (standalone: MASTER does analyze+decide+process in one job)
};

const nativeImportMix = getNativeFunction("importMix");
const nativeBeginMaster = getNativeFunction("beginMaster");
const nativePreviewOriginal = getNativeFunction("previewOriginal");
const nativePreviewMaster = getNativeFunction("previewMaster");
const nativeStopPreview = getNativeFunction("stopPreview");
const nativeExportMix = getNativeFunction("exportMix");
const nativeGetWaveforms = getNativeFunction("getWaveforms");

const ringProgress = document.getElementById("ring-progress");
const ringLabel = document.getElementById("ring-label");
const statusText = document.getElementById("status-text");
const masterBtn = document.getElementById("master-btn");
const previewBtn = document.getElementById("preview-btn");
const trackName = document.getElementById("track-name");
const trackMeta = document.getElementById("track-meta");

const RING_CIRC = 2 * Math.PI * 86;
function setRing(fraction) {
  ringProgress.setAttribute("stroke-dasharray", RING_CIRC.toFixed(2));
  ringProgress.setAttribute("stroke-dashoffset", (RING_CIRC - RING_CIRC * fraction).toFixed(2));
}

function render() {
  masterBtn.classList.toggle("disabled", state.busy || !state.hasImport);
  previewBtn.classList.toggle("disabled", state.busy || !state.hasImport);

  if (state.busy) {
    ringLabel.innerHTML = state.hasMaster ? "MASTERING" : "ANALYZING";
    ringProgress.classList.add("analyzing");
    setRing(0.5);
    statusText.textContent = "Nova is listening and analyzing your track...";
  } else if (!state.hasImport) {
    ringLabel.innerHTML = "MASTER<br>AI";
    ringProgress.classList.remove("analyzing");
    setRing(0);
    statusText.textContent = "Import a stereo mix to begin.";
  } else if (!state.hasMaster) {
    ringLabel.innerHTML = "MASTER<br>AI";
    ringProgress.classList.remove("analyzing");
    setRing(0);
    statusText.textContent = "Ready. Press MASTER for a one-click master.";
  } else {
    ringLabel.innerHTML = "MASTER<br>COMPLETE";
    ringProgress.classList.remove("analyzing");
    setRing(1);
    statusText.textContent = "Master complete. Preview and export when ready.";
  }
}

// ── Import ─────────────────────────────────────────────────────────────
document.getElementById("import-btn").addEventListener("click", () => {
  nativeImportMix();
});

// ── Master ────────────────────────────────────────────────────────────
function handleMaster() {
  if (masterBtn.classList.contains("disabled")) return;
  nativeBeginMaster(STYLES[state.styleIndex], state.intensity, LOUDNESS_MODES[state.loudnessIndex]);
}
masterBtn.addEventListener("click", handleMaster);
document.getElementById("ring-btn").addEventListener("click", handleMaster);

// ── Preview ───────────────────────────────────────────────────────────
let previewMode = null; // 'original' | 'master' | null
previewBtn.addEventListener("click", () => {
  if (previewBtn.classList.contains("disabled")) return;
  if (previewMode) {
    nativeStopPreview();
    previewMode = null;
    previewBtn.classList.remove("active-preview");
    previewBtn.textContent = "PREVIEW";
  } else if (state.hasMaster) {
    nativePreviewMaster();
    previewMode = "master";
    previewBtn.classList.add("active-preview");
    previewBtn.textContent = "STOP (MASTER)";
  } else {
    nativePreviewOriginal();
    previewMode = "original";
    previewBtn.classList.add("active-preview");
    previewBtn.textContent = "STOP (ORIGINAL)";
  }
});

// ── Style / Loudness pills ────────────────────────────────────────────
function buildPills(containerId, options, selectedIndex, onSelect) {
  const container = document.getElementById(containerId);
  container.innerHTML = "";
  options.forEach((opt, i) => {
    const el = document.createElement("div");
    el.className = "pill" + (i === selectedIndex ? " selected" : "");
    el.textContent = opt;
    el.addEventListener("click", () => {
      [...container.children].forEach((c) => c.classList.remove("selected"));
      el.classList.add("selected");
      onSelect(i);
    });
    container.appendChild(el);
  });
}
buildPills("style-pills", STYLES, state.styleIndex, (i) => { state.styleIndex = i; });
buildPills("loudness-pills", LOUDNESS_MODES, state.loudnessIndex, (i) => { state.loudnessIndex = i; });

// ── Intensity slider ──────────────────────────────────────────────────
const track = document.getElementById("intensity-track");
const fill = document.getElementById("intensity-fill");
const thumb = document.getElementById("intensity-thumb");
function renderIntensity() {
  const pct = (state.intensity * 100).toFixed(1) + "%";
  fill.style.width = pct;
  thumb.style.left = pct;
}
renderIntensity();
let draggingIntensity = false;
function setIntensityFromEvent(e) {
  const rect = track.getBoundingClientRect();
  const x = (e.touches ? e.touches[0].clientX : e.clientX) - rect.left;
  state.intensity = Math.max(0, Math.min(1, x / rect.width));
  renderIntensity();
}
track.addEventListener("pointerdown", (e) => { draggingIntensity = true; setIntensityFromEvent(e); });
window.addEventListener("pointermove", (e) => { if (draggingIntensity) setIntensityFromEvent(e); });
window.addEventListener("pointerup", () => { draggingIntensity = false; });

// ── Export (the EXPORT FORMATS cards double as the export trigger - the
// mockup has no separate explicit export button, matching walkthrough
// step "4. PREVIEW & EXPORT") ────────────────────────────────────────
const exportFormats = { 0: "wav24", 1: "wav16", 3: "aiff24" }; // index 2 (MP3) intentionally excluded - not implemented for v0.1
document.querySelectorAll(".ef-item").forEach((el, i) => {
  if (!(i in exportFormats)) return; // MP3 stays visually present but non-interactive
  el.style.cursor = "pointer";
  el.addEventListener("click", () => {
    if (!state.hasMaster || state.busy) return;
    statusText.textContent = "Exporting...";
    nativeExportMix(exportFormats[i]).then((ok) => {
      statusText.textContent = ok ? "Export complete." : "Export failed or cancelled.";
      setTimeout(render, 2000);
    });
  });
});

// ── Waveforms (real peak data from the native side) ──────────────────
function drawWaveform(canvasId, peaks, color) {
  const canvas = document.getElementById(canvasId);
  const ctx = canvas.getContext("2d");
  const w = canvas.width, h = canvas.height, mid = h / 2;
  ctx.clearRect(0, 0, w, h);
  if (!peaks || peaks.length === 0) return;
  const barWidth = w / peaks.length;
  ctx.fillStyle = color;
  for (let i = 0; i < peaks.length; i++) {
    const barH = Math.max(1, peaks[i] * (h - 6));
    ctx.fillRect(i * barWidth, mid - barH / 2, Math.max(1, barWidth - 1), barH);
  }
}

let waveformsFetched = false;
function refreshWaveformsIfNeeded() {
  if (!state.hasImport) { waveformsFetched = false; return; }
  if (waveformsFetched && !state.busy) return;
  nativeGetWaveforms(120).then((data) => {
    if (!data) return;
    drawWaveform("wave-original", data.original, "rgba(122,122,134,0.85)");
    drawWaveform("wave-mastered", data.mastered, "#d4a843");
    waveformsFetched = true;
  });
}

// ── Native -> JS status push ──────────────────────────────────────────
window.updateStatus = function (status) {
  state.hasImport = !!status.hasImport;
  state.hasMaster = !!status.hasMaster;
  state.busy = !!status.busy;
  state.isPlaying = !!status.isPlaying;

  if (status.hasImport) {
    trackName.textContent = status.fileName || "Imported mix";
    const khz = (status.sampleRate / 1000).toFixed(1);
    trackMeta.textContent = `${khz} kHz | ${status.bitDepth}-bit | ${status.stereo ? "Stereo" : "Mono"} | ${status.lengthSeconds.toFixed(1)}s`;
  } else {
    trackName.textContent = "No file imported";
    trackMeta.textContent = "Import a stereo mix to begin";
  }

  if (!state.isPlaying && previewMode) {
    previewMode = null;
    previewBtn.classList.remove("active-preview");
    previewBtn.textContent = "PREVIEW";
  }

  render();
  refreshWaveformsIfNeeded();
};

render();
