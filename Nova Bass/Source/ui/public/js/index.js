import { getSliderState, getToggleState, getComboBoxState, getNativeFunction } from "./juce/index.js";

const nativeGetLibrary = getNativeFunction("getLibrary");
const nativeSelectPreset = getNativeFunction("selectPreset");
const nativeNextPreset = getNativeFunction("nextPreset");
const nativePreviousPreset = getNativeFunction("previousPreset");
const nativeToggleFavorite = getNativeFunction("toggleFavorite");
const nativeJsError = getNativeFunction("jsError");

window.onerror = function (msg, src, line, col, err) {
  try {
    nativeJsError(String(msg) + " @" + line + ":" + col + (err && err.stack ? "\n" + err.stack : ""));
  } catch (e) { /* nothing else to do if the native bridge itself is unavailable */ }
};

(function () {
  const KNOB_PARAMS = ["shape", "decay", "drive", "width", "glide"];
  const knobStates = {};
  for (const id of KNOB_PARAMS) knobStates[id] = getSliderState(id);
  const bassState = getSliderState("bass");
  const colorState = getComboBoxState("colorMode");
  const bypassState = getToggleState("bypass");

  let library = { presets: [], categories: ["All"], currentIndex: -1, vaultFound: false };
  let activeTab = "presets";
  let activeCategory = "All";
  let searchQuery = "";

  // ---------- Knobs (SHAPE/DECAY/DRIVE/WIDTH/GLIDE) ----------
  function updateKnobVisual(id) {
    const state = knobStates[id];
    const normal = state.getNormalisedValue();
    const knobEl = document.querySelector('.knob[data-param="' + id + '"]');
    const readoutEl = document.querySelector('[data-readout="' + id + '"]');
    if (knobEl) knobEl.style.setProperty("--turn", (-135 + normal * 270) + "deg");
    if (readoutEl) readoutEl.textContent = Math.round(normal * 100) + "%";
  }

  let activeDrag = null;

  function getClientY(event) {
    if (event.touches && event.touches.length > 0) return event.touches[0].clientY;
    return typeof event.clientY === "number" ? event.clientY : 0;
  }

  function startKnobDrag(id, event) {
    if (typeof event.button === "number" && event.button !== 0) return;
    event.preventDefault();
    const state = knobStates[id];
    state.sliderDragStarted();
    activeDrag = { id, startY: getClientY(event), startValue: state.getNormalisedValue() };
  }

  function updateDrag(event) {
    if (!activeDrag) return;
    const deltaPixels = (activeDrag.startY - getClientY(event)) / (window.__novaBassScale || 1);
    const deltaNormalised = deltaPixels / 160; // 160px drag = full range
    const newValue = Math.min(1, Math.max(0, activeDrag.startValue + deltaNormalised));
    knobStates[activeDrag.id].setNormalisedValue(newValue);
    updateKnobVisual(activeDrag.id);
  }

  function endDrag() {
    if (!activeDrag) return;
    knobStates[activeDrag.id].sliderDragEnded();
    activeDrag = null;
  }

  for (const id of KNOB_PARAMS) {
    const knobEl = document.querySelector('.knob[data-param="' + id + '"]');
    if (knobEl) knobEl.addEventListener("pointerdown", (e) => startKnobDrag(id, e));
    knobStates[id].valueChangedEvent.addListener(() => updateKnobVisual(id));
    updateKnobVisual(id);
  }
  window.addEventListener("pointermove", updateDrag);
  window.addEventListener("pointerup", endDrag);

  // ---------- BASS orb + range ----------
  const ORB_CIRCUMFERENCE = 2 * Math.PI * 88;
  const orbFillGreen = document.getElementById("orbFillGreen");
  const orbFillGold = document.getElementById("orbFillGold");
  const bassValueEl = document.getElementById("bassValue");
  const bassRangeEl = document.getElementById("bassRange");

  function updateBassVisual() {
    const normal = bassState.getNormalisedValue();
    const percent = Math.round(normal * 100);
    bassValueEl.textContent = percent;
    bassRangeEl.value = percent;

    // Two half-arcs (green rising from 0, gold rising from the far side)
    // meeting at the current value - a stylised take on the approved
    // mockup's green/gold ring around the orb, driven by the real BASS value.
    const half = ORB_CIRCUMFERENCE / 2;
    const filled = normal * ORB_CIRCUMFERENCE;
    orbFillGreen.style.strokeDasharray = half + " " + ORB_CIRCUMFERENCE;
    orbFillGreen.style.strokeDashoffset = String(half - Math.min (filled, half));
    orbFillGold.style.strokeDasharray = half + " " + ORB_CIRCUMFERENCE;
    orbFillGold.style.strokeDashoffset = String(ORB_CIRCUMFERENCE - Math.max (0, filled - half));
  }

  bassRangeEl.addEventListener("input", () => {
    bassState.setNormalisedValue(bassRangeEl.value / 100);
    updateBassVisual();
  });
  bassState.valueChangedEvent.addListener(updateBassVisual);
  updateBassVisual();

  // The orb itself is also draggable (vertical), mirroring the range slider.
  const orbCore = document.getElementById("orbCore");
  let orbDrag = null;
  orbCore.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    bassState.sliderDragStarted();
    orbDrag = { startY: getClientY(e), startValue: bassState.getNormalisedValue() };
  });
  window.addEventListener("pointermove", (e) => {
    if (!orbDrag) return;
    const deltaNormalised = (orbDrag.startY - getClientY(e)) / (window.__novaBassScale || 1) / 160;
    bassState.setNormalisedValue(Math.min(1, Math.max(0, orbDrag.startValue + deltaNormalised)));
    updateBassVisual();
  });
  window.addEventListener("pointerup", () => {
    if (orbDrag) { bassState.sliderDragEnded(); orbDrag = null; }
  });

  // ---------- Color mode (CLEAN/WARM/GRITTY) ----------
  function updateColorVisual() {
    const index = colorState.getChoiceIndex();
    document.querySelectorAll(".color-btn").forEach((btn) => {
      btn.classList.toggle("active", Number(btn.dataset.color) === index);
    });
  }
  document.querySelectorAll(".color-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      colorState.setChoiceIndex(Number(btn.dataset.color));
      updateColorVisual();
    });
  });
  colorState.valueChangedEvent.addListener(updateColorVisual);
  updateColorVisual();

  // ---------- Bypass ----------
  const bypassBtn = document.getElementById("bypassBtn");
  function updateBypassVisual() {
    const isBypassed = bypassState.getValue();
    bypassBtn.classList.toggle("active", isBypassed);
    document.getElementById("powerDot").style.opacity = isBypassed ? 0.25 : 1;
  }
  bypassBtn.addEventListener("click", () => {
    bypassState.setValue(!bypassState.getValue());
    updateBypassVisual();
  });
  bypassState.valueChangedEvent.addListener(updateBypassVisual);
  updateBypassVisual();

  // ---------- Preset browser ----------
  const categoryListEl = document.getElementById("categoryList");
  const presetListEl = document.getElementById("presetList");
  const searchBoxEl = document.getElementById("searchBox");

  function currentPreset() {
    if (library.currentIndex < 0) return null;
    return library.presets.find((p) => p.index === library.currentIndex) || null;
  }

  function renderCategories() {
    categoryListEl.innerHTML = "";
    for (const cat of library.categories) {
      const count = cat === "All" ? library.presets.length : library.presets.filter((p) => p.category === cat).length;
      const row = document.createElement("div");
      row.className = "cat-row" + (cat === activeCategory ? " selected" : "");
      row.innerHTML = '<span>' + cat + '</span><span class="cat-count">' + count + '</span>';
      row.addEventListener("click", () => { activeCategory = cat; renderCategories(); renderPresets(); });
      categoryListEl.appendChild(row);
    }
  }

  function renderPresets() {
    presetListEl.innerHTML = "";
    const q = searchQuery.toLowerCase();
    const filtered = library.presets.filter((p) => {
      if (activeTab === "favorites" && !p.favorite) return false;
      if (activeCategory !== "All" && p.category !== activeCategory) return false;
      if (q && !p.name.toLowerCase().includes(q)) return false;
      return true;
    });

    for (const p of filtered) {
      const row = document.createElement("div");
      row.className = "preset-row" + (p.index === library.currentIndex ? " selected" : "");
      const star = document.createElement("span");
      star.className = "star" + (p.favorite ? " favorite" : "");
      star.textContent = p.favorite ? "★" : "☆";
      star.addEventListener("click", async (e) => {
        e.stopPropagation();
        library = await nativeToggleFavorite(p.index);
        renderAll();
      });
      const nameSpan = document.createElement("span");
      nameSpan.textContent = p.name;
      row.appendChild(nameSpan);
      row.appendChild(star);
      row.addEventListener("click", async () => { library = await nativeSelectPreset(p.index); renderAll(); });
      presetListEl.appendChild(row);
    }
  }

  function renderHero() {
    const preset = currentPreset();
    document.getElementById("presetName").textContent = preset ? preset.name : "--";
    document.getElementById("description").textContent = preset ? preset.description : "";
    const heart = document.getElementById("favHeart");
    const isFav = !!(preset && preset.favorite);
    heart.classList.toggle("favorite", isFav);
    heart.innerHTML = isFav ? "&#9829;" : "&#9825;";
  }

  function renderAll() {
    renderCategories();
    renderPresets();
    renderHero();
  }

  document.querySelectorAll(".tab").forEach((tab) => {
    tab.addEventListener("click", () => {
      activeTab = tab.dataset.tab;
      document.querySelectorAll(".tab").forEach((t) => t.classList.toggle("active", t === tab));
      renderPresets();
    });
  });

  searchBoxEl.addEventListener("input", () => { searchQuery = searchBoxEl.value; renderPresets(); });

  document.getElementById("prevBtn").addEventListener("click", async () => { library = await nativePreviousPreset(); renderAll(); });
  document.getElementById("nextBtn").addEventListener("click", async () => { library = await nativeNextPreset(); renderAll(); });
  document.getElementById("favHeart").addEventListener("click", async () => {
    if (library.currentIndex < 0) return;
    library = await nativeToggleFavorite(library.currentIndex);
    renderAll();
  });

  (async function init() {
    try {
      library = await nativeGetLibrary();
      renderAll();
    } catch (e) { /* native bridge not ready yet in a plain browser preview */ }
  })();

  // ---------- Status push from C++ (voice activity indicator) ----------
  window.updateStatus = function (status) {
    const dot = document.getElementById("powerDot");
    if (!bypassState.getValue())
      dot.style.boxShadow = status.anyVoiceActive ? "0 0 14px #7fe8b0" : "0 0 8px #7fe8b0";
  };
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
    const scale = Math.min(w / 1150, h / 720);
    window.__novaBassScale = scale || 1;
    const stage = document.getElementById("app");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();
