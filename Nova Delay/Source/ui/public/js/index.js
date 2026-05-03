const choices = {
  preset: ["Vintage Echo", "Modern Tape", "BBD Lo-Fi", "Dub Space"],
  delay_time_sync: ["1/32", "1/16", "1/8", "1/8D", "1/8T", "1/4", "1/4D", "1/4T", "1/2", "1/1"],
  mode: ["Analog", "Tape", "BBD"],
  delay_model: ["Vintage Tape", "Warm Bucket", "Modern Analog", "Echo Chamber"]
};

const paramStates = {};
const values = {
  preset: "Vintage Echo",
  delay_time_sync: "1/4",
  delay_time_free_ms: 500,
  sync_enabled: 1,
  feedback: 35,
  mix: 40,
  tone: 55,
  wow_flutter: 25,
  saturation: 30,
  mode: "Tape",
  ping_pong: 0,
  stereo: 1,
  lofi: 0,
  freeze: 0,
  hp_filter_hz: 120,
  lp_filter_hz: 6000,
  ducking: 20,
  delay_model: "Vintage Tape"
};

let isUpdating = false;

function ensureJuceInterop() {
  if (window.__JUCE__ && window.__JUCE__.backend) return;

  const noop = () => {};
  window.__JUCE__ = {
    backend: {
      addEventListener: noop,
      removeEventListener: noop,
      emitEvent: noop,
    },
    initialisationData: {
      __juce__functions: [],
      __juce__sliders: [],
      __juce__toggles: [],
      __juce__comboBoxes: [],
    },
  };
}

function createListenerList() {
  return {
    listeners: [],
    addListener(fn) {
      this.listeners.push(fn);
      return this.listeners.length - 1;
    },
    callListeners(payload) {
      this.listeners.forEach((listener) => listener(payload));
    },
  };
}

function createSliderState(name) {
  ensureJuceInterop();
  const backend = window.__JUCE__.backend;

  const state = {
    name,
    identifier: `__juce__slider${name}`,
    scaledValue: 0,
    properties: {
      start: 0,
      end: 1,
      skew: 1,
      interval: 0,
      numSteps: 100,
    },
    valueChangedEvent: createListenerList(),
    addValueChangedListener(fn) {
      return this.valueChangedEvent.addListener(fn);
    },
    getNormalisedValue() {
      const start = typeof this.properties.start === "number" ? this.properties.start : 0;
      const end = typeof this.properties.end === "number" ? this.properties.end : 1;
      const range = end - start;
      if (range === 0) return 0;
      return Math.max(0, Math.min(1, (this.scaledValue - start) / range));
    },
    setNormalisedValue(newValue) {
      const normalised = Math.max(0, Math.min(1, Number(newValue) || 0));
      const start = typeof this.properties.start === "number" ? this.properties.start : 0;
      const end = typeof this.properties.end === "number" ? this.properties.end : 1;
      const interval = typeof this.properties.interval === "number" ? this.properties.interval : 0;

      let scaled = start + normalised * (end - start);
      if (interval > 0) scaled = Math.round(scaled / interval) * interval;

      this.scaledValue = scaled;
      backend.emitEvent(this.identifier, { eventType: "valueChanged", value: scaled });
    },
    sliderDragStarted() {
      backend.emitEvent(this.identifier, { eventType: "sliderDragStarted" });
    },
    sliderDragEnded() {
      backend.emitEvent(this.identifier, { eventType: "sliderDragEnded" });
    },
  };

  backend.addEventListener(state.identifier, (event) => {
    if (!event) return;

    if (event.eventType === "valueChanged") {
      if (typeof event.value === "number") state.scaledValue = event.value;
      state.valueChangedEvent.callListeners(event);
    }

    if (event.eventType === "propertiesChanged") {
      const nextProps = {};
      Object.keys(event).forEach((key) => {
        if (key !== "eventType") nextProps[key] = event[key];
      });
      state.properties = nextProps;
    }
  });

  backend.emitEvent(state.identifier, { eventType: "requestInitialUpdate" });
  return state;
}

