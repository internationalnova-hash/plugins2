#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

namespace
{
    constexpr auto alignModeId          = "align_mode";
    constexpr auto alignStrengthId      = "align_strength";
    constexpr auto preserveTransientsId = "preserve_transients";
    constexpr auto smoothingId          = "smoothing";
    constexpr auto phraseSensitivityId  = "phrase_sensitivity";
    constexpr auto maxShiftMsId         = "max_shift_ms";

    constexpr auto fileStateChildId = "novaAlignFiles";
    constexpr auto guidePathProp    = "guidePath";
    constexpr auto targetPathProp   = "targetPath";
}

NovaAlignAudioProcessor::NovaAlignAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        // Guide capture, VocALign-real-time-style: route the
                        // Guide track's output into this sidechain input via
                        // the host's mixer, then arm Guide capture and press
                        // play. Off by default until the host connects it.
                        .withInput  ("Guide Sidechain", juce::AudioChannelSet::stereo(), false)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaAlign"), createParameterLayout()),
      alignedTempFile (juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("NovaAlign_" + juce::Uuid().toString() + ".wav"))
{
}

NovaAlignAudioProcessor::~NovaAlignAudioProcessor()
{
    previewPlayer.stop();
    alignedTempFile.deleteFile();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaAlignAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { alignModeId, 1 },
        "Align Mode",
        juce::StringArray { "Natural", "Tight", "Loose" },
        0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { alignStrengthId, 1 },
        "Align Strength",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        75.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { preserveTransientsId, 1 },
        "Preserve Transients",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { smoothingId, 1 },
        "Smoothing",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        25.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { phraseSensitivityId, 1 },
        "Phrase Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        75.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { maxShiftMsId, 1 },
        "Max Shift",
        50, 3000,
        1000,
        juce::String(),
        [] (int value, int) { return juce::String (value) + " ms"; }));

    return layout;
}

const juce::String NovaAlignAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaAlignAudioProcessor::acceptsMidi() const  { return false; }
bool NovaAlignAudioProcessor::producesMidi() const { return false; }
bool NovaAlignAudioProcessor::isMidiEffect() const { return false; }

double NovaAlignAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaAlignAudioProcessor::getNumPrograms()                { return 1; }
int NovaAlignAudioProcessor::getCurrentProgram()              { return 0; }
void NovaAlignAudioProcessor::setCurrentProgram (int index)   { juce::ignoreUnused (index); }
const juce::String NovaAlignAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}
void NovaAlignAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaAlignAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    substitutionReadPosition = 0;
    wasHostPlaying = false;
    captureEngine.prepare (sampleRate);
}

void NovaAlignAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaAlignAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainIn != mainOut)
        return false;

    // Guide sidechain input: fine disabled, mono, or stereo, independent of
    // the main bus's choice.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.inputBuses.getReference (1);
        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}
#endif

void NovaAlignAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Bus-aware access is required now that there's a second (sidechain)
    // input bus - `buffer` itself spans all buses' channels combined, and
    // main-in/main-out are no longer guaranteed to alias the same memory the
    // way they do for a plugin with only one input bus.
    auto mainInput  = getBusBuffer (buffer, true, 0);
    auto mainOutput = getBusBuffer (buffer, false, 0);
    const bool hasSidechain = getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    auto guideBus = hasSidechain ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();

    bool isPlaying = false;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            isPlaying = position->getIsPlaying();

    captureEngine.processBlock (guideBus, mainInput, isPlaying, getSampleRate());

    auto alignedBuffer = alignJobRunner.getAlignedBuffer();

    if (alignedBuffer == nullptr)
    {
        // No aligned result yet - transparent passthrough of whatever the host
        // is sending (the raw, un-aligned Target audio). Explicit copy since
        // main-in/main-out may not be aliased once a sidechain bus exists.
        for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
        {
            const int srcChannel = juce::jmin (channel, mainInput.getNumChannels() - 1);
            if (mainInput.getNumChannels() > 0)
                mainOutput.copyFrom (channel, 0, mainInput, srcChannel, 0, mainOutput.getNumSamples());
        }
        wasHostPlaying = isPlaying;
        return;
    }

    if (isPlaying && ! wasHostPlaying)
        substitutionReadPosition = 0; // transport just started - play the aligned result from the top

    if (! isPlaying)
    {
        mainOutput.clear();
        wasHostPlaying = isPlaying;
        return;
    }

    const int numSamples = mainOutput.getNumSamples();
    const int alignedChannels = alignedBuffer->getNumChannels();
    const int64_t alignedLength = alignedBuffer->getNumSamples();

    for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
    {
        auto* dest = mainOutput.getWritePointer (channel);
        const auto* source = alignedBuffer->getReadPointer (juce::jmin (channel, alignedChannels - 1));

        for (int i = 0; i < numSamples; ++i)
        {
            const int64_t readPos = substitutionReadPosition + i;
            dest[i] = (readPos >= 0 && readPos < alignedLength) ? source[readPos] : 0.0f;
        }
    }

    substitutionReadPosition += numSamples;
    wasHostPlaying = isPlaying;
}

