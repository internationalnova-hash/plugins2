// Headless validation harness for NovaMasteringDecisionEngine. Checks the
// spec's core requirements directly: the engine must be source-aware (same
// Style on different material -> different parameters, spec section 3),
// already-compressed material must get less additional compression (spec
// section 15), and safety clamps must hold even for pathological input
// (spec section 39).

#include <juce_core/juce_core.h>
#include "Analysis/NovaAudioProfile.h"
#include "Decision/NovaMasteringDecisionEngine.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;

    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    NovaAudioProfile makeNeutralProfile()
    {
        NovaAudioProfile p;
        p.integratedLufs = -16.f;
        p.crestFactorDb = 12.f;
        p.compressionAmount = 0.3f;
        p.subEnergy = 0.10f; p.bassEnergy = 0.18f; p.lowMidEnergy = 0.16f;
        p.midEnergy = 0.20f; p.upperMidEnergy = 0.16f; p.presenceEnergy = 0.12f; p.highEnergy = 0.08f;
        p.spectralTilt = -0.15f;
        p.harshness = 0.1f; p.resonanceScore = 0.1f;
        p.stereoWidth = 0.4f; p.stereoCorrelation = 0.7f;
        p.transientStrength = 0.4f; p.transientDensity = 0.3f;
        p.isSilent = false;
        p.lengthSeconds = 180.0;
        return p;
    }
}

