#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>

NovaChordAudioProcessor::NovaChordAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
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

    // Built-in synth voice character: 0=Keys 1=Pad 2=Pluck 3=Organ 4=Bell
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("voiceType", "Voice",
        juce::StringArray { "Keys", "Pad", "Pluck", "Organ", "Bell" }, 0));

    return { params.begin(), params.end() };
}

void NovaChordAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    activeMidiNotes.clear();
    chordSemis.fill (0);
    chordNoteCount = 0;
    for (auto& v : previewVoices)
        v = {};
    previewPending.store (false);

    for (auto& v : synthVoices)
        v = {};
}

void NovaChordAudioProcessor::requestChordPreview (int rootMidiNote, const std::array<int, 6>& semis, int count, int velocity) noexcept
{
    const int clampedCount = juce::jlimit (0, 6, count);
    for (int i = 0; i < 6; ++i)
        previewSemis[(size_t) i].store (i < clampedCount ? semis[(size_t) i] : -1);

    previewRoot.store (juce::jlimit (0, 127, rootMidiNote));
    previewCount.store (clampedCount);
    previewVelocity.store (juce::jlimit (1, 127, velocity));
    previewPending.store (true);
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

namespace
{
    // Additive-synthesis character per Voice/Timbre selection: up to 4
    // partials (ratio relative to the fundamental + relative amplitude),
    // plus its own ADSR shape. Distinct, real, synthesized characters -
    // not samples - but genuinely different from each other, not just a
    // generic tone.
    struct VoiceTimbre
    {
        float ratio[4];
        float amp[4];
        float attackMs, decayMs, sustainLevel, releaseMs;
    };

    static const VoiceTimbre kTimbres[5] = {
        // 0 Keys: bright percussive attack settling to a moderate sustain -
        // electric-piano-ish.
        { { 1.0f, 2.0f, 3.0f, 4.0f }, { 1.0f, 0.35f, 0.18f, 0.08f }, 3.0f, 220.0f, 0.55f, 260.0f },
        // 1 Pad: slow, smooth, mostly-fundamental swell with a long tail.
        { { 1.0f, 2.0f, 3.0f, 0.0f }, { 1.0f, 0.15f, 0.05f, 0.0f }, 220.0f, 400.0f, 0.85f, 700.0f },
        // 2 Pluck: fast attack/decay to a low sustain - plucked-string feel.
        { { 1.0f, 2.0f, 3.0f, 4.0f }, { 1.0f, 0.5f, 0.3f, 0.15f }, 2.0f, 180.0f, 0.12f, 120.0f },
        // 3 Organ: strong stacked harmonics, holds at full level while
        // pressed (drawbar-style), quick release.
        { { 1.0f, 2.0f, 3.0f, 4.0f }, { 1.0f, 0.6f, 0.45f, 0.3f }, 5.0f, 40.0f, 1.0f, 90.0f },
        // 4 Bell: inharmonic partials, long decay-only shape (sustain=0).
        { { 1.0f, 2.76f, 5.4f, 8.9f }, { 1.0f, 0.5f, 0.25f, 0.12f }, 2.0f, 900.0f, 0.0f, 1200.0f },
    };

    float msToCoeff (float ms, double sampleRate) noexcept
    {
        const float samples = juce::jmax (1.0f, ms * 0.001f * (float) sampleRate);
        return std::exp (-1.0f / samples);
    }
}

void NovaChordAudioProcessor::startSynthVoice (int midiNote, int velocity) noexcept
{
    // Steal an inactive slot for this note, or re-steal any existing voice
    // already sounding that exact note (avoids doubled voices on retrigger).
    int slot = -1;
    for (int i = 0; i < maxSynthVoices; ++i)
        if (synthVoices[(size_t) i].midiNote == midiNote) { slot = i; break; }
    if (slot < 0)
        for (int i = 0; i < maxSynthVoices; ++i)
            if (! synthVoices[(size_t) i].active) { slot = i; break; }
    if (slot < 0) slot = 0; // pool exhausted: steal the oldest slot

    const int voiceTypeIdx = juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("voiceType")->load());
    const auto& t = kTimbres[(size_t) voiceTypeIdx];

    auto& v = synthVoices[(size_t) slot];
    v.active = true;
    v.midiNote = midiNote;
    v.phase = 0.0;
    const double freqHz = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
    v.phaseIncrement = juce::MathConstants<double>::twoPi * freqHz / currentSampleRate;

    for (int h = 0; h < 4; ++h)
    {
        // Guard against aliasing: drop partials that would exceed Nyquist.
        const bool withinNyquist = (freqHz * t.ratio[h]) < (currentSampleRate * 0.45);
        v.harmonicRatio[(size_t) h] = t.ratio[h];
        v.harmonicAmp[(size_t) h]   = (t.ratio[h] > 0.0f && withinNyquist) ? t.amp[h] : 0.0f;
    }

    v.attackCoeff  = msToCoeff (t.attackMs,  currentSampleRate);
    v.decayCoeff   = msToCoeff (t.decayMs,   currentSampleRate);
    v.releaseCoeff = msToCoeff (t.releaseMs, currentSampleRate);
    v.sustainLevel = t.sustainLevel;
    v.stage = SynthVoice::Stage::attack;
    v.velocityGain = juce::jlimit (0.0f, 1.0f, (float) velocity / 127.0f);
}

