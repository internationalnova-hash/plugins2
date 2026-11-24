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

project(NovaVox VERSION 1.0.0)

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

set(PLUGIN_TARGET NovaVox)

if(WIN32)
    set(PLUGIN_FORMATS VST3 ${NOVA_AAX_FORMAT} Standalone)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME                "International Nova"
    COMPANY_WEBSITE             "https://getnovaaudio.com"
    BUNDLE_ID                   "com.internationalnova.novavox"
    PLUGIN_MANUFACTURER_CODE    "Inov"
    PLUGIN_CODE                 "NvVx"
    FORMATS                     ${PLUGIN_FORMATS}
    PRODUCT_NAME                "Nova Vox"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Vocal
    AAX_CATEGORY                Dynamics
)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/Visualizer.cpp
        Source/ui/ModuleCard.cpp
        Source/ui/AdvancedPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Console/NovaConsoleDSP.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Dynamics/NovaLevelDSP.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Modulation/NovaMotionDSP.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Spatial/NovaSpaceDSP.cpp
)

target_include_directories(${PLUGIN_TARGET}
    PRIVATE
        Source
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Console
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Dynamics
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Modulation
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Spatial
)

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
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

target_compile_definitions(${PLUGIN_TARGET}
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

if(WIN32)
    target_compile_definitions(${PLUGIN_TARGET} PUBLIC _USE_MATH_DEFINES=1)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaVox)
