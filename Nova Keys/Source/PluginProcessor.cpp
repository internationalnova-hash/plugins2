#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Sampler/NovaSampleFolderLoader.h"

namespace
{
    // Guards samplerEngines[]/keysEngine's layer configuration during a
    // preset switch, held across BOTH the audio thread's render call and
    // the message thread's reconfiguration - see the header comment on
    // hardResetAllVoices() for why this is necessary (a released voice
    // holds a raw pointer into zone data that a preset switch is about to
    // replace). Held for the full duration of loadPresetByIndex, including
    // its file I/O - a deliberate, documented v0.1 trade-off: correctness
    // (no dangling pointers/crashes) over a possible brief audio dropout
    // during a preset switch, which is not one of the v0.1 success
    // criteria ("no clicks during NORMAL VOICE STEALING" is a different,
    // already-solved requirement).
    juce::SpinLock& getEngineLock()
    {
        static juce::SpinLock lock;
        return lock;
    }
}

NovaKeysProcessor::NovaKeysProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& n : heldNotes) n.store (false);
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaKeysProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "master", "Master", juce::NormalisableRange<float> (-60.f, 6.f, 0.01f), -3.2f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "tune", "Tune", juce::NormalisableRange<float> (-100.f, 100.f, 0.01f), 0.f));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macroTone", "Tone", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macroMotion", "Motion", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macroSpace", "Space", 0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macroCharacter", "Character", 0.f, 1.f, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "portaTimeMs", "Portamento Time", juce::NormalisableRange<float> (0.f, 500.f, 0.1f), 80.f));
    layout.add (std::make_unique<juce::AudioParameterInt> ("unisonVoices", "Unison Voices", 1, 8, 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("glideAmount", "Glide", 0.f, 1.f, 0.32f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "voiceMode", "Voice Mode", juce::StringArray { "Poly", "Mono" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("randomAmount", "Random", 0.f, 1.f, 0.f));

    return layout;
}

void NovaKeysProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    maxBlockSize = samplesPerBlock;

    for (auto& n : heldNotes) n.store (false);

    {
        // Scoped tightly: rescanVaultAndLoadFirstPreset() below calls
        // loadPresetByIndex(), which takes getEngineLock() itself. Holding
        // it across that call would self-deadlock (SpinLock is not
        // recursive) - this previously hung the message thread at 100% CPU
        // on every startup with a vault present, before any window showed.
        const juce::SpinLock::ScopedLockType sl (getEngineLock());

        for (auto& se : samplerEngines) se.prepare (sampleRate);
        keysEngine.prepare (sampleRate, maxBlockSize, 32); // 32-voice pool for v0.1
        effectsChain.prepare (sampleRate, maxBlockSize);
        engineScratch.setSize (2, maxBlockSize, false, false, true);
    }

    if (currentPresetIndex < 0)
        rescanVaultAndLoadFirstPreset();
}

void NovaKeysProcessor::rescanVaultAndLoadFirstPreset()
{
    vault.initialize();
    if (! vault.isVaultFound()) return;

    presetManager.scanDirectory (vault.getPresetsFolder());
    if (presetManager.getNumPresets() == 0) return;

    int indexToLoad = 0;
    if (currentPresetNameForRecall.isNotEmpty())
    {
        for (int i = 0; i < presetManager.getNumPresets(); ++i)
            if (presetManager.getPreset (i)->meta.name == currentPresetNameForRecall) { indexToLoad = i; break; }
    }
    loadPresetByIndex (indexToLoad);
}

bool NovaKeysProcessor::loadPresetByIndex (int index)
{
    auto* preset = presetManager.getPreset (index);
    if (preset == nullptr) return false;

    const juce::SpinLock::ScopedLockType sl (getEngineLock());

    keysEngine.hardResetAllVoices(); // must complete before any zone data below is replaced - see getEngineLock() comment

    for (int i = 0; i < NovaKeysEngine::kNumLayerSlots; ++i)
    {
        const auto& layer = preset->layers[(size_t) i];

        if (! layer.used)
        {
            keysEngine.setLayerUnconfigured (i);
            samplerEngines[(size_t) i].clearZones();
            continue;
        }

        if (layer.sourceType == NovaLayerSourceType::Sample)
        {
            samplerEngines[(size_t) i].clearZones();
            auto folder = vault.resolveSampleRef (layer.sampleRef);
            NovaSampleFolderLoader::loadFolderIntoEngine (folder, samplerEngines[(size_t) i]);
            keysEngine.configureLayerAsSample (i, &samplerEngines[(size_t) i], layer.layerParams);
        }
        else
        {
            samplerEngines[(size_t) i].clearZones();
            keysEngine.configureLayerAsSynth (i, layer.synthParams, layer.layerParams);
        }
    }

    effectsChain.setParams (preset->effects);
    effectsChain.setMacros (preset->macros);

    if (auto* p = apvts.getParameter ("macroTone"))        p->setValueNotifyingHost (p->convertTo0to1 (preset->macros.tone));
    if (auto* p = apvts.getParameter ("macroMotion"))       p->setValueNotifyingHost (p->convertTo0to1 (preset->macros.motion));
    if (auto* p = apvts.getParameter ("macroSpace"))        p->setValueNotifyingHost (p->convertTo0to1 (preset->macros.space));
    if (auto* p = apvts.getParameter ("macroCharacter"))    p->setValueNotifyingHost (p->convertTo0to1 (preset->macros.character));

    currentPresetIndex = index;
    currentPresetNameForRecall = preset->meta.name;
    return true;
}

bool NovaKeysProcessor::relocateVault (const juce::File& newPath)
{
    vault.setVaultPath (newPath);
    if (! vault.isVaultFound()) return false;

    presetManager.scanDirectory (vault.getPresetsFolder());
    if (presetManager.getNumPresets() > 0)
        loadPresetByIndex (0);
    return true;
}

void NovaKeysProcessor::refreshMacrosFromApvts()
{
    NovaMacroValues macros;
    macros.tone = *apvts.getRawParameterValue ("macroTone");
    macros.motion = *apvts.getRawParameterValue ("macroMotion");
    macros.space = *apvts.getRawParameterValue ("macroSpace");
    macros.character = *apvts.getRawParameterValue ("macroCharacter");

    const juce::SpinLock::ScopedLockType sl (getEngineLock());
    effectsChain.setMacros (macros);
}

void NovaKeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    {
        const juce::SpinLock::ScopedLockType sl (uiNoteLock);
        for (int i = 0; i < pendingUiNoteCount; ++i)
        {
            const auto& n = pendingUiNotes[(size_t) i];
            if (n.isOn)
                midi.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) 100), 0);
            else
                midi.addEvent (juce::MidiMessage::noteOff (1, n.note), 0);
        }
        pendingUiNoteCount = 0;
    }

    buffer.clear();

    const juce::SpinLock::ScopedLockType sl (getEngineLock());

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            heldNotes[(size_t) msg.getNoteNumber()].store (true, std::memory_order_relaxed);
            midiActivity.store (true, std::memory_order_relaxed);
            keysEngine.noteOn (msg.getNoteNumber(), (int) msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            heldNotes[(size_t) msg.getNoteNumber()].store (false, std::memory_order_relaxed);
            keysEngine.noteOff (msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& n : heldNotes) n.store (false, std::memory_order_relaxed);
            keysEngine.allNotesOff();
        }
    }

    const int numSamples = buffer.getNumSamples();
    const int n = juce::jmin (numSamples, engineScratch.getNumSamples());
    if (n > 0)
    {
        engineScratch.clear();
        keysEngine.renderNextBlock (engineScratch.getWritePointer (0), engineScratch.getWritePointer (1), n);

        juce::AudioBuffer<float> scratchView (engineScratch.getArrayOfWritePointers(), 2, n);
        effectsChain.process (scratchView);

        const float masterGain = juce::Decibels::decibelsToGain ((float) *apvts.getRawParameterValue ("master"));
        const int outCh = juce::jmin (2, buffer.getNumChannels());
        for (int ch = 0; ch < outCh; ++ch)
            buffer.addFrom (ch, 0, engineScratch, ch, 0, n, masterGain); // buffer.clear()'d above, so add == copy here
    }
}

