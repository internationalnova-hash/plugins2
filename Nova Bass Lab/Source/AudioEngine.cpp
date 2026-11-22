#include "AudioEngine.h"

using namespace nova::bass;

namespace
{
    NovaBassPreset makeDefaultSound()
    {
        NovaBassPreset p;
        p.meta.name = "Untitled";
        p.meta.category = "808";
        p.sourceType = NovaBassSourceType::Synth;
        p.voiceParams.pitchEnvAmountSemitones = 12.0f;
        p.voiceParams.pitchEnvDecaySeconds = 0.2f;
        p.voiceParams.ampDecaySeconds = 0.6f;
        p.voiceParams.ampSustain = 0.5f;
        p.voiceParams.transientAmount = 0.3f;
        p.colorMode = NovaColorMode::Warm;
        p.colorDrive = 0.3f;
        p.stereoParams.widthAmount = 0.5f;
        return p;
    }
}

NovaBassLabAudioEngine::NovaBassLabAudioEngine()
    : currentSound (makeDefaultSound()),
      rng (juce::Time::currentTimeMillis())
{
    vault.initialize();
    vault.ensureVaultStructure(); // Lab is responsible for the vault existing/being well-formed

    candidateIndex.setIndexFileLocation (vault.getMetadataFolder().getChildFile ("candidate_index.json"));
    candidateIndex.load();

    presetManager.scanDirectory (vault.getFactoryPresetsFolder());

    deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (this);

    for (auto& midiInput : juce::MidiInput::getAvailableDevices())
    {
        if (deviceManager.isMidiInputDeviceEnabled (midiInput.identifier))
            deviceManager.addMidiInputDeviceCallback (midiInput.identifier, this);
        else
        {
            deviceManager.setMidiInputDeviceEnabled (midiInput.identifier, true);
            deviceManager.addMidiInputDeviceCallback (midiInput.identifier, this);
        }
    }
}

NovaBassLabAudioEngine::~NovaBassLabAudioEngine()
{
    for (auto& midiInput : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (midiInput.identifier, this);
    deviceManager.removeAudioCallback (this);
}

void NovaBassLabAudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    const int blockSize = device->getCurrentBufferSizeSamples();

    const juce::SpinLock::ScopedLockType sl (engineLock);
    liveEngine.prepare (currentSampleRate, blockSize);
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

void NovaBassLabAudioEngine::audioDeviceStopped() {}

void NovaBassLabAudioEngine::audioDeviceIOCallbackWithContext (
    const float* const* /*inputChannelData*/, int /*numInputChannels*/,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    if (numOutputChannels < 2) return;

    const juce::SpinLock::ScopedLockType sl (engineLock);

    {
        const juce::SpinLock::ScopedLockType uiLock (uiNoteLock);
        for (int i = 0; i < pendingUiNoteCount; ++i)
        {
            const auto& n = pendingUiNotes[(size_t) i];
            if (n.isOn) liveEngine.noteOn (n.note, 1.0f);
            else liveEngine.noteOff (n.note);
        }
        pendingUiNoteCount = 0;
    }

    juce::AudioBuffer<float> buffer (outputChannelData, numOutputChannels, numSamples);
    liveEngine.process (buffer);
}

void NovaBassLabAudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    if (message.isNoteOn())
        liveEngine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        liveEngine.noteOff (message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        for (int n = 0; n < 128; ++n) liveEngine.noteOff (n);
}

void NovaBassLabAudioEngine::injectNoteFromUI (int midiNote, bool isOn)
{
    const juce::SpinLock::ScopedLockType sl (uiNoteLock);
    if (pendingUiNoteCount < kMaxPendingUiNotes)
        pendingUiNotes[(size_t) pendingUiNoteCount++] = { midiNote, isOn };
}

void NovaBassLabAudioEngine::setVoiceParams (const NovaBassVoiceParams& params)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    currentSound.voiceParams = params;
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

NovaBassVoiceParams NovaBassLabAudioEngine::getVoiceParams() const
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    return currentSound.voiceParams;
}

void NovaBassLabAudioEngine::setColorMode (NovaColorMode mode)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    currentSound.colorMode = mode;
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

void NovaBassLabAudioEngine::setColorDrive (float drive01)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    currentSound.colorDrive = drive01;
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

NovaColorMode NovaBassLabAudioEngine::getColorMode() const
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    return currentSound.colorMode;
}

