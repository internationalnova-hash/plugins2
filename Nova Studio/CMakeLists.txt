set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaStudio VERSION 0.1.0)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_WEB_BROWSER OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)

find_package(CURL REQUIRED)

juce_add_gui_app(NovaStudio
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novastudio"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/StudioAudioEngine.cpp
    Source/TransportState.cpp
    Source/TimelineModel.cpp
    Source/Session.cpp
    Source/ArrangementModel.cpp
    Source/NovaAlignProcessor.cpp
    Source/ui/StudioComponents.cpp
    Source/ui/WaveformCache.cpp
    Source/ui/Theme.cpp
    Source/ui/EditWindow.cpp
    Source/ui/MixerWindow.cpp
    Source/ui/BeatWindow.cpp
)

target_link_libraries(NovaStudio PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    ${GTK3_LIBRARIES}
    ${WEBKIT_LIBRARIES}
    CURL::libcurl
    asound
)

target_include_directories(NovaStudio PRIVATE ${GTK3_INCLUDE_DIRS})
target_include_directories(NovaStudio PRIVATE ${WEBKIT_INCLUDE_DIRS})
target_compile_options(NovaStudio PRIVATE ${GTK3_CFLAGS_OTHER} ${WEBKIT_CFLAGS_OTHER})

juce_generate_juce_header(NovaStudio)

target_compile_definitions(NovaStudio PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ─── Validation test executable ───────────────────────────────────────────────
juce_add_console_app(NovaStudioValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novastudio.validation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaStudioValidation PRIVATE
    Source/ValidationTest.cpp
    Source/StudioAudioEngine.cpp
    Source/TransportState.cpp
    Source/TimelineModel.cpp
    Source/Session.cpp
    Source/ArrangementModel.cpp
    Source/NovaAlignProcessor.cpp
)

target_link_libraries(NovaStudioValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    ${GTK3_LIBRARIES}
    ${WEBKIT_LIBRARIES}
    CURL::libcurl
    asound
)

target_include_directories(NovaStudioValidation PRIVATE ${GTK3_INCLUDE_DIRS})
target_include_directories(NovaStudioValidation PRIVATE ${WEBKIT_INCLUDE_DIRS})
target_compile_options(NovaStudioValidation PRIVATE ${GTK3_CFLAGS_OTHER} ${WEBKIT_CFLAGS_OTHER})

juce_generate_juce_header(NovaStudioValidation)

target_compile_definitions(NovaStudioValidation PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
)
