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

project(NovaBend VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

if(APPLE OR WIN32)
    juce_set_aax_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
endif()

set(NOVA_BEND_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaBendCore)
# Shared tracker/shifter (same as Nova Pitch and Nova AI Vocal); NovaBendCore still supplies the formant stage
set(NOVA_TUNE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTuneCore)

set(PLUGIN_TARGET NovaBend)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 AAX Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
    set(WEBVIEW_BACKEND "WebView2")
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU AAX Standalone)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova Bend: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaBend_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/n_logo.png
        Source/ui/public/img/knob_pitch_black_gold.png
        Source/ui/public/img/knob_pitch_outer_ring.png
        Source/ui/public/img/pitch_ring_active.png
        Source/ui/public/img/knob_formant_graphite_champagne.png
        Source/ui/public/img/knob_formant_outer_ring.png
        Source/ui/public/img/formant_ring_active.png
        Source/ui/public/img/knob_mix_small.png
        Source/ui/public/img/utility_button_inactive.png
        Source/ui/public/img/utility_button_hover.png
        Source/ui/public/img/utility_button_active.png
        Source/ui/public/img/utility_button_pressed.png
        Source/ui/public/img/note_dropdown_inactive.png
        Source/ui/public/img/note_dropdown_hover.png
        Source/ui/public/img/note_dropdown_active.png
        Source/ui/public/img/octave_dropdown_inactive.png
        Source/ui/public/img/octave_dropdown_hover.png
        Source/ui/public/img/octave_dropdown_active.png
        Source/ui/public/img/preset_selector_inactive.png
        Source/ui/public/img/preset_selector_hover.png
        Source/ui/public/img/preset_selector_active.png
        Source/ui/public/img/icon_button_inactive.png
        Source/ui/public/img/icon_button_hover.png
        Source/ui/public/img/icon_button_active.png
        Source/ui/public/img/nova_n_emblem.png
        Source/ui/public/img/nova_emblem_gold_glow.png
        Source/ui/public/img/main_panel_aged_graphite.png
        Source/ui/public/img/lower_utility_panel.png
        Source/ui/public/img/screw_brass.svg
        Source/ui/public/img/chevron_down.svg
        Source/ui/public/img/arrow_next.svg
        Source/ui/public/img/arrow_previous.svg
        Source/ui/public/img/save_icon.svg
        Source/ui/public/img/settings_icon.svg
        Source/ui/public/img/corner_bracket.svg
        Source/ui/public/img/divider_gold.svg
    NAMESPACE nova_bend_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novabend"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvBn
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Bend"
    VST3_CATEGORIES Fx Pitch
    AAX_CATEGORY PitchShift
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
    Source/NovaBendTunedEngine.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/Presets/NovaBendPreset.h
    Source/Presets/NovaBendFactoryPresets.h
    Source/Presets/NovaBendFactoryPresets.cpp
    Source/Presets/NovaBendPresetManager.h
    Source/Presets/NovaBendPresetManager.cpp
    ${NOVA_BEND_CORE_DIR}/NovaBendTypes.h
    ${NOVA_BEND_CORE_DIR}/Analysis/NovaBendPitchDetector.cpp
    ${NOVA_BEND_CORE_DIR}/Analysis/NovaBendPitchDetector.h
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFlattenTarget.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFlattenTarget.h
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendShifter.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendShifter.h
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFormantProcessor.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFormantProcessor.h
    ${NOVA_BEND_CORE_DIR}/NovaBendEngine.cpp
    ${NOVA_BEND_CORE_DIR}/NovaBendEngine.h
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_BEND_CORE_DIR} ${NOVA_TUNE_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaBend_WebUI
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

juce_add_console_app(NovaBendPitchDetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.pitchdetectorvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaBendPitchDetectorValidation PRIVATE
    Source/PitchDetectorValidation.cpp
    ${NOVA_BEND_CORE_DIR}/Analysis/NovaBendPitchDetector.cpp)
target_include_directories(NovaBendPitchDetectorValidation PRIVATE ${NOVA_BEND_CORE_DIR})
target_link_libraries(NovaBendPitchDetectorValidation PRIVATE
    juce::juce_core juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaBendPitchDetectorValidation PRIVATE JucePlugin_Name="NovaBendPitchDetectorValidation")
if(APPLE)
    target_compile_definitions(NovaBendPitchDetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaBendShifterValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.shiftervalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaBendShifterValidation PRIVATE
    Source/ShifterValidation.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendShifter.cpp)
target_include_directories(NovaBendShifterValidation PRIVATE ${NOVA_BEND_CORE_DIR})
target_link_libraries(NovaBendShifterValidation PRIVATE
    juce::juce_core juce::juce_recommended_config_flags)
target_compile_definitions(NovaBendShifterValidation PRIVATE JucePlugin_Name="NovaBendShifterValidation")
if(APPLE)
    target_compile_definitions(NovaBendShifterValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaBendFormantValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.formantvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaBendFormantValidation PRIVATE
    Source/FormantValidation.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFormantProcessor.cpp)
target_include_directories(NovaBendFormantValidation PRIVATE ${NOVA_BEND_CORE_DIR})
target_link_libraries(NovaBendFormantValidation PRIVATE
    juce::juce_core juce::juce_recommended_config_flags)
target_compile_definitions(NovaBendFormantValidation PRIVATE JucePlugin_Name="NovaBendFormantValidation")
if(APPLE)
    target_compile_definitions(NovaBendFormantValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaBendEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.enginevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaBendEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_BEND_CORE_DIR}/Analysis/NovaBendPitchDetector.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFlattenTarget.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendShifter.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFormantProcessor.cpp
    ${NOVA_BEND_CORE_DIR}/NovaBendEngine.cpp)