float NovaBassLabAudioEngine::getColorDrive() const
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    return currentSound.colorDrive;
}

void NovaBassLabAudioEngine::setStereoParams (const NovaBassStereoParams& params)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    currentSound.stereoParams = params;
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

NovaBassStereoParams NovaBassLabAudioEngine::getStereoParams() const
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    return currentSound.stereoParams;
}

void NovaBassLabAudioEngine::loadSound (const NovaBassPreset& sound)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    currentSound = sound;
    currentSound.macros.clear(); // see header comment: raw editing must not be shadowed by stale macro mappings
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

NovaBassPreset NovaBassLabAudioEngine::getCurrentSound() const
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    return currentSound;
}

void NovaBassLabAudioEngine::setMacroTable (const NovaMacroTable& table)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    currentSound.macros = table;
    liveEngine.loadPreset (currentSound, nullptr, 0);
}

juce::String NovaBassLabAudioEngine::generateNew (const juce::String& profileId)
{
    auto generated = nova::basslab::NovaBassGenerator::generate (profileId, rng);
    loadSound (generated);

    currentCandidateId = candidateIndex.addCandidate (profileId);
    candidateIndex.save();
    return currentCandidateId;
}

juce::String NovaBassLabAudioEngine::mutateCurrent (nova::basslab::NovaMutateIntensity intensity)
{
    const auto sourceCandidateId = currentCandidateId;
    auto mutated = nova::basslab::NovaBassMutator::mutate (getCurrentSound(), intensity, rng);
    loadSound (mutated);

    currentCandidateId = candidateIndex.addCandidate ("", sourceCandidateId);
    candidateIndex.save();
    return currentCandidateId;
}

bool NovaBassLabAudioEngine::setCurrentLifecycleStatus (nova::basslab::NovaCandidateStatus status)
{
    if (currentCandidateId.isEmpty()) return false;
    const bool ok = candidateIndex.setStatus (currentCandidateId, status);
    if (ok) candidateIndex.save();
    return ok;
}

juce::String NovaBassLabAudioEngine::renderCurrentToVault (const nova::basslab::NovaBassRenderOptions& options)
{
    const auto sound = getCurrentSound(); // renderEngine is never touched by the live audio callback

    auto result = nova::basslab::NovaBassRenderer::renderToBuffer (renderEngine, sound, options);
    if (! result.success) return {};

    const auto fileName = nova::basslab::NovaBassRenderer::makeAssetFileName (sound.meta.category, sound.meta.name, options.rootNote);
    const auto outputFile = vault.getNovaGeneratedSamplesFolder().getChildFile (fileName);

    if (! nova::basslab::NovaBassRenderer::writeToWavFile (result, outputFile, options.bitDepth))
        return {};

    NovaBassManifestEntry entry;
    entry.source = NovaBassAssetSource::NovaGenerated;
    entry.engineVersion = "0.1.0";
    if (currentCandidateId.isNotEmpty())
        if (auto* candidate = candidateIndex.find (currentCandidateId))
            entry.generatorId = candidate->generatorId;
    const auto now = juce::Time::getCurrentTime();
    entry.dateAcquired = juce::String::formatted ("%04d-%02d-%02d", now.getYear(), now.getMonth() + 1, now.getDayOfMonth());
    entry.novaProcessedAsset = fileName;
    entry.novaPresetsUsing.add (sound.meta.name);
    vault.appendManifestEntry (entry);

    if (currentCandidateId.isNotEmpty())
    {
        candidateIndex.setRenderedAssetPath (currentCandidateId, fileName);
        candidateIndex.save();
    }

    return fileName;
}

bool NovaBassLabAudioEngine::saveCurrentAsPreset (const NovaBassPresetMeta& meta)
{
    auto preset = getCurrentSound();
    preset.meta = meta;

    const auto fileName = meta.name.isNotEmpty() ? meta.name : juce::String ("Untitled");
    juce::String slug;
    for (auto c : fileName)
        slug += juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (juce::CharacterFunctions::toLowerCase (c)) : "_";

    const auto outputFile = vault.getFactoryPresetsFolder().getChildFile (slug + ".novabasspreset");
    const bool ok = NovaBassPresetManager::savePresetFile (preset, outputFile);

    if (ok)
        presetManager.scanDirectory (vault.getFactoryPresetsFolder());

    return ok;
}
