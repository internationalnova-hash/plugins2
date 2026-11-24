# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaBass VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK path (established rollout pattern for this catalog).
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NOVA_BASS_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaBassCore)
set(NOVA_LICENSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense)

juce_add_binary_data(NovaBass_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_bass_BinaryData
)

juce_add_plugin(NovaBass
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvBs
    FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone
    PRODUCT_NAME "Nova Bass"
    VST3_CATEGORIES Instrument
    AAX_CATEGORY SWGenerators
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaBass
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${NOVA_LICENSE_DIR}/NovaLicense.cpp
        ${NOVA_LICENSE_DIR}/NovaLicense.h
        ${NOVA_BASS_CORE_DIR}/NovaBassEngine.cpp
        ${NOVA_BASS_CORE_DIR}/Voice/NovaBassVoiceEngine.cpp
        ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplerEngine.cpp
        ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplePlayback.cpp
        ${NOVA_BASS_CORE_DIR}/Color/NovaColorEngine.cpp
        ${NOVA_BASS_CORE_DIR}/Stereo/NovaBassStereoProcessor.cpp
        ${NOVA_BASS_CORE_DIR}/Macro/NovaMacroEngine.cpp
        ${NOVA_BASS_CORE_DIR}/Preset/NovaBassPresetManager.cpp
        ${NOVA_BASS_CORE_DIR}/Vault/NovaBassVault.cpp
)

target_include_directories(NovaBass PRIVATE ${NOVA_BASS_CORE_DIR} ${NOVA_LICENSE_DIR})

target_link_libraries(NovaBass
    PRIVATE
        NovaBass_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_product_unlocking
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(NovaBass
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        NOVA_LICENSE_SERVER_URL="https://nova-store-olive.vercel.app"
)

if(APPLE)
    target_compile_definitions(NovaBass PUBLIC JUCE_USE_CURL=0)
endif()

# ── Voice engine validation harness ─────────────────────────────────────
# Drives NovaBassVoiceEngine directly (mono retrigger/legato/glide, held-
# note-stack fallback). See VoiceValidation.cpp.

juce_add_console_app(NovaBassVoiceValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.voicevalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassVoiceValidation PRIVATE
    Source/VoiceValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Voice/NovaBassVoiceEngine.cpp
)

target_include_directories(NovaBassVoiceValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassVoiceValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassVoiceValidation PRIVATE
    JucePlugin_Name="NovaBassVoiceValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassVoiceValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Sampler engine validation harness ───────────────────────────────────
# Drives NovaBassSamplerEngine/NovaBassSamplePlayback directly (zone
# lookup, round-robin, velocity crossfade, Catmull-Rom-vs-linear sub-bass
# regression). See SamplerValidation.cpp.

juce_add_console_app(NovaBassSamplerValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.samplervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassSamplerValidation PRIVATE
    Source/SamplerValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplerEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplePlayback.cpp
)

target_include_directories(NovaBassSamplerValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassSamplerValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassSamplerValidation PRIVATE
    JucePlugin_Name="NovaBassSamplerValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassSamplerValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Color engine validation harness ─────────────────────────────────────
# Drives NovaColorEngine directly (mode x drive interaction, harmonic
# content, near-zero-drive passthrough). See ColorValidation.cpp.

juce_add_console_app(NovaBassColorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.colorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassColorValidation PRIVATE
    Source/ColorValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Color/NovaColorEngine.cpp
)

target_include_directories(NovaBassColorValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassColorValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassColorValidation PRIVATE
    JucePlugin_Name="NovaBassColorValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassColorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Stereo processor validation harness ─────────────────────────────────
# Drives NovaBassStereoProcessor directly (mono-bass protection below the
# crossover, width scaling above it, mid preservation). See StereoValidation.cpp.

juce_add_console_app(NovaBassStereoValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.stereovalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassStereoValidation PRIVATE
    Source/StereoValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Stereo/NovaBassStereoProcessor.cpp
)

target_include_directories(NovaBassStereoValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassStereoValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassStereoValidation PRIVATE
    JucePlugin_Name="NovaBassStereoValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassStereoValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Macro engine validation harness ─────────────────────────────────────
# Drives NovaMacroMap/NovaMacroEngine directly (curve/polarity resolution,
# single-target and multi-target fan-out, graceful unknown-target
# handling). See MacroValidation.cpp.

juce_add_console_app(NovaBassMacroValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.macrovalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassMacroValidation PRIVATE
    Source/MacroValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Macro/NovaMacroEngine.cpp
)

target_include_directories(NovaBassMacroValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassMacroValidation PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassMacroValidation PRIVATE
    JucePlugin_Name="NovaBassMacroValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassMacroValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Preset system validation harness ────────────────────────────────────
# Drives NovaBassPresetManager directly (JSON round-trip incl. macro
# table, directory scan, search/category/favorites filtering, favorite
# persistence to disk). See PresetValidation.cpp.

juce_add_console_app(NovaBassPresetValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.presetvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassPresetValidation PRIVATE
    Source/PresetValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Preset/NovaBassPresetManager.cpp
    ${NOVA_BASS_CORE_DIR}/Color/NovaColorEngine.cpp
)

target_include_directories(NovaBassPresetValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassPresetValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassPresetValidation PRIVATE
    JucePlugin_Name="NovaBassPresetValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassPresetValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Sound Vault validation harness ──────────────────────────────────────
# Drives NovaBassVault directly, entirely via isolated temp config/vault
# paths - never touches the real user's Nova Bass Vault. See
# VaultValidation.cpp.

juce_add_console_app(NovaBassVaultValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.vaultvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassVaultValidation PRIVATE
    Source/VaultValidation.cpp
    ${NOVA_BASS_CORE_DIR}/Vault/NovaBassVault.cpp
)

target_include_directories(NovaBassVaultValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassVaultValidation PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassVaultValidation PRIVATE
    JucePlugin_Name="NovaBassVaultValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassVaultValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── End-to-end integration validation harness ───────────────────────────
# Reconstructs the full NovaBassEngine composition (every subsystem below)
# and drives it against real hand-built presets: Synth/Sample/Hybrid
# source types, macro changes, and bypass ramping. See
# IntegrationValidation.cpp.

juce_add_console_app(NovaBassIntegrationValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabass.integrationvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassIntegrationValidation PRIVATE
    Source/IntegrationValidation.cpp
    ${NOVA_BASS_CORE_DIR}/NovaBassEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Voice/NovaBassVoiceEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplerEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplePlayback.cpp
    ${NOVA_BASS_CORE_DIR}/Color/NovaColorEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Stereo/NovaBassStereoProcessor.cpp
    ${NOVA_BASS_CORE_DIR}/Macro/NovaMacroEngine.cpp
)

target_include_directories(NovaBassIntegrationValidation PRIVATE ${NOVA_BASS_CORE_DIR})

target_link_libraries(NovaBassIntegrationValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassIntegrationValidation PRIVATE
    JucePlugin_Name="NovaBassIntegrationValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassIntegrationValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Nova license activation (one-time keys and Nova All Access) ──
include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaBass)
