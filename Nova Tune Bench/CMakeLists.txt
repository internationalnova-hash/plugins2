cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaTuneBench VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# Nova Tune Bench
# Offline test bench for pitch correction. Hosts any VST3/AU tuner, renders
# test vocals through it and scores the result with an independent pitch
# tracker: note accuracy, warble, wrong notes, snap speed, clicks, dropouts,
# roughness, latency. See README.md.

juce_add_console_app(NovaTuneBench
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novatunebench"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaTuneBench PRIVATE
    Source/Main.cpp
    Source/Analysis.cpp
    Source/Synth.cpp
)

target_link_libraries(NovaTuneBench PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_core
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
)

if(APPLE)
    # AU hosting pulls in AUGenericView
    target_link_libraries(NovaTuneBench PRIVATE "-framework CoreAudioKit")
endif()

target_compile_definitions(NovaTuneBench PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)
