// Nova Meter UI: Live View (graded rings) + Set Targets (draggable ranges).
// Talks to the C++ editor through JUCE native functions; when opened in a plain
// browser (no JUCE backend) it falls back to a mock so the UI can be previewed.

const inJuce = !!(window.__JUCE__ && window.__JUCE__.backend);

const native = {};
const NATIVE_NAMES = ["getState", "setTarget", "setMeterOption", "setView", "loadProfile",
                      "saveProfile", "deleteProfile", "resetMeters", "activateLicense",
                      "refBrowse", "refUploadBegin", "refUploadChunk", "refUploadEnd", "refClear", "refMakeTargets"];


// ── State ────────────────────────────────────────────────────────────────
let S = null;                    // state from getState()
const readings = { cur: [], max: [], min: [] };
const shown = [];                // ballistic display values (peaks fall back smoothly)
const PEAK_IDS = new Set([0, 1]);
const PEAK_FALL_DB_PER_FRAME = 0.7; // ~20 dB/s at 30 Hz

const $ = (id) => document.getElementById(id);
const fmt = (v, d = 1) => (v === null || v === undefined || !isFinite(v)) ? "—" : v.toFixed(d);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

function grade(v, t) {
  if (v === null || v === undefined || !isFinite(v)) return "nosig";
  if (v < t.min) return "below";
  if (v > t.max) return "over";
  return "inrange";
}
const gradeColor = (g) => `var(--${g})`;
const meterTarget = (i) => S.working.meters[S.specs[i].key];
const heldFor = (i) => meterTarget(i).hold === "lowest" ? readings.min[i] : readings.max[i];

// ── Scale-to-fit (same approach as other Nova web UIs) ────────────────────
function fit() {
  const s = Math.min(window.innerWidth / 980, window.innerHeight / 620);
  $("plugin").style.transform = `scale(${s})`;
}
window.addEventListener("resize", fit);
fit();

// ── Header ───────────────────────────────────────────────────────────────
function renderHeader() {
  const sel = $("profileSelect");
  sel.innerHTML = "";
  const groups = [["Built-in", S.profiles.filter(p => p.factory)], ["Your profiles", S.profiles.filter(p => !p.factory)]];
  let found = false;
  for (const [label, list] of groups) {
    if (!list.length) continue;
    const og = document.createElement("optgroup");
    og.label = label;
    for (const p of list) {
      const o = document.createElement("option");
      o.value = o.textContent = p.name;
      if (p.name === S.active) { o.selected = true; found = true; }
      og.appendChild(o);
    }
    sel.appendChild(og);
  }
  if (!found) {
    const o = document.createElement("option");
    o.value = o.textContent = S.active || "Custom";
    o.selected = true;
    sel.prepend(o);
  }

  const activeProfile = S.profiles.find(p => p.name === S.active);
  $("modifiedMark").textContent = S.modified ? "●" : "";
  $("saveBtn").disabled = !activeProfile || activeProfile.factory || !S.modified;
  $("deleteBtn").disabled = !activeProfile || activeProfile.factory;

  for (const b of $("viewSwitch").children)
    b.classList.toggle("active", b.dataset.view === S.view);
  for (const v of ["live", "targets", "reference"])
    $(v).classList.toggle("active", S.view === v);
}

$("viewSwitch").addEventListener("click", (e) => {
  const v = e.target.dataset?.view;
  if (!v || v === S.view) return;
  S.view = v;
  native.setView(v);
  closeMenu();
  renderAll();
});

$("profileSelect").addEventListener("change", async (e) => {
  S = await native.loadProfile(e.target.value);
  renderAll();
});

$("saveBtn").addEventListener("click", async () => {
  const r = await native.saveProfile(S.active);
  S = r.state;
  renderAll();
});

$("deleteBtn").addEventListener("click", async () => {
  if (!(await askConfirm(`Delete the profile "${S.active}"? Your current targets stay loaded.`, "DELETE"))) return;
  S = await native.deleteProfile(S.active);
  renderAll();
});

$("resetBtn").addEventListener("click", () => {
  native.resetMeters();
  readings.max = readings.max.map(() => null);
  readings.min = readings.min.map(() => null);
  shown.fill(null);
  updateCurrentView();
});

