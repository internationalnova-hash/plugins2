#include "PluginProcessor.h"
#include "PluginEditor.h"

NovaLinkAudioProcessor::NovaLinkAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Talkback", juce::AudioChannelSet::stereo(), false))
{
    displayName = juce::SystemStats::getFullUserName().upToFirstOccurrenceOf(" ", false, false);
    if (displayName.isEmpty()) displayName = juce::SystemStats::getLogonName();
}

NovaLinkAudioProcessor::~NovaLinkAudioProcessor()
{
    video.stop();
    session.leave();
}

bool NovaLinkAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void NovaLinkAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    session.prepare(sampleRate, juce::jmax(samplesPerBlock, 4096));
}

void NovaLinkAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: audio passes through, no session
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer(buffer, true, 0);
    juce::AudioBuffer<float> sidechainView;
    const juce::AudioBuffer<float>* sidechain = nullptr;
    if (getBusCount(true) > 1 && getBus(true, 1) != nullptr && getBus(true, 1)->isEnabled())
    {
        sidechainView = getBusBuffer(buffer, true, 1);
        if (sidechainView.getNumChannels() > 0) sidechain = &sidechainView;
    }

    NovaLink::HostPosition pos;
    bool hasPos = false;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition())
        {
            hasPos = true;
            pos.playing = info->getIsPlaying();
            pos.recording = info->getIsRecording();
            if (auto t = info->getTimeInSeconds()) pos.seconds = *t;
            else if (auto s = info->getTimeInSamples()) pos.seconds = (double) *s / getSampleRate();
            if (auto q = info->getPpqPosition()) pos.ppq = *q;
            if (auto b = info->getBpm()) pos.bpm = *b;
            if (auto ts = info->getTimeSignature()) { pos.num = ts->numerator; pos.den = ts->denominator; }
        }
    hostPlaying.store(pos.playing);
    hostRecording.store(pos.recording);

    // Output channels beyond the main bus are silent.
    for (int ch = main.getNumChannels(); ch < buffer.getNumChannels(); ++ch)
        if (ch >= getTotalNumInputChannels()) buffer.clear(ch, 0, buffer.getNumSamples());

    session.processBlock(main, sidechain, pos, hasPos);
}

juce::AudioProcessorEditor* NovaLinkAudioProcessor::createEditor()
{
    return new NovaLinkAudioProcessorEditor(*this);
}

void NovaLinkAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree s("NovaLink");
    s.setProperty("name", displayName, nullptr);
    juce::MemoryOutputStream out(destData, false);
    s.writeToStream(out);
}

void NovaLinkAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto s = juce::ValueTree::readFromData(data, (size_t) sizeInBytes);
    if (s.isValid() && s.hasProperty("name"))
        displayName = s.getProperty("name").toString();
}
