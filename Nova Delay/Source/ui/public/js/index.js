import * as Juce from "./juce/index.js";

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

function setupViewportFit() {
  const plugin = document.querySelector(".plugin");
  if (!plugin) return;

  const designWidth = 1280;
  const designHeight = 840;
  const padding = 20;

  const updateScale = () => {
    const availW = Math.max(320, window.innerWidth - padding);
    const availH = Math.max(320, window.innerHeight - padding);
    const scale = Math.min(1, availW / designWidth, availH / designHeight);
    plugin.style.setProperty("--ui-scale", scale.toFixed(4));
  };

  updateScale();
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
      paramStates[id] = Juce.getSliderState(id);
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

window.updateMeters = (inputLevel, outputLevel, hot) => {
  setMeterLevel("inputMeter", inputLevel, false);
  setMeterLevel("outputMeter", outputLevel, !!hot);
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
    t += 0.011;

    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);

    const centerY = h * 0.56;

    const bg = ctx.createRadialGradient(w * 0.38, h * 0.54, 8, w * 0.38, h * 0.54, w * 0.64);
    bg.addColorStop(0, "rgba(255, 154, 52, 0.15)");
    bg.addColorStop(0.6, "rgba(180, 98, 29, 0.08)");
    bg.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, w, h);

    const fbNorm = Math.max(0, Math.min(1, Number(values.feedback) / 100));
    const mixNorm = Math.max(0, Math.min(1, Number(values.mix) / 100));
    const wowNorm = Math.max(0, Math.min(1, Number(values.wow_flutter) / 100));
    const toneNorm = Math.max(0, Math.min(1, Number(values.tone) / 100));

    const repeats = Math.round(8 + fbNorm * 8);
    const baseSpacing = w * 0.085;
    const startX = w * 0.13;

    for (let i = 0; i < repeats; i++) {
      const decay = Math.pow(0.72, i) * (0.6 + mixNorm * 0.5);
      const jitter = Math.sin((i * 0.9) + (t * (0.8 + wowNorm * 2.0))) * (2 + wowNorm * 6);
      const spacingJitter = Math.sin((i * 1.7) + t * 1.4) * (5 + wowNorm * 10);
      const x = startX + i * baseSpacing + spacingJitter;
      const spikeHeight = (h * 0.4) * decay * (0.88 + 0.16 * Math.sin(t * 2 + i));
      const spread = 2 + i * 0.35 + wowNorm * 4;

      const colorA = 255;
      const colorB = 170 - Math.round(i * 5);
      const colorC = 68 - Math.round(i * 2);
      const alpha = Math.max(0.08, 0.9 * decay);

      ctx.strokeStyle = `rgba(${colorA},${Math.max(70, colorB)},${Math.max(18, colorC)},${alpha})`;
      ctx.lineWidth = 1.6 + decay * 2.6;
      ctx.shadowColor = `rgba(255, 168, 60, ${0.45 * decay})`;
      ctx.shadowBlur = 12 + i * 1.1;

      ctx.beginPath();
      ctx.moveTo(x, centerY - spikeHeight);
      ctx.lineTo(x + jitter * 0.12, centerY + spikeHeight * (0.55 + 0.20 * (1 - toneNorm)));
      ctx.stroke();

      ctx.shadowBlur = 0;
      ctx.strokeStyle = `rgba(255, 150, 50, ${0.26 * decay})`;
      ctx.lineWidth = spread;
      ctx.beginPath();
      ctx.moveTo(x - 4, centerY + spikeHeight * 0.34);
      ctx.lineTo(x + 5, centerY + spikeHeight * 0.36);
      ctx.stroke();

      for (let p = 0; p < 2; p++) {
        const px = x + (Math.random() - 0.5) * 22;
        const py = centerY - spikeHeight * (0.1 + Math.random() * 0.9);
        ctx.fillStyle = `rgba(255, 183, 81, ${Math.max(0.03, decay * 0.45)})`;
        ctx.fillRect(px, py, 1.2, 1.2);
      }
    }

    ctx.strokeStyle = "rgba(255, 190, 92, 0.92)";
    ctx.lineWidth = 2.6;
    ctx.beginPath();
    for (let x = 0; x < w; x += 4) {
      const progress = x / w;
      const env = Math.exp (-progress * (2 + fbNorm * 4));
      const y = centerY + Math.sin(x * 0.02 + t * 2.5) * env * 11;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    requestAnimationFrame(draw);
  };

  draw();
}

document.addEventListener("DOMContentLoaded", () => {
  setupViewportFit();

  populateSelect("presetSelect", choices.preset);
  populateSelect("delayModelSelect", choices.delay_model);

  createMeterBars("inputMeter");
  createMeterBars("outputMeter");

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

  setMeterLevel("inputMeter", 0.22);
  setMeterLevel("outputMeter", 0.31);
});
