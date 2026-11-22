// Nova Mid UI: Push gauge, midrange focus display, output meter with a draggable
// ceiling line, soft-clip meter and clip light. Talks to C++ through JUCE native
// functions; in a plain browser it runs on a mock so the UI can be previewed.

const inJuce = !!(window.__JUCE__ && window.__JUCE__.backend);
const native = {};
const NATIVE_NAMES = ["getState", "setParam", "setProgram", "activateLicense"];

const $ = (id) => document.getElementById(id);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const RANGES = { push: [0, 20, 0], ceiling: [-24, 0, 0], output: [-12, 0, 0] };

let S = null;                       // getState()
const P = { push: 0, ceiling: 0, output: 0, bypass: false };
let program = 0;
const dragging = new Set();

// ── Scale to fit ──────────────────────────────────────────────────────────
function fit() {
  const s = Math.min(window.innerWidth / 980, window.innerHeight / 600);
  $("plugin").style.transform = `translate(-50%, -50%) scale(${s})`;
}
window.addEventListener("resize", fit);
fit();

// ── Parameter writes ──────────────────────────────────────────────────────
function send(id, value, phase) {
  P[id] = value;
  native.setParam(id, value, phase);
  render();
}

// Vertical drag helper: shift = fine, double-click = default.
function dragValue(el, id, pxPerRange, opts = {}) {
  const [lo, hi, def] = RANGES[id];
  let startY = 0, startV = 0;
  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    startY = e.clientY; startV = P[id];
    dragging.add(id);
    if (opts.absolute) {
      const v = opts.absolute(e);
      startV = v;
      send(id, v, "begin");
    } else {
      native.setParam(id, P[id], "begin");
    }
  });
  el.addEventListener("pointermove", (e) => {
    if (!dragging.has(id)) return;
    const scale = (hi - lo) / pxPerRange * (e.shiftKey ? 0.2 : 1);
    const v = Math.round(clamp(startV + (startY - e.clientY) * scale, lo, hi) * 10) / 10;
    if (v !== P[id]) send(id, v, "move");
  });
  const end = () => { if (dragging.delete(id)) native.setParam(id, P[id], "end"); };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.addEventListener("dblclick", () => send(id, def, "set"));
  el.addEventListener("wheel", (e) => {
    e.preventDefault();
    const step = (e.shiftKey ? 0.1 : 0.5) * (e.deltaY < 0 ? 1 : -1);
    send(id, Math.round(clamp(P[id] + step, lo, hi) * 10) / 10, "set");
  }, { passive: false });
}

// ── Gauge (Push) ──────────────────────────────────────────────────────────
const NS = "http://www.w3.org/2000/svg";
const A0 = 135, SWEEP = 270;
const polar = (cx, cy, r, deg) => { const a = deg * Math.PI / 180; return [cx + r * Math.cos(a), cy + r * Math.sin(a)]; };
function arcPath(cx, cy, r, a0, a1) {
  const [x0, y0] = polar(cx, cy, r, a0), [x1, y1] = polar(cx, cy, r, a1);
  return `M${x0} ${y0} A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1} ${y1}`;
}
function el(tag, attrs, parent) {
  const n = document.createElementNS(NS, tag);
  for (const k in attrs) n.setAttribute(k, attrs[k]);
  if (parent) parent.appendChild(n);
  return n;
}

