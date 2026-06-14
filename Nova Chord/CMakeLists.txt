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

project(NovaChord VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

set(PLUGIN_TARGET NovaChord)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
    set(WEBVIEW_BACKEND "WebView2")
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
    set(WEBVIEW_BACKEND "WKWebView")
    set(AU_MAIN_TYPE kAudioUnitType_MIDIProcessor)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(WEBVIEW_BACKEND "WebKitGTK")
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/NovaChord")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

juce_add_binary_data(NovaChord_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/n_logo.png
        Source/ui/public/js/index.js
        Source/ui/public/js/fallback-boot.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_chord_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://noizefield.com"
    BUNDLE_ID "com.internationalnova.novachord"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvCh
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Chord"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx MIDI
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaChord_WebUI
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

target_compile_definitions(${PLUGIN_TARGET}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
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
