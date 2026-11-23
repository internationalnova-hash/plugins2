// Nova Link UI
const $ = (id) => document.getElementById(id);
const plugin = $("plugin");
const NATIVE = ["getState", "setName", "host", "join", "leave", "copyCode", "chat", "marker", "setSections", "talk",
                "setRecordEnabled", "setHearSelf", "setLossless", "setSendClick", "setMonitor", "camera", "dragTake", "revealTake", "playTake", "deleteTake"];
const native = {};
let state = null;
let sections = [];
let lastRemoteFrame = -1;
let selfTick = 0;

// ── scale to the window ─────────────────────────────────────────────
function fit() {
  const s = Math.min(innerWidth / 1140, innerHeight / 720);
  plugin.style.transform = `translate(-50%, -50%) scale(${s})`;
}
addEventListener("resize", fit); fit();

// ── helpers ─────────────────────────────────────────────────────────
const toDb = (g) => (g > 1e-6 ? 20 * Math.log10(g) : -100);
const meterPct = (g) => Math.max(0, Math.min(100, (toDb(g) + 60) / 60 * 100));
const esc = (s) => String(s ?? "").replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
function setSwitch(el, on) { el.classList.toggle("on", !!on); }
function barOf(sec, pos) {
  const beatsPerBar = pos.num * 4 / pos.den;
  return sec * pos.bpm / 60 / beatsPerBar;     // 0-based bars (constant tempo)
}
function secOfBar(bar, pos) {
  const beatsPerBar = pos.num * 4 / pos.den;
  return bar * beatsPerBar * 60 / pos.bpm;
}

// ── session strip (the host's timeline) ─────────────────────────────
function renderStrip() {
  const pos = state.pos;
  const nowBar = barOf(pos.sec, pos);
  let lastBar = nowBar + 8;
  for (const t of state.takes) lastBar = Math.max(lastBar, barOf(t.start + t.length, pos) + 2);
  for (const s of sections) lastBar = Math.max(lastBar, barOf(s.sec, pos) + 4);
  const total = Math.max(32, Math.ceil(lastBar / 8) * 8);
  const pct = (bar) => (bar / total) * 100;

  const ruler = $("ruler");
  let r = "";
  const step = total <= 32 ? 4 : total <= 64 ? 8 : 16;
  for (let b = 0; b < total; b += step) r += `<span style="left:${pct(b)}%">${b + 1}</span>`;
  ruler.innerHTML = r;

  let l = "";
  for (let b = 0; b < total; b += step) l += `<div class="barline" style="left:${pct(b)}%"></div>`;
  for (const s of sections) l += `<div class="sec" style="left:${pct(barOf(s.sec, pos))}%">${esc(s.name).toUpperCase()}</div>`;
  for (const t of state.takes) {
    const a = barOf(t.start, pos), w = Math.max(0.6, barOf(t.length || 0, pos));
    const live = t.state === "recording";
    l += `<div class="tk${live ? " live" : ""}" style="left:${pct(a)}%;width:${pct(live ? Math.max(w, nowBar - a) : w)}%">${esc(t.name)}${live ? " · recording" : ""}</div>`;
  }
  for (const c of state.chat) if (c.marker >= 0) l += `<div class="mk" style="left:${pct(barOf(c.marker, pos))}%" title="${esc(c.text)}"></div>`;
  l += `<div class="ph" style="left:${pct(nowBar)}%"></div>`;
  $("lane").innerHTML = l;

  const bar = Math.floor(nowBar) + 1, beat = Math.floor((nowBar % 1) * pos.num) + 1;
  $("pos").textContent = `BAR ${bar} · ${beat} · ${Math.round(pos.bpm * 10) / 10} BPM`;
  $("lane").dataset.total = total;
}

