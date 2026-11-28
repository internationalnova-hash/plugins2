# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaRack VERSION 0.1.0)

# Customer builds require a Nova license key (unlock screen until activated).
# Local/dev builds leave it OFF so the rack opens straight away for testing.
option(NOVA_RACK_REQUIRE_LICENSE "Require NovaLicense activation (customer builds)" OFF)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX is built when the shared AAX SDK is present (it is not committed to git).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

if(WIN32)
    set(PLUGIN_FORMATS VST3 ${NOVA_AAX_FORMAT} Standalone)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone)
else()
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
endif()

# Faceplate artwork taken from each plugin's own UI (Source/ui/public/skins).
file(GLOB NOVA_RACK_SKINS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/public/skins/*")

juce_add_binary_data(NovaRack_WebUI
    SOURCES
        ${NOVA_RACK_SKINS}
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/match.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/n_logo.png
    NAMESPACE nova_rack_BinaryData
)

juce_add_plugin(NovaRack
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    PRODUCT_NAME "Nova Rack"
    BUNDLE_ID "com.internationalnova.novarack"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvRk
    FORMATS ${PLUGIN_FORMATS}
    LV2URI "https://getnovaaudio.com/plugins/nova-rack"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 FALSE
    VST3_CAN_REPLACE_VST2 FALSE
    VST3_CATEGORIES Fx
    AAX_CATEGORY None
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE   # preset name + search fields need key input
)

set(NOVA_LICENSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense)

target_sources(NovaRack
    PRIVATE
        Source/CreatePluginFilter.cpp
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginLibrary.cpp
        Source/PluginLibrary.h
        Source/RackPresets.cpp
        Source/RackPresets.h
        Source/FactoryRacks.cpp
        Source/RackSlot.cpp
        Source/RackSlot.h
        Source/Match/VocalProfile.cpp
        Source/Match/VocalProfile.h
        Source/Match/MatchEngine.cpp
        Source/Match/MatchEngine.h
        Source/Match/MatchSession.cpp
        Source/Match/MatchSession.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTuneCore/NovaVocalTuner.cpp
)

target_include_directories(NovaRack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTuneCore)

if(NOVA_RACK_REQUIRE_LICENSE)
    target_sources(NovaRack PRIVATE ${NOVA_LICENSE_DIR}/NovaLicense.cpp ${NOVA_LICENSE_DIR}/NovaLicense.h)
    target_include_directories(NovaRack PRIVATE ${NOVA_LICENSE_DIR})
    target_compile_definitions(NovaRack PUBLIC
        NOVA_RACK_REQUIRE_LICENSE=1
        NOVA_LICENSE_SERVER_URL="https://nova-store-olive.vercel.app")
    target_link_libraries(NovaRack PRIVATE juce::juce_cryptography juce::juce_product_unlocking)
else()
    target_compile_definitions(NovaRack PUBLIC NOVA_RACK_REQUIRE_LICENSE=0)
endif()

target_include_directories(NovaRack PRIVATE Source)

target_link_libraries(NovaRack
    PRIVATE
        NovaRack_WebUI
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

target_compile_definitions(NovaRack
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        # The rack hosts Nova plugins as VST3 on every platform, so rack presets
        # move between macOS and Windows sessions unchanged.
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=0
)

if(WIN32)
    target_compile_definitions(NovaRack PUBLIC
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        _USE_MATH_DEFINES=1)
elseif(APPLE)
    target_compile_definitions(NovaRack PUBLIC JUCE_USE_CURL=0)
else()
    target_compile_definitions(NovaRack PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()

if(TARGET NovaRack_VST3)
    target_compile_definitions(NovaRack_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# ── Rack tests: preset round-trips and the latency-compensated slot mixer ──
#   cmake --build "Nova Rack/build" --target NovaRackTests && "Nova Rack/build/NovaRackTests_artefacts/Release/NovaRackTests"
juce_add_console_app(NovaRackTests PRODUCT_NAME "NovaRackTests")
target_sources(NovaRackTests PRIVATE Tests/RackTests.cpp Source/RackPresets.cpp Source/FactoryRacks.cpp Source/RackSlot.cpp)
target_include_directories(NovaRackTests PRIVATE Source)
target_compile_definitions(NovaRackTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0)
target_link_libraries(NovaRackTests
    PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_audio_processors juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ── Dev tool: dump installed Nova plugins' parameters (used to author factory racks) ──
juce_add_console_app(NovaRackParamDump PRODUCT_NAME "NovaRackParamDump")
target_sources(NovaRackParamDump PRIVATE Tools/ParamDump.cpp Source/RackPresets.cpp Source/FactoryRacks.cpp Source/RackSlot.cpp)
target_include_directories(NovaRackParamDump PRIVATE Source)
target_compile_definitions(NovaRackParamDump PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0)
target_link_libraries(NovaRackParamDump
    PRIVATE juce::juce_core juce::juce_audio_processors juce::juce_gui_basics juce::juce_events
    PUBLIC juce::juce_recommended_config_flags)

# ── Nova Match tests: analyse real vocals and rebuild a known chain with the installed plugins ──
#   cmake --build "Nova Rack/build-release" --target NovaMatchTests && "Nova Rack/build-release/NovaMatchTests_artefacts/Release/NovaMatchTests"
set(NOVA_TUNE_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaTuneCore)
juce_add_console_app(NovaMatchTests PRODUCT_NAME "NovaMatchTests")
target_sources(NovaMatchTests PRIVATE Tests/MatchTests.cpp Source/Match/VocalProfile.cpp Source/Match/MatchEngine.cpp Source/Match/MatchSession.cpp Source/PluginLibrary.cpp
    Source/RackPresets.cpp Source/FactoryRacks.cpp Source/RackSlot.cpp ${NOVA_TUNE_CORE_DIR}/NovaVocalTuner.cpp)
target_include_directories(NovaMatchTests PRIVATE Source ${NOVA_TUNE_CORE_DIR})
target_compile_definitions(NovaMatchTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(NovaMatchTests
    PRIVATE juce::juce_core juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics juce::juce_events
    PUBLIC juce::juce_recommended_config_flags)

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaRack)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaRack)
