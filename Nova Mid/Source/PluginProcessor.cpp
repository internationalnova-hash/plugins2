#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::vector<MidPreset>& midPresets()
{
    static const std::vector<MidPreset> presets {
        { "Default",            0.0f,   0.0f,  0.0f },
        { "Vocal Weight",       6.0f,  -3.0f,  0.0f },
        { "Thick Lead Vocal",   9.0f,  -6.0f,  0.0f },
        { "Rap Vocal Forward",  8.0f,  -4.5f,  0.0f },
        { "808 Body",          10.0f,  -4.0f,  0.0f },
        { "Kick and Snare Mass", 7.0f, -5.0f,  0.0f },
        { "Drum Bus Density",   6.5f,  -4.0f,  0.0f },
        { "Loop Glue",          5.0f,  -2.0f,  0.0f },
        { "Guitar Chunk",       8.0f,  -3.5f,  0.0f },
        { "Mix Bus Weight",     3.0f,  -1.0f,  0.0f },
        { "Heavy Hitter",      14.0f,  -8.0f,  0.0f },
        { "Full Weight",       20.0f, -12.0f,  0.0f },
    };
    return presets;
}

NovaMidAudioProcessor::NovaMidAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NovaMid", createLayout())
{
    pushParam    = apvts.getRawParameterValue ("push");
    ceilingParam = apvts.getRawParameterValue ("ceiling");
    outputParam  = apvts.getRawParameterValue ("output");
    bypassParam  = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaMidAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto db   = [] (float v, int) { return String (v, 1) + " dB"; };
    auto dbfs = [] (float v, int) { return String (v, 1) + " dBFS"; };

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "push", 1 }, "Push",
               NormalisableRange<float> (0.0f, nm::kPushMaxDb, 0.1f), 0.0f,
               AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (db)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling",
               NormalisableRange<float> (nm::kCeilingMinDb, nm::kCeilingMaxDb, 0.1f), 0.0f,
               AudioParameterFloatAttributes().withLabel ("dBFS").withStringFromValueFunction (dbfs)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
               NormalisableRange<float> (-12.0f, 0.0f, 0.1f), 0.0f,
               AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (db)));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

bool NovaMidAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void NovaMidAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int nCh = juce::jmax (1, getTotalNumOutputChannels());
    engine.setPushDb (pushParam->load());
    engine.setCeilingDb (ceilingParam->load());
    engine.setOutputDb (outputParam->load());
    engine.prepare (sampleRate, samplesPerBlock, nCh);

    const int lat = engine.getLatencySamples();
    setLatencySamples (lat);

    dryBuffer.setSize (nCh, juce::jmax (1, samplesPerBlock), false, true, false);
    delayRing.setSize (nCh, lat + 1, false, true, false);
    delayRing.clear();
    ringPos = 0;

    wetAmount.reset (sampleRate, 0.03);
    wetAmount.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
}

void NovaMidAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    const int n   = buffer.getNumSamples();
    const int nCh = juce::jmin (buffer.getNumChannels(), delayRing.getNumChannels());
    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (n == 0 || nCh == 0)
        return;

    // Read params once per block.
    engine.setPushDb (pushParam->load());
    engine.setCeilingDb (ceilingParam->load());
    engine.setOutputDb (outputParam->load());
    wetAmount.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    float inPk = 0.0f;
    for (int ch = 0; ch < nCh; ++ch)
        inPk = juce::jmax (inPk, buffer.getMagnitude (ch, 0, n));

    // Latency-matched dry copy (for bypass).
    dryBuffer.setSize (nCh, n, false, false, true);
    const int ringLen = delayRing.getNumSamples();
    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        float* ring = delayRing.getWritePointer (ch);
        float* dry  = dryBuffer.getWritePointer (ch);
        int p = ringPos;
        for (int i = 0; i < n; ++i)
        {
            ring[p] = in[i];
            p = (p + 1) % ringLen;
            dry[i] = ring[p]; // oldest sample = input delayed by ringLen-1 = latency
        }
    }
    ringPos = (ringPos + n) % ringLen;

    engine.process (buffer.getArrayOfWritePointers(), nCh, n);
    overDb.store (juce::jmax (overDb.load(), engine.getLastOverDb()));

    if (wetAmount.isSmoothing() || wetAmount.getTargetValue() < 0.5f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float w = wetAmount.getNextValue();
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* out = buffer.getWritePointer (ch);
                out[i] = out[i] * w + dryBuffer.getSample (ch, i) * (1.0f - w);
            }
        }
    }

    float outPk = 0.0f;
    for (int ch = 0; ch < nCh; ++ch)
        outPk = juce::jmax (outPk, buffer.getMagnitude (ch, 0, n));

    inPeak.store (juce::jmax (inPeak.load(), inPk));
    outPeak.store (juce::jmax (outPeak.load(), outPk));
}

//==============================================================================
int NovaMidAudioProcessor::getNumPrograms() { return (int) midPresets().size(); }

const juce::String NovaMidAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (midPresets()[(size_t) index].name) : juce::String();
}

void NovaMidAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    const auto& p = midPresets()[(size_t) index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("push", p.push);
    set ("ceiling", p.ceiling);
    set ("output", p.output);
}

void NovaMidAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaMidAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* NovaMidAudioProcessor::createEditor()
{
    return new NovaMidAudioProcessorEditor (*this);
}
