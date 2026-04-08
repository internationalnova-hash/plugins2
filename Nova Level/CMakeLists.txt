# Nova Level CMake configuration

juce_add_plugin(NovaLevel
    COMPANY_NAME "International Nova"
    PRODUCT_NAME "Nova Level"
    FORMATS VST3
    NEEDS_WEB_BROWSER TRUE
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(NovaLevel PUBLIC JUCE_WEB_BROWSER=1)
target_link_libraries(NovaLevel PRIVATE juce::juce_dsp juce::juce_gui_extra)
