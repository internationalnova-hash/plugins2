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

juce_add_plugin(NovaLevel
    COMPANY_NAME "International Nova"
    PRODUCT_NAME "Nova Level"
    FORMATS VST3 # Only build VST3 to avoid VST2/VST3 automation conflict
    NEEDS_WEB_BROWSER TRUE
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(NovaLevel PUBLIC JUCE_WEB_BROWSER=1)
target_link_libraries(NovaLevel PRIVATE juce::juce_dsp juce::juce_gui_extra)