bool NovaAlignAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaAlignAudioProcessor::createEditor()
{
    return new NovaAlignAudioProcessorEditor (*this);
}

void NovaAlignAudioProcessor::setGuideFile (const juce::File& file)   { guideFile = file; }
void NovaAlignAudioProcessor::setTargetFile (const juce::File& file)  { targetFile = file; }

juce::File NovaAlignAudioProcessor::finishCapture()
{
    const auto slot = captureEngine.getArmedSlot();
    const auto file = captureEngine.finishAndWriteToFile();

    if (file != juce::File {})
    {
        if (slot == NovaAlign::CaptureEngine::Slot::guide)
            setGuideFile (file);
        else if (slot == NovaAlign::CaptureEngine::Slot::target)
            setTargetFile (file);
    }

    return file;
}

NovaAlign::AlignEngine::AlignSettings NovaAlignAudioProcessor::settingsFromApvts() const
{
    NovaAlign::AlignEngine::AlignSettings settings;

    const int mode = (int) apvts.getRawParameterValue (alignModeId)->load();
    static constexpr float tightnessForMode[] = { 0.5f, 0.85f, 0.2f }; // Natural, Tight, Loose
    settings.tightness = tightnessForMode[juce::jlimit (0, 2, mode)];

    settings.amount             = apvts.getRawParameterValue (alignStrengthId)->load() / 100.0f;
    settings.consonantPriority  = apvts.getRawParameterValue (preserveTransientsId)->load() / 100.0f;
    settings.naturalness        = apvts.getRawParameterValue (smoothingId)->load() / 100.0f;
    settings.phraseSensitivity  = apvts.getRawParameterValue (phraseSensitivityId)->load() / 100.0f;
    settings.maxShiftMs         = (int) apvts.getRawParameterValue (maxShiftMsId)->load();

    return settings;
}

void NovaAlignAudioProcessor::requestAlign (std::function<void (NovaAlign::AlignJobRunner::Result)> onComplete)
{
    if (! guideFile.existsAsFile() || ! targetFile.existsAsFile())
    {
        if (onComplete)
            onComplete ({ false, "Load both Guide and Target first.", 0 });
        return;
    }

    alignJobRunner.requestAlign (guideFile, targetFile, settingsFromApvts(),
        [this, onComplete] (NovaAlign::AlignJobRunner::Result result)
        {
            if (result.success)
                renderAlignedTo (alignedTempFile); // keep the preview-file copy in sync
            if (onComplete)
                onComplete (result);
        });
}

bool NovaAlignAudioProcessor::renderAlignedTo (const juce::File& destinationFile) const
{
    auto buffer = alignJobRunner.getAlignedBuffer();
    if (buffer == nullptr)
        return false;

    std::unique_ptr<juce::FileOutputStream> fileStream (destinationFile.createOutputStream());
    if (fileStream == nullptr || ! fileStream->openedOk())
        return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (fileStream.release(), alignJobRunner.getAlignedSampleRate(),
                                   static_cast<unsigned int> (buffer->getNumChannels()), 32, {}, 0));
    if (writer == nullptr)
        return false;

    writer->writeFromAudioSampleBuffer (*buffer, 0, buffer->getNumSamples());
    return true;
}

void NovaAlignAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    auto fileState = state.getOrCreateChildWithName (fileStateChildId, nullptr);
    fileState.setProperty (guidePathProp, guideFile.getFullPathName(), nullptr);
    fileState.setProperty (targetPathProp, targetFile.getFullPathName(), nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaAlignAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xmlState);

    juce::File restoredGuide, restoredTarget;
    auto fileState = state.getChildWithName (fileStateChildId);
    if (fileState.isValid())
    {
        restoredGuide = juce::File (fileState.getProperty (guidePathProp).toString());
        restoredTarget = juce::File (fileState.getProperty (targetPathProp).toString());
        state.removeChild (fileState, nullptr);
    }

    apvts.replaceState (state);

    guideFile = restoredGuide;
    targetFile = restoredTarget;

    // Restore what the user had loaded, but don't silently leave the plugin
    // reverted to raw passthrough - re-run alignment so live substitution is
    // ready again. Deferred via callAsync since setStateInformation can be
    // called from a non-message thread depending on the host.
    if (guideFile.existsAsFile() && targetFile.existsAsFile())
    {
        juce::MessageManager::callAsync ([this]
        {
            requestAlign();
        });
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaAlignAudioProcessor();
}
