# Nova Melody plugin + processor test (included from CMakeLists.txt)
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
    set(AU_MAIN_TYPE kAudioUnitType_MusicDevice)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(NovaMelody_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
    NAMESPACE nova_melody_BinaryData
)

juce_add_plugin(NovaMelody
    COMPANY_NAME "International Nova"
    COMPANY_WEBSITE "https://getnovaaudio.com"
    BUNDLE_ID "com.internationalnova.novamelody"
    PLUGIN_MANUFACTURER_CODE Inov
    PLUGIN_CODE NvMy
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nova Melody"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    VST3_CATEGORIES Instrument Generator
    AAX_CATEGORY SWGenerators
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Nova Melody listens to your humming to write it down as notes."
)

target_sources(NovaMelody PRIVATE
    ${MELODY_CORE}
    Source/MelodySynth.h
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
)
target_include_directories(NovaMelody PRIVATE Source "${NOVA_NOTES_SRC}")

target_link_libraries(NovaMelody
    PRIVATE
        NovaMelody_WebUI
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

target_compile_definitions(NovaMelody PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)
if(WIN32)
    target_compile_definitions(NovaMelody PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 _USE_MATH_DEFINES=1 JUCE_USE_WINDOWS_MEDIA_FORMAT=1)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../_shared/NovaLicense/NovaLicense.cmake)
nova_license_enable(NovaMelody)

# ── Processor test ──────────────────────────────────────────────────────────
juce_add_console_app(NovaMelodyProcessorTests PRODUCT_NAME "NovaMelodyProcessorTests")
target_sources(NovaMelodyProcessorTests PRIVATE
    Tests/ProcessorTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp ${MELODY_CORE})
target_include_directories(NovaMelodyProcessorTests PRIVATE Source "${NOVA_NOTES_SRC}")
target_link_libraries(NovaMelodyProcessorTests PRIVATE
    NovaMelody_WebUI
    juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats juce::juce_audio_processors juce::juce_core
    juce::juce_data_structures juce::juce_dsp juce::juce_events juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    juce::juce_recommended_config_flags)
target_compile_definitions(NovaMelodyProcessorTests PRIVATE
    JucePlugin_Name="NovaMelodyProcessorTests" JucePlugin_IsSynth=1 JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_MP3AUDIOFORMAT=1)
nova_license_test_target(NovaMelodyProcessorTests)
