#include "PluginProcessor.h"
#include "PluginEditor.h"

NovaMotionFXProcessor::NovaMotionFXProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{}

juce::AudioProcessorValueTreeState::ParameterLayout NovaMotionFXProcessor::createLayout()
{
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
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)block, 2 };
    motionDSP.prepare (spec);
}

void NovaMotionFXProcessor::releaseResources() {}

void NovaMotionFXProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (*apvts.getRawParameterValue ("bypass") > 0.5f) return;

    // Translate APVTS → plain parameter struct (no DSP logic here)
    NovaMotionParameters params;
    params.cutoff    = apvts.getRawParameterValue ("cutoff")->load();
    params.resonance = apvts.getRawParameterValue ("resonance")->load();
    params.feedback  = apvts.getRawParameterValue ("feedback")->load();
    params.delayMix  = apvts.getRawParameterValue ("delay_mix")->load();
    params.reverbMix = apvts.getRawParameterValue ("reverb_mix")->load();
    params.size      = apvts.getRawParameterValue ("size")->load();
    params.inputDb   = apvts.getRawParameterValue ("input")->load();
    params.outputDb  = apvts.getRawParameterValue ("output")->load();
    params.mix       = apvts.getRawParameterValue ("mix")->load();
    // Future-use params (read but not yet wired to DSP)
    params.motion    = apvts.getRawParameterValue ("motion")->load();
    params.drive     = apvts.getRawParameterValue ("drive")->load();
    params.lfoRate   = apvts.getRawParameterValue ("lfo_rate")->load();
    params.lfoDepth  = apvts.getRawParameterValue ("lfo_depth")->load();
    params.decay     = apvts.getRawParameterValue ("decay")->load();

    motionDSP.setParameters (params);
    motionDSP.process (buf);

    // Mirror into legacy atomics so existing editor code keeps working
    peakL.store (motionDSP.getPeakL());
    peakR.store (motionDSP.getPeakR());
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
