import * as Juce from "./juce/index.js";

const choiceOptions = {
  low_freq: ["20", "30", "60", "100"],
  high_boost_freq: ["3k", "4k", "5k", "8k", "10k", "12k", "16k"],
  high_attenuation_freq: ["5k", "10k", "20k"],
  mode_preset: ["Neutral", "Vocal", "Bass", "Air", "Master"]
};

const presets = {
  Neutral: {
    low_freq: "60",
    low_boost: 0.0,
    low_attenuation: 0.0,
    high_boost_freq: "8k",
    high_boost: 0.0,
    bandwidth: 5.0,
    high_attenuation_freq: "10k",
    high_attenuation: 0.0,
    output_gain: 0.0,
    note: "Neutral tone state ready for musical shaping."
  },
  Vocal: {
    low_freq: "100",
    low_boost: 2.0,
    low_attenuation: 1.5,
    high_boost_freq: "10k",
    high_boost: 5.5,
    bandwidth: 4.0,
    high_attenuation_freq: "5k",
    high_attenuation: 2.0,
    output_gain: 0.0,
    note: "Clarity and presence without harshness."
  },
  Bass: {
    low_freq: "60",
    low_boost: 6.0,
    low_attenuation: 4.5,
    high_boost_freq: "3k",
    high_boost: 1.5,
    bandwidth: 3.0,
    high_attenuation_freq: "5k",
    high_attenuation: 0.0,
    output_gain: 0.0,
    note: "Classic Pultec low-end trick for weight and control."
  },
  Air: {
    low_freq: "30",
    low_boost: 0.0,
    low_attenuation: 0.0,
    high_boost_freq: "12k",
    high_boost: 7.0,
    bandwidth: 6.0,
    high_attenuation_freq: "10k",
    high_attenuation: 2.5,
    output_gain: 0.0,
    note: "Expensive top-end shine with smooth polish."
  },
  Master: {
    low_freq: "30",
    low_boost: 3.0,
    low_attenuation: 2.0,
    high_boost_freq: "10k",
    high_boost: 4.0,
    bandwidth: 5.0,
    high_attenuation_freq: "20k",
    high_attenuation: 1.5,
    output_gain: 0.0,
    note: "Subtle glue and broad tonal polish."
  }
};

const knobIds = {
  low_boost: "lowBoost-value",
  low_attenuation: "lowAttenuation-value",
  high_boost: "highBoost-value",
  bandwidth: "bandwidth-value",
  high_attenuation: "highAttenuation-value",
  output_gain: "outputGain-value"
};

const currentValues = {
  low_freq: "60",
  low_boost: 0.0,
  low_attenuation: 0.0,
  high_boost_freq: "8k",
  high_boost: 0.0,
  bandwidth: 5.0,
  high_attenuation_freq: "10k",
  high_attenuation: 0.0,
  output_gain: 0.0,
  mode_preset: "Neutral"
};

const parameterStates = {};
let juceAvailable = false;
let isApplyingPreset = false;

function getNormalizedValue(paramId, value) {
  if (choiceOptions[paramId]) {
    const options = choiceOptions[paramId];
    const index = Math.max(0, options.indexOf(String(value)));
    return options.length <= 1 ? 0 : index / (options.length - 1);
  }

  switch (paramId) {
    case "bandwidth":
      return (value - 1.0) / 9.0;
    case "output_gain":
      return (value + 10.0) / 20.0;
    default:
      return value / 10.0;
  }
}

function getActualValue(paramId, normalized) {
  const clamped = Math.max(0, Math.min(1, normalized));

  if (choiceOptions[paramId]) {
    const options = choiceOptions[paramId];
    const index = Math.round(clamped * (options.length - 1));
    return options[index];
  }

  switch (paramId) {
    case "bandwidth":
      return 1.0 + (clamped * 9.0);
    case "output_gain":
      return -10.0 + (clamped * 20.0);
    default:
      return clamped * 10.0;
  }
}

function formatValue(paramId, value) {
  if (paramId === "bandwidth") return `BW ${value.toFixed(1)}`;
  if (paramId === "output_gain") {
    const sign = value >= 0 ? "+" : "";
    return `${sign}${value.toFixed(1)} dB`;
  }
  return Number(value).toFixed(1);
}

