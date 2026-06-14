#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>

NovaChordAudioProcessor::NovaChordAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    chordSemis.fill (0);
    activeMidiNotes.reserve (8);
}

NovaChordAudioProcessor::~NovaChordAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
NovaChordAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 0 = Triad, 1 = Maj7, 2 = Dom7, 3 = Min7, 4 = Min9, 5 = Maj9,
    // 6 = Sus2, 7 = Sus4, 8 = Add9, 9 = Dim7, 10 = HalfDim, 11 = Aug
    params.push_back (std::make_unique<juce::AudioParameterInt> ("styleIdx", "Chord Style", 0, 11, 1));

    // MIDI octave offset for transposition: -2 to +2
    params.push_back (std::make_unique<juce::AudioParameterInt> ("octaveShift", "Octave Shift", -2, 2, 0));

    // Velocity 1-127
    params.push_back (std::make_unique<juce::AudioParameterInt> ("velocity", "Velocity", 1, 127, 80));

    // Pass-through toggle: when on, original note-on/off are forwarded unchanged
    params.push_back (std::make_unique<juce::AudioParameterBool> ("passThrough", "Pass Through", false));

    return { params.begin(), params.end() };
}

void NovaChordAudioProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // No audio buffering needed — MIDI-only processor
    activeMidiNotes.clear();
    chordSemis.fill (0);
    chordNoteCount = 0;
}

void NovaChordAudioProcessor::releaseResources()
{
    activeMidiNotes.clear();
}

void NovaChordAudioProcessor::buildChordVoicing (int styleIdx,
                                                  std::array<int, 6>& semis,
                                                  int& count)
{
    // Semitone offsets from root for each chord type
    // Matching CHORD_STYLES in index.html exactly
    static const int styles[][6] = {
        { 0, 4, 7, -1, -1, -1 }, // 0 Triad
        { 0, 4, 7, 11, -1, -1 }, // 1 Maj7
        { 0, 4, 7, 10, -1, -1 }, // 2 Dom7
        { 0, 3, 7, 10, -1, -1 }, // 3 Min7
        { 0, 3, 7, 10, 14, -1 }, // 4 Min9
        { 0, 4, 7, 11, 14, -1 }, // 5 Maj9
        { 0, 2, 7, -1, -1, -1 }, // 6 Sus2
        { 0, 5, 7, -1, -1, -1 }, // 7 Sus4
        { 0, 4, 7, 14, -1, -1 }, // 8 Add9
        { 0, 3, 6,  9, -1, -1 }, // 9 Dim7
        { 0, 3, 6, 10, -1, -1 }, // 10 HalfDim
        { 0, 4, 8, -1, -1, -1 }, // 11 Aug
    };

    const int idx = juce::jlimit (0, 11, styleIdx);
    const int* row = styles[idx];
    count = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (row[i] < 0) break;
        semis[(size_t)i] = row[i];
        ++count;
    }
}

void NovaChordAudioProcessor::processBlock (juce::AudioBuffer<float>& /*buffer*/,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int styleIdx   = (int) apvts.getRawParameterValue ("styleIdx")->load();
    const int octShift   = (int) apvts.getRawParameterValue ("octaveShift")->load();
    const int vel        = juce::jlimit (1, 127, (int) apvts.getRawParameterValue ("velocity")->load());
    const bool passThru  = apvts.getRawParameterValue ("passThrough")->load() > 0.5f;

    juce::MidiBuffer outBuffer;

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        const int samplePos = meta.samplePosition;

        if (passThru)
        {
            outBuffer.addEvent (msg, samplePos);
            continue;
        }

        if (msg.isNoteOn())
        {
            const int root = msg.getNoteNumber() + octShift * 12;

            // Send note-offs for any still-ringing chord notes
            for (int note : activeMidiNotes)
            {
                outBuffer.addEvent (
                    juce::MidiMessage::noteOff (msg.getChannel(), note),
                    samplePos);
            }
            activeMidiNotes.clear();

            // Build and send the new chord
            buildChordVoicing (styleIdx, chordSemis, chordNoteCount);
            lastNoteOn.store (msg.getNoteNumber());
            chordRootMidi.store (root);
            chordActive.store (true);

            for (int i = 0; i < chordNoteCount; ++i)
            {
                const int midiNote = juce::jlimit (0, 127, root + chordSemis[(size_t)i]);
                outBuffer.addEvent (
                    juce::MidiMessage::noteOn (msg.getChannel(), midiNote, (juce::uint8) vel),
                    samplePos);
                activeMidiNotes.push_back (midiNote);
            }
        }
        else if (msg.isNoteOff())
        {
            const int root = msg.getNoteNumber() + octShift * 12;
            // Only release chord if this was the note that triggered it
            if (msg.getNoteNumber() == lastNoteOn.load())
            {
                for (int note : activeMidiNotes)
                {
                    outBuffer.addEvent (
                        juce::MidiMessage::noteOff (msg.getChannel(), note),
                        samplePos);
                }
                activeMidiNotes.clear();
                chordActive.store (false);
            }
            juce::ignoreUnused (root);
        }
        else
        {
            // Forward all other MIDI (CC, pitch bend, etc.)
            outBuffer.addEvent (msg, samplePos);
        }
    }

    midiMessages.swapWith (outBuffer);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaChordAudioProcessor::isBusesLayoutSupported (const BusesLayout& /*layouts*/) const
{
    // MIDI-only: no audio buses needed
    return true;
}
#endif

juce::AudioProcessorEditor* NovaChordAudioProcessor::createEditor()
{
    return new NovaChordAudioProcessorEditor (*this);
}

bool NovaChordAudioProcessor::hasEditor() const { return true; }

const juce::String NovaChordAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaChordAudioProcessor::acceptsMidi()  const { return true; }
bool NovaChordAudioProcessor::producesMidi() const { return true; }
bool NovaChordAudioProcessor::isMidiEffect() const { return true; }
double NovaChordAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaChordAudioProcessor::getNumPrograms()                              { return 1; }
int NovaChordAudioProcessor::getCurrentProgram()                           { return 0; }
void NovaChordAudioProcessor::setCurrentProgram (int)                      {}
const juce::String NovaChordAudioProcessor::getProgramName (int)           { return {}; }
void NovaChordAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaChordAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaChordAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaChordAudioProcessor();
}