void NovaChordAudioProcessor::releaseSynthVoice (int midiNote) noexcept
{
    for (auto& v : synthVoices)
        if (v.active && v.midiNote == midiNote && v.stage != SynthVoice::Stage::release)
            v.stage = SynthVoice::Stage::release;
}

void NovaChordAudioProcessor::renderSynthVoices (float* left, float* right, int numSamples, bool stereo) noexcept
{
    // Headroom compensation: chords stack several voices at once.
    constexpr float masterGain = 0.22f;

    for (auto& v : synthVoices)
    {
        if (! v.active) continue;

        for (int s = 0; s < numSamples; ++s)
        {
            switch (v.stage)
            {
                case SynthVoice::Stage::attack:
                    v.envelopeLevel = v.attackCoeff * v.envelopeLevel + (1.0f - v.attackCoeff) * 1.02f;
                    if (v.envelopeLevel >= 0.999f) { v.envelopeLevel = 1.0f; v.stage = SynthVoice::Stage::decay; }
                    break;
                case SynthVoice::Stage::decay:
                    v.envelopeLevel = v.decayCoeff * v.envelopeLevel + (1.0f - v.decayCoeff) * v.sustainLevel;
                    if (std::abs (v.envelopeLevel - v.sustainLevel) < 0.001f) v.stage = SynthVoice::Stage::sustain;
                    break;
                case SynthVoice::Stage::sustain:
                    v.envelopeLevel = v.sustainLevel;
                    break;
                case SynthVoice::Stage::release:
                    v.envelopeLevel = v.releaseCoeff * v.envelopeLevel;
                    if (v.envelopeLevel < 0.0005f) { v.envelopeLevel = 0.0f; v.stage = SynthVoice::Stage::idle; v.active = false; }
                    break;
                case SynthVoice::Stage::idle:
                    break;
            }

            float sample = 0.0f;
            for (int h = 0; h < 4; ++h)
                if (v.harmonicAmp[(size_t) h] > 0.0f)
                    sample += v.harmonicAmp[(size_t) h] * (float) std::sin (v.phase * v.harmonicRatio[(size_t) h]);

            sample *= v.envelopeLevel * v.velocityGain * masterGain;

            left[s] += sample;
            if (stereo) right[s] += sample;

            v.phase += v.phaseIncrement;
            if (v.phase > juce::MathConstants<double>::twoPi)
                v.phase -= juce::MathConstants<double>::twoPi;

            if (! v.active) break; // voice finished mid-block
        }
    }
}

void NovaChordAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    juce::ScopedNoDenormals noDenormals;

    const int styleIdx   = (int) apvts.getRawParameterValue ("styleIdx")->load();
    const int octShift   = (int) apvts.getRawParameterValue ("octaveShift")->load();
    const int vel        = juce::jlimit (1, 127, (int) apvts.getRawParameterValue ("velocity")->load());
    const bool passThru  = apvts.getRawParameterValue ("passThrough")->load() > 0.5f;

    juce::MidiBuffer outBuffer;
    const int numSamples = buffer.getNumSamples();

    // ── UI chord-preview (click-to-play in the suggestions/progression
    // panels): independent of incoming MIDI, so it works whether or not a
    // real keyboard is playing. New requests start immediately at sample 0;
    // already-sounding preview notes count down and release themselves.
    if (previewPending.exchange (false))
    {
        const int root  = previewRoot.load();
        const int count = previewCount.load();
        const int pvel  = previewVelocity.load();
        const int holdSamples = juce::jmax (1, (int) (previewHoldSeconds * currentSampleRate));

        for (int i = 0; i < count && i < 6; ++i)
        {
            const int semis = previewSemis[(size_t) i].load();
            if (semis < 0) continue;
            const int midiNote = juce::jlimit (0, 127, root + semis);

            // Reuse an inactive voice slot, or steal the first one.
            int slot = -1;
            for (int v = 0; v < 6; ++v)
                if (previewVoices[(size_t) v].midiNote < 0) { slot = v; break; }
            if (slot < 0) slot = i % 6;

            if (previewVoices[(size_t) slot].midiNote >= 0)
            {
                outBuffer.addEvent (juce::MidiMessage::noteOff (1, previewVoices[(size_t) slot].midiNote), 0);
                releaseSynthVoice (previewVoices[(size_t) slot].midiNote);
            }

            outBuffer.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) pvel), 0);
            startSynthVoice (midiNote, pvel);
            previewVoices[(size_t) slot] = { midiNote, holdSamples };
        }
    }

    for (auto& voice : previewVoices)
    {
        if (voice.midiNote < 0) continue;
        voice.samplesRemaining -= numSamples;
        if (voice.samplesRemaining <= 0)
        {
            outBuffer.addEvent (juce::MidiMessage::noteOff (1, voice.midiNote), juce::jlimit (0, juce::jmax (0, numSamples - 1), numSamples + voice.samplesRemaining));
            releaseSynthVoice (voice.midiNote);
            voice.midiNote = -1;
        }
    }

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
                releaseSynthVoice (note);
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
                startSynthVoice (midiNote, vel);
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
                    releaseSynthVoice (note);
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

    // Render the built-in synth's audio for all active voices - real-time,
    // no allocation, into the (already-cleared) output buffer.
    if (buffer.getNumChannels() >= 2)
        renderSynthVoices (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, true);
    else if (buffer.getNumChannels() == 1)
    {
        auto* mono = buffer.getWritePointer (0);
        renderSynthVoices (mono, mono, numSamples, false);
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaChordAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}
#endif

juce::AudioProcessorEditor* NovaChordAudioProcessor::createEditor()
{
    return new NovaChordAudioProcessorEditor (*this);
}

bool NovaChordAudioProcessor::hasEditor() const { return true; }

const juce::String NovaChordAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaChordAudioProcessor::acceptsMidi()  const { return true; }
bool NovaChordAudioProcessor::producesMidi() const { return true; } // still forwards chord MIDI for an external instrument
bool NovaChordAudioProcessor::isMidiEffect() const { return false; } // now a hybrid instrument: it renders its own audio too
double NovaChordAudioProcessor::getTailLengthSeconds() const { return 1.5; } // longest release (Bell) plus headroom

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
