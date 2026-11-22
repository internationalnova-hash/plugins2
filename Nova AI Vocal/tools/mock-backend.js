// Fake JUCE backend for previewing the UI in a normal browser (not shipped).
// Answers slider/combo/toggle sync, native functions and emits a fake
// "novaStatus" at 30 Hz. Click the orb to walk Listen -> Mixed.
(function () {
  const SL = { clean: [0, 1, .6], deess: [0, 1, .55], punch: [0, 1, .7], air: [0, 1, .75], warmth: [0, 1, .25], space: [0, 1, .55], tune: [0, 1, .95], mix: [0, 1, 1], output: [-12, 12, 0], bpm: [40, 240, 120] };
  const VIBES = ["Reggaeton Bright", "Melodic Trap", "R&B Smooth", "Pop Polished", "Drill Dark", "Raw Natural"];
  const MAC = [[.6,.55,.7,.75,.25,.55,.95],[.65,.5,.75,.7,.3,.6,1],[.5,.5,.55,.6,.45,.65,.55],[.6,.6,.65,.65,.3,.4,.7],[.7,.45,.85,.35,.6,.3,.45],[.4,.4,.35,.4,.15,.15,0]];
  const KEYS = ["Auto (Listen)", "Chromatic"]; ["C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"].forEach(n => KEYS.push(n + " Major", n + " Minor"));
  const val = {}; Object.keys(SL).forEach(k => val[k] = SL[k][2]);
  const combo = { vibe: 0, key: 0 }, combos = { vibe: VIBES, key: KEYS };
  let toggles = { bypass: false, tempoSync: true }, bypass = false, presets = ["Lead Vox", "Hook Doubles"], current = "";
  let ls = "Idle", t0 = 0, summary = "";
  const send = (id, obj) => setTimeout(() => window.__JUCE__.backend.emitByBackend(id, JSON.stringify(obj)), 0);
  const sliderOut = k => send("__juce__slider" + k, { eventType: "valueChanged", value: val[k] });
  const FACT = [["Puerto Rico","LATIN"],["Odisea Nights","LATIN"],["Casablanca Bronx","MELODIC TRAP"],["A Weekend in Canada","R&B"],["Barbados Gold","POP"],["Woo Back Baby","DRILL"]];
  const plist = () => ({ names: ["Default", ...FACT.map(f => f[0]), ...presets], groups: ["", ...FACT.map(f => f[1]), ...presets.map(() => "USER")], current });
  function applyVibe() { ["clean","deess","punch","air","warmth","space","tune"].forEach((k, i) => { val[k] = MAC[combo.vibe][i]; sliderOut(k); }); }
  const fns = {
    toggleListen() { if (ls === "Listening") { ls = "Analyzing"; setTimeout(() => { ls = "Mixed"; summary = "bright top | strong S's | wide dynamics | low-mid build-up"; }, 900); } else { ls = "Listening"; t0 = performance.now(); } },
    getPresets: plist,
    loadPreset(n) { current = n === "Default" ? "" : n; return plist(); },
    savePreset(n) { if (!presets.includes(n)) presets.push(n); current = n; return Object.assign(plist(), { ok: true }); },
    deletePreset(n) { presets = presets.filter(p => p !== n); if (current === n) current = ""; return plist(); },
    revealPresets() {}, reapplyVibe() { applyVibe(); }, forgetVoice() { ls = "Idle"; summary = ""; applyVibe(); },
    setWindowScale(s) { console.log("setWindowScale", s); },
  };
  window.__JUCE__ = {
    initialisationData: { __juce__platform: [], __juce__functions: Object.keys(fns), __juce__registeredGlobalEventIds: [],
      __juce__sliders: Object.keys(SL), __juce__toggles: ["bypass", "tempoSync"], __juce__comboBoxes: ["vibe", "key"] },
    postMessage(json) {
      const { eventId, payload } = JSON.parse(json);
      if (eventId === "__juce__invoke") { const r = fns[payload.name] ? fns[payload.name](...payload.params) : null; send("__juce__complete", { promiseId: payload.resultId, result: r === undefined ? null : r }); return; }
      if (eventId.startsWith("__juce__slider")) { const k = eventId.slice(14);
        if (payload.eventType === "requestInitialUpdate") { send(eventId, { eventType: "propertiesChanged", start: SL[k][0], end: SL[k][1], skew: 1, interval: 0, name: k, label: "", numSteps: 1000, parameterIndex: 0 }); sliderOut(k); }
        if (payload.eventType === "valueChanged") val[k] = payload.value; return; }
      if (eventId.startsWith("__juce__comboBox")) { const k = eventId.slice(16), n = combos[k].length;
        if (payload.eventType === "requestInitialUpdate") { send(eventId, { eventType: "propertiesChanged", name: k, parameterIndex: 0, choices: combos[k] }); send(eventId, { eventType: "valueChanged", value: combo[k] / (n - 1) }); }
        if (payload.eventType === "valueChanged") { const i = Math.round(payload.value * (n - 1)); if (k === "vibe" && i !== combo.vibe) { combo.vibe = i; applyVibe(); } else combo[k] = i; } return; }
      if (eventId.startsWith("__juce__toggle")) { const k = eventId.slice(14); if (payload.eventType === "requestInitialUpdate") send(eventId, { eventType: "valueChanged", value: toggles[k] }); else toggles[k] = payload.value; }
    },
  };
  const SCALE = [7, 9, 11, 0, 2, 4, 6]; let note = 55, nt = 0;
  setInterval(() => {
    const t = performance.now() / 1000, on = ls !== "Idle" || true;
    if (t - nt > .9) { nt = t; note = 48 + SCALE[(Math.random() * 7) | 0] + 12 * (Math.random() < .5); }
    const lvl = -16 + Math.sin(t * 7) * 5 + Math.random() * 4;
    const spectrum = Array.from({ length: 96 }, (_, i) => { const f = 30 * Math.pow(20000 / 30, i / 96);
      return -40 + 22 * Math.exp(-Math.pow(Math.log10(f / 400) / .4, 2)) - (f > 3000 ? 14 * Math.log10(f / 3000) : 0) - (f < 90 ? 20 : 0) + (Math.random() - .5) * 8; });
    const st = { state: ls, summary, captured: ls === "Listening" ? (performance.now() - t0) / 1000 : 0,
      inDb: lvl, outDb: lvl + 4, compGr: 2 + 4 * Math.abs(Math.sin(t * 4)), deessGr: Math.max(0, 6 * Math.random() - 2),
      inMidi: note + (Math.random() - .5) * .3 * (1 - val.tune), targetMidi: note, keyName: "G major (auto)", presetName: current, hostTempo: 96, tempo: toggles.tempoSync ? 96 : val.bpm,
      planActive: ls === "Mixed", planMud: -3, planBox: -1.5, planPresence: 1, planAir: 1.5, spectrum };
    if (window.__JUCE__.backend) window.__JUCE__.backend.emitByBackend("novaStatus", JSON.stringify(st));
  }, 33);
})();
