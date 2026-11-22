import { getNativeFunction } from "./juce/index.js";

const nativeSetParam = getNativeFunction("setParam");
const nativeNoteOn = getNativeFunction("noteOn");
const nativeNoteOff = getNativeFunction("noteOff");
const nativeJsError = getNativeFunction("jsError");
const nativeGetLibrary = getNativeFunction("getLibrary");
const nativeSelectPreset = getNativeFunction("selectPreset");
const nativeNextPreset = getNativeFunction("nextPreset");
const nativePreviousPreset = getNativeFunction("previousPreset");
const nativeToggleFavorite = getNativeFunction("toggleFavorite");
const nativeLocateLibrary = getNativeFunction("locateLibrary");

// Current .app CSS transform:scale() factor (kept in sync by fitStage()
// at the bottom of this file). Pointer events report clientX/clientY in
// real screen pixels, unaffected by a CSS transform on an ancestor, so
// drag handlers must divide by this to stay correctly calibrated once the
// window is no longer 1:1 with the design canvas.
let currentStageScale = 1;

window.onerror = function (msg, src, line, col, err) {
  try {
    nativeJsError(String(msg) + " @" + line + ":" + col + (err && err.stack ? "\n" + err.stack : ""));
  } catch (e) { /* ignore */ }
};

// ── Rotary knob component (real for params wired below, cosmetic for FX
// controls not yet backed by engine DSP - see task list) ─────────────────
function createKnob (opts) {
  const { size = 46, value = 0.5, paramId = null, onChange = null, arcColor = "var(--purple-bright)" } = opts;
  const el = document.createElement("div");
  el.className = "knob";
  el.style.width = size + "px";
  el.style.height = size + "px";

  const r = size / 2 - 4;
  const circumference = 2 * Math.PI * r;
  const sweep = 0.75; // 270 degrees of the circle

  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("viewBox", `0 0 ${size} ${size}`);
  const track = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  track.setAttribute("class", "arc-track");
  track.setAttribute("cx", size / 2); track.setAttribute("cy", size / 2); track.setAttribute("r", r);
  track.setAttribute("stroke-width", 3);
  track.setAttribute("stroke-dasharray", `${circumference * sweep} ${circumference}`);
  const fill = document.createElementNS("http://www.w3.org/2000/svg", "circle");
  fill.setAttribute("class", "arc-fill");
  fill.setAttribute("cx", size / 2); fill.setAttribute("cy", size / 2); fill.setAttribute("r", r);
  fill.setAttribute("stroke-width", 3);
  fill.style.stroke = arcColor;
  svg.appendChild(track); svg.appendChild(fill);
  el.appendChild(svg);

  let current = value;
  function render() {
    const len = circumference * sweep * Math.max(0, Math.min(1, current));
    fill.setAttribute("stroke-dasharray", `${len} ${circumference}`);
  }
  render();

  let dragging = false, startY = 0, startVal = 0.5;
  el.addEventListener("pointerdown", (e) => {
    dragging = true; startY = e.clientY; startVal = current;
    el.setPointerCapture(e.pointerId);
  });
  el.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const dy = (startY - e.clientY) / (currentStageScale || 1);
    current = Math.max(0, Math.min(1, startVal + dy / 140));
    render();
    if (paramId) nativeSetParam(paramId, current);
    if (onChange) onChange(current);
  });
  el.addEventListener("pointerup", () => { dragging = false; });

  el.setValue = (v) => { current = v; render(); };
  return el;
}

// ── Header + output master knob (mirrored: both bound to "master") ──────
const masterKnobs = [];
function bindMasterKnobs() {
  document.getElementById("master-knob-hdr").replaceWith(
    (() => { const k = createKnob({ size: 30, value: 0.7, paramId: "master" }); k.id = "master-knob-hdr"; masterKnobs.push(k); return k; })()
  );
  document.getElementById("output-knob").replaceWith(
    (() => { const k = createKnob({ size: 44, value: 0.7, paramId: "master", arcColor: "var(--gold-bright)" }); k.id = "output-knob"; masterKnobs.push(k); return k; })()
  );
}
bindMasterKnobs();

