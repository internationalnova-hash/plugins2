import { getNativeFunction, getSliderState, getComboBoxState, getToggleState } from "./juce/index.js";

const native = {
  toggleListen: getNativeFunction("toggleListen"),
  getPresets: getNativeFunction("getPresets"),
  loadPreset: getNativeFunction("loadPreset"),
  savePreset: getNativeFunction("savePreset"),
  deletePreset: getNativeFunction("deletePreset"),
  revealPresets: getNativeFunction("revealPresets"),
  reapplyVibe: getNativeFunction("reapplyVibe"),
  forgetVoice: getNativeFunction("forgetVoice"),
  setWindowScale: getNativeFunction("setWindowScale"),
};
const $ = (id) => document.getElementById(id);

// Vibe facts mirrored from Source/dsp/NovaVibes.h (display only: card text
// and the Chain Curve's EQ shape).
const VIBES = [
  { d: "bright · tight · hard tune",  hpf: 140, mud: 5, presHz: 4000, presDb: 5, airHz: 12000, airDb: 8, dsHz: 6500 },
  { d: "glossy · wide · full tune",   hpf: 150, mud: 5, presHz: 3500, presDb: 4, airHz: 11000, airDb: 8, dsHz: 6500 },
  { d: "warm · silky · soft tune",    hpf: 110, mud: 4, presHz: 3000, presDb: 3, airHz: 10000, airDb: 7, dsHz: 6000 },
  { d: "clean · forward · polished",  hpf: 120, mud: 4, presHz: 3500, presDb: 4, airHz: 12000, airDb: 7, dsHz: 7000 },
  { d: "dark · punchy · gritty",      hpf: 160, mud: 6, presHz: 2500, presDb: 5, airHz:  9000, airDb: 3, dsHz: 6000 },
  { d: "light touch · no tune",       hpf:  90, mud: 3, presHz: 3000, presDb: 2, airHz: 12000, airDb: 4, dsHz: 6500 },
];
const DEFAULTS = { clean: 0.6, deess: 0.55, punch: 0.7, air: 0.75, warmth: 0.25, space: 0.55, tune: 0.95, mix: 1, output: 0.5 };

// ── Knobs ────────────────────────────────────────────────────────────────
// Vertical drag (up = more), Shift = fine, wheel steps, double-click resets.
const A0 = -135, A1 = 135;
function pol(c, r, d) { const a = (d - 90) * Math.PI / 180; return [c + r * Math.cos(a), c + r * Math.sin(a)]; }
function arc(c, r, a, b) {
  if (Math.abs(b - a) < .01) return "";
  const [x0, y0] = pol(c, r, a), [x1, y1] = pol(c, r, b);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)} A${r} ${r} 0 ${Math.abs(b - a) > 180 ? 1 : 0} ${b > a ? 1 : 0} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

