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

project(NovaPitch VERSION 1.0.0)

include(FetchContent)
include(ExternalProject)

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# ---------------------------------------------------------------------------
# RubberBand — build from source as an ExternalProject so CMake passes
# the correct CMAKE_OSX_ARCHITECTURES through, producing a proper fat binary.
# ---------------------------------------------------------------------------
set(RUBBERBAND_INSTALL_DIR "${CMAKE_BINARY_DIR}/rubberband-install")

ExternalProject_Add(rubberband_ext
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        v3.3.0
    GIT_SHALLOW    TRUE
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        -DCMAKE_INSTALL_PREFIX=${RUBBERBAND_INSTALL_DIR}
        -DUSE_BUILTIN_FFT=ON
    BUILD_BYPRODUCTS "${RUBBERBAND_INSTALL_DIR}/lib/librubberband.a"
)

# Pre-create the install directories so CMake doesn't reject the imported
# target at configure time (ExternalProject builds happen at build time).
file(MAKE_DIRECTORY "${RUBBERBAND_INSTALL_DIR}/include")
file(MAKE_DIRECTORY "${RUBBERBAND_INSTALL_DIR}/lib")

# Import the built library as a target
add_library(rubberband_imported STATIC IMPORTED GLOBAL)
add_dependencies(rubberband_imported rubberband_ext)
set_target_properties(rubberband_imported PROPERTIES
    IMPORTED_LOCATION "${RUBBERBAND_INSTALL_DIR}/lib/librubberband.a"
    INTERFACE_INCLUDE_DIRECTORIES "${RUBBERBAND_INSTALL_DIR}/include"
)

set(PLUGIN_TARGET NovaPitch)
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
        Source/ui/public/n_logo.png
        Source/ui/public/js/index.js
        Source/ui/public/js/fallback-boot.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_pitch_BinaryData
)

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://noizefield.com"
    BUNDLE_ID "com.internationalnova.novapitch"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvPt
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Pitch"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Pitch
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
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
        NovaPitch_WebUI
        rubberband_imported
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

if(APPLE)
    target_link_libraries(${PLUGIN_TARGET} PRIVATE "-framework Accelerate")
endif()

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
