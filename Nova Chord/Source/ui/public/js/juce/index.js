// JUCE Native Interop - Bridges JUCE backend to JavaScript UI
// This file is auto-injected by JUCE's WebView when native methods are available

(function() {
    'use strict';

    // Detect JUCE backend availability
    if (typeof window === 'undefined') return;

    const juceBackendAvailable = () => {
        return window.__JUCE && window.__JUCE.sendMessage !== undefined;
    };

    // Create bridge API
    window.JuceAPI = {
        parameters: {},

        // Send message to JUCE backend
        send: (command, data) => {
            if (!juceBackendAvailable()) {
                console.warn('JUCE backend not available for command:', command);
                return null;
            }
            try {
                const msg = JSON.stringify({ command, data });
                return window.__JUCE.sendMessage(msg);
            } catch (e) {
                console.error('Error sending message to JUCE:', e);
                return null;
            }
        },

        // Set plugin parameter
        setParameter: (paramName, value) => {
            return window.JuceAPI.send('setParameter', { parameter: paramName, value });
        },

        // Get plugin parameter
        getParameter: (paramName) => {
            return window.JuceAPI.send('getParameter', { parameter: paramName });
        },

        // Update parameter sync with JUCE state tree
        syncParameter: (paramName, value) => {
            window.JuceAPI.parameters[paramName] = value;
            return window.JuceAPI.setParameter(paramName, value);
        },

        // Request pitch data
        getPitchData: () => {
            return window.JuceAPI.send('getPitchData', {});
        }
    };

    // Global bridge for backward compatibility
    window.NativeBridge = window.JuceAPI;

    console.log('JUCE Interop initialized');
})();
