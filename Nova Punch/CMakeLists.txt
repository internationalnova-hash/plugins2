# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
# Ensure JUCE does not search for VST2 SDK
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaPunch VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NOVA_PUNCH_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaPunchCore)

set(PLUGIN_TARGET NovaPunch)
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

message(STATUS "Nova Punch: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaPunch_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/accent_divider.svg
        Source/ui/public/img/accent_underlight.svg
        Source/ui/public/img/button_clip_safe_off@2x.png
        Source/ui/public/img/button_clip_safe_on@2x.png
        Source/ui/public/img/button_focus_active@2x.png
        Source/ui/public/img/button_focus_hover@2x.png
        Source/ui/public/img/button_focus_inactive@2x.png
        Source/ui/public/img/button_focus_pressed@2x.png
        Source/ui/public/img/button_tab_active@2x.png
        Source/ui/public/img/button_tab_hover@2x.png
        Source/ui/public/img/button_tab_inactive@2x.png
        Source/ui/public/img/button_tab_pressed@2x.png
        Source/ui/public/img/corner_screw.svg
        Source/ui/public/img/display_processing_activity_frame_grid@2x.png
        Source/ui/public/img/display_transient_intelligence_frame_grid@2x.png
        Source/ui/public/img/footer_frame@2x.png
        Source/ui/public/img/icon_arrow_next.svg
        Source/ui/public/img/icon_arrow_previous.svg
        Source/ui/public/img/header_icon_button_shell@2x.png
        Source/ui/public/img/icon_menu.svg
        Source/ui/public/img/icon_save.svg
        Source/ui/public/img/icon_settings.svg
        Source/ui/public/img/indicator_lamp_off@2x.png
        Source/ui/public/img/indicator_lamp_on@2x.png
        Source/ui/public/img/knob_punch_physical@2x.png
        Source/ui/public/img/knob_body_physical@2x.png
        Source/ui/public/img/knob_mix_physical@2x.png
        Source/ui/public/img/knob_sensitivity_physical@2x.png
        Source/ui/public/img/meter_in_housing_background@2x.png
        Source/ui/public/img/meter_out_housing_background@2x.png
        Source/ui/public/img/nova_n_mark.svg
        Source/ui/public/img/texture_graphite_lower@2x.png
        Source/ui/public/img/texture_titanium_main@2x.png
        Source/ui/public/img/texture_titanium_mockup_swatch.png
        Source/ui/public/img/module_separator_bevel@2x.png
        Source/ui/public/img/texture_graphite_top@2x.png
        Source/ui/public/img/preset_selector_shell@2x.png
        Source/ui/public/img/rack_edge_left@2x.png
        Source/ui/public/img/rack_edge_right@2x.png
    NAMESPACE nova_punch_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novapunch"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvPn
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Punch"
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
    Source/Presets/NovaPunchPreset.h
    Source/Presets/NovaPunchFactoryPresets.h
    Source/Presets/NovaPunchFactoryPresets.cpp
    Source/Presets/NovaPunchPresetManager.h
    Source/Presets/NovaPunchPresetManager.cpp
    ${NOVA_PUNCH_CORE_DIR}/NovaPunchTypes.h
    ${NOVA_PUNCH_CORE_DIR}/NovaPunchEngine.h
    ${NOVA_PUNCH_CORE_DIR}/NovaPunchEngine.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEnvelopeFollower.h
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEnvelopeFollower.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.h
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchTransientDetector.h
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchTransientDetector.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEventSegmenter.h
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEventSegmenter.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchGainShaper.h
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchGainShaper.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchClipSafe.h
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchClipSafe.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_PUNCH_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaPunch_WebUI
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

juce_add_console_app(NovaPunchDetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapunch.detectorvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaPunchDetectorValidation PRIVATE
    Source/DetectorValidation.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEnvelopeFollower.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchTransientDetector.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEventSegmenter.cpp)
target_include_directories(NovaPunchDetectorValidation PRIVATE ${NOVA_PUNCH_CORE_DIR})
target_link_libraries(NovaPunchDetectorValidation PRIVATE
    juce::juce_core juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaPunchDetectorValidation PRIVATE JucePlugin_Name="NovaPunchDetectorValidation")
if(APPLE)
    target_compile_definitions(NovaPunchDetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaPunchCrossoverValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapunch.crossovervalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaPunchCrossoverValidation PRIVATE
    Source/CrossoverValidation.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.cpp)
target_include_directories(NovaPunchCrossoverValidation PRIVATE ${NOVA_PUNCH_CORE_DIR})
target_link_libraries(NovaPunchCrossoverValidation PRIVATE
    juce::juce_core juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaPunchCrossoverValidation PRIVATE JucePlugin_Name="NovaPunchCrossoverValidation")
if(APPLE)
    target_compile_definitions(NovaPunchCrossoverValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaPunchEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapunch.enginevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaPunchEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_PUNCH_CORE_DIR}/NovaPunchEngine.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEnvelopeFollower.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchTransientDetector.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEventSegmenter.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchGainShaper.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchClipSafe.cpp)
target_include_directories(NovaPunchEngineValidation PRIVATE ${NOVA_PUNCH_CORE_DIR})
target_link_libraries(NovaPunchEngineValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaPunchEngineValidation PRIVATE JucePlugin_Name="NovaPunchEngineValidation")
if(APPLE)
    target_compile_definitions(NovaPunchEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaPunchStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapunch.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaPunchStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets/NovaPunchFactoryPresets.cpp
    Source/Presets/NovaPunchPresetManager.cpp
    ${NOVA_PUNCH_CORE_DIR}/NovaPunchEngine.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEnvelopeFollower.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchTransientDetector.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEventSegmenter.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchGainShaper.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchClipSafe.cpp)
target_include_directories(NovaPunchStateRoundTripValidation PRIVATE ${NOVA_PUNCH_CORE_DIR})
target_link_libraries(NovaPunchStateRoundTripValidation PRIVATE
    NovaPunch_WebUI
    juce::juce_core juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_data_structures juce::juce_dsp
    juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaPunchStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaPunchStateRoundTripValidation"
    JUCE_WEB_BROWSER=1)
if(APPLE)
    target_compile_definitions(NovaPunchStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaPunchRealMaterialDiagnostic
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapunch.realmaterialdiagnostic"
    VERSION ${PROJECT_VERSION})
target_sources(NovaPunchRealMaterialDiagnostic PRIVATE
    Source/RealMaterialDiagnostic.cpp
    ${NOVA_PUNCH_CORE_DIR}/NovaPunchEngine.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEnvelopeFollower.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchCrossover.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchTransientDetector.cpp
    ${NOVA_PUNCH_CORE_DIR}/Analysis/NovaPunchEventSegmenter.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchGainShaper.cpp
    ${NOVA_PUNCH_CORE_DIR}/DSP/NovaPunchClipSafe.cpp)
target_include_directories(NovaPunchRealMaterialDiagnostic PRIVATE ${NOVA_PUNCH_CORE_DIR})
target_link_libraries(NovaPunchRealMaterialDiagnostic PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaPunchRealMaterialDiagnostic PRIVATE JucePlugin_Name="NovaPunchRealMaterialDiagnostic")
if(APPLE)
    target_compile_definitions(NovaPunchRealMaterialDiagnostic PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaPunch)
nova_license_test_target(NovaPunchStateRoundTripValidation)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaPunch)
