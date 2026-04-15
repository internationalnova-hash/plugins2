// Fallback boot script - loads when native bridge is not available
console.warn('JUCE native bridge not available - running in fallback mode');

// Mock implementations for testing
window.juceAPI = {
    parameters: {
        key: 0,
        scale: 0,
        tolerance: 0,
        amount: 0,
        confidenceThreshold: 0,
        vibrato: 0,
        formant: 0,
        lowLatency: 0,
        detectedPitch: 0,
        correctedPitch: 0
    },

    setParameter: function(param, value) {
        if (this.parameters.hasOwnProperty(param)) {
            this.parameters[param] = value;
            console.log('Set ' + param + ' to ' + value);
        }
    },

    getParameter: function(param) {
        return this.parameters[param] || 0;
    },

    updatePitchData: function(detected, corrected) {
        this.parameters.detectedPitch = detected;
        this.parameters.correctedPitch = corrected;
    }
};

// Expose to window
window.NativeBridge = {
    callNativeFunction: function(name, data) {
        if (name === 'setParameter' && data.parameter) {
            window.juceAPI.setParameter(data.parameter, data.value);
        } else if (name === 'getParameter' && data.parameter) {
            return window.juceAPI.getParameter(data.parameter);
        }
        return null;
    }
};

console.log('Fallback API initialized');
