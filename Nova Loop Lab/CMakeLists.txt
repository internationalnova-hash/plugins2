# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaLoopLab VERSION 1.0.0)

# Customer builds require a Nova license key (unlock screen until activated).
# Local/dev builds leave it OFF so the plugin opens straight away for testing.
#   cmake -S "Nova Loop Lab" -B "Nova Loop Lab/build" -DNOVA_LOOPLAB_REQUIRE_LICENSE=ON
option(NOVA_LOOPLAB_REQUIRE_LICENSE "Require NovaLicense activation (customer builds)" OFF)

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

juce_add_binary_data(NovaLoopLab_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/n_logo.png
    NAMESPACE nova_looplab_BinaryData
)

juce_add_plugin(NovaLoopLab
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    PRODUCT_NAME "Nova Loop Lab"
    BUNDLE_ID "com.internationalnova.novalooplab"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvLL
    FORMATS ${PLUGIN_FORMATS}
    LV2URI "https://getnovaaudio.com/plugins/nova-loop-lab"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 FALSE
    VST3_CAN_REPLACE_VST2 FALSE
    VST3_CATEGORIES Instrument Sampler
    AAX_CATEGORY SWGenerators
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE   # license key field
)

set(NOVA_LICENSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense)

target_sources(NovaLoopLab
    PRIVATE
        Source/CreatePluginFilter.cpp
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/LoopLabEngine.cpp
        Source/LoopLabEngine.h
        Source/LoopLabCore.cpp
        Source/LoopLabCore.h
)

if(NOVA_LOOPLAB_REQUIRE_LICENSE)
    target_sources(NovaLoopLab PRIVATE ${NOVA_LICENSE_DIR}/NovaLicense.cpp ${NOVA_LICENSE_DIR}/NovaLicense.h)
    target_include_directories(NovaLoopLab PRIVATE ${NOVA_LICENSE_DIR})
    target_compile_definitions(NovaLoopLab PUBLIC
        NOVA_LOOPLAB_REQUIRE_LICENSE=1
        NOVA_LICENSE_SERVER_URL="https://nova-store-olive.vercel.app")
    target_link_libraries(NovaLoopLab PRIVATE juce::juce_cryptography juce::juce_product_unlocking)
else()
    target_compile_definitions(NovaLoopLab PUBLIC NOVA_LOOPLAB_REQUIRE_LICENSE=0)
endif()

target_include_directories(NovaLoopLab PRIVATE Source)

target_link_libraries(NovaLoopLab
    PRIVATE
        NovaLoopLab_WebUI
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

target_compile_definitions(NovaLoopLab
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(NovaLoopLab PUBLIC
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        _USE_MATH_DEFINES=1)
elseif(APPLE)
    target_compile_definitions(NovaLoopLab PUBLIC JUCE_USE_CURL=0)
else()
    target_compile_definitions(NovaLoopLab PUBLIC JUCE_USE_CURL=0 JUCE_JACK=1)
endif()

if(TARGET NovaLoopLab_VST3)
    target_compile_definitions(NovaLoopLab_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# ── Engine tests ─────────────────────────────────────────────────────────────
#   cmake --build "Nova Loop Lab/build" --target NovaLoopLabTests && "Nova Loop Lab/build/NovaLoopLabTests_artefacts/Release/NovaLoopLabTests"
juce_add_console_app(NovaLoopLabTests PRODUCT_NAME "NovaLoopLabTests")
target_sources(NovaLoopLabTests PRIVATE Tests/LoopLabTests.cpp Source/LoopLabEngine.cpp)
target_include_directories(NovaLoopLabTests PRIVATE Source)
target_compile_definitions(NovaLoopLabTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(NovaLoopLabTests
    PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ── Plugin harness: the real processor driven like a DAW (no UI) ─────────
#   cmake --build "Nova Loop Lab/build" --target NovaLoopLabHarness && "Nova Loop Lab/build/NovaLoopLabHarness"
add_executable(NovaLoopLabHarness EXCLUDE_FROM_ALL Tests/PluginHarness.cpp)
target_include_directories(NovaLoopLabHarness PRIVATE Source $<TARGET_PROPERTY:NovaLoopLab,INCLUDE_DIRECTORIES>)
target_compile_definitions(NovaLoopLabHarness PRIVATE $<TARGET_PROPERTY:NovaLoopLab,COMPILE_DEFINITIONS>)
target_compile_features(NovaLoopLabHarness PRIVATE cxx_std_17)
target_link_libraries(NovaLoopLabHarness PRIVATE NovaLoopLab)
set_target_properties(NovaLoopLabHarness PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaLoopLab)
