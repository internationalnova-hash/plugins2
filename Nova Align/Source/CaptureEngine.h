#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace NovaAlign
{
    // Records audio arriving at one of Nova Align's input buses (Guide =
    // sidechain, Target = main input) into memory while the host transport
    // plays, mirroring how VocALign's non-ARA/real-time versions capture
    // Guide/Target audio: no DAW-selection query, no ARA - just a plain
    // record-while-you-play tap on an input bus, matching the actual
    // documented VocALign VST3/AU real-time mechanism (Synchro Arts uses a
    // sidechain input for Guide capture in exactly this non-ARA situation).
    //
    // Threading: arm()/cancel()/finishAndWriteToFile() are called from the
    // message thread. processBlock() runs on the audio thread and only reads
    // the atomic armed-slot flag plus writes into a buffer pre-allocated in
    // prepare() - never allocates/frees on the audio thread.
    class CaptureEngine
    {
    public:
        CaptureEngine() = default;

        enum class Slot { none, guide, target };

        void prepare (double sampleRate);

        void arm (Slot slot) noexcept;
        void cancel() noexcept;

        Slot getArmedSlot() const noexcept { return static_cast<Slot> (armedSlot.load()); }
        bool isCapturing() const noexcept { return capturing.load(); }
        double getCapturedSeconds() const noexcept { return capturedSeconds.load(); }

        // Audio-thread. guideBus/targetBus are this block's sidechain/main
        // input bus views (may have 0 channels if the sidechain bus isn't
        // connected in the host).
        void processBlock (const juce::AudioBuffer<float>& guideBus,
                           const juce::AudioBuffer<float>& targetBus,
                           bool isPlaying, double sampleRate,
                           juce::int64 timelineSample = -1);

        // Host timeline sample where the current/last capture started (-1 if
        // the host didn't report one), and how many samples it holds so far.
        juce::int64 getStartTimelineSample() const noexcept { return startTimelineSample.load(); }
        int getWrittenSamples() const noexcept { return writePosition.load(); }

        // Message-thread. Stops capture and writes whatever was captured to a
        // temp WAV file. Returns File{} if nothing (or too little) was
        // captured.
        juce::File finishAndWriteToFile();

    private:
        static constexpr double kMaxCaptureSeconds = 180.0;

        std::atomic<int> armedSlot { static_cast<int> (Slot::none) };
        std::atomic<bool> capturing { false };
        std::atomic<double> capturedSeconds { 0.0 };
        std::atomic<int> writePosition { 0 };
        std::atomic<juce::int64> startTimelineSample { -1 };

        double preparedSampleRate = 44100.0;
        double captureSampleRate = 44100.0;
        juce::AudioBuffer<float> captureBuffer;
        bool wasPlayingLastBlock = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaptureEngine)
    };
}
