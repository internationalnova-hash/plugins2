#pragma once

#include <juce_core/juce_core.h>
#include "NovaVocalChain.h"
#include "NovaVocalTuner.h"

// What Listen measured about the voice. Spectral band levels are average
// power density in dB, relative to the 1-2 kHz band, measured only over the
// frames where someone is actually singing (silence and breaths ignored).
struct NovaVocalProfile
{
    bool  valid = false;
    float activeSeconds   = 0.f;
    float activeRmsDb     = -100.f; // loudness while singing
    float peakDb          = -100.f;
    float dynamicSpreadDb = 0.f;    // loud vs. quiet phrases (95th - 10th percentile)
    float sibilanceDb     = -30.f;  // 90th percentile of 5-10 kHz vs. full band, per frame

    float rumbleDb   = 0.f; //   20-80 Hz
    float lowDb      = 0.f; //  100-200 Hz
    float mudDb      = 0.f; //  200-450 Hz
    float boxDb      = 0.f; //  500-900 Hz
    float presenceDb = 0.f; // 2-5 kHz
    float sibBandDb  = 0.f; // 5-9 kHz
    float airDb      = 0.f; // 10-16 kHz

    // Musical key heard in the take (drives the tuner when Key = Auto)
    bool  keyValid = false;
    int   keyPc    = 0;
    bool  keyMinor = false;

    std::unique_ptr<juce::XmlElement> toXml() const;
    static NovaVocalProfile fromXml (const juce::XmlElement&);
};

struct NovaVocalDecision
{
    NovaVocalMacros macros;
    NovaVocalPlan   plan;
    juce::String    summary; // short human-readable notes for the UI
};

// Offline analysis of a captured vocal (runs on a background thread).
NovaVocalProfile analyzeNovaVocal (const float* const* channels, int numChannels, int numSamples, double sampleRate);

// Profile + vibe -> knob positions and corrective plan. Pure and fast.
NovaVocalDecision decideNovaVocalMix (const NovaVocalProfile& profile, int vibeIndex);

// Knob positions for a vibe when nothing has been analysed yet.
NovaVocalMacros novaVibeDefaultMacros (int vibeIndex);

juce::String novaKeyName (int keyPc, bool minor);

std::unique_ptr<juce::XmlElement> novaVocalPlanToXml (const NovaVocalPlan&);
NovaVocalPlan novaVocalPlanFromXml (const juce::XmlElement&);
