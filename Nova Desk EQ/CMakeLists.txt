# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaDeskEQ VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK path wired from day one (established catalog rollout pattern) -
# NOT built until commercial AAX infra is ready, so AAX is deliberately
# excluded from FORMATS below (matches Nova 7even's posture).
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NOVA_DESK_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaDeskCore)

juce_add_binary_data(NovaDeskEQ_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/app.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/mockup-faceplate-tile.jpg
        Source/ui/public/img/mockup-wood.jpg
        Source/ui/public/img/knob-red.png
        Source/ui/public/img/knob-tan.png
        Source/ui/public/img/knob-green.png
        Source/ui/public/img/knob-blue.png
        Source/ui/public/img/knob-rim-red.png
        Source/ui/public/img/knob-rim-green.png
    NAMESPACE nova_desk_eq_BinaryData
)

juce_add_plugin(NovaDeskEQ
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadeskeq"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvDk
    FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone
    PRODUCT_NAME "Nova Desk EQ"
    VST3_CATEGORIES Fx Eq
    AAX_CATEGORY EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaDeskEQ
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.hpp
        Source/ABSnapshot.h
        Source/Presets/NovaDeskPreset.h
        Source/Presets/NovaDeskFactoryPresets.h
        Source/Presets/NovaDeskFactoryPresets.cpp
        Source/Presets/NovaDeskPresetManager.h
        Source/Presets/NovaDeskPresetManager.cpp
        ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskEQBands.cpp
        ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskEQBands.h
        ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskDrive.cpp
        ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskDrive.h
        ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskTypes.h
)

target_include_directories(NovaDeskEQ PRIVATE ${NOVA_DESK_CORE_DIR})

