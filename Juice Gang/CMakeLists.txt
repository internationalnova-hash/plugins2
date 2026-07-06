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

project(JuiceGang VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

set(PLUGIN_TARGET JuiceGang)

if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(${PLUGIN_TARGET}
    COMPANY_NAME                "Nova Audio"
    PLUGIN_MANUFACTURER_CODE    "NvAu"
    PLUGIN_CODE                 "JcGg"
    FORMATS                     ${PLUGIN_FORMATS}
    PRODUCT_NAME                "Juice Gang"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             "Fx" "Filter"
)

juce_add_binary_data(JuiceGangBinaryData
    SOURCES
        Source/JuiceTag.m4a
)

target_sources(${PLUGIN_TARGET}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(${PLUGIN_TARGET} PRIVATE Source)

target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        JuiceGangBinaryData
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
