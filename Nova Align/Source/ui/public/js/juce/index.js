// Nova Align — JUCE WebView native bridge.
//
// The real JUCE-generated bridge (juce_audio_processors' WebBrowserComponent
// JS support) ships as an ES module, but loading it via `type="module"`
// through JUCE's resource-provider scheme hasn't proven reliable in this
// codebase's plugins — every shipped plugin here instead talks directly to
// the same underlying `window.__JUCE__.backend` event bridge with a plain
// script. This file does that, implementing the same wire protocol the real
// bridge uses (verified by reading it): `__juce__invoke` / `__juce__complete`
// for native functions, `__juce__slider<name>` for WebSliderRelay sync.
(function (global) {
  "use strict";

  if (typeof global.__JUCE__ === "undefined") {
    console.warn("Nova Align: window.__JUCE__ not present — native bridge unavailable (browser preview mode)");
    return;
  }

  var backend = global.__JUCE__.backend;
  var lastPromiseId = 0;
  var pendingPromises = {};

  backend.addEventListener("__juce__complete", function (event) {
    var entry = pendingPromises[event.promiseId];
    if (entry) {
      entry.resolve(event.result);
      delete pendingPromises[event.promiseId];
    }
  });

  global.getNativeFunction = function (name) {
    return function () {
      var promiseId = lastPromiseId++;
      var args = Array.prototype.slice.call(arguments);
      var promise = new Promise(function (resolve) {
        pendingPromises[promiseId] = { resolve: resolve };
      });
      backend.emitEvent("__juce__invoke", { name: name, params: args, resultId: promiseId });
      return promise;
    };
  };

  var sliderStates = {};

  function createSliderState(name) {
    var identifier = "__juce__slider" + name;
    var state = {
      name: name,
      identifier: identifier,
      scaledValue: 0,
      properties: { start: 0, end: 1, skew: 1, interval: 0, numSteps: 100 },
      listeners: [],
      addListener: function (fn) {
        this.listeners.push(fn);
      },
      callListeners: function () {
        for (var i = 0; i < this.listeners.length; i += 1) this.listeners[i]();
      },
      getNormalisedValue: function () {
        var range = this.properties.end - this.properties.start;
        if (range === 0) return 0;
        return Math.pow((this.scaledValue - this.properties.start) / range, this.properties.skew);
      },
      getScaledValue: function () {
        return this.scaledValue;
      },
      setNormalisedValue: function (norm) {
        var clamped = Math.max(0, Math.min(1, norm));
        var scaled = Math.pow(clamped, 1 / this.properties.skew) * (this.properties.end - this.properties.start) + this.properties.start;
        var interval = this.properties.interval;
        if (interval > 0)
          scaled = this.properties.start + interval * Math.round((scaled - this.properties.start) / interval);
        this.scaledValue = scaled;
        backend.emitEvent(this.identifier, { eventType: "valueChanged", value: scaled });
      },
      sliderDragStarted: function () {
        backend.emitEvent(this.identifier, { eventType: "sliderDragStarted" });
      },
      sliderDragEnded: function () {
        backend.emitEvent(this.identifier, { eventType: "sliderDragEnded" });
      }
    };

    backend.addEventListener(identifier, function (event) {
      if (!event) return;
      if (event.eventType === "valueChanged") {
        if (typeof event.value === "number") state.scaledValue = event.value;
        state.callListeners();
      } else if (event.eventType === "propertiesChanged") {
        var props = {};
        for (var key in event) {
          if (key !== "eventType") props[key] = event[key];
        }
        state.properties = props;
        state.callListeners();
      }
    });

    backend.emitEvent(identifier, { eventType: "requestInitialUpdate" });
    return state;
  }

  global.getSliderState = function (name) {
    if (!sliderStates[name]) sliderStates[name] = createSliderState(name);
    return sliderStates[name];
  };

  console.log("Nova Align: JUCE bridge initialised");
})(window);