function buildKnob(svg, size, opts) {
  const c = size / 2, R = opts.r;
  const defs = el("defs", {}, svg);
  const gid = "g" + Math.random().toString(36).slice(2);
  const g = el("linearGradient", { id: gid, x1: "0", y1: "1", x2: "1", y2: "0" }, defs);
  el("stop", { offset: "0%", "stop-color": "#ff8a16" }, g);
  el("stop", { offset: "100%", "stop-color": "#ffe0a0" }, g);
  const bid = "b" + gid;
  const rg = el("radialGradient", { id: bid, cx: "50%", cy: "32%", r: "70%" }, defs);
  el("stop", { offset: "0%", "stop-color": "#2a344d" }, rg);
  el("stop", { offset: "60%", "stop-color": "#0d1424" }, rg);
  el("stop", { offset: "100%", "stop-color": "#04070f" }, rg);

  if (opts.ticks) {
    for (let i = 0; i <= opts.ticks; i++) {
      const a = A0 + SWEEP * i / opts.ticks, major = i % 5 === 0;
      const [x0, y0] = polar(c, c, R + 10, a), [x1, y1] = polar(c, c, R + (major ? 20 : 15), a);
      el("line", { x1: x0, y1: y0, x2: x1, y2: y1, stroke: major ? "rgba(255,189,74,.7)" : "rgba(174,183,203,.3)", "stroke-width": major ? 2 : 1 }, svg);
      if (major && opts.labels) {
        const [lx, ly] = polar(c, c, R + 32, a);
        const t = el("text", { x: lx, y: ly + 4, "text-anchor": "middle", fill: "#6f7a92", "font-size": 11 }, svg);
        t.textContent = opts.labels(i / opts.ticks);
      }
    }
  }
  el("path", { d: arcPath(c, c, R, A0, A0 + SWEEP), stroke: "rgba(255,255,255,.07)", "stroke-width": opts.w, fill: "none", "stroke-linecap": "round" }, svg);
  const fill = el("path", { stroke: `url(#${gid})`, "stroke-width": opts.w, fill: "none", "stroke-linecap": "round",
                            style: `filter: drop-shadow(0 0 ${opts.w}px rgba(255,170,56,.55))` }, svg);
  el("circle", { cx: c, cy: c, r: opts.body, fill: `url(#${bid})`, stroke: "rgba(255,176,62,.28)", "stroke-width": 1.2 }, svg);
  el("circle", { cx: c, cy: c, r: opts.body - 6, fill: "none", stroke: "rgba(255,255,255,.05)", "stroke-width": 1 }, svg);
  const ptr = el("line", { stroke: "#ffbd4a", "stroke-width": opts.ptrW, "stroke-linecap": "round" }, svg);
  return (norm) => {
    const a = A0 + SWEEP * clamp(norm, 0, 1);
    fill.setAttribute("d", norm > 0.002 ? arcPath(c, c, R, A0, a) : "");
    const [x0, y0] = polar(c, c, opts.body - opts.ptrIn, a), [x1, y1] = polar(c, c, opts.body - 4, a);
    ptr.setAttribute("x1", x0); ptr.setAttribute("y1", y0); ptr.setAttribute("x2", x1); ptr.setAttribute("y2", y1);
  };
}

const drawGauge = buildKnob($("gaugeSvg"), 330, { r: 124, w: 10, body: 98, ptrIn: 26, ptrW: 4, ticks: 20,
                                                  labels: (f) => String(Math.round(f * 20)) });
const outSvg = el("svg", { viewBox: "0 0 84 84" }, $("outKnob"));
const drawOut = buildKnob(outSvg, 84, { r: 36, w: 4, body: 28, ptrIn: 12, ptrW: 2.5 });