// Save As modal
function openSaveAs() {
  const base = S.profiles.find(p => p.name === S.active && !p.factory) ? S.active : "";
  $("saveName").value = base;
  $("saveErr").textContent = "";
  $("saveOverlay").classList.add("open");
  setTimeout(() => $("saveName").focus(), 0);
}
async function confirmSaveAs() {
  const name = $("saveName").value.trim();
  const exists = S.profiles.find(p => p.name === name && !p.factory);
  if (exists && name !== S.active) {
    $("saveOverlay").classList.remove("open");
    if (!(await askConfirm(`Replace your profile "${name}"?`, "REPLACE"))) { $("saveOverlay").classList.add("open"); return; }
  }
  const r = await native.saveProfile(name);
  if (r.error) { $("saveErr").textContent = r.error; return; }
  S = r.state;
  $("saveOverlay").classList.remove("open");
  renderAll();
}
$("saveAsBtn").addEventListener("click", openSaveAs);
$("saveCancel").addEventListener("click", () => $("saveOverlay").classList.remove("open"));
$("saveConfirm").addEventListener("click", confirmSaveAs);
$("saveName").addEventListener("keydown", (e) => {
  if (e.key === "Enter") confirmSaveAs();
  if (e.key === "Escape") $("saveOverlay").classList.remove("open");
});

// License gate (only when the build requires it)
async function tryUnlock() {
  const msgs = { empty: "Paste your license key first.", invalid: "That license key wasn't recognised.",
                 revoked: "This license has been revoked.", seats: "This key is already active on its maximum number of computers.",
                 network: "Couldn't reach the Nova license server. Check your connection.", error: "Activation failed. Try again." };
  $("licenseGo").disabled = true;
  $("licenseErr").textContent = "Checking…";
  const r = await native.activateLicense($("licenseKey").value);
  $("licenseGo").disabled = false;
  if (r === "ok") { S.unlocked = true; $("licenseOverlay").classList.remove("open"); return; }
  $("licenseErr").textContent = msgs[r] || msgs.error;
}
$("licenseGo").addEventListener("click", tryUnlock);
$("licenseKey").addEventListener("keydown", (e) => { if (e.key === "Enter") tryUnlock(); });

// ── Meter name menu ("change how each indicator behaves from its name") ─────
function openMenu(i, anchor) {
  const spec = S.specs[i], t = meterTarget(i), m = $("menu");
  m.innerHTML = "";
  const help = document.createElement("div");
  help.className = "help";
  help.textContent = spec.help;
  m.appendChild(help);

  const item = (label, checked, fn) => {
    const b = document.createElement("button");
    b.textContent = label;
    if (checked !== undefined) b.className = checked ? "checked" : "unchecked";
    b.onclick = () => { fn(); closeMenu(); };
    m.appendChild(b);
  };
  const sect = (txt) => { const d = document.createElement("div"); d.className = "sect"; d.textContent = txt; m.appendChild(d); };

  sect("HELD VALUE SHOWS");
  item("Highest since reset", t.hold !== "lowest", () => setOption(i, "hold", "highest"));
  item("Lowest since reset", t.hold === "lowest", () => setOption(i, "hold", "lowest"));
  sect("METER");
  item(t.visible ? "Hide this meter" : "Show this meter", undefined, () => setOption(i, "visible", !t.visible));
  item("Reset range to default", undefined, () => setRange(i, spec.defMin, spec.defMax, true));

  const host = $("plugin").getBoundingClientRect(), a = anchor.getBoundingClientRect();
  const scale = host.width / 980;
  m.classList.add("open");
  const left = clamp((a.left - host.left) / scale, 8, 980 - m.offsetWidth - 8);
  let top = (a.bottom - host.top) / scale + 4;
  if (top + m.offsetHeight > 612) top = (a.top - host.top) / scale - m.offsetHeight - 4;
  m.style.left = left + "px";
  m.style.top = top + "px";
}
function closeMenu() { $("menu").classList.remove("open"); }
document.addEventListener("pointerdown", (e) => {
  if (!$("menu").contains(e.target) && !e.target.classList.contains("name-btn")) closeMenu();
});

function nameButton(i) {
  const b = document.createElement("button");
  b.className = "name-btn";
  b.textContent = S.specs[i].name;
  b.title = "Meter options";
  b.onclick = (e) => { e.stopPropagation(); $("menu").classList.contains("open") ? closeMenu() : openMenu(i, b); };
  return b;
}

async function setOption(i, key, value) {
  meterTarget(i)[key] = value;
  S.modified = await native.setMeterOption(i, key, value);
  renderAll();
}
async function setRange(i, lo, hi, rerender) {
  const t = meterTarget(i);
  t.min = Math.min(lo, hi);
  t.max = Math.max(lo, hi);
  S.modified = await native.setTarget(i, t.min, t.max);
  renderHeader();
  if (rerender) renderAll();
}

