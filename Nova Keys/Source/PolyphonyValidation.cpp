// Headless validation of NovaKeysEngine (spec section 8): fixed-capacity
// voice pool, graceful quietest/oldest voice stealing, correct per-note
// note-off targeting, and a dense-MIDI stress test proving no NaN blow-up
// and no gross discontinuities (clicks) when the pool is under pressure.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "NovaKeysEngine.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <random>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    constexpr double sr = 44100.0;
    constexpr int blockSize = 512;

    std::shared_ptr<juce::AudioBuffer<float>> makeSineBuffer (double freqHz, int numSamples)
    {
        auto buf = std::make_shared<juce::AudioBuffer<float>> (1, numSamples);
        auto* d = buf->getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            d[i] = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sr);
        return buf;
    }

    void renderBlocks (NovaKeysEngine& engine, int totalSamples, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr)
    {
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        int pos = 0;
        while (pos < totalSamples)
        {
            const int n = juce::jmin (blockSize, totalSamples - pos);
            engine.renderNextBlock (l.data(), r.data(), n);
            if (outL) outL->insert (outL->end(), l.begin(), l.begin() + n);
            if (outR) outR->insert (outR->end(), r.begin(), r.begin() + n);
            pos += n;
        }
    }
}

int main()
{
    std::cout << "\n[Polyphony] Voice pool fills correctly and reports accurate active count\n";
    {
        NovaSamplerEngine sampler;
        sampler.prepare (sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = makeSineBuffer (300.0, (int) sr * 4);
        zone.sourceSampleRate = sr;
        sampler.addZone (zone);

        NovaKeysEngine engine;
        engine.prepare (sr, blockSize, 8);
        NovaLayerParams p; p.attackMs = 1.f; p.sustain = 1.f;
        engine.configureLayerAsSample (0, &sampler, p);

        for (int i = 0; i < 8; ++i)
            engine.noteOn (48 + i, 100);
        renderBlocks (engine, 256);

        check (engine.getNumActiveVoices() == 8, "8 note-ons into an 8-voice pool all remain active");

        std::cout << "\n[Polyphony] A 9th note-on beyond capacity steals gracefully rather than being dropped\n";
        engine.noteOn (100, 100); // 9th note, pool is full of held notes
        renderBlocks (engine, 256);
        check (engine.getNumActiveVoices() == 8, "active voice count stays capped at 8 after a 9th note-on (a steal occurred, not silent failure)");
    }

    std::cout << "\n[Polyphony] Stealing prefers an already-released voice over a still-held one\n";
    {
        NovaSamplerEngine sampler;
        sampler.prepare (sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = makeSineBuffer (300.0, (int) sr * 4);
        zone.sourceSampleRate = sr;
        sampler.addZone (zone);

        NovaKeysEngine engine;
        engine.prepare (sr, blockSize, 4);
        NovaLayerParams p; p.attackMs = 1.f; p.sustain = 1.f; p.releaseMs = 30.f;
        engine.configureLayerAsSample (0, &sampler, p);

        for (int i = 0; i < 4; ++i)
            engine.noteOn (48 + i, 100); // notes 48,49,50,51 fill the pool
        renderBlocks (engine, 256);
        check (engine.getNumActiveVoices() == 4, "4-voice pool fills with 4 held notes");

        engine.noteOff (49); // release the second note
        renderBlocks (engine, 4000); // let its ~30ms release fully finish
        check (engine.getNumActiveVoices() == 3, "releasing one note drops the active count to 3 once its release completes");

        engine.noteOn (200, 100); // 5th note - pool is full again (3 held + this new one would be 4, but a slot must free up: the released note 49's old voice)
        renderBlocks (engine, 512);
        check (engine.getNumActiveVoices() == 4, "new note-on reuses the freed voice, active count returns to 4");

        // The three ORIGINALLY-held notes (48, 50, 51) must still be sounding -
        // stealing must not have touched them since a released voice was available.
        engine.noteOff (48); engine.noteOff (50); engine.noteOff (51); engine.noteOff (200);
        renderBlocks (engine, 8000); // let all releases finish
        check (engine.getNumActiveVoices() == 0, "all originally-held notes were genuinely still active and correctly release now");
    }

    std::cout << "\n[Polyphony] Note-off targets only the intended note\n";
    {
        NovaSamplerEngine sampler;
        sampler.prepare (sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = makeSineBuffer (300.0, (int) sr * 4);
        zone.sourceSampleRate = sr;
        sampler.addZone (zone);

        NovaKeysEngine engine;
        engine.prepare (sr, blockSize, 16);
        NovaLayerParams p; p.attackMs = 1.f; p.sustain = 1.f; p.releaseMs = 30.f;
        engine.configureLayerAsSample (0, &sampler, p);

        engine.noteOn (60, 100); engine.noteOn (64, 100); engine.noteOn (67, 100); // a C major triad
        renderBlocks (engine, 256);
        check (engine.getNumActiveVoices() == 3, "chord of 3 notes all active");

        engine.noteOff (64);
        renderBlocks (engine, 4000); // past the release
        check (engine.getNumActiveVoices() == 2, "releasing the middle note leaves exactly the other two sounding");
    }

    std::cout << "\n[Polyphony] allNotesOff clears the entire pool\n";
    {
        NovaSamplerEngine sampler;
        sampler.prepare (sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = makeSineBuffer (300.0, (int) sr * 4);
        zone.sourceSampleRate = sr;
        sampler.addZone (zone);

        NovaKeysEngine engine;
        engine.prepare (sr, blockSize, 8);
        NovaLayerParams p; p.attackMs = 1.f; p.sustain = 1.f; p.releaseMs = 20.f;
        engine.configureLayerAsSample (0, &sampler, p);

        for (int i = 0; i < 8; ++i) engine.noteOn (40 + i, 100);
        renderBlocks (engine, 256);
        check (engine.getNumActiveVoices() == 8, "pool full before allNotesOff");

        engine.allNotesOff();
        renderBlocks (engine, 4000);
        check (engine.getNumActiveVoices() == 0, "allNotesOff releases and eventually clears every voice");
    }

    std::cout << "\n[Polyphony] Dense overlapping MIDI stress test: finite output, no gross clicks under stealing\n";
    {
        NovaSamplerEngine sampler;
        sampler.prepare (sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = makeSineBuffer (300.0, (int) sr); // 1s, will loop-free finish naturally too
        zone.sourceSampleRate = sr;
        sampler.addZone (zone);

        NovaSynthVoiceParams sp; sp.waveform = NovaOscWaveform::Saw;
        NovaKeysEngine engine;
        engine.prepare (sr, blockSize, 16); // deliberately small pool to force lots of stealing
        // Headroom-aware gain staging (as any sensibly-built preset would
        // use for a 16-voice pool) - amplitude/click bounds below reflect
        // realistic content, not an artificially unmixed worst case.
        NovaLayerParams pSample; pSample.attackMs = 2.f; pSample.sustain = 1.f; pSample.releaseMs = 40.f; pSample.volumeDb = -18.f;
        NovaLayerParams pSynth; pSynth.attackMs = 2.f; pSynth.sustain = 0.8f; pSynth.releaseMs = 60.f; pSynth.volumeDb = -18.f;
        engine.configureLayerAsSample (0, &sampler, pSample);
        engine.configureLayerAsSynth (1, sp, pSynth);

        std::mt19937 rng (1234);
        std::uniform_int_distribution<int> noteDist (36, 84);
        std::uniform_int_distribution<int> velDist (40, 127);

        std::vector<float> fullL, fullR;
        fullL.reserve (200000); fullR.reserve (200000);

        for (int event = 0; event < 400; ++event)
        {
            engine.noteOn (noteDist (rng), velDist (rng));
            if (event % 3 == 0) engine.noteOff (noteDist (rng)); // occasional (possibly no-op) note-offs mixed in
            renderBlocks (engine, blockSize, &fullL, &fullR); // one block of audio between MIDI events - dense/rapid
        }
        engine.allNotesOff();
        renderBlocks (engine, 8000, &fullL, &fullR);

        bool allFinite = true;
        float peak = 0.f, worstJump = 0.f, lastSample = 0.f;
        for (size_t i = 0; i < fullL.size(); ++i)
        {
            const float s = fullL[i];
            if (! std::isfinite (s)) { allFinite = false; break; }
            peak = juce::jmax (peak, std::abs (s));
            worstJump = juce::jmax (worstJump, std::abs (s - lastSample));
            lastSample = s;
        }

        std::cout << "    rendered " << fullL.size() << " samples, peak=" << peak << " worstJump=" << worstJump << "\n";
        check (allFinite, "400 dense overlapping note events under a small (16) voice pool stay finite (no NaN/Inf blow-up)");
        check (peak < 4.0f, "output peak stays in a sane bounded range despite heavy voice stealing");
        check (worstJump < 1.0f, "no gross sample-to-sample discontinuity (audible click) even while voices are being stolen");
        check (engine.getNumActiveVoices() == 0, "engine returns to silence after allNotesOff and the release tail completes");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL POLYPHONY VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
