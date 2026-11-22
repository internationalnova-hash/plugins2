# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaAIMasterStandalone VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# Nova AI Master Standalone
# Import -> analyze -> master -> preview -> export desktop app.
# Shares its DSP/analysis/decision core with the "Nova AI Master" plugin -
# see ../_shared/NovaAIMasterCore. Do NOT duplicate mastering logic here.

juce_add_binary_data(NovaAIMasterStandalone_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_ai_master_standalone_BinaryData
)

juce_add_gui_app(NovaAIMasterStandalone
    COMPANY_NAME  "International Nova"
    BUNDLE_ID     "com.internationalnova.novaaimaster.standalone"
    PRODUCT_NAME  "Nova AI Master"
    VERSION       ${PROJECT_VERSION}
)

set(NOVA_AI_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaAIMasterCore)

target_sources(NovaAIMasterStandalone
    PRIVATE
        Source/Main.cpp
        Source/MainWindow.h
        Source/MainWindow.cpp
        Source/BufferAudioSource.h
        Source/AudioEngine.h
        Source/AudioEngine.cpp
        ${NOVA_AI_CORE_DIR}/NovaAIMasterEngine.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaLoudnessAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaDynamicsAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaSpectralAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Analysis/NovaStereoAnalyzer.cpp
        ${NOVA_AI_CORE_DIR}/Decision/NovaMasteringDecisionEngine.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaCorrectiveEQ.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaResonanceControl.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaGlueCompressor.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaSaturation.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaStereoProcessor.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaSoftClipper.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaLimiter.cpp
        ${NOVA_AI_CORE_DIR}/DSP/NovaMasteringChain.cpp
)

target_include_directories(NovaAIMasterStandalone PRIVATE ${NOVA_AI_CORE_DIR})

juce_generate_juce_header(NovaAIMasterStandalone)

target_link_libraries(NovaAIMasterStandalone
    PRIVATE
        NovaAIMasterStandalone_WebUI
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
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(NovaAIMasterStandalone
    PUBLIC
        JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaAIMasterStandalone PUBLIC JUCE_USE_CURL=0)
endif()

# ── End-to-end validation harness ───────────────────────────────────────
# Drives the real NovaAIMasterAudioEngine (same class the app's UI calls)
# against a real WAV file: import -> analyze+master -> export. See
# EndToEndValidation.cpp. Usage: NovaAIMasterEndToEndValidation <wav path>

juce_add_console_app(NovaAIMasterEndToEndValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novaaimaster.endtoendvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaAIMasterEndToEndValidation PRIVATE
    Source/EndToEndValidation.cpp
    Source/AudioEngine.cpp
    ${NOVA_AI_CORE_DIR}/NovaAIMasterEngine.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaLoudnessAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaDynamicsAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaSpectralAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaStereoAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Decision/NovaMasteringDecisionEngine.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaCorrectiveEQ.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaResonanceControl.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaGlueCompressor.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaSaturation.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaStereoProcessor.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaSoftClipper.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaLimiter.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaMasteringChain.cpp
)

target_include_directories(NovaAIMasterEndToEndValidation PRIVATE ${NOVA_AI_CORE_DIR} Source)

target_link_libraries(NovaAIMasterEndToEndValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaAIMasterEndToEndValidation PRIVATE
    JucePlugin_Name="NovaAIMasterEndToEndValidation"
    JUCE_MODAL_LOOPS_PERMITTED=1
)

if(APPLE)
    target_compile_definitions(NovaAIMasterEndToEndValidation PRIVATE JUCE_USE_CURL=0)
endif()