function setupViewportFit() {
  const plugin = document.querySelector(".plugin");
  if (!plugin) return;

  const padding = 20;

  const updateScale = () => {
    // Reset scale before measurement so dimensions are based on natural layout size.
    plugin.style.setProperty("--ui-scale", "1");

    // Use scroll dimensions as well, because some sections intentionally overflow
    // the plugin box (e.g. lower controls) and must be included in fit math.
    const designWidth = Math.max(1, plugin.offsetWidth || 1280, plugin.scrollWidth || 0);
    const designHeight = Math.max(1, plugin.offsetHeight || 840, plugin.scrollHeight || 0);

    const availW = Math.max(320, window.innerWidth - padding);
    const availH = Math.max(320, window.innerHeight - padding);
    const safeW = Math.max(1, availW - 4);
    const safeH = Math.max(1, availH - 8);

    // Hard-cap scale to the suite-standard editor target (1080x680).
    // This prevents hosts that report oversized webview bounds from
    // rendering the legacy 1280x840 visual footprint.
    const suiteTargetW = 1080;
    const suiteTargetH = 680;
    const suiteCapScale = Math.min(1, suiteTargetW / designWidth, suiteTargetH / designHeight);

    const scale = Math.min(1, suiteCapScale, safeW / designWidth, safeH / designHeight);
    plugin.style.setProperty("--ui-scale", scale.toFixed(4));
  };

  updateScale();
  window.addEventListener("load", updateScale);
  window.addEventListener("resize", updateScale);
}

function populateSelect(id, options) {
  const select = document.getElementById(id);
  if (!select) return;
  select.innerHTML = options.map((o) => `<option value="${o}">${o}</option>`).join("");
}

function asNorm(param, value) {
  if (choices[param]) {
    const arr = choices[param];
    const idx = Math.max(0, arr.indexOf(String(value)));
    return arr.length <= 1 ? 0 : idx / (arr.length - 1);
  }

  if (["sync_enabled", "ping_pong", "stereo", "lofi", "freeze"].includes(param)) {
    return Number(value) > 0 ? 1 : 0;
  }

  if (param === "delay_time_free_ms") return (value - 20) / 1980;
  if (param === "hp_filter_hz") return (value - 20) / 780;
  if (param === "lp_filter_hz") return (value - 1000) / 17000;

  return Number(value) / 100;
}

function fromNorm(param, norm) {
  const n = Math.max(0, Math.min(1, norm));

  if (choices[param]) {
    const arr = choices[param];
    return arr[Math.round(n * (arr.length - 1))];
  }

  if (["sync_enabled", "ping_pong", "stereo", "lofi", "freeze"].includes(param)) {
    return n >= 0.5 ? 1 : 0;
  }

  if (param === "delay_time_free_ms") return 20 + n * 1980;
  if (param === "hp_filter_hz") return 20 + n * 780;
  if (param === "lp_filter_hz") return 1000 + n * 17000;

  return n * 100;
}

function setParam(param, value) {
  values[param] = value;
  const st = paramStates[param];
  if (st) st.setNormalisedValue(asNorm(param, value));
}

function formatValue(param, value) {
  if (param === "delay_time_free_ms") return `${Math.round(value)} ms`;
  if (param === "feedback" || param === "mix" || param === "tone" || param === "wow_flutter" || param === "saturation" || param === "ducking") return `${Math.round(value)}%`;
  if (param === "hp_filter_hz") return `${Math.round(value)} Hz`;
  if (param === "lp_filter_hz") return `${(value / 1000).toFixed(1)} kHz`;
  return `${value}`;
}

function updateKnobVisual(knob, norm) {
  const n = Math.max(0, Math.min(1, norm));
  const deg = -140 + n * 280;
  knob.style.setProperty("--turn", `${deg}deg`);
  const ring = knob.querySelector(".ring");
  if (ring) ring.style.setProperty("--fill", `${n * 100}%`);
}