// Host: click the strip to name a section starting at that bar.
$("lane").addEventListener("click", (e) => {
  if (!state || state.role !== "host") return;
  const rect = e.currentTarget.getBoundingClientRect();
  const total = +e.currentTarget.dataset.total || 32;
  const bar = Math.max(0, Math.round(((e.clientX - rect.left) / rect.width) * total));
  const existing = sections.findIndex((s) => Math.abs(barOf(s.sec, state.pos) - bar) < 0.5);
  const name = prompt(existing >= 0 ? `Rename the section at bar ${bar + 1} (empty to remove):` : `Name the section starting at bar ${bar + 1} (e.g. Hook):`,
                      existing >= 0 ? sections[existing].name : "");
  if (name === null) return;
  if (existing >= 0) sections.splice(existing, 1);
  if (name.trim()) sections.push({ sec: secOfBar(bar, state.pos), name: name.trim() });
  sections.sort((a, b) => a.sec - b.sec);
  native.setSections(sections);
});

// ── takes ───────────────────────────────────────────────────────────
function renderTakes() {
  const el = $("takes");
  if (!state.takes.length) { el.innerHTML = `<div class="empty">${state.role === "host" ? "Hit record in your DAW — takes from the artist appear here." : "Takes appear here when the engineer records."}</div>`; return; }
  el.innerHTML = state.takes.slice().reverse().slice(0, 5).map((t) => {
    const bar = Math.floor(barOf(t.start, state.pos)) + 1;
    const len = t.length ? `${Math.floor(t.length / 60)}:${String(Math.round(t.length % 60)).padStart(2, "0")}` : "";
    const stTxt = { recording: "Rec", transferring: `${Math.round(t.progress * 100)}%`, waiting: "Waiting", ready: "Ready", sending: `${Math.round(t.progress * 100)}%`, sent: "Sent" }[t.state] || t.state;
    const cls = { recording: "rec", transferring: "go", sending: "go", waiting: "go", ready: "ok", sent: "ok" }[t.state] || "";
    const usable = state.role === "host" && (t.state === "ready" || t.state === "waiting");
    const acts = usable
      ? `<button data-play="${t.id}" class="${t.playing ? "on" : ""}" title="${t.playing ? "This take plays over its part of the song" : "Play this take instead of the one over the same spot"}">${t.playing ? "PLAYING" : "PLAY"}</button>`
        + `<button data-drag="${t.id}" title="Drag into your DAW (lined up from 0:00)">DRAG</button>`
        + `<button data-del="${t.id}" title="Delete this take">✕</button>` : "";
    return `<div class="take"><span class="n">${esc(t.name)}</span><span class="m">Bar ${bar}${len ? " · " + len : ""}</span>
      <div class="bar"><i style="width:${Math.round((t.state === "recording" ? 0 : t.progress) * 100)}%"></i></div><span class="st ${cls}">${stTxt}</span><span class="acts">${acts}</span></div>`;
  }).join("");
}
$("takes").addEventListener("mousedown", (e) => {
  const d = e.target.closest("[data-drag]"), p = e.target.closest("[data-play]"), x = e.target.closest("[data-del]");
  if (d) native.dragTake(+d.dataset.drag);
  if (p) native.playTake(+p.dataset.play);
  if (x) {
    const t = state.takes.find((k) => k.id === +x.dataset.del);
    if (t && confirm(`Delete ${t.name}? Its files go to the Trash on both computers.`)) native.deleteTake(t.id);
  }
});

// ── chat ────────────────────────────────────────────────────────────
let lastChatCount = -1;
function renderChat() {
  if (state.chat.length === lastChatCount) return;
  lastChatCount = state.chat.length;
  $("msgs").innerHTML = state.chat.slice(-6).map((c) => {
    if (c.marker >= 0) {
      const bar = Math.floor(barOf(c.marker, state.pos)) + 1;
      return `<div class="msg"><b>◆ Marker</b><span>Bar ${bar} — ${esc(c.text)}</span><span class="mk">${c.take ? "TAKE " + c.take : ""}</span></div>`;
    }
    return `<div class="msg${c.mine ? "" : " them"}"><b>${c.mine ? "You" : esc(c.from)}</b><span>${esc(c.text)}</span></div>`;
  }).join("") || `<div class="empty">Say hi — messages and markers appear here.</div>`;
}
function sendChat() { const v = $("chatIn").value.trim(); if (!v) return; native.chat(v); $("chatIn").value = ""; }
$("send").onclick = sendChat;
$("chatIn").addEventListener("keydown", (e) => { if (e.key === "Enter") sendChat(); });
$("addMarker").onclick = () => { const t = prompt("Marker note (e.g. redo 'tonight'):", ""); if (t !== null) native.marker(t || "Marker"); };

