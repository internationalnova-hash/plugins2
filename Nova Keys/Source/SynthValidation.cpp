// Headless validation of NovaSynthVoice (spec section 7): waveforms,
// unison/detune, octave shift, amp ADSR, filter, and pitch/filter LFO -
// black-box tested via the same renderAdding() the real four-layer engine
// will call, not internal waveform internals.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Synth/NovaSynthVoice.h"
#include <iostream>
#include <cmath>
#include <vector>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    int countZeroCrossings (const std::vector<float>& s)
    {
        int c = 0;
        for (size_t i = 1; i < s.size(); ++i)
            if ((s[i - 1] < 0.f) != (s[i] < 0.f)) ++c;
        return c;
    }

    float rms (const std::vector<float>& s)
    {
        double sum = 0.0;
        for (float v : s) sum += (double) v * (double) v;
        return (float) std::sqrt (sum / juce::jmax ((size_t) 1, s.size()));
    }

    std::vector<float> renderVoice (NovaSynthVoiceParams params, int midiNote, float velocity, double sr, int numSamples, bool sendNoteOff = false, int noteOffAtSample = -1)
    {
        NovaSynthVoice voice;
        voice.prepare (sr);
        voice.setParams (params);
        voice.noteOn (midiNote, velocity);

        std::vector<float> outL ((size_t) numSamples, 0.f), outR ((size_t) numSamples, 0.f);

        if (sendNoteOff && noteOffAtSample > 0 && noteOffAtSample < numSamples)
        {
            voice.renderAdding (outL.data(), outR.data(), noteOffAtSample);
            voice.noteOff();
            voice.renderAdding (outL.data() + noteOffAtSample, outR.data() + noteOffAtSample, numSamples - noteOffAtSample);
        }
        else
        {
            voice.renderAdding (outL.data(), outR.data(), numSamples);
        }
        return outL;
    }
}