function setKnobVisual(paramId, value) {
  currentValues[paramId] = value;
  const knob = document.querySelector(`.knob[data-param="${paramId}"] .arc`);
  if (knob) {
    const rotation = -42 + (getNormalizedValue(paramId, value) * 210);
    knob.style.setProperty("--turn", `${rotation}deg`);
  }

  const readout = document.getElementById(knobIds[paramId]);
  if (readout) readout.textContent = formatValue(paramId, value);
}

function setSelectorVisual(paramId, value) {
  currentValues[paramId] = value;
  document.querySelectorAll(`.option[data-param="${paramId}"]`).forEach((option) => {
    option.classList.toggle("active", option.dataset.value === String(value));
  });
}

function setModeVisual(modeName) {
  currentValues.mode_preset = modeName;
  document.querySelectorAll(".mode-button").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === modeName);
  });
}

function pushParameter(paramId, value) {
  const state = parameterStates[paramId];
  if (!state) return;
  state.setNormalisedValue(getNormalizedValue(paramId, value));
}

function applyPreset(modeName, pushToPlugin = true) {
  const preset = presets[modeName];
  if (!preset || isApplyingPreset) return;

  isApplyingPreset = true;

  setSelectorVisual("low_freq", preset.low_freq);
  setKnobVisual("low_boost", preset.low_boost);
  setKnobVisual("low_attenuation", preset.low_attenuation);
  setSelectorVisual("high_boost_freq", preset.high_boost_freq);
  setKnobVisual("high_boost", preset.high_boost);
  setKnobVisual("bandwidth", preset.bandwidth);
  setSelectorVisual("high_attenuation_freq", preset.high_attenuation_freq);
  setKnobVisual("high_attenuation", preset.high_attenuation);
  setKnobVisual("output_gain", preset.output_gain);
  setModeVisual(modeName);

  const note = document.getElementById("left-note");
  if (note) note.textContent = preset.note;

  if (pushToPlugin) {
    pushParameter("mode_preset", modeName);
    pushParameter("low_freq", preset.low_freq);
    pushParameter("low_boost", preset.low_boost);
    pushParameter("low_attenuation", preset.low_attenuation);
    pushParameter("high_boost_freq", preset.high_boost_freq);
    pushParameter("high_boost", preset.high_boost);
    pushParameter("bandwidth", preset.bandwidth);
    pushParameter("high_attenuation_freq", preset.high_attenuation_freq);
    pushParameter("high_attenuation", preset.high_attenuation);
    pushParameter("output_gain", preset.output_gain);
  }

  isApplyingPreset = false;
}

function bindSelectorButtons() {
  document.querySelectorAll(".option[data-param]").forEach((option) => {
    option.addEventListener("click", () => {
      const { param, value } = option.dataset;
      setSelectorVisual(param, value);
      pushParameter(param, value);
    });
  });
}

function getClientY(event) {
  if (!event) return 0;
  if (event.touches && event.touches.length > 0) return event.touches[0].clientY;
  if (event.changedTouches && event.changedTouches.length > 0) return event.changedTouches[0].clientY;
  return typeof event.clientY === "number" ? event.clientY : 0;
}

let activeDrag = null;

function handleDragMove(event) {
  if (!activeDrag) return;
  if (event?.preventDefault) event.preventDefault();

  const { paramId, min, max, startY, startValue } = activeDrag;
  const delta = startY - getClientY(event);
  const nextValue = Math.max(min, Math.min(max, startValue + (delta * (max - min) * 0.005)));
  setKnobVisual(paramId, nextValue);
  pushParameter(paramId, nextValue);
}

function endActiveDrag() {
  if (!activeDrag) return;

  const { paramId, knob } = activeDrag;
  knob.classList.remove("dragging");
  parameterStates[paramId]?.sliderDragEnded?.();
  activeDrag = null;
}

