#pragma once

#include "NovaDoublerVoice.h"
#include <array>
#include <cmath>

// Owns up to 4 NovaDoublerVoice instances and maps WIDTH/TIGHTNESS onto
// each voice's baseline delay/pitch/pan and modulation depth. Produces
// only the WET (doubled) signal - dry/wet mixing and bypass live one
// level up, in NovaDoublerEngine.
//
// WIDTH = how different the voices are from EACH OTHER (pan separation,
// inter-voice delay/pitch spread). TIGHTNESS (as looseness L=1-tightness)
// = how different each voice is from the ORIGINAL (offset magnitude +
// modulation depth). Each voice has a fixed "shape" (pan/delay/pitch
// unit position + a distinct RNG seed) so voices are never a duplicate-
// and-pan copy of each other (spec section 9).
class NovaVoiceEnsemble
{
public:
    static constexpr int kMaxVoices = 4;

    NovaVoiceEnsemble() = default;

    void prepare (double sampleRate, float maxDelayMs) noexcept
    {
        sr = sampleRate;
        for (auto& v : voices) v.prepare (sampleRate, maxDelayMs);
        buildShapeTables();
        lastFourVoiceMode = false;
        applyShapesForMode (false);
    }

    void reset() noexcept
    {
        for (auto& v : voices) v.reset();
    }

    // widthNorm/tightnessNorm: 0..1 (caller converts from the 0-100%
    // parameter range). fourVoiceMode: false = 2 voices, true = 4.
    void setParams (float widthNorm, float tightnessNorm, bool fourVoiceMode) noexcept
    {
        if (fourVoiceMode != lastFourVoiceMode)
        {
            // Reconfigure (reseed) only on an actual mode change, not
            // every call - setParams() runs every block, and re-seeding
            // the modulators every block would erase their whole point
            // (slow organic drift accumulated over real time).
            applyShapesForMode (fourVoiceMode);
            lastFourVoiceMode = fourVoiceMode;
        }

        numActiveVoices = fourVoiceMode ? 4 : 2;
        const float width = juce::jlimit (0.0f, 1.0f, widthNorm);
        const float looseness = 1.0f - juce::jlimit (0.0f, 1.0f, tightnessNorm);

        const float delayRangeMs = juce::jmap (looseness, 5.0f, 35.0f);
        const float pitchRangeCents = juce::jmap (looseness, 3.0f, 12.0f);
        const float sep = juce::jmap (width, 0.35f, 1.0f);
        const float panMax = juce::jmap (width, 0.15f, 0.95f);
        const float modDepthScale = juce::jmap (looseness, 0.15f, 1.0f);

        const int baseIndex = fourVoiceMode ? 4 : 0;
        for (int i = 0; i < numActiveVoices; ++i)
        {
            const auto& shape = shapeTable[(size_t) (baseIndex + i)];

            const float baseDelayMs = kBaseDelayMs + shape.centeredDelayUnit * sep * delayRangeMs;
            const float basePitchCents = shape.pitchSign * shape.pitchUnit * sep * pitchRangeCents;
            const float basePan = shape.panSign * shape.panUnit * panMax;

            const float delayModDepthMs = modDepthScale * delayRangeMs * 0.3f;
            const float pitchModDepthCents = modDepthScale * pitchRangeCents * 0.3f;
            const float panModDepth = modDepthScale * panMax * 0.2f;

            voices[(size_t) i].setTargets (baseDelayMs, delayModDepthMs, basePitchCents, pitchModDepthCents, basePan, panModDepth);
        }

        // Gain compensation: starts as a lerp between coherent (1/N) and
        // decorrelated (1/sqrt(N)) summation, weighted by how decorrelated
        // the voices currently are (low Width/Tight = voices nearly
        // identical to each other and the dry signal = more coherent
        // summation; high Width/Loose = more decorrelated). This starting
        // formula is deliberately simple - EnsembleValidation.cpp's RMS
        // sweep is what actually confirms (or corrects) it empirically,
        // per this codebase's verify-don't-theorize methodology.
        const float decorrelation = juce::jlimit (0.0f, 1.0f, 0.25f + 0.75f * width * looseness);
        const float coherentGain = 1.0f / (float) numActiveVoices;
        const float decorrelatedGain = 1.0f / std::sqrt ((float) numActiveVoices);
        currentVoiceGain = juce::jmap (decorrelation, coherentGain, decorrelatedGain);
    }