// ── Midrange focus display ────────────────────────────────────────────────
function bellDb(f, gainDb, fs = 48000, f0 = 850, Q = 0.5) {
  const A = Math.pow(10, gainDb / 40), w0 = 2 * Math.PI * f0 / fs, al = Math.sin(w0) / (2 * Q), cw = Math.cos(w0);
  const b0 = 1 + al * A, b1 = -2 * cw, b2 = 1 - al * A, a0 = 1 + al / A, a1 = -2 * cw, a2 = 1 - al / A;
  const w = 2 * Math.PI * f / fs;
  const re = (c0, c1, c2) => c0 + c1 * Math.cos(w) + c2 * Math.cos(2 * w);
  const im = (c1, c2) => -(c1 * Math.sin(w) + c2 * Math.sin(2 * w));
  const num = Math.hypot(re(b0, b1, b2), im(b1, b2)), den = Math.hypot(re(a0, a1, a2), im(a1, a2));
  return 20 * Math.log10(num / den);
}
function drawEq() {
  const cv = $("eq"), r = cv.getBoundingClientRect(), dpr = 2;
  const W = Math.max(10, Math.round(cv.clientWidth)), H = Math.max(10, Math.round(cv.clientHeight));
  if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
  const g = cv.getContext("2d");
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, W, H);
  const fx = (f) => (Math.log10(f) - Math.log10(30)) / (Math.log10(20000) - Math.log10(30)) * W;
  const y = (db) => H - 8 - (db + 1) / 22 * (H - 16);
  // grid
  g.strokeStyle = "rgba(174,183,203,.07)"; g.lineWidth = 1;
  for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) { g.beginPath(); g.moveTo(fx(f), 0); g.lineTo(fx(f), H); g.stroke(); }
  for (const d of [0, 5, 10, 15, 20]) { g.beginPath(); g.moveTo(0, y(d)); g.lineTo(W, y(d)); g.stroke(); }
  // zone shading
  const zg = g.createLinearGradient(fx(200), 0, fx(4000), 0);
  zg.addColorStop(0, "rgba(255,170,56,0)"); zg.addColorStop(.5, "rgba(255,170,56,.08)"); zg.addColorStop(1, "rgba(255,170,56,0)");
  g.fillStyle = zg; g.fillRect(fx(200), 0, fx(4000) - fx(200), H);
  // curve
  const gain = P.bypass ? 0 : P.push;
  g.beginPath();
  for (let x = 0; x <= W; x++) {
    const f = Math.pow(10, Math.log10(30) + x / W * (Math.log10(20000) - Math.log10(30)));
    const yy = y(bellDb(f, gain));
    x ? g.lineTo(x, yy) : g.moveTo(x, yy);
  }
  const line = new Path2D(); // reuse for fill
  g.lineWidth = 2.2; g.strokeStyle = "#ffbd4a"; g.shadowColor = "rgba(255,170,56,.7)"; g.shadowBlur = 8; g.stroke();
  g.shadowBlur = 0;
  g.lineTo(W, y(0)); g.lineTo(0, y(0)); g.closePath();
  const fg = g.createLinearGradient(0, y(20), 0, y(0));
  fg.addColorStop(0, "rgba(255,138,22,.35)"); fg.addColorStop(1, "rgba(255,138,22,0)");
  g.fillStyle = fg; g.fill();
  void line; void r;
}

(function buildAxis() {
  const pos = (f) => (Math.log10(f) - Math.log10(30)) / (Math.log10(20000) - Math.log10(30)) * 100;
  for (const [f, t] of [[50, "50"], [200, "200"], [1000, "1k"], [5000, "5k"], [15000, "15k"]]) {
    const s = document.createElement("span");
    s.textContent = t; s.style.left = `${pos(f)}%`;
    $("axis").appendChild(s);
  }
})();

// ── Meters ────────────────────────────────────────────────────────────────
const M_LO = -36;
const meterY = (db) => clamp((db - M_LO) / -M_LO, 0, 1); // 0 bottom .. 1 top
(function buildScale() {
  const s = $("scale");
  for (const d of [0, -3, -6, -12, -18, -24, -30, -36]) {
    const t = document.createElement("span");
    t.textContent = d; t.style.top = `${(1 - meterY(d)) * 100}%`;
    s.appendChild(t);
  }
})();
const shown = { in: M_LO, out: M_LO, sc: 0 };
let holdDb = M_LO, holdUntil = 0, clipUntil = 0, peakText = -Infinity, peakTextUntil = 0;