target_include_directories(NovaBendEngineValidation PRIVATE ${NOVA_BEND_CORE_DIR})
target_link_libraries(NovaBendEngineValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaBendEngineValidation PRIVATE JucePlugin_Name="NovaBendEngineValidation")
if(APPLE)
    target_compile_definitions(NovaBendEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(ClickPatternAnalysis
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.clickpatternanalysis"
    VERSION ${PROJECT_VERSION})
target_sources(ClickPatternAnalysis PRIVATE Source/ClickPatternAnalysis.cpp)
target_link_libraries(ClickPatternAnalysis PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_recommended_config_flags)
target_compile_definitions(ClickPatternAnalysis PRIVATE JucePlugin_Name="ClickPatternAnalysis")
if(APPLE)
    target_compile_definitions(ClickPatternAnalysis PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaBendRealVocalDiagnostic
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.realvocaldiagnostic"
    VERSION ${PROJECT_VERSION})
target_sources(NovaBendRealVocalDiagnostic PRIVATE
    Source/RealVocalDiagnostic.cpp
    ${NOVA_BEND_CORE_DIR}/Analysis/NovaBendPitchDetector.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFlattenTarget.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendShifter.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFormantProcessor.cpp
    ${NOVA_BEND_CORE_DIR}/NovaBendEngine.cpp)
target_include_directories(NovaBendRealVocalDiagnostic PRIVATE ${NOVA_BEND_CORE_DIR})
target_link_libraries(NovaBendRealVocalDiagnostic PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaBendRealVocalDiagnostic PRIVATE JucePlugin_Name="NovaBendRealVocalDiagnostic")
if(APPLE)
    target_compile_definitions(NovaBendRealVocalDiagnostic PRIVATE JUCE_USE_CURL=0)
endif()

set(NOVA_PITCH_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaPitchCore)
juce_add_console_app(NovaPitchRealVocalDiagnostic
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.novapitchrealvocaldiagnostic"
    VERSION ${PROJECT_VERSION})
target_sources(NovaPitchRealVocalDiagnostic PRIVATE
    Source/NovaPitchRealVocalDiagnostic.cpp
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp)
target_include_directories(NovaPitchRealVocalDiagnostic PRIVATE ${NOVA_PITCH_CORE_DIR})
target_link_libraries(NovaPitchRealVocalDiagnostic PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaPitchRealVocalDiagnostic PRIVATE JucePlugin_Name="NovaPitchRealVocalDiagnostic")
if(APPLE)
    target_compile_definitions(NovaPitchRealVocalDiagnostic PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaBendStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabend.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaBendStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/NovaBendTunedEngine.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
    Source/PluginEditor.cpp
    Source/Presets/NovaBendFactoryPresets.cpp
    Source/Presets/NovaBendPresetManager.cpp
    ${NOVA_BEND_CORE_DIR}/Analysis/NovaBendPitchDetector.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFlattenTarget.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendShifter.cpp
    ${NOVA_BEND_CORE_DIR}/DSP/NovaBendFormantProcessor.cpp
    ${NOVA_BEND_CORE_DIR}/NovaBendEngine.cpp)
target_include_directories(NovaBendStateRoundTripValidation PRIVATE ${NOVA_BEND_CORE_DIR} ${NOVA_TUNE_CORE_DIR})
target_link_libraries(NovaBendStateRoundTripValidation PRIVATE
    NovaBend_WebUI
    juce::juce_core juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_data_structures juce::juce_dsp
    juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaBendStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaBendStateRoundTripValidation"
    JUCE_WEB_BROWSER=1)
if(APPLE)
    target_compile_definitions(NovaBendStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()
