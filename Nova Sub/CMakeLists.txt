# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
# Ensure JUCE does not search for VST2 SDK
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaSub VERSION 1.0.0)

# JUCE setup using FetchContent
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
if(APPLE OR WIN32)
    juce_set_aax_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
endif()

# Nova Sub
# 808/bass saturation enhancer with a WebView interface

set(PLUGIN_TARGET NovaSub)
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

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
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/NovaSub")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Nova Sub: Building for ${PLUGIN_FORMATS}")
message(STATUS "Nova Sub: WebView backend: ${WEBVIEW_BACKEND}")

juce_add_binary_data(NovaSub_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/n_logo.png
    NAMESPACE nova_sub_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novasub"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvSb
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Sub"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Distortion
    AAX_CATEGORY Harmonic
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

set(NOVA_LICENSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ParameterIDs.hpp
        ${NOVA_LICENSE_DIR}/NovaLicense.cpp
        ${NOVA_LICENSE_DIR}/NovaLicense.h
)

target_include_directories(${PLUGIN_TARGET} PRIVATE ${NOVA_LICENSE_DIR})

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        NovaSub_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_product_unlocking
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(${PLUGIN_TARGET}
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        NOVA_LICENSE_SERVER_URL="https://nova-store-olive.vercel.app"
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
endif()

# ── Offline diagnostic harness ──────────────────────────────────────────────
juce_add_console_app(NovaSubOfflineDiag
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novasub.offlinediag"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaSubOfflineDiag PRIVATE
    Source/OfflineDiag.cpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    ${NOVA_LICENSE_DIR}/NovaLicense.cpp
    ${NOVA_LICENSE_DIR}/NovaLicense.h
)

target_include_directories(NovaSubOfflineDiag PRIVATE ${NOVA_LICENSE_DIR})

target_link_libraries(NovaSubOfflineDiag PRIVATE
    NovaSub_WebUI
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_cryptography
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_product_unlocking
    juce::juce_recommended_config_flags
)

juce_generate_juce_header(NovaSubOfflineDiag)

target_compile_definitions(NovaSubOfflineDiag PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
    NOVA_LICENSE_SERVER_URL="https://nova-store-olive.vercel.app"
    JucePlugin_Name="NovaSubOfflineDiag"
)

if(APPLE)
    target_compile_definitions(NovaSubOfflineDiag PRIVATE JUCE_USE_CURL=0)
endif()