juce::AudioProcessorEditor* NovaKeysProcessor::createEditor()
{
    return new NovaKeysEditor (*this);
}

void NovaKeysProcessor::injectNoteFromUI (int midiNote, bool isOn)
{
    const juce::SpinLock::ScopedLockType sl (uiNoteLock);
    if (pendingUiNoteCount < kMaxPendingUiNotes)
        pendingUiNotes[(size_t) pendingUiNoteCount++] = { midiNote, isOn };
}

void NovaKeysProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("NovaKeysState");

    auto state = apvts.copyState();
    if (auto apvtsXml = state.createXml())
        root.addChildElement (apvtsXml.release());

    root.setAttribute ("currentPresetName", currentPresetNameForRecall);

    copyXmlToBinary (root, dest);
}

void NovaKeysProcessor::setStateInformation (const void* data, int size)
{
    auto root = getXmlFromBinary (data, size);
    if (root == nullptr) return;

    if (auto* apvtsXml = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

    currentPresetNameForRecall = root->getStringAttribute ("currentPresetName");

    // If prepareToPlay already ran (e.g. state restored after the host
    // already activated audio), re-resolve immediately; otherwise
    // prepareToPlay's own rescanVaultAndLoadFirstPreset() will pick up
    // currentPresetNameForRecall once it runs.
    if (maxBlockSize > 0 && currentPresetIndex < 0)
        rescanVaultAndLoadFirstPreset();

    refreshMacrosFromApvts();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaKeysProcessor(); }
