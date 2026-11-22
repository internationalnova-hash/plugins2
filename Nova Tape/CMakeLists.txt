# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaTape VERSION 0.1.0)

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

set(NOVA_TAPE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTapeCore)

juce_add_binary_data(NovaTape_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/images/reel_left.png
        Source/ui/public/images/reel_right.png
        Source/ui/public/images/vu_meter_pair_housing.png
        Source/ui/public/images/tape_head_center_housing.png
        Source/ui/public/images/knob_large_input.png
        Source/ui/public/images/knob_large_tape.png
        Source/ui/public/images/knob_large_output.png
        Source/ui/public/images/knob_small.png
        Source/ui/public/images/led_meter_vertical.png
        Source/ui/public/images/lamp_red.png
        Source/ui/public/images/lamp_amber.png
        Source/ui/public/images/lamp_green.png
        Source/ui/public/images/lamp_amber_on.png
        Source/ui/public/images/button_header_dark.png
        Source/ui/public/images/button_header_amber.png
        Source/ui/public/images/button_tape_type_blank.png
        Source/ui/public/images/button_ips_blank.png
        Source/ui/public/images/roller_left_large.png
        Source/ui/public/images/roller_left_small.png
        Source/ui/public/images/roller_right_large.png
        Source/ui/public/images/roller_right_small.png
        Source/ui/public/images/mix_slider_track_thumb.png
        Source/ui/public/images/mix_thumb.png
        Source/ui/public/images/footer_treatment.png
        Source/ui/public/images/settings_icon.png
        Source/ui/public/images/menu_icon_button.png
    NAMESPACE nova_tape_BinaryData
)

juce_add_plugin(NovaTape
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novatape"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvTp
    FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone
    PRODUCT_NAME "Nova Tape"
    VST3_CATEGORIES Fx Distortion
    AAX_CATEGORY EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaTape
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.hpp
        Source/ABSnapshot.h
        Source/NovaTapePresetManager.cpp
        Source/NovaTapePresetManager.h
        ${NOVA_TAPE_CORE_DIR}/NovaTapeEngine.cpp
)

target_include_directories(NovaTape PRIVATE ${NOVA_TAPE_CORE_DIR})

target_link_libraries(NovaTape
    PRIVATE
        NovaTape_WebUI
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

target_compile_definitions(NovaTape
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(APPLE)
    target_compile_definitions(NovaTape PUBLIC JUCE_USE_CURL=0)
endif()

# ── Saturator validation harness ─────────────────────────────────────────
# Drives NovaTapeSaturator directly: THD vs input level, harmonic spectrum
# (even-harmonic dominance), TapeType personalities measurably differ, DC
# stays blocked, bounded/finite output under extreme drive. See
# SaturatorValidation.cpp.

juce_add_console_app(NovaTapeSaturatorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novatape.saturatorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaTapeSaturatorValidation PRIVATE
    Source/SaturatorValidation.cpp
)

target_include_directories(NovaTapeSaturatorValidation PRIVATE ${NOVA_TAPE_CORE_DIR})

target_link_libraries(NovaTapeSaturatorValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaTapeSaturatorValidation PRIVATE
    JucePlugin_Name="NovaTapeSaturatorValidation"
)

if(APPLE)
    target_compile_definitions(NovaTapeSaturatorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Head filter / IPS / TapeType response validation harness ────────────
# Drives NovaTapeHeadFilter directly: head-bump frequency/gain response,
# HF rolloff differs meaningfully across 7.5/15/30 IPS and across
# TapeType. See HeadFilterValidation.cpp.

juce_add_console_app(NovaTapeHeadFilterValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novatape.headfiltervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaTapeHeadFilterValidation PRIVATE
    Source/HeadFilterValidation.cpp
)

target_include_directories(NovaTapeHeadFilterValidation PRIVATE ${NOVA_TAPE_CORE_DIR})

target_link_libraries(NovaTapeHeadFilterValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaTapeHeadFilterValidation PRIVATE
    JucePlugin_Name="NovaTapeHeadFilterValidation"
)

if(APPLE)
    target_compile_definitions(NovaTapeHeadFilterValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Engine integration validation harness ────────────────────────────────
# Drives NovaTapeEngine directly: finite/bounded output, mix crossfade,
# bypass crossfade, sample-rate independence, silence-in/silence-out,
# reset() safety, oversampling mode switch safety. See EngineValidation.cpp.

juce_add_console_app(NovaTapeEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novatape.enginevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaTapeEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_TAPE_CORE_DIR}/NovaTapeEngine.cpp
)

target_include_directories(NovaTapeEngineValidation PRIVATE ${NOVA_TAPE_CORE_DIR})

target_link_libraries(NovaTapeEngineValidation PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaTapeEngineValidation PRIVATE
    JucePlugin_Name="NovaTapeEngineValidation"
)

if(APPLE)
    target_compile_definitions(NovaTapeEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── State save/recall round-trip validation harness ──────────────────────
juce_add_console_app(NovaTapeStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novatape.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaTapeStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/NovaTapePresetManager.cpp
    Source/NovaTapePresetManager.h
    ${NOVA_TAPE_CORE_DIR}/NovaTapeEngine.cpp
)

target_include_directories(NovaTapeStateRoundTripValidation PRIVATE ${NOVA_TAPE_CORE_DIR})

target_link_libraries(NovaTapeStateRoundTripValidation PRIVATE
    NovaTape_WebUI
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaTapeStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaTapeStateRoundTripValidation"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaTapeStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()
