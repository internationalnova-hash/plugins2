# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
# Ensure JUCE does not search for VST2 SDK
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    # Ensure consistent macOS binary compatibility regardless of CI defaults.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaGlue VERSION 0.1.0)

# JUCE setup using FetchContent
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK path (established rollout pattern for this catalog).
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

# Nova Glue
# Analog VCA Buss Compressor (SSL-buss-compressor-workflow-inspired,
# original Nova implementation - no SSL circuit/component accuracy claims),
# with a WebView interface.

set(NOVA_GLUE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaGlueCore)

set(PLUGIN_TARGET NovaGlue)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 ${NOVA_AAX_FORMAT} Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
    set(WEBVIEW_BACKEND "WebView2")
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova Glue: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaGlue_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/n_logo.png
        Source/ui/public/img/main_faceplate_graphite.png
        Source/ui/public/img/lower_panel_graphite.png
        Source/ui/public/img/knob_large_champagne.png
        Source/ui/public/img/knob_small_champagne.png
        Source/ui/public/img/knob_glue_black.png
        Source/ui/public/img/glue_ring_active.png
        Source/ui/public/img/glue_ring_inactive.png
        Source/ui/public/img/gr_meter_housing.png
        Source/ui/public/img/button_standard_inactive.png
        Source/ui/public/img/button_standard_hover.png
        Source/ui/public/img/button_standard_active.png
        Source/ui/public/img/preset_shell_inactive.png
        Source/ui/public/img/icon_button_inactive.png
        Source/ui/public/img/icon_button_hover.png
        Source/ui/public/img/gr_needle.svg
        Source/ui/public/img/arrow_next.svg
        Source/ui/public/img/arrow_previous.svg
        Source/ui/public/img/save_icon.svg
        Source/ui/public/img/settings_icon.svg
        Source/ui/public/img/screw_brass.svg
    NAMESPACE nova_glue_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novaglue"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvGl
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Glue"
    VST3_CATEGORIES Fx Dynamics
    AAX_CATEGORY Dynamics
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
)

target_sources(${PLUGIN_TARGET} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    ${NOVA_GLUE_CORE_DIR}/NovaGlueEngine.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueDetector.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaConsoleStage.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaOversampledConsoleStage.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueAutoGain.cpp
    ${NOVA_GLUE_CORE_DIR}/Meter/NovaVUBallistics.cpp
    ${NOVA_GLUE_CORE_DIR}/Preset/NovaGluePresetManager.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_GLUE_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaGlue_WebUI
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
if(APPLE)
    target_compile_definitions(${PLUGIN_TARGET} PUBLIC JUCE_USE_CURL=0)
endif()

# ============================================================
# DSP-core validation targets (headless, no plugin/UI needed).
# Built first, per the phased plan, to de-risk the DSP before the
# plugin/UI targets (added once UI assets are wired up).
# ============================================================

juce_add_console_app(NovaGlueDetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaglue.detectorvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaGlueDetectorValidation PRIVATE
    Source/DetectorValidation.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueDetector.cpp)
target_include_directories(NovaGlueDetectorValidation PRIVATE ${NOVA_GLUE_CORE_DIR})
target_link_libraries(NovaGlueDetectorValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaGlueDetectorValidation PRIVATE JucePlugin_Name="NovaGlueDetectorValidation")
if(APPLE)
    target_compile_definitions(NovaGlueDetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaGlueEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaglue.enginevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaGlueEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_GLUE_CORE_DIR}/NovaGlueEngine.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueDetector.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaConsoleStage.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaOversampledConsoleStage.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueAutoGain.cpp
    ${NOVA_GLUE_CORE_DIR}/Meter/NovaVUBallistics.cpp)
target_include_directories(NovaGlueEngineValidation PRIVATE ${NOVA_GLUE_CORE_DIR})
target_link_libraries(NovaGlueEngineValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaGlueEngineValidation PRIVATE JucePlugin_Name="NovaGlueEngineValidation")
if(APPLE)
    target_compile_definitions(NovaGlueEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaGlueStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaglue.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaGlueStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${NOVA_GLUE_CORE_DIR}/NovaGlueEngine.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueDetector.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaConsoleStage.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaOversampledConsoleStage.cpp
    ${NOVA_GLUE_CORE_DIR}/DSP/NovaGlueAutoGain.cpp
    ${NOVA_GLUE_CORE_DIR}/Meter/NovaVUBallistics.cpp
    ${NOVA_GLUE_CORE_DIR}/Preset/NovaGluePresetManager.cpp)
target_include_directories(NovaGlueStateRoundTripValidation PRIVATE ${NOVA_GLUE_CORE_DIR})
target_link_libraries(NovaGlueStateRoundTripValidation PRIVATE
    NovaGlue_WebUI
    juce::juce_core juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_data_structures juce::juce_dsp
    juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaGlueStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaGlueStateRoundTripValidation"
    JUCE_WEB_BROWSER=1)
if(APPLE)
    target_compile_definitions(NovaGlueStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()