const knobState = {};
let uid = 0;
function makeKnob(w) {
  const k = w.dataset.k, S = +w.dataset.size, bi = w.dataset.bi === "1", id = "kg" + (uid++);
  const state = getSliderState(k);
  knobState[k] = state;
  const c = 50, R = 47, inset = S * 0.16, ptrLen = S * 0.2, ptrOff = (S - 2 * inset) / 2 - 4;
  w.insertAdjacentHTML("afterbegin", `
    <div class="mk" style="width:${S}px;height:${S}px">
      <svg class="arc" viewBox="0 0 100 100">
        <defs><linearGradient id="${id}" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="#c46a10"/><stop offset=".6" stop-color="#ffa830"/><stop offset="1" stop-color="#ffe0a0"/></linearGradient></defs>
        <path d="${arc(c, R, A0, A1)}" fill="none" stroke="rgba(255,255,255,0.08)" stroke-width="4" stroke-linecap="round"/>
        <path class="va" fill="none" stroke="url(#${id})" stroke-width="4.5" stroke-linecap="round" style="filter:drop-shadow(0 0 3px #ff9a2a) drop-shadow(0 0 7px rgba(255,150,40,.6))"/>
      </svg>
      <div class="body" style="inset:${inset}px"></div>
      <div class="ptr"><i style="top:${-ptrOff}px;height:${ptrLen}px"></i></div>
    </div>
    ${w.dataset.label ? `<div class="kl">${w.dataset.label}</div><div class="kh">${w.dataset.hint}</div><div class="kv"></div>` : ""}`);
  const va = w.querySelector(".va"), ptr = w.querySelector(".ptr"), kv = w.querySelector(".kv"), kn = w.querySelector(".mk");

  function render() {
    const v = state.getNormalisedValue(), a = A0 + v * (A1 - A0), from = bi ? 0 : A0;
    va.setAttribute("d", arc(c, R, Math.min(from, a), Math.max(from, a)));
    ptr.style.transform = `rotate(${a}deg)`;
    if (kv) kv.textContent = Math.round(v * 100) + "%";
    if (k === "tune") renderTuneExtras(v);
    if (k === "output") { const db = state.getScaledValue(); $("outval").textContent = (db >= 0 ? "+" : "") + db.toFixed(1) + " dB"; }
  }

  let drag = null;
  kn.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    e.preventDefault();
    drag = { y: e.clientY, s: state.getNormalisedValue(), id: e.pointerId };
    try { kn.setPointerCapture(e.pointerId); } catch (_) {}
    state.sliderDragStarted();
  });
  kn.addEventListener("pointermove", (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    const sc = window.__novaVocalScale || 1;
    state.setNormalisedValue(Math.max(0, Math.min(1, drag.s + (drag.y - e.clientY) / sc * (e.shiftKey ? .001 : .0045))));
    render();
  });
  const end = (e) => {
    if (!drag || e.pointerId !== drag.id) return;
    drag = null;
    try { kn.releasePointerCapture(e.pointerId); } catch (_) {}
    state.sliderDragEnded();
  };
  kn.addEventListener("pointerup", end);
  kn.addEventListener("pointercancel", end);
  kn.addEventListener("wheel", (e) => {
    e.preventDefault();
    state.sliderDragStarted();
    state.setNormalisedValue(Math.max(0, Math.min(1, state.getNormalisedValue() + (e.shiftKey ? .0015 : .01) * (e.deltaY > 0 ? -1 : 1))));
    state.sliderDragEnded();
    render();
  }, { passive: false });
  kn.addEventListener("dblclick", () => {
    state.sliderDragStarted();
    state.setNormalisedValue(DEFAULTS[k] ?? 0.5);
    state.sliderDragEnded();
    render();
  });

  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();
}

function setKnob(k, v) {
  const s = knobState[k];
  s.sliderDragStarted();
  s.setNormalisedValue(v);
  s.sliderDragEnded();
}

// Tune amount readout + NATURAL / BALANCED / HARD shortcuts
const segs = [...document.querySelectorAll("#seg div")];
function renderTuneExtras(v) {
  $("tuneval").textContent = v < 0.005 ? "OFF" : Math.round(v * 100) + "%";
  let best = null;
  segs.forEach((d) => { if (!best || Math.abs(+d.dataset.v - v) < Math.abs(+best.dataset.v - v)) best = d; });
  segs.forEach((d) => d.classList.toggle("on", d === best && Math.abs(+d.dataset.v - v) < 0.151));
}
segs.forEach((d) => d.addEventListener("click", () => setKnob("tune", +d.dataset.v)));

document.querySelectorAll(".kw[data-k]").forEach(makeKnob);

// ── Vibe (cards + arrows) ────────────────────────────────────────────────
const vibeState = getComboBoxState("vibe");
const vibesEl = $("vibes");
function vibeCount() { return (vibeState.properties.choices || []).length || VIBES.length; }
function setVibe(i) { const n = vibeCount(); vibeState.setChoiceIndex(((i % n) + n) % n); renderVibe(); }
function renderVibe() {
  const choices = vibeState.properties.choices || [];
  const idx = vibeState.getChoiceIndex();
  if (vibesEl.children.length !== choices.length) {
    vibesEl.innerHTML = "";
    choices.forEach((name, i) => {
      const el = document.createElement("div");
      el.className = "vibe";
      el.innerHTML = `<div class="vn"></div><div class="vd"></div>`;
      el.querySelector(".vn").textContent = name;
      el.querySelector(".vd").textContent = VIBES[i] ? VIBES[i].d : "";
      el.addEventListener("click", () => setVibe(i));
      vibesEl.appendChild(el);
    });
  }
  [...vibesEl.children].forEach((c, j) => c.classList.toggle("sel2", j === idx));
  if (choices[idx]) $("vname").textContent = choices[idx];
}
$("vprev").addEventListener("click", () => setVibe(vibeState.getChoiceIndex() - 1));
$("vnext").addEventListener("click", () => setVibe(vibeState.getChoiceIndex() + 1));
vibeState.valueChangedEvent.addListener(renderVibe);
vibeState.propertiesChangedEvent.addListener(renderVibe);
renderVibe();

