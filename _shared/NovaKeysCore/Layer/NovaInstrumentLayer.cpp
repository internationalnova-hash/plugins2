#include "NovaInstrumentLayer.h"
#include <algorithm>
#include <cmath>

void NovaInstrumentLayer::prepare (double outputSampleRate, int maxBlockSize)
{
    sampleRate = outputSampleRate;
    layerEnvelope.setSampleRate (sampleRate);
    filterEnvelope.setSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2u };
    layerFilter.prepare (spec);
    layerFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    synthVoice.prepare (sampleRate);

    scratchL.assign ((size_t) maxBlockSize, 0.f);
    scratchR.assign ((size_t) maxBlockSize, 0.f);

    reset();
}

void NovaInstrumentLayer::configureAsSample (NovaSamplerEngine* engine, const NovaLayerParams& p) noexcept
{
    sourceType = NovaLayerSourceType::Sample;
    samplerEngine = engine;
    layerParams = p;
}

void NovaInstrumentLayer::configureAsSynth (const NovaSynthVoiceParams& sp, const NovaLayerParams& p) noexcept
{
    sourceType = NovaLayerSourceType::Synth;
    synthParams = sp;
    layerParams = p;
}

bool NovaInstrumentLayer::matchesRange (int midiNote, int velocity) const noexcept
{
    return layerParams.on
        && midiNote >= layerParams.lowKey && midiNote <= layerParams.highKey
        && velocity >= layerParams.lowVelocity && velocity <= layerParams.highVelocity;
}

void NovaInstrumentLayer::noteOn (int midiNote, int velocity) noexcept
{
    if (! matchesRange (midiNote, velocity)) return;

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack  = juce::jmax (0.0001f, layerParams.attackMs / 1000.f);
    adsrParams.decay   = juce::jmax (0.0001f, layerParams.decayMs / 1000.f);
    adsrParams.sustain = juce::jlimit (0.f, 1.f, layerParams.sustain);
    adsrParams.release = juce::jmax (0.0001f, layerParams.releaseMs / 1000.f);
    layerEnvelope.setParameters (adsrParams);
    layerEnvelope.noteOn();

    layerFilter.setType (layerParams.filterHighPass ? juce::dsp::StateVariableTPTFilterType::highpass
                                                      : juce::dsp::StateVariableTPTFilterType::lowpass);
    layerFilter.setCutoffFrequency (juce::jlimit (20.f, 20000.f, layerParams.filterCutoffHz));
    layerFilter.setResonance (juce::jmax (0.1f, layerParams.filterResonance));
    layerFilter.reset();

    juce::ADSR::Parameters filterEnvParams;
    filterEnvParams.attack  = juce::jmax (0.0001f, layerParams.filterEnvAttackMs / 1000.f);
    filterEnvParams.decay   = juce::jmax (0.0001f, layerParams.filterEnvDecayMs / 1000.f);
    filterEnvParams.sustain = juce::jlimit (0.f, 1.f, layerParams.filterEnvSustain);
    filterEnvParams.release = juce::jmax (0.0001f, layerParams.filterEnvReleaseMs / 1000.f);
    filterEnvelope.setParameters (filterEnvParams);
    filterEnvelope.noteOn();

    const int effectiveNote = midiNote + layerParams.octaveShift * 12;

    if (sourceType == NovaLayerSourceType::Sample)
    {
        samplePlayback.reset();
        if (samplerEngine != nullptr)
        {
            auto selection = samplerEngine->selectZones (midiNote, velocity);
            samplePlayback.start (selection, effectiveNote, sampleRate, layerParams.tuneCents);
        }
    }
    else
    {
        auto p = synthParams;
        p.tuneCents += layerParams.tuneCents;
        synthVoice.setParams (p);
        synthVoice.noteOn (effectiveNote, (float) velocity / 127.f);
    }
}

void NovaInstrumentLayer::noteOff() noexcept
{
    layerEnvelope.noteOff();
    filterEnvelope.noteOff();
    if (sourceType == NovaLayerSourceType::Synth)
        synthVoice.noteOff();
}

void NovaInstrumentLayer::reset() noexcept
{
    layerEnvelope.reset();
    filterEnvelope.reset();
    layerFilter.reset();
    samplePlayback.reset();
    synthVoice.reset();
}

void NovaInstrumentLayer::renderAdding (float* outL, float* outR, int numSamples) noexcept
{
    if (! layerEnvelope.isActive()) return;

    const int n = juce::jmin (numSamples, (int) scratchL.size());
    if (n <= 0) return;

    std::fill (scratchL.begin(), scratchL.begin() + n, 0.f);
    std::fill (scratchR.begin(), scratchR.begin() + n, 0.f);

    if (sourceType == NovaLayerSourceType::Sample)
        samplePlayback.renderAdding (scratchL.data(), scratchR.data(), n);
    else
        synthVoice.renderAdding (scratchL.data(), scratchR.data(), n);

    const float gain = juce::Decibels::decibelsToGain (layerParams.volumeDb);
    const float panL = juce::jmin (1.f, 1.f - juce::jmax (0.f, layerParams.pan));
    const float panR = juce::jmin (1.f, 1.f + juce::jmin (0.f, layerParams.pan));

    const bool hasFilterEnvelope = layerParams.filterEnvAmount != 0.f;

    for (int i = 0; i < n; ++i)
    {
        const float env = layerEnvelope.getNextSample();

        if (hasFilterEnvelope)
        {
            const float filterEnvValue = filterEnvelope.getNextSample();
            const float octaveMod = filterEnvValue * layerParams.filterEnvAmount;
            const float modulatedCutoff = juce::jlimit (20.f, 20000.f,
                layerParams.filterCutoffHz * std::pow (2.f, octaveMod));
            layerFilter.setCutoffFrequency (modulatedCutoff);
        }

        const float l = layerFilter.processSample (0, scratchL[(size_t) i]) * env * gain * panL;
        const float r = layerFilter.processSample (1, scratchR[(size_t) i]) * env * gain * panR;
        outL[i] += l;
        outR[i] += r;
    }
}