    // input: mono dry source (already summed from stereo if needed, by
    // the caller - see NovaDoublerEngine). outL/outR: OVERWRITTEN (not
    // added to) with this block's wet sum.
    void processSample (float input, float& outL, float& outR) noexcept
    {
        outL = 0.0f;
        outR = 0.0f;
        for (int i = 0; i < numActiveVoices; ++i)
            voices[(size_t) i].processSample (input, outL, outR);
        outL *= currentVoiceGain;
        outR *= currentVoiceGain;
    }

    int getNumActiveVoices() const noexcept { return numActiveVoices; }
    float getCurrentVoiceGain() const noexcept { return currentVoiceGain; }

private:
    struct Shape
    {
        float panSign = 1.0f, panUnit = 1.0f;
        float centeredDelayUnit = 0.0f;
        float pitchSign = 1.0f, pitchUnit = 1.0f;
        uint32_t seedBase = 1;
    };

    void buildShapeTables() noexcept
    {
        // Indices 0-1: 2-voice shape set. Indices 4-7: 4-voice shape set.
        // Raw delayUnit values are centered (mean subtracted) here so the
        // resulting spread is symmetric around the base delay regardless
        // of the raw units chosen.
        struct Raw { float panSign, panUnit, delayUnit, pitchSign, pitchUnit; uint32_t seed; };

        const Raw raw2[2] = {
            { -1.0f, 1.0f, 0.35f, -1.0f, 0.6f, 0x1000 },
            { +1.0f, 1.0f, 0.80f, +1.0f, 1.0f, 0x2000 },
        };
        const Raw raw4[4] = {
            { -1.0f, 1.00f, 0.25f, -1.0f, 0.40f, 0x1000 }, // outer left
            { -1.0f, 0.45f, 0.55f, +1.0f, 0.70f, 0x2000 }, // inner left
            { +1.0f, 0.45f, 0.70f, -1.0f, 0.85f, 0x3000 }, // inner right
            { +1.0f, 1.00f, 1.00f, +1.0f, 1.00f, 0x4000 }, // outer right
        };

        float mean2 = 0.0f;
        for (auto& r : raw2) mean2 += r.delayUnit;
        mean2 /= 2.0f;
        for (int i = 0; i < 2; ++i)
            shapeTable[(size_t) i] = { raw2[i].panSign, raw2[i].panUnit, raw2[i].delayUnit - mean2,
                                        raw2[i].pitchSign, raw2[i].pitchUnit, raw2[i].seed };

        float mean4 = 0.0f;
        for (auto& r : raw4) mean4 += r.delayUnit;
        mean4 /= 4.0f;
        for (int i = 0; i < 4; ++i)
            shapeTable[(size_t) (4 + i)] = { raw4[i].panSign, raw4[i].panUnit, raw4[i].delayUnit - mean4,
                                              raw4[i].pitchSign, raw4[i].pitchUnit, raw4[i].seed };
    }

    static NovaDoublerVoiceShape toVoiceShape (const Shape& s) noexcept
    {
        NovaDoublerVoiceShape vs;
        vs.panSign = s.panSign;
        vs.delayUnit = s.centeredDelayUnit;
        vs.pitchSign = s.pitchSign;
        vs.pitchUnit = s.pitchUnit;
        vs.seedBase = s.seedBase;
        return vs;
    }

    void applyShapesForMode (bool fourVoiceMode) noexcept
    {
        const int baseIndex = fourVoiceMode ? 4 : 0;
        const int n = fourVoiceMode ? 4 : 2;
        for (int i = 0; i < n; ++i)
            voices[(size_t) i].configure (toVoiceShape (shapeTable[(size_t) (baseIndex + i)]));
    }

    static constexpr float kBaseDelayMs = 20.0f; // mid-point of the 5-35ms spec range

    std::array<NovaDoublerVoice, kMaxVoices> voices;
    std::array<Shape, 8> shapeTable; // [0..1]=2-voice set, [4..7]=4-voice set
    int numActiveVoices = 2;
    bool lastFourVoiceMode = false;
    float currentVoiceGain = 1.0f;
    double sr = 44100.0;
};
