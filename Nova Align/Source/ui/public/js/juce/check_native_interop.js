// Development utility to verify JUCE native integration
(function () {
  function checkNativeInterop() {
    if (typeof window.__JUCE__ !== "undefined") {
      console.log("Nova Align: JUCE native backend detected");
      return true;
    }
    console.warn("Nova Align: JUCE native backend NOT detected (browser preview mode)");
    return false;
  }
  checkNativeInterop();
})();
