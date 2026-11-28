# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.24)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(NovaLink VERSION 1.0.0 LANGUAGES C CXX)

option(NOVA_LINK_REQUIRE_LICENSE "Require NovaLicense activation (customer builds)" OFF)
# Matchmaking server (exchanges connection details by session code).
set(NOVA_LINK_SIGNAL_URL "https://nova-store-olive.vercel.app" CACHE STRING "Nova Link matchmaking server")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Build every dependency into the plugin: no separate .dylib files to ship.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON)   # nothing here is "installed"; skips dependency export checks
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)   # older dependency CMake files
include(FetchContent)

FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8)
FetchContent_MakeAvailable(juce)

# ── Networking: Mbed TLS (DTLS) + libdatachannel (WebRTC data channels, ICE/STUN) ──
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mbedtls GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git GIT_TAG v3.6.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(mbedtls)
# libdatachannel's DTLS layer needs DTLS-SRTP support compiled into Mbed TLS.
foreach(t mbedtls mbedx509 mbedcrypto)
    target_compile_definitions(${t} PUBLIC MBEDTLS_SSL_DTLS_SRTP)
endforeach()
if(NOT TARGET MbedTLS::MbedTLS)
    add_library(NovaMbedTLS INTERFACE)
    target_link_libraries(NovaMbedTLS INTERFACE mbedtls mbedx509 mbedcrypto)
    add_library(MbedTLS::MbedTLS ALIAS NovaMbedTLS)
endif()

set(USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
FetchContent_Declare(libdatachannel GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git GIT_TAG v0.22.5
                     GIT_SHALLOW TRUE GIT_SUBMODULES "deps/plog" "deps/usrsctp" "deps/libjuice" "deps/json")
FetchContent_MakeAvailable(libdatachannel)

# ── Opus (live audio codec) ──
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(opus GIT_REPOSITORY https://github.com/xiph/opus.git GIT_TAG v1.5.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(opus)

# Core sources (networking + session logic), compiled into the plugin and the tests.
set(NOVA_LINK_CORE_SOURCES
    Source/LinkTransport.cpp
    Source/Signaling.cpp
    Source/LinkAudio.cpp
    Source/LinkSession.cpp
)

# AAX when the shared SDK is present
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
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(NovaLink_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/n_logo.png
    NAMESPACE nova_link_BinaryData
)

juce_add_plugin(NovaLink
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    PRODUCT_NAME "Nova Link"
    BUNDLE_ID "com.internationalnova.novalink"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvLk
    FORMATS ${PLUGIN_FORMATS}
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 FALSE
    VST3_CAN_REPLACE_VST2 FALSE
    VST3_CATEGORIES Fx Tools
    AAX_CATEGORY Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    CAMERA_PERMISSION_ENABLED TRUE
    CAMERA_PERMISSION_TEXT "Nova Link uses the camera for video during remote sessions."
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Nova Link's standalone app records and streams your microphone."
)

target_sources(NovaLink PRIVATE
    Source/CreatePluginFilter.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LinkVideo.cpp
    ${NOVA_LINK_CORE_SOURCES}
)
if(APPLE)
    target_sources(NovaLink PRIVATE Source/LinkVideoMac.mm)
endif()
target_include_directories(NovaLink PRIVATE Source)
target_compile_definitions(NovaLink PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_CAMERA=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    NOVA_LINK_SIGNAL_URL="${NOVA_LINK_SIGNAL_URL}")
target_link_libraries(NovaLink
    PRIVATE
        NovaLink_WebUI
        datachannel-static
        opus
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
        juce::juce_video
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_add_console_app(NovaLinkTests PRODUCT_NAME "NovaLinkTests")
target_sources(NovaLinkTests PRIVATE Tests/LinkTests.cpp ${NOVA_LINK_CORE_SOURCES})
target_include_directories(NovaLinkTests PRIVATE Source)
target_compile_definitions(NovaLinkTests PRIVATE NOVA_LINK_SIGNAL_URL="${NOVA_LINK_SIGNAL_URL}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(NovaLinkTests PRIVATE datachannel-static opus
    juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_events
    PUBLIC juce::juce_recommended_config_flags)

# ── Nova license activation (a key, like every Nova plugin) ─────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaLink)

# Windows: WebView2 (Edge) backend for the WebView UI (no-op on macOS)
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/cmake/NovaWindows.cmake)
nova_windows_webview2(NovaLink)
