/**
 * Nova Chord — JUCE WebBrowser bridge (non-module, globally scoped)
 * Provides getNativeFunction() for calling C++ withNativeFunction() handlers.
 */
(function (global) {
  'use strict';

  // Promise handler for native function calls
  var lastPromiseId = 0;
  var promises = {};

  function setupCompleteListener() {
    if (!global.__JUCE__ || !global.__JUCE__.backend) return;
    global.__JUCE__.backend.addEventListener('__juce__complete', function (evt) {
      var p = promises[evt.promiseId];
      if (p) {
        p.resolve(evt.result);
        delete promises[evt.promiseId];
      }
    });
  }

  // Try immediately; retry after DOMContentLoaded in case JUCE inits late
  setupCompleteListener();
  document.addEventListener('DOMContentLoaded', setupCompleteListener);

  /**
   * Returns a function that calls a JUCE withNativeFunction() handler by name.
   * Usage: var fn = getNativeFunction('myFunc'); fn(arg1, arg2).then(result => ...);
   */
  global.getNativeFunction = function (name) {
    return function () {
      if (!global.__JUCE__ || !global.__JUCE__.backend) {
        console.warn('JUCE backend not available for native function:', name);
        return Promise.resolve(null);
      }

      var promiseId = lastPromiseId++;
      var result = new Promise(function (resolve, reject) {
        promises[promiseId] = { resolve: resolve, reject: reject };
      });

      global.__JUCE__.backend.emitEvent('__juce__invoke', {
        name: name,
        params: Array.prototype.slice.call(arguments),
        resultId: promiseId,
      });

      return result;
    };
  };

  // Expose slider state helper (used for parameter sync via WebSliderRelay)
  global.getSliderState = function (name) {
    if (!global.__JUCE__ || !global.__JUCE__.sliderState) return null;
    return global.__JUCE__.sliderState[name] || null;
  };

  console.log('Nova Chord JUCE bridge initialised');
})(window);
