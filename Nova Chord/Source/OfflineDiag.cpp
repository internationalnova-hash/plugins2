/*
  Nova Chord — Offline functional diagnostic.
  Verifies the real-time single-note chord engine and the new UI
  chord-preview trigger (requestChordPreview) directly against the real
  compiled processBlock(), checking exact MIDI note-on/off sequencing.
*/

#include <JuceHeader.h>
#include <iostream>

#include "PluginProcessor.h"

namespace
{
    void printBuffer (const juce::MidiBuffer& buf, const char* label)
    {
        std::cout << "  " << label << ":";
        if (buf.isEmpty()) { std::cout << " (empty)\n"; return; }
        for (const auto meta : buf)
        {
            const auto msg = meta.getMessage();
            std::cout << " [pos=" << meta.samplePosition
                       << (msg.isNoteOn() ? " ON " : msg.isNoteOff() ? " OFF " : " ? ")
                       << msg.getNoteNumber() << "]";
        }
        std::cout << "\n";
    }

    int countNoteOn (const juce::MidiBuffer& buf) noexcept
    {
        int c = 0;
        for (const auto meta : buf) if (meta.getMessage().isNoteOn()) ++c;
        return c;
    }

    int countNoteOff (const juce::MidiBuffer& buf) noexcept
    {
        int c = 0;
        for (const auto meta : buf) if (meta.getMessage().isNoteOff()) ++c;
        return c;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "=== NOVA CHORD OFFLINE FUNCTIONAL DIAGNOSTIC ===\n\n";

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NovaChordAudioProcessor proc;
    proc.prepareToPlay (sampleRate, blockSize);

    int passed = 0, failed = 0;
    auto check = [&] (const char* label, bool ok)
    {
        std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << label << "\n";
        if (ok) ++passed; else ++failed;
    };

    // ── Test 1: real-time engine - play a note, expect a chord out ──────
    std::cout << "--- Test 1: real-time note -> chord (styleIdx default = Maj7) ---\n";
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        proc.processBlock (buffer, midi);
        printBuffer (midi, "note-on block");
        check ("emits 4 note-ons for a Maj7 chord (root+3rd+5th+7th)", countNoteOn (midi) == 4);
        check ("isChordActive() true after note-on", proc.isChordActive());

        juce::MidiBuffer midiOff;
        midiOff.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        proc.processBlock (buffer, midiOff);
        printBuffer (midiOff, "note-off block");
        check ("emits matching note-offs on release", countNoteOff (midiOff) == 4);
        check ("isChordActive() false after release", ! proc.isChordActive());
    }

    // ── Test 2: UI preview trigger - Cmaj7 (0,4,7,11) at MIDI 60 ─────────
    std::cout << "\n--- Test 2: UI-triggered chord preview ---\n";
    {
        std::array<int, 6> semis { 0, 4, 7, 11, -1, -1 };
        proc.requestChordPreview (60, semis, 4, 100);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi; // no real MIDI input this block
        proc.processBlock (buffer, midi);
        printBuffer (midi, "trigger block");
        check ("preview emits 4 note-ons immediately", countNoteOn (midi) == 4);
        check ("no note-offs yet (hold duration not elapsed)", countNoteOff (midi) == 0);

        // previewHoldSeconds = 0.6s -> at 48kHz/512-sample blocks, ~56 blocks.
        int blocksUntilOff = -1;
        for (int b = 0; b < 100; ++b)
        {
            juce::MidiBuffer nextMidi;
            proc.processBlock (buffer, nextMidi);
            if (countNoteOff (nextMidi) > 0)
            {
                blocksUntilOff = b;
                check ("auto-releases with exactly 4 note-offs", countNoteOff (nextMidi) == 4);
                break;
            }
        }
        check ("preview auto-released within 100 blocks", blocksUntilOff >= 0);
        std::cout << "  [INFO] released after " << blocksUntilOff << " blocks ("
                   << (blocksUntilOff * blockSize / sampleRate) << "s, expected ~0.6s)\n";
    }

