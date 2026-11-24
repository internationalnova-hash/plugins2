# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
# Ensure JUCE does not search for VST2 SDK
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    # Default to universal, but allow CI/local builds to override (e.g. arm64-only).
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaPitch VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK (Avid Pro Tools format) - shared across plugins, same pattern as
# the shared NovaLicense module. Mac/Windows only (JUCE's own AAX SDK path
# function silently no-ops on other platforms).
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(RUBBERBAND_LINK_TARGETS "")

set(NOVA_PITCH_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaPitchCore)
# New tuner (shared with Nova AI Vocal) - NovaPitchCore is still used for the Formant Warp stage
set(NOVA_TUNE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTuneCore)

set(PLUGIN_TARGET NovaPitch)
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
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/NovaPitch")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova Pitch: Building for ${PLUGIN_FORMATS}")
message(STATUS "Nova Pitch: WebView backend: ${WEBVIEW_BACKEND}")

juce_add_binary_data(NovaPitch_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_pitch_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novapitch"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvPt
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Pitch"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Pitch
    AAX_CATEGORY PitchShift
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE   # Harmony MIDI mode
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
    Source/NovaPitchTunedEngine.cpp
    Source/NovaPitchHarmony.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.hpp
        Source/ABSnapshot.h
        Source/NovaPitchPresetManager.cpp
        Source/NovaPitchPresetManager.h
        # Core DSP sources are added here incrementally as each subsystem
        # (NovaPitchDetector, NovaScaleQuantizer, NovaPitchSynchronousShifter,
        # NovaFormantCorrector, NovaPitchEngine) is built and validated
        # standalone - matching the proven build order from Nova Doubler.
        ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
        ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.h
        ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
        ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.h
        ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
        ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.h
        ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
        ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.h
        ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.cpp
        ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.h
        ${NOVA_PITCH_CORE_DIR}/NovaPitchTypes.h
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_PITCH_CORE_DIR} ${NOVA_TUNE_CORE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaPitch_WebUI
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
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# RubberBand removed — plugin uses built-in dual-head pitch shifter

    target_compile_definitions(${PLUGIN_TARGET}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        # MIDI input is only for Harmony notes: don't expose 2048 "MIDI CC" parameters in hosts
        JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
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

    if(TARGET ${PLUGIN_TARGET}_Standalone)
        target_link_libraries(${PLUGIN_TARGET}_Standalone
            PRIVATE
                asound
                jack
        )
    endif()
endif()

# ── Offline diagnostic harness ──────────────────────────────────────────────
# Feeds a WAV file through the real PluginProcessor::processBlock() offline
# and dumps its own internal detected/corrected-pitch state per block, for
# root-cause investigation without relying on external pitch-tracking
# approximations. Read-only observer via PluginProcessor's public getters -
# no changes to the plugin's own source.

juce_add_console_app(NovaPitchOfflineDiag
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.offlinediag"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchOfflineDiag PRIVATE
    Source/OfflineDiag.cpp
    Source/PluginProcessor.cpp
    Source/NovaPitchTunedEngine.cpp
    Source/NovaPitchHarmony.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/NovaPitchPresetManager.cpp
    Source/NovaPitchPresetManager.h
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
    ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
    ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.cpp
)

target_include_directories(NovaPitchOfflineDiag PRIVATE ${NOVA_PITCH_CORE_DIR} ${NOVA_TUNE_CORE_DIR})

target_link_libraries(NovaPitchOfflineDiag PRIVATE
    NovaPitch_WebUI
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
    juce::juce_opengl
    juce::juce_recommended_config_flags
)

juce_generate_juce_header(NovaPitchOfflineDiag)

target_compile_definitions(NovaPitchOfflineDiag PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
    JucePlugin_Name="NovaPitchOfflineDiag"
)

if(APPLE)
    target_compile_definitions(NovaPitchOfflineDiag PRIVATE JUCE_USE_CURL=0)
endif()

# ── Pitch detector validation harness ────────────────────────────────────
# Drives NovaPitchDetector directly: pitch accuracy across the vocal
# range, confidence-gate hold-through-silence/noise behavior, analysis
# window sizing (2048 vs 4096 vs low-latency-forced-2048) across sample
# rates. See PitchDetectorValidation.cpp.

juce_add_console_app(NovaPitchDetectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.detectorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchDetectorValidation PRIVATE
    Source/PitchDetectorValidation.cpp
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
)

target_include_directories(NovaPitchDetectorValidation PRIVATE ${NOVA_PITCH_CORE_DIR})

target_link_libraries(NovaPitchDetectorValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchDetectorValidation PRIVATE
    JucePlugin_Name="NovaPitchDetectorValidation"
)

if(APPLE)
    target_compile_definitions(NovaPitchDetectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Scale quantizer validation harness ───────────────────────────────────
# Drives NovaScaleQuantizer directly: nearestScaleMidi() regression-matches
# the retired quantizeToScale() across all 5 scales, Amount=0 is a
# steady-state passthrough, RetuneSpeed reshapes glide time monotonically,
# octave-invariance. See ScaleQuantizerValidation.cpp.

juce_add_console_app(NovaPitchScaleQuantizerValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.scalequantizervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchScaleQuantizerValidation PRIVATE
    Source/ScaleQuantizerValidation.cpp
    ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
)

target_include_directories(NovaPitchScaleQuantizerValidation PRIVATE ${NOVA_PITCH_CORE_DIR})

target_link_libraries(NovaPitchScaleQuantizerValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchScaleQuantizerValidation PRIVATE
    JucePlugin_Name="NovaPitchScaleQuantizerValidation"
)

if(APPLE)
    target_compile_definitions(NovaPitchScaleQuantizerValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Formant corrector validation harness ─────────────────────────────────
# Drives NovaFormantCorrector directly: processPost(processPre(x)) is a
# bit/near-bit-transparent round trip at amount=0 (any warp) and at
# amount>0/warp=0; nonzero amount measurably reduces formant-frequency
# movement under a synthetic pitch shift vs. no correction.
# See FormantCorrectorValidation.cpp.

juce_add_console_app(NovaPitchFormantCorrectorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.formantcorrectorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchFormantCorrectorValidation PRIVATE
    Source/FormantCorrectorValidation.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
)

target_include_directories(NovaPitchFormantCorrectorValidation PRIVATE ${NOVA_PITCH_CORE_DIR})

target_link_libraries(NovaPitchFormantCorrectorValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchFormantCorrectorValidation PRIVATE
    JucePlugin_Name="NovaPitchFormantCorrectorValidation"
)

if(APPLE)
    target_compile_definitions(NovaPitchFormantCorrectorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Pitch-synchronous shifter validation harness ─────────────────────────
# Drives NovaPitchSynchronousShifter directly: unity-ratio transparency,
# known-shift cents accuracy, and the key regression test - a simulated-
# vibrato instantaneous input pitch produces a measurably SMALLER output
# cents deviation than the input's own modulation, proving the old "locked
# reference passes vibrato straight through" bug class can't recur here.
# See ShifterValidation.cpp.

juce_add_console_app(NovaPitchShifterValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.shiftervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchShifterValidation PRIVATE
    Source/ShifterValidation.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
)

target_include_directories(NovaPitchShifterValidation PRIVATE ${NOVA_PITCH_CORE_DIR})

target_link_libraries(NovaPitchShifterValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchShifterValidation PRIVATE
    JucePlugin_Name="NovaPitchShifterValidation"
)

if(APPLE)
    target_compile_definitions(NovaPitchShifterValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Engine integration validation harness ────────────────────────────────
# Drives NovaPitchEngine directly (not through PluginProcessor/JUCE
# hosting): mono/stereo handling, automation across all params, sample-
# rate independence, getLatencySamples() matches measured delay,
# silence-in/silence-out with no NaN/denormal blowup, reset() safety
# mid-stream. See EngineValidation.cpp.

juce_add_console_app(NovaPitchEngineValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.enginevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchEngineValidation PRIVATE
    Source/EngineValidation.cpp
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
    ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
    ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.cpp
)

target_include_directories(NovaPitchEngineValidation PRIVATE ${NOVA_PITCH_CORE_DIR})

target_link_libraries(NovaPitchEngineValidation PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchEngineValidation PRIVATE
    JucePlugin_Name="NovaPitchEngineValidation"
)

if(APPLE)
    target_compile_definitions(NovaPitchEngineValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── State save/recall round-trip validation harness ──────────────────────
# Drives the REAL NovaPitchAudioProcessor class through
# getStateInformation()/setStateInformation() and confirms all params
# survive, with particular attention to the formant bool->float migration.
# See StateRoundTripValidation.cpp.

juce_add_console_app(NovaPitchStateRoundTripValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.stateroundtripvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchStateRoundTripValidation PRIVATE
    Source/StateRoundTripValidation.cpp
    Source/PluginProcessor.cpp
    Source/NovaPitchTunedEngine.cpp
    Source/NovaPitchHarmony.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/NovaPitchPresetManager.cpp
    Source/NovaPitchPresetManager.h
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
    ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
    ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.cpp
)

target_include_directories(NovaPitchStateRoundTripValidation PRIVATE ${NOVA_PITCH_CORE_DIR} ${NOVA_TUNE_CORE_DIR})

target_link_libraries(NovaPitchStateRoundTripValidation PRIVATE
    NovaPitch_WebUI
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchStateRoundTripValidation PRIVATE
    JucePlugin_Name="NovaPitchStateRoundTripValidation"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaPitchStateRoundTripValidation PRIVATE JUCE_USE_CURL=0)
endif()

# Harmony feature checks (see HarmonyValidation.cpp)
juce_add_console_app(NovaPitchHarmonyValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.harmonyvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchHarmonyValidation PRIVATE
    Source/HarmonyValidation.cpp
    Source/PluginProcessor.cpp
    Source/NovaPitchTunedEngine.cpp
    Source/NovaPitchHarmony.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/NovaPitchPresetManager.cpp
    Source/NovaPitchPresetManager.h
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
    ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
    ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.cpp
)

target_include_directories(NovaPitchHarmonyValidation PRIVATE ${NOVA_PITCH_CORE_DIR} ${NOVA_TUNE_CORE_DIR})

target_link_libraries(NovaPitchHarmonyValidation PRIVATE
    NovaPitch_WebUI
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchHarmonyValidation PRIVATE
    JucePlugin_Name="NovaPitchHarmonyValidation"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaPitchHarmonyValidation PRIVATE JUCE_USE_CURL=0)
endif()

# Tracking mode checks (see TrackingValidation.cpp)
juce_add_console_app(NovaPitchTrackingValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novapitch.trackingvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaPitchTrackingValidation PRIVATE
    Source/TrackingValidation.cpp
    Source/PluginProcessor.cpp
    Source/NovaPitchTunedEngine.cpp
    Source/NovaPitchHarmony.cpp
    ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    Source/NovaPitchPresetManager.cpp
    Source/NovaPitchPresetManager.h
    ${NOVA_PITCH_CORE_DIR}/Analysis/NovaPitchDetector.cpp
    ${NOVA_PITCH_CORE_DIR}/Correction/NovaScaleQuantizer.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaPitchSynchronousShifter.cpp
    ${NOVA_PITCH_CORE_DIR}/DSP/NovaFormantCorrector.cpp
    ${NOVA_PITCH_CORE_DIR}/NovaPitchEngine.cpp
)

target_include_directories(NovaPitchTrackingValidation PRIVATE ${NOVA_PITCH_CORE_DIR} ${NOVA_TUNE_CORE_DIR})

target_link_libraries(NovaPitchTrackingValidation PRIVATE
    NovaPitch_WebUI
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaPitchTrackingValidation PRIVATE
    JucePlugin_Name="NovaPitchTrackingValidation"
    JUCE_WEB_BROWSER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

if(APPLE)
    target_compile_definitions(NovaPitchTrackingValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaPitch)
nova_license_test_target(NovaPitchOfflineDiag)
nova_license_test_target(NovaPitchStateRoundTripValidation)
nova_license_test_target(NovaPitchHarmonyValidation)
nova_license_test_target(NovaPitchTrackingValidation)
