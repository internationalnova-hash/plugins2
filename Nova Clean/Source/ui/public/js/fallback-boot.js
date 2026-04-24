(function() {
  'use strict';

  if (window.__JUCE__ && window.__JUCE__.backend) return;

  if (!window.NativeBridge) {
    window.NativeBridge = {
      setParameter: function() {}
    };
  }
})();