// small vertical meter bar builder
function buildMeter (container, count, litColor) {
  container.innerHTML = "";
  for (let i = 0; i < count; i++) {
    const bar = document.createElement("i");
    bar.style.height = (3 + Math.random() * (container.classList.contains("output-meter") ? 40 : 10)) + "px";
    if (Math.random() > 0.55) bar.style.background = litColor;
    container.appendChild(bar);
  }
}
buildMeter(document.getElementById("hdr-meter"), 5, "var(--purple-bright)");
buildMeter(document.getElementById("output-meter"), 8, "var(--gold-bright)");

// ── Macros (TONE / MOTION / SPACE / CHARACTER) - real params, and their
// knob visuals are re-synced whenever a preset switch changes them
// programmatically (see applyPresetToMacroKnobs below) ──────────────────
const MACROS = [
  { id: "macroTone", key: "tone", label: "TONE", lo: "DARK", hi: "BRIGHT" },
  { id: "macroMotion", key: "motion", label: "MOTION", lo: "STILL", hi: "MOVING" },
  { id: "macroSpace", key: "space", label: "SPACE", lo: "DRY", hi: "HUGE" },
  { id: "macroCharacter", key: "character", label: "CHARACTER", lo: "CLEAN", hi: "RICH" },
];
const macroKnobs = {};
{
  const macrosRow = document.getElementById("macros-row");
  MACROS.forEach((m) => {
    const wrap = document.createElement("div");
    wrap.className = "macro";
    const label = document.createElement("div");
    label.className = "macro-label";
    label.textContent = m.label;
    const knob = createKnob({ size: 58, value: 0.5, paramId: m.id });
    macroKnobs[m.key] = knob;
    const range = document.createElement("div");
    range.className = "macro-range";
    range.innerHTML = `<span>${m.lo}</span><span>${m.hi}</span>`;
    range.style.width = "72px";
    wrap.appendChild(label); wrap.appendChild(knob); wrap.appendChild(range);
    macrosRow.appendChild(wrap);
  });
}
function applyPresetToMacroKnobs (macros) {
  if (!macros) return;
  MACROS.forEach((m) => { if (macros[m.key] !== undefined) macroKnobs[m.key].setValue(macros[m.key]); });
}

// ── FX modules - real values from the selected preset's NovaEffectsParams
// (task #94 wired the DSP; this renders its current parameter values) ────
function renderFx (fx) {
  const fxGrid = document.getElementById("fx-grid");
  fxGrid.innerHTML = "";
  if (!fx) return;

  const modules = [
    { name: "CHORUS", label: "MIX", value: (fx.chorusMix * 100).toFixed(0) + "%", knobValue: fx.chorusMix },
    { name: "DELAY", label: "TIME", value: Math.round(fx.delayTimeMs) + " ms", knobValue: fx.delayMix },
    { name: "REVERB", label: "SIZE", value: (fx.reverbRoomSize * 100).toFixed(0) + "%", knobValue: fx.reverbMix },
    { name: "COMP", label: "RATIO", value: fx.compRatio.toFixed(1) + ":1", knobValue: Math.min(1, fx.compRatio / 10) },
  ];
  modules.forEach((m) => {
    const mod = document.createElement("div");
    mod.className = "fx-module";
    const knob = createKnob({ size: 30, value: m.knobValue });
    mod.innerHTML = `<div class="fx-name"><span class="fx-dot"></span>${m.name}</div>`;
    mod.appendChild(knob);
    const label = document.createElement("div"); label.className = "fx-label"; label.textContent = m.label;
    const val = document.createElement("div"); val.className = "fx-value"; val.textContent = m.value;
    mod.appendChild(label); mod.appendChild(val);
    fxGrid.appendChild(mod);
  });
  // EQ module: mini curve instead of a knob
  const eqMod = document.createElement("div");
  eqMod.className = "fx-module";
  eqMod.innerHTML = `<div class="fx-name"><span class="fx-dot"></span>EQ</div>
    <svg class="fx-eq-curve" viewBox="0 0 60 22"><path d="M0,16 C12,18 18,4 30,6 C42,8 48,18 60,14" stroke="var(--purple-bright)" stroke-width="1.5" fill="none"/></svg>
    <div class="fx-label" style="display:flex; gap:4px;"><span>LOW</span><span>MID</span><span>HIGH</span></div>`;
  fxGrid.appendChild(eqMod);
}

