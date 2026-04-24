(function() {
  'use strict';

  if (typeof window === 'undefined') return;

  window.JuceAPI = {
    send: function(command, data) {
      if (!window.__JUCE__ || !window.__JUCE__.backend) return null;
      try {
        return window.__JUCE__.backend.postMessage(JSON.stringify({ command: command, data: data }));
      } catch (e) {
        console.warn('JUCE send failed', e);
        return null;
      }
    },
    setParameter: function(parameter, value) {
      if (!window.__JUCE__ || !window.__JUCE__.backend) return;
      window.__JUCE__.backend.emitEvent(`__juce__slider${parameter}`, {
        eventType: 'valueChanged',
        value: value,
      });
    }
  };

  window.NativeBridge = window.JuceAPI;
})();
