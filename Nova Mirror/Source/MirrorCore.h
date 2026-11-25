#pragma once

// Nova Mirror core: frequency bands, reference/target profiles (with song
// sections) and the real-time STFT match engine. No plugin code here, so it
// can be tested offline (Tests/MirrorTests.cpp).

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace NovaMirror
{
    constexpr int kBands = 64;
    constexpr float kFMin = 20.f, kFMax = 20000.f;
    using BandArray = std::array<float, kBands>;

    float bandCenter (int b) noexcept;                  // Hz, log-spaced 20 Hz .. 20 kHz
    int   bandIndexFor (float hz) noexcept;

    // Maps FFT bins <-> bands for one (sampleRate, fftSize)
    struct BandMap
    {
        void build (double sampleRate, int fftSize);
        // band power (mean power per bin, triangular weights) from a power spectrum (fftSize/2+1 bins)
        void bandPower (const float* binPower, BandArray& out) const;
        // per-bin gain (linear) from band gains (dB), interpolated on log frequency
        void binGains (const BandArray& gainDb, float* out) const;
        int numBins = 0;
    private:
        struct Band { int first = 0; std::vector<float> w; float norm = 1.f; };
        std::array<Band, kBands> bands;
        std::vector<int> binLo;      // band index below each bin
        std::vector<float> binFrac;  // interpolation fraction toward binLo+1
    };

    // Song sections by energy (a reference is mapped into these; the input is classified live)
    enum SectionClass { secAll = 0, secBreak = 1, secVerse = 2, secHook = 3, kSectionClasses = 4 };
    const char* sectionName (int cls) noexcept;

    struct Section { float start = 0, end = 0; int cls = secAll; };

    // A target to match: from a reference track, a Nova Target, or a saved user target.
    struct Profile
    {
        juce::String name;
        juce::String kind = "reference";     // reference | nova | user
        std::array<BandArray, kSectionClasses> mono {}, side {};
        std::array<bool, kSectionClasses> has { true, false, false, false };
        std::vector<float> peaks;            // waveform overview 0..1 (reference only)
        std::vector<Section> sections;
        double seconds = 0.0;

        juce::var toVar() const;              // shapes + sections (no audio)
        static Profile fromVar (const juce::var& v);
    };

    // Analyses a whole reference track (any sample rate, 1-2 channels).
    Profile analyseReference (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);

    // Built-in Nova Targets (designed tonal-balance curves)
    juce::StringArray novaTargetNames();
    Profile novaTarget (const juce::String& name);

    // ── Real-time engine ────────────────────────────────────────────────────
    struct EngineParams
    {
        float cut = 0.5f, boost = 0.5f;          // 0..1
        float speedMs = 150.f;                   // input smoothing time
        float fromHz = 20.f, toHz = 20000.f;
        int   mode = 2;                          // 0 vocal, 1 beat, 2 mix bus, 3 master
        int   stereo = 0;                        // 0 stereo (linked), 1 mid only, 2 side only, 3 mid+side
        bool  punchGuard = true;
        bool  sections = true;
        bool  freeze = false;
    };

    struct EngineMeters      // copied to the UI (message thread reads under a spin lock)
    {
        BandArray inDb {}, targetDb {}, gainDb {};
        float match = 0.f;
        int   section = secAll;
        bool  active = false;
    };

    class MatchEngine
    {
    public:
        void prepare (double sampleRate, int maxBlock, int numChannels);
        void reset();
        int  getLatency() const noexcept { return fftSize; }

        void setParams (const EngineParams& p) noexcept { params = p; }
        void setProfile (const Profile* p) noexcept { profile = p; }   // caller keeps it alive

        // In-place; numChannels 1 or 2
        void process (float* const* ch, int numChannels, int numSamples);

        void getMeters (EngineMeters& out) const;

    private:
        struct Lane
        {
            BandArray smoothP {}, gainDb {}, frozenDb {}, longGainDb {}, target {};
            float shortTotal = 0.f;
            bool  primed = false, targetPrimed = false;
        };
        void processFrame();
        void analyseLane (Lane& lane, const BandArray& power, int profileSide, bool transient);
        int  liveSection();

        double sr = 48000.0;
        int fftOrder = 12, fftSize = 4096, hop = 1024, nCh = 2;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window;
        std::vector<std::vector<float>> inFifo, outAcc, frame;   // per channel (A/B after M/S)
        std::vector<float> outFifoA, outFifoB;
        int fifoPos = 0;
        BandMap map;
        std::vector<float> power, power2, gains;
        Lane lanes[2];
        EngineParams params;
        const Profile* profile = nullptr;
        // section tracking
        float slowMs = 0.f, runMax = 0.f;
        double heardSec = 0.0;
        int curSection = secAll;
        bool frozenCaptured = false;
        // meters
        mutable juce::SpinLock meterLock;
        EngineMeters meters;
    };
}