// ── Preset browser + Sound Vault (real data via getLibrary/selectPreset/
// nextPreset/previousPreset/toggleFavorite/locateLibrary) ────────────────
let library = { vaultFound: false, currentIndex: -1, presets: [], categories: ["All"] };
let selectedCategory = "All";
let selectedPreset = null;

function updateVaultOverlay() {
  document.getElementById("vault-missing-overlay").classList.toggle("shown", !library.vaultFound);
}

function renderCategories() {
  const list = document.getElementById("category-list");
  list.innerHTML = "";
  library.categories.forEach((c) => {
    const count = c === "All" ? library.presets.length : library.presets.filter((p) => p.category === c).length;
    const row = document.createElement("div");
    row.className = "cat-row" + (c === selectedCategory ? " selected" : "");
    row.innerHTML = `<span>${c}</span><span class="n">${count}</span>`;
    row.addEventListener("click", () => { selectedCategory = c; renderCategories(); renderPresets(); });
    list.appendChild(row);
  });
}

function presetRow (p) {
  const row = document.createElement("div");
  row.className = "preset-row" + (selectedPreset && p.index === selectedPreset.index ? " selected" : "");
  row.innerHTML = `<span>${p.name}</span><span class="star${p.favorite ? " fav" : ""}">&#9733;</span>`;
  row.addEventListener("click", (e) => {
    if (e.target.classList.contains("star")) {
      nativeToggleFavorite(p.index).then((fav) => { p.favorite = fav; renderPresets(); });
      return;
    }
    nativeSelectPreset(p.index).then(onPresetLoaded);
  });
  return row;
}

function renderPresets() {
  const list = document.getElementById("preset-list");
  list.innerHTML = "";
  const filtered = selectedCategory === "All" ? library.presets : library.presets.filter((p) => p.category === selectedCategory);
  filtered.forEach((p) => list.appendChild(presetRow(p)));
}

function iconForLayer (l) {
  const n = (l.name || "").toLowerCase();
  if (n.includes("piano") || n.includes("grand")) return "\u{1F3B9}";
  if (n.includes("bell") || n.includes("mallet") || n.includes("glock")) return "\u{1F514}";
  if (n.includes("pad") || n.includes("warm") || n.includes("air") || n.includes("texture")) return "\u{2601}\u{FE0F}";
  if (n.includes("string") || n.includes("violin")) return "\u{1F3BB}";
  if (n.includes("brass") || n.includes("trumpet")) return "\u{1F3BA}";
  if (n.includes("bass")) return "\u{1F3B8}";
  if (n.includes("drum") || n.includes("snare") || n.includes("perc")) return "\u{1F941}";
  if (n.includes("lead")) return "\u{26A1}";
  return l.type === "synth" ? "\u{26A1}" : "\u{1F3B5}";
}

function noteName (midi) {
  const NAMES = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
  const octave = Math.floor(midi / 12) - 1;
  return NAMES[midi % 12] + octave;
}

function renderLayers (preset) {
  const layerList = document.getElementById("layer-list");
  layerList.innerHTML = "";
  (preset ? preset.layers : []).forEach((l) => {
    const strip = document.createElement("div");
    strip.className = "layer-strip";
    if (!l.used) {
      strip.style.opacity = "0.4";
      strip.innerHTML = `
        <div class="layer-letter">${l.letter}</div>
        <div class="layer-thumb">&#8212;</div>
        <div class="layer-info"><div class="layer-name">Empty</div><div class="layer-meta">Not assigned</div></div>`;
      layerList.appendChild(strip);
      return;
    }
    strip.innerHTML = `
      <div class="layer-letter">${l.letter}</div>
      <div class="layer-thumb">${iconForLayer(l)}</div>
      <div class="layer-info"><div class="layer-name">${l.name || "Untitled"}</div><div class="layer-meta">${noteName(l.lowKey)}–${noteName(l.highKey)} • Vel ${l.lowVelocity}–${l.highVelocity}</div></div>
      <div class="layer-btn">S</div>
      <div class="layer-btn${l.on ? " on" : ""}">M</div>`;
    const knob = createKnob({ size: 22, value: Math.max(0, Math.min(1, (l.volumeDb + 60) / 66)) });
    knob.classList.add("layer-vol");
    strip.appendChild(knob);
    const meter = document.createElement("div");
    meter.className = "layer-meter";
    buildMeter(meter, 4, "var(--purple-bright)");
    strip.appendChild(meter);
    layerList.appendChild(strip);
  });
}

