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

project(NovaMelody VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# Pitch tracking is shared with Nova Notes (one tracker for both)
set(NOVA_NOTES_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../Nova\ Notes/Source)
set(MELODY_CORE Source/MelodyCore.cpp Source/MelodyCore.h "${NOVA_NOTES_SRC}/NotesCore.cpp")

# ── Core tests ──────────────────────────────────────────────────────────────
juce_add_console_app(NovaMelodyTests PRODUCT_NAME "NovaMelodyTests")
target_sources(NovaMelodyTests PRIVATE Tests/MelodyTests.cpp ${MELODY_CORE})
target_include_directories(NovaMelodyTests PRIVATE Source "${NOVA_NOTES_SRC}")
target_link_libraries(NovaMelodyTests PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
target_compile_definitions(NovaMelodyTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp)
    include(${CMAKE_CURRENT_SOURCE_DIR}/plugin.cmake)
endif()

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaMelody)