function refreshUi() {
  const knobParams = ["feedback", "mix", "tone", "wow_flutter", "saturation", "hp_filter_hz", "lp_filter_hz", "ducking"];
  knobParams.forEach((param) => {
    const knob = document.querySelector(`.knob[data-param="${param}"]`);
    if (knob) updateKnobVisual(knob, asNorm(param, values[param]));
    const readout = document.getElementById(`${param}Value`);
    if (readout) readout.textContent = formatValue(param, values[param]);
  });

  const delayLabel = values.sync_enabled ? values.delay_time_sync : `${Math.round(values.delay_time_free_ms)} ms`;
  const delayReadout = document.getElementById("delayTimeValue");
  const syncDisplay = document.getElementById("syncDisplay");
  if (delayReadout) delayReadout.textContent = delayLabel;
  if (syncDisplay) syncDisplay.textContent = values.sync_enabled ? values.delay_time_sync : "FREE";

  document.querySelectorAll(".toggle[data-param='sync_enabled']").forEach((btn) => {
    const isSync = values.sync_enabled === 1;
    const wanted = Number(btn.dataset.value) === 1;
    btn.classList.toggle("active", wanted === isSync);
  });

  const syncNoteRow = document.getElementById("syncNoteRow");
  if (syncNoteRow) syncNoteRow.classList.toggle("disabled", values.sync_enabled !== 1);

  document.querySelectorAll(".sync-note-btn").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.syncValue === values.delay_time_sync);
  });

  ["ping_pong", "stereo", "lofi", "freeze"].forEach((param) => {
    const btn = document.querySelector(`.fx-btn[data-param='${param}']`);
    if (btn) btn.classList.toggle("active", values[param] > 0);
  });

  document.querySelectorAll(".mode-btn[data-param='mode']").forEach((btn) => {
    btn.classList.toggle("active", Number(btn.dataset.index) === choices.mode.indexOf(values.mode));
  });

  const presetSelect = document.getElementById("presetSelect");
  if (presetSelect && presetSelect.value !== values.preset) presetSelect.value = values.preset;

  const modelSelect = document.getElementById("delayModelSelect");
  if (modelSelect && modelSelect.value !== values.delay_model) modelSelect.value = values.delay_model;

  const delayKnob = document.getElementById("delayTimeKnob");
  if (delayKnob) {
    const norm = values.sync_enabled ? asNorm("delay_time_sync", values.delay_time_sync) : asNorm("delay_time_free_ms", values.delay_time_free_ms);
    updateKnobVisual(delayKnob, norm);
  }
}

