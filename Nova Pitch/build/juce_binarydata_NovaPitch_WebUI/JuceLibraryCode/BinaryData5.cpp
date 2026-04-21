/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace nova_pitch_BinaryData
{

//================== index.js ==================
static const unsigned char temp_binary_data_4[] =
"// JUCE Native Interop - Bridges JUCE backend to JavaScript UI\n"
"// This file is auto-injected by JUCE's WebView when native methods are available\n"
"\n"
"(function() {\n"
"    'use strict';\n"
"\n"
"    // Detect JUCE backend availability\n"
"    if (typeof window === 'undefined') return;\n"
"\n"
"    const juceBackendAvailable = () => {\n"
"        return window.__JUCE && window.__JUCE.sendMessage !== undefined;\n"
"    };\n"
"\n"
"    // Create bridge API\n"
"    window.JuceAPI = {\n"
"        parameters: {},\n"
"\n"
"        // Send message to JUCE backend\n"
"        send: (command, data) => {\n"
"            if (!juceBackendAvailable()) {\n"
"                console.warn('JUCE backend not available for command:', command);\n"
"                return null;\n"
"            }\n"
"            try {\n"
"                const msg = JSON.stringify({ command, data });\n"
"                return window.__JUCE.sendMessage(msg);\n"
"            } catch (e) {\n"
"                console.error('Error sending message to JUCE:', e);\n"
"                return null;\n"
"            }\n"
"        },\n"
"\n"
"        // Set plugin parameter\n"
"        setParameter: (paramName, value) => {\n"
"            return window.JuceAPI.send('setParameter', { parameter: paramName, value });\n"
"        },\n"
"\n"
"        // Get plugin parameter\n"
"        getParameter: (paramName) => {\n"
"            return window.JuceAPI.send('getParameter', { parameter: paramName });\n"
"        },\n"
"\n"
"        // Update parameter sync with JUCE state tree\n"
"        syncParameter: (paramName, value) => {\n"
"            window.JuceAPI.parameters[paramName] = value;\n"
"            return window.JuceAPI.setParameter(paramName, value);\n"
"        },\n"
"\n"
"        // Request pitch data\n"
"        getPitchData: () => {\n"
"            return window.JuceAPI.send('getPitchData', {});\n"
"        }\n"
"    };\n"
"\n"
"    // Global bridge for backward compatibility\n"
"    window.NativeBridge = window.JuceAPI;\n"
"\n"
"    console.log('JUCE Interop initialized');\n"
"})();\n";

const char* index_js2 = (const char*) temp_binary_data_4;
}