target_link_libraries(NovaDeskEQ
    PRIVATE
        NovaDeskEQ_WebUI
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

target_compile_definitions(NovaDeskEQ
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(APPLE)
    target_compile_definitions(NovaDeskEQ PUBLIC JUCE_USE_CURL=0)
endif()

# ── DSP validation harnesses ──────────────────────────────────────────────
# Added incrementally as each _shared/NovaDeskCore DSP class lands (tasks
# #153/#154), matching the catalog's newest testing convention (see
# Nova 7even's CMakeLists.txt for the pattern this will follow).

# ── EQ bands validation harness ───────────────────────────────────────────
# Drives NovaDeskEQBands directly via its analytic magnitude-response
# methods: HPF/LPF -3dB corners land at the labeled frequency, band gain
# at center matches the requested dB, Bell vs Shelf shape differs
# correctly, E vs G personality produces a real measurable Q difference
# (not cosmetic), and no NaN/Inf at extreme parameter combinations. See
# EQBandsValidation.cpp.

juce_add_console_app(NovaDeskEQBandsValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadeskeq.eqbandsvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDeskEQBandsValidation PRIVATE
    Source/EQBandsValidation.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskEQBands.cpp
)

target_include_directories(NovaDeskEQBandsValidation PRIVATE ${NOVA_DESK_CORE_DIR})

target_link_libraries(NovaDeskEQBandsValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDeskEQBandsValidation PRIVATE
    JucePlugin_Name="NovaDeskEQBandsValidation"
)

if(APPLE)
    target_compile_definitions(NovaDeskEQBandsValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Drive stage validation harness ────────────────────────────────────────
# Drives NovaDeskDrive directly via sine-sweep Goertzel probes: THD rises
# monotonically with drive amount, E vs G produce a real measurable
# 2nd-harmonic (asymmetry) difference at matched drive, the DC blocker
# keeps output offset near zero despite asymmetric shaping, oversampling
# measurably reduces aliased energy vs running un-oversampled, drive~0
# bypasses the stage exactly (bit-identical output, zero latency), and no
# NaN/Inf at extreme drive. See DriveValidation.cpp.

juce_add_console_app(NovaDeskDriveValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadeskeq.drivevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDeskDriveValidation PRIVATE
    Source/DriveValidation.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskDrive.cpp
)

target_include_directories(NovaDeskDriveValidation PRIVATE ${NOVA_DESK_CORE_DIR})

target_link_libraries(NovaDeskDriveValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDeskDriveValidation PRIVATE
    JucePlugin_Name="NovaDeskDriveValidation"
)

if(APPLE)
    target_compile_definitions(NovaDeskDriveValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Full-chain DSP harness ─────────────────────────────────────────────────
# Drives the REAL NovaDeskEQAudioProcessor directly (not a reimplementation)
# through silence/DC/sine-sweep/impulse/noise/transient probes, Mix=0%
# dry-match, hard bypass, Drive-dependent latency reporting, and an
# integration-level E-vs-G measurable-difference check. See DSPHarness.cpp.

juce_add_console_app(NovaDeskEQDSPHarness
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadeskeq.dspharness"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDeskEQDSPHarness PRIVATE
    Source/DSPHarness.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/Presets/NovaDeskPreset.h
    Source/Presets/NovaDeskFactoryPresets.h
    Source/Presets/NovaDeskFactoryPresets.cpp
    Source/Presets/NovaDeskPresetManager.h
    Source/Presets/NovaDeskPresetManager.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskEQBands.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskDrive.cpp
)

target_include_directories(NovaDeskEQDSPHarness PRIVATE ${NOVA_DESK_CORE_DIR})

target_link_libraries(NovaDeskEQDSPHarness PRIVATE
    NovaDeskEQ_WebUI
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

target_compile_definitions(NovaDeskEQDSPHarness PRIVATE
    JucePlugin_Name="NovaDeskEQDSPHarness"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaDeskEQDSPHarness PRIVATE JUCE_USE_CURL=0)
endif()

# ── Preset gain-matching tool ──────────────────────────────────────────────
# One-time authoring tool (not a regression harness) that measures each
# factory preset's broadband RMS against a neutral reference and prints a
# suggested Output trim for presets the spec leaves as "Gain-match" -
# see PresetGainMatch.cpp for the full methodology/caveats.

juce_add_console_app(NovaDeskEQPresetGainMatch
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadeskeq.presetgainmatch"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDeskEQPresetGainMatch PRIVATE
    Source/PresetGainMatch.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/Presets/NovaDeskPreset.h
    Source/Presets/NovaDeskFactoryPresets.h
    Source/Presets/NovaDeskFactoryPresets.cpp
    Source/Presets/NovaDeskPresetManager.h
    Source/Presets/NovaDeskPresetManager.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskEQBands.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskDrive.cpp
)

target_include_directories(NovaDeskEQPresetGainMatch PRIVATE ${NOVA_DESK_CORE_DIR})

target_link_libraries(NovaDeskEQPresetGainMatch PRIVATE
    NovaDeskEQ_WebUI
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

target_compile_definitions(NovaDeskEQPresetGainMatch PRIVATE
    JucePlugin_Name="NovaDeskEQPresetGainMatch"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaDeskEQPresetGainMatch PRIVATE JUCE_USE_CURL=0)
endif()

# ── Preset validation harness ──────────────────────────────────────────────
# Drives the REAL NovaDeskEQAudioProcessor through every factory preset:
# library shape (26 unique presets, 7 categories), exact value application,
# finite/bounded audio on every preset, next/prev navigation wraparound,
# click-free mid-stream preset switching, and selected-index session
# recall. See PresetValidation.cpp.

juce_add_console_app(NovaDeskEQPresetValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadeskeq.presetvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDeskEQPresetValidation PRIVATE
    Source/PresetValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/Presets/NovaDeskPreset.h
    Source/Presets/NovaDeskFactoryPresets.h
    Source/Presets/NovaDeskFactoryPresets.cpp
    Source/Presets/NovaDeskPresetManager.h
    Source/Presets/NovaDeskPresetManager.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskEQBands.cpp
    ${NOVA_DESK_CORE_DIR}/DSP/NovaDeskDrive.cpp
)

target_include_directories(NovaDeskEQPresetValidation PRIVATE ${NOVA_DESK_CORE_DIR})

target_link_libraries(NovaDeskEQPresetValidation PRIVATE
    NovaDeskEQ_WebUI
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

target_compile_definitions(NovaDeskEQPresetValidation PRIVATE
    JucePlugin_Name="NovaDeskEQPresetValidation"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaDeskEQPresetValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaDeskEQ)
nova_license_test_target(NovaDeskEQDSPHarness)
nova_license_test_target(NovaDeskEQPresetGainMatch)
nova_license_test_target(NovaDeskEQPresetValidation)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaDeskEQ)
