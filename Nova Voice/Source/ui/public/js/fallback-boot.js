(function () {
  if (window.__novaVoiceBooted) return;
  window.__novaVoiceBooted = true;

  var KNOB_PARAMS = ["pitch", "morph", "texture", "form", "air", "blend"];
  var ALL_PARAMS = ["pitch", "morph", "texture", "form", "air", "blend", "voice_mode"];
  var PRESETS = [
    { name: "Clean Lift", category: "Vocal", tags: ["Clean", "Polished", "Lead"], pitch: 0.0, morph: 1.7, texture: 0.9, form: 4.1, air: 5.2, blend: 32, mode: 0 },
    { name: "Velvet Tone", category: "Vocal", tags: ["Smooth", "Warm", "Body"], pitch: 0.0, morph: 3.5, texture: 2.0, form: 4.5, air: 4.0, blend: 45, mode: 2 },
    { name: "Air Pop", category: "Vocal", tags: ["Airy", "Bright", "Modern"], pitch: 0.0, morph: 2.8, texture: 2.5, form: 5.2, air: 7.0, blend: 40, mode: 0 },
    { name: "Future Pop", category: "Modern FX", tags: ["Glossy", "Radio", "Modern"], pitch: 0.0, morph: 5.5, texture: 4.0, form: 5.5, air: 6.5, blend: 55, mode: 1 },
    { name: "Hyper Clean", category: "Modern FX", tags: ["Processed", "Smooth", "Digital"], pitch: 0.0, morph: 4.5, texture: 3.0, form: 5.0, air: 6.0, blend: 60, mode: 1 },
    { name: "Glass Voice", category: "Modern FX", tags: ["Bright", "Transparent", "Airy"], pitch: 0.0, morph: 4.0, texture: 2.5, form: 5.8, air: 7.5, blend: 50, mode: 2 },
    { name: "Alien Lead", category: "Creative", tags: ["Transformed", "Alien", "Extreme"], pitch: 0.0, morph: 8.0, texture: 5.5, form: 7.0, air: 6.0, blend: 70, mode: 3 },
    { name: "Deep Form", category: "Creative", tags: ["Dark", "Massive", "Extreme"], pitch: 0.0, morph: 6.5, texture: 4.0, form: 3.0, air: 4.5, blend: 65, mode: 3 },
    { name: "Neon Character", category: "Creative", tags: ["Synthetic", "Colorful", "Digital"], pitch: 0.0, morph: 7.0, texture: 5.0, form: 6.5, air: 6.5, blend: 75, mode: 1 },
    { name: "Robot Commander", category: "Creative", tags: ["Robot", "Synthetic", "HardTune"], pitch: -4.0, morph: 9.6, texture: 8.8, form: 7.6, air: 3.0, blend: 92, mode: 4 },
    { name: "Adlib Shine", category: "Adlibs", tags: ["Bright", "Adlib", "Digital"], pitch: 0.0, morph: 5.0, texture: 3.5, form: 6.0, air: 7.5, blend: 65, mode: 1 },
    { name: "Wide Layer", category: "Adlibs", tags: ["Wide", "Background", "Hybrid"], pitch: 0.0, morph: 4.0, texture: 2.5, form: 5.5, air: 6.0, blend: 55, mode: 2 },
    { name: "Soft Double", category: "Adlibs", tags: ["Natural", "Double", "Clean"], pitch: 0.0, morph: 3.0, texture: 1.8, form: 5.2, air: 5.5, blend: 45, mode: 0 },
    { name: "Dream Vocal", category: "Atmosphere", tags: ["Airy", "Floating", "Dreamy"], pitch: 0.0, morph: 5.5, texture: 3.0, form: 6.5, air: 8.0, blend: 70, mode: 2 },
    { name: "Ghost Layer", category: "Atmosphere", tags: ["Soft", "Distant", "Ethereal"], pitch: 0.0, morph: 6.0, texture: 2.5, form: 7.0, air: 7.5, blend: 60, mode: 2 },
    { name: "Ethereal FX", category: "Atmosphere", tags: ["Cinematic", "Extreme", "Airy"], pitch: 0.0, morph: 7.5, texture: 4.5, form: 6.8, air: 8.5, blend: 75, mode: 3 },
    { name: "Midnight Glow", category: "R&B / Pop", tags: ["Silky", "Emotional", "Modern"], pitch: 0.0, morph: 4.8, texture: 2.8, form: 5.8, air: 7.5, blend: 55, mode: 2 },
    { name: "Velvet Nights", category: "R&B / Pop", tags: ["Warm", "Intimate", "Clean"], pitch: 0.0, morph: 3.8, texture: 2.0, form: 4.8, air: 5.5, blend: 50, mode: 0 },
    { name: "Neon Soul", category: "R&B / Pop", tags: ["Futuristic", "R&B", "Digital"], pitch: 0.0, morph: 6.2, texture: 4.0, form: 6.2, air: 6.8, blend: 65, mode: 1 },
    { name: "Topline Polish", category: "Rap", tags: ["Clean", "Rap", "Modern"], pitch: 0.0, morph: 3.0, texture: 2.5, form: 5.2, air: 6.0, blend: 45, mode: 0 },
    { name: "Melodic Drip", category: "Rap", tags: ["Glossy", "Melodic", "Digital"], pitch: 0.0, morph: 5.5, texture: 3.5, form: 5.8, air: 7.0, blend: 60, mode: 1 },
    { name: "Auto Energy", category: "Rap", tags: ["Hyped", "Modern", "Hybrid"], pitch: 0.0, morph: 6.0, texture: 4.2, form: 5.5, air: 6.5, blend: 65, mode: 2 },
    { name: "Astro Tone", category: "FX Rap", tags: ["Wide", "Futuristic", "Digital"], pitch: 0.0, morph: 7.2, texture: 5.0, form: 6.8, air: 6.5, blend: 70, mode: 1 },
    { name: "Space Adlib", category: "FX Rap", tags: ["Floating", "Space", "Extreme"], pitch: 0.0, morph: 8.0, texture: 4.5, form: 7.2, air: 7.5, blend: 75, mode: 3 },
    { name: "Dark Bounce", category: "FX Rap", tags: ["Dark", "Heavy", "Extreme"], pitch: 0.0, morph: 6.5, texture: 4.0, form: 3.8, air: 5.0, blend: 65, mode: 3 },
    { name: "Soft Air", category: "Alt Pop", tags: ["Breathy", "Airy", "Hybrid"], pitch: 0.0, morph: 4.5, texture: 2.5, form: 6.5, air: 8.5, blend: 55, mode: 2 },
    { name: "Glass Pop", category: "Alt Pop", tags: ["Clean", "Polished", "Pop"], pitch: 0.0, morph: 3.5, texture: 2.0, form: 6.0, air: 7.5, blend: 50, mode: 0 },
    { name: "Alt Texture", category: "Alt Pop", tags: ["Gritty", "Alt", "Digital"], pitch: 0.0, morph: 5.8, texture: 4.5, form: 6.2, air: 6.0, blend: 60, mode: 1 },
    { name: "Hyper Voice", category: "Hyperpop", tags: ["Aggressive", "Bright", "Extreme"], pitch: 0.0, morph: 9.0, texture: 6.5, form: 8.0, air: 8.5, blend: 80, mode: 3 },
    { name: "Bubble Tone", category: "Hyperpop", tags: ["Playful", "Pitched", "Digital"], pitch: 0.0, morph: 7.5, texture: 5.5, form: 8.5, air: 7.5, blend: 75, mode: 1 },
    { name: "Digital Angel", category: "Hyperpop", tags: ["Airy", "Synthetic", "Hybrid"], pitch: 0.0, morph: 6.8, texture: 4.0, form: 7.2, air: 9.0, blend: 70, mode: 2 },
    { name: "Wide Stack", category: "Stacks", tags: ["Background", "Wide", "Hybrid"], pitch: 0.0, morph: 4.2, texture: 2.5, form: 5.8, air: 6.5, blend: 60, mode: 2 },
    { name: "Soft Layer", category: "Stacks", tags: ["Subtle", "Support", "Clean"], pitch: 0.0, morph: 3.0, texture: 1.8, form: 5.5, air: 5.5, blend: 45, mode: 0 },
    { name: "Ghost Double", category: "Stacks", tags: ["Airy", "Doubled", "Hybrid"], pitch: 0.0, morph: 5.5, texture: 2.8, form: 6.8, air: 7.5, blend: 65, mode: 2 },
    { name: "International Nova", category: "Signature", tags: ["Flagship", "Signature", "Digital"], pitch: 3.5, morph: 6.8, texture: 3.6, form: 6.4, air: 8.6, blend: 62, mode: 1 },
    { name: "Nova Signature", category: "Signature", tags: ["Flagship", "Glossy", "Digital"], pitch: 0.0, morph: 6.0, texture: 4.2, form: 5.8, air: 6.8, blend: 60, mode: 1 },
    { name: "Vocal Glow", category: "Signature", tags: ["Glossy", "Airy", "Hybrid"], pitch: 0.0, morph: 5.0, texture: 3.0, form: 5.5, air: 8.0, blend: 65, mode: 2 },
    { name: "Energy Mode", category: "Signature", tags: ["Aggressive", "Modern", "Extreme"], pitch: 0.0, morph: 8.5, texture: 6.0, form: 6.5, air: 7.0, blend: 80, mode: 3 }
  ];
  var currentValues = { pitch: 0.0, morph: 5.0, texture: 4.0, form: 5.5, air: 5.0, blend: 55.0, voice_mode: 2 };
  var parameterStates = {};
  var activeDrag = null;
  var currentCategory = "Signature";
  var currentPresetIdx = 0;
  var waveTick = 0;

  function getParamRange(id) {
    if (id === "pitch") return { min: -12, max: 12 };
    if (id === "blend") return { min: 0, max: 100 };
    if (id === "voice_mode") return { min: 0, max: 4 };
    return { min: 0, max: 10 };
  }

  function clamp(v, min, max) {
    return Math.max(min, Math.min(max, v));
  }

  function normalize(id, value) {
    if (id === "pitch") return (value + 12) / 24;
    if (id === "blend") return value / 100;
    if (id === "voice_mode") return value / 4;
    return value / 10;
  }

  function denormalize(id, value) {
    var c = clamp(value, 0, 1);
    if (id === "pitch") return -12 + c * 24;
    if (id === "blend") return c * 100;
    if (id === "voice_mode") return Math.round(c * 4);
    return c * 10;
  }

  function createFallbackSliderState(id) {
    if (!window.__JUCE__ || !window.__JUCE__.backend) return null;

    var identifier = "__juce__slider" + id;
    var range = getParamRange(id);
    var min = range.min;
    var max = range.max;
    var valueChangedListeners = [];
    var normalisedValue = normalize(id, currentValues[id]);

    window.__JUCE__.backend.addEventListener(identifier, function (event) {
      if (!event || event.eventType !== "valueChanged") return;
      var scaled = Number(event.value);
      var nv = (scaled - min) / (max - min);
      if (isFinite(nv)) normalisedValue = clamp(nv, 0, 1);
      for (var i = 0; i < valueChangedListeners.length; i++) valueChangedListeners[i]();
    });

    return {
      setNormalisedValue: function (newValue) {
        normalisedValue = clamp(Number(newValue), 0, 1);
        var scaled = min + normalisedValue * (max - min);
        window.__JUCE__.backend.emitEvent(identifier, {
          eventType: "valueChanged",
          value: scaled
        });
      },
      getNormalisedValue: function () {
        return normalisedValue;
      },
      sliderDragStarted: function () {
        window.__JUCE__.backend.emitEvent(identifier, { eventType: "sliderDragStarted" });
      },
      sliderDragEnded: function () {
        window.__JUCE__.backend.emitEvent(identifier, { eventType: "sliderDragEnded" });
      },
      valueChangedEvent: {
        addListener: function (fn) {
          valueChangedListeners.push(fn);
        }
      }
    };
  }

  function updateMode(mode) {
    currentValues.voice_mode = mode;
    var buttons = document.querySelectorAll(".mode-btn");
    for (var i = 0; i < buttons.length; i++) {
      var b = buttons[i];
      var m = parseInt(b.getAttribute("data-mode"), 10);
      if (m === mode) b.classList.add("active");
      else b.classList.remove("active");
    }
  }

  function updateKnob(id, value) {
    currentValues[id] = value;
    var col = document.querySelector('.knob-col[data-param="' + id + '"]');
    if (!col) return;

    var min = parseFloat(col.getAttribute("data-min") || (id === "pitch" ? "-12" : "0"));
    var max = parseFloat(col.getAttribute("data-max") || (id === "blend" ? "100" : id === "pitch" ? "12" : "10"));
    var frac = clamp((value - min) / (max - min), 0, 1);

    var fill = col.querySelector(".knob-fill");
    if (fill) fill.setAttribute("stroke-dasharray", (frac * 159.2).toFixed(1) + " 238.76");

    var ind = col.querySelector(".knob-indicator");
    if (ind) {
      var deg = -120 + frac * 240;
      ind.style.transform = "rotate(" + deg.toFixed(1) + "deg)";
    }

    var val = col.querySelector(".knob-value");
    if (val) {
      if (id === "blend") val.textContent = Math.round(value) + "%";
      else if (id === "pitch") val.textContent = (value >= 0 ? "+" : "") + value.toFixed(1) + " st";
      else val.textContent = value.toFixed(1);
    }
  }

  function pushParam(id, actualValue) {
    var st = parameterStates[id];
    if (!st) return;
    st.setNormalisedValue(normalize(id, actualValue));
  }

  function getFilteredPresets(category) {
    if (category === "All") return PRESETS;
    var out = [];
    for (var i = 0; i < PRESETS.length; i++) {
      if (PRESETS[i].category === category) out.push(PRESETS[i]);
    }
    return out;
  }

  function renderPresetDisplay(preset) {
    var nameEl = document.getElementById("preset-name");
    var tagsEl = document.getElementById("preset-tags");
    if (nameEl) nameEl.textContent = preset.name;
    if (tagsEl) {
      tagsEl.innerHTML = "";
      for (var i = 0; i < preset.tags.length; i++) {
        var tag = document.createElement("span");
        tag.className = "tag";
        tag.textContent = preset.tags[i];
        tagsEl.appendChild(tag);
      }
    }
  }

  function applyPreset(preset, pushToPlugin) {
    if (!preset) return;
    renderPresetDisplay(preset);

    var targets = {
      pitch: (typeof preset.pitch === "number") ? preset.pitch : 0,
      morph: preset.morph,
      texture: preset.texture,
      form: preset.form,
      air: preset.air,
      blend: preset.blend
    };

    for (var i = 0; i < KNOB_PARAMS.length; i++) {
      var id = KNOB_PARAMS[i];
      updateKnob(id, targets[id]);
      if (pushToPlugin) pushParam(id, targets[id]);
    }

    updateMode(preset.mode);
    if (pushToPlugin) pushParam("voice_mode", preset.mode);
  }

  function navigatePreset(direction) {
    var list = getFilteredPresets(currentCategory);
    if (!list.length) return;
    currentPresetIdx = (currentPresetIdx + direction + list.length) % list.length;
    applyPreset(list[currentPresetIdx], true);
  }

  function bindBrowser() {
    var sel = document.getElementById("category-select");
    var prev = document.getElementById("prev-preset");
    var next = document.getElementById("next-preset");

    if (sel) {
      sel.addEventListener("change", function () {
        currentCategory = sel.value;
        currentPresetIdx = 0;
        var list = getFilteredPresets(currentCategory);
        if (list.length) applyPreset(list[0], true);
      });
    }

    if (prev) prev.addEventListener("click", function () { navigatePreset(-1); });
    if (next) next.addEventListener("click", function () { navigatePreset(1); });
  }

  function bindModeButtons() {
    var buttons = document.querySelectorAll(".mode-btn");
    for (var i = 0; i < buttons.length; i++) {
      (function (btn) {
        function activate(ev) {
          if (ev && ev.preventDefault) ev.preventDefault();
          var mode = parseInt(btn.getAttribute("data-mode"), 10);
          updateMode(mode);
          pushParam("voice_mode", mode);
        }

        btn.addEventListener("click", activate);
        btn.addEventListener("touchend", activate);
      })(buttons[i]);
    }
  }

  function bindKnobs() {
    var cols = document.querySelectorAll(".knob-col[data-param]");
    for (var i = 0; i < cols.length; i++) {
      (function (col) {
        var id = col.getAttribute("data-param");
        var min = parseFloat(col.getAttribute("data-min") || "0");
        var max = parseFloat(col.getAttribute("data-max") || "10");

        function onStart(ev) {
          if (typeof ev.button === "number" && ev.button !== 0) return;
          if (ev.preventDefault) ev.preventDefault();
          activeDrag = {
            id: id,
            min: min,
            max: max,
            startY: (ev.touches && ev.touches[0]) ? ev.touches[0].clientY : ev.clientY,
            startVal: currentValues[id]
          };
          if (parameterStates[id] && parameterStates[id].sliderDragStarted) parameterStates[id].sliderDragStarted();
          col.classList.add("dragging");
        }

        col.addEventListener("mousedown", onStart);
        col.addEventListener("touchstart", onStart, { passive: false });

        col.addEventListener("dblclick", function () {
          var def = (id === "blend") ? 55 : (id === "pitch" ? 0 : 5);
          updateKnob(id, def);
          pushParam(id, def);
        });
      })(cols[i]);
    }

    function onMove(ev) {
      if (!activeDrag) return;
      if (ev.preventDefault) ev.preventDefault();
      var y = (ev.touches && ev.touches[0]) ? ev.touches[0].clientY : ev.clientY;
      var delta = (activeDrag.startY - y) * (activeDrag.max - activeDrag.min) * 0.005;
      var v = clamp(activeDrag.startVal + delta, activeDrag.min, activeDrag.max);
      updateKnob(activeDrag.id, v);
      pushParam(activeDrag.id, v);
    }

    function onEnd() {
      if (!activeDrag) return;
      var col = document.querySelector('.knob-col[data-param="' + activeDrag.id + '"]');
      if (col) col.classList.remove("dragging");
      if (parameterStates[activeDrag.id] && parameterStates[activeDrag.id].sliderDragEnded) {
        parameterStates[activeDrag.id].sliderDragEnded();
      }
      activeDrag = null;
    }

    window.addEventListener("mousemove", onMove, { passive: false });
    window.addEventListener("touchmove", onMove, { passive: false });
    window.addEventListener("mouseup", onEnd);
    window.addEventListener("touchend", onEnd);
    window.addEventListener("touchcancel", onEnd);
  }

  function connectParameters() {
    if (!window.__JUCE__ || !window.__JUCE__.backend) return;

    for (var i = 0; i < ALL_PARAMS.length; i++) {
      var id = ALL_PARAMS[i];
      parameterStates[id] = createFallbackSliderState(id);
    }

    for (var j = 0; j < KNOB_PARAMS.length; j++) {
      (function (id) {
        var st = parameterStates[id];
        if (!st || !st.valueChangedEvent || !st.valueChangedEvent.addListener) return;
        st.valueChangedEvent.addListener(function () {
          updateKnob(id, denormalize(id, st.getNormalisedValue()));
        });
        updateKnob(id, denormalize(id, st.getNormalisedValue()));
      })(KNOB_PARAMS[j]);
    }

    var modeState = parameterStates.voice_mode;
    if (modeState && modeState.valueChangedEvent && modeState.valueChangedEvent.addListener) {
      modeState.valueChangedEvent.addListener(function () {
        updateMode(denormalize("voice_mode", modeState.getNormalisedValue()));
      });
      updateMode(denormalize("voice_mode", modeState.getNormalisedValue()));
    }
  }

  function initSimpleWave() {
    var canvas = document.getElementById("voice-canvas");
    if (!canvas) return;
    var ctx = canvas.getContext("2d");
    if (!ctx) return;

    function resize() {
      var wrap = canvas.parentElement;
      if (!wrap) return;
      var r = wrap.getBoundingClientRect();
      canvas.width = Math.max(1, Math.round(r.width));
      canvas.height = Math.max(1, Math.round(r.height));
    }

    function draw() {
      waveTick += 0.04;
      var W = canvas.width;
      var H = canvas.height;
      if (W > 1 && H > 1) {
        var morph = currentValues.morph / 10;
        var texture = currentValues.texture / 10;
        var air = currentValues.air / 10;
        var amp = H * (0.10 + morph * 0.18 + texture * 0.06);

        ctx.clearRect(0, 0, W, H);
        ctx.fillStyle = "#030210";
        ctx.fillRect(0, 0, W, H);

        var grad = ctx.createLinearGradient(0, 0, W, 0);
        grad.addColorStop(0, "rgba(205,80,255,0.95)");
        grad.addColorStop(0.6, "rgba(50,190,255,0.95)");
        grad.addColorStop(1, "rgba(0,245,255,0.98)");
        ctx.strokeStyle = grad;
        ctx.lineWidth = 3;
        ctx.shadowBlur = 16;
        ctx.shadowColor = "rgba(110,200,255,0.7)";

        ctx.beginPath();
        for (var x = 0; x < W; x++) {
          var t = x / W;
          var y = H * 0.5
            + Math.sin(t * 16 + waveTick) * amp * 0.55
            + Math.sin(t * 32 - waveTick * (0.8 + air * 0.6)) * amp * 0.18;
          if (x === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.shadowBlur = 0;
      }
      requestAnimationFrame(draw);
    }

    resize();
    window.addEventListener("resize", resize);
    draw();
  }

  function bootFallbackUi() {
    for (var i = 0; i < KNOB_PARAMS.length; i++) {
      var id = KNOB_PARAMS[i];
      updateKnob(id, currentValues[id]);
    }
    updateMode(currentValues.voice_mode);

    var sel = document.getElementById("category-select");
    if (sel) sel.value = "Signature";

    var list = getFilteredPresets("Signature");
    if (list.length) {
      currentCategory = "Signature";
      currentPresetIdx = 0;
      applyPreset(list[0], false);
    }

    bindBrowser();
    bindKnobs();
    bindModeButtons();
    connectParameters();
    initSimpleWave();

    console.warn("Nova Voice fallback UI bootstrap active.");
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bootFallbackUi, { once: true });
  } else {
    bootFallbackUi();
  }
})();