// ── Live view ────────────────────────────────────────────────────────────
const R = 48, CX = 60, CY = 60;
function pt(f) {
  const a = (135 + 270 * f) * Math.PI / 180;
  return [CX + R * Math.cos(a), CY + R * Math.sin(a)];
}
function arcPath(f0, f1) {
  f0 = clamp(f0, 0, 1); f1 = clamp(f1, 0, 1);
  if (f1 - f0 < 0.0005) return "";
  const [x0, y0] = pt(f0), [x1, y1] = pt(f1);
  const large = (f1 - f0) * 270 > 180 ? 1 : 0;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${R} ${R} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}
const frac = (i, v) => { const s = S.specs[i]; return (v - s.scaleMin) / (s.scaleMax - s.scaleMin); };

const liveEls = [];
function renderLive() {
  const host = $("live");
  host.innerHTML = "";
  liveEls.length = 0;
  const visible = S.specs.map((_, i) => i).filter(i => meterTarget(i).visible);

  if (!visible.length) {
    host.style.gridTemplateColumns = "1fr";
    host.innerHTML = `<div class="empty-note">Every meter is hidden. Switch to SET TARGETS and turn some back on.</div>`;
    return;
  }

  const cols = visible.length <= 4 ? visible.length : Math.ceil(visible.length / 2);
  host.style.gridTemplateColumns = `repeat(${cols}, 1fr)`;

  for (const i of visible) {
    const t = meterTarget(i), spec = S.specs[i];
    const tile = document.createElement("div");
    tile.className = "tile";
    tile.appendChild(nameButton(i));

    const wrap = document.createElement("div");
    wrap.className = "ring-wrap";
    const [m0x, m0y] = pt(clamp(frac(i, t.min), 0, 1)), [m1x, m1y] = pt(clamp(frac(i, t.max), 0, 1));
    const tickIn = (x, y) => [CX + (x - CX) * 0.80, CY + (y - CY) * 0.80];
    const [a0x, a0y] = tickIn(m0x, m0y), [a1x, a1y] = tickIn(m1x, m1y);
    wrap.innerHTML = `
      <svg viewBox="0 0 120 120">
        <path class="track" d="${arcPath(0, 1)}" fill="none" stroke-width="9" stroke-linecap="round"/>
        <path class="zone" d="${arcPath(frac(i, t.min), frac(i, t.max))}" fill="none" stroke-width="9"/>
        <path class="arc" d="" fill="none" stroke-width="9" stroke-linecap="round"/>
        <line class="tick" x1="${a0x}" y1="${a0y}" x2="${m0x}" y2="${m0y}"/>
        <line class="tick" x1="${a1x}" y1="${a1y}" x2="${m1x}" y2="${m1y}"/>
      </svg>
      <div class="ring-readout"><div class="ring-value">—</div><div class="ring-unit">${spec.unit}</div></div>`;
    tile.appendChild(wrap);

    const held = document.createElement("div");
    held.className = "held";
    tile.appendChild(held);
    host.appendChild(tile);

    liveEls[i] = { arc: wrap.querySelector(".arc"), value: wrap.querySelector(".ring-value"), held };
  }
  updateLive();
}

function updateLive() {
  for (let i = 0; i < S.specs.length; i++) {
    const el = liveEls[i];
    if (!el) continue;
    const v = shown[i], t = meterTarget(i), g = grade(v, t);
    el.arc.setAttribute("d", (v === null || !isFinite(v)) ? "" : arcPath(0, Math.max(frac(i, v), 0.004)));
    el.arc.style.stroke = gradeColor(g);
    el.arc.style.color = gradeColor(g);
    el.value.textContent = fmt(v);
    el.value.style.color = g === "nosig" ? "var(--text-soft)" : gradeColor(g);
    const h = heldFor(i), hg = grade(h, t);
    el.held.innerHTML = `${t.hold === "lowest" ? "LOWEST" : "HIGHEST"} <b style="color:${hg === "nosig" ? "var(--text-main)" : gradeColor(hg)}">${fmt(h)}</b>`;
  }
}

