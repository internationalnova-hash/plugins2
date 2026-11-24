#pragma once

// Single source of truth for Nova Pitch's APVTS parameter-id strings, used
// by PluginProcessor, PluginEditor, and every validation harness. IDs
// carried over from the pre-rebuild plugin are kept stable even where
// their semantics/type changed (formant: bool -> float) so existing
// automation/session data isn't broken by the DSP rebuild.
namespace ParameterIDs
{
    static constexpr const char* key                 = "key";
    static constexpr const char* scale                = "scale";
    static constexpr const char* tolerance             = "tolerance";             // Retune Speed: 0=fast/hard, 100=slow/natural
    static constexpr const char* amount                = "amount";                // Humanize / correction strength
    static constexpr const char* confidenceThreshold   = "confidenceThreshold";
    static constexpr const char* vibrato               = "vibrato";               // Natural Vibrato preserve amount
    static constexpr const char* formant               = "formant";               // was bool (on/off), now 0-100 amount
    static constexpr const char* lowLatency            = "lowLatency";

    // New in the ground-up rebuild.
    static constexpr const char* formantWarp           = "formantWarp";           // -100..100, 0=neutral
    static constexpr const char* mix                   = "mix";                   // dry/wet, 0-100
    static constexpr const char* outputGain             = "outputGain";            // dB
    static constexpr const char* range                 = "range";                 // detector search-range bias, 0-100
    static constexpr const char* vibratoRate            = "vibratoRate";           // Hz
    static constexpr const char* transients             = "transients";           // attack-hold amount, 0-100
    static constexpr const char* bypass                 = "bypass";
    static constexpr const char* tracking               = "tracking";      // near-zero-latency mode for recording

    // Harmony (new): parts 1-4 + voice stack
    static constexpr const char* harmOn       = "harmOn";
    static constexpr const char* harmMode     = "harmMode";      // 0 = Scale, 1 = MIDI
    static constexpr const char* harmStyle    = "harmStyle";
    static constexpr const char* harmVoices   = "harmVoices";
    static constexpr const char* harmWidth    = "harmWidth";
    static constexpr const char* harmHumanize = "harmHumanize";
    static constexpr const char* harmLead     = "harmLead";
    static constexpr const char* harmMix      = "harmMix";
    static constexpr const char* partOn[4]       = { "part1On", "part2On", "part3On", "part4On" };
    static constexpr const char* partInterval[4] = { "part1Interval", "part2Interval", "part3Interval", "part4Interval" };
    static constexpr const char* partLevel[4]    = { "part1Level", "part2Level", "part3Level", "part4Level" };
}
