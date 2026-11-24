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

project(NovaRoyal670 VERSION 0.1.0)

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

# Nova Royal 670
# Fairchild-670-inspired variable-mu tube limiter, with a WebView interface

set(NOVA_ROYAL670_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaRoyal670Core)

set(PLUGIN_TARGET NovaRoyal670)
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

message(STATUS "Nova Royal 670: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaRoyal670_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/desk-knob-red.png
        Source/ui/public/img/black-knob.png
        Source/ui/public/img/tube-strip.png
        Source/ui/public/img/nameplate-dark.png
        Source/ui/public/img/m-medallion.png
        Source/ui/public/img/n_logo.png
        Source/ui/public/img/panel-texture.png
        Source/ui/public/img/screw-slotted.png
        Source/ui/public/img/screw-phillips.png
        Source/ui/public/img/vu-face.jpg
    NAMESPACE nova_royal670_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novaroyal670"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvR6
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Royal 670"
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
    ${NOVA_ROYAL670_CORE_DIR}/NovaRoyal670Engine.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaVariableMuDetector.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaOversampledInputStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaOversampledOutputStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/Stereo/NovaStereoModeCrossfader.cpp
    ${NOVA_ROYAL670_CORE_DIR}/Meter/NovaVUBallistics.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_ROYAL670_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaRoyal670_WebUI
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

juce_add_console_app(NovaRoyal670DetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaroyal670.detectorvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaRoyal670DetectorValidation PRIVATE
    Source/DetectorValidation.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaVariableMuDetector.cpp)
target_include_directories(NovaRoyal670DetectorValidation PRIVATE ${NOVA_ROYAL670_CORE_DIR})
target_link_libraries(NovaRoyal670DetectorValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_recommended_config_flags)
target_compile_definitions(NovaRoyal670DetectorValidation PRIVATE JucePlugin_Name="NovaRoyal670DetectorValidation")
if(APPLE)
    target_compile_definitions(NovaRoyal670DetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaRoyal670TubeTransformerValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaroyal670.tubetransformervalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaRoyal670TubeTransformerValidation PRIVATE
    Source/TubeTransformerValidation.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTransformerStage.cpp)
target_include_directories(NovaRoyal670TubeTransformerValidation PRIVATE ${NOVA_ROYAL670_CORE_DIR})
target_link_libraries(NovaRoyal670TubeTransformerValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaRoyal670TubeTransformerValidation PRIVATE JucePlugin_Name="NovaRoyal670TubeTransformerValidation")
if(APPLE)
    target_compile_definitions(NovaRoyal670TubeTransformerValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaRoyal670EngineIntegrationValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaroyal670.engineintegrationvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaRoyal670EngineIntegrationValidation PRIVATE
    Source/EngineIntegrationValidation.cpp
    ${NOVA_ROYAL670_CORE_DIR}/NovaRoyal670Engine.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaVariableMuDetector.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaOversampledInputStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaOversampledOutputStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/Stereo/NovaStereoModeCrossfader.cpp
    ${NOVA_ROYAL670_CORE_DIR}/Meter/NovaVUBallistics.cpp)
target_include_directories(NovaRoyal670EngineIntegrationValidation PRIVATE ${NOVA_ROYAL670_CORE_DIR})
target_link_libraries(NovaRoyal670EngineIntegrationValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaRoyal670EngineIntegrationValidation PRIVATE JucePlugin_Name="NovaRoyal670EngineIntegrationValidation")
if(APPLE)
    target_compile_definitions(NovaRoyal670EngineIntegrationValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaRoyal670StateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaroyal670.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaRoyal670StateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${NOVA_ROYAL670_CORE_DIR}/NovaRoyal670Engine.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaVariableMuDetector.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTubeStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaOversampledInputStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/DSP/NovaOversampledOutputStage.cpp
    ${NOVA_ROYAL670_CORE_DIR}/Stereo/NovaStereoModeCrossfader.cpp
    ${NOVA_ROYAL670_CORE_DIR}/Meter/NovaVUBallistics.cpp)
target_include_directories(NovaRoyal670StateRoundTripValidation PRIVATE ${NOVA_ROYAL670_CORE_DIR})
target_link_libraries(NovaRoyal670StateRoundTripValidation PRIVATE
    NovaRoyal670_WebUI
    juce::juce_core juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_data_structures juce::juce_dsp
    juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaRoyal670StateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaRoyal670StateRoundTripValidation"
    JUCE_WEB_BROWSER=1)
if(APPLE)
    target_compile_definitions(NovaRoyal670StateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaRoyal670)
nova_license_test_target(NovaRoyal670StateRoundTripValidation)