// ── Targets view ─────────────────────────────────────────────────────────
const targetEls = [];
function renderTargets() {
  const host = $("targets");
  host.innerHTML = `<div class="col-hdr"><span></span><span>METER</span><span>DRAG THE HANDLES TO SET YOUR RANGE · SHIFT = FINE</span><span>MIN</span><span>MAX</span><span>NOW</span></div>`;
  targetEls.length = 0;

  S.specs.forEach((spec, i) => {
    const t = meterTarget(i);
    const row = document.createElement("div");
    row.className = "trow" + (t.visible ? "" : " hidden-meter");

    const eye = document.createElement("button");
    eye.className = "eye" + (t.visible ? " on" : "");
    eye.textContent = t.visible ? "◉" : "○";
    eye.title = t.visible ? "Shown in Live View (click to hide)" : "Hidden (click to show)";
    eye.onclick = () => setOption(i, "visible", !t.visible);
    row.appendChild(eye);
    row.appendChild(nameButton(i));

    const bar = document.createElement("div");
    bar.className = "bar";
    bar.innerHTML = `<div class="bar-bg"><div class="seg-below"></div><div class="seg-in"></div><div class="seg-over"></div></div>
      <div class="live-mark"></div><div class="handle" data-h="min"></div><div class="handle" data-h="max"></div>
      <span class="scale-lbl" style="left:0">${spec.scaleMin}</span><span class="scale-lbl" style="right:0">${spec.scaleMax > 0 ? "+" : ""}${spec.scaleMax}</span>`;
    row.appendChild(bar);

    const mkInput = (which) => {
      const inp = document.createElement("input");
      inp.className = "num"; inp.type = "number"; inp.step = "0.1";
      inp.addEventListener("change", () => {
        const v = parseFloat(inp.value);
        if (!isFinite(v)) { syncTargetRow(i); return; }
        const c = clamp(v, spec.scaleMin, spec.scaleMax);
        which === "min" ? setRange(i, Math.min(c, t.max), t.max) : setRange(i, t.min, Math.max(c, t.min));
        syncTargetRow(i);
      });
      return inp;
    };
    const minIn = mkInput("min"), maxIn = mkInput("max");
    row.appendChild(minIn); row.appendChild(maxIn);

    const now = document.createElement("div");
    now.className = "tread";
    row.appendChild(now);
    host.appendChild(row);

    targetEls[i] = { bar, minIn, maxIn, now };
    attachDrag(i, bar);
    syncTargetRow(i);
  });
  updateTargets();
}

function pctOf(i, v) { return clamp(frac(i, v), 0, 1) * 100; }

function syncTargetRow(i) {
  const el = targetEls[i]; if (!el) return;
  const t = meterTarget(i);
  const a = pctOf(i, t.min), b = pctOf(i, t.max);
  const [sb, si, so] = el.bar.querySelector(".bar-bg").children;
  sb.style.width = a + "%"; si.style.width = (b - a) + "%"; so.style.width = (100 - b) + "%";
  el.bar.querySelector('[data-h="min"]').style.left = a + "%";
  el.bar.querySelector('[data-h="max"]').style.left = b + "%";
  if (document.activeElement !== el.minIn) el.minIn.value = t.min.toFixed(1);
  if (document.activeElement !== el.maxIn) el.maxIn.value = t.max.toFixed(1);
}

function attachDrag(i, bar) {
  const spec = S.specs[i];
  let drag = null;

  const valueAt = (clientX) => {
    const r = bar.getBoundingClientRect();
    return spec.scaleMin + clamp((clientX - r.left) / r.width, 0, 1) * (spec.scaleMax - spec.scaleMin);
  };

  bar.addEventListener("pointerdown", (e) => {
    const t = meterTarget(i);
    let which = e.target.dataset?.h;
    if (!which) { // click on the bar: grab the nearest handle
      const v = valueAt(e.clientX);
      which = Math.abs(v - t.min) <= Math.abs(v - t.max) ? "min" : "max";
    }
    drag = { which, lastX: e.clientX, lastVal: t[which] };
    bar.setPointerCapture(e.pointerId);
    move(e);
  });

  const move = (e) => {
    if (!drag) return;
    const t = meterTarget(i);
    const r = bar.getBoundingClientRect();
    const span = spec.scaleMax - spec.scaleMin;
    let v = e.shiftKey
      ? drag.lastVal + (e.clientX - drag.lastX) / r.width * span * 0.1   // fine: 1/10 speed
      : valueAt(e.clientX);
    v = clamp(v, spec.scaleMin, spec.scaleMax);
    drag.lastX = e.clientX; drag.lastVal = v;
    v = Math.round(v * 10) / 10;
    if (drag.which === "min") t.min = Math.min(v, t.max); else t.max = Math.max(v, t.min);
    syncTargetRow(i);
    native.setTarget(i, t.min, t.max).then((m) => { S.modified = m; renderHeader(); });
  };
  bar.addEventListener("pointermove", move);
  const end = (e) => {
    if (!drag) return;
    drag = null;
    try { bar.releasePointerCapture(e.pointerId); } catch (_) {}
  };
  bar.addEventListener("pointerup", end);
  bar.addEventListener("pointercancel", end);
}