// ── Key (arrows) ─────────────────────────────────────────────────────────
// Choices: "Auto (Listen)", "Chromatic", then each key Major/Minor.
const keyState = getComboBoxState("key");
let lastKeyName = "";
function renderKey() {
  const choices = keyState.properties.choices || [];
  const idx = keyState.getChoiceIndex();
  let t = choices[idx] || "Auto";
  if (idx === 0) t = lastKeyName ? "Auto · " + lastKeyName.replace(" (auto)", "") : "Auto";
  $("kname").textContent = t;
}
function stepKey(d) {
  const n = (keyState.properties.choices || []).length || 26;
  keyState.setChoiceIndex(((keyState.getChoiceIndex() + d) % n + n) % n);
  renderKey();
}
$("kprev").addEventListener("click", () => stepKey(-1));
$("knext").addEventListener("click", () => stepKey(1));
keyState.valueChangedEvent.addListener(renderKey);
keyState.propertiesChangedEvent.addListener(renderKey);
renderKey();

// ── Bypass (power: lit = processing) ─────────────────────────────────────
const bypassState = getToggleState("bypass");
function renderBypass() { $("pwr").classList.toggle("off", !!bypassState.getValue()); }
$("pwr").addEventListener("click", () => { bypassState.setValue(!bypassState.getValue()); renderBypass(); });
bypassState.valueChangedEvent.addListener(renderBypass);
renderBypass();

// ── Tempo: Sync (follow the song) / Manual BPM ───────────────────────────
// Drives the delay rhythm and the reverb's pre-delay and length.
const syncState = getToggleState("tempoSync");
const bpmState = getSliderState("bpm");
let hostTempo = 0, activeTempo = 120, bpmEditing = false;
const bpmBox = $("bpmbox"), bpmVal = $("bpmval");
function bpmNorm(b) { return (b - 40) / 200; }
function renderTempo() {
  const sync = !!syncState.getValue();
  $("t-sync").classList.toggle("on", sync);
  $("t-man").classList.toggle("on", !sync);
  const synced = sync && hostTempo > 20;
  bpmBox.classList.toggle("synced", synced);
  if (!bpmEditing) bpmVal.textContent = (synced ? hostTempo : (bpmState.getScaledValue() || 120)).toFixed(1);
  $("tnote").textContent = synced ? "from project" : sync ? "no song tempo" : "manual";
}
function setBpm(b) {
  b = clamp(Math.round(b * 10) / 10, 40, 240);
  if (syncState.getValue()) syncState.setValue(false);
  bpmState.sliderDragStarted(); bpmState.setNormalisedValue(bpmNorm(b)); bpmState.sliderDragEnded();
  renderTempo();
}
$("t-sync").addEventListener("click", () => { syncState.setValue(true); renderTempo(); });
$("t-man").addEventListener("click", () => {
  // Start manual from the tempo you're hearing, so nothing jumps
  if (syncState.getValue() && hostTempo > 20) { bpmState.sliderDragStarted(); bpmState.setNormalisedValue(bpmNorm(hostTempo)); bpmState.sliderDragEnded(); }
  syncState.setValue(false); renderTempo();
});
let bpmDrag = null;
bpmBox.addEventListener("pointerdown", (e) => {
  if (e.button !== 0 || bpmEditing) return;
  e.preventDefault();
  bpmDrag = { y: e.clientY, start: parseFloat(bpmVal.textContent) || 120, id: e.pointerId, moved: false };
  try { bpmBox.setPointerCapture(e.pointerId); } catch (_) {}
});
bpmBox.addEventListener("pointermove", (e) => {
  if (!bpmDrag || e.pointerId !== bpmDrag.id) return;
  const dy = (bpmDrag.y - e.clientY) / (window.__novaVocalScale || 1);
  if (Math.abs(dy) < 2 && !bpmDrag.moved) return;
  bpmDrag.moved = true;
  setBpm(bpmDrag.start + dy * (e.shiftKey ? 0.05 : 0.5));
});
const bpmEnd = (e) => { if (bpmDrag && e.pointerId === bpmDrag.id) { bpmDrag = null; try { bpmBox.releasePointerCapture(e.pointerId); } catch (_) {} } };
bpmBox.addEventListener("pointerup", bpmEnd);
bpmBox.addEventListener("pointercancel", bpmEnd);
bpmBox.addEventListener("wheel", (e) => { e.preventDefault(); setBpm((parseFloat(bpmVal.textContent) || 120) + (e.deltaY > 0 ? -1 : 1) * (e.shiftKey ? 0.1 : 1)); }, { passive: false });
bpmBox.addEventListener("dblclick", () => {
  if (bpmEditing) return;
  bpmEditing = true;
  const cur = bpmVal.textContent;
  bpmVal.innerHTML = `<input id="bpm-input" type="text" inputmode="decimal" maxlength="6">`;
  const inp = $("bpm-input");
  inp.value = cur;
  const done = (commit) => {
    if (!bpmEditing) return;
    bpmEditing = false;
    const v = parseFloat(inp.value);
    bpmVal.textContent = cur;
    if (commit && isFinite(v)) setBpm(v); else renderTempo();
  };
  inp.addEventListener("keydown", (e) => { e.stopPropagation(); if (e.key === "Enter") done(true); if (e.key === "Escape") done(false); });
  inp.addEventListener("blur", () => done(true));
  setTimeout(() => { inp.focus(); inp.select(); }, 20);
});
syncState.valueChangedEvent.addListener(renderTempo);
bpmState.valueChangedEvent.addListener(renderTempo);
bpmState.propertiesChangedEvent.addListener(renderTempo);
renderTempo();

