// Accuracy tests for the measurement engine, using EBU Tech 3341 / BS.1770-style signals.
#include "MeterEngine.h"
#include "ReferenceAnalyzer.h"

#include <cstdio>
#include <functional>
#include <vector>

using namespace tz;

static int failures = 0;

static void check (const char* what, float got, float expected, float tol)
{
    const bool ok = std::isfinite (got) && std::abs (got - expected) <= tol;
    std::printf ("%s  %-48s got %8.3f  expected %8.3f (+/-%.2f)\n", ok ? "PASS" : "FAIL", what, got, expected, tol);
    if (! ok) ++failures;
}

struct Rig
{
    double fs;
    MeterEngine eng;
    double phase = 0;

    explicit Rig (double sampleRate, int channels = 2) : fs (sampleRate) { eng.prepare (fs, channels); }

    // Feed `seconds` of a sine at `dbfs` peak amplitude, frequency f, in 512-sample blocks.
    void sine (double seconds, double dbfs, double f, int channels = 2, double phaseOffset = 0)
    {
        const double amp = std::pow (10.0, dbfs / 20.0);
        const int total = (int) (seconds * fs);
        std::vector<float> l (512), r (512);
        const float* ch[2] = { l.data(), r.data() };

        for (int done = 0; done < total; done += 512)
        {
            const int n = std::min (512, total - done);
            for (int i = 0; i < n; ++i)
            {
                l[(size_t) i] = r[(size_t) i] = (float) (amp * std::sin (phase + phaseOffset));
                phase += 2.0 * 3.14159265358979323846 * f / fs;
            }
            eng.process (ch, channels, n);
        }
    }
};

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("\n--- %.0f Hz ---\n", fs);

        { Rig r (fs); r.sine (20, -23, 1000);
          check ("Integrated: stereo 1k sine -23 dBFS", r.eng.current (LufsInt), -23.0f, 0.1f);
          check ("Short-term: stereo 1k sine -23 dBFS", r.eng.current (LufsShort), -23.0f, 0.1f);
          check ("RMS: 1k sine -23 dBFS peak", r.eng.current (Rms), -26.01f, 0.05f);
          check ("Mid density: 1k sine -23 dBFS peak", r.eng.current (MidDensity), -26.01f, 0.5f);
          check ("Dynamic range (PSR) of steady sine", r.eng.current (DynRange), 0.0f, 0.2f);
          check ("Held max peak", r.eng.heldMax (Peak), -23.0f, 0.05f); }

        { Rig r (fs); r.sine (20, -33, 1000);
          check ("Integrated: stereo 1k sine -33 dBFS", r.eng.current (LufsInt), -33.0f, 0.1f); }

        { Rig r (fs); r.sine (10, -36, 1000); r.sine (60, -23, 1000); r.sine (10, -36, 1000);
          check ("Integrated: relative gate (-36/-23/-36)", r.eng.current (LufsInt), -23.0f, 0.1f); }

        { Rig r (fs); r.sine (10, -72, 1000); r.sine (60, -36, 1000); r.sine (10, -72, 1000);
          check ("Integrated: absolute gate (-72/-36/-72)", r.eng.current (LufsInt), -36.0f, 0.1f); }

        { Rig r (fs); r.sine (5, 0, 20);
          check ("Mid density rejects 20 Hz (should be < -30)", r.eng.current (MidDensity) < -30.0f ? 1.0f : 0.0f, 1.0f, 0.0f); }

        { // fs/4 sine at 45 degrees: samples sit at +/-0.707, real peak is 0 dBFS
          Rig r (fs); r.sine (2, 0, fs / 4.0, 2, 3.14159265358979323846 / 4.0);
          check ("Sample peak of fs/4 sine @45deg", r.eng.heldMax (Peak), -3.01f, 0.05f);
          check ("True peak of fs/4 sine @45deg", r.eng.heldMax (TruePeak), 0.0f, 0.3f); }

        { Rig r (fs); r.sine (5, -6, 997);
          check ("True peak of 997 Hz sine at -6", r.eng.heldMax (TruePeak), -6.0f, 0.2f); }

        { Rig r (fs); r.sine (5, -20, 1000); r.eng.requestReset();
          r.sine (0.01, -1000, 1000); // silence
          check ("Reset clears integrated (expect no value)", std::isfinite (r.eng.current (LufsInt)) ? 0.0f : 1.0f, 1.0f, 0.0f);
          check ("Reset clears held max", std::isfinite (r.eng.heldMax (Peak)) ? 0.0f : 1.0f, 1.0f, 0.0f); }
    }

    // ── Reference analysis from a real WAV file ──────────────────────────
    {
        std::printf ("\n--- Reference file analysis ---\n");
        const double fs = 44100.0;
        auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("NovaMeterTestRef", ".wav");
        {
            // 2 s silence intro, 20 s of 1 kHz at -14 dBFS, 2 s silence: silence must not skew the stats
            juce::AudioBuffer<float> b (2, (int) (fs * 24));
            b.clear();
            const float amp = std::pow (10.0f, -14.0f / 20.0f);
            for (int i = (int) (fs * 2); i < (int) (fs * 22); ++i)
                b.setSample (0, i, amp * (float) std::sin (2.0 * 3.14159265358979323846 * 1000.0 * i / fs)),
                b.setSample (1, i, b.getSample (0, i));
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::OutputStream> os (wav.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (os.get(), fs, 2, 24, {}, 0));
            if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
        }

        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (wav));
        if (reader == nullptr) { std::printf ("FAIL  could not read test WAV\n"); ++failures; }
        else
        {
            const auto ref = analyzeReader (*reader, "test");
            check ("Ref integrated loudness", ref.m[LufsInt].headline, -14.0f, 0.1f);
            check ("Ref loudest short-term", ref.m[LufsShort].headline, -14.0f, 0.15f);
            check ("Ref max peak", ref.m[Peak].headline, -14.0f, 0.05f);
            check ("Ref max true peak", ref.m[TruePeak].headline, -14.0f, 0.2f);
            check ("Ref typical RMS (silence excluded)", ref.m[Rms].headline, -17.01f, 0.1f);
            check ("Ref RMS 10th percentile (silence excluded)", ref.m[Rms].low, -17.01f, 0.1f);
            check ("Ref duration", (float) ref.durationSec, 24.0f, 0.01f);

            const auto p = targetsFromReference (ref, Profile::defaults ("x"));
            check ("Targets from ref: LUFS int min", p.meters[LufsInt].min, -15.0f, 0.15f);
            check ("Targets from ref: LUFS int max", p.meters[LufsInt].max, -13.0f, 0.15f);
            check ("Targets from ref: true peak max", p.meters[TruePeak].max, -13.7f, 0.25f);
        }
        reader.reset();
        wav.deleteFile();
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