// ── update from the plugin ──────────────────────────────────────────
window.novaLinkUpdate = (s) => {
  state = s;
  const role = s.role;
  plugin.classList.toggle("idle", role === "none");
  plugin.classList.toggle("artist", role === "artist");
  if (role === "none") return;

  sections = s.sections || [];
  const connected = s.state === "Connected";
  $("dot").classList.toggle("wait", !connected);
  $("sstate").textContent = connected ? "Connected" : s.state === "Failed" ? "Connection lost" : role === "host" ? "Waiting for artist" : "Connecting";
  $("code").textContent = s.code;
  $("peer").textContent = s.peer || (role === "host" ? "Waiting for the artist to join…" : "Connecting…");
  $("delay").textContent = connected ? `${Math.round(s.rtt / 2 + (s.buffer || 0))} ms` : "–";
  $("jit").textContent = s.jitter != null ? `${s.jitter.toFixed(1)} ms` : "–";
  $("loss").textContent = s.loss != null ? `${s.loss.toFixed(1)}%` : "–";
  $("rtt").textContent = connected ? `${Math.round(s.rtt)} ms` : "–";

  const recording = s.activeTake > 0;
  plugin.classList.toggle("recording", recording);
  $("recLabel").textContent = recording ? `RECORDING TAKE ${s.activeTake}` : "RECORDING";

  // meters: host shows what comes back from the artist; artist shows their own mic
  const pk = role === "host" ? s.recvPeak : s.sendPeak;
  $("m1").style.height = meterPct(pk[0]) + "%";
  $("m2").style.height = meterPct(pk[1]) + "%";
  const db = toDb(Math.max(pk[0], pk[1]));
  $("lvl").textContent = db > -90 ? `${db.toFixed(1)} dB` : "–";
  setSwitch($("swLossless"), s.lossless);
  setSwitch($("swRecord"), s.recordEnabled);
  setSwitch($("swSelf"), s.hearSelf);
  setSwitch($("swClick"), s.sendClick);
  $("talk").classList.toggle("held", s.talkback);

  // video
  const screen = $("screen");
  if (s.remoteFrame !== lastRemoteFrame && s.remoteFrame > 0) {
    lastRemoteFrame = s.remoteFrame;
    $("remoteImg").src = `video/remote.jpg?n=${s.remoteFrame}`;
    screen.classList.add("has-video");
  }
  if (s.remoteFps < 0.5) screen.classList.remove("has-video");
  $("fps").textContent = s.remoteFps >= 0.5 ? `${Math.round(s.remoteFps)} fps` : "off";
  $("pip").classList.toggle("has-video", s.camera);
  if (s.camera) $("selfImg").src = `video/self.jpg?n=${++selfTick}`;
  $("cam").textContent = s.camera ? "Camera off" : "Camera on";
  $("novideo").textContent = s.videoError || (!s.hostAllowsCamera ? "This DAW doesn't allow plugin cameras — audio and takes still work." : "");

  renderStrip();
  renderTakes();
  renderChat();
};

// ── controls ────────────────────────────────────────────────────────
const toggles = { swLossless: "setLossless", swRecord: "setRecordEnabled", swSelf: "setHearSelf", swClick: "setSendClick" };
for (const [id, fn] of Object.entries(toggles))
  $(id).onclick = (e) => { const on = !e.currentTarget.classList.contains("on"); setSwitch(e.currentTarget, on); native[fn](on); };