function bindKnobs() {
  document.querySelectorAll(".knob[data-param]").forEach((knob) => {
    const paramId = knob.dataset.param;
    const min = Number(knob.dataset.min);
    const max = Number(knob.dataset.max);

    const startDrag = (event) => {
      if (typeof event.button === "number" && event.button !== 0) return;
      if (event?.preventDefault) event.preventDefault();
      if (event?.stopPropagation) event.stopPropagation();

      knob.classList.add("dragging");
      activeDrag = {
        paramId,
        knob,
        min,
        max,
        startY: getClientY(event),
        startValue: Number(currentValues[paramId])
      };

      if (typeof knob.setPointerCapture === "function" && typeof event.pointerId !== "undefined") {
        try { knob.setPointerCapture(event.pointerId); } catch {}
      }

      parameterStates[paramId]?.sliderDragStarted?.();
    };

    knob.addEventListener("pointerdown", startDrag);
    knob.addEventListener("mousedown", startDrag);
    knob.addEventListener("touchstart", startDrag, { passive: false });

    knob.addEventListener("dblclick", () => {
      const defaultValue = presets[currentValues.mode_preset]?.[paramId] ?? currentValues[paramId];
      setKnobVisual(paramId, defaultValue);
      pushParameter(paramId, defaultValue);
    });
  });

  window.addEventListener("pointermove", handleDragMove, { passive: false });
  window.addEventListener("mousemove", handleDragMove, { passive: false });
  window.addEventListener("touchmove", handleDragMove, { passive: false });
  window.addEventListener("pointerup", endActiveDrag);
  window.addEventListener("mouseup", endActiveDrag);
  window.addEventListener("touchend", endActiveDrag);
  window.addEventListener("touchcancel", endActiveDrag);
}

function bindModeButtons() {
  document.querySelectorAll(".mode-button").forEach((button) => {
    button.addEventListener("click", () => applyPreset(button.dataset.mode, true));
  });
}

function connectParameters() {
  if (!juceAvailable) return;

  const ids = [
    "low_freq",
    "low_boost",
    "low_attenuation",
    "high_boost_freq",
    "high_boost",
    "bandwidth",
    "high_attenuation_freq",
    "high_attenuation",
    "output_gain",
    "mode_preset"
  ];

  ids.forEach((id) => {
    try {
      parameterStates[id] = Juce.getSliderState(id);
    } catch (error) {
      console.warn(`Failed to connect parameter ${id}:`, error);
    }
  });

  ["low_boost", "low_attenuation", "high_boost", "bandwidth", "high_attenuation", "output_gain"].forEach((id) => {
    const state = parameterStates[id];
    if (!state) return;
    state.addValueChangedListener(() => {
      if (isApplyingPreset) return;
      setKnobVisual(id, getActualValue(id, state.getNormalisedValue()));
    });
    setKnobVisual(id, getActualValue(id, state.getNormalisedValue()));
  });

  ["low_freq", "high_boost_freq", "high_attenuation_freq"].forEach((id) => {
    const state = parameterStates[id];
    if (!state) return;
    state.addValueChangedListener(() => {
      if (isApplyingPreset) return;
      setSelectorVisual(id, getActualValue(id, state.getNormalisedValue()));
    });
    setSelectorVisual(id, getActualValue(id, state.getNormalisedValue()));
  });

  const modeState = parameterStates.mode_preset;
  if (modeState) {
    modeState.addValueChangedListener(() => {
      const modeName = getActualValue("mode_preset", modeState.getNormalisedValue());
      if (!isApplyingPreset) applyPreset(modeName, true);
    });
  }
}

window.updateOutputMeter = (level, isHot) => {
  const bars = document.querySelectorAll("#output-meter .meter-bar");
  const activeCount = Math.round(Math.max(0, Math.min(1, level)) * bars.length);
  bars.forEach((bar, index) => {
    bar.classList.toggle("active", index < activeCount);
    bar.classList.toggle("hot", Boolean(isHot) && index < activeCount);
  });
};

document.addEventListener("DOMContentLoaded", () => {
  juceAvailable = typeof window.__JUCE__ !== "undefined";

  bindSelectorButtons();
  bindKnobs();
  bindModeButtons();
  connectParameters();
  applyPreset("Neutral", false);
});
