import { getNativeFunction } from "./juce/index.js";

const STYLES = ["CLEAN", "WARM", "PUNCHY", "LOUD", "OPEN"];
const LOUDNESS_MODES = ["STREAMING", "BALANCED", "LOUD"];

const state = {
  styleIndex: 0,
  intensity: 0.5,
  loudnessIndex: 0,
  buttonState: "Idle", // Idle | Analyzing | Ready | Processing | Complete
  bypassed: false,
};

const nativeSetStyle = getNativeFunction("setStyle");
const nativeSetIntensity = getNativeFunction("setIntensity");
const nativeSetLoudness = getNativeFunction("setLoudness");
const nativeBeginAnalyze = getNativeFunction("beginAnalyze");
const nativeBeginMaster = getNativeFunction("beginMaster");
const nativeTogglePreview = getNativeFunction("togglePreview");

// ── Ring button ──────────────────────────────────────────────────────────
const RING_CIRC = 2 * Math.PI * 86;
const ringProgress = document.getElementById("ring-progress");
const ringLabel = document.getElementById("ring-label");
const ringPct = document.getElementById("ring-pct");
const statusText = document.getElementById("status-text");
const masterBtn = document.getElementById("master-btn");
const previewBtn = document.getElementById("preview-btn");
const bypassBtn = document.getElementById("bypass-btn");

function renderRing() {
  ringProgress.classList.toggle("analyzing", state.buttonState === "Analyzing" || state.buttonState === "Processing");

  switch (state.buttonState) {
    case "Idle":
      ringLabel.innerHTML = "MASTER<br>AI";
      ringPct.textContent = "";
      statusText.textContent = "Play audio, then press MASTER AI to analyze.";
      setRing(0);
      masterBtn.classList.add("disabled");
      break;
    case "Analyzing":
      ringLabel.innerHTML = "ANALYZING";
      ringPct.textContent = "Listening...";
      statusText.textContent = "Capturing audio - click again to finish and analyze.";
      setRing(0.5, true);
      masterBtn.classList.add("disabled");
      break;
    case "Ready":
      ringLabel.innerHTML = "READY TO<br>MASTER";
      ringPct.textContent = "";
      statusText.textContent = "Analysis complete. Press MASTER to apply.";
      setRing(1);
      masterBtn.classList.remove("disabled");
      break;
    case "Processing":
      ringLabel.innerHTML = "MASTERING";
      ringPct.textContent = "";
      statusText.textContent = "Applying your master...";
      setRing(1, true);
      masterBtn.classList.add("disabled");
      break;
    case "Complete":
      ringLabel.innerHTML = "MASTER<br>COMPLETE";
      ringPct.textContent = "";
      statusText.textContent = "Master applied. Use PREVIEW to A/B against the original.";
      setRing(1);
      masterBtn.classList.remove("disabled");
      break;
  }
}

function setRing(fraction, glow) {
  const dash = RING_CIRC * fraction;
  ringProgress.setAttribute("stroke-dasharray", RING_CIRC.toFixed(2));
  ringProgress.setAttribute("stroke-dashoffset", (RING_CIRC - dash).toFixed(2));
}

document.getElementById("ring-btn").addEventListener("click", () => {
  if (state.buttonState === "Idle" || state.buttonState === "Ready" || state.buttonState === "Complete") {
    if (state.buttonState === "Ready" || state.buttonState === "Complete") {
      handleMaster();
    } else {
      nativeBeginAnalyze();
      state.buttonState = "Analyzing";
      renderRing();
    }
  } else if (state.buttonState === "Analyzing") {
    nativeBeginAnalyze(); // second call stops capture, starts background analysis
    renderRing();
  }
});

function handleMaster() {
  nativeBeginMaster(STYLES[state.styleIndex], state.intensity, LOUDNESS_MODES[state.loudnessIndex]);
}
masterBtn.addEventListener("click", () => {
  if (!masterBtn.classList.contains("disabled")) handleMaster();
});

previewBtn.addEventListener("click", () => {
  nativeTogglePreview().then((bypassed) => {
    state.bypassed = !!bypassed;
    previewBtn.classList.toggle("active-preview", state.bypassed);
    bypassBtn.classList.toggle("active", state.bypassed);
  });
});
bypassBtn.addEventListener("click", () => previewBtn.click());

// ── Style / Loudness dropdowns ──────────────────────────────────────────
function setupDropdown(dropdownId, menuId, valueId, options, onSelect) {
  const dropdown = document.getElementById(dropdownId);
  const menu = document.getElementById(menuId);
  const valueEl = document.getElementById(valueId);

  menu.innerHTML = "";
  options.forEach((opt, i) => {
    const el = document.createElement("div");
    el.className = "opt";
    el.textContent = opt;
    el.addEventListener("click", (e) => {
      e.stopPropagation();
      valueEl.textContent = opt;
      [...menu.children].forEach((c) => c.classList.remove("selected"));
      el.classList.add("selected");
      menu.classList.remove("open");
      onSelect(i);
    });
    menu.appendChild(el);
  });
  menu.children[0].classList.add("selected");

  dropdown.addEventListener("click", () => {
    document.querySelectorAll(".dropdown-menu").forEach((m) => { if (m !== menu) m.classList.remove("open"); });
    const rect = dropdown.getBoundingClientRect();
    menu.style.left = rect.left + "px";
    menu.style.top = rect.bottom + 4 + "px";
    menu.style.width = rect.width + "px";
    menu.classList.toggle("open");
  });
}
document.addEventListener("click", () => document.querySelectorAll(".dropdown-menu").forEach((m) => m.classList.remove("open")));

setupDropdown("style-dropdown", "style-menu", "style-val", STYLES, (i) => {
  state.styleIndex = i;
  nativeSetStyle(i);
});
setupDropdown("loudness-dropdown", "loudness-menu", "loudness-val", LOUDNESS_MODES, (i) => {
  state.loudnessIndex = i;
  nativeSetLoudness(i);
});

// ── Intensity slider ─────────────────────────────────────────────────────
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
  nativeSetIntensity(state.intensity);
}
track.addEventListener("pointerdown", (e) => { draggingIntensity = true; setIntensityFromEvent(e); });
window.addEventListener("pointermove", (e) => { if (draggingIntensity) setIntensityFromEvent(e); });
window.addEventListener("pointerup", () => { draggingIntensity = false; });

// ── Native -> JS status push (called from a C++ timer) ───────────────────
window.updateStatus = function (buttonState) {
  if (state.buttonState !== buttonState) {
    state.buttonState = buttonState;
    renderRing();
  }
};

renderRing();

// ---- stage scale-to-fit (same technique as Nova Tape/Nova Pitch) ----
(function () {
  let lastFitW = -1, lastFitH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastFitW && h === lastFitH) return;
    lastFitW = w; lastFitH = h;
    const scale = Math.min(w / 420, h / 560);
    const stage = document.getElementById("stage");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
}());
