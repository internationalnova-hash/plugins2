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
    layout.add (std::make_unique<juce::AudioParameterFloat> ("cutoff",     "Cutoff",     20.f, 20000.f, 2450.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("resonance",  "Resonance",  0.1f, 20.f, 0.707f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("drive",      "Drive",      0.f, 1.f, 0.22f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("feedback",   "Feedback",   0.f, 0.95f, 0.35f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("delay_mix",  "Delay Mix",  0.f, 1.f, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("size",       "Size",       0.f, 1.f, 0.6f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("decay",      "Decay",      0.1f, 10.f, 2.85f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("reverb_mix", "Reverb Mix", 0.f, 1.f, 0.35f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("lfo_rate",   "LFO Rate",   0.01f, 8.f, 0.25f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("lfo_depth",  "LFO Depth",  0.f, 1.f, 0.75f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("input",      "Input",      -24.f, 12.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("output",     "Output",     -24.f, 12.f, 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",        "Mix",        0.f, 1.f, 1.f));

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
    init (smCutoff,    2450.f);
    init (smResonance, 0.707f);
    init (smDrive,     0.22f);
    init (smFeedback,  0.35f);
    init (smDelayMix,  0.4f);
    init (smSize,      0.6f);
    init (smReverbMix, 0.35f);
    init (smInput,     1.f);
    init (smOutput,    1.f);
    init (smMix,       1.f);

    dryBuf.setSize (2, block);
}

void NovaMotionFXProcessor::releaseResources() {}

void NovaMotionFXProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int N = buf.getNumSamples();

    // Update targets
    smCutoff   .setTargetValue (*apvts.getRawParameterValue ("cutoff"));
    smResonance.setTargetValue (*apvts.getRawParameterValue ("resonance"));
    smDrive    .setTargetValue (*apvts.getRawParameterValue ("drive"));
    smFeedback .setTargetValue (*apvts.getRawParameterValue ("feedback"));
    smDelayMix .setTargetValue (*apvts.getRawParameterValue ("delay_mix"));
    smSize     .setTargetValue (*apvts.getRawParameterValue ("size"));
    smReverbMix.setTargetValue (*apvts.getRawParameterValue ("reverb_mix"));
    smInput    .setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("input")->load()));
    smOutput   .setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
    smMix      .setTargetValue (*apvts.getRawParameterValue ("mix"));

    // Save dry
    dryBuf.makeCopyOf (buf);

    // Input gain
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer(ch), smInput.getNextValue(), N);

    // Filter
    filterL.setCutoffFrequency (smCutoff.getNextValue());
    filterL.setResonance       (smResonance.getNextValue());
    filterR.setCutoffFrequency (smCutoff.getCurrentValue());
    filterR.setResonance       (smResonance.getCurrentValue());

    {
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : L;
        for (int i = 0; i < N; ++i) {
            L[i] = filterL.processSample (0, L[i]);
            R[i] = filterR.processSample (0, R[i]);
        }
    }

    // Delay
    {
        const float fb     = smFeedback.getNextValue();
        const float dMix   = smDelayMix.getNextValue();
        const float delaySamples = juce::jmin ((float)maxDelaySamples - 1.f,
                                               (float)(currentSR * 0.25));  // 1/4-note @ 120bpm ~ 0.5s

        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : L;

        for (int i = 0; i < N; ++i) {
            const float dL = delayLineL.popSample (0, delaySamples);
            const float dR = delayLineR.popSample (0, delaySamples);
            delayLineL.pushSample (0, L[i] + dL * fb);
            delayLineR.pushSample (0, R[i] + dR * fb);
            L[i] = L[i] * (1.f - dMix) + dL * dMix;
            R[i] = R[i] * (1.f - dMix) + dR * dMix;
        }
    }

    // Reverb
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = smSize.getNextValue();
        rp.wetLevel   = smReverbMix.getNextValue();
        rp.dryLevel   = 1.f - smReverbMix.getCurrentValue() * 0.5f;
        rp.damping    = 0.45f;
        rp.width      = 1.f;
        reverbL.setParameters (rp); reverbR.setParameters (rp);

        if (buf.getNumChannels() >= 2) {
            reverbL.processStereo (buf.getWritePointer(0), buf.getWritePointer(1), N);
        } else {
            reverbL.processMono (buf.getWritePointer(0), N);
        }
    }

    // Dry/Wet blend
    {
        const float wet = smMix.getNextValue();
        const float dry = 1.f - wet;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
            auto* w = buf.getWritePointer (ch);
            auto* d = dryBuf.getReadPointer (ch);
            for (int i = 0; i < N; ++i) w[i] = w[i]*wet + d[i]*dry;
        }
    }

    // Output gain
    const float outGain = smOutput.getNextValue();
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::multiply (buf.getWritePointer(ch), outGain, N);

    // Peak meters
    peakL.store (buf.getMagnitude (0, 0, N));
    if (buf.getNumChannels() > 1) peakR.store (buf.getMagnitude (1, 0, N));
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