int main()
{
    constexpr double sr = 44100.0;

    std::cout << "\n[Synth] Basic activation: noteOn/noteOff lifecycle\n";
    {
        NovaSynthVoice voice;
        voice.prepare (sr);
        NovaSynthVoiceParams p;
        p.ampAttackMs = 1.f; p.ampDecayMs = 10.f; p.ampSustain = 0.8f; p.ampReleaseMs = 50.f;
        voice.setParams (p);

        check (! voice.isActive(), "voice starts inactive before any noteOn");
        voice.noteOn (60, 1.f);
        check (voice.isActive(), "voice becomes active immediately after noteOn");

        std::vector<float> outL (2000, 0.f), outR (2000, 0.f);
        voice.renderAdding (outL.data(), outR.data(), 2000);
        voice.noteOff();

        std::vector<float> releaseL (10000, 0.f), releaseR (10000, 0.f); // well past the 50ms release at 44.1kHz (~2205 samples)
        voice.renderAdding (releaseL.data(), releaseR.data(), 10000);
        check (! voice.isActive(), "voice becomes inactive once the release phase fully completes");
    }

    std::cout << "\n[Synth] Sine oscillator pitch: A4 (note 69) produces ~440Hz\n";
    {
        NovaSynthVoiceParams p;
        p.waveform = NovaOscWaveform::Sine;
        p.ampAttackMs = 0.1f; p.ampSustain = 1.f;
        auto outL = renderVoice (p, 69, 1.f, sr, (int) sr); // 1 second
        const int crossings = countZeroCrossings (outL);
        std::cout << "    crossings=" << crossings << " (expect ~880 for 440Hz over 1s)\n";
        check (crossings > 800 && crossings < 960, "A4 sine produces roughly 440Hz (880 zero-crossings/sec)");
    }

    std::cout << "\n[Synth] Octave shift: +1 octave roughly doubles the frequency\n";
    {
        NovaSynthVoiceParams p;
        p.waveform = NovaOscWaveform::Sine;
        p.ampAttackMs = 0.1f; p.ampSustain = 1.f;
        auto base = renderVoice (p, 60, 1.f, sr, (int) sr);
        p.octaveShift = 1;
        auto up = renderVoice (p, 60, 1.f, sr, (int) sr);
        const double ratio = (double) countZeroCrossings (up) / (double) juce::jmax (1, countZeroCrossings (base));
        std::cout << "    ratio=" << ratio << "\n";
        check (ratio > 1.85 && ratio < 2.15, "+1 octave roughly doubles the oscillator frequency");
    }

    std::cout << "\n[Synth] Unison stays amplitude-bounded regardless of voice count\n";
    {
        NovaSynthVoiceParams p;
        p.waveform = NovaOscWaveform::Saw;
        p.ampAttackMs = 0.1f; p.ampSustain = 1.f;
        p.unisonDetuneCents = 25.f;

        for (int voices : { 1, 4, 8 })
        {
            p.unisonVoices = voices;
            auto out = renderVoice (p, 57, 1.f, sr, 4096);
            float peak = 0.f;
            for (float v : out) peak = juce::jmax (peak, std::abs (v));
            // Some transient filter overshoot right at note-on (a fresh
            // resonant filter hit with a full-amplitude step) is normal DSP
            // behavior, not a bug - bound generously enough to catch a real
            // blow-up (e.g. unison summing without normalization) while
            // tolerating that.
            check (peak <= 1.5f, juce::String (voices) + "-voice unison stays within a sane amplitude bound (peak=" + juce::String (peak, 3) + ")");
        }
    }

    std::cout << "\n[Synth] Amp ADSR: attack ramps up, release ramps down to silence\n";
    {
        NovaSynthVoiceParams p;
        p.waveform = NovaOscWaveform::Square; // constant-magnitude waveform makes envelope shape easy to read
        p.ampAttackMs = 50.f; p.ampDecayMs = 10.f; p.ampSustain = 0.7f; p.ampReleaseMs = 100.f;

        NovaSynthVoice voice;
        voice.prepare (sr);
        voice.setParams (p);
        voice.noteOn (60, 1.f);

        std::vector<float> outL (10000, 0.f), outR (10000, 0.f); // ~227ms, covers attack+decay+into sustain
        voice.renderAdding (outL.data(), outR.data(), 10000);

        const float earlyMag = std::abs (outL[50]);
        const float sustainMag = std::abs (outL[9000]);
        check (earlyMag < sustainMag, "amplitude 50 samples in (early attack) is lower than in the sustain region");
        check (sustainMag > 0.5f && sustainMag < 0.85f, "sustain-region amplitude roughly matches the 0.7 sustain level");

        voice.noteOff();
        std::vector<float> relL (6000, 0.f), relR (6000, 0.f); // past the 100ms release (~4410 samples)
        voice.renderAdding (relL.data(), relR.data(), 6000);
        const float tailMag = std::abs (relL[5900]);
        check (tailMag < 0.01f, "amplitude has decayed to near-silence well past the release time");
    }

    std::cout << "\n[Synth] Low-pass filter measurably attenuates a high, harmonic-rich note\n";
    {
        NovaSynthVoiceParams p;
        p.waveform = NovaOscWaveform::Square; // rich in harmonics
        p.ampAttackMs = 0.1f; p.ampSustain = 1.f;
        p.filterHighPass = false;

        p.filterCutoffHz = 18000.f;
        auto openFilter = renderVoice (p, 84, 1.f, sr, 4096); // ~1046Hz fundamental

        p.filterCutoffHz = 120.f;
        auto closedFilter = renderVoice (p, 84, 1.f, sr, 4096);

        const float rmsOpen = rms (openFilter);
        const float rmsClosed = rms (closedFilter);
        std::cout << "    rmsOpen=" << rmsOpen << " rmsClosed=" << rmsClosed << "\n";
        check (rmsClosed < rmsOpen * 0.5f, "closing the low-pass filter well below the fundamental meaningfully reduces output energy");
    }

    std::cout << "\n[Synth] Pitch LFO (vibrato) audibly modulates the oscillator without breaking it\n";
    {
        NovaSynthVoiceParams p;
        p.waveform = NovaOscWaveform::Sine;
        p.ampAttackMs = 0.1f; p.ampSustain = 1.f;
        p.lfoRateHz = 6.f;

        auto noVibrato = renderVoice (p, 69, 1.f, sr, 4096);
        p.lfoToPitchCents = 80.f;
        auto withVibrato = renderVoice (p, 69, 1.f, sr, 4096);

        bool differs = false;
        for (size_t i = 0; i < noVibrato.size(); ++i)
            if (std::abs (noVibrato[i] - withVibrato[i]) > 0.01f) { differs = true; break; }
        check (differs, "enabling pitch LFO changes the rendered output (vibrato is audible)");

        const int crossingsNoVib = countZeroCrossings (noVibrato);
        const int crossingsVib = countZeroCrossings (withVibrato);
        const double ratio = (double) crossingsVib / (double) juce::jmax (1, crossingsNoVib);
        check (ratio > 0.85 && ratio < 1.15, "vibrato stays centered around the base pitch (crossing count within +/-15%)");

        bool allFinite = true;
        for (float v : withVibrato) if (! std::isfinite (v)) { allFinite = false; break; }
        check (allFinite, "vibrato output stays finite (no NaN/Inf blowup)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL SYNTH VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
