'use strict';

// ── Nova Chord JS Index ──────────────────────────────────────────
// Minimal module-style entry point — the main UI logic lives inline
// in index.html. This file satisfies the BinaryData requirement so
// the resource provider can serve it, and wires up any JUCE
// parameter relays that live outside the HTML script block.

const paramBridges = {};

function hasJuceBackend() {
  return typeof window !== 'undefined' && !!(window.__JUCE__ && window.__JUCE__.backend);
}

function getParamRange(param) {
  if (param === 'styleIdx')    return { min: 0,  max: 11 };
  if (param === 'octaveShift') return { min: -2, max: 2  };
  if (param === 'velocity')    return { min: 1,  max: 127 };
  if (param === 'passThrough') return { min: 0,  max: 1  };
  return { min: 0, max: 100 };
}

function getParamBridge(param) {
  if (paramBridges[param]) return paramBridges[param];
  if (!hasJuceBackend()) return null;

  const identifier = `__juce__slider${param}`;
  const range = getParamRange(param);

  const bridge = {
    setValue(actualValue) {
      const clamped = Math.max(range.min, Math.min(range.max, actualValue));
      window.__JUCE__.backend.emitEvent(identifier, {
        eventType: 'valueChanged',
        value: clamped,
      });
    },
    sliderDragStarted() {
      window.__JUCE__.backend.emitEvent(identifier, { eventType: 'sliderDragStarted' });
    },
    sliderDragEnded() {
      window.__JUCE__.backend.emitEvent(identifier, { eventType: 'sliderDragEnded' });
    },
  };

  paramBridges[param] = bridge;
  return bridge;
}

function syncParam(param, value) {
  const bridge = getParamBridge(param);
  if (bridge) bridge.setValue(value);
}

function requestInitialHostParameters() {
  if (!hasJuceBackend()) return;

  const params = ['styleIdx', 'octaveShift', 'velocity', 'passThrough'];

  params.forEach((param) => {
    const identifier = `__juce__slider${param}`;

    window.__JUCE__.backend.addEventListener(identifier, (event) => {
      if (!event || event.eventType !== 'valueChanged' || typeof event.value !== 'number')
        return;
      // Dispatch a custom event so the inline HTML script can react
      window.dispatchEvent(new CustomEvent('nova-chord-param', {
        detail: { param, value: event.value }
      }));
    });

    window.__JUCE__.backend.emitEvent(identifier, { eventType: 'requestInitialUpdate' });
  });
}

// DSP telemetry receiver — called by PluginEditor timerCallback via evaluateJavascript
window.receiveChordDSP = function(data) {
  if (!data || typeof data !== 'object') return;
  window.dispatchEvent(new CustomEvent('nova-chord-dsp', { detail: data }));
};

// Expose sync helper globally so the inline HTML script can use it
window.__novaChordSync = syncParam;

// Boot
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', requestInitialHostParameters);
} else {
  requestInitialHostParameters();
}
