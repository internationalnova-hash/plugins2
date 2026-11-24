#include "PluginProcessor.h"
#include "PluginEditor.h"

NovaMeterAudioProcessor::NovaMeterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    loadProfile ("Streaming Master");
}

bool NovaMeterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return in == layouts.getMainOutputChannelSet();
}

void NovaMeterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    meterEngine.prepare (sampleRate, getTotalNumInputChannels());
    doubleScratch.setSize (getTotalNumInputChannels(), samplesPerBlock);
}

void NovaMeterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;
    // Measurement only: the audio passes through untouched.
    meterEngine.process (buffer.getArrayOfReadPointers(), getTotalNumInputChannels(), buffer.getNumSamples());
}

void NovaMeterAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nc = getTotalNumInputChannels(), n = buffer.getNumSamples();
    if (doubleScratch.getNumSamples() < n || doubleScratch.getNumChannels() < nc)
        return; // host exceeded its promised block size: skip metering rather than allocate

    for (int ch = 0; ch < nc; ++ch)
    {
        auto* dst = doubleScratch.getWritePointer (ch);
        const auto* src = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            dst[i] = (float) src[i];
    }
    meterEngine.process (doubleScratch.getArrayOfReadPointers(), nc, n);
}

//==============================================================================
bool NovaMeterAudioProcessor::collectReference()
{
    tz::ReferenceStats r;
    if (! referenceAnalyzer.takeResult (r))
        return false;
    reference = r;
    return true;
}

bool NovaMeterAudioProcessor::isModified() const
{
    if (const auto* p = profiles.find (activeProfile))
        return ! working.sameTargetsAs (*p);
    return true;
}

void NovaMeterAudioProcessor::loadProfile (const juce::String& name)
{
    if (const auto* p = profiles.find (name))
    {
        working = *p;
        activeProfile = name;
    }
    else if (activeProfile.isEmpty())
    {
        working = tz::Profile::defaults ("Default");
        activeProfile = "Default";
    }
}

void NovaMeterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("version", 1);
    obj->setProperty ("activeProfile", activeProfile);
    obj->setProperty ("view", viewName);
    obj->setProperty ("working", working.toVar());
    if (reference.valid())
        obj->setProperty ("reference", reference.toVar());

    destData.reset();
    juce::MemoryOutputStream out (destData, false);
    out.writeString (juce::JSON::toString (juce::var (obj), true));
}

void NovaMeterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream in (data, (size_t) sizeInBytes, false);
    const auto v = juce::JSON::parse (in.readString());
    if (! v.isObject())
        return;

    activeProfile = v.getProperty ("activeProfile", "").toString();
    viewName = v.getProperty ("view", "live").toString();
    if (viewName != "targets" && viewName != "reference")
        viewName = "live";
    // Targets saved in the session win, even if the shared profile file changed since.
    working = tz::Profile::fromVar (v.getProperty ("working", {}));
    reference = tz::ReferenceStats::fromVar (v.getProperty ("reference", {}));
    ++stateGeneration;
}

juce::AudioProcessorEditor* NovaMeterAudioProcessor::createEditor()
{
    return new NovaMeterAudioProcessorEditor (*this);
}
