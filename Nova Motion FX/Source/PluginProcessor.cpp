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
    float* dataL  = buf.getWritePointer (0);
    float* dataR  = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : dataL;

    // Read on/off bools — stages never run unless explicitly active
    const bool filterOn = *apvts.getRawParameterValue ("cutoff") < (float)(currentSR * 0.40) - 100.f;
    const bool delayOn  = *apvts.getRawParameterValue ("delay_mix")  > 0.001f;
    const bool reverbOn = *apvts.getRawParameterValue ("reverb_mix") > 0.001f;

    // Parameter targets
    smCutoff   .setTargetValue (*apvts.getRawParameterValue ("cutoff"));
    smResonance.setTargetValue (*apvts.getRawParameterValue ("resonance"));
    smFeedback .setTargetValue (*apvts.getRawParameterValue ("feedback"));
    smDelayMix .setTargetValue (*apvts.getRawParameterValue ("delay_mix"));
    smSize     .setTargetValue (*apvts.getRawParameterValue ("size"));
    smReverbMix.setTargetValue (*apvts.getRawParameterValue ("reverb_mix"));
    smInput    .setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("input")->load()));
    smOutput   .setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
    smMix      .setTargetValue (*apvts.getRawParameterValue ("mix"));

    // One-time reset when a stage turns off
    if (!filterOn && prevFilterActive) { filterL.reset(); filterR.reset(); }
    if (!delayOn  && prevDelayActive)  { delayLineL.reset(); delayLineR.reset(); }
    if (!reverbOn && prevReverbActive) { reverbL.reset(); }
    prevFilterActive = filterOn;
    prevDelayActive  = delayOn;
    prevReverbActive = reverbOn;

    // Reverb params (set once per block, outside per-sample loop)
    if (reverbOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = smSize.getCurrentValue();
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.damping  = 0.45f;
        rp.width    = 1.f;
        reverbL.setParameters (rp);
    }

    float peakOutL = 0.f, peakOutR = 0.f;

    for (int n = 0; n < N; ++n)
    {
        const float inGain  = smInput.getNextValue();
        float L = dataL[n] * inGain;
        float R = dataR[n] * inGain;
        const float dryL = L, dryR = R;

        // Filter
        if (filterOn)
        {
            const float cut = juce::jmin (smCutoff.getNextValue(), (float)(currentSR * 0.40));
            const float res = smResonance.getNextValue();
            const float g   = std::tan (juce::MathConstants<float>::pi * cut / (float)currentSR);
            const float R2  = 1.f - juce::jlimit (0.f, 0.97f, (res - 0.1f) / 19.9f);
            filterL.setCutoffFrequency (cut); filterL.setResonance (res);
            filterR.setCutoffFrequency (cut); filterR.setResonance (res);
            L = filterL.processSample (0, L);
            R = filterR.processSample (0, R);
        }

        // Delay
        if (delayOn)
        {
            const float fb   = smFeedback.getNextValue();
            const float dMix = smDelayMix.getNextValue();
            const float ds   = juce::jmin ((float)maxDelaySamples - 1.f, (float)(currentSR * 0.25));
            const float dL   = delayLineL.popSample (0, ds);
            const float dR   = delayLineR.popSample (0, ds);
            delayLineL.pushSample (0, L + dL * fb);
            delayLineR.pushSample (0, R + dR * fb);
            L = L * (1.f - dMix) + dL * dMix;
            R = R * (1.f - dMix) + dR * dMix;
        }

        // Reverb (wet signal only — blended below)
        float revL = L, revR = R;
        if (reverbOn)
        {
            // processStereo expects arrays; process single sample via mono trick
            float tmpL = L, tmpR = R;
            reverbL.processStereo (&tmpL, &tmpR, 1);
            const float revMix = smReverbMix.getNextValue();
            revL = L * (1.f - revMix) + tmpL * revMix;
            revR = R * (1.f - revMix) + tmpR * revMix;
            smSize.getNextValue(); // keep smoother advancing
        }
        else
        {
            revL = L; revR = R;
            smReverbMix.getNextValue();
            smSize.getNextValue();
        }

        // Master wet/dry blend
        const float wet  = smMix.getNextValue();
        const float outL = dryL * (1.f - wet) + revL * wet;
        const float outR = dryR * (1.f - wet) + revR * wet;

        // Output gain + hard limiter (same as Juice Gang)
        const float outGain = smOutput.getNextValue();
        dataL[n] = juce::jlimit (-1.f, 1.f, outL * outGain);
        dataR[n] = juce::jlimit (-1.f, 1.f, outR * outGain);

        peakOutL = std::max (peakOutL, std::abs (dataL[n]));
        peakOutR = std::max (peakOutR, std::abs (dataR[n]));
    }

    peakL.store (peakOutL);
    peakR.store (peakOutR);
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
