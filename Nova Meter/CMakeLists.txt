# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaMeter VERSION 0.1.0)

# Customer builds require a Nova license key (unlock screen until activated).
# Local/dev builds leave it OFF so the plugin opens straight away for testing.
#   cmake -S "Nova Meter" -B "Nova Meter/build" -DNOVA_METER_REQUIRE_LICENSE=ON
option(NOVA_METER_REQUIRE_LICENSE "Require NovaLicense activation (customer builds)" OFF)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX is built when the shared AAX SDK is present (it is not committed to git,
# so CI checkouts build VST3/AU/Standalone only).
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

juce_add_binary_data(NovaMeter_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/n_logo.png
    NAMESPACE nova_meter_BinaryData
)

juce_add_plugin(NovaMeter
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    PRODUCT_NAME "Nova Meter"
    BUNDLE_ID "com.internationalnova.novameter"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvMt
    FORMATS ${PLUGIN_FORMATS}
    LV2URI "https://getnovaaudio.com/plugins/nova-meter"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 FALSE
    VST3_CAN_REPLACE_VST2 FALSE
    VST3_CATEGORIES Fx Analyzer
    AAX_CATEGORY None
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE   # Save-As name field needs key input
)

set(NOVA_LICENSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense)

target_sources(NovaMeter
    PRIVATE
        Source/CreatePluginFilter.cpp
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/MeterEngine.cpp
        Source/MeterEngine.h
        Source/Profiles.cpp
        Source/Profiles.h
        Source/ReferenceAnalyzer.cpp
        Source/ReferenceAnalyzer.h
)

if(NOVA_METER_REQUIRE_LICENSE)
    target_sources(NovaMeter PRIVATE ${NOVA_LICENSE_DIR}/NovaLicense.cpp ${NOVA_LICENSE_DIR}/NovaLicense.h)
    target_include_directories(NovaMeter PRIVATE ${NOVA_LICENSE_DIR})
    target_compile_definitions(NovaMeter PUBLIC
        NOVA_METER_REQUIRE_LICENSE=1
        NOVA_LICENSE_SERVER_URL="https://nova-store-olive.vercel.app")
    target_link_libraries(NovaMeter PRIVATE juce::juce_cryptography juce::juce_product_unlocking)
else()
    target_compile_definitions(NovaMeter PUBLIC NOVA_METER_REQUIRE_LICENSE=0)
endif()

target_include_directories(NovaMeter PRIVATE Source)

target_link_libraries(NovaMeter
    PRIVATE
        NovaMeter_WebUI
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

target_compile_definitions(NovaMeter
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_MP3AUDIOFORMAT=1   # reference tracks are often MP3s
)

if(WIN32)
    target_compile_definitions(NovaMeter PUBLIC
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        _USE_MATH_DEFINES=1)
elseif(APPLE)
    target_compile_definitions(NovaMeter PUBLIC JUCE_USE_CURL=0)
else()
    target_compile_definitions(NovaMeter PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()

if(TARGET NovaMeter_VST3)
    target_compile_definitions(NovaMeter_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# ── Measurement accuracy tests (EBU Tech 3341 / BS.1770 signals) ──────────
#   cmake --build "Nova Meter/build" --target NovaMeterTests && "Nova Meter/build/NovaMeterTests_artefacts/Release/NovaMeterTests"
juce_add_console_app(NovaMeterTests PRODUCT_NAME "NovaMeterTests")
target_sources(NovaMeterTests PRIVATE Tests/MeterTests.cpp Source/MeterEngine.cpp Source/ReferenceAnalyzer.cpp Source/Profiles.cpp)
target_include_directories(NovaMeterTests PRIVATE Source)
target_compile_definitions(NovaMeterTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(NovaMeterTests
    PRIVATE juce::juce_core juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaMeter)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaMeter)
