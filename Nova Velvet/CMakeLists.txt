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

project(NovaVelvet VERSION 1.0.0)

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

set(NOVA_VELVET_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaVelvetCore)

set(PLUGIN_TARGET NovaVelvet)
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

message(STATUS "Nova Velvet: Building for ${PLUGIN_FORMATS}")

juce_add_binary_data(NovaVelvet_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/amber_lamp_idle@2x.png
        Source/ui/public/img/amber_lamp_off@2x.png
        Source/ui/public/img/amber_lamp_on@2x.png
        Source/ui/public/img/amber_lamp_pressed@2x.png
        Source/ui/public/img/analog_mode_neutral_center@2x.png
        Source/ui/public/img/analog_mode_position_bottom@2x.png
        Source/ui/public/img/analog_mode_position_center@2x.png
        Source/ui/public/img/analog_mode_position_top@2x.png
        Source/ui/public/img/bottom_frame_charcoal@2x.png
        Source/ui/public/img/button_menu_shell@2x.png
        Source/ui/public/img/button_next_shell@2x.png
        Source/ui/public/img/button_prev_shell@2x.png
        Source/ui/public/img/button_save_shell@2x.png
        Source/ui/public/img/button_settings_shell@2x.png
        Source/ui/public/img/divider_line.svg
        Source/ui/public/img/divider_metal@2x.png
        Source/ui/public/img/edge_highlight_cyan@2x.png
        Source/ui/public/img/knob_large_pointer_up@2x.png
        Source/ui/public/img/knob_small_pointer_up@2x.png
        Source/ui/public/img/lower_inset_bevel@2x.png
        Source/ui/public/img/lower_inset_blue@2x.png
        Source/ui/public/img/main_faceplate_blue@2x.png
        Source/ui/public/img/menu_ellipsis.svg
        Source/ui/public/img/next_arrow.svg
        Source/ui/public/img/nova_audio_signature@2x.png
        Source/ui/public/img/nova_n_mark.svg
        Source/ui/public/img/preset_selector_shell@2x.png
        Source/ui/public/img/previous_arrow.svg
        Source/ui/public/img/rack_ear@2x.png
        Source/ui/public/img/rack_screw@2x.png
        Source/ui/public/img/save_icon.svg
        Source/ui/public/img/settings_icon.svg
        Source/ui/public/img/stereo_link_off@2x.png
        Source/ui/public/img/stereo_link_on@2x.png
        Source/ui/public/img/time_select_neutral_center@2x.png
        Source/ui/public/img/time_select_position_bottom@2x.png
        Source/ui/public/img/time_select_position_center@2x.png
        Source/ui/public/img/time_select_position_top@2x.png
        Source/ui/public/img/top_header_graphite@2x.png
        Source/ui/public/img/top_header_shell@2x.png
        Source/ui/public/img/tube_active@2x.png
        Source/ui/public/img/tube_idle@2x.png
        Source/ui/public/img/tube_off@2x.png
        Source/ui/public/img/tube_strong@2x.png
        Source/ui/public/img/vu_amber_illumination@2x.png
        Source/ui/public/img/vu_face_blank@2x.png
        Source/ui/public/img/vu_glass_overlay@2x.png
        Source/ui/public/img/vu_housing_empty@2x.png
        Source/ui/public/img/vu_needle_vertical@2x.png
    NAMESPACE nova_velvet_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novavelvet"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvVl
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Velvet"
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
    Source/Presets/NovaVelvetPreset.h
    Source/Presets/NovaVelvetFactoryPresets.h
    Source/Presets/NovaVelvetFactoryPresets.cpp
    Source/Presets/NovaVelvetPresetManager.h
    Source/Presets/NovaVelvetPresetManager.cpp
    ${NOVA_VELVET_CORE_DIR}/NovaVelvetEngine.h
    ${NOVA_VELVET_CORE_DIR}/NovaVelvetEngine.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetCalibration.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetDetector.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetDetector.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTubeStage.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTubeStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTransformerStage.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTransformerStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOversampledInputStage.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOversampledInputStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOversampledOutputStage.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOversampledOutputStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetAnalogModeProfile.h
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetLevelEnvelope.h
    ${NOVA_VELVET_CORE_DIR}/Meter/NovaVUBallistics.h
    ${NOVA_VELVET_CORE_DIR}/Meter/NovaVUBallistics.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_VELVET_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaVelvet_WebUI
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
# Built phase-by-phase as each Core class lands, per the approved plan's
# own "one correction -> rebuild -> remeasure -> keep or revert" rule.
# The full plugin target (juce_add_plugin + WebView UI) is added once the
# DSP core (Phases 1-10) is validated and the UI is wired to the real
# asset pack (Phase 11) - see the plan file.
# ============================================================

juce_add_console_app(NovaVelvetTransferCurveValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.transfercurvevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetTransferCurveValidation PRIVATE
    Source/TransferCurveValidation.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetDetector.cpp)
target_include_directories(NovaVelvetTransferCurveValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetTransferCurveValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetTransferCurveValidation PRIVATE JucePlugin_Name="NovaVelvetTransferCurveValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetTransferCurveValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetAttackValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.attackvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetAttackValidation PRIVATE
    Source/AttackValidation.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.cpp)
target_include_directories(NovaVelvetAttackValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetAttackValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetAttackValidation PRIVATE JucePlugin_Name="NovaVelvetAttackValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetAttackValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetReleaseValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.releasevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetReleaseValidation PRIVATE
    Source/ReleaseValidation.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.cpp)
target_include_directories(NovaVelvetReleaseValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetReleaseValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetReleaseValidation PRIVATE JucePlugin_Name="NovaVelvetReleaseValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetReleaseValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetProgramDependentReleaseValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.programdependentreleasevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetProgramDependentReleaseValidation PRIVATE
    Source/ProgramDependentReleaseValidation.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.cpp)
target_include_directories(NovaVelvetProgramDependentReleaseValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetProgramDependentReleaseValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetProgramDependentReleaseValidation PRIVATE JucePlugin_Name="NovaVelvetProgramDependentReleaseValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetProgramDependentReleaseValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetFixManValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.fixmanvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetFixManValidation PRIVATE
    Source/FixManValidation.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.cpp)
target_include_directories(NovaVelvetFixManValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetFixManValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetFixManValidation PRIVATE JucePlugin_Name="NovaVelvetFixManValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetFixManValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetHarmonicValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.harmonicvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetHarmonicValidation PRIVATE
    Source/HarmonicValidation.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTubeStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTransformerStage.cpp)
target_include_directories(NovaVelvetHarmonicValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetHarmonicValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetHarmonicValidation PRIVATE JucePlugin_Name="NovaVelvetHarmonicValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetHarmonicValidation PRIVATE JUCE_USE_CURL=0)
endif()

set(NOVA_VELVET_ENGINE_SOURCES
    ${NOVA_VELVET_CORE_DIR}/NovaVelvetEngine.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetDetector.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOpticalCell.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTubeStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetTransformerStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOversampledInputStage.cpp
    ${NOVA_VELVET_CORE_DIR}/DSP/NovaVelvetOversampledOutputStage.cpp
    ${NOVA_VELVET_CORE_DIR}/Meter/NovaVUBallistics.cpp)

juce_add_console_app(NovaVelvetEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.enginevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_VELVET_ENGINE_SOURCES})
target_include_directories(NovaVelvetEngineValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetEngineValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetEngineValidation PRIVATE JucePlugin_Name="NovaVelvetEngineValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetNullAndSafetyValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.nullandsafetyvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetNullAndSafetyValidation PRIVATE
    Source/NullAndSafetyValidation.cpp
    ${NOVA_VELVET_ENGINE_SOURCES})
target_include_directories(NovaVelvetNullAndSafetyValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetNullAndSafetyValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetNullAndSafetyValidation PRIVATE JucePlugin_Name="NovaVelvetNullAndSafetyValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetNullAndSafetyValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetFrequencyResponseValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.frequencyresponsevalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetFrequencyResponseValidation PRIVATE
    Source/FrequencyResponseValidation.cpp
    ${NOVA_VELVET_ENGINE_SOURCES})
target_include_directories(NovaVelvetFrequencyResponseValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetFrequencyResponseValidation PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetFrequencyResponseValidation PRIVATE JucePlugin_Name="NovaVelvetFrequencyResponseValidation")
if(APPLE)
    target_compile_definitions(NovaVelvetFrequencyResponseValidation PRIVATE JUCE_USE_CURL=0)
endif()

juce_add_console_app(NovaVelvetStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novavelvet.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION})
target_sources(NovaVelvetStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets/NovaVelvetFactoryPresets.cpp
    Source/Presets/NovaVelvetPresetManager.cpp
    ${NOVA_VELVET_ENGINE_SOURCES})
target_include_directories(NovaVelvetStateRoundTripValidation PRIVATE ${NOVA_VELVET_CORE_DIR})
target_link_libraries(NovaVelvetStateRoundTripValidation PRIVATE
    NovaVelvet_WebUI
    juce::juce_core juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_data_structures juce::juce_dsp
    juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaVelvetStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaVelvetStateRoundTripValidation"
    JUCE_WEB_BROWSER=1)
if(APPLE)
    target_compile_definitions(NovaVelvetStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()