$("mon").oninput = (e) => {
  const v = +e.target.value;
  $("monTxt").textContent = v <= -60 ? "Off" : `${v.toFixed(1)} dB`;
  e.target.style.setProperty("--p", `${(v + 60) / 66 * 100}%`);
  native.setMonitor(v);
};
let latched = false;
$("talk").onmousedown = () => { if (!latched) native.talk(true); };
$("talk").onmouseup = $("talk").onmouseleave = () => { if (!latched) native.talk(false); };
$("talk").ondblclick = () => { latched = !latched; native.talk(latched); $("talkHint").textContent = latched ? "LATCHED · DOUBLE-CLICK TO RELEASE" : "HOLD TO TALK · DOUBLE-CLICK TO LATCH"; };
$("raise").onclick = () => $("chatIn").focus();
$("cam").onclick = async () => { const err = await native.camera(!(state && state.camera)); if (err) $("novideo").textContent = err; };
$("copy").onclick = () => { native.copyCode(); $("copy").textContent = "Copied"; setTimeout(() => ($("copy").textContent = "Copy code"), 1500); };
$("leave").onclick = () => { if (confirm("Leave the session?")) { native.leave(); lastChatCount = -1; } };
$("nameIn").onchange = (e) => native.setName(e.target.value);
$("startBtn").onclick = async () => { await native.setName($("nameIn").value); await native.host(); };
$("joinBtn").onclick = async () => {
  await native.setName($("nameIn").value);
  const ok = await native.join($("codeIn").value);
  $("joinErr").textContent = ok ? "" : "That code doesn't look right — it's like NOVA-7K2Q-9MX4.";
};
$("codeIn").addEventListener("keydown", (e) => { if (e.key === "Enter") $("joinBtn").click(); });

// ── start ───────────────────────────────────────────────────────────
(async () => {
  if (!window.__JUCE__) {
    // Browser preview only (no plugin): sample data so the layout can be checked.
    const demo = { role: "host", code: "NOVA-7K2Q-9MX4", state: "Connected", peer: "Marcus", myName: "Studio A", rtt: 62, jitter: 4.2, loss: 0.1, buffer: 40,
      sendPeak: [0.4, 0.4], recvPeak: [0.3, 0.28], activeTake: 3, lossless: true, recordEnabled: true, hearSelf: true, sendClick: true, talkback: false,
      pos: { sec: 66, ppq: 132, bpm: 120, num: 4, den: 4, playing: true, rec: true },
      takes: [ { id: 1, name: "Take 1", start: 16, length: 30, state: "ready", progress: 1, playing: true }, { id: 2, name: "Take 2", start: 48, length: 14, state: "ready", progress: 1, playing: false }, { id: 4, name: "Take 4", start: 48, length: 14, state: "transferring", progress: 0.62 },
               { id: 3, name: "Take 3", start: 64, length: 0, state: "recording", progress: 0 } ],
      chat: [ { from: "Marcus", text: "More beat in my cans please", marker: -1 }, { from: "Studio A", text: "Done. Going from the hook.", marker: -1, mine: true },
              { from: "Studio A", text: "redo 'tonight'", marker: 64.5, take: 3, mine: true } ],
      sections: [ { sec: 0, name: "Intro" }, { sec: 16, name: "Verse 1" }, { sec: 48, name: "Hook" } ],
      camera: false, remoteFps: 0, remoteFrame: 0, hostAllowsCamera: true, videoError: "" };
    for (const n of NATIVE) native[n] = async () => true;
    native.getState = async () => demo;
    window.novaLinkUpdate(demo);
    setInterval(() => { demo.pos.sec += 0.1; window.novaLinkUpdate(demo); }, 100);
    return;
  }
  const { getNativeFunction } = await import("./juce/index.js");
  for (const n of NATIVE) native[n] = getNativeFunction(n);
  const s = await native.getState();
  $("nameIn").value = s.myName || "";
  window.novaLinkUpdate(s);
})();
