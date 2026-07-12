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
    const int N   = buf.getNumSamples();
    const int nCh = buf.getNumChannels();

    if (*apvts.getRawParameterValue ("bypass") > 0.5f) return;

    // Read parameters directly — no SmoothedValue intermediary for stage gating
    const float cutoffHz  = *apvts.getRawParameterValue ("cutoff");
    const float resonance = *apvts.getRawParameterValue ("resonance");
    const float delayMix  = *apvts.getRawParameterValue ("delay_mix");
    const float feedback  = *apvts.getRawParameterValue ("feedback");
    const float reverbMix = *apvts.getRawParameterValue ("reverb_mix");
    const float roomSize  = *apvts.getRawParameterValue ("size");
    const float wetMix    = *apvts.getRawParameterValue ("mix");
    const float inGain    = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("input")->load());
    const float outGain   = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());

    const float safeMaxCutoff = (float)(currentSR * 0.40f);
    const bool  filterActive  = cutoffHz < safeMaxCutoff - 100.f;
    const bool  delayActive   = delayMix  > 0.001f;
    const bool  reverbActive  = reverbMix > 0.001f;
    const bool  anyDsp        = filterActive || delayActive || reverbActive;

    // Fast passthrough when no DSP active — identical to confirmed-working pure passthrough
    if (!anyDsp)
    {
        // Only reset DSP state on the block where we transition from active → inactive
        if (prevFilterActive) { filterL.reset(); filterR.reset(); }
        if (prevDelayActive)  { delayLineL.reset(); delayLineR.reset(); }
        if (prevReverbActive) { reverbL.reset(); }
        prevFilterActive = prevDelayActive = prevReverbActive = false;

        for (int ch = 0; ch < nCh; ++ch)
            juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), inGain * outGain, N);
        peakL.store (buf.getMagnitude (0, 0, N));
        if (nCh > 1) peakR.store (buf.getMagnitude (1, 0, N));
        return;
    }

    // Save dry for wet/dry blend
    dryBuf.makeCopyOf (buf, true);

    // Input gain
    for (int ch = 0; ch < nCh; ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), inGain, N);

    // Filter
    if (filterActive)
    {
        const float safeCut = juce::jmin (cutoffHz, safeMaxCutoff);
        filterL.setCutoffFrequency (safeCut); filterL.setResonance (resonance);
        filterR.setCutoffFrequency (safeCut); filterR.setResonance (resonance);
        auto* L = buf.getWritePointer (0);
        auto* R = nCh > 1 ? buf.getWritePointer (1) : L;
        for (int i = 0; i < N; ++i) {
            L[i] = filterL.processSample (0, L[i]);
            R[i] = filterR.processSample (0, R[i]);
            if (!std::isfinite (L[i]) || !std::isfinite (R[i])) {
                filterL.reset(); filterR.reset();
                L[i] = R[i] = 0.f;
            }
        }
    }
    else if (prevFilterActive) { filterL.reset(); filterR.reset(); }

    // Delay
    if (delayActive)
    {
        const float delaySamples = juce::jmin ((float)maxDelaySamples - 1.f,
                                               (float)(currentSR * 0.25));
        auto* L = buf.getWritePointer (0);
        auto* R = nCh > 1 ? buf.getWritePointer (1) : L;
        for (int i = 0; i < N; ++i) {
            const float dL = delayLineL.popSample (0, delaySamples);
            const float dR = delayLineR.popSample (0, delaySamples);
            delayLineL.pushSample (0, L[i] + dL * feedback);
            delayLineR.pushSample (0, R[i] + dR * feedback);
            L[i] = L[i] * (1.f - delayMix) + dL * delayMix;
            R[i] = R[i] * (1.f - delayMix) + dR * delayMix;
        }
    }
    else if (prevDelayActive) { delayLineL.reset(); delayLineR.reset(); }

    // Reverb
    if (reverbActive)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = roomSize;
        rp.wetLevel = reverbMix;
        rp.dryLevel = 1.f - reverbMix * 0.5f;
        rp.damping  = 0.45f;
        rp.width    = 1.f;
        reverbL.setParameters (rp);
        if (nCh >= 2)
            reverbL.processStereo (buf.getWritePointer(0), buf.getWritePointer(1), N);
        else
            reverbL.processMono (buf.getWritePointer(0), N);
    }
    else if (prevReverbActive) { reverbL.reset(); }

    prevFilterActive = filterActive;
    prevDelayActive  = delayActive;
    prevReverbActive = reverbActive;

    // Wet/dry blend
    if (wetMix < 0.999f)
    {
        const float dry = 1.f - wetMix;
        for (int ch = 0; ch < nCh; ++ch) {
            auto* w = buf.getWritePointer (ch);
            auto* d = dryBuf.getReadPointer (ch);
            for (int i = 0; i < N; ++i) w[i] = w[i] * wetMix + d[i] * dry;
        }
    }

    // Output gain
    for (int ch = 0; ch < nCh; ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer (ch), outGain, N);

    // NaN/inf safety net — catch any remaining blowup before it silences the output
    for (int ch = 0; ch < nCh; ++ch) {
        auto* data = buf.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
            if (!std::isfinite (data[i])) data[i] = 0.f;
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
