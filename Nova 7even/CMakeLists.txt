# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaSeven VERSION 0.1.0)

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

set(NOVA_SEVEN_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaSevenCore)

juce_add_binary_data(NovaSeven_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/img/wood-strip.jpg
        Source/ui/public/img/faceplate-seamless.jpg
        Source/ui/public/img/knob-red.jpg
        Source/ui/public/img/knob-dark.jpg
        Source/ui/public/img/knob-cream.jpg
        Source/ui/public/img/desk-knob-red.png
        Source/ui/public/img/desk-knob-tan.png
        Source/ui/public/img/desk-knob-rim-red.png
        Source/ui/public/img/desk-knob-teal.png
        Source/ui/public/img/desk-knob-rim-teal.png
        Source/ui/public/img/vu-face.jpg
    NAMESPACE nova_seven_BinaryData
)

juce_add_plugin(NovaSeven
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE Nv7e
    FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone
    PRODUCT_NAME "Nova 7even"
    VST3_CATEGORIES Fx Eq
    AAX_CATEGORY EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaSeven
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.hpp
        Source/ABSnapshot.h
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaTransformerStage.cpp
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaTransformerStage.h
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaClassAAmp.cpp
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaClassAAmp.h
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaOversampledColorStage.cpp
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaOversampledColorStage.h
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaSevenEQ.cpp
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaSevenEQ.h
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaVUBallistics.cpp
        ${NOVA_SEVEN_CORE_DIR}/DSP/NovaVUBallistics.h
)

