# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaDoubler VERSION 0.1.0)

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

set(NOVA_DOUBLER_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaDoublerCore)

juce_add_binary_data(NovaDoubler_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_doubler_BinaryData
)

juce_add_plugin(NovaDoubler
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvDb
    FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone
    PRODUCT_NAME "Nova Doubler"
    VST3_CATEGORIES Fx Modulation
    AAX_CATEGORY Modulation
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaDoubler
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.hpp
        # Core DSP sources are added here incrementally as each subsystem
        # (NovaMicroShiftLine, NovaDoublerVoice, NovaVoiceEnsemble,
        # NovaStereoFieldAnalyzer, NovaDoublerEngine) is built and validated
        # standalone - matching the proven build order from Nova Keys.
)

target_include_directories(NovaDoubler PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoubler
    PRIVATE
        NovaDoubler_WebUI
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

target_compile_definitions(NovaDoubler
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(APPLE)
    target_compile_definitions(NovaDoubler PUBLIC JUCE_USE_CURL=0)
endif()

# ── Organic modulator validation harness ────────────────────────────────
# Drives NovaOrganicModulator directly (header-only): bounded output,
# non-periodicity, seed decorrelation, sample-rate-invariant amplitude and
# time constant across 44.1/48/88.2/96kHz. See ModulatorValidation.cpp.

juce_add_console_app(NovaDoublerModulatorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.modulatorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerModulatorValidation PRIVATE
    Source/ModulatorValidation.cpp
)

target_include_directories(NovaDoublerModulatorValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerModulatorValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerModulatorValidation PRIVATE
    JucePlugin_Name="NovaDoublerModulatorValidation"
)

if(APPLE)
    target_compile_definitions(NovaDoublerModulatorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Micro shift-line validation harness ─────────────────────────────────
# Drives NovaMicroShiftLine directly: delay-ms accuracy, pitch-cents
# accuracy across the spec's +-3 to +-12 cent range, bounded jump-
# crossfade discontinuity, buffer sizing scales with sample rate - all at
# 44.1/48/88.2/96kHz. See ShiftLineValidation.cpp.

juce_add_console_app(NovaDoublerShiftLineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.shiftlinevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerShiftLineValidation PRIVATE
    Source/ShiftLineValidation.cpp
)

target_include_directories(NovaDoublerShiftLineValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerShiftLineValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerShiftLineValidation PRIVATE
    JucePlugin_Name="NovaDoublerShiftLineValidation"
)

if(APPLE)
    target_compile_definitions(NovaDoublerShiftLineValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Voice validation harness ─────────────────────────────────────────────
# Drives NovaDoublerVoice directly: finite output under modulation,
# modulation actually has an effect, equal-power pan behaves correctly,
# different seeds produce non-identical output. See VoiceValidation.cpp.

juce_add_console_app(NovaDoublerVoiceValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.voicevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerVoiceValidation PRIVATE
    Source/VoiceValidation.cpp
)

target_include_directories(NovaDoublerVoiceValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerVoiceValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerVoiceValidation PRIVATE
    JucePlugin_Name="NovaDoublerVoiceValidation"
)

if(APPLE)
    target_compile_definitions(NovaDoublerVoiceValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Voice ensemble validation harness ────────────────────────────────────
# Drives NovaVoiceEnsemble directly: mono-compatibility (no destructive
# cancellation summing L+R at max spread), empirical RMS-sweep gain-
# compensation check across the Width x Tightness x Voices grid,
# automation-zipper check, sample-rate independence. See EnsembleValidation.cpp.

juce_add_console_app(NovaDoublerEnsembleValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.ensemblevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerEnsembleValidation PRIVATE
    Source/EnsembleValidation.cpp
)

target_include_directories(NovaDoublerEnsembleValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerEnsembleValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerEnsembleValidation PRIVATE
    JucePlugin_Name="NovaDoublerEnsembleValidation"
)

if(APPLE)
    target_compile_definitions(NovaDoublerEnsembleValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Stereo field analyzer validation harness ─────────────────────────────
# Drives NovaStereoFieldAnalyzer directly against synthetic mono,
# decorrelated-noise, and phase-inverted signals to confirm correlation
# and spread hit their expected extremes. See StereoAnalyzerValidation.cpp.

juce_add_console_app(NovaDoublerStereoAnalyzerValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.stereoanalyzervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerStereoAnalyzerValidation PRIVATE
    Source/StereoAnalyzerValidation.cpp
)

target_include_directories(NovaDoublerStereoAnalyzerValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerStereoAnalyzerValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerStereoAnalyzerValidation PRIVATE
    JucePlugin_Name="NovaDoublerStereoAnalyzerValidation"
)

if(APPLE)
    target_compile_definitions(NovaDoublerStereoAnalyzerValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Integration validation harness ───────────────────────────────────────
# Drives NovaDoublerEngine directly (not through PluginProcessor/JUCE
# hosting) with mono-in and stereo-in test signals end-to-end: dry/wet MIX
# crossfade, bypass ramp, mono/stereo image handling, automation zipper
# across all params. See IntegrationValidation.cpp.

juce_add_console_app(NovaDoublerIntegrationValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.integrationvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerIntegrationValidation PRIVATE
    Source/IntegrationValidation.cpp
)

target_include_directories(NovaDoublerIntegrationValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerIntegrationValidation PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerIntegrationValidation PRIVATE
    JucePlugin_Name="NovaDoublerIntegrationValidation"
)

if(APPLE)
    target_compile_definitions(NovaDoublerIntegrationValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── State save/recall round-trip validation harness ──────────────────────
# Drives the REAL NovaDoublerProcessor class (not a reimplementation)
# through getStateInformation()/setStateInformation() and confirms all 5
# params survive, plus a direct isBusesLayoutSupported() check. Needs the
# full PluginProcessor/PluginEditor/WebUI dependency set since
# PluginProcessor::createEditor() references NovaDoublerEditor directly.
# See StateRoundTripValidation.cpp.

juce_add_console_app(NovaDoublerStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novadoubler.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaDoublerStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
)

target_include_directories(NovaDoublerStateRoundTripValidation PRIVATE ${NOVA_DOUBLER_CORE_DIR})

target_link_libraries(NovaDoublerStateRoundTripValidation PRIVATE
    NovaDoubler_WebUI
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaDoublerStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaDoublerStateRoundTripValidation"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaDoublerStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaDoubler)
nova_license_test_target(NovaDoublerStateRoundTripValidation)
