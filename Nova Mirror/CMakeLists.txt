set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaMirror VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NEEDS_WEBVIEW2 FALSE)
set(AU_MAIN_TYPE "")
if(WIN32)
    set(PLUGIN_FORMATS VST3 ${NOVA_AAX_FORMAT} Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone)
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(NovaMirror_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_mirror_BinaryData
)

juce_add_plugin(NovaMirror
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novamirror"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvMr
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Mirror"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    VST3_CATEGORIES Fx EQ
    AAX_CATEGORY EQ
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaMirror PRIVATE
    Source/MirrorCore.cpp Source/MirrorCore.h
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
)
target_include_directories(NovaMirror PRIVATE Source)

target_link_libraries(NovaMirror
    PRIVATE
        NovaMirror_WebUI
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

target_compile_definitions(NovaMirror PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
if(WIN32)
    target_compile_definitions(NovaMirror PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 _USE_MATH_DEFINES=1 JUCE_USE_WINDOWS_MEDIA_FORMAT=1)
endif()

# ── Engine tests ────────────────────────────────────────────────────────────
juce_add_console_app(NovaMirrorTests PRODUCT_NAME "NovaMirrorTests")
target_sources(NovaMirrorTests PRIVATE Tests/MirrorTests.cpp Source/MirrorCore.cpp)
target_include_directories(NovaMirrorTests PRIVATE Source)
target_link_libraries(NovaMirrorTests PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
target_compile_definitions(NovaMirrorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

# ── Nova license activation ─────────────────────────────────────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaMirror)

# ── Processor test ──────────────────────────────────────────────────────────
juce_add_console_app(NovaMirrorProcessorTests PRODUCT_NAME "NovaMirrorProcessorTests")
target_sources(NovaMirrorProcessorTests PRIVATE
    Tests/ProcessorTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/MirrorCore.cpp)
target_include_directories(NovaMirrorProcessorTests PRIVATE Source)
target_link_libraries(NovaMirrorProcessorTests PRIVATE
    NovaMirror_WebUI
    juce::juce_audio_basics juce::juce_audio_formats juce::juce_audio_processors juce::juce_core
    juce::juce_data_structures juce::juce_dsp juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaMirrorProcessorTests PRIVATE
    JucePlugin_Name="NovaMirrorProcessorTests" JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
nova_license_test_target(NovaMirrorProcessorTests)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaMirror)
