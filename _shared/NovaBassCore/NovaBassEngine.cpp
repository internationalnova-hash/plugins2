#include "NovaBassEngine.h"
#include <cmath>

namespace nova::bass
{

namespace
{
    constexpr const char* kMacroNames[] = { "SHAPE", "DECAY", "DRIVE", "WIDTH", "GLIDE", "BASS" };
}

void NovaBassEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;

    monoVoice.prepare (sampleRate);
    samplerEngine.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maximumBlockSize, 2 };
    colorEngine.prepare (spec);
    stereoProcessor.prepare (spec);

    bypassGain.reset (sampleRate, 0.02); // 20ms click-free ramp

    for (auto& name : kMacroNames)
        if (macroKnobValues01.find (name) == macroKnobValues01.end())
            macroKnobValues01[name] = 0.5f;

    reset();
}

void NovaBassEngine::reset() noexcept
{
    monoVoice.reset();
    for (auto& slot : polyVoices) { slot.playback.reset(); slot.active = false; }
    colorEngine.reset();
    stereoProcessor.reset();
    bypassGain.setCurrentAndTargetValue (bypassed ? 0.0f : 1.0f);
}

void NovaBassEngine::loadPreset (const NovaBassPreset& preset, const NovaBassSampleZone* sampleZones, int numSampleZones)
{
    currentPreset = preset;

    baselineTargets.voice = preset.voiceParams;
    baselineTargets.colorDrive = preset.colorDrive;
    baselineTargets.stereoWidth = preset.stereoParams.widthAmount;
    liveColorMode = preset.colorMode;

    samplerEngine.clearZones();
    if (sampleZones != nullptr)
        for (int i = 0; i < numSampleZones; ++i)
            samplerEngine.addZone (sampleZones[i]);

    recomputeResolvedTargets();
    applyResolvedTargetsToVoice();
}

void NovaBassEngine::setMacroValue (const juce::String& macroName, float value01) noexcept
{
    macroKnobValues01[macroName] = juce::jlimit (0.0f, 1.0f, value01);
    recomputeResolvedTargets();
    applyResolvedTargetsToVoice();
}

float NovaBassEngine::getMacroValue (const juce::String& macroName) const noexcept
{
    auto it = macroKnobValues01.find (macroName);
    return it != macroKnobValues01.end() ? it->second : 0.5f;
}

void NovaBassEngine::recomputeResolvedTargets() noexcept
{
    resolvedTargets = NovaMacroEngine::resolve (baselineTargets, currentPreset.macros, macroKnobValues01);
}

void NovaBassEngine::applyResolvedTargetsToVoice() noexcept
{
    monoVoice.setParameters (resolvedTargets.voice);
    colorEngine.setParams (liveColorMode, resolvedTargets.colorDrive);
    stereoProcessor.setParams (resolvedTargets.stereoWidth, currentPreset.stereoParams.crossoverHz);
}

void NovaBassEngine::setBypassed (bool shouldBypass) noexcept
{
    bypassed = shouldBypass;
    bypassGain.setTargetValue (bypassed ? 0.0f : 1.0f);
}

void NovaBassEngine::setColorMode (NovaColorMode mode) noexcept
{
    liveColorMode = mode;
    colorEngine.setParams (liveColorMode, resolvedTargets.colorDrive);
}

void NovaBassEngine::noteOn (int midiNote, float velocity) noexcept
{
    const bool useSynth = currentPreset.sourceType == NovaBassSourceType::Synth || currentPreset.sourceType == NovaBassSourceType::Hybrid;
    const bool useSample = currentPreset.sourceType == NovaBassSourceType::Sample || currentPreset.sourceType == NovaBassSourceType::Hybrid;

    if (useSynth)
        monoVoice.noteOn (midiNote, velocity);

    if (useSample)
    {
        const int midiVelocity = juce::jlimit (1, 127, (int) std::round (velocity * 127.0f));
        auto selection = samplerEngine.selectZones (midiNote, midiVelocity);
        if (selection.primary != nullptr)
        {
            int slotIndex = -1;
            for (int i = 0; i < kMaxPolyVoices; ++i)
                if (! polyVoices[(size_t) i].active) { slotIndex = i; break; }
            if (slotIndex < 0)
                slotIndex = 0; // simple steal: v0.1 does not need quietest/oldest sophistication for one-shot bass hits

            auto& slot = polyVoices[(size_t) slotIndex];
            slot.playback.start (selection, midiNote, sampleRate);
            slot.active = true;
        }
    }
}

void NovaBassEngine::noteOff (int midiNote) noexcept
{
    const bool useSynth = currentPreset.sourceType == NovaBassSourceType::Synth || currentPreset.sourceType == NovaBassSourceType::Hybrid;
    if (useSynth)
        monoVoice.noteOff (midiNote);

    // Sample-based voices are one-shot (spec: "a real doubled performance"-
    // style 808 sample is simple enough to map chromatically and trigger
    // fire-and-forget) - they ring out naturally rather than being
    // envelope-released on noteOff. A sustained-pad-style sample use case
    // would need a NovaInstrumentLayer-style envelope wrapper on top of
    // NovaBassSamplePlayback; not required by the frozen v0.1 scope.
}

void NovaBassEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0) return;

    buffer.clear();

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    const bool useSynth = currentPreset.sourceType == NovaBassSourceType::Synth || currentPreset.sourceType == NovaBassSourceType::Hybrid;
    const bool useSample = currentPreset.sourceType == NovaBassSourceType::Sample || currentPreset.sourceType == NovaBassSourceType::Hybrid;

    if (useSample)
    {
        for (auto& slot : polyVoices)
        {
            if (! slot.active) continue;
            const bool stillActive = slot.playback.renderAdding (outL, outR, numSamples);
            if (! stillActive) slot.active = false;
        }
    }

    if (useSynth)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float monoSample = monoVoice.getNextSample();
            outL[i] += monoSample;
            outR[i] += monoSample;
        }
    }

    colorEngine.process (buffer);
    stereoProcessor.process (buffer);

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = bypassGain.getNextValue();
        outL[i] *= gain;
        outR[i] *= gain;
    }
}

bool NovaBassEngine::isAnyVoiceActive() const noexcept
{
    if (monoVoice.isActive()) return true;
    for (auto& slot : polyVoices)
        if (slot.active) return true;
    return false;
}

} // namespace nova::bass