function setupKnobDragging() {
  let active = null;

  const start = (event, knob, param, min, max) => {
    if (event.button !== undefined && event.button !== 0) return;
    event.preventDefault();
    knob.classList.add("active");
    active = {
      knob,
      param,
      min,
      max,
      startY: event.touches?.[0]?.clientY ?? event.clientY,
      startValue: Number(values[param])
    };
    paramStates[param]?.sliderDragStarted?.();
  };

  const move = (event) => {
    if (!active) return;
    event.preventDefault();
    const y = event.touches?.[0]?.clientY ?? event.clientY;
    const delta = active.startY - y;
    const next = Math.max(active.min, Math.min(active.max, active.startValue + delta * (active.max - active.min) * 0.0045));
    values[active.param] = next;
    setParam(active.param, next);
    refreshUi();
  };

  const end = () => {
    if (!active) return;
    paramStates[active.param]?.sliderDragEnded?.();
    active.knob.classList.remove("active");
    active = null;
  };

  document.querySelectorAll(".knob[data-param]").forEach((knob) => {
    const param = knob.dataset.param;
    const min = Number(knob.dataset.min || 0);
    const max = Number(knob.dataset.max || 100);

    knob.addEventListener("mousedown", (e) => start(e, knob, param, min, max));
    knob.addEventListener("touchstart", (e) => start(e, knob, param, min, max), { passive: false });
  });

  const delayKnob = document.getElementById("delayTimeKnob");
  if (delayKnob) {
    delayKnob.addEventListener("mousedown", (event) => {
      if (event.button !== undefined && event.button !== 0) return;
      event.preventDefault();
      delayKnob.classList.add("active");
      const param = values.sync_enabled ? "delay_time_sync" : "delay_time_free_ms";
      const min = values.sync_enabled ? 0 : 20;
      const max = values.sync_enabled ? 9 : 2000;
      const startValue = values.sync_enabled ? choices.delay_time_sync.indexOf(values.delay_time_sync) : values.delay_time_free_ms;
      active = {
        knob: delayKnob,
        param,
        min,
        max,
        startY: event.clientY,
        startValue
      };
      paramStates[param]?.sliderDragStarted?.();
    });

    delayKnob.addEventListener("touchstart", (event) => {
      event.preventDefault();
      delayKnob.classList.add("active");
      const param = values.sync_enabled ? "delay_time_sync" : "delay_time_free_ms";
      const min = values.sync_enabled ? 0 : 20;
      const max = values.sync_enabled ? 9 : 2000;
      const startValue = values.sync_enabled ? choices.delay_time_sync.indexOf(values.delay_time_sync) : values.delay_time_free_ms;
      active = {
        knob: delayKnob,
        param,
        min,
        max,
        startY: event.touches?.[0]?.clientY ?? 0,
        startValue
      };
      paramStates[param]?.sliderDragStarted?.();
    }, { passive: false });
  }

  window.addEventListener("mousemove", move, { passive: false });
  window.addEventListener("touchmove", move, { passive: false });
  window.addEventListener("mouseup", end);
  window.addEventListener("touchend", end);
}

function setupButtons() {
  document.querySelectorAll(".toggle[data-param='sync_enabled']").forEach((btn) => {
    btn.addEventListener("click", () => {
      const v = Number(btn.dataset.value);
      values.sync_enabled = v;
      setParam("sync_enabled", v);
      refreshUi();
    });
  });

  document.querySelectorAll(".sync-note-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      const value = btn.dataset.syncValue;
      if (!value) return;
      values.sync_enabled = 1;
      setParam("sync_enabled", 1);
      values.delay_time_sync = value;
      setParam("delay_time_sync", value);
      refreshUi();
    });
  });

  document.querySelectorAll(".fx-btn[data-param]").forEach((btn) => {
    const param = btn.dataset.param;
    btn.addEventListener("click", () => {
      const v = values[param] > 0 ? 0 : 1;
      values[param] = v;
      setParam(param, v);
      refreshUi();
    });
  });

  document.querySelectorAll(".mode-btn[data-param='mode']").forEach((btn) => {
    btn.addEventListener("click", () => {
      const index = Number(btn.dataset.index);
      const mode = choices.mode[index] || "TAPE";
      values.mode = mode;
      setParam("mode", mode);
      refreshUi();
    });
  });

  const presetSelect = document.getElementById("presetSelect");
  presetSelect?.addEventListener("change", () => {
    values.preset = presetSelect.value;
    setParam("preset", values.preset);
    refreshUi();
  });

  const delayModelSelect = document.getElementById("delayModelSelect");
  delayModelSelect?.addEventListener("change", () => {
    values.delay_model = delayModelSelect.value;
    setParam("delay_model", values.delay_model);
    refreshUi();
  });
}