function onMeters(d) {
  const now = performance.now();
  const inDb = d.in == null ? -120 : d.in, outDb = d.out == null ? -120 : d.out;
  shown.in  = Math.max(inDb,  shown.in  - 0.7);
  shown.out = Math.max(outDb, shown.out - 0.7);
  shown.sc  = Math.max(d.over || 0, shown.sc - 0.35);
  if (outDb >= holdDb || now > holdUntil) { holdDb = outDb; holdUntil = now + 1200; }
  if (outDb >= peakText || now > peakTextUntil) { peakText = outDb; peakTextUntil = now + 1500; }
  if ((d.over || 0) > 0.1) clipUntil = now + 250;

  $("inFill").style.height = `${meterY(shown.in) * 100}%`;
  $("outFill").style.height = `${meterY(shown.out) * 100}%`;
  const hold = $("outHold");
  hold.style.display = holdDb > M_LO ? "block" : "none";
  hold.style.bottom = `calc(${meterY(holdDb) * 100}% - 1px)`;
  $("scFill").style.height = `${clamp(shown.sc / 12, 0, 1) * 100}%`;
  $("scVal").textContent = shown.sc.toFixed(1);
  $("peakVal").textContent = peakText > -90 ? peakText.toFixed(1) : "-inf";
  $("clipLed").classList.toggle("on", now < clipUntil);
  $("hero").style.setProperty("--heat", (0.04 + clamp(shown.sc / 12, 0, 1) * 0.22).toFixed(3));
}

// ── Render controls ───────────────────────────────────────────────────────
function presetMatches(p) {
  return Math.abs(p.push - P.push) < 0.05 && Math.abs(p.ceiling - P.ceiling) < 0.05 && Math.abs(p.output - P.output) < 0.05;
}
function render() {
  drawGauge(P.push / 20);
  $("pushVal").textContent = (P.push >= 0 ? "+" : "") + P.push.toFixed(1);
  drawOut((P.output + 12) / 12);
  $("outVal").textContent = `${P.output.toFixed(1)} dB`;
  $("ceil").style.top = `${(1 - meterY(P.ceiling)) * 100}%`;
  $("ceilVal").textContent = P.ceiling.toFixed(1);
  $("power").classList.toggle("off", P.bypass);
  $("plugin").classList.toggle("bypassed", P.bypass);
  if (S) {
    const p = S.presets[program];
    $("pnameText").textContent = p ? p.name : "Custom";
    $("pname").classList.toggle("modified", !!p && !presetMatches(p));
    [...$("plist").children].forEach((n, i) => n.classList.toggle("on", i === program));
  }
  drawEq();
}

// ── Presets ───────────────────────────────────────────────────────────────
async function loadProgram(i) {
  const n = S.presets.length;
  program = ((i % n) + n) % n;
  const st = await native.setProgram(program);
  if (st) Object.assign(P, { push: st.push, ceiling: st.ceiling, output: st.output });
  render();
}
function buildPresetList() {
  const list = $("plist");
  list.innerHTML = "";
  S.presets.forEach((p, i) => {
    const d = document.createElement("div");
    d.innerHTML = `<span>${p.name}</span><small>+${p.push.toFixed(0)} / ${p.ceiling.toFixed(1)}</small>`;
    d.onclick = (e) => { e.stopPropagation(); list.classList.remove("open"); loadProgram(i); };
    list.appendChild(d);
  });
}

// ── Wire up ───────────────────────────────────────────────────────────────
function wire() {
  dragValue($("gauge"), "push", 260);
  dragValue($("outKnob"), "output", 200);
  dragValue($("ceilStat"), "ceiling", 200);
  const wrap = $("outWrap");
  const ceilFromY = (e) => {
    const r = wrap.getBoundingClientRect();
    const f = 1 - clamp((e.clientY - r.top) / r.height, 0, 1);
    return Math.round(clamp(M_LO + f * -M_LO, -24, 0) * 10) / 10;
  };
  // Ceiling line: grab it and drag (absolute position on the meter).
  const ceil = $("ceil");
  ceil.addEventListener("pointerdown", (e) => {
    e.preventDefault(); e.stopPropagation();
    ceil.setPointerCapture(e.pointerId);
    dragging.add("ceiling");
    native.setParam("ceiling", P.ceiling, "begin");
  });
  ceil.addEventListener("pointermove", (e) => {
    if (!dragging.has("ceiling")) return;
    const v = ceilFromY(e);
    if (v !== P.ceiling) send("ceiling", v, "move");
  });
  const endCeil = () => { if (dragging.delete("ceiling")) native.setParam("ceiling", P.ceiling, "end"); };
  ceil.addEventListener("pointerup", endCeil);
  ceil.addEventListener("pointercancel", endCeil);
  ceil.addEventListener("dblclick", () => send("ceiling", 0, "set"));
  wrap.addEventListener("pointerdown", (e) => { if (e.target !== ceil) send("ceiling", ceilFromY(e), "set"); });

  $("power").onclick = () => send("bypass", !P.bypass, "set");
  $("prev").onclick = () => loadProgram(program - 1);
  $("next").onclick = () => loadProgram(program + 1);
  $("pname").onclick = () => $("plist").classList.toggle("open");
  document.addEventListener("pointerdown", (e) => { if (!$("pname").contains(e.target)) $("plist").classList.remove("open"); });

  $("licGo").onclick = async () => {
    const r = await native.activateLicense($("licKey").value);
    const msgs = { ok: "", invalid: "That key isn't valid.", revoked: "That key has been revoked.", seats: "That key is already active on 2 computers.",
                   network: "Couldn't reach the license server. Check your connection.", empty: "Enter your key.", error: "Something went wrong. Try again." };
    if (r === "ok") $("licenseOverlay").classList.remove("open");
    else $("licMsg").textContent = msgs[r] || msgs.error;
  };
  window.addEventListener("resize", drawEq);
}

