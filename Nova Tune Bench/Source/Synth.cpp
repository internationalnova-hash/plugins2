#include "Synth.h"

#include <cmath>

namespace
{
    struct Note { float midi; float centsOff; float seconds; };

    float formantGain (float hz)
    {
        // Vowel "ah": F1..F4
        static const float F[] = { 700.f, 1220.f, 2600.f, 3300.f };
        static const float B[] = { 130.f,   70.f,  160.f,  250.f };
        static const float A[] = { 1.0f,   0.5f,   0.25f,  0.12f };
        float g = 0.03f;
        for (int i = 0; i < 4; ++i)
        {
            const float x = (hz - F[i]) / (B[i] * 0.5f);
            g += A[i] / (1.f + x * x);
        }
        return g;
    }

    // Renders a pitch contour (midi per sample) with an amplitude envelope.
    juce::AudioBuffer<float> render (const std::vector<float>& midi, const std::vector<float>& amp, double sr, int seed)
    {
        const int n = (int) midi.size();
        juce::AudioBuffer<float> b (2, n);
        b.clear();
        juce::Random rng (seed);
        double phase = 0;
        float drift = 0, breathLp = 0;

        for (int i = 0; i < n; ++i)
        {
            if (amp[(size_t) i] <= 0.f) { phase = 0; continue; }

            // slow random drift of a couple of cents, like a real singer
            drift += (rng.nextFloat() - 0.5f) * 0.02f;
            drift *= 0.9995f;
            const double hz = 440.0 * std::pow (2.0, (midi[(size_t) i] + drift * 0.01 - 69.0) / 12.0);
            phase += hz / sr;
            if (phase > 1e6) phase -= std::floor (phase);

            double s = 0;
            for (int k = 1; k * hz < sr * 0.45; ++k)
                s += (formantGain ((float) (k * hz)) / k) * std::sin (2.0 * juce::MathConstants<double>::pi * k * phase);

            breathLp += 0.3f * ((rng.nextFloat() * 2.f - 1.f) - breathLp);
            const float v = amp[(size_t) i] * (float) (0.35 * s) + amp[(size_t) i] * 0.004f * breathLp;
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    // Note list -> per-sample contour, with glides between notes and
    // silence padding at both ends.
    void buildContour (const std::vector<Note>& notes, float glideSec, double sr,
                       std::vector<float>& midi, std::vector<float>& amp,
                       float vibHz = 0.f, float vibCents = 0.f)
    {
        const int pad = (int) (0.3 * sr);
        midi.assign ((size_t) pad, 0.f);
        amp.assign ((size_t) pad, 0.f);

        double t = 0;
        for (size_t ni = 0; ni < notes.size(); ++ni)
        {
            const float target = notes[ni].midi + notes[ni].centsOff / 100.f;
            const float prev = ni > 0 ? notes[ni - 1].midi + notes[ni - 1].centsOff / 100.f : target;
            const int len = (int) (notes[ni].seconds * sr);
            const int glide = ni > 0 ? (int) (glideSec * sr) : 0;
            for (int i = 0; i < len; ++i, t += 1.0 / sr)
            {
                float m = target;
                if (i < glide) m = prev + (target - prev) * (float) i / (float) glide;
                // vibrato fades in after 250 ms, like a real singer
                const float vibAmt = juce::jlimit (0.f, 1.f, (float) (i / sr - 0.25) / 0.3f);
                m += vibAmt * vibCents / 100.f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * vibHz * t);
                midi.push_back (m);
                amp.push_back (1.f);
            }
        }

        // short fades at phrase start/end
        const int fade = (int) (0.02 * sr);
        const size_t first = (size_t) pad, last = amp.size();
        for (int i = 0; i < fade && first + (size_t) i < last; ++i)
        {
            amp[first + (size_t) i] *= (float) i / fade;
            amp[last - 1 - (size_t) i] *= (float) i / fade;
        }

        midi.insert (midi.end(), (size_t) pad, 0.f);
        amp.insert (amp.end(), (size_t) pad, 0.f);
    }
}

std::vector<SynthVocal> makeSynthVocals (double sr)
{
    std::vector<SynthVocal> out;
    std::vector<float> m, a;

    auto add = [&] (const char* name, const char* desc, int seed)
    {
        out.push_back ({ name, desc, render (m, a, sr, seed) });
    };

    // C major scale, alternately ~35 cents sharp and ~30 cents flat
    const std::vector<float> scale { 48, 50, 52, 53, 55, 57, 59, 60 };
    std::vector<Note> offkey;
    for (size_t i = 0; i < scale.size(); ++i) offkey.push_back ({ scale[i], i % 2 ? -30.f : 35.f, 0.45f });

    buildContour (offkey, 0.06f, sr, m, a);
    add ("synth_offkey_low", "C major scale C3-C4, every note 30-35 cents off, 60 ms slides", 1);

    for (auto& nt : offkey) nt.midi += 12.f;
    buildContour (offkey, 0.06f, sr, m, a);
    add ("synth_offkey_high", "Same scale an octave up (C4-C5)", 2);

    buildContour ({ { 57, 20, 1.3f }, { 60, -15, 1.3f }, { 64, 20, 1.3f } }, 0.08f, sr, m, a, 5.5f, 45.f);
    add ("synth_vibrato", "Held notes A3, C4, E4 with 5.5 Hz, +-45 cent vibrato, slightly off-key", 3);

    // Slow glide G3 -> G4 -> G3 (should become a staircase with hard tune)
    {
        const int pad = (int) (0.3 * sr), len = (int) (2.0 * sr);
        m.assign ((size_t) pad, 0.f); a.assign ((size_t) pad, 0.f);
        for (int i = 0; i < 2 * len; ++i)
        {
            const float p = i < len ? (float) i / len : 2.f - (float) i / len;
            m.push_back (55.f + 12.f * p);
            a.push_back (juce::jlimit (0.f, 1.f, juce::jmin (i, 2 * len - i) / (float) (0.02 * sr)));
        }
        m.insert (m.end(), (size_t) pad, 0.f); a.insert (a.end(), (size_t) pad, 0.f);
        add ("synth_glide", "Slow slide G3 up to G4 and back (2 s each way)", 4);
    }

    // Fast melisma: 16 notes at 120 ms
    {
        const float run[] = { 60, 62, 64, 67, 69, 67, 64, 62, 60, 62, 64, 62, 60, 57, 55, 57 };
        std::vector<Note> notes;
        for (int i = 0; i < 16; ++i) notes.push_back ({ run[i], (i % 3 == 0 ? 25.f : (i % 3 == 1 ? -20.f : 10.f)), 0.12f });
        buildContour (notes, 0.02f, sr, m, a);
        add ("synth_fast_run", "16-note run at 120 ms per note, 10-25 cents off", 5);
    }

    return out;
}
