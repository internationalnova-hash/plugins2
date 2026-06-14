// Nova Chord — Fallback boot script
// Loads when the JUCE native bridge is not available (e.g. browser preview).
// Provides stub implementations so the UI doesn't throw errors.

(function () {
  'use strict';

  if (typeof window.__JUCE__ !== 'undefined') {
    // Native bridge is present — nothing to do.
    return;
  }

  console.warn('Nova Chord: JUCE native bridge not available — running in fallback mode');

  // Stub getNativeFunction if juce/index.js hasn't provided one yet
  if (typeof window.getNativeFunction === 'undefined') {
    window.getNativeFunction = function (name) {
      return function () {
        console.log('Fallback stub called:', name, Array.prototype.slice.call(arguments));
        return Promise.resolve(null);
      };
    };
  }

  // Stub getSliderState
  if (typeof window.getSliderState === 'undefined') {
    window.getSliderState = function () { return null; };
  }

  console.log('Nova Chord fallback stubs ready');
})();
