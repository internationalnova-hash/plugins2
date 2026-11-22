#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nova::bass;

NovaBassProcessor::NovaBassProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaBassProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::shape, "Shape", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::decay, "Decay", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::drive, "Drive", 0.f, 1.f, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::width, "Width", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::glide, "Glide", 0.f, 1.f, 0.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::bass, "Bass", 0.f, 1.f, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::colorMode, "Color", juce::StringArray { "Clean", "Warm", "Gritty" }, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::bypass, "Bypass", false));

    return layout;
}

void NovaBassProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    maxBlockSize = samplesPerBlock;

    engine.prepare (sampleRate, maxBlockSize);

    if (currentPresetIndex < 0)
        rescanVaultAndLoadFirstPreset();
    else
        pushMacroAndColorParamsToEngine();
}

void NovaBassProcessor::rescanVaultAndLoadFirstPreset()
{
    vault.initialize();
    if (! vault.isVaultFound()) return;

    presetManager.scanDirectory (vault.getFactoryPresetsFolder());
    if (presetManager.getNumPresets() == 0) return;

    int indexToLoad = 0;
    if (currentPresetNameForRecall.isNotEmpty())
    {
        for (int i = 0; i < presetManager.getNumPresets(); ++i)
            if (presetManager.getPreset (i)->meta.name == currentPresetNameForRecall) { indexToLoad = i; break; }
    }
    loadPresetByIndex (indexToLoad);
}

bool NovaBassProcessor::loadPresetByIndex (int index)
{
    auto* preset = presetManager.getPreset (index);
    if (preset == nullptr) return false;

    // Sample/Hybrid-sourced presets: v0.1 maps one root sample chromatically
    // across the full key range (spec section 15's own suggested minimum
    // for "simple one-shot 808s") - a multi-zone Sample Vault browser is
    // Nova Bass Lab territory (Milestone B), not required here.
    NovaBassSampleZone zone;
    bool haveZone = false;

    if (preset->sourceType != NovaBassSourceType::Synth && preset->sampleRef.isNotEmpty())
    {
        auto sampleFile = vault.resolveSampleRef (preset->sampleRef);
        if (sampleFile.existsAsFile())
        {
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (sampleFile)))
            {
                auto buffer = std::make_shared<juce::AudioBuffer<float>> ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (buffer.get(), 0, (int) reader->lengthInSamples, 0, true, true);

                zone.name = preset->meta.name;
                zone.lowKey = 0; zone.highKey = 127;
                zone.lowVelocity = 1; zone.highVelocity = 127;
                zone.rootNote = 60;
                zone.sourceSampleRate = reader->sampleRate;
                zone.buffer = buffer;
                haveZone = true;
            }
        }
    }

    engine.loadPreset (*preset, haveZone ? &zone : nullptr, haveZone ? 1 : 0);

    // The CLEAN/WARM/GRITTY selector is a plugin-level APVTS param (so the
    // user's live button clicks persist independent of whatever preset is
    // loaded) - but a newly-loaded preset's own authored colorMode must
    // still win as the new starting point, or switching presets would keep
    // whatever color was last clicked instead of what the sound designer
    // actually saved.
    if (auto* colorParam = apvts.getParameter (ParameterIDs::colorMode))
        colorParam->setValueNotifyingHost ((float) static_cast<int> (preset->colorMode) / 2.0f);

    pushMacroAndColorParamsToEngine();

    currentPresetIndex = index;
    currentPresetNameForRecall = preset->meta.name;
    return true;
}

bool NovaBassProcessor::relocateVault (const juce::File& newPath)
{
    vault.setVaultPath (newPath);
    if (! vault.isVaultFound()) return false;

    presetManager.scanDirectory (vault.getFactoryPresetsFolder());
    if (presetManager.getNumPresets() > 0)
        loadPresetByIndex (0);
    return true;
}

void NovaBassProcessor::pushMacroAndColorParamsToEngine()
{
    engine.setMacroValue ("SHAPE", *apvts.getRawParameterValue (ParameterIDs::shape));
    engine.setMacroValue ("DECAY", *apvts.getRawParameterValue (ParameterIDs::decay));
    engine.setMacroValue ("DRIVE", *apvts.getRawParameterValue (ParameterIDs::drive));
    engine.setMacroValue ("WIDTH", *apvts.getRawParameterValue (ParameterIDs::width));
    engine.setMacroValue ("GLIDE", *apvts.getRawParameterValue (ParameterIDs::glide));
    engine.setMacroValue ("BASS", *apvts.getRawParameterValue (ParameterIDs::bass));

    const int colorIndex = (int) *apvts.getRawParameterValue (ParameterIDs::colorMode);
    engine.setColorMode (colorIndex == 0 ? NovaColorMode::Clean : colorIndex == 2 ? NovaColorMode::Gritty : NovaColorMode::Warm);

    engine.setBypassed (*apvts.getRawParameterValue (ParameterIDs::bypass) > 0.5f);
}

void NovaBassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    pushMacroAndColorParamsToEngine();

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            midiActivity.store (true, std::memory_order_relaxed);
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            engine.noteOff (msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (int n = 0; n < 128; ++n) engine.noteOff (n);
        }
    }

    engine.process (buffer);

    anyVoiceActive.store (engine.isAnyVoiceActive(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* NovaBassProcessor::createEditor()
{
    return new NovaBassEditor (*this);
}

void NovaBassProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("NovaBassState");

    auto state = apvts.copyState();
    if (auto apvtsXml = state.createXml())
        root.addChildElement (apvtsXml.release());

    root.setAttribute ("currentPresetName", currentPresetNameForRecall);

    copyXmlToBinary (root, dest);
}

void NovaBassProcessor::setStateInformation (const void* data, int size)
{
    auto root = getXmlFromBinary (data, size);
    if (root == nullptr) return;

    if (auto* apvtsXml = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

    currentPresetNameForRecall = root->getStringAttribute ("currentPresetName");

    if (maxBlockSize > 0 && currentPresetIndex < 0)
        rescanVaultAndLoadFirstPreset();

    pushMacroAndColorParamsToEngine();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaBassProcessor(); }
