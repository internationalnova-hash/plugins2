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

project(NovaGlow VERSION 0.1.0)

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

# Nova Glow
# Optical tube leveling amplifier (LA-2A-workflow-inspired, original Nova
# implementation), with a WebView interface.

set(NOVA_GLOW_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaGlowCore)

set(PLUGIN_TARGET NovaGlow)
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

message(STATUS "Nova Glow: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaGlow_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/n_logo.png
        Source/ui/public/img/knob_large_bakelite.png
        Source/ui/public/img/knob_small_bakelite.png
        Source/ui/public/img/vu_meter_blank.png
        Source/ui/public/img/vu_needle.png
        Source/ui/public/img/vu_pivot.png
        Source/ui/public/img/vu_glass_highlight.png
        Source/ui/public/img/top_bar_frame.png
        Source/ui/public/img/wood_cheek_left.png
        Source/ui/public/img/wood_cheek_right.png
        Source/ui/public/img/cream_metal_texture.png
        Source/ui/public/img/black_brushed_texture.png
        Source/ui/public/img/power_jewel_off.png
        Source/ui/public/img/power_jewel_on.png
        Source/ui/public/img/compress_limit_compress.png
        Source/ui/public/img/compress_limit_limit.png
        Source/ui/public/img/analog_button_inactive.png
        Source/ui/public/img/analog_button_hover.png
        Source/ui/public/img/analog_button_active.png
        Source/ui/public/img/oversample_button_inactive.png
        Source/ui/public/img/oversample_button_hover.png
        Source/ui/public/img/oversample_button_active.png
        Source/ui/public/img/stereo_link_off.png
        Source/ui/public/img/stereo_link_on.png
        Source/ui/public/img/meter_selector.png
        Source/ui/public/img/preset_selector_shell.png
        Source/ui/public/img/preset_arrow_left.png
        Source/ui/public/img/preset_arrow_right.png
        Source/ui/public/img/settings_button.png
        Source/ui/public/img/save_button.png
        Source/ui/public/img/settings_icon.png
        Source/ui/public/img/save_icon.png
        Source/ui/public/img/brass_screw.png
        Source/ui/public/img/brass_divider.png
    NAMESPACE nova_glow_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novaglow"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvGw
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Glow"
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
    ${NOVA_GLOW_CORE_DIR}/NovaGlowEngine.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalDetector.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalGainCell.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaEmphasisFilter.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOversampledInputStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOversampledOutputStage.cpp
    ${NOVA_GLOW_CORE_DIR}/Meter/NovaVUBallistics.cpp
    ${NOVA_GLOW_CORE_DIR}/Preset/NovaGlowPresetManager.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_GLOW_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaGlow_WebUI
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
# ============================================================

juce_add_console_app(NovaGlowDetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaglow.detectorvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaGlowDetectorValidation PRIVATE
    Source/DetectorValidation.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalDetector.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalGainCell.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaEmphasisFilter.cpp)
target_include_directories(NovaGlowDetectorValidation PRIVATE ${NOVA_GLOW_CORE_DIR})
target_link_libraries(NovaGlowDetectorValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaGlowDetectorValidation PRIVATE JucePlugin_Name="NovaGlowDetectorValidation")
if(APPLE)
    target_compile_definitions(NovaGlowDetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaGlowEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaglow.enginevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaGlowEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_GLOW_CORE_DIR}/NovaGlowEngine.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalDetector.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalGainCell.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaEmphasisFilter.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOversampledInputStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOversampledOutputStage.cpp
    ${NOVA_GLOW_CORE_DIR}/Meter/NovaVUBallistics.cpp)
target_include_directories(NovaGlowEngineValidation PRIVATE ${NOVA_GLOW_CORE_DIR})
target_link_libraries(NovaGlowEngineValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaGlowEngineValidation PRIVATE JucePlugin_Name="NovaGlowEngineValidation")
if(APPLE)
    target_compile_definitions(NovaGlowEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaGlowStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaglow.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaGlowStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${NOVA_GLOW_CORE_DIR}/NovaGlowEngine.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalDetector.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOpticalGainCell.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaEmphasisFilter.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOversampledInputStage.cpp
    ${NOVA_GLOW_CORE_DIR}/DSP/NovaOversampledOutputStage.cpp
    ${NOVA_GLOW_CORE_DIR}/Meter/NovaVUBallistics.cpp
    ${NOVA_GLOW_CORE_DIR}/Preset/NovaGlowPresetManager.cpp)
target_include_directories(NovaGlowStateRoundTripValidation PRIVATE ${NOVA_GLOW_CORE_DIR})
target_link_libraries(NovaGlowStateRoundTripValidation PRIVATE
    NovaGlow_WebUI
    juce::juce_core juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_data_structures juce::juce_dsp
    juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaGlowStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaGlowStateRoundTripValidation"
    JUCE_WEB_BROWSER=1)
if(APPLE)
    target_compile_definitions(NovaGlowStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaGlow)
nova_license_test_target(NovaGlowStateRoundTripValidation)