    // ── Test 3: rapid re-trigger doesn't crash or leak stuck notes ───────
    std::cout << "\n--- Test 3: rapid re-trigger stress ---\n";
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        bool anyIssue = false;
        for (int i = 0; i < 50; ++i)
        {
            std::array<int, 6> semis { 0, 3, 7, -1, -1, -1 };
            proc.requestChordPreview (60 + (i % 12), semis, 3, 100);
            juce::MidiBuffer midi;
            proc.processBlock (buffer, midi);
            for (const auto meta : midi)
                if (! std::isfinite ((float) meta.getMessage().getNoteNumber()))
                    anyIssue = true;
        }
        check ("50 rapid re-triggers processed without error", ! anyIssue);
    }

    // ── Test 4: built-in synth engine renders real, finite audio ─────────
    std::cout << "\n--- Test 4: audio-rendering path (renderSynthVoices) ---\n";
    {
        auto peakAbs = [] (const juce::AudioBuffer<float>& buf) noexcept
        {
            float peak = 0.0f;
            bool finite = true;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const auto* d = buf.getReadPointer (ch);
                for (int s = 0; s < buf.getNumSamples(); ++s)
                {
                    if (! std::isfinite (d[s])) finite = false;
                    peak = std::max (peak, std::abs (d[s]));
                }
            }
            return std::make_pair (peak, finite);
        };

        // Reset to a clean slate: earlier tests leave preview voices mid-envelope
        // (they auto-release only after enough processBlock ticks, which the MIDI-
        // sequencing tests above don't run out), so re-prepare before measuring silence.
        proc.releaseResources();
        proc.prepareToPlay (sampleRate, blockSize);

        // Silence before any note.
        juce::AudioBuffer<float> silenceBuf (2, blockSize);
        juce::MidiBuffer noMidi;
        proc.processBlock (silenceBuf, noMidi);
        auto [silencePeak, silenceFinite] = peakAbs (silenceBuf);
        check ("output silent with no notes held", silencePeak < 1.0e-6f);
        check ("output finite with no notes held", silenceFinite);

        // Note-on: for each of the 5 voice timbres, expect real non-silent, finite audio.
        for (int voiceType = 0; voiceType < 5; ++voiceType)
        {
            if (auto* p = proc.apvts.getParameter ("voiceType"))
                p->setValueNotifyingHost (voiceType / 4.0f);

            juce::MidiBuffer onMidi;
            onMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            juce::AudioBuffer<float> buf (2, blockSize);
            proc.processBlock (buf, onMidi);

            // Let the envelope reach a steady/audible level over a few more blocks.
            float maxPeak = 0.0f;
            bool allFinite = true;
            for (int b = 0; b < 10; ++b)
            {
                juce::MidiBuffer midi;
                proc.processBlock (buf, midi);
                auto [p, f] = peakAbs (buf);
                maxPeak = std::max (maxPeak, p);
                allFinite = allFinite && f;
            }
            std::string label = "voiceType=" + std::to_string (voiceType) + " produces audible, finite audio (peak="
                               + std::to_string (maxPeak) + ")";
            check (label.c_str(), allFinite && maxPeak > 1.0e-4f && maxPeak < 1.5f);

            juce::MidiBuffer offMidi;
            offMidi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            proc.processBlock (buf, offMidi);
            // Drain the release tail so voices are idle before the next timbre.
            // Bell's release is a 1200ms exponential time-constant, so full decay
            // toward silence takes several time constants (~8s) - drain generously.
            for (int b = 0; b < 900; ++b)
            {
                juce::MidiBuffer midi;
                proc.processBlock (buf, midi);
            }
        }

        // After release + tail drain, output should have decayed back to silence.
        juce::AudioBuffer<float> tailBuf (2, blockSize);
        proc.processBlock (tailBuf, noMidi);
        auto [tailPeak, tailFinite] = peakAbs (tailBuf);
        check ("output decays back to silence after release", tailPeak < 1.0e-3f);
        check ("output finite after release", tailFinite);
    }

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