window.novaMidUpdate = (d) => {
  for (const k of ["push", "ceiling", "output"]) if (!dragging.has(k) && typeof d[k] === "number") P[k] = d[k];
  P.bypass = !!d.bypass;
  if (typeof d.program === "number") program = d.program;
  onMeters(d);
  render();
};

async function main() {
  if (inJuce) {
    const { getNativeFunction } = await import("./juce/index.js");
    for (const n of NATIVE_NAMES) native[n] = getNativeFunction(n);
  } else {
    Object.assign(native, createMock());
  }
  S = await native.getState();
  Object.assign(P, { push: S.push, ceiling: S.ceiling, output: S.output, bypass: S.bypass });
  program = S.program;
  buildPresetList();
  wire();
  render();
  if (S.licenseRequired && !S.unlocked) $("licenseOverlay").classList.add("open");
}
main();

// ── Browser-preview mock (not used inside the plugin) ─────────────────────
function createMock() {
  const presets = [
    ["Default", 0, 0], ["Vocal Weight", 6, -3], ["Thick Lead Vocal", 9, -6], ["Rap Vocal Forward", 8, -4.5],
    ["808 Body", 10, -4], ["Kick and Snare Mass", 7, -5], ["Drum Bus Density", 6.5, -4], ["Loop Glue", 5, -2],
    ["Guitar Chunk", 8, -3.5], ["Mix Bus Weight", 3, -1], ["Heavy Hitter", 14, -8], ["Full Weight", 20, -12],
  ].map(([name, push, ceiling]) => ({ name, push, ceiling, output: 0 }));
  const st = { push: 9, ceiling: -6, output: 0, bypass: false, program: 2 };
  const q = new URLSearchParams(location.search);
  for (const k of ["push", "ceiling", "output"]) if (q.has(k)) st[k] = parseFloat(q.get(k));
  let t = 0;
  setInterval(() => {
    t += 1 / 30;
    const beat = Math.pow(Math.max(0, Math.sin(t * Math.PI * 2 * 1.5)), 6);
    const inDb = -14 + beat * 10 + Math.random() * 2;
    const driven = inDb + (st.bypass ? 0 : st.push * 0.75);
    const over = st.bypass ? 0 : Math.max(0, driven - st.ceiling);
    const out = (st.bypass ? inDb : Math.min(driven, st.ceiling) - over * 0.02) + st.output;
    window.novaMidUpdate({ in: inDb, out, over, ...st });
  }, 1000 / 30);
  return {
    getState: async () => ({ presets, ...st, licenseRequired: false, unlocked: true }),
    setParam: async (id, v) => { st[id] = v; },
    setProgram: async (i) => { st.program = i; Object.assign(st, { push: presets[i].push, ceiling: presets[i].ceiling, output: 0 }); return { ...st }; },
    activateLicense: async () => "ok",
  };
}
