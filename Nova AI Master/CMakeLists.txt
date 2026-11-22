# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
# Ensure JUCE does not search for VST2 SDK
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaAIMaster VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK path wired in from day one (same pattern as every other Nova
# plugin), but AAX is not added to PLUGIN_FORMATS yet - not built until
# commercial AAX infra is ready for this product (spec section 32/45).
if(APPLE OR WIN32)
    juce_set_aax_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
endif()

# Nova AI Master
# Intelligent one-click mastering plugin - lives on the DAW master bus.

set(PLUGIN_TARGET NovaAIMaster)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
    set(WEBVIEW_BACKEND "WebView2")
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/NovaAIMaster")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova AI Master: Building for ${PLUGIN_FORMATS}")
message(STATUS "Nova AI Master: WebView backend: ${WEBVIEW_BACKEND}")

juce_add_binary_data(NovaAIMaster_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_ai_master_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novaaimaster"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvAi
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova AI Master"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Mastering
    AAX_CATEGORY Dynamics
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

set(NOVA_AI_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaAIMasterCore)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${NOVA_AI_CORE_DIR}/NovaAIMasterEngine.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaLoudnessAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaDynamicsAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaSpectralAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaStereoAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Decision/NovaMasteringDecisionEngine.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaCorrectiveEQ.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaResonanceControl.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaGlueCompressor.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaSaturation.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaStereoProcessor.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaSoftClipper.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaLimiter.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaMasteringChain.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_AI_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaAIMaster_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(${PLUGIN_TARGET}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(${PLUGIN_TARGET}
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
            _USE_MATH_DEFINES=1
    )
endif()

if(APPLE)
    target_compile_definitions(${PLUGIN_TARGET}
        PUBLIC
            JUCE_USE_CURL=0
    )
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(${PLUGIN_TARGET}
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_JACK=1
    )
endif()

# ── Analysis engine validation harness ──────────────────────────────────
# Headless checks of the NovaAIMasterCore analyzers against synthetic
# signals with known-correct relationships - see AnalysisValidation.cpp.
# Run before trusting any analyzer output on real material.

juce_add_console_app(NovaAIMasterAnalysisValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaaimaster.analysisvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaAIMasterAnalysisValidation PRIVATE
    Source/AnalysisValidation.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaLoudnessAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaDynamicsAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaStereoAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaSpectralAnalyzer.cpp
)

target_include_directories(NovaAIMasterAnalysisValidation PRIVATE ${NOVA_AI_CORE_DIR})

target_link_libraries(NovaAIMasterAnalysisValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaAIMasterAnalysisValidation PRIVATE
    JucePlugin_Name="NovaAIMasterAnalysisValidation"
)

# ── DSP mastering chain validation harness ──────────────────────────────
# Headless checks of NovaMasteringChain - passthrough when inactive,
# stability/boundedness when active, true-peak ceiling holds. See
# DSPValidation.cpp. Run before trusting the chain on real material.

juce_add_console_app(NovaAIMasterDSPValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaaimaster.dspvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaAIMasterDSPValidation PRIVATE
    Source/DSPValidation.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaCorrectiveEQ.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaResonanceControl.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaGlueCompressor.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaSaturation.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaStereoProcessor.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaSoftClipper.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaLimiter.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaMasteringChain.cpp
)

target_include_directories(NovaAIMasterDSPValidation PRIVATE ${NOVA_AI_CORE_DIR})

target_link_libraries(NovaAIMasterDSPValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaAIMasterDSPValidation PRIVATE
    JucePlugin_Name="NovaAIMasterDSPValidation"
)

# ── Decision engine validation harness ──────────────────────────────────
# Headless checks of NovaMasteringDecisionEngine - source-awareness (same
# style, different mixes -> different plans), the "less compression for
# already-compressed material" rule, style differentiation, and safety
# clamps under pathological input. See DecisionValidation.cpp.

juce_add_console_app(NovaAIMasterDecisionValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaaimaster.decisionvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaAIMasterDecisionValidation PRIVATE
    Source/DecisionValidation.cpp
    ${NOVA_AI_CORE_DIR}/Decision/NovaMasteringDecisionEngine.cpp
)

target_include_directories(NovaAIMasterDecisionValidation PRIVATE ${NOVA_AI_CORE_DIR})

target_link_libraries(NovaAIMasterDecisionValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaAIMasterDecisionValidation PRIVATE
    JucePlugin_Name="NovaAIMasterDecisionValidation"
)

# ── Plugin realtime workflow validation harness ─────────────────────────
# Drives the REAL NovaAIMasterProcessor (same class the AU/VST3 wrapper
# instantiates) through real processBlock() calls plus the real
# beginAnalyze()/beginMaster() control surface, pumping the JUCE message
# loop so the background-thread completion callbacks actually fire. Proves
# the capture -> background analyze -> plan handoff -> crossfade pipeline
# genuinely works under real cross-thread conditions, not just compiles.
# See PluginWorkflowValidation.cpp.

juce_add_console_app(NovaAIMasterPluginWorkflowValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaaimaster.workflowvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaAIMasterPluginWorkflowValidation PRIVATE
    Source/PluginWorkflowValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${NOVA_AI_CORE_DIR}/NovaAIMasterEngine.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaLoudnessAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaDynamicsAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaSpectralAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaStereoAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Decision/NovaMasteringDecisionEngine.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaCorrectiveEQ.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaResonanceControl.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaGlueCompressor.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaSaturation.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaStereoProcessor.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaSoftClipper.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaLimiter.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaMasteringChain.cpp
)

target_include_directories(NovaAIMasterPluginWorkflowValidation PRIVATE ${NOVA_AI_CORE_DIR})

target_link_libraries(NovaAIMasterPluginWorkflowValidation PRIVATE
    NovaAIMaster_WebUI
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaAIMasterPluginWorkflowValidation PRIVATE
    JucePlugin_Name="NovaAIMasterPluginWorkflowValidation"
    JUCE_WEB_BROWSER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

if(APPLE)
    target_compile_definitions(NovaAIMasterPluginWorkflowValidation PRIVATE JUCE_USE_CURL=0)
endif()