// Categories with a real generated hero image (see CMakeLists.txt /
// PluginEditor.cpp getResource()) - any artworkRef outside this set falls
// back to the plain gradient background already in the CSS.
const HERO_IMAGE_CATEGORIES = new Set([
  "pianos", "strings", "brass", "woodwinds", "mallets", "percussion",
  "choirs", "electric_pianos", "organs", "guitars", "bass", "synth_bass",
  "leads", "pads", "textures", "plucks", "arps", "cinematic", "world",
  "nova_signature",
]);

function renderHero (preset) {
  if (!preset) return;
  document.getElementById("hero-name").textContent = preset.name.toUpperCase();
  document.getElementById("hero-fav").style.color = preset.favorite ? "var(--gold-bright)" : "var(--text-dimmer)";
  document.getElementById("hero-tags").textContent = (preset.tags && preset.tags.length ? preset.tags : [preset.category]).join(" • ").toUpperCase();
  document.getElementById("hero-desc").textContent = preset.description || "";

  const heroArt = document.getElementById("hero-art");
  if (preset.artworkRef && HERO_IMAGE_CATEGORIES.has(preset.artworkRef)) {
    heroArt.style.backgroundImage = `url('images/hero-${preset.artworkRef}.jpg')`;
  } else {
    heroArt.style.backgroundImage = "";
  }
}

function onPresetLoaded (preset) {
  if (!preset) return;
  selectedPreset = preset;
  const idx = library.presets.findIndex((p) => p.index === preset.index);
  if (idx >= 0) library.presets[idx] = preset;
  renderPresets();
  renderHero(preset);
  renderLayers(preset);
  renderFx(preset.fx);
  applyPresetToMacroKnobs(preset.macros);
}

document.getElementById("hero-prev").addEventListener("click", () => nativePreviousPreset().then(onPresetLoaded));
document.getElementById("hero-next").addEventListener("click", () => nativeNextPreset().then(onPresetLoaded));
document.getElementById("hero-fav").addEventListener("click", () => {
  if (!selectedPreset) return;
  nativeToggleFavorite(selectedPreset.index).then((fav) => {
    selectedPreset.favorite = fav;
    const idx = library.presets.findIndex((p) => p.index === selectedPreset.index);
    if (idx >= 0) library.presets[idx].favorite = fav;
    renderHero(selectedPreset);
    renderPresets();
  });
});

document.getElementById("preset-search").addEventListener("input", (e) => {
  const q = e.target.value.toLowerCase();
  const list = document.getElementById("preset-list");
  list.innerHTML = "";
  library.presets.filter((p) => p.name.toLowerCase().includes(q)).forEach((p) => list.appendChild(presetRow(p)));
});

document.querySelectorAll(".browser-tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".browser-tab").forEach((t) => t.classList.remove("active"));
    tab.classList.add("active");
    if (tab.dataset.tab === "favorites") {
      const list = document.getElementById("preset-list");
      list.innerHTML = "";
      library.presets.filter((p) => p.favorite).forEach((p) => list.appendChild(presetRow(p)));
    } else if (tab.dataset.tab === "browser") {
      renderPresets();
    }
  });
});

document.getElementById("vault-locate-btn").addEventListener("click", () => {
  nativeLocateLibrary().then((lib) => {
    library = lib;
    selectedCategory = "All";
    selectedPreset = library.presets.length ? library.presets[Math.max(0, library.currentIndex)] : null;
    updateVaultOverlay();
    renderCategories();
    renderPresets();
    renderHero(selectedPreset);
    renderLayers(selectedPreset);
    if (selectedPreset) { renderFx(selectedPreset.fx); applyPresetToMacroKnobs(selectedPreset.macros); }
  });
});

