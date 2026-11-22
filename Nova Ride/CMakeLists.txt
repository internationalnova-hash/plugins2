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

project(NovaRide VERSION 1.0.0)

# JUCE setup using FetchContent
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK (Avid Pro Tools format) - shared across plugins.
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NOVA_RIDE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaRideCore)

# Nova Ride
# Intelligent automatic vocal gain-riding/leveling - not a compressor. Raises
# and lowers vocal gain to hold a stable perceived level while preserving
# phrasing/dynamics, with a WebView interface.

set(PLUGIN_TARGET NovaRide)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

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
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/NovaRide")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova Ride: Building for ${PLUGIN_FORMATS}")
message(STATUS "Nova Ride: WebView backend: ${WEBVIEW_BACKEND}")

juce_add_binary_data(NovaRide_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/n_logo.png
        Source/ui/public/img/knob_target_large.png
        Source/ui/public/img/knob_medium.png
        Source/ui/public/img/knob_sensitivity_small.png
        Source/ui/public/img/knob_arc_large.png
        Source/ui/public/img/knob_arc_medium.png
        Source/ui/public/img/knob_arc_small.png
        Source/ui/public/img/fader_cap.png
        Source/ui/public/img/fader_rail.png
        Source/ui/public/img/meter_lens_off.png
        Source/ui/public/img/meter_lens_gold_on.png
        Source/ui/public/img/meter_lens_red_on.png
        Source/ui/public/img/meter_column_shell.png
        Source/ui/public/img/toggle_off.png
        Source/ui/public/img/toggle_on.png
        Source/ui/public/img/mode_button_inactive.png
        Source/ui/public/img/mode_button_hover.png
        Source/ui/public/img/mode_button_active.png
        Source/ui/public/img/bypass_button_inactive.png
        Source/ui/public/img/bypass_button_hover.png
        Source/ui/public/img/bypass_button_pressed.png
        Source/ui/public/img/preset_selector_shell.png
        Source/ui/public/img/sidechain_selector_shell.png
        Source/ui/public/img/help_icon.png
        Source/ui/public/img/preset_arrow_left.png
        Source/ui/public/img/preset_arrow_right.png
        Source/ui/public/img/settings_icon.png
        Source/ui/public/img/settings_button.png
        Source/ui/public/img/corner_bracket.png
        Source/ui/public/img/divider_gold.png
        Source/ui/public/img/panel_inset.png
        Source/ui/public/img/outer_frame.png
        Source/ui/public/img/header_frame.png
        Source/ui/public/img/footer_frame.png
        Source/ui/public/img/screw_gold.png
        Source/ui/public/img/ride_history_grid.png
        Source/ui/public/img/texture_graphite_vector.png
    NAMESPACE nova_ride_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novaride"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvRd
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Ride"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Dynamics
    AAX_CATEGORY Dynamics
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/NovaRidePresetManager.cpp
        Source/NovaRidePresetManager.h
        ${NOVA_RIDE_CORE_DIR}/NovaRideEngine.cpp
        ${NOVA_RIDE_CORE_DIR}/NovaRideEngine.h
        ${NOVA_RIDE_CORE_DIR}/NovaRideConstants.h
        ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideDetector.cpp
        ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideDetector.h
        ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideSidechainAnalyzer.cpp
        ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideSidechainAnalyzer.h
        ${NOVA_RIDE_CORE_DIR}/Engine/NovaRideGainEngine.cpp
        ${NOVA_RIDE_CORE_DIR}/Engine/NovaRideGainEngine.h
        ${NOVA_RIDE_CORE_DIR}/RideHistory/NovaRideAutomationBuffer.cpp
        ${NOVA_RIDE_CORE_DIR}/RideHistory/NovaRideAutomationBuffer.h
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_RIDE_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaRide_WebUI
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

# ---------------------------------------------------------------------------
# Validation console apps (not shipped - headless synthetic-signal testing)
# ---------------------------------------------------------------------------

juce_add_console_app(NovaRideDetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaride.detectorvalidation"
    VERSION ${PROJECT_VERSION}
)
target_sources(NovaRideDetectorValidation PRIVATE
    Source/DetectorValidation.cpp
    ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideDetector.cpp
    ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideSidechainAnalyzer.cpp
)
target_include_directories(NovaRideDetectorValidation PRIVATE ${NOVA_RIDE_CORE_DIR})
target_link_libraries(NovaRideDetectorValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags
)
target_compile_definitions(NovaRideDetectorValidation PRIVATE JucePlugin_Name="NovaRideDetectorValidation")
if(APPLE)
    target_compile_definitions(NovaRideDetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaRideEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaride.enginevalidation"
    VERSION ${PROJECT_VERSION}
)
target_sources(NovaRideEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_RIDE_CORE_DIR}/NovaRideEngine.cpp
    ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideDetector.cpp
    ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideSidechainAnalyzer.cpp
    ${NOVA_RIDE_CORE_DIR}/Engine/NovaRideGainEngine.cpp
    ${NOVA_RIDE_CORE_DIR}/RideHistory/NovaRideAutomationBuffer.cpp
)
target_include_directories(NovaRideEngineValidation PRIVATE ${NOVA_RIDE_CORE_DIR})
target_link_libraries(NovaRideEngineValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags
)
target_compile_definitions(NovaRideEngineValidation PRIVATE JucePlugin_Name="NovaRideEngineValidation")
if(APPLE)
    target_compile_definitions(NovaRideEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaRideStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaride.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION}
)
target_sources(NovaRideStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NovaRidePresetManager.cpp
    ${NOVA_RIDE_CORE_DIR}/NovaRideEngine.cpp
    ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideDetector.cpp
    ${NOVA_RIDE_CORE_DIR}/Detection/NovaRideSidechainAnalyzer.cpp
    ${NOVA_RIDE_CORE_DIR}/Engine/NovaRideGainEngine.cpp
    ${NOVA_RIDE_CORE_DIR}/RideHistory/NovaRideAutomationBuffer.cpp
)
target_include_directories(NovaRideStateRoundTripValidation PRIVATE ${NOVA_RIDE_CORE_DIR})
target_link_libraries(NovaRideStateRoundTripValidation PRIVATE
    NovaRide_WebUI
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
target_compile_definitions(NovaRideStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaRideStateRoundTripValidation"
    JUCE_WEB_BROWSER=1
)
if(APPLE)
    target_compile_definitions(NovaRideStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()
