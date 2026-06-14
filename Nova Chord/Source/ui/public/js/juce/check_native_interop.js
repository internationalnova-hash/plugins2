/**
 * Nova Chord — JUCE native interop check
 * Verifies that the JUCE backend is available and logs its state.
 */
(function () {
  'use strict';

  function checkJuceBackend() {
    if (typeof window.__JUCE__ !== 'undefined') {
      console.log('JUCE native backend detected — native mode active');
      if (window.__JUCE__.initialisationData) {
        console.log('JUCE functions:', window.__JUCE__.initialisationData.__juce__functions || []);
      }
      window.juceNativeMode = true;
    } else {
      console.log('JUCE native backend not detected — fallback mode active');
      window.juceNativeMode = false;
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', checkJuceBackend);
  } else {
    checkJuceBackend();
  }
})();
