cmake_minimum_required(VERSION 3.15)
project(NovaLevel VERSION 1.0.0)

# JUCE setup using FetchContent
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        develop # Use the latest JUCE develop branch for macOS 15 compatibility
)
FetchContent_MakeAvailable(juce)




# Only enable VST3 and AU, disable all other plugin formats
set(JUCE_BUILD_VST2 OFF CACHE BOOL "Disable VST2 format")
set(JUCE_BUILD_AU ON CACHE BOOL "Enable AU format")
set(JUCE_BUILD_AUv3 OFF CACHE BOOL "Disable AUv3 format")
set(JUCE_BUILD_LV2 OFF CACHE BOOL "Disable LV2 format")
set(JUCE_BUILD_AAX OFF CACHE BOOL "Disable AAX format")
set(JUCE_BUILD_STANDALONE OFF CACHE BOOL "Disable Standalone format")
set(JUCE_BUILD_UNITY OFF CACHE BOOL "Disable Unity format")
set(JUCE_BUILD_ARA OFF CACHE BOOL "Disable ARA format")



juce_add_plugin(NovaLevel
    COMPANY_NAME "International Nova"
    PRODUCT_NAME "Nova Level"
    BUNDLE_ID "com.internationalnova.NovaLevel"
    PLUGIN_MANUFACTURER_CODE "INVA"
    PLUGIN_CODE "NLVL"
    FORMATS VST3 AU
    NEEDS_WEB_BROWSER TRUE
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

set_target_properties(NovaLevel PROPERTIES
    JUCE_PLUGIN_FORMATS "VST3;AU"
)

target_compile_definitions(NovaLevel PUBLIC JUCE_WEB_BROWSER=1)
target_link_libraries(NovaLevel PRIVATE juce::juce_dsp juce::juce_gui_extra)