function updateTargets() {
  for (let i = 0; i < S.specs.length; i++) {
    const el = targetEls[i]; if (!el) continue;
    const v = shown[i], t = meterTarget(i), g = grade(v, t);
    const mark = el.bar.querySelector(".live-mark");
    if (v === null || !isFinite(v)) mark.style.display = "none";
    else { mark.style.display = "block"; mark.style.left = pctOf(i, v) + "%"; mark.style.background = gradeColor(g); mark.style.boxShadow = `0 0 8px ${gradeColor(g)}`; }
    el.now.textContent = fmt(v);
    el.now.style.color = g === "nosig" ? "var(--text-soft)" : gradeColor(g);
  }
}

// ── Updates from C++ ─────────────────────────────────────────────────────
window.novaMeterUpdate = (d) => {
  if (!S) return;
  readings.max = d.max; readings.min = d.min;
  for (let i = 0; i < d.cur.length; i++) {
    const v = d.cur[i];
    if (PEAK_IDS.has(i)) {
      const prev = shown[i];
      const fallen = (prev === null || prev === undefined) ? null : prev - PEAK_FALL_DB_PER_FRAME;
      const next = v === null ? fallen : (fallen === null ? v : Math.max(v, fallen));
      shown[i] = (next === null || next < S.specs[i].scaleMin) ? null : next;
    } else {
      shown[i] = v;
    }
  }
  updateCurrentView();
};

window.novaMeterStateChanged = async () => { S = await native.getState(); renderAll(); };

function renderAll() {
  renderHeader();
  liveEls.length = 0; targetEls.length = 0; refEls.length = 0;
  if (S.view === "live") renderLive();
  else if (S.view === "targets") renderTargets();
  else renderReference();
}

function updateCurrentView() {
  if (S.view === "live") updateLive();
  else if (S.view === "targets") updateTargets();
  else updateReference();
}

// ── Reference view ───────────────────────────────────────────────────────
let refStatus = { state: "idle" };
let uploading = null; // { name, progress } while a dropped file streams to the plugin
const refEls = [];

// What "your mix" is compared on, per meter (same order as the specs).
const REF_COMPARE = [
  { src: "max", tag: "HELD MAX", up: "hotter", down: "lower" },             // Peak
  { src: "max", tag: "HELD MAX", up: "hotter", down: "lower" },             // True peak
  { src: "cur", tag: "SO FAR",   up: "louder", down: "quieter" },           // LUFS int
  { src: "max", tag: "LOUDEST",  up: "louder", down: "quieter" },           // LUFS short
  { src: "cur", tag: "LIVE",     up: "louder", down: "quieter" },           // RMS
  { src: "cur", tag: "LIVE",     up: "more open", down: "more compressed" },// PSR
  { src: "cur", tag: "LIVE",     up: "denser", down: "thinner" },           // Mid density
];
const REF_TAG = ["MAX", "MAX", "", "LOUDEST", "TYPICAL", "TYPICAL", "TYPICAL"];

function fmtDuration(sec) {
  const m = Math.floor(sec / 60), s = Math.round(sec % 60);
  return `${m}:${String(s).padStart(2, "0")}`;
}

function renderRefCard() {
  const card = $("refCard");
  card.className = "ref-card";
  card.onclick = null;
  const busy = uploading || refStatus.state === "analyzing";
  const err = refStatus.state === "error" ? `<div class="ref-err">${escapeHtml(refStatus.error)}</div>` : "";

  if (busy) {
    const label = uploading ? `Loading ${escapeHtml(uploading.name)}…` : "Analysing reference…";
    const p = uploading ? uploading.progress : (refStatus.progress || 0);
    card.innerHTML = `<div class="ref-icon">◔</div><div><div class="ref-meta-label">REFERENCE</div>
      <div class="ref-name">${label}</div><div class="progress"><div style="width:${(p * 100).toFixed(1)}%"></div></div></div>`;
    return;
  }

  if (!S.reference) {
    card.classList.add("empty");
    card.innerHTML = `<div class="ref-icon">♫</div><div><div class="ref-drop-title">Drop a reference track here, or click to browse</div>
      <div class="ref-drop-sub">A record your client loves. Its loudness, peaks, dynamics and density become your comparison. WAV, AIFF, FLAC, MP3, M4A.</div>${err}</div>`;
    card.onclick = () => native.refBrowse();
    return;
  }

  const r = S.reference;
  const ch = r.channels === 1 ? "mono" : "stereo";
  card.innerHTML = `<div class="ref-icon">♫</div><div><div class="ref-meta-label">REFERENCE</div>
      <div class="ref-name" title="${escapeHtml(r.name)}">${escapeHtml(r.name)}</div>
      <div class="ref-meta">${fmtDuration(r.duration)} · ${(r.sampleRate / 1000).toFixed(1)} kHz · ${ch} · ${fmt(r.meters[2].headline)} LUFS integrated</div>${err}</div>
    <div class="ref-actions">
      <button class="btn primary" id="refUse" title="Set every meter's target range from this reference">USE AS TARGETS</button>
      <button class="btn" id="refReplace">REPLACE…</button>
      <button class="btn" id="refClearBtn">CLEAR</button>
    </div>`;
  $("refUse").onclick = async () => { S = await native.refMakeTargets(); renderAll(); };
  $("refReplace").onclick = () => native.refBrowse();
  $("refClearBtn").onclick = async () => {
    if (!(await askConfirm(`Remove the reference "${r.name}"?`, "REMOVE"))) return;
    S = await native.refClear(); renderAll();
  };
}

