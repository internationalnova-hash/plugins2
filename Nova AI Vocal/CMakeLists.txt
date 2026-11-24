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

project(NovaAIVocal VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK shared by every Nova plugin. AAX builds are ad-hoc signed (load in
# Pro Tools Developer); retail Pro Tools additionally needs PACE signing.
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

# Nova AI Vocal
# Listens to a vocal and mixes it in a chosen vibe: gain staging, corrective
# EQ, de-essing, compression, warmth, delay and reverb, driven by six macros.

set(PLUGIN_TARGET NovaAIVocal)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 ${NOVA_AAX_FORMAT} Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone)
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/NovaAIVocal")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova AI Vocal: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaAIVocal_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_ai_vocal_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novaaivocal"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvAv
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova AI Vocal"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Dynamics EQ
    AAX_CATEGORY Dynamics
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

# Tuner shared with Nova Pitch
set(NOVA_TUNE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTuneCore)

set(NOVA_AI_VOCAL_DSP
    Source/dsp/NovaVocalChain.cpp
    Source/dsp/NovaVocalAnalysis.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${NOVA_AI_VOCAL_DSP}
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_TUNE_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaAIVocal_WebUI
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
    target_compile_definitions(${PLUGIN_TARGET} PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 _USE_MATH_DEFINES=1)
endif()

if(APPLE)
    target_compile_definitions(${PLUGIN_TARGET} PUBLIC JUCE_USE_CURL=0)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(${PLUGIN_TARGET} PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()

# ── Validation harness ──────────────────────────────────────────────────
# Headless checks of the chain (transparency at zero, stability at full
# settings, every vibe) plus an offline render mode:
#   NovaAIVocalValidation <vocal.wav> <outDir>
# analyzes the file like Listen does and renders it through every vibe.

juce_add_console_app(NovaAIVocalValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaaivocal.validation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaAIVocalValidation PRIVATE
    Source/Validation.cpp
    ${NOVA_AI_VOCAL_DSP}
)

target_include_directories(NovaAIVocalValidation PRIVATE ${NOVA_TUNE_CORE_DIR})

target_link_libraries(NovaAIVocalValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaAIVocalValidation PRIVATE
    JucePlugin_Name="NovaAIVocalValidation"
    JUCE_USE_CURL=0
)

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaAIVocal)