// ── Popovers: presets, save, settings, more ──────────────────────────────
const pops = ["pop-presets", "pop-save", "pop-settings", "pop-more"].map($);
function closePops(except) { pops.forEach((p) => { if (p !== except) p.classList.remove("open"); }); }
function togglePop(p) { const open = !p.classList.contains("open"); closePops(); p.classList.toggle("open", open); return open; }
pops.forEach((p) => p.addEventListener("pointerdown", (e) => e.stopPropagation()));
document.addEventListener("pointerdown", () => closePops());

let presets = { names: ["Default"], current: "" };
function presetLabel() { return presets.current ? presets.current : "Nova AI Vocal"; }
function applyPresetList(r) {
  if (r && r.names) presets = r;
  $("pname").textContent = presetLabel();
  const pop = $("pop-presets");
  pop.innerHTML = "";
  const groups = presets.groups || [];
  let lastGroup = null;
  presets.names.forEach((name, i) => {
    const group = groups[i] !== undefined ? groups[i] : (name === "Default" ? "" : "USER");
    if (group !== lastGroup) {
      lastGroup = group;
      const hd = document.createElement("div");
      hd.className = "hd";
      hd.textContent = group === "" ? "PRESETS" : group === "USER" ? "MY PRESETS" : group;
      pop.appendChild(hd);
    }
    const it = document.createElement("div");
    const isCur = name === "Default" ? !presets.current : name === presets.current;
    it.className = "it" + (isCur ? " cur" : "");
    const label = document.createElement("span");
    label.textContent = name === "Default" ? "Default (factory)" : name;
    it.appendChild(label);
    if (group === "USER") {
      const x = document.createElement("span");
      x.className = "x"; x.textContent = "✕"; x.title = "Delete preset";
      x.addEventListener("click", async (e) => {
        e.stopPropagation();
        applyPresetList(await native.deletePreset(name));
        pop.classList.add("open");
      });
      it.appendChild(x);
    }
    it.addEventListener("click", async () => { closePops(); applyPresetList(await native.loadPreset(name)); });
    pop.appendChild(it);
  });
}
async function refreshPresets() { try { applyPresetList(await native.getPresets()); } catch (_) {} }
async function stepPreset(d) {
  const n = presets.names.length; if (!n) return;
  const cur = presets.current ? presets.names.indexOf(presets.current) : 0;
  const next = presets.names[(((cur < 0 ? 0 : cur) + d) % n + n) % n];
  applyPresetList(await native.loadPreset(next));
}
$("pprev").addEventListener("click", () => stepPreset(-1));
$("pnext").addEventListener("click", () => stepPreset(1));
$("pname").addEventListener("pointerdown", (e) => e.stopPropagation());
$("pname").addEventListener("click", () => { refreshPresets(); togglePop($("pop-presets")); });