target_include_directories(NovaSeven PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSeven
    PRIVATE
        NovaSeven_WebUI
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

target_compile_definitions(NovaSeven
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(APPLE)
    target_compile_definitions(NovaSeven PUBLIC JUCE_USE_CURL=0)
endif()

# ── Transformer stage validation harness ─────────────────────────────────
# Drives NovaTransformerStage directly: silence-in/silence-out, DC
# boundedness, near-linear at low level, harmonic content scales with both
# input amplitude and driveLevel, no NaN/Inf at extreme drive, L/R
# channel independence. See TransformerStageValidation.cpp.

juce_add_console_app(NovaSevenTransformerStageValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.transformerstagevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenTransformerStageValidation PRIVATE
    Source/TransformerStageValidation.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaTransformerStage.cpp
)

target_include_directories(NovaSevenTransformerStageValidation PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenTransformerStageValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaSevenTransformerStageValidation PRIVATE
    JucePlugin_Name="NovaSevenTransformerStageValidation"
)

if(APPLE)
    target_compile_definitions(NovaSevenTransformerStageValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Class-A amp validation harness ────────────────────────────────────────
# Drives NovaClassAAmp directly: Clean/Classic/Hot produce distinct THD and
# 2nd-harmonic content at matched input, sag envelope builds progressively
# rather than reacting instantly to transients, bounded output at max
# drive. See ClassAAmpValidation.cpp.

juce_add_console_app(NovaSevenClassAAmpValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.classaampvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenClassAAmpValidation PRIVATE
    Source/ClassAAmpValidation.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaClassAAmp.cpp
)

target_include_directories(NovaSevenClassAAmpValidation PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenClassAAmpValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaSevenClassAAmpValidation PRIVATE
    JucePlugin_Name="NovaSevenClassAAmpValidation"
)

if(APPLE)
    target_compile_definitions(NovaSevenClassAAmpValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Oversampling wrapper validation harness ───────────────────────────────
# Drives NovaOversampledColorStage directly: latency reporting, aliasing
# reduction vs. non-oversampled processing of the same input, correct
# finite/bounded behavior at 44.1/48/88.2/96/192kHz. See
# OversamplingValidation.cpp.

juce_add_console_app(NovaSevenOversamplingValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.oversamplingvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenOversamplingValidation PRIVATE
    Source/OversamplingValidation.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaClassAAmp.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaOversampledColorStage.cpp
)

target_include_directories(NovaSevenOversamplingValidation PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenOversamplingValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaSevenOversamplingValidation PRIVATE
    JucePlugin_Name="NovaSevenOversamplingValidation"
)

if(APPLE)
    target_compile_definitions(NovaSevenOversamplingValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── EQ validation harness ──────────────────────────────────────────────
# Drives NovaSevenEQ directly: for every stepped HPF/Low/Mid choice,
# measures the actual filter response at its labeled frequency and
# asserts it matches (plan section 8.2's explicit gate), at both 44.1kHz
# and 96kHz. See EQValidation.cpp.

juce_add_console_app(NovaSevenEQValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.eqvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenEQValidation PRIVATE
    Source/EQValidation.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaSevenEQ.cpp
)

target_include_directories(NovaSevenEQValidation PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenEQValidation PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaSevenEQValidation PRIVATE
    JucePlugin_Name="NovaSevenEQValidation"
)

if(APPLE)
    target_compile_definitions(NovaSevenEQValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── VU ballistics validation harness ──────────────────────────────────────
# Drives NovaVUBallistics directly: step response settles to ~99% at
# 300ms (ANSI C16.5), symmetric attack/release time constants, 0VU
# calibration against a settled -18dBFS input. See VUMeterValidation.cpp.

juce_add_console_app(NovaSevenVUMeterValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.vumetervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenVUMeterValidation PRIVATE
    Source/VUMeterValidation.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaVUBallistics.cpp
)

target_include_directories(NovaSevenVUMeterValidation PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenVUMeterValidation PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaSevenVUMeterValidation PRIVATE
    JucePlugin_Name="NovaSevenVUMeterValidation"
)

if(APPLE)
    target_compile_definitions(NovaSevenVUMeterValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Full-chain DSP harness ─────────────────────────────────────────────
# Drives the REAL NovaSevenAudioProcessor class directly (not a
# reimplementation) through the section 63 suite - silence, DC, sine
# sweep, impulse, white/pink noise, transient burst - plus Mix=0%/bypass/
# phase-invert exactness checks and latency reporting. Needs the full
# PluginProcessor/PluginEditor/WebUI dependency set since
# PluginProcessor::createEditor() references NovaSevenEditor directly.
# See DSPHarness.cpp.

juce_add_console_app(NovaSevenDSPHarness
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.dspharness"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenDSPHarness PRIVATE
    Source/DSPHarness.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaClassAAmp.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaOversampledColorStage.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaSevenEQ.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaVUBallistics.cpp
)

target_include_directories(NovaSevenDSPHarness PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenDSPHarness PRIVATE
    NovaSeven_WebUI
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

target_compile_definitions(NovaSevenDSPHarness PRIVATE
    JucePlugin_Name="NovaSevenDSPHarness"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaSevenDSPHarness PRIVATE JUCE_USE_CURL=0)
endif()

# ── A/B snapshot validation harness ───────────────────────────────────────
# Drives the REAL NovaSevenAudioProcessor + its ABSnapshot member: capture
# A, edit to B, recall A restores exact original values, both snapshots
# survive a save/reload cycle, copyActiveToOther. See ABSnapshotValidation.cpp.

juce_add_console_app(NovaSevenABSnapshotValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaseven.absnapshotvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSevenABSnapshotValidation PRIVATE
    Source/ABSnapshotValidation.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/ParameterIDs.hpp
    Source/ABSnapshot.h
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaTransformerStage.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaClassAAmp.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaOversampledColorStage.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaSevenEQ.cpp
    ${NOVA_SEVEN_CORE_DIR}/DSP/NovaVUBallistics.cpp
)

target_include_directories(NovaSevenABSnapshotValidation PRIVATE ${NOVA_SEVEN_CORE_DIR})

target_link_libraries(NovaSevenABSnapshotValidation PRIVATE
    NovaSeven_WebUI
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

target_compile_definitions(NovaSevenABSnapshotValidation PRIVATE
    JucePlugin_Name="NovaSevenABSnapshotValidation"
    JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaSevenABSnapshotValidation PRIVATE JUCE_USE_CURL=0)
endif()