nativeGetLibrary().then((lib) => {
  library = lib;
  updateVaultOverlay();
  selectedCategory = "All";
  selectedPreset = library.presets.find((p) => p.index === library.currentIndex) || library.presets[0] || null;
  renderCategories();
  renderPresets();
  renderHero(selectedPreset);
  renderLayers(selectedPreset);
  if (selectedPreset) { renderFx(selectedPreset.fx); applyPresetToMacroKnobs(selectedPreset.macros); }
});

// ── Performance strip: pitch/mod wheels ──────────────────────────────────
function setupWheel (trackId, thumbId, { springBack }) {
  const track = document.getElementById(trackId);
  const thumb = document.getElementById(thumbId);
  const trackH = 78, thumbH = 22;
  let value = springBack ? 0.5 : 0.5; // 0..1, 0.5 = center/default
  function place() { thumb.style.top = ((1 - value) * (trackH - thumbH)) + "px"; }
  place();
  let dragging = false;
  function setFromY (clientY) {
    const rect = track.getBoundingClientRect();
    const y = clientY - rect.top;
    value = Math.max(0, Math.min(1, 1 - (y - thumbH / 2) / (trackH - thumbH)));
    place();
  }
  track.addEventListener("pointerdown", (e) => { dragging = true; setFromY(e.clientY); track.setPointerCapture(e.pointerId); });
  track.addEventListener("pointermove", (e) => { if (dragging) setFromY(e.clientY); });
  track.addEventListener("pointerup", () => {
    dragging = false;
    if (springBack) { value = 0.5; place(); }
  });
}
setupWheel("pitch-wheel", "pitch-thumb", { springBack: true });
setupWheel("mod-wheel", "mod-thumb", { springBack: false });

// ── Key range mini visualization (static full-range placeholder until
// per-layer key ranges are shown per selected layer tab) ─────────────────
{
  const mini = document.getElementById("mini-keys");
  for (let i = 0; i < 42; i++) {
    const seg = document.createElement("i");
    seg.className = "in-range";
    mini.appendChild(seg);
  }
}

// ── Performance center tabs (MAIN implemented with real params; other
// tabs are architecture-only for v0.1 per spec sections 24-27) ───────────
function renderMainTab() {
  const row = document.getElementById("perf-main-content");
  row.innerHTML = "";
  const ctl = (label, value, knobOpts) => {
    const wrap = document.createElement("div");
    wrap.className = "perf-ctl";
    const lab = document.createElement("div"); lab.className = "perf-ctl-label"; lab.textContent = label;
    const knob = createKnob({ size: 34, ...knobOpts });
    const val = document.createElement("div"); val.className = "perf-ctl-value"; val.textContent = value;
    wrap.appendChild(lab); wrap.appendChild(knob); wrap.appendChild(val);
    return wrap;
  };
  row.appendChild(ctl("PORTA", "80 ms", { value: 0.16, paramId: "portaTimeMs" }));
  row.appendChild(ctl("UNISON", "4", { value: 0.43, paramId: "unisonVoices" }));
  row.appendChild(ctl("GLIDE", "32%", { value: 0.32, paramId: "glideAmount" }));

  const vmWrap = document.createElement("div");
  vmWrap.className = "perf-ctl";
  vmWrap.innerHTML = `<div class="perf-ctl-label">VOICE MODE</div>`;
  const select = document.createElement("select");
  select.className = "voice-mode-select";
  ["Poly", "Mono"].forEach((v, i) => { const o = document.createElement("option"); o.value = i; o.textContent = v; select.appendChild(o); });
  select.addEventListener("change", () => nativeSetParam("voiceMode", parseInt(select.value, 10)));
  vmWrap.appendChild(select);
  row.appendChild(vmWrap);

  row.appendChild(ctl("RANDOM", "0%", { value: 0.0, paramId: "randomAmount" }));
}
function renderPlaceholderTab (title) {
  const row = document.getElementById("perf-main-content");
  row.innerHTML = `<div style="color:var(--text-dimmer); font-size:10px; letter-spacing:0.08em;">${title} — architecture reserved for v0.1, not yet built.</div>`;
}
renderMainTab();
document.querySelectorAll(".perf-tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".perf-tab").forEach((t) => t.classList.remove("active"));
    tab.classList.add("active");
    const which = tab.dataset.ptab;
    if (which === "main") renderMainTab();
    else renderPlaceholderTab(tab.textContent);
  });
});

