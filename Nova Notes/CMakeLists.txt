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

project(NovaNotes VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX when the shared SDK is present (local builds)
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NEEDS_WEBVIEW2 FALSE)
set(AU_MAIN_TYPE "")
if(WIN32)
    set(PLUGIN_FORMATS VST3 ${NOVA_AAX_FORMAT} Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone)
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(NovaNotes_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_notes_BinaryData
)

juce_add_plugin(NovaNotes
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novanotes"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvNt
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Notes"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    VST3_CATEGORIES Fx Pitch
    AAX_CATEGORY PitchShift
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

target_sources(NovaNotes PRIVATE
    Source/NotesCore.cpp
    Source/NotesCore.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)
target_include_directories(NovaNotes PRIVATE Source)

target_link_libraries(NovaNotes
    PRIVATE
        NovaNotes_WebUI
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

target_compile_definitions(NovaNotes PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)
if(WIN32)
    target_compile_definitions(NovaNotes PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 _USE_MATH_DEFINES=1)
endif()

# ── Engine tests (no plugin code) ───────────────────────────────────────────
juce_add_console_app(NovaNotesTests PRODUCT_NAME "NovaNotesTests")
target_sources(NovaNotesTests PRIVATE Tests/NotesTests.cpp Source/NotesCore.cpp)
target_include_directories(NovaNotesTests PRIVATE Source)
target_link_libraries(NovaNotesTests PRIVATE
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp juce::juce_events juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags)
target_compile_definitions(NovaNotesTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_FLAC=1)

# ── Nova license activation ─────────────────────────────────────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaNotes)

# ── Processor test: capture -> analysis -> AI Assist -> playback -> session reload ──
juce_add_console_app(NovaNotesProcessorTests PRODUCT_NAME "NovaNotesProcessorTests")
target_sources(NovaNotesProcessorTests PRIVATE
    Tests/ProcessorTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/NotesCore.cpp)
target_include_directories(NovaNotesProcessorTests PRIVATE Source)
target_link_libraries(NovaNotesProcessorTests PRIVATE
    NovaNotes_WebUI
    juce::juce_audio_basics juce::juce_audio_formats juce::juce_audio_processors juce::juce_core
    juce::juce_data_structures juce::juce_dsp juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaNotesProcessorTests PRIVATE
    JucePlugin_Name="NovaNotesProcessorTests" JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_USE_FLAC=1 JUCE_MODAL_LOOPS_PERMITTED=1)
nova_license_test_target(NovaNotesProcessorTests)

juce_add_console_app(NovaNotesDump PRODUCT_NAME "NovaNotesDump")
target_sources(NovaNotesDump PRIVATE Tests/Dump.cpp Source/NotesCore.cpp)
target_include_directories(NovaNotesDump PRIVATE Source)
target_link_libraries(NovaNotesDump PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(NovaNotesDump PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaNotes)