function renderReference() {
  renderRefCard();
  const host = $("refTable");
  refEls.length = 0;
  host.innerHTML = `<div class="ref-hdr"><span>METER</span><span>REFERENCE</span><span title="10th to 90th percentile across the body of the song">TYPICAL RANGE</span><span>YOUR MIX</span><span>DIFFERENCE</span></div>`;

  S.specs.forEach((spec, i) => {
    const r = S.reference ? S.reference.meters[i] : null;
    const row = document.createElement("div");
    row.className = "ref-row" + (S.reference ? "" : " dim");
    const range = (r && i !== 2 && r.low !== null && r.high !== null) ? `${fmt(r.low)} … ${fmt(r.high)}` : "—";
    row.innerHTML = `<span class="meter">${spec.name}</span>
      <span class="ref-val">${r ? fmt(r.headline) : "—"}<small>${r ? REF_TAG[i] : ""}</small></span>
      <span class="ref-range">${range}</span>
      <span class="ref-val mine">—</span>
      <span class="delta"></span>`;
    host.appendChild(row);
    refEls[i] = { mine: row.querySelector(".mine"), delta: row.querySelector(".delta") };
  });
  updateReference();
}

function updateReference() {
  if (refEls.length === 0) return;
  for (let i = 0; i < S.specs.length; i++) {
    const el = refEls[i]; if (!el) continue;
    const c = REF_COMPARE[i];
    const mine = c.src === "max" ? readings.max[i] : shown[i];
    el.mine.innerHTML = `${fmt(mine)}<small>${c.tag}</small>`;

    const ref = S.reference ? S.reference.meters[i].headline : null;
    if (ref === null || ref === undefined || mine === null || mine === undefined || !isFinite(mine)) {
      el.delta.innerHTML = "";
      continue;
    }
    const d = mine - ref, ad = Math.abs(d);
    const g = ad <= 1 ? "inrange" : (d < 0 ? "below" : "over");
    const unit = (i === 2 || i === 3) ? "LU" : (i === 5 ? "" : "dB");
    const words = ad < 0.3 ? "matches the reference" : `${ad.toFixed(1)}${unit ? " " + unit : ""} ${d > 0 ? c.up : c.down}`;
    el.delta.innerHTML = `<span class="pill" style="background:${gradeColor(g)}">${d >= 0 ? "+" : "−"}${ad.toFixed(1)}</span><span class="words">${words}</span>`;
  }
}

window.novaMeterRefStatus = (s) => {
  const was = refStatus.state;
  refStatus = s;
  if (S && S.view === "reference" && (s.state === "analyzing" || was !== s.state)) renderRefCard();
};

