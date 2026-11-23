#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Nova Loop Lab engine: everything here is offline (message/worker thread).
// The audio thread only ever plays buffers this produces.
namespace ll
{
    static constexpr int kMaxChops = 16;
    static constexpr int kNumVariations = 8;
    static constexpr int kPeakBins = 2400;

    struct Sample
    {
        juce::AudioBuffer<float> audio; // stereo
        double sampleRate = 44100.0;
        int length() const noexcept { return audio.getNumSamples(); }
        double seconds() const noexcept { return length() / sampleRate; }
    };

    struct Analysis
    {
        double bpm = 120.0;
        double beats = 4.0;          // sample length in beats at bpm
        int bars = 1;
        bool loopFits = false;       // length is a whole number of bars at bpm
        int key = 0;                 // 0-11 major (C..B), 12-23 minor (Cm..Bm)
        float keyConfidence = 0.0f;
        std::vector<float> peaks;    // kPeakBins, 0..1
        std::vector<float> onset;    // onset strength per hop
        int hop = 512;
    };

    juce::String keyName (int key);
    Analysis analyse (const Sample& s);

    enum class ChopMode { transient = 0, grid = 1, musical = 2 };

    struct ChopSettings
    {
        ChopMode mode = ChopMode::transient;
        float sensitivity = 0.6f; // 0..1, transient + musical
        int gridDiv = 8;          // 4, 8, 16 (slices per bar)
    };

    struct Chop
    {
        int start = 0, end = 0; // sample frames in the source
    };

    struct ChopResult
    {
        std::vector<Chop> chops;
        int effectiveGridDiv = 0; // grid mode: division actually used (may be coarser to fit 16 pads)
    };

    ChopResult makeChops (const Sample& s, const Analysis& a, const ChopSettings& cs);

    struct ChopFx
    {
        int pitch = 0;      // semitones -12..12 (varispeed, like a sampler)
        int stretch = 100;  // % length 50..200, pitch kept
        int filter = 0;     // -100 low-pass .. 0 open .. +100 high-pass
        int tone = 0;       // lo-fi amount 0..100
        bool reverse = false;
        bool use = true;    // available to the generator

        bool operator== (const ChopFx& o) const noexcept
        {
            return pitch == o.pitch && stretch == o.stretch && filter == o.filter && tone == o.tone
                && reverse == o.reverse && use == o.use;
        }
    };

    // One pattern event on a 16th-note grid.
    struct Step
    {
        int t = 0;       // start, in 16ths from loop start
        int len = 1;     // length, in 16ths
        int chop = 0;
        bool stutter = false;
    };

    struct GenSettings
    {
        int bars = 2;            // 1, 2, 4
        float variation = 0.55f; // 0 = close to the original order, 1 = wild
        float stutter = 0.15f;   // 0..1
        juce::uint32 seed = 1;
    };

    std::vector<Step> generate (const std::vector<Chop>& chops, const std::vector<ChopFx>& fx,
                                const Sample& s, const Analysis& a, const GenSettings& g);

    // Renders one chop with its effects at outRate. tempoRatio = sampleBpm / hostBpm
    // (time-stretches the chop to the host tempo, pitch kept).
    juce::AudioBuffer<float> renderChop (const Sample& s, const Chop& c, const ChopFx& fx,
                                         double outRate, double tempoRatio);

    struct LoopSpec
    {
        int bars = 2;
        double bpm = 120.0;
        double sampleRate = 48000.0;
        float swing = 0.0f; // 0..1, 1 = triplet feel
    };

    // Places rendered chops on the grid. Length is exactly bars * 4 beats.
    juce::AudioBuffer<float> renderLoop (const std::vector<juce::AudioBuffer<float>>& renderedChops,
                                         const std::vector<Step>& steps, const LoopSpec& spec);

    int loopLengthSamples (const LoopSpec& spec);

    // Building blocks, exposed for tests.
    juce::AudioBuffer<float> timeStretch (const juce::AudioBuffer<float>& in, double factor);
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double ratio); // ratio = input samples per output sample
}
