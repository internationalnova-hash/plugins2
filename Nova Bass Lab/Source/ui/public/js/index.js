import { getNativeFunction } from "./juce/index.js";

const nf = {};
for (const name of [
  "noteOn", "noteOff",
  "getVoiceParams", "setVoiceParams",
  "getColorState", "setColorMode", "setColorDrive",
  "getStereoParams", "setStereoParams",
  "generate", "mutate", "getGeneratorProfiles",
  "setLifecycleStatus", "getCandidateInfo",
  "render",
  "getMacroTable", "setMacroTable",
  "saveAsPreset", "getPresetLibrary", "loadPresetIntoLab",
  "jsError",
]) nf[name] = getNativeFunction(name);

window.onerror = function (msg, src, line, col, err) {
  try { nf.jsError(String(msg) + " @" + line + ":" + col); } catch (e) {}
};

const MACRO_NAMES = ["SHAPE", "DECAY", "DRIVE", "WIDTH", "GLIDE", "BASS"];
const TARGET_PARAMS = [
  "sineTriBlend", "fineTuneCents", "pitchEnvAmountSemitones", "pitchEnvDecaySeconds",
  "pitchEnvCurve", "ampAttackSeconds", "ampDecaySeconds", "ampSustain", "ampReleaseSeconds",
  "transientAmount", "transientToneHz", "transientDecaySeconds", "filterCutoffHz",
  "filterResonance", "glideTimeSeconds", "colorDrive", "stereoWidth",
];

let macroTable = {};
let activeMacro = MACRO_NAMES[0];

function $(id) { return document.getElementById(id); }

// ── Tabs ──────────────────────────────────────────────────────────────
function initTabs() {
  const buttons = document.querySelectorAll(".tabBtn");
  buttons.forEach((btn) => {
    btn.addEventListener("click", () => {
      buttons.forEach((b) => b.classList.remove("active"));
      document.querySelectorAll(".panel").forEach((p) => p.classList.remove("active"));
      btn.classList.add("active");
      $("panel-" + btn.dataset.tab).classList.add("active");
      if (btn.dataset.tab === "lifecycle") refreshCandidateInfo();
      if (btn.dataset.tab === "presets") refreshPresetLibrary();
      if (btn.dataset.tab === "macros") refreshMacroTable();
    });
  });
}

// ── Keyboard ──────────────────────────────────────────────────────────
function initKeyboard() {
  const keyboardEl = $("keyboard");
  const WHITE_OFFSETS = [0, 2, 4, 5, 7, 9, 11];

  function noteName(n) {
    const names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
    return names[n % 12] + (Math.floor(n / 12) - 1);
  }

  for (let octaveBase = 36; octaveBase < 60; octaveBase += 12) {
    for (let i = 0; i < 7; i++) {
      const note = octaveBase + WHITE_OFFSETS[i];
      const key = document.createElement("div");
      key.className = "key";
      key.title = noteName(note);
      key.addEventListener("pointerdown", () => { nf.noteOn(note); key.classList.add("active"); });
      const release = () => { nf.noteOff(note); key.classList.remove("active"); };
      key.addEventListener("pointerup", release);
      key.addEventListener("pointerleave", release);
      keyboardEl.appendChild(key);
    }
  }
}

// ── Synth page ────────────────────────────────────────────────────────
const SYNTH_RANGE_IDS = [
  "sineTriBlend", "fineTuneCents", "pitchEnvAmountSemitones", "pitchEnvDecaySeconds",
  "pitchEnvCurve", "ampAttackSeconds", "ampDecaySeconds", "ampSustain", "ampReleaseSeconds",
  "transientAmount", "transientToneHz", "transientDecaySeconds", "filterCutoffHz",
  "filterResonance", "glideTimeSeconds",
];

function initSynthPage() {
  SYNTH_RANGE_IDS.forEach((id) => {
    const input = $(id);
    input.addEventListener("input", () => {
      $("v_" + id).textContent = Number(input.value).toFixed(3);
      nf.setVoiceParams({ [id]: Number(input.value) });
    });
  });
  $("filterHighPass").addEventListener("change", (e) => nf.setVoiceParams({ filterHighPass: e.target.checked }));
  $("retrigger").addEventListener("change", (e) => nf.setVoiceParams({ retrigger: e.target.value }));
  $("glideMode").addEventListener("change", (e) => nf.setVoiceParams({ glideMode: e.target.value }));
}

async function refreshSynthPage() {
  const p = await nf.getVoiceParams();
  if (!p) return;
  SYNTH_RANGE_IDS.forEach((id) => {
    if (p[id] === undefined) return;
    $(id).value = p[id];
    $("v_" + id).textContent = Number(p[id]).toFixed(3);
  });
  $("filterHighPass").checked = !!p.filterHighPass;
  $("retrigger").value = p.retrigger;
  $("glideMode").value = p.glideMode;
}