function connectParameters() {
  const ids = [
    "preset", "delay_time_sync", "delay_time_free_ms", "sync_enabled",
    "feedback", "mix", "tone", "wow_flutter", "saturation", "mode",
    "ping_pong", "stereo", "lofi", "freeze", "hp_filter_hz", "lp_filter_hz", "ducking", "delay_model"
  ];

  ids.forEach((id) => {
    try {
      paramStates[id] = createSliderState(id);
      paramStates[id].addValueChangedListener(() => {
        if (isUpdating) return;
        isUpdating = true;
        values[id] = fromNorm(id, paramStates[id].getNormalisedValue());
        refreshUi();
        isUpdating = false;
      });
      values[id] = fromNorm(id, paramStates[id].getNormalisedValue());
    } catch (error) {
      console.warn("Parameter unavailable", id, error);
    }
  });

  refreshUi();
}

function createMeterBars(id, count = 20) {
  const meter = document.getElementById(id);
  if (!meter) return;
  meter.innerHTML = "";
  for (let i = 0; i < count; i++) {
    const bar = document.createElement("div");
    bar.className = "bar";
    meter.appendChild(bar);
  }
}

function setMeterLevel(id, level, hot = false) {
  const meter = document.getElementById(id);
  if (!meter) return;
  const bars = Array.from(meter.children);
  const active = Math.round(Math.max(0, Math.min(1, level)) * bars.length);
  bars.forEach((bar, i) => {
    bar.classList.toggle("on", i < active);
    bar.classList.toggle("hot", hot && i >= bars.length - 4 && i < active);
  });
}

window.updateMeters = (inL, inR, outL, outR, hot) => {
  // Backward compatibility for old mono signature: (inputLevel, outputLevel, hot)
  if (typeof outL !== "number" || typeof outR !== "number") {
    const monoIn = Number(inL) || 0;
    const monoOut = Number(inR) || 0;
    const monoHot = Boolean(outL);
    setMeterLevel("inputMeterL", monoIn, false);
    setMeterLevel("inputMeterR", monoIn, false);
    setMeterLevel("outputMeterL", monoOut, monoHot);
    setMeterLevel("outputMeterR", monoOut, monoHot);
    return;
  }

  setMeterLevel("inputMeterL", Number(inL) || 0, false);
  setMeterLevel("inputMeterR", Number(inR) || 0, false);
  setMeterLevel("outputMeterL", Number(outL) || 0, !!hot);
  setMeterLevel("outputMeterR", Number(outR) || 0, !!hot);
};

