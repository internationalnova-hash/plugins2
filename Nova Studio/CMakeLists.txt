set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# Default to an optimised build (an unoptimised audio engine glitches under load).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(NovaStudio VERSION 0.1.0)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_WEB_BROWSER OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# ── Platform-specific dependencies ────────────────────────────────────────────

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    find_package(CURL REQUIRED)
    set(PLATFORM_LIBS ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES} CURL::libcurl asound)
    set(PLATFORM_INCLUDES ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    set(PLATFORM_CFLAGS ${GTK3_CFLAGS_OTHER} ${WEBKIT_CFLAGS_OTHER})
elseif(APPLE)
    find_library(COREAUDIO_LIB CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
    find_library(COREMIDI_LIB CoreMIDI REQUIRED)
    find_library(ACCELERATE_LIB Accelerate REQUIRED)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(IOKIT_LIB IOKit REQUIRED)
    find_library(COREFOUNDATION_LIB CoreFoundation REQUIRED)
    set(PLATFORM_LIBS
        ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} ${COREMIDI_LIB}
        ${ACCELERATE_LIB} ${COCOA_LIB} ${IOKIT_LIB} ${COREFOUNDATION_LIB})
    set(PLATFORM_INCLUDES "")
    set(PLATFORM_CFLAGS "")
elseif(WIN32)
    set(PLATFORM_LIBS winmm ole32 oleaut32 imm32 comdlg32 shlwapi rpcrt4 wininet)
    set(PLATFORM_INCLUDES "")
    set(PLATFORM_CFLAGS "")
endif()

# ── Binary data ───────────────────────────────────────────────────────────────

juce_add_binary_data(NovaStudioBinaryData SOURCES
    Source/ui/Assets/n_logo.png
)

# ── Main application ──────────────────────────────────────────────────────────

juce_add_gui_app(NovaStudio
    COMPANY_NAME        "International Nova"
    BUNDLE_ID           "com.internationalnova.novastudio"
    PRODUCT_NAME        "Nova Studio"
    VERSION             ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "Nova Studio needs microphone access to record audio."
    DOCUMENT_BROWSER_ENABLED        TRUE
    FILE_SHARING_ENABLED            TRUE
)

# Nova AI Master's own analysis/decision core (shared source, one engine everywhere)
set(NOVA_AI_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaAIMasterCore)
set(NOVA_AI_CORE_SOURCES
    ${NOVA_AI_CORE_DIR}/NovaAIMasterEngine.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaLoudnessAnalyzer.cpp ${NOVA_AI_CORE_DIR}/Analysis/NovaDynamicsAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Analysis/NovaSpectralAnalyzer.cpp ${NOVA_AI_CORE_DIR}/Analysis/NovaStereoAnalyzer.cpp
    ${NOVA_AI_CORE_DIR}/Decision/NovaMasteringDecisionEngine.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaCorrectiveEQ.cpp ${NOVA_AI_CORE_DIR}/DSP/NovaResonanceControl.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaGlueCompressor.cpp ${NOVA_AI_CORE_DIR}/DSP/NovaSaturation.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaStereoProcessor.cpp ${NOVA_AI_CORE_DIR}/DSP/NovaSoftClipper.cpp
    ${NOVA_AI_CORE_DIR}/DSP/NovaLimiter.cpp ${NOVA_AI_CORE_DIR}/DSP/NovaMasteringChain.cpp)

target_sources(NovaStudio PRIVATE
    Source/mix/MixAnalysis.cpp
    Source/mix/MixPlanner.cpp
    Source/mix/MixApplier.cpp
    Source/ui/MixAssistantWindow.cpp
    ${NOVA_AI_CORE_SOURCES}
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/StudioAudioEngine.cpp
    Source/StudioAudioEngineCore.cpp
    Source/StudioAudioEngineSession.cpp
    Source/ClipStreamer.cpp
    Source/TimeStretcher.cpp
    Source/StudioAudioEngineWarp.cpp
    Source/TransportState.cpp
    Source/TimelineModel.cpp
    Source/Session.cpp
    Source/ArrangementModel.cpp
    Source/NovaAlignProcessor.cpp
    Source/ui/StudioComponents.cpp
    Source/ui/WaveformCache.cpp
    Source/ui/Theme.cpp
    Source/ui/EditWindow.cpp
    Source/ui/MixerWindow.cpp
    Source/ui/BeatWindow.cpp
    Source/ui/PluginBrowserPanel.cpp
    Source/ui/MacroPanel.cpp
)
target_include_directories(NovaStudio PRIVATE ${NOVA_AI_CORE_DIR})

target_link_libraries(NovaStudio PRIVATE
    NovaStudioBinaryData
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
    juce::juce_recommended_config_flags
    ${PLATFORM_LIBS}
)

if(PLATFORM_INCLUDES)
    target_include_directories(NovaStudio PRIVATE ${PLATFORM_INCLUDES})
endif()
if(PLATFORM_CFLAGS)
    target_compile_options(NovaStudio PRIVATE ${PLATFORM_CFLAGS})
endif()

juce_generate_juce_header(NovaStudio)

target_compile_definitions(NovaStudio PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_AU=$<IF:$<BOOL:${APPLE}>,1,0>
)

# ── Validation test executable ────────────────────────────────────────────────

juce_add_console_app(NovaStudioValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novastudio.validation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaStudioValidation PRIVATE
    Source/ValidationTest.cpp
    Source/StudioAudioEngine.cpp
    Source/StudioAudioEngineCore.cpp
    Source/StudioAudioEngineSession.cpp
    Source/ClipStreamer.cpp
    Source/TimeStretcher.cpp
    Source/StudioAudioEngineWarp.cpp
    Source/TransportState.cpp
    Source/TimelineModel.cpp
    Source/Session.cpp
    Source/ArrangementModel.cpp
    Source/NovaAlignProcessor.cpp
)

target_link_libraries(NovaStudioValidation PRIVATE
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
    juce::juce_recommended_config_flags
    ${PLATFORM_LIBS}
)

if(PLATFORM_INCLUDES)
    target_include_directories(NovaStudioValidation PRIVATE ${PLATFORM_INCLUDES})
endif()
if(PLATFORM_CFLAGS)
    target_compile_options(NovaStudioValidation PRIVATE ${PLATFORM_CFLAGS})
endif()

juce_generate_juce_header(NovaStudioValidation)

target_compile_definitions(NovaStudioValidation PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_AU=$<IF:$<BOOL:${APPLE}>,1,0>
)

# ── Mix Assistant checks on real sessions ───────────────────────────────────
#   NovaStudioMixTest <session.novastudio> ...            what each track is
#   NovaStudioMixTest --apply <session> <outdir> [style]   plan + apply + render before/after
juce_add_console_app(NovaStudioMixTest PRODUCT_NAME "NovaStudioMixTest")
target_sources(NovaStudioMixTest PRIVATE
    Tests/MixAssistantTest.cpp
    Source/mix/MixAnalysis.cpp Source/mix/MixPlanner.cpp Source/mix/MixApplier.cpp Source/ui/MixAssistantWindow.cpp
    Source/StudioAudioEngine.cpp Source/StudioAudioEngineCore.cpp Source/StudioAudioEngineSession.cpp
    Source/ClipStreamer.cpp Source/TimeStretcher.cpp Source/StudioAudioEngineWarp.cpp Source/TransportState.cpp
    Source/TimelineModel.cpp Source/Session.cpp Source/ArrangementModel.cpp Source/NovaAlignProcessor.cpp
    ${NOVA_AI_CORE_SOURCES})
target_include_directories(NovaStudioMixTest PRIVATE ${NOVA_AI_CORE_DIR})
target_link_libraries(NovaStudioMixTest PRIVATE
    juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats juce::juce_audio_processors
    juce::juce_audio_utils juce::juce_core juce::juce_data_structures juce::juce_dsp juce::juce_events
    juce::juce_graphics juce::juce_gui_basics juce::juce_recommended_config_flags ${PLATFORM_LIBS})
if(PLATFORM_INCLUDES)
    target_include_directories(NovaStudioMixTest PRIVATE ${PLATFORM_INCLUDES})
endif()
if(PLATFORM_CFLAGS)
    target_compile_options(NovaStudioMixTest PRIVATE ${PLATFORM_CFLAGS})
endif()
juce_generate_juce_header(NovaStudioMixTest)
target_compile_definitions(NovaStudioMixTest PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_LV2=1 JUCE_PLUGINHOST_AU=$<IF:$<BOOL:${APPLE}>,1,0>)
