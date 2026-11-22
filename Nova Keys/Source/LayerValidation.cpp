// Headless validation of NovaInstrumentLayer (spec section 6): key/velocity
// range gating, on/off, note-off silencing a looping sample via the layer's
// own envelope (a sample zone has no envelope of its own - this is the
// mechanism that makes that actually work), synth-layer double-envelope
// release, and four layers stacking together for one note.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Layer/NovaInstrumentLayer.h"
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

    float peakOf (const std::vector<float>& v)
    {
        float p = 0.f;
        for (float s : v) p = juce::jmax (p, std::abs (s));
        return p;
    }

    int countZeroCrossings (const std::vector<float>& s)
    {
        int c = 0;
        for (size_t i = 1; i < s.size(); ++i)
            if ((s[i - 1] < 0.f) != (s[i] < 0.f)) ++c;
        return c;
    }

    void renderInChunks (NovaInstrumentLayer& layer, std::vector<float>& outL, std::vector<float>& outR)
    {
        int pos = 0;
        const int total = (int) outL.size();
        while (pos < total)
        {
            const int n = juce::jmin (blockSize, total - pos);
            layer.renderAdding (outL.data() + pos, outR.data() + pos, n);
            pos += n;
        }
    }
}

int main()
{
    std::cout << "\n[Layer] Sample layer: key/velocity range gating\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto buf = makeSineBuffer (300.0, (int) sr * 2);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = buf; zone.sourceSampleRate = sr;
        engine.addZone (zone);

        NovaLayerParams p;
        p.lowKey = 48; p.highKey = 72;
        p.lowVelocity = 40; p.highVelocity = 127;
        p.attackMs = 1.f; p.releaseMs = 50.f;

        NovaInstrumentLayer layer;
        layer.prepare (sr, blockSize);
        layer.configureAsSample (&engine, p);

        layer.noteOn (60, 100); // in range
        check (layer.isActive(), "note 60 vel 100 (in range) activates the layer");
        layer.reset();

        layer.noteOn (90, 100); // key out of range
        check (! layer.isActive(), "note 90 (outside key range) does not activate the layer");

        layer.noteOn (60, 10); // velocity out of range
        check (! layer.isActive(), "velocity 10 (outside velocity range) does not activate the layer");
    }

    std::cout << "\n[Layer] Layer off never sounds, even in range\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto buf = makeSineBuffer (300.0, (int) sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = buf; zone.sourceSampleRate = sr;
        engine.addZone (zone);

        NovaLayerParams p;
        p.on = false;
        NovaInstrumentLayer layer;
        layer.prepare (sr, blockSize);
        layer.configureAsSample (&engine, p);
        layer.noteOn (60, 100);
        check (! layer.isActive(), "layer with on=false never activates");
    }

    std::cout << "\n[Layer] Note-off silences a LOOPING sample via the layer's own envelope\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto buf = makeSineBuffer (300.0, 2000);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = buf; zone.sourceSampleRate = sr;
        zone.loopEnabled = true; zone.loopMode = NovaLoopMode::Forward;
        zone.loopStart = 100; zone.loopEnd = 1900; // loops forever - only the layer envelope can stop it
        engine.addZone (zone);

        NovaLayerParams p;
        p.attackMs = 1.f; p.decayMs = 1.f; p.sustain = 1.f; p.releaseMs = 80.f;
        NovaInstrumentLayer layer;
        layer.prepare (sr, blockSize);
        layer.configureAsSample (&engine, p);

        layer.noteOn (60, 100);
        std::vector<float> heldL (8000, 0.f), heldR (8000, 0.f); // well past the raw 2000-sample buffer - proves it's looping
        renderInChunks (layer, heldL, heldR);
        check (layer.isActive(), "layer is still active (looping) well past the raw sample length");
        check (peakOf (heldL) > 0.1f, "held note produces real, non-silent audio while looping");

        layer.noteOff();
        std::vector<float> tailL (10000, 0.f), tailR (10000, 0.f); // well past the 80ms release (~3528 samples)
        renderInChunks (layer, tailL, tailR);
        check (! layer.isActive(), "layer becomes inactive once release completes, even though the zone itself loops forever");

        const float tailEnd = std::abs (tailL[9900]);
        check (tailEnd < 0.01f, "audio has decayed to near-silence by the end of the release tail");
    }

    std::cout << "\n[Layer] Synth layer: activation, range gating, and release lifecycle\n";
    {
        NovaSynthVoiceParams sp;
        sp.waveform = NovaOscWaveform::Saw;

        NovaLayerParams p;
        p.lowKey = 36; p.highKey = 84;
        p.attackMs = 1.f; p.decayMs = 10.f; p.sustain = 1.f; p.releaseMs = 60.f;

        NovaInstrumentLayer layer;
        layer.prepare (sr, blockSize);
        layer.configureAsSynth (sp, p);

        layer.noteOn (20, 100); // out of key range
        check (! layer.isActive(), "synth layer out of key range does not activate");

        layer.noteOn (60, 100);
        check (layer.isActive(), "synth layer in range activates");

        std::vector<float> outL (2000, 0.f), outR (2000, 0.f);
        renderInChunks (layer, outL, outR);
        check (peakOf (outL) > 0.05f, "synth layer produces real audio");

        layer.noteOff();
        std::vector<float> tailL (8000, 0.f), tailR (8000, 0.f); // past the 60ms release
        renderInChunks (layer, tailL, tailR);
        check (! layer.isActive(), "synth layer becomes inactive once its release completes (layer envelope governs it)");
    }

    std::cout << "\n[Layer] Volume and octave shift measurably affect output\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto buf = makeSineBuffer (220.0, (int) sr);
        NovaSampleZone zone;
        zone.rootNote = 60; zone.lowKey = 0; zone.highKey = 127; zone.lowVelocity = 1; zone.highVelocity = 127;
        zone.buffer = buf; zone.sourceSampleRate = sr;
        engine.addZone (zone);

        NovaLayerParams loud; loud.attackMs = 0.1f; loud.sustain = 1.f; loud.volumeDb = 0.f;
        NovaInstrumentLayer loudLayer;
        loudLayer.prepare (sr, blockSize);
        loudLayer.configureAsSample (&engine, loud);
        loudLayer.noteOn (60, 100);
        std::vector<float> loudL (4096, 0.f), loudR (4096, 0.f);
        renderInChunks (loudLayer, loudL, loudR);

        NovaLayerParams quiet = loud; quiet.volumeDb = -40.f;
        NovaInstrumentLayer quietLayer;
        quietLayer.prepare (sr, blockSize);
        quietLayer.configureAsSample (&engine, quiet);
        quietLayer.noteOn (60, 100);
        std::vector<float> quietL (4096, 0.f), quietR (4096, 0.f);
        renderInChunks (quietLayer, quietL, quietR);

        check (peakOf (quietL) < peakOf (loudL) * 0.02f, "-40dB layer volume produces much quieter output than 0dB");

        NovaLayerParams shifted = loud; shifted.octaveShift = 1;
        NovaInstrumentLayer shiftedLayer;
        shiftedLayer.prepare (sr, blockSize);
        shiftedLayer.configureAsSample (&engine, shifted);
        shiftedLayer.noteOn (60, 100);
        std::vector<float> shiftedL (4096, 0.f), shiftedR (4096, 0.f);
        renderInChunks (shiftedLayer, shiftedL, shiftedR);

        const double ratio = (double) countZeroCrossings (shiftedL) / (double) juce::jmax (1, countZeroCrossings (loudL));
        check (ratio > 1.85 && ratio < 2.15, "layer octaveShift=+1 roughly doubles the sample playback frequency");
    }

    std::cout << "\n[Layer] Four layers stack together for one note, each self-gating by range\n";
    {
        NovaSamplerEngine sampleEngineA;
        sampleEngineA.prepare (sr);
        auto bufA = makeSineBuffer (261.63, (int) sr); // C4
        NovaSampleZone zoneA; zoneA.rootNote = 60; zoneA.lowKey = 0; zoneA.highKey = 127; zoneA.lowVelocity = 1; zoneA.highVelocity = 127;
        zoneA.buffer = bufA; zoneA.sourceSampleRate = sr;
        sampleEngineA.addZone (zoneA);

        NovaLayerParams pA; pA.attackMs = 1.f; pA.sustain = 1.f; // Layer A: full range, always contributes
        NovaInstrumentLayer layerA;
        layerA.prepare (sr, blockSize);
        layerA.configureAsSample (&sampleEngineA, pA);

        NovaLayerParams pB; pB.attackMs = 1.f; pB.sustain = 1.f; pB.lowKey = 72; pB.highKey = 127; // Layer B: high keys only
        NovaSynthVoiceParams spB; spB.waveform = NovaOscWaveform::Sine;
        NovaInstrumentLayer layerB;
        layerB.prepare (sr, blockSize);
        layerB.configureAsSynth (spB, pB);

        NovaLayerParams pC; pC.attackMs = 1.f; pC.sustain = 1.f; pC.lowVelocity = 100; // Layer C: hard hits only
        NovaSynthVoiceParams spC; spC.waveform = NovaOscWaveform::Triangle;
        NovaInstrumentLayer layerC;
        layerC.prepare (sr, blockSize);
        layerC.configureAsSynth (spC, pC);

        NovaLayerParams pD; pD.on = false; // Layer D: disabled
        NovaSynthVoiceParams spD;
        NovaInstrumentLayer layerD;
        layerD.prepare (sr, blockSize);
        layerD.configureAsSynth (spD, pD);

        const int testNote = 60, testVelocity = 110; // low key, hard hit
        layerA.noteOn (testNote, testVelocity);
        layerB.noteOn (testNote, testVelocity);
        layerC.noteOn (testNote, testVelocity);
        layerD.noteOn (testNote, testVelocity);

        check (layerA.isActive(), "Layer A (full range) activates for note 60");
        check (! layerB.isActive(), "Layer B (keys 72-127 only) does NOT activate for note 60");
        check (layerC.isActive(), "Layer C (velocity 100+ only) activates for velocity 110");
        check (! layerD.isActive(), "Layer D (off) never activates");

        std::vector<float> mixL (2048, 0.f), mixR (2048, 0.f);
        renderInChunks (layerA, mixL, mixR);
        renderInChunks (layerB, mixL, mixR);
        renderInChunks (layerC, mixL, mixR);
        renderInChunks (layerD, mixL, mixR);

        bool allFinite = true;
        for (float v : mixL) if (! std::isfinite (v)) { allFinite = false; break; }
        check (allFinite, "combined four-layer output stays finite");
        check (peakOf (mixL) > 0.05f, "combined four-layer output produces real, non-silent audio");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL LAYER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
