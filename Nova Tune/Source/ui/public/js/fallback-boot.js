(function () {
  if (window.__novaTuneBooted) return;
  window.__novaTuneBooted = true;

  var KNOB_PARAMS = ["pitch", "morph", "texture", "form", "air", "blend"];
  var ALL_PARAMS = ["pitch", "morph", "texture", "form", "air", "blend", "voice_mode"];
  var PRESETS = [
    { name: "Nova Prime", category: "Signature", tags: ["Signature", "Mutate", "Lead"], pitch: 0.0, morph: 5.6, texture: 2.8, form: 5.4, air: 4.6, blend: 68, mode: 2 },
    { name: "Chrome Saint", category: "Chrome", tags: ["Glossy", "Shiny", "Synthetic"], pitch: 3.0, morph: 4.8, texture: 3.5, form: 6.3, air: 5.0, blend: 72, mode: 1 },
    { name: "Lead Melt", category: "Lead", tags: ["Wide", "Modern", "Lead"], pitch: 2.0, morph: 6.0, texture: 2.2, form: 4.8, air: 4.4, blend: 66, mode: 2 },
    { name: "Monster Vox", category: "Creature", tags: ["Huge", "Dark", "Monster"], pitch: -5.0, morph: 7.8, texture: 4.6, form: 2.8, air: 2.5, blend: 84, mode: 3 },
    { name: "Android Air", category: "Android", tags: ["Robotic", "Sharp", "Future"], pitch: 1.0, morph: 6.2, texture: 5.1, form: 6.8, air: 6.5, blend: 74, mode: 4 },
    { name: "Choir Glass", category: "Choir", tags: ["Layered", "Wide", "Dreamy"], pitch: 5.0, morph: 4.2, texture: 1.6, form: 5.9, air: 5.8, blend: 64, mode: 0 },
    { name: "Warp Child", category: "Warp", tags: ["Bent", "Elastic", "Alien"], pitch: 7.0, morph: 7.4, texture: 3.2, form: 7.3, air: 4.8, blend: 78, mode: 2 },
    { name: "Atmos Bloom", category: "Atmosphere", tags: ["Airy", "Float", "Soft"], pitch: 0.0, morph: 4.4, texture: 1.8, form: 6.1, air: 7.4, blend: 62, mode: 0 }
  ];
  var currentValues = { pitch: 0.0, morph: 4.8, texture: 2.5, form: 5.0, air: 4.2, blend: 60.0, voice_mode: 2 };
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

  function initStarfield() {
    var bg = document.getElementById("bg-canvas");
    if (!bg) return;

    var bctx = bg.getContext("2d");
    if (!bctx) return;

    function drawStarfield() {
      var wrap = bg.parentElement && bg.parentElement.parentElement ? bg.parentElement.parentElement : null;
      var rect = wrap ? wrap.getBoundingClientRect() : null;
      var width = Math.max(1, Math.round(rect ? rect.width : 1080));
      var height = Math.max(1, Math.round(rect ? rect.height : 680));

      bg.width = width;
      bg.height = height;
      bctx.clearRect(0, 0, width, height);

      var cloudA = bctx.createRadialGradient(width * 0.22, height * 0.52, 10, width * 0.22, height * 0.52, Math.max(width, height) * 0.38);
      cloudA.addColorStop(0, "rgba(178, 60, 255, 0.30)");
      cloudA.addColorStop(0.35, "rgba(135, 45, 225, 0.16)");
      cloudA.addColorStop(0.65, "rgba(90, 25, 185, 0.07)");
      cloudA.addColorStop(1, "rgba(0,0,0,0)");
      bctx.fillStyle = cloudA;
      bctx.fillRect(0, 0, width, height);

      var cloudB = bctx.createRadialGradient(width * 0.80, height * 0.32, 8, width * 0.80, height * 0.32, Math.max(width, height) * 0.32);
      cloudB.addColorStop(0, "rgba(55, 200, 255, 0.24)");
      cloudB.addColorStop(0.40, "rgba(28, 130, 225, 0.13)");
      cloudB.addColorStop(0.70, "rgba(10, 68, 185, 0.05)");
      cloudB.addColorStop(1, "rgba(0,0,0,0)");
      bctx.fillStyle = cloudB;
      bctx.fillRect(0, 0, width, height);

      var cloudC = bctx.createRadialGradient(width * 0.52, height * 0.46, 5, width * 0.52, height * 0.46, Math.max(width, height) * 0.45);
      cloudC.addColorStop(0, "rgba(125, 55, 245, 0.20)");
      cloudC.addColorStop(0.45, "rgba(85, 32, 205, 0.10)");
      cloudC.addColorStop(1, "rgba(0,0,0,0)");
      bctx.fillStyle = cloudC;
      bctx.fillRect(0, 0, width, height);

      for (var i = 0; i < 620; i++) {
        var x = Math.random() * width;
        var y = Math.random() * height;
        var radius = Math.random() * 1.45 + 0.14;
        var alpha = (Math.random() * 0.52 + 0.03).toFixed(2);
        bctx.beginPath();
        bctx.arc(x, y, radius, 0, Math.PI * 2);
        bctx.fillStyle = "rgba(255,255,255," + alpha + ")";
        bctx.fill();
      }

      for (var j = 0; j < 58; j++) {
        var sx = Math.random() * width;
        var sy = Math.random() * height;
        bctx.shadowBlur = 12;
        bctx.shadowColor = "rgba(185,205,255,0.85)";
        bctx.beginPath();
        bctx.arc(sx, sy, Math.random() * 1.35 + 0.95, 0, Math.PI * 2);
        bctx.fillStyle = "rgba(232,240,255,0.90)";
        bctx.fill();
        bctx.shadowBlur = 0;
      }
    }

    drawStarfield();
    window.addEventListener("resize", drawStarfield);
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
          var defaults = { pitch: 0, morph: 4.8, texture: 2.5, form: 5.0, air: 4.2, blend: 60 };
          var def = defaults[id];
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
    initStarfield();
    initSimpleWave();

    console.warn("Nova Tune fallback UI bootstrap active.");
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bootFallbackUi, { once: true });
  } else {
    bootFallbackUi();
  }
})();