function setupVisualizer() {
  const canvas = document.getElementById("echoViz");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    canvas.style.background = "linear-gradient(180deg, rgba(35, 20, 10, 0.96), rgba(10, 6, 3, 0.98))";
    canvas.style.border = "1px solid rgba(255, 170, 68, 0.45)";
    return;
  }

  const resize = () => {
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;
  };

  resize();
  window.addEventListener("resize", resize);

  let t = 0;

  const draw = () => {
    t += 0.014;

    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    const horizonY = h * 0.63;

    const bg = ctx.createRadialGradient(w * 0.52, h * 0.52, 12, w * 0.52, h * 0.52, w * 0.72);
    bg.addColorStop(0, "rgba(255, 175, 72, 0.22)");
    bg.addColorStop(0.45, "rgba(149, 83, 28, 0.12)");
    bg.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, w, h);

    const haze = ctx.createLinearGradient(0, 0, 0, h);
    haze.addColorStop(0, "rgba(0,0,0,0.10)");
    haze.addColorStop(0.55, "rgba(0,0,0,0.0)");
    haze.addColorStop(1, "rgba(0,0,0,0.36)");
    ctx.fillStyle = haze;
    ctx.fillRect(0, 0, w, h);

    const fbNorm = Math.max(0, Math.min(1, Number(values.feedback) / 100));
    const mixNorm = Math.max(0, Math.min(1, Number(values.mix) / 100));
    const wowNorm = Math.max(0, Math.min(1, Number(values.wow_flutter) / 100));
    const toneNorm = Math.max(0, Math.min(1, Number(values.tone) / 100));

    const repeats = Math.round(9 + fbNorm * 10);
    const startX = w * 0.13;
    let x = startX;
    let spacing = w * 0.13;
    const tilt = (Math.sin(t * 0.5) * 0.5 + 0.5) * 2 - 1;
    const shimmer = 0.92 + 0.08 * Math.sin(t * 1.7);

    ctx.globalCompositeOperation = "screen";

    for (let i = 0; i < repeats; i++) {
      const perspective = Math.pow(0.84, i);
      const decay = Math.pow(0.70, i) * (0.88 + mixNorm * 0.34);
      const wobble = Math.sin(t * (2.0 + wowNorm * 3.2) + i * 0.75) * (1.5 + wowNorm * 8.0);
      const driftY = (i * (1.35 + toneNorm * 0.8)) * tilt;
      const farDrift = i > 4 ? Math.sin(t * 1.2 + i) * (0.8 + i * 0.14) : 0;
      const pulseX = x + wobble;
      const pulseY = horizonY - driftY + farDrift;
      const pulseHeight = h * (0.78 * decay + 0.08);
      const coreWidth = i < 2 ? (7.5 - i * 0.8) : 6 + i * 0.55;
      const blurWidth = coreWidth * (2.8 + i * 0.14);

      const glowTier = i < 2 ? 1.35 : i < 5 ? 0.72 : 0.22;
      const flicker = 0.88 + 0.12 * Math.sin(t * (5.4 + i * 0.45) + i * 0.7);
      const alphaCore = Math.max(0.02, 1.15 * decay * glowTier * shimmer * flicker);
      const alphaBloom = Math.max(0.01, 0.78 * decay * glowTier * shimmer);
      const warmShift = Math.min(1, i / 12);
      const red = 255;
      const green = Math.round(194 - warmShift * 68 - (1 - toneNorm) * 8);
      const blue = Math.round(112 - warmShift * 60);

      ctx.shadowColor = `rgba(${red}, ${Math.max(86, green)}, ${Math.max(28, blue)}, ${0.7 * decay * glowTier})`;
      ctx.shadowBlur = 30 + i * 5.2;
      const pulseGrad = ctx.createLinearGradient(pulseX, pulseY - pulseHeight, pulseX, pulseY + pulseHeight * 0.45);
      pulseGrad.addColorStop(0, `rgba(${red}, ${Math.max(94, green)}, ${Math.max(26, blue)}, 0)`);
      pulseGrad.addColorStop(0.14, `rgba(${red}, ${Math.max(120, green + 24)}, ${Math.max(68, blue + 32)}, ${alphaCore * (i < 1 ? 1.1 : 1)})`);
      pulseGrad.addColorStop(0.18, `rgba(255, 246, 232, ${i < 2 ? alphaCore * 0.60 : alphaCore * 0.12})`);
      pulseGrad.addColorStop(0.52, `rgba(${red}, ${Math.max(88, green - 6)}, ${Math.max(24, blue - 4)}, ${alphaCore * 0.8})`);
      pulseGrad.addColorStop(1, `rgba(${red}, ${Math.max(74, green - 18)}, ${Math.max(20, blue - 8)}, 0)`);
      ctx.fillStyle = pulseGrad;
      ctx.fillRect(pulseX - coreWidth * 0.5, pulseY - pulseHeight, coreWidth, pulseHeight * 1.45);

      const aura = ctx.createRadialGradient(pulseX, pulseY - pulseHeight * 0.42, 2, pulseX, pulseY - pulseHeight * 0.35, pulseHeight * 0.9);
      aura.addColorStop(0, `rgba(255, ${Math.max(100, green + 8)}, ${Math.max(34, blue + 8)}, ${alphaBloom})`);
      aura.addColorStop(0.7, `rgba(255, ${Math.max(88, green - 4)}, ${Math.max(24, blue - 6)}, ${alphaBloom * 0.25})`);
      aura.addColorStop(1, "rgba(0,0,0,0)");
      ctx.fillStyle = aura;
      ctx.fillRect(pulseX - blurWidth, pulseY - pulseHeight * 1.22, blurWidth * 2, pulseHeight * 1.8);

      if (i >= 4) {
        const fog = ctx.createRadialGradient(pulseX, pulseY - pulseHeight * 0.18, 0, pulseX, pulseY - pulseHeight * 0.2, pulseHeight * 1.2);
        fog.addColorStop(0, `rgba(255, ${Math.max(92, green - 8)}, ${Math.max(22, blue - 8)}, ${0.10 * decay})`);
        fog.addColorStop(1, "rgba(0,0,0,0)");
        ctx.fillStyle = fog;
        ctx.fillRect(pulseX - blurWidth * 1.3, pulseY - pulseHeight * 1.5, blurWidth * 2.6, pulseHeight * 2.3);
      }

      ctx.shadowBlur = 0;

      const reflectionBoost = i < 3 ? 1.35 : 0.92;
      const reflectionHeight = pulseHeight * (0.21 + perspective * 0.11) * reflectionBoost;
      const reflection = ctx.createLinearGradient(pulseX, pulseY + 2, pulseX, pulseY + reflectionHeight + 24);
      reflection.addColorStop(0, `rgba(${red}, ${Math.max(98, green + 8)}, ${Math.max(34, blue + 10)}, ${alphaCore * (i < 3 ? 0.48 : 0.28)})`);
      reflection.addColorStop(1, "rgba(0,0,0,0)");
      ctx.fillStyle = reflection;
      ctx.beginPath();
      ctx.ellipse(pulseX, pulseY + reflectionHeight * 0.5, blurWidth * 0.95, reflectionHeight, 0, 0, Math.PI * 2);
      ctx.fill();

      x += spacing;
      spacing *= 0.80;
      if (x > w * 0.95) break;
    }

    ctx.globalCompositeOperation = "source-over";

    const floorGlow = ctx.createLinearGradient(0, horizonY - 4, 0, h);
    floorGlow.addColorStop(0, "rgba(255, 170, 72, 0.14)");
    floorGlow.addColorStop(0.35, "rgba(130, 75, 26, 0.12)");
    floorGlow.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = floorGlow;
    ctx.fillRect(0, horizonY - 2, w, h - horizonY + 2);

    ctx.strokeStyle = "rgba(255, 190, 98, 0.34)";
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.moveTo(w * 0.08, horizonY + 0.5);
    ctx.lineTo(w * 0.92, horizonY + 0.5);
    ctx.stroke();

    const vignette = ctx.createRadialGradient(w * 0.5, h * 0.5, h * 0.24, w * 0.5, h * 0.5, h * 0.82);
    vignette.addColorStop(0, "rgba(0,0,0,0)");
    vignette.addColorStop(1, "rgba(0,0,0,0.38)");
    ctx.fillStyle = vignette;
    ctx.fillRect(0, 0, w, h);

    requestAnimationFrame(draw);
  };

  draw();
}

document.addEventListener("DOMContentLoaded", () => {
  setupViewportFit();

  populateSelect("presetSelect", choices.preset);
  populateSelect("delayModelSelect", choices.delay_model);

  createMeterBars("inputMeterL");
  createMeterBars("inputMeterR");
  createMeterBars("outputMeterL");
  createMeterBars("outputMeterR");

  // Keep the visualizer alive even if any parameter bridge step fails.
  setupVisualizer();

  const safeRun = (fn, label) => {
    try {
      fn();
    } catch (error) {
      console.warn(`UI init step failed: ${label}`, error);
    }
  };

  safeRun(setupKnobDragging, "setupKnobDragging");
  safeRun(setupButtons, "setupButtons");
  safeRun(connectParameters, "connectParameters");

  setMeterLevel("inputMeterL", 0.22);
  setMeterLevel("inputMeterR", 0.18);
  setMeterLevel("outputMeterL", 0.31);
  setMeterLevel("outputMeterR", 0.27);
});
