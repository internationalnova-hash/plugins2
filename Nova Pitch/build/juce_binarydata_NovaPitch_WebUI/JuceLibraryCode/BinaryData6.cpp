/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace nova_pitch_BinaryData
{

//================== check_native_interop.js ==================
static const unsigned char temp_binary_data_5[] =
"// JUCE Native Interop Check\n"
"// Verifies availability of JUCE backend and sets up appropriate mode\n"
"\n"
"(function() {\n"
"    'use strict';\n"
"\n"
"    const checkJuceBackend = () => {\n"
"        return typeof window !== 'undefined' && window.__JUCE !== undefined;\n"
"    };\n"
"\n"
"    const initMode = () => {\n"
"        if (checkJuceBackend()) {\n"
"            console.log('JUCE native backend detected - running in native mode');\n"
"            window.juceNativeMode = true;\n"
"        } else {\n"
"            console.log('JUCE native backend not detected - fallback mode active');\n"
"            window.juceNativeMode = false;\n"
"        }\n"
"    };\n"
"\n"
"    // Wait for document to be ready\n"
"    if (document.readyState === 'loading') {\n"
"        document.addEventListener('DOMContentLoaded', initMode);\n"
"    } else {\n"
"        initMode();\n"
"    }\n"
"})();\n";

const char* check_native_interop_js = (const char*) temp_binary_data_5;
}