// ── Virtual keyboard ──────────────────────────────────────────────────
const WHITE_OFFSETS = [0, 2, 4, 5, 7, 9, 11];
const BLACK_AFTER = { 0: true, 2: true, 5: true, 7: true, 9: true }; // black key after this white-key offset
const NOTE_LOW = 24, NOTE_HIGH = 107; // C1..B7 range
const keyEls = new Map();

function buildKeyboard() {
  const kb = document.getElementById("keyboard");
  kb.innerHTML = "";
  const whiteNotes = [];
  for (let n = NOTE_LOW; n <= NOTE_HIGH; n++) {
    const offset = n % 12;
    if (WHITE_OFFSETS.includes(offset)) whiteNotes.push(n);
  }
  whiteNotes.forEach((note) => {
    const key = document.createElement("div");
    key.className = "wkey";
    key.dataset.note = note;
    if (note % 12 === 0) {
      const oct = Math.floor(note / 12) - 1;
      key.innerHTML = `<span class="oct-label">C${oct}</span>`;
    }
    wireKey(key, note);
    kb.appendChild(key);
    keyEls.set(note, key);
  });
  // black keys, positioned relative to the white key row
  const whiteWidthPct = 100 / whiteNotes.length;
  let whiteIndex = 0;
  for (let n = NOTE_LOW; n <= NOTE_HIGH; n++) {
    const offset = n % 12;
    if (WHITE_OFFSETS.includes(offset)) { whiteIndex++; continue; }
  }
  whiteIndex = 0;
  for (let n = NOTE_LOW; n < NOTE_HIGH; n++) {
    const offset = n % 12;
    if (WHITE_OFFSETS.includes(offset)) {
      if (BLACK_AFTER[offset] && n + 1 <= NOTE_HIGH) {
        const bKey = document.createElement("div");
        bKey.className = "bkey";
        bKey.dataset.note = n + 1;
        bKey.style.left = `calc(${(whiteIndex + 1) * whiteWidthPct}% - ${whiteWidthPct * 0.31}%)`;
        bKey.style.width = (whiteWidthPct * 0.62) + "%";
        wireKey(bKey, n + 1);
        kb.appendChild(bKey);
        keyEls.set(n + 1, bKey);
      }
      whiteIndex++;
    }
  }
}

function wireKey (el, note) {
  const press = (e) => { e.preventDefault(); nativeNoteOn(note); el.classList.add("active"); };
  const release = () => { nativeNoteOff(note); el.classList.remove("active"); };
  el.addEventListener("pointerdown", press);
  el.addEventListener("pointerup", release);
  el.addEventListener("pointerleave", (e) => { if (e.buttons > 0) release(); });
}
buildKeyboard();

// ── Native -> JS status push (real MIDI/held-note state + real params) ──
let lastHeldSet = new Set();
window.updateStatus = function (status) {
  document.getElementById("tune-val").textContent = Number(status.tune).toFixed(2);

  if (status.midiActivity) {
    const dot = document.getElementById("midi-dot");
    dot.classList.add("active");
    setTimeout(() => dot.classList.remove("active"), 120);
  }

  const held = new Set(status.heldNotes || []);
  for (const note of held) if (!lastHeldSet.has(note)) keyEls.get(note)?.classList.add("active");
  for (const note of lastHeldSet) if (!held.has(note)) keyEls.get(note)?.classList.remove("active");
  lastHeldSet = held;
};

// ---- stage scale-to-fit (same technique as Nova Tape/Nova Pitch) ----
(function () {
  let lastW = -1, lastH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    const scale = Math.min(w / 1360, h / 800);
    currentStageScale = scale || 1;
    const stage = document.querySelector(".app");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();