int main()
{
    NovaMasteringDecisionEngine engine;

    // ---- Source-awareness: same style, two different mixes -> different plans ----
    {
        std::cout << "\n[Decision] Same Style on two different mixes -> different parameters (spec section 3)\n";

        NovaAudioProfile darkMuddy = makeNeutralProfile();
        darkMuddy.spectralTilt = -0.9f;      // very dark
        darkMuddy.bassEnergy = 0.35f;        // excess bass
        darkMuddy.lowMidEnergy = 0.28f;      // muddy
        darkMuddy.harshness = 0.05f;

        NovaAudioProfile brightHarsh = makeNeutralProfile();
        brightHarsh.spectralTilt = 0.7f;     // very bright
        brightHarsh.harshness = 0.8f;        // harsh
        brightHarsh.bassEnergy = 0.08f;

        NovaMasterUserPrefs prefs; // identical prefs for both
        prefs.style = NovaMasterStyle::Clean;
        prefs.intensity = 0.6f;
        prefs.loudness = NovaLoudnessMode::Balanced;

        auto planDark  = engine.decide (darkMuddy, prefs);
        auto planBright = engine.decide (brightHarsh, prefs);

        std::cout << "    dark mix:   lowShelf=" << planDark.correctiveEQ.lowShelfDb
                  << " highShelf=" << planDark.correctiveEQ.highShelfDb
                  << " resonance=" << planDark.resonanceControl.amount << "\n";
        std::cout << "    bright mix: lowShelf=" << planBright.correctiveEQ.lowShelfDb
                  << " highShelf=" << planBright.correctiveEQ.highShelfDb
                  << " resonance=" << planBright.resonanceControl.amount << "\n";

        check (std::abs (planDark.correctiveEQ.lowShelfDb - planBright.correctiveEQ.lowShelfDb) > 0.5f,
              "low shelf differs meaningfully between dark/bright mixes");
        check (std::abs (planDark.correctiveEQ.highShelfDb - planBright.correctiveEQ.highShelfDb) > 0.5f,
              "high shelf differs meaningfully between dark/bright mixes");
        check (planBright.resonanceControl.amount > planDark.resonanceControl.amount,
              "harsher mix gets more resonance suppression");
        check (planDark.correctiveEQ.lowShelfDb < 0.f, "excess-bass mix gets a low shelf CUT, not a boost");
        check (planBright.correctiveEQ.highShelfDb < 0.f, "bright mix gets a high shelf CUT, not a boost");
    }

    // ---- Already-compressed material gets less additional compression (spec section 15) ----
    {
        std::cout << "\n[Decision] Heavily compressed source gets less compression than a dynamic one (spec section 15)\n";

        NovaAudioProfile dynamic = makeNeutralProfile();
        dynamic.crestFactorDb = 18.f;
        dynamic.compressionAmount = 0.05f; // barely compressed already

        NovaAudioProfile squashed = makeNeutralProfile();
        squashed.crestFactorDb = 3.f;
        squashed.compressionAmount = 0.9f; // already heavily limited

        NovaMasterUserPrefs prefs;
        prefs.style = NovaMasterStyle::Clean;
        prefs.intensity = 0.7f;
        prefs.loudness = NovaLoudnessMode::Balanced;

        auto planDynamic  = engine.decide (dynamic, prefs);
        auto planSquashed = engine.decide (squashed, prefs);

        std::cout << "    dynamic source ratio=" << planDynamic.compression.ratio << "\n";
        std::cout << "    squashed source ratio=" << planSquashed.compression.ratio << "\n";

        check (planDynamic.compression.ratio > planSquashed.compression.ratio,
              "dynamic source gets a higher compression ratio than already-squashed source");
    }

    // ---- Style differentiation on the SAME source (styles should still diverge) ----
    {
        std::cout << "\n[Decision] Different styles on the same source -> different plans\n";
        NovaAudioProfile p = makeNeutralProfile();

        NovaMasterUserPrefs cleanPrefs; cleanPrefs.style = NovaMasterStyle::Clean; cleanPrefs.intensity = 0.6f; cleanPrefs.loudness = NovaLoudnessMode::Balanced;
        NovaMasterUserPrefs loudPrefs;  loudPrefs.style  = NovaMasterStyle::Loud;  loudPrefs.intensity  = 0.6f; loudPrefs.loudness  = NovaLoudnessMode::Balanced;
        NovaMasterUserPrefs openPrefs;  openPrefs.style  = NovaMasterStyle::Open;  openPrefs.intensity  = 0.6f; openPrefs.loudness  = NovaLoudnessMode::Balanced;

        auto planClean = engine.decide (p, cleanPrefs);
        auto planLoud   = engine.decide (p, loudPrefs);
        auto planOpen   = engine.decide (p, openPrefs);

        std::cout << "    clean: ratio=" << planClean.compression.ratio << " clip=" << planClean.clipping.active << " width=" << planClean.stereo.widthAmount << "\n";
        std::cout << "    loud:  ratio=" << planLoud.compression.ratio  << " clip=" << planLoud.clipping.active  << " width=" << planLoud.stereo.widthAmount << "\n";
        std::cout << "    open:  ratio=" << planOpen.compression.ratio  << " clip=" << planOpen.clipping.active  << " width=" << planOpen.stereo.widthAmount << "\n";

        check (planLoud.compression.ratio > planClean.compression.ratio, "Loud style compresses more than Clean on identical source");
        check (planOpen.stereo.widthAmount >= planClean.stereo.widthAmount, "Open style widens at least as much as Clean");
        check (planLoud.clipping.active && ! planClean.clipping.active, "Loud style engages clipping, Clean does not (at same loudness mode)");
    }

    // ---- Safety clamps hold under pathological input (spec section 39) ----
    {
        std::cout << "\n[Decision] Safety clamps hold for extreme/pathological profile values\n";
        NovaAudioProfile extreme;
        extreme.integratedLufs = -70.f;     // absurdly quiet
        extreme.crestFactorDb = 0.f;
        extreme.compressionAmount = 1.f;
        extreme.subEnergy = 0.9f; extreme.bassEnergy = 0.9f;
        extreme.harshness = 1.f; extreme.resonanceScore = 1.f;
        extreme.spectralTilt = 5.f;         // way outside normal range
        extreme.stereoWidth = 1.f;
        extreme.isSilent = false;
        extreme.lengthSeconds = 180.0;

        NovaMasterUserPrefs prefs;
        prefs.style = NovaMasterStyle::Loud;
        prefs.intensity = 1.f;
        prefs.loudness = NovaLoudnessMode::Loud;

        auto plan = engine.decide (extreme, prefs);

        std::cout << "    outputGainDb=" << plan.output.gainDb
                  << " lowShelfDb=" << plan.correctiveEQ.lowShelfDb
                  << " ratio=" << plan.compression.ratio
                  << " widthAmount=" << plan.stereo.widthAmount
                  << " truePeakCeil=" << plan.limiting.truePeakCeilDb << "\n";

        check (plan.output.gainDb <= 12.f && plan.output.gainDb >= -6.f, "output gain stays within safety clamp");
        check (std::abs (plan.correctiveEQ.lowShelfDb) <= 3.f, "low shelf stays within safety clamp");
        check (std::abs (plan.correctiveEQ.highMidDb) <= 4.f, "high-mid cut stays within safety clamp");
        check (plan.compression.ratio <= 6.f, "compression ratio stays within safety clamp");
        check (plan.stereo.widthAmount >= 0.75f && plan.stereo.widthAmount <= 1.5f, "stereo width stays within safety clamp");
        check (plan.limiting.truePeakCeilDb < 0.f, "true peak ceiling never reaches/exceeds 0dBTP");
    }

    // ---- Silence -> conservative fallback plan ----
    {
        std::cout << "\n[Decision] Silent profile -> conservative safety-only plan\n";
        NovaAudioProfile silent;
        silent.isSilent = true;

        NovaMasterUserPrefs prefs;
        prefs.style = NovaMasterStyle::Loud;
        prefs.intensity = 1.f;

        auto plan = engine.decide (silent, prefs);
        check (! plan.correctiveEQ.active, "corrective EQ stays off for silence");
        check (! plan.compression.active, "compression stays off for silence");
        check (plan.limiting.active, "safety limiter stays on regardless");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL DECISION ENGINE VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
