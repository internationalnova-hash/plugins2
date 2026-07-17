cmake_minimum_required(VERSION 3.15)
project(NovaDSPRegressionRunner C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────────────────
# Use FetchContent.  If a local copy exists in a sibling plugin's build cache,
# redirect FetchContent to it via FETCHCONTENT_SOURCE_DIR_JUCE so no network
# access is required.  FetchContent handles the CMake compiler detection
# correctly in both cases.

include(FetchContent)

set(JUCE_LOCAL_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/../../Juice Gang/build/_deps/juce-src")

if(EXISTS "${JUCE_LOCAL_CACHE}/CMakeLists.txt")
    message(STATUS "NovaDSP runner: redirecting FetchContent to cached JUCE at ${JUCE_LOCAL_CACHE}")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${JUCE_LOCAL_CACHE}" CACHE PATH "" FORCE)
endif()

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# ── Engine source files ───────────────────────────────────────────────────
set(ENGINE_SOURCES
    ../Dynamics/NovaLevelDSP.cpp
    ../Modulation/NovaMotionDSP.cpp
    ../Spatial/NovaSpaceDSP.cpp
    ../Console/NovaConsoleDSP.cpp
)

# ── Runner executable ─────────────────────────────────────────────────────
add_executable(NovaDSPRegressionRunner
    NovaDSPRegressionRunner.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(NovaDSPRegressionRunner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"         # NovaDSPTestResult.h
    "${CMAKE_CURRENT_SOURCE_DIR}/.."      # NovaDSP/ root
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dynamics"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Modulation"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spatial"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Console"
)

target_compile_definitions(NovaDSPRegressionRunner PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    $<$<PLATFORM_ID:Windows>:_USE_MATH_DEFINES=1>
)

target_link_libraries(NovaDSPRegressionRunner PRIVATE
    juce_audio_basics
    juce_audio_processors
    juce_dsp
    juce_core
    juce_events
    juce_data_structures
    juce_graphics
    juce_gui_basics
    juce_recommended_config_flags
    juce_recommended_warning_flags
)

# Suppress JUCE's own warnings in the runner (they're third-party)
if(MSVC)
    target_compile_options(NovaDSPRegressionRunner PRIVATE /W0)
else()
    target_compile_options(NovaDSPRegressionRunner PRIVATE -w)
endif()