// ── Color page ────────────────────────────────────────────────────────
function initColorPage() {
  $("colorMode").addEventListener("change", (e) => nf.setColorMode(e.target.value));
  $("colorDrive").addEventListener("input", (e) => {
    $("v_colorDrive").textContent = Number(e.target.value).toFixed(3);
    nf.setColorDrive(Number(e.target.value));
  });
}

async function refreshColorPage() {
  const c = await nf.getColorState();
  if (!c) return;
  $("colorMode").value = c.mode;
  $("colorDrive").value = c.drive;
  $("v_colorDrive").textContent = Number(c.drive).toFixed(3);
}

// ── Stereo page ───────────────────────────────────────────────────────
function initStereoPage() {
  ["widthAmount", "crossoverHz"].forEach((id) => {
    $(id).addEventListener("input", (e) => {
      $("v_" + id).textContent = Number(e.target.value).toFixed(3);
      nf.setStereoParams({ [id]: Number(e.target.value) });
    });
  });
}

async function refreshStereoPage() {
  const p = await nf.getStereoParams();
  if (!p) return;
  $("widthAmount").value = p.widthAmount;
  $("v_widthAmount").textContent = Number(p.widthAmount).toFixed(3);
  $("crossoverHz").value = p.crossoverHz;
  $("v_crossoverHz").textContent = Number(p.crossoverHz).toFixed(3);
}

// ── Generate / Mutate page ───────────────────────────────────────────
async function initGeneratePage() {
  const profiles = await nf.getGeneratorProfiles();
  const select = $("genProfile");
  (profiles || []).forEach((p) => {
    const opt = document.createElement("option");
    opt.value = p.id;
    opt.textContent = p.id + " (" + p.category + ")";
    select.appendChild(opt);
  });

  $("btnGenerate").addEventListener("click", async () => {
    await nf.generate(select.value);
    await refreshAllLiveState();
  });
  $("btnMutate").addEventListener("click", async () => {
    await nf.mutate($("mutateIntensity").value);
    await refreshAllLiveState();
  });
}

// ── Lifecycle page ────────────────────────────────────────────────────
function initLifecyclePage() {
  $("btnStatusCandidate").addEventListener("click", async () => { await nf.setLifecycleStatus("candidate"); refreshCandidateInfo(); });
  $("btnStatusFavorite").addEventListener("click", async () => { await nf.setLifecycleStatus("favorite"); refreshCandidateInfo(); });
  $("btnStatusApproved").addEventListener("click", async () => { await nf.setLifecycleStatus("approved"); refreshCandidateInfo(); });
  $("btnStatusRejected").addEventListener("click", async () => { await nf.setLifecycleStatus("rejected"); refreshCandidateInfo(); });
}

async function refreshCandidateInfo() {
  const info = await nf.getCandidateInfo();
  $("candidateInfo").textContent = info
    ? `id: ${info.id}\nstatus: ${info.status}\ngeneratorId: ${info.generatorId || "(mutation)"}\ncreated: ${info.createdDate}\nmutationParent: ${info.mutationParentId || "-"}\nrenderedAsset: ${info.renderedAssetPath || "(not rendered yet)"}`
    : "No candidate loaded (generate or mutate a sound first).";
}

// ── Render page ───────────────────────────────────────────────────────
function initRenderPage() {
  $("btnRender").addEventListener("click", async () => {
    const fileName = await nf.render({
      rootNote: Number($("renderRootNote").value),
      durationSeconds: Number($("renderDuration").value),
      tailSeconds: Number($("renderTail").value),
    });
    $("renderResult").textContent = fileName ? `Rendered: ${fileName}` : "Render failed.";
  });
}

// ── Macro editor page ─────────────────────────────────────────────────
function initMacroTabs() {
  const container = $("macroTabs");
  MACRO_NAMES.forEach((name) => {
    const btn = document.createElement("button");
    btn.className = "macroTabBtn" + (name === activeMacro ? " active" : "");
    btn.textContent = name;
    btn.addEventListener("click", () => {
      activeMacro = name;
      container.querySelectorAll(".macroTabBtn").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      renderMacroRows();
    });
    container.appendChild(btn);
  });
}