function openSave() {
  const p = $("pop-save");
  if (togglePop(p)) { const inp = $("save-name"); inp.value = presets.current || ""; setTimeout(() => { inp.focus(); inp.select(); }, 30); }
}
async function doSave() {
  const name = $("save-name").value.trim();
  if (!name) { $("save-name").focus(); return; }
  const r = await native.savePreset(name);
  applyPresetList(r);
  closePops();
}
$("btn-save").addEventListener("pointerdown", (e) => e.stopPropagation());
$("btn-save").addEventListener("click", openSave);
$("save-ok").addEventListener("click", doSave);
$("save-cancel").addEventListener("click", () => closePops());
$("save-name").addEventListener("keydown", (e) => { e.stopPropagation(); if (e.key === "Enter") doSave(); if (e.key === "Escape") closePops(); });

let windowScale = 1;
function renderSizes() { document.querySelectorAll("#pop-settings .sizes .b").forEach((b) => b.classList.toggle("on", Math.abs(+b.dataset.s - windowScale) < 0.01)); }
document.querySelectorAll("#pop-settings .sizes .b").forEach((b) => b.addEventListener("click", () => {
  windowScale = +b.dataset.s; renderSizes(); native.setWindowScale(windowScale);
}));
$("btn-settings").addEventListener("pointerdown", (e) => e.stopPropagation());
$("btn-settings").addEventListener("click", () => { renderSizes(); togglePop($("pop-settings")); });

$("btn-more").addEventListener("pointerdown", (e) => e.stopPropagation());
$("btn-more").addEventListener("click", () => togglePop($("pop-more")));
$("m-reapply").addEventListener("click", () => { closePops(); native.reapplyVibe(); });
$("m-forget").addEventListener("click", () => { closePops(); native.forgetVoice(); });
$("m-folder").addEventListener("click", () => { closePops(); native.revealPresets(); });

refreshPresets();

// ── Auto Mix orb ─────────────────────────────────────────────────────────
const CHECK = `<svg viewBox="0 0 12 12"><path d="M1.5 6.5l3 3 6-7"/></svg>`;
let ls = "Idle", captured = 0, lastChips = null;
$("orb").addEventListener("click", () => native.toggleListen());

function renderListen(s) {
  ls = s.state; captured = s.captured || 0;
  const ok = $("ok"), ol = $("ol"), os = $("os"), oi = $("oicon"), d = $("amdesc");
  ol.style.fontSize = (ls === "Listening" || ls === "Analyzing") ? "24px" : "31px";
  let chips = [];
  switch (ls) {
    case "Idle":
      ok.textContent = "NOVA"; ol.textContent = "LISTEN"; os.textContent = "ANALYZE & MIX"; oi.className = "play";
      d.innerHTML = s.summary ? escapeHtml(s.summary) : "Analyzes your vocal and applies an<br>intelligent processing chain.";
      break;
    case "Listening":
      ok.textContent = "NOVA IS"; ol.textContent = "LISTENING"; os.textContent = captured.toFixed(1) + " S · TAP TO MIX"; oi.className = "stop";
      d.innerHTML = captured < 2 ? "Play your vocal now…" : "Keep going, then tap the orb to mix.<br>10–30 seconds works best.";
      break;
    case "Analyzing":
      ok.textContent = "NOVA IS"; ol.textContent = "MIXING"; os.textContent = "ANALYZING YOUR VOICE"; oi.className = "";
      d.innerHTML = "Analyzing your voice…";
      break;
    case "Mixed":
      ok.textContent = "VOICE"; ol.textContent = "MIXED"; os.textContent = "TAP TO RE-LISTEN"; oi.className = "play";
      d.innerHTML = "Chain built for your voice. Change vibe<br>to re-mix, or fine-tune the knobs.";
      chips = (s.summary || "").split(" | ").filter(Boolean).slice(0, 4);
      break;
  }
  const key = chips.join("|");
  if (key !== lastChips) {
    lastChips = key;
    $("chips").innerHTML = chips.map((t) => `<div class="chip" title="${escapeHtml(t)}">${CHECK}<span style="overflow:hidden;text-overflow:ellipsis">${escapeHtml(capitalize(t))}</span></div>`).join("");
  }
}
function escapeHtml(t) { return String(t).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c])); }
function capitalize(t) { return t.charAt(0).toUpperCase() + t.slice(1); }

