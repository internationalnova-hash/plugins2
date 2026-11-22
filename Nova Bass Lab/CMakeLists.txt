# Ensure JUCE does not build VST2 client code (force disable)
set(JUCE_BUILD_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_VST2_FORMAT OFF CACHE BOOL "" FORCE)
set(JUCE_VST2SDK_PATH "" CACHE PATH "" FORCE)

cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(NovaBassLab VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
)
FetchContent_MakeAvailable(juce)

# Nova Bass Lab - internal-only sound-design tool for Nova Bass (spec
# sections 29-54). Drives the SAME _shared/NovaBassCore engine the "Nova
# Bass" plugin uses - do NOT duplicate DSP here. Never shipped to
# customers; no NovaLicense gate.

set(NOVA_BASS_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaBassCore)

juce_add_binary_data(NovaBassLab_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_bass_lab_BinaryData
)

juce_add_gui_app(NovaBassLab
    COMPANY_NAME  "International Nova"
    BUNDLE_ID     "com.internationalnova.novabasslab"
    PRODUCT_NAME  "Nova Bass Lab"
    VERSION       ${PROJECT_VERSION}
)

target_sources(NovaBassLab
    PRIVATE
        Source/Main.cpp
        Source/MainWindow.h
        Source/MainWindow.cpp
        Source/AudioEngine.h
        Source/AudioEngine.cpp
        Source/Generate/NovaBassGenerator.h
        Source/Generate/NovaBassGenerator.cpp
        Source/Mutate/NovaBassMutator.h
        Source/Mutate/NovaBassMutator.cpp
        Source/Lifecycle/NovaBassCandidateIndex.h
        Source/Lifecycle/NovaBassCandidateIndex.cpp
        Source/Render/NovaBassRenderer.h
        Source/Render/NovaBassRenderer.cpp
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

target_include_directories(NovaBassLab PRIVATE ${NOVA_BASS_CORE_DIR} Source)

juce_generate_juce_header(NovaBassLab)

target_link_libraries(NovaBassLab
    PRIVATE
        NovaBassLab_WebUI
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
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(NovaBassLab
    PUBLIC
        JUCE_WEB_BROWSER=1
)

if(APPLE)
    target_compile_definitions(NovaBassLab PUBLIC JUCE_USE_CURL=0)
endif()

# ── Generator validation harness ────────────────────────────────────────
# Drives NovaBassGenerator directly - confirms constrained generation
# actually stays within each profile's declared ranges. See
# GeneratorValidation.cpp.

juce_add_console_app(NovaBassLabGeneratorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabasslab.generatorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassLabGeneratorValidation PRIVATE
    Source/GeneratorValidation.cpp
    Source/Generate/NovaBassGenerator.cpp
)

target_include_directories(NovaBassLabGeneratorValidation PRIVATE ${NOVA_BASS_CORE_DIR} Source)

target_link_libraries(NovaBassLabGeneratorValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassLabGeneratorValidation PRIVATE
    JucePlugin_Name="NovaBassLabGeneratorValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassLabGeneratorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Mutator validation harness ───────────────────────────────────────────
# Drives NovaBassMutator directly - confirms Subtle < Medium < Wild
# perturbation magnitude scaling and output bounds. See MutatorValidation.cpp.

juce_add_console_app(NovaBassLabMutatorValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabasslab.mutatorvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassLabMutatorValidation PRIVATE
    Source/MutatorValidation.cpp
    Source/Mutate/NovaBassMutator.cpp
)

target_include_directories(NovaBassLabMutatorValidation PRIVATE ${NOVA_BASS_CORE_DIR} Source)

target_link_libraries(NovaBassLabMutatorValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_core
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassLabMutatorValidation PRIVATE
    JucePlugin_Name="NovaBassLabMutatorValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassLabMutatorValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Candidate index validation harness ──────────────────────────────────
# Drives NovaBassCandidateIndex directly via an isolated temp file - never
# touches the real vault's Metadata/candidate_index.json. See
# CandidateIndexValidation.cpp.

juce_add_console_app(NovaBassLabCandidateIndexValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabasslab.candidateindexvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassLabCandidateIndexValidation PRIVATE
    Source/CandidateIndexValidation.cpp
    Source/Lifecycle/NovaBassCandidateIndex.cpp
)

target_include_directories(NovaBassLabCandidateIndexValidation PRIVATE Source)

target_link_libraries(NovaBassLabCandidateIndexValidation PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassLabCandidateIndexValidation PRIVATE
    JucePlugin_Name="NovaBassLabCandidateIndexValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassLabCandidateIndexValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Renderer validation harness ─────────────────────────────────────────
# Drives NovaBassRenderer directly - render correctness, auto-trim, peak
# normalization, and a real WAV round-trip via AudioFormatReader. See
# RendererValidation.cpp.

juce_add_console_app(NovaBassLabRendererValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabasslab.renderervalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassLabRendererValidation PRIVATE
    Source/RendererValidation.cpp
    Source/Render/NovaBassRenderer.cpp
    ${NOVA_BASS_CORE_DIR}/NovaBassEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Voice/NovaBassVoiceEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplerEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplePlayback.cpp
    ${NOVA_BASS_CORE_DIR}/Color/NovaColorEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Stereo/NovaBassStereoProcessor.cpp
    ${NOVA_BASS_CORE_DIR}/Macro/NovaMacroEngine.cpp
)

target_include_directories(NovaBassLabRendererValidation PRIVATE ${NOVA_BASS_CORE_DIR} Source)

target_link_libraries(NovaBassLabRendererValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassLabRendererValidation PRIVATE
    JucePlugin_Name="NovaBassLabRendererValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassLabRendererValidation PRIVATE JUCE_USE_CURL=0)
endif()

# ── Vault save integration validation harness ───────────────────────────
# Confirms NovaBassRenderer's output + NovaBassVault's existing
# appendManifestEntry/resolveSampleRef work together correctly, via an
# isolated temp vault. See VaultSaveIntegrationValidation.cpp.

juce_add_console_app(NovaBassLabVaultSaveIntegrationValidation
    COMPANY_NAME "International Nova"
    BUNDLE_ID "com.internationalnova.novabasslab.vaultsaveintegrationvalidation"
    VERSION ${PROJECT_VERSION}
)

target_sources(NovaBassLabVaultSaveIntegrationValidation PRIVATE
    Source/VaultSaveIntegrationValidation.cpp
    Source/Render/NovaBassRenderer.cpp
    ${NOVA_BASS_CORE_DIR}/NovaBassEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Voice/NovaBassVoiceEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplerEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Sampler/NovaBassSamplePlayback.cpp
    ${NOVA_BASS_CORE_DIR}/Color/NovaColorEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Stereo/NovaBassStereoProcessor.cpp
    ${NOVA_BASS_CORE_DIR}/Macro/NovaMacroEngine.cpp
    ${NOVA_BASS_CORE_DIR}/Vault/NovaBassVault.cpp
)

target_include_directories(NovaBassLabVaultSaveIntegrationValidation PRIVATE ${NOVA_BASS_CORE_DIR} Source)

target_link_libraries(NovaBassLabVaultSaveIntegrationValidation PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
)

target_compile_definitions(NovaBassLabVaultSaveIntegrationValidation PRIVATE
    JucePlugin_Name="NovaBassLabVaultSaveIntegrationValidation"
)

if(APPLE)
    target_compile_definitions(NovaBassLabVaultSaveIntegrationValidation PRIVATE JUCE_USE_CURL=0)
endif()
