// JUCE Native Interop Check
// Verifies availability of JUCE backend and sets up appropriate mode

(function() {
    'use strict';

    const checkJuceBackend = () => {
        return typeof window !== 'undefined' && window.__JUCE !== undefined;
    };

    const initMode = () => {
        if (checkJuceBackend()) {
            console.log('JUCE native backend detected - running in native mode');
            window.juceNativeMode = true;
        } else {
            console.log('JUCE native backend not detected - fallback mode active');
            window.juceNativeMode = false;
        }
    };

    // Wait for document to be ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initMode);
    } else {
        initMode();
    }
})();