const wave = $("wave"), WB = 19;
wave.innerHTML = `<defs><linearGradient id="wg" x1="0" x2="1"><stop offset="0" stop-color="#c46a10"/><stop offset=".5" stop-color="#ffe0a0"/><stop offset="1" stop-color="#c46a10"/></linearGradient></defs>` +
  Array.from({ length: WB }, (_, i) => `<rect x="${i * 4.8 + 1}" width="2.4" rx="1.2" fill="url(#wg)"/>`).join("");
const bars = [...wave.querySelectorAll("rect")];
const oc = $("orbc"), ox = oc.getContext("2d");
let inLevel = 0; // 0..1 from the input meter, drives the waveform

function drawOrb(t) {
  const W = oc.width, C = W / 2;
  ox.clearRect(0, 0, W, W);
  const live = ls === "Listening" || ls === "Analyzing";
  bars.forEach((b, i) => {
    const mid = 1 - Math.abs(i - (WB - 1) / 2) / ((WB - 1) / 2);
    const idle = 4 + 26 * mid * (0.55 + 0.25 * Math.sin(t * 1.5 + i * 0.9)) * (0.4 + 0.6 * Math.abs(Math.sin(i * 2.3)));
    const act = 4 + 28 * mid * (0.35 + 0.65 * Math.abs(Math.sin(t * 6 + i * 1.7) * Math.cos(t * 3.3 + i)));
    const h = idle + (act - idle) * Math.min(1, inLevel * 1.4);
    b.setAttribute("height", h.toFixed(1)); b.setAttribute("y", ((34 - h) / 2).toFixed(1));
  });
  if (live) {
    const R = 175, frac = ls === "Analyzing" ? 1 : Math.min(1, captured / 30);
    ox.lineWidth = 6; ox.lineCap = "round"; ox.strokeStyle = "#fff1cc"; ox.shadowColor = "#ffb44a"; ox.shadowBlur = 16;
    ox.beginPath(); ox.arc(C, C, R, -Math.PI / 2, -Math.PI / 2 + Math.max(0.02, frac) * Math.PI * 2); ox.stroke();
    const a = t * 2 - Math.PI / 2; ox.fillStyle = "#fff"; ox.shadowBlur = 22;
    ox.beginPath(); ox.arc(C + Math.cos(a) * R, C + Math.sin(a) * R, 5, 0, Math.PI * 2); ox.fill(); ox.shadowBlur = 0;
  }
  $("glowring").style.opacity = (live ? 0.75 + 0.25 * Math.sin(t * 5) : 0.85 + 0.15 * Math.sin(t * 1.6)).toFixed(2);
}

