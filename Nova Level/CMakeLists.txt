cmake_minimum_required(VERSION 3.15)
project(NovaLevel VERSION 1.0.0)

# JUCE setup using FetchContent
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9 # Use the latest stable JUCE release for compatibility
)
FetchContent_MakeAvailable(juce)







juce_add_plugin(NovaLevel
    COMPANY_NAME "International Nova"
    PRODUCT_NAME "Nova Level"
    BUNDLE_ID "com.internationalnova.NovaLevel"
    PLUGIN_MANUFACTURER_CODE "INVA"
    PLUGIN_CODE "NLVL"
    FORMATS VST3
    NEEDS_WEB_BROWSER TRUE
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

set_target_properties(NovaLevel PROPERTIES
    JUCE_PLUGIN_FORMATS "VST3"
)

target_compile_definitions(NovaLevel PUBLIC JUCE_WEB_BROWSER=1)
target_link_libraries(NovaLevel PRIVATE juce::juce_dsp juce::juce_gui_extra)
