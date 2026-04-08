(function () {
  var parameterStates = {};
  var modeNames = ["Studio", "Arena", "Dream", "Vintage"];
  var macroParams = ["space", "air", "depth", "mix", "width"];
  var presets = {
    Studio:  { space: 18, air: 32, depth: 26, mix: 16, width: 38 },
    Arena:   { space: 66, air: 60, depth: 70, mix: 28, width: 86 },
    Dream:   { space: 84, air: 64, depth: 82, mix: 34, width: 90 },
    Vintage: { space: 42, air: 20, depth: 52, mix: 24, width: 48 }
  };

  var currentValues = {
    space: presets.Studio.space,
    air: presets.Studio.air,
    depth: presets.Studio.depth,
    mix: presets.Studio.mix,
    width: presets.Studio.width
  };

  var juceAvailable = false;
  var activeDrag = null;

  function clamp(value, min, max) {
    var safeMin = typeof min === "number" ? min : 0;
    var safeMax = typeof max === "number" ? max : 100;
    return Math.min(safeMax, Math.max(safeMin, value));
  }

  function moveCaretToEnd(element) {
    if (!window.getSelection || !document.createRange) return;

    var selection = window.getSelection();
    var range = document.createRange();
    range.selectNodeContents(element);
    range.collapse(false);
    selection.removeAllRanges();
    selection.addRange(range);
  }

  function getDataAttribute(element, name) {
    if (!element) return "";
    if (element.dataset && Object.prototype.hasOwnProperty.call(element.dataset, name)) {
      return element.dataset[name];
    }
    return element.getAttribute("data-" + name) || "";
  }

  function updateVisual(param, percent) {
    var safeValue = clamp(Number(percent) || 0);
    var knob = document.getElementById(param + "-knob");
    var readout = document.getElementById(param + "-value");

    currentValues[param] = safeValue;

    if (knob) {
      var indicator = knob.querySelector(".indicator");
      var rotation = -120 + (safeValue / 100) * 240;
      if (indicator && indicator.style) {
        indicator.style.setProperty("--rotation", rotation + "deg");
      }
    }

    if (readout && document.activeElement !== readout) {
      readout.textContent = String(Math.round(safeValue)) + "%";
    }
  }

  function createListenerList() {
    return {
      listeners: [],
      addListener: function (fn) {
        this.listeners.push(fn);
        return this.listeners.length - 1;
      },
      callListeners: function (payload) {
        for (var i = 0; i < this.listeners.length; i += 1) {
          this.listeners[i](payload);
        }
      }
    };
  }

  function createSliderState(name) {
    if (!juceAvailable || !window.__JUCE__ || !window.__JUCE__.backend) return null;

    var backend = window.__JUCE__.backend;
    var state = {
      name: name,
      identifier: "__juce__slider" + name,
      scaledValue: 0,
      properties: {
        start: 0,
        end: 1,
        skew: 1,
        interval: 0,
        numSteps: 100
      },
      valueChangedEvent: createListenerList(),
      getNormalisedValue: function () {
        var start = typeof this.properties.start === "number" ? this.properties.start : 0;
        var end = typeof this.properties.end === "number" ? this.properties.end : 1;
        var range = end - start;

        if (range === 0) return 0;
        return clamp((this.scaledValue - start) / range, 0, 1);
      },
      setNormalisedValue: function (newValue) {
        var normalised = clamp(newValue, 0, 1);
        var start = typeof this.properties.start === "number" ? this.properties.start : 0;
        var end = typeof this.properties.end === "number" ? this.properties.end : 1;
        var interval = typeof this.properties.interval === "number" ? this.properties.interval : 0;
        var scaled = start + (normalised * (end - start));

        if (interval > 0) {
          scaled = Math.round(scaled / interval) * interval;
        }

        this.scaledValue = scaled;
        backend.emitEvent(this.identifier, {
          eventType: "valueChanged",
          value: scaled
        });
      },
      sliderDragStarted: function () {
        backend.emitEvent(this.identifier, { eventType: "sliderDragStarted" });
      },
      sliderDragEnded: function () {
        backend.emitEvent(this.identifier, { eventType: "sliderDragEnded" });
      }
    };

    backend.addEventListener(state.identifier, function (event) {
      if (!event) return;

      if (event.eventType === "valueChanged") {
        if (typeof event.value === "number") {
          state.scaledValue = event.value;
        }
        state.valueChangedEvent.callListeners(event);
      }

      if (event.eventType === "propertiesChanged") {
        var updatedProperties = {};
        for (var key in event) {
          if (Object.prototype.hasOwnProperty.call(event, key) && key !== "eventType") {
            updatedProperties[key] = event[key];
          }
        }
        state.properties = updatedProperties;
      }
    });

    backend.emitEvent(state.identifier, { eventType: "requestInitialUpdate" });
    return state;
  }

  function pushToBackend(param, percent, useGesture) {
    var state = parameterStates[param];
    if (!state) return;

    try {
      if (useGesture) state.sliderDragStarted();
      state.setNormalisedValue(clamp(percent) / 100);
      if (useGesture) state.sliderDragEnded();
    } catch (error) {
      console.warn("Failed to update " + param + ":", error && error.message ? error.message : error);
    }
  }

  function setParamPercent(param, percent, options) {
    var safeValue = clamp(percent);
    var settings = options || {};

    updateVisual(param, safeValue);

    if (settings.pushToBackend) {
      pushToBackend(param, safeValue, !!settings.useGesture);
    }
  }

  function setActiveMode(modeName) {
    var buttons = document.querySelectorAll(".mode-button");
    Array.prototype.forEach.call(buttons, function (button) {
      button.classList.toggle("active", getDataAttribute(button, "mode") === modeName);
    });
  }

  function applyPreset(modeName, pushModeToBackend) {
    var preset = presets[modeName];
    var shouldPush = pushModeToBackend !== false;

    if (!preset) return;

    setActiveMode(modeName);

    Array.prototype.forEach.call(macroParams, function (param) {
      setParamPercent(param, preset[param], { pushToBackend: shouldPush });
    });

    if (shouldPush && parameterStates.nova_mode) {
      try {
        var modeIndex = modeNames.indexOf(modeName);
        parameterStates.nova_mode.setNormalisedValue(modeIndex / (modeNames.length - 1));
      } catch (error) {
        console.warn("Failed to set mode:", error && error.message ? error.message : error);
      }
    }
  }

  function initialiseParameterStates() {
    if (!juceAvailable) return;

    Array.prototype.forEach.call(macroParams.concat(["nova_mode"]), function (param) {
      parameterStates[param] = createSliderState(param);
    });

    Array.prototype.forEach.call(macroParams, function (param) {
      var state = parameterStates[param];
      if (!state || !state.valueChangedEvent) return;

      state.valueChangedEvent.addListener(function () {
        updateVisual(param, state.getNormalisedValue() * 100);
      });
    });

    if (parameterStates.nova_mode && parameterStates.nova_mode.valueChangedEvent) {
      parameterStates.nova_mode.valueChangedEvent.addListener(function () {
        var modeIndex = Math.round(parameterStates.nova_mode.getNormalisedValue() * (modeNames.length - 1));
        setActiveMode(modeNames[clamp(modeIndex, 0, modeNames.length - 1)]);
      });
    }

    window.setTimeout(function () {
      Array.prototype.forEach.call(macroParams, function (param) {
        var state = parameterStates[param];
        if (state) {
          updateVisual(param, state.getNormalisedValue() * 100);
        }
      });

      if (parameterStates.nova_mode) {
        var modeIndex = Math.round(parameterStates.nova_mode.getNormalisedValue() * (modeNames.length - 1));
        setActiveMode(modeNames[clamp(modeIndex, 0, modeNames.length - 1)]);
      }
    }, 60);
  }

  function getClientY(event) {
    if (!event) return 0;
    if (event.touches && event.touches.length > 0) return event.touches[0].clientY;
    if (event.changedTouches && event.changedTouches.length > 0) return event.changedTouches[0].clientY;
    return typeof event.clientY === "number" ? event.clientY : 0;
  }

  function startDrag(param, knob, event, defaultPercent) {
    if (event && typeof event.button === "number" && event.button !== 0) return;

    if (event && event.preventDefault) event.preventDefault();
    if (event && event.stopPropagation) event.stopPropagation();

    knob.classList.add("dragging");
    activeDrag = {
      param: param,
      knob: knob,
      pointerId: event && typeof event.pointerId !== "undefined" ? event.pointerId : null,
      startY: getClientY(event),
      startValue: typeof currentValues[param] === "number" ? currentValues[param] : defaultPercent
    };

    if (typeof knob.setPointerCapture === "function" && event && typeof event.pointerId !== "undefined") {
      try {
        knob.setPointerCapture(event.pointerId);
      } catch (error) {
        console.warn(error && error.message ? error.message : error);
      }
    }

    if (parameterStates[param]) {
      try {
        parameterStates[param].sliderDragStarted();
      } catch (error) {
        console.warn(error && error.message ? error.message : error);
      }
    }
  }

  function updateActiveDrag(event) {
    if (!activeDrag) return;
    if (activeDrag.pointerId !== null && event && typeof event.pointerId !== "undefined" && event.pointerId !== activeDrag.pointerId) {
      return;
    }

    if (event && event.preventDefault) event.preventDefault();

    var deltaY = activeDrag.startY - getClientY(event);
    var nextValue = clamp(activeDrag.startValue + (deltaY * 0.45));
    setParamPercent(activeDrag.param, nextValue, { pushToBackend: true });
  }

  function finishActiveDrag(event) {
    if (!activeDrag) return;
    if (activeDrag.pointerId !== null && event && typeof event.pointerId !== "undefined" && event.pointerId !== activeDrag.pointerId) {
      return;
    }

    if (parameterStates[activeDrag.param]) {
      try {
        parameterStates[activeDrag.param].sliderDragEnded();
      } catch (error) {
        console.warn(error && error.message ? error.message : error);
      }
    }

    if (typeof activeDrag.knob.releasePointerCapture === "function" && activeDrag.pointerId !== null) {
      try {
        activeDrag.knob.releasePointerCapture(activeDrag.pointerId);
      } catch (error) {
        console.warn(error && error.message ? error.message : error);
      }
    }

    activeDrag.knob.classList.remove("dragging");
    activeDrag = null;
  }

  function initialiseKnobs() {
    var knobs = document.querySelectorAll(".macro-knob");

    Array.prototype.forEach.call(knobs, function (knob) {
      var param = getDataAttribute(knob, "param");
      var defaultPercent = Number(getDataAttribute(knob, "default") || 0);

      updateVisual(param, typeof currentValues[param] === "number" ? currentValues[param] : defaultPercent);

      var startHandler = function (event) {
        startDrag(param, knob, event, defaultPercent);
      };

      knob.addEventListener("pointerdown", startHandler);
      knob.addEventListener("mousedown", startHandler);
      knob.addEventListener("touchstart", startHandler, { passive: false });

      knob.addEventListener("dblclick", function () {
        setParamPercent(param, defaultPercent, { pushToBackend: true });
      });
    });

    document.addEventListener("pointermove", updateActiveDrag);
    document.addEventListener("mousemove", updateActiveDrag);
    document.addEventListener("touchmove", updateActiveDrag, { passive: false });

    document.addEventListener("pointerup", finishActiveDrag);
    document.addEventListener("mouseup", finishActiveDrag);
    document.addEventListener("touchend", finishActiveDrag);
    document.addEventListener("pointercancel", finishActiveDrag);
    document.addEventListener("touchcancel", finishActiveDrag);
    document.addEventListener("mouseleave", finishActiveDrag);
  }

  function initialiseReadouts() {
    var readouts = document.querySelectorAll(".editable-value");

    Array.prototype.forEach.call(readouts, function (readout) {
      var param = getDataAttribute(readout, "param");

      readout.addEventListener("focus", function () {
        readout.textContent = String(Math.round(typeof currentValues[param] === "number" ? currentValues[param] : 0));
        moveCaretToEnd(readout);
      });

      readout.addEventListener("input", function () {
        var cleaned = readout.textContent.replace(/[^0-9.]/g, "");
        if (readout.textContent !== cleaned) {
          readout.textContent = cleaned;
          moveCaretToEnd(readout);
        }
      });

      readout.addEventListener("keydown", function (event) {
        if (event.key === "Enter") {
          event.preventDefault();
          readout.blur();
        }

        if (event.key === "Escape") {
          event.preventDefault();
          updateVisual(param, typeof currentValues[param] === "number" ? currentValues[param] : 0);
          readout.blur();
        }
      });

      readout.addEventListener("blur", function () {
        var raw = readout.textContent.replace(/[^0-9.]/g, "");
        var nextValue = raw === "" ? (typeof currentValues[param] === "number" ? currentValues[param] : 0) : clamp(Number(raw));
        setParamPercent(param, nextValue, { pushToBackend: true });
      });
    });
  }

  function initialiseModeButtons() {
    var buttons = document.querySelectorAll(".mode-button");

    Array.prototype.forEach.call(buttons, function (button) {
      button.addEventListener("click", function (event) {
        if (event && event.preventDefault) event.preventDefault();
        applyPreset(getDataAttribute(button, "mode"), true);
      });
    });
  }

  function bootUi() {
    juceAvailable = !!(window.__JUCE__ && window.__JUCE__.backend && typeof window.__JUCE__.backend.emitEvent === "function");

    initialiseKnobs();
    initialiseReadouts();
    initialiseModeButtons();

    if (juceAvailable) {
      try {
        initialiseParameterStates();
      } catch (error) {
        juceAvailable = false;
        console.warn("Parameter sync unavailable, keeping local UI active:", error && error.message ? error.message : error);
        applyPreset("Studio", false);
      }
    } else {
      applyPreset("Studio", false);
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", bootUi, { once: true });
  } else {
    bootUi();
  }
}());