function renderMacroRows() {
  const rowsEl = $("macroRows");
  rowsEl.innerHTML = "";
  const mappings = macroTable[activeMacro] || [];
  mappings.forEach((m, idx) => {
    const row = document.createElement("div");
    row.className = "macroRow";

    const targetSelect = document.createElement("select");
    TARGET_PARAMS.forEach((tp) => {
      const opt = document.createElement("option");
      opt.value = tp; opt.textContent = tp;
      if (tp === m.targetParam) opt.selected = true;
      targetSelect.appendChild(opt);
    });
    targetSelect.addEventListener("change", () => { m.targetParam = targetSelect.value; });

    const minInput = document.createElement("input");
    minInput.type = "number"; minInput.step = "0.01"; minInput.value = m.min;
    minInput.addEventListener("change", () => { m.min = Number(minInput.value); });

    const maxInput = document.createElement("input");
    maxInput.type = "number"; maxInput.step = "0.01"; maxInput.value = m.max;
    maxInput.addEventListener("change", () => { m.max = Number(maxInput.value); });

    const curveSelect = document.createElement("select");
    ["linear", "exponential", "logarithmic", "sCurve"].forEach((c) => {
      const opt = document.createElement("option");
      opt.value = c; opt.textContent = c;
      if (c === m.curve) opt.selected = true;
      curveSelect.appendChild(opt);
    });
    curveSelect.addEventListener("change", () => { m.curve = curveSelect.value; });

    const polSelect = document.createElement("select");
    ["normal", "inverted"].forEach((pol) => {
      const opt = document.createElement("option");
      opt.value = pol; opt.textContent = pol;
      if (pol === m.polarity) opt.selected = true;
      polSelect.appendChild(opt);
    });
    polSelect.addEventListener("change", () => { m.polarity = polSelect.value; });

    const removeBtn = document.createElement("button");
    removeBtn.textContent = "✕";
    removeBtn.className = "action danger";
    removeBtn.addEventListener("click", () => {
      mappings.splice(idx, 1);
      renderMacroRows();
    });

    row.appendChild(targetSelect);
    row.appendChild(minInput);
    row.appendChild(maxInput);
    row.appendChild(curveSelect);
    row.appendChild(polSelect);
    row.appendChild(removeBtn);
    rowsEl.appendChild(row);
  });
}

async function refreshMacroTable() {
  macroTable = (await nf.getMacroTable()) || {};
  MACRO_NAMES.forEach((name) => { if (!macroTable[name]) macroTable[name] = []; });
  renderMacroRows();
}

function initMacroPage() {
  initMacroTabs();
  $("btnAddMapping").addEventListener("click", () => {
    if (!macroTable[activeMacro]) macroTable[activeMacro] = [];
    macroTable[activeMacro].push({ targetParam: TARGET_PARAMS[0], min: 0, max: 1, curve: "linear", polarity: "normal" });
    renderMacroRows();
  });
  $("btnSaveMacros").addEventListener("click", async () => {
    await nf.setMacroTable(macroTable);
  });
}

// ── Presets page ──────────────────────────────────────────────────────
function initPresetsPage() {
  $("btnSavePreset").addEventListener("click", async () => {
    const tags = $("presetTags").value.split(",").map((t) => t.trim()).filter(Boolean);
    const ok = await nf.saveAsPreset({
      name: $("presetName").value,
      category: $("presetCategory").value,
      description: $("presetDescription").value,
      tags,
    });
    $("saveResult").textContent = ok ? "Saved to Nova Bass Vault factory presets." : "Save failed.";
    if (ok) refreshPresetLibrary();
  });
  $("btnRefreshPresets").addEventListener("click", refreshPresetLibrary);
}

async function refreshPresetLibrary() {
  const list = (await nf.getPresetLibrary()) || [];
  const listEl = $("presetList");
  listEl.innerHTML = "";
  list.forEach((p) => {
    const row = document.createElement("div");
    row.className = "listRow";
    const name = document.createElement("span");
    name.className = "name";
    name.textContent = `${p.name} (${p.category})`;
    const loadBtn = document.createElement("button");
    loadBtn.className = "action secondary";
    loadBtn.textContent = "LOAD";
    loadBtn.addEventListener("click", async () => {
      await nf.loadPresetIntoLab(p.index);
      await refreshAllLiveState();
    });
    row.appendChild(name);
    row.appendChild(loadBtn);
    listEl.appendChild(row);
  });
}

// ── Cross-page refresh (after Generate/Mutate/Load change the whole sound) ──
async function refreshAllLiveState() {
  await Promise.all([refreshSynthPage(), refreshColorPage(), refreshStereoPage(), refreshCandidateInfo()]);
  if ($("panel-macros").classList.contains("active")) await refreshMacroTable();
}

// ── Boot ──────────────────────────────────────────────────────────────
(function () {
  initTabs();
  initKeyboard();
  initSynthPage();
  initColorPage();
  initStereoPage();
  initGeneratePage();
  initLifecyclePage();
  initRenderPage();
  initMacroPage();
  initPresetsPage();

  refreshAllLiveState();

  window.updateStatus = function (status) {
    $("status").textContent = status && status.ready ? "Ready — click keys to audition live." : "Loading...";
  };
})();
