# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
# Ensure JUCE does not search for VST2 SDK
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)
# VST2/LV2 stay disabled; AU and AAX are now built alongside VST3 (see
# juce_add_plugin's FORMATS below) to match the rest of the catalog.
set(JUCE_VST2_FORMAT OFF)
set(JUCE_LV2_FORMAT OFF)
set(JUCE_STANDALONE_FORMAT ON)
# Explicitly disable VST2 support to avoid missing header errors
set(JUCE_VST2_FORMAT OFF)
cmake_minimum_required(VERSION 3.15)

if(APPLE)
    # Ensure consistent macOS binary compatibility regardless of CI defaults.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaLevel VERSION 1.0.0)

# JUCE setup using FetchContent
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8 # Match newer VST3 SDK metadata path used by known-good bundle
)
FetchContent_MakeAvailable(juce)

# AAX SDK (Avid Pro Tools format) - shared across plugins, same pattern as
# the shared NovaLicense module. Mac/Windows only (JUCE's own AAX SDK path
# function silently no-ops on other platforms).
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
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

juce_add_binary_data(NovaLevel_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/n_logo.png
    NAMESPACE nova_level_BinaryData
)







juce_add_plugin(NovaLevel
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    PRODUCT_NAME "Nova Level"
    BUNDLE_ID "com.internationalnova.novalevel"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvLv
    FORMATS ${PLUGIN_FORMATS}
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 FALSE
    VST3_CAN_REPLACE_VST2 FALSE
    VST3_CATEGORIES Fx Dynamics
    AAX_CATEGORY Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
)

juce_generate_juce_header(NovaLevel)

target_sources(NovaLevel
    PRIVATE
        Source/CreatePluginFilter.cpp
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP/Dynamics/NovaLevelDSP.cpp
)

target_compile_definitions(NovaLevel PUBLIC JUCE_WEB_BROWSER=1)
# Shared DSP engine (NovaDSP/Dynamics/NovaLevelDSP)
target_include_directories(NovaLevel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NovaDSP)

target_link_libraries(NovaLevel PRIVATE NovaLevel_WebUI juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra juce::juce_audio_utils juce::juce_audio_devices)

# JUCE VST3 client defaults JUCE_VST3_CAN_REPLACE_VST2 to 1, which pulls in
# the legacy VST2 header vstfxstore.h. Force-disable it for VST3-only builds.
target_compile_definitions(NovaLevel PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)

if(TARGET NovaLevel_VST3)
    target_compile_definitions(NovaLevel_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
endif()