// ── Chain Curve: real biquad responses of the chain's EQ ─────────────────
const SR = 48000;
function biquadMag(b0, b1, b2, a0, a1, a2, f) {
  const w = 2 * Math.PI * f / SR, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
  const nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
  const dr = a0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
  return Math.sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
}
function hpDb(hz, q, f) { const w0 = 2 * Math.PI * hz / SR, c = Math.cos(w0), al = Math.sin(w0) / (2 * q);
  return 20 * Math.log10(biquadMag((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al, f)); }
function peakDb(hz, q, g, f) { if (Math.abs(g) < 0.01) return 0; const A = Math.pow(10, g / 40), w0 = 2 * Math.PI * hz / SR, c = Math.cos(w0), al = Math.sin(w0) / (2 * q);
  return 20 * Math.log10(biquadMag(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A, f)); }
function shelfDb(hz, q, g, f) { if (Math.abs(g) < 0.01) return 0; const A = Math.pow(10, g / 40), w0 = 2 * Math.PI * hz / SR, c = Math.cos(w0), al = Math.sin(w0) / (2 * q), sA = 2 * Math.sqrt(A) * al;
  return 20 * Math.log10(biquadMag(A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
    (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA, f)); }
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

let plan = { active: false, mud: 0, box: 0, pres: 0, air: 0 };
let deessGr = 0, spectrum = null, playing = false;
let voicing = null; // active recipe's EQ from C++ (artist presets carry their own)
function activeV() {
  const B = VIBES[vibeState.getChoiceIndex()] || VIBES[0];
  return voicing ? { hpf: voicing.hpf, mud: voicing.mud, presHz: voicing.presHz, presDb: voicing.presDb,
                     airHz: voicing.airHz, airDb: voicing.airDb, dsHz: voicing.dsHz } : B;
}
function chainDb(f) {
  const V = activeV();
  const n = (k) => knobState[k] ? knobState[k].getNormalisedValue() : 0;
  const clean = n("clean"), air = n("air");
  const on = plan.active;
  const hpf = 30 + clean * (V.hpf - 30);
  return hpDb(hpf, 0.5412, f) + hpDb(hpf, 1.3066, f)
    + peakDb(300, 1.0, clamp((on ? plan.mud : 0) - clean * V.mud, -15, 6), f)
    + peakDb(750, 1.4, clamp(on ? plan.box : 0, -9, 4), f)
    + peakDb(V.presHz, 0.8, clamp((on ? plan.pres : 0) + air * V.presDb, -9, 12), f)
    + shelfDb(V.airHz, 0.707, clamp((on ? plan.air : 0) + air * V.airDb, -9, 12), f);
}

const sc = $("spec"), sx = sc.getContext("2d");
const shown = new Float32Array(96).fill(-120);
function drawSpec() {
  const cw = Math.round(sc.clientWidth * 2), ch = Math.round(sc.clientHeight * 2);
  if (cw > 0 && (sc.width !== cw || sc.height !== ch)) { sc.width = cw; sc.height = ch; }
  const W = sc.width, H = sc.height, L = 64, B = 40, T = 10, Rr = 20, PW = W - L - Rr, PH = H - B - T;
  sx.clearRect(0, 0, W, H);
  const fLo = 30, fHi = 20000;
  const fx = (f) => L + (Math.log10(f / fLo) / Math.log10(fHi / fLo)) * PW;
  const dy = (db) => T + PH / 2 - db * (PH / 2 / 14);
  sx.fillStyle = "rgba(0,0,0,0.35)"; sx.fillRect(L, T, PW, PH);
  sx.strokeStyle = "rgba(255,255,255,0.07)"; sx.lineWidth = 1.5; sx.font = "22px -apple-system, Helvetica, Arial"; sx.fillStyle = "#d6d1c6";
  sx.textAlign = "center";
  [50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000].forEach((f) => { const x = fx(f); sx.beginPath(); sx.moveTo(x, T); sx.lineTo(x, T + PH); sx.stroke();
    sx.fillText(f >= 1000 ? f / 1000 + "k" : f, Math.min(x, L + PW - 14), H - 8); });
  sx.textAlign = "right";
  [12, 6, 0, -6, -12].forEach((d) => { const y = dy(d); sx.beginPath(); sx.moveTo(L, y); sx.lineTo(L + PW, y); sx.stroke();
    sx.fillText((d > 0 ? "+" : "") + d, L - 12, y + 7); });
  sx.strokeStyle = "rgba(255,255,255,0.15)"; sx.strokeRect(L, T, PW, PH);

  sx.save(); sx.beginPath(); sx.rect(L, T, PW, PH); sx.clip();

  // Input spectrum: -84..0 dBFS mapped onto the panel height
  if (spectrum && spectrum.length) {
    const nb = spectrum.length, bw = PW / nb;
    for (let i = 0; i < nb; i++) {
      shown[i] = spectrum[i];
      const p = clamp((spectrum[i] + 84) / 84, 0, 1);
      if (p <= 0.01) continue;
      const y = T + PH - p * PH;
      const g = sx.createLinearGradient(0, y, 0, T + PH); g.addColorStop(0, "rgba(220,215,205,0.45)"); g.addColorStop(1, "rgba(220,215,205,0.03)");
      sx.fillStyle = g; sx.fillRect(L + i * bw + 1, y, Math.max(1, bw - 2), T + PH - y);
    }
  }

  // Chain curve + live de-ess dip
  const V = activeV();
  const pts = [];
  for (let x = L; x <= L + PW; x += 3) { const f = fLo * Math.pow(fHi / fLo, (x - L) / PW); pts.push([x, f, dy(Math.max(-16, chainDb(f)))]); }
  const dsd = Math.min(12, deessGr);
  if (dsd > 0.1) {
    sx.beginPath(); pts.forEach(([x, , y], i) => i ? sx.lineTo(x, y) : sx.moveTo(x, y));
    for (let i = pts.length - 1; i >= 0; i--) { const [x, f, y] = pts[i];
      sx.lineTo(x, y + dsd * (f > V.dsHz ? 1 : Math.pow(f / V.dsHz, 4)) * (PH / 2 / 14)); }
    sx.closePath();
    const rg = sx.createLinearGradient(0, dy(4), 0, dy(-8)); rg.addColorStop(0, "rgba(255,60,60,0.25)"); rg.addColorStop(1, "rgba(255,60,60,0.75)");
    sx.fillStyle = rg; sx.fill();
  }
  sx.beginPath(); pts.forEach(([x, , y], i) => i ? sx.lineTo(x, y) : sx.moveTo(x, y));
  sx.strokeStyle = "#ffb03a"; sx.lineWidth = 4; sx.shadowColor = "#ff9a2a"; sx.shadowBlur = 18; sx.stroke(); sx.stroke(); sx.shadowBlur = 0;
  sx.restore();
}

// ── Tune readout ─────────────────────────────────────────────────────────
const NOTES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
function renderTuner(s) {
  if (s.keyName !== undefined && s.keyName !== lastKeyName) { lastKeyName = s.keyName; renderKey(); }
  if (s.inMidi > 0 && s.targetMidi > 0) {
    const n = Math.round(s.targetMidi);
    const cents = Math.round((s.inMidi - s.targetMidi) * 100);
    $("note").innerHTML = `${NOTES[((n % 12) + 12) % 12]}<sub>${Math.floor(n / 12) - 1}</sub>`;
    $("cval").textContent = (cents >= 0 ? "+" : "") + cents + " cents";
    $("cdot").style.left = clamp(50 + cents, 2, 98) + "%";
    $("note").style.opacity = 1;
  } else {
    $("note").style.opacity = 0.35;
    $("cval").textContent = "no voice";
    $("cdot").style.left = "50%";
  }
}

// ── Meters ───────────────────────────────────────────────────────────────
const sm = { inDb: -100, outDb: -100, compGr: 0, deessGr: 0 };
function smooth(k, v, fall) { sm[k] = v > sm[k] ? v : sm[k] + (v - sm[k]) * fall; return sm[k]; }
function bar(id, p) { document.querySelector(`#${id} .f`).style.height = (clamp(p, 0, 1) * 100).toFixed(1) + "%"; }

// ── Status from C++ (30 Hz) ──────────────────────────────────────────────
if (window.__JUCE__ && window.__JUCE__.backend) {
  window.__JUCE__.backend.addEventListener("novaStatus", (s) => {
    renderListen(s);
    renderTuner(s);
    plan = { active: !!s.planActive, mud: +s.planMud || 0, box: +s.planBox || 0, pres: +s.planPresence || 0, air: +s.planAir || 0 };
    spectrum = s.spectrum || null;
    voicing = s.voicing || null;
    if (s.hostTempo !== undefined && (Math.abs((s.hostTempo || 0) - hostTempo) > 0.05 || Math.abs((s.tempo || 0) - activeTempo) > 0.05)) {
      hostTempo = s.hostTempo || 0; activeTempo = s.tempo || 120; renderTempo();
    }
    deessGr = smooth("deessGr", s.deessGr || 0, 0.25);
    const inDb = smooth("inDb", s.inDb, 0.15), outDb = smooth("outDb", s.outDb, 0.15);
    inLevel = clamp((inDb + 60) / 60, 0, 1);
    bar("m-in", (inDb + 60) / 60);
    bar("m-out", (outDb + 60) / 60);
    bar("m-ds", deessGr / 12);
    bar("m-cp", smooth("compGr", s.compGr || 0, 0.25) / 15);
    if (s.presetName !== undefined && s.presetName !== presets.current) { presets.current = s.presetName; $("pname").textContent = presetLabel(); }
  });
}
renderListen({ state: "Idle", summary: "", captured: 0 });

// ── Animation loop ───────────────────────────────────────────────────────
function frame(now) {
  const t = now / 1000;
  drawOrb(t);
  drawSpec();
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

// ── Stage scale-to-fit (JS-computed and polled, same as Nova AI Master) ──
(function () {
  let lastW = -1, lastH = -1;
  function fitStage() {
    const w = document.documentElement.clientWidth || window.innerWidth || 0;
    const h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0 || (w === lastW && h === lastH)) return;
    lastW = w; lastH = h;
    const scale = Math.min(w / 1100, h / 700);
    window.__novaVocalScale = scale;
    windowScale = Math.round(w / 1100 * 100) / 100;
    const stage = $("stage");
    if (stage) stage.style.transform = `scale(${scale})`;
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
}());
