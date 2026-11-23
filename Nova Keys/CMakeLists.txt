# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaKeys VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# AAX SDK path wired from day one (established rollout pattern for this
# catalog) - NOT built until commercial AAX infra is ready, so AAX is
# deliberately excluded from PLUGIN_FORMATS below (see Nova AI Master).
# AAX builds only when the shared AAX SDK is present (it is not in git, so
# GitHub's automatic builds skip AAX; local builds include it).
set(NOVA_AAX_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../_shared/AAX-SDK")
set(NOVA_AAX_FORMAT "")
if((APPLE OR WIN32) AND EXISTS "${NOVA_AAX_SDK}/Interfaces")
    juce_set_aax_sdk_path("${NOVA_AAX_SDK}")
    set(NOVA_AAX_FORMAT AAX)
endif()

set(NOVA_KEYS_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaKeysCore)

juce_add_binary_data(NovaKeys_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/images/hero-pianos.jpg
        Source/ui/public/images/hero-strings.jpg
        Source/ui/public/images/hero-brass.jpg
        Source/ui/public/images/hero-woodwinds.jpg
        Source/ui/public/images/hero-mallets.jpg
        Source/ui/public/images/hero-percussion.jpg
        Source/ui/public/images/hero-choirs.jpg
        Source/ui/public/images/hero-electric_pianos.jpg
        Source/ui/public/images/hero-organs.jpg
        Source/ui/public/images/hero-guitars.jpg
        Source/ui/public/images/hero-bass.jpg
        Source/ui/public/images/hero-synth_bass.jpg
        Source/ui/public/images/hero-leads.jpg
        Source/ui/public/images/hero-pads.jpg
        Source/ui/public/images/hero-textures.jpg
        Source/ui/public/images/hero-plucks.jpg
        Source/ui/public/images/hero-arps.jpg
        Source/ui/public/images/hero-cinematic.jpg
        Source/ui/public/images/hero-world.jpg
        Source/ui/public/images/hero-nova_signature.jpg
    NAMESPACE nova_keys_BinaryData
)

juce_add_plugin(NovaKeys
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvKy
    FORMATS VST3 AU ${NOVA_AAX_FORMAT} Standalone
    PRODUCT_NAME "Nova Keys"
    VST3_CATEGORIES Instrument
    AAX_CATEGORY SWGenerators
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(NovaKeys
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplerEngine.cpp
        ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplePlayback.cpp
        ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSampleFolderLoader.cpp
        ${NOVA_KEYS_CORE_DIR}/Synth/NovaSynthVoice.cpp
        ${NOVA_KEYS_CORE_DIR}/Layer/NovaInstrumentLayer.cpp
        ${NOVA_KEYS_CORE_DIR}/NovaKeysEngine.cpp
        ${NOVA_KEYS_CORE_DIR}/Effects/NovaEffectsChain.cpp
        ${NOVA_KEYS_CORE_DIR}/Preset/NovaPresetManager.cpp
        ${NOVA_KEYS_CORE_DIR}/Vault/NovaSoundVault.cpp
)

target_include_directories(NovaKeys PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeys
    PRIVATE
        NovaKeys_WebUI
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

target_compile_definitions(NovaKeys
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(APPLE)
    target_compile_definitions(NovaKeys PUBLIC JUCE_USE_CURL=0)
endif()

# ── Sampler engine validation harness ───────────────────────────────────
# Drives NovaSamplerEngine/NovaSamplePlayback directly against synthetic
# sine-tone zones (no real sample library needed) to prove zone lookup,
# velocity-layer crossfade, round-robin rotation, pitch-shifting and looping
# work BEFORE any real sample sourcing happens. See SamplerValidation.cpp.

juce_add_console_app(NovaKeysSamplerValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.samplervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysSamplerValidation PRIVATE
    Source/SamplerValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplerEngine.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplePlayback.cpp
)

target_include_directories(NovaKeysSamplerValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysSamplerValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysSamplerValidation PRIVATE
    JucePlugin_Name="NovaKeysSamplerValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysSamplerValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Synth engine validation harness ─────────────────────────────────────
# Drives NovaSynthVoice directly (spec section 7): waveforms, unison/detune,
# filter, amp ADSR, pitch/filter LFO. See SynthValidation.cpp.

juce_add_console_app(NovaKeysSynthValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.synthvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysSynthValidation PRIVATE
    Source/SynthValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Synth/NovaSynthVoice.cpp
)

target_include_directories(NovaKeysSynthValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysSynthValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysSynthValidation PRIVATE
    JucePlugin_Name="NovaKeysSynthValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysSynthValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Four-layer engine validation harness ────────────────────────────────
# Drives NovaInstrumentLayer directly (spec section 6): key/velocity range
# gating, on/off, note-off silencing a looping sample via the layer's own
# envelope, synth-layer double-envelope interaction, and four layers
# stacking together for one note. See LayerValidation.cpp.

juce_add_console_app(NovaKeysLayerValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.layervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysLayerValidation PRIVATE
    Source/LayerValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplerEngine.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplePlayback.cpp
    ${NOVA_KEYS_CORE_DIR}/Synth/NovaSynthVoice.cpp
    ${NOVA_KEYS_CORE_DIR}/Layer/NovaInstrumentLayer.cpp
)

target_include_directories(NovaKeysLayerValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysLayerValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysLayerValidation PRIVATE
    JucePlugin_Name="NovaKeysLayerValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysLayerValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Polyphony / voice-stealing validation harness ───────────────────────
# Drives NovaKeysEngine directly (spec section 8): fixed voice pool,
# graceful quietest/oldest stealing, correct note-off targeting, and a
# dense-MIDI stress test checking for finite output and no gross
# discontinuities (clicks) under voice stealing. See PolyphonyValidation.cpp.

juce_add_console_app(NovaKeysPolyphonyValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.polyphonyvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysPolyphonyValidation PRIVATE
    Source/PolyphonyValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplerEngine.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplePlayback.cpp
    ${NOVA_KEYS_CORE_DIR}/Synth/NovaSynthVoice.cpp
    ${NOVA_KEYS_CORE_DIR}/Layer/NovaInstrumentLayer.cpp
    ${NOVA_KEYS_CORE_DIR}/NovaKeysEngine.cpp
)

target_include_directories(NovaKeysPolyphonyValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysPolyphonyValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysPolyphonyValidation PRIVATE
    JucePlugin_Name="NovaKeysPolyphonyValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysPolyphonyValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Effects chain / macro validation harness ────────────────────────────
# Drives NovaEffectsChain directly (spec sections 13-14): each stage's
# measurable effect (EQ tilt, saturation harmonic content, compression gain
# reduction, chorus/delay/reverb energy, output ceiling), plus the four
# macros' derived parameter mappings. See EffectsValidation.cpp.

juce_add_console_app(NovaKeysEffectsValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.effectsvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysEffectsValidation PRIVATE
    Source/EffectsValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Effects/NovaEffectsChain.cpp
)

target_include_directories(NovaKeysEffectsValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysEffectsValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysEffectsValidation PRIVATE
    JucePlugin_Name="NovaKeysEffectsValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysEffectsValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Preset system validation harness ────────────────────────────────────
# Drives NovaPresetManager directly (spec sections 15-17): JSON round-trip
# fidelity, directory scanning, search/category/favorites filtering, and
# favorite-state persistence to disk. See PresetValidation.cpp.

juce_add_console_app(NovaKeysPresetValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.presetvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysPresetValidation PRIVATE
    Source/PresetValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Preset/NovaPresetManager.cpp
)

target_include_directories(NovaKeysPresetValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysPresetValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysPresetValidation PRIVATE
    JucePlugin_Name="NovaKeysPresetValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysPresetValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Sound Vault validation harness ──────────────────────────────────────
# Drives NovaSoundVault directly (spec sections 19, 22-23, 26-27): missing-
# vault handling never crashes, relocating persists across a fresh
# instance, sample-ref resolution, vault folder structure creation, and
# license manifest round-trip. Uses an isolated temp config file so this
# never touches the real user's Sound Vault settings. See VaultValidation.cpp.

juce_add_console_app(NovaKeysVaultValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.vaultvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysVaultValidation PRIVATE
    Source/VaultValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Vault/NovaSoundVault.cpp
)

target_include_directories(NovaKeysVaultValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysVaultValidation PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysVaultValidation PRIVATE
    JucePlugin_Name="NovaKeysVaultValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysVaultValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Sample folder loader validation harness ─────────────────────────────
# Drives NovaSampleFolderLoader against the REAL downloaded CC0 test
# library in the actual Sound Vault (task #97) - proves note-name parsing,
# key-range spreading, velocity-tier splitting, and round-robin grouping
# work on genuine content, not just synthetic zones. See
# FolderLoaderValidation.cpp.

juce_add_console_app(NovaKeysFolderLoaderValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.folderloadervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysFolderLoaderValidation PRIVATE
    Source/FolderLoaderValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplerEngine.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplePlayback.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSampleFolderLoader.cpp
)

target_include_directories(NovaKeysFolderLoaderValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysFolderLoaderValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysFolderLoaderValidation PRIVATE
    JucePlugin_Name="NovaKeysFolderLoaderValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysFolderLoaderValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── End-to-end integration validation harness ───────────────────────────
# Reconstructs PluginProcessor's real composition (Sound Vault -> preset
# manager -> per-layer sampler engines -> NovaKeysEngine -> effects chain)
# and drives it against all 12 REAL presets from the real downloaded CC0
# library (task #97): load, noteOn, render, noteOff, confirm clean release.
# See IntegrationValidation.cpp for why this doesn't link PluginProcessor/
# PluginEditor directly.

juce_add_console_app(NovaKeysIntegrationValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novakeys.integrationvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaKeysIntegrationValidation PRIVATE
    Source/IntegrationValidation.cpp
    ${NOVA_KEYS_CORE_DIR}/Vault/NovaSoundVault.cpp
    ${NOVA_KEYS_CORE_DIR}/Preset/NovaPresetManager.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplerEngine.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSamplePlayback.cpp
    ${NOVA_KEYS_CORE_DIR}/Sampler/NovaSampleFolderLoader.cpp
    ${NOVA_KEYS_CORE_DIR}/Synth/NovaSynthVoice.cpp
    ${NOVA_KEYS_CORE_DIR}/Layer/NovaInstrumentLayer.cpp
    ${NOVA_KEYS_CORE_DIR}/NovaKeysEngine.cpp
    ${NOVA_KEYS_CORE_DIR}/Effects/NovaEffectsChain.cpp
)

target_include_directories(NovaKeysIntegrationValidation PRIVATE ${NOVA_KEYS_CORE_DIR})

target_link_libraries(NovaKeysIntegrationValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaKeysIntegrationValidation PRIVATE
    JucePlugin_Name="NovaKeysIntegrationValidation"
)

if(APPLE)
    target_compile_definitions(NovaKeysIntegrationValidation PRIVATE JUCE_USE_CURL=0)
endif()
