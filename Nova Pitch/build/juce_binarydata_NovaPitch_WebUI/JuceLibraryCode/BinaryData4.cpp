/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace nova_pitch_BinaryData
{

//================== fallback-boot.js ==================
static const unsigned char temp_binary_data_3[] =
"// Fallback boot script - loads when native bridge is not available\n"
"console.warn('JUCE native bridge not available - running in fallback mode');\n"
"\n"
"// Mock implementations for testing\n"
"window.juceAPI = {\n"
"    parameters: {\n"
"        key: 0,\n"
"        scale: 0,\n"
"        tolerance: 0,\n"
"        amount: 0,\n"
"        confidenceThreshold: 0,\n"
"        vibrato: 0,\n"
"        formant: 0,\n"
"        lowLatency: 0,\n"
"        detectedPitch: 0,\n"
"        correctedPitch: 0\n"
"    },\n"
"\n"
"    setParameter: function(param, value) {\n"
"        if (this.parameters.hasOwnProperty(param)) {\n"
"            this.parameters[param] = value;\n"
"            console.log('Set ' + param + ' to ' + value);\n"
"        }\n"
"    },\n"
"\n"
"    getParameter: function(param) {\n"
"        return this.parameters[param] || 0;\n"
"    },\n"
"\n"
"    updatePitchData: function(detected, corrected) {\n"
"        this.parameters.detectedPitch = detected;\n"
"        this.parameters.correctedPitch = corrected;\n"
"    }\n"
"};\n"
"\n"
"// Expose to window\n"
"window.NativeBridge = {\n"
"    callNativeFunction: function(name, data) {\n"
"        if (name === 'setParameter' && data.parameter) {\n"
"            window.juceAPI.setParameter(data.parameter, data.value);\n"
"        } else if (name === 'getParameter' && data.parameter) {\n"
"            return window.juceAPI.getParameter(data.parameter);\n"
"        }\n"
"        return null;\n"
"    }\n"
"};\n"
"\n"
"console.log('Fallback API initialized');\n";

const char* fallbackboot_js = (const char*) temp_binary_data_3;
}