function escapeHtml(t) {
  return String(t ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

// Drag & drop: the web view can't see file paths, so the file streams to the
// plugin in base64 chunks and is analysed from a temporary copy.
const AUDIO_EXT = /\.(wav|wave|aif|aiff|flac|mp3|m4a|aac|ogg)$/i;
const CHUNK = 512 * 1024;

function readChunkBase64(blob) {
  return new Promise((resolve, reject) => {
    const fr = new FileReader();
    fr.onload = () => resolve(String(fr.result).split(",")[1] || "");
    fr.onerror = () => reject(fr.error);
    fr.readAsDataURL(blob);
  });
}

async function uploadReference(file) {
  if (uploading) return;
  if (!AUDIO_EXT.test(file.name)) {
    refStatus = { state: "error", error: "That isn't an audio file. Use WAV, AIFF, FLAC, MP3, M4A or OGG." };
    renderRefCard();
    return;
  }
  uploading = { name: file.name, progress: 0 };
  renderRefCard();
  try {
    const begin = await native.refUploadBegin(file.name, file.size);
    if (begin !== "ok") throw new Error(begin);
    for (let off = 0; off < file.size; off += CHUNK) {
      const r = await native.refUploadChunk(await readChunkBase64(file.slice(off, off + CHUNK)));
      if (r !== "ok") throw new Error(r);
      uploading.progress = Math.min(1, (off + CHUNK) / file.size);
      renderRefCard();
    }
    const end = await native.refUploadEnd();
    if (end !== "ok") throw new Error(end);
    uploading = null;
    refStatus = { state: "analyzing", progress: 0 };
  } catch (e) {
    uploading = null;
    refStatus = { state: "error", error: (e && e.message) || "Upload failed." };
  }
  renderRefCard();
}

let dragDepth = 0;
document.addEventListener("dragenter", (e) => { e.preventDefault(); if (++dragDepth === 1) $("dropHint").classList.add("on"); });
document.addEventListener("dragleave", (e) => { e.preventDefault(); if (--dragDepth <= 0) { dragDepth = 0; $("dropHint").classList.remove("on"); } });
document.addEventListener("dragover", (e) => { e.preventDefault(); if (e.dataTransfer) e.dataTransfer.dropEffect = "copy"; });
document.addEventListener("drop", async (e) => {
  e.preventDefault(); // stop the web view navigating to the file
  dragDepth = 0;
  $("dropHint").classList.remove("on");
  const file = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
  if (!file || !S) return;
  if (S.view !== "reference") { S.view = "reference"; native.setView("reference"); closeMenu(); renderAll(); }
  uploadReference(file);
});

// In-UI confirm (window.confirm is not reliably available inside plugin web views)
function askConfirm(message, okLabel = "OK") {
  return new Promise((resolve) => {
    $("confirmMsg").textContent = message;
    $("confirmOk").textContent = okLabel;
    $("confirmOverlay").classList.add("open");
    const done = (v) => { $("confirmOverlay").classList.remove("open"); $("confirmOk").onclick = $("confirmCancel").onclick = null; resolve(v); };
    $("confirmOk").onclick = () => done(true);
    $("confirmCancel").onclick = () => done(false);
  });
}

async function main() {
  if (inJuce) {
    const { getNativeFunction } = await import("./juce/index.js");
    for (const n of NATIVE_NAMES) native[n] = getNativeFunction(n);
  } else {
    Object.assign(native, createMockBackend());
  }

  S = await native.getState();
  shown.length = S.specs.length; shown.fill(null);
  renderAll();
  if (S.licenseRequired && !S.unlocked) $("licenseOverlay").classList.add("open");
}
main();

// ── Browser-preview mock (not used inside the plugin) ─────────────────────
function createMockBackend() {
  const specs = [
    { key: "peak", name: "PEAK", unit: "dBFS", scaleMin: -30, scaleMax: 3, defMin: -6, defMax: -0.3, help: "Loudest single sample. How close you are to the top." },
    { key: "truePeak", name: "TRUE PEAK", unit: "dBTP", scaleMin: -30, scaleMax: 3, defMin: -3, defMax: -1, help: "Inter-sample peaks (4x oversampled) that can clip after conversion or encoding." },
    { key: "lufsInt", name: "LUFS INT", unit: "LUFS", scaleMin: -40, scaleMax: 0, defMin: -16, defMax: -9, help: "Gated average loudness of everything played since the last reset (BS.1770)." },
    { key: "lufsShort", name: "LUFS SHORT", unit: "LUFS", scaleMin: -40, scaleMax: 0, defMin: -14, defMax: -7, help: "Loudness over a moving 3-second window." },
    { key: "rms", name: "RMS", unit: "dBFS", scaleMin: -40, scaleMax: 0, defMin: -18, defMax: -9, help: "Sustained energy over 300 ms." },
    { key: "dynRange", name: "DYN RANGE", unit: "PSR", scaleMin: 0, scaleMax: 25, defMin: 6, defMax: 12, help: "Peak-to-short-term-loudness ratio." },
    { key: "midDens", name: "MID DENSITY", unit: "dB", scaleMin: -50, scaleMax: 0, defMin: -24, defMax: -12, help: "Energy in the 250 Hz - 4 kHz band." },
  ];
  const mk = (name, factory) => ({ name, factory, meters: Object.fromEntries(specs.map(s => [s.key, { min: s.defMin, max: s.defMax, visible: true, hold: "highest" }])) });
  const store = [mk("Streaming Master", true), mk("Loud Master", true)];
  let working = structuredClone(store[0]), active = "Streaming Master", view = "live";
  const same = () => JSON.stringify(working.meters) === JSON.stringify((store.find(p => p.name === active) || {}).meters);
  const state = () => ({ reference, specs, profiles: store.map(p => ({ name: p.name, factory: p.factory })), active, modified: !same(), view, working: structuredClone(working), licenseRequired: false, unlocked: true });
  const held = { max: specs.map(() => null), min: specs.map(() => null) };
  let reference = null;
  const fakeAnalyze = (name) => {
    let p = 0;
    const iv = setInterval(() => {
      p += 0.07;
      window.novaMeterRefStatus?.({ state: "analyzing", progress: Math.min(p, 1) });
      if (p >= 1) {
        clearInterval(iv);
        reference = { name, duration: 203.4, sampleRate: 44100, channels: 2, meters: [
          { headline: -0.4, low: -3.1, high: -0.6, max: -0.4 }, { headline: 0.3, low: -2.8, high: -0.2, max: 0.3 },
          { headline: -7.6, low: -7.6, high: -7.6, max: -7.6 }, { headline: -5.9, low: -9.4, high: -6.4, max: -5.9 },
          { headline: -8.8, low: -11.9, high: -7.6, max: -6.9 }, { headline: 6.4, low: 5.1, high: 8.2, max: 10.3 },
          { headline: -13.1, low: -16.0, high: -11.4, max: -10.2 } ] };
        window.novaMeterRefStatus?.({ state: "idle" });
        window.novaMeterStateChanged?.();
      }
    }, 80);
  };

  let t = 0;
  setInterval(() => {
    t += 1 / 30;
    const base = [-1.2, -0.9, -10.5, -9 + 3 * Math.sin(t / 2), -12 + 2 * Math.sin(t / 1.3), 8 + 2 * Math.sin(t / 3), -18 + 4 * Math.sin(t / 1.7)];
    const cur = base.map((b, i) => i < 2 ? b - Math.abs(4 * Math.sin(t * 3 + i)) + Math.random() * 0.4 : b + Math.random() * 0.3);
    cur.forEach((v, i) => { held.max[i] = held.max[i] === null ? v : Math.max(held.max[i], v); held.min[i] = held.min[i] === null ? v : Math.min(held.min[i], v); });
    window.novaMeterUpdate?.({ cur, max: held.max, min: held.min });
  }, 33);

  return {
    getState: async () => state(),
    setTarget: async (i, lo, hi) => { Object.assign(working.meters[specs[i].key], { min: lo, max: hi }); return !same(); },
    setMeterOption: async (i, k, v) => { working.meters[specs[i].key][k] = v; return !same(); },
    setView: async (v) => { view = v; },
    loadProfile: async (n) => { const p = store.find(x => x.name === n); if (p) { working = structuredClone(p); active = n; } return state(); },
    saveProfile: async (n) => {
      const p = store.find(x => x.name === n);
      if (!n) return { error: "Give the profile a name.", state: state() };
      if (p && p.factory) return { error: `"${n}" is a built-in profile. Save under a new name.`, state: state() };
      const copy = structuredClone(working); copy.name = n; copy.factory = false;
      if (p) Object.assign(p, copy); else store.push(copy);
      active = n; return { state: state() };
    },
    deleteProfile: async (n) => { const i = store.findIndex(x => x.name === n && !x.factory); if (i >= 0) store.splice(i, 1); active = n + " (unsaved)"; return state(); },
    resetMeters: async () => { held.max.fill(null); held.min.fill(null); },
    activateLicense: async () => "ok",
    refBrowse: async () => fakeAnalyze("Client Favourite - Reference Mix"),
    refUploadBegin: async () => "ok",
    refUploadChunk: async () => "ok",
    refUploadEnd: async () => { fakeAnalyze("Dropped Reference"); return "ok"; },
    refClear: async () => { reference = null; return state(); },
    refMakeTargets: async () => {
      if (reference) {
        const r = reference.meters;
        const set = (k, lo, hi) => Object.assign(working.meters[k], { min: Math.round(lo * 10) / 10, max: Math.round(hi * 10) / 10 });
        set("peak", r[0].max - 2, r[0].max + 0.3); set("truePeak", r[1].max - 1.5, r[1].max + 0.3);
        set("lufsInt", r[2].headline - 1, r[2].headline + 1); set("lufsShort", r[3].low, r[3].max + 0.5);
        set("rms", r[4].low, r[4].high); set("dynRange", r[5].low, r[5].high); set("midDens", r[6].low, r[6].high);
        active = "Reference: " + reference.name;
      }
      return state();
    },
  };
}
