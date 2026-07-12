#include "PluginProcessor.h"
#include "PluginEditor.h"

NovaMotionFXProcessor::NovaMotionFXProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      delayLineL (192000), delayLineR (192000)
{}

juce::AudioProcessorValueTreeState::ParameterLayout NovaMotionFXProcessor::createLayout()
{
    using P = juce::AudioProcessorValueTreeState;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("motion",     "Motion",     0.f, 1.f, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("cutoff",     "Cutoff",     20.f, 20000.f, 20000.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("resonance",  "Resonance",  0.1f, 20.f, 0.707f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("drive",      "Drive",      0.f, 1.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("feedback",   "Feedback",   0.f, 0.95f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("delay_mix",  "Delay Mix",  0.f, 1.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("size",       "Size",       0.f, 1.f, 0.6f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("decay",      "Decay",      0.1f, 10.f, 2.85f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("reverb_mix", "Reverb Mix", 0.f, 1.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("lfo_rate",   "LFO Rate",   0.01f, 8.f, 0.25f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("lfo_depth",  "LFO Depth",  0.f, 1.f, 0.75f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("input",      "Input",      -24.f, 12.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("output",     "Output",     -24.f, 12.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",        0.f, 1.f, 1.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("bypass",     "Bypass",     0.f, 1.f, 0.f));

    return layout;
}

void NovaMotionFXProcessor::prepareToPlay (double sr, int block)
{
    currentSR = sr;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32)block, 2 };

    filterL.prepare (spec); filterL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.prepare (spec); filterR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    maxDelaySamples = (int)(sr * 2.0);
    delayLineL.prepare (spec); delayLineL.setMaximumDelayInSamples (maxDelaySamples);
    delayLineR.prepare (spec); delayLineR.setMaximumDelayInSamples (maxDelaySamples);

    reverbL.reset(); reverbR.reset();

    auto init = [&](juce::SmoothedValue<float>& sv, float v) { sv.reset (sr, 0.02); sv.setCurrentAndTargetValue (v); };
    init (smCutoff,    20000.f);
    init (smResonance, 0.707f);
    init (smDrive,     0.f);
    init (smFeedback,  0.f);
    init (smDelayMix,  0.f);
    init (smSize,      0.6f);
    init (smReverbMix, 0.f);
    init (smInput,     1.f);
    init (smOutput,    1.f);
    init (smMix,       1.f);

    dryBuf.setSize (2, block);
}

void NovaMotionFXProcessor::releaseResources() {}

void NovaMotionFXProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (*apvts.getRawParameterValue ("bypass") > 0.5f) return;

    const int N   = buf.getNumSamples();
    const int nCh = buf.getNumChannels();
    float* dataL  = buf.getWritePointer (0);
    float* dataR  = nCh > 1 ? buf.getWritePointer (1) : dataL;

    // Read params once per block
    const float cutoffHz  = *apvts.getRawParameterValue ("cutoff");
    const float resonance = *apvts.getRawParameterValue ("resonance");
    const float delayMix  = *apvts.getRawParameterValue ("delay_mix");
    const float feedback  = *apvts.getRawParameterValue ("feedback");
    const float reverbMix = *apvts.getRawParameterValue ("reverb_mix");
    const float roomSize  = *apvts.getRawParameterValue ("size");
    const float masterMix = *apvts.getRawParameterValue ("mix");
    const float inGain    = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("input")->load());
    const float outGain   = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());

    const float safeMaxCutoff = (float)(currentSR * 0.40f);
    const bool  filterOn = cutoffHz < safeMaxCutoff - 100.f;
    const bool  delayOn  = delayMix  > 0.001f;
    const bool  reverbOn = reverbMix > 0.001f;

    // One-time reset on stage transition
    if (!filterOn && prevFilterActive) { filterL.reset(); filterR.reset(); }
    if (!delayOn  && prevDelayActive)  { delayLineL.reset(); delayLineR.reset(); }
    if (!reverbOn && prevReverbActive) { reverbL.reset(); }
    prevFilterActive = filterOn;
    prevDelayActive  = delayOn;
    prevReverbActive = reverbOn;

    // Apply input gain
    for (int ch = 0; ch < nCh; ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), inGain, N);

    // Save dry for master wet/dry blend
    dryBuf.setSize (nCh, N, false, false, true);
    dryBuf.makeCopyOf (buf, true);

    // Filter — coefficients set ONCE per block, processSample per sample
    if (filterOn)
    {
        const float safeCut = juce::jmin (cutoffHz, safeMaxCutoff);
        filterL.setCutoffFrequency (safeCut); filterL.setResonance (resonance);
        filterR.setCutoffFrequency (safeCut); filterR.setResonance (resonance);
        for (int i = 0; i < N; ++i) {
            dataL[i] = filterL.processSample (0, dataL[i]);
            dataR[i] = filterR.processSample (0, dataR[i]);
        }
    }

    // Delay — per sample
    if (delayOn)
    {
        const float ds = juce::jmin ((float)maxDelaySamples - 1.f, (float)(currentSR * 0.25));
        for (int i = 0; i < N; ++i) {
            const float dL = delayLineL.popSample (0, ds);
            const float dR = delayLineR.popSample (0, ds);
            delayLineL.pushSample (0, dataL[i] + dL * feedback);
            delayLineR.pushSample (0, dataR[i] + dR * feedback);
            dataL[i] = dataL[i] * (1.f - delayMix) + dL * delayMix;
            dataR[i] = dataR[i] * (1.f - delayMix) + dR * delayMix;
        }
    }

    // Reverb — process full block at once (juce::Reverb is block-based, not per-sample)
    if (reverbOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = roomSize;
        rp.wetLevel = reverbMix;
        rp.dryLevel = 1.f - reverbMix;
        rp.damping  = 0.45f;
        rp.width    = 1.f;
        rp.freezeMode = 0.f;
        reverbL.setParameters (rp);
        if (nCh >= 2)
            reverbL.processStereo (dataL, dataR, N);
        else
            reverbL.processMono (dataL, N);
    }

    // Master wet/dry blend
    if (masterMix < 0.999f)
    {
        const float dry = 1.f - masterMix;
        for (int ch = 0; ch < nCh; ++ch) {
            auto* w = buf.getWritePointer (ch);
            const auto* d = dryBuf.getReadPointer (ch);
            for (int i = 0; i < N; ++i)
                w[i] = w[i] * masterMix + d[i] * dry;
        }
    }

    // Output gain + hard limiter — prevents any blowup from ever silencing output
    for (int i = 0; i < N; ++i) {
        dataL[i] = juce::jlimit (-1.f, 1.f, dataL[i] * outGain);
        dataR[i] = juce::jlimit (-1.f, 1.f, dataR[i] * outGain);
    }

    peakL.store (buf.getMagnitude (0, 0, N));
    if (nCh > 1) peakR.store (buf.getMagnitude (1, 0, N));
}

juce::AudioProcessorEditor* NovaMotionFXProcessor::createEditor()
{
    return new NovaMotionFXEditor (*this);
}

void NovaMotionFXProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void NovaMotionFXProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaMotionFXProcessor(); }
