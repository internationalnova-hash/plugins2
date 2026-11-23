#pragma once

#include "LoopLabEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <array>
#include <memory>

// Everything between "the user did something" and "the audio thread has new
// buffers to play". The message thread edits a Spec; a worker thread turns
// it into a Model (chops, patterns, rendered chops and loops). The audio
// thread only reads the newest Model's buffers through a raw pointer that the
// message thread keeps alive (see LoopLabCore::retire).
namespace ll
{
    struct Spec
    {
        // Sample (set by the worker after a load).
        std::shared_ptr<const Sample> sample;
        std::shared_ptr<const Analysis> analysis;
        juce::String sampleName, samplePath;
        double tempoMult = 1.0; // x2 / /2 tempo correction

        ChopSettings chop;
        std::vector<ChopFx> fx; // by chop index (grows as needed)

        std::array<juce::uint32, kNumVariations> seeds {};
        std::array<bool, kNumVariations> locked {};
        std::array<std::vector<Step>, kNumVariations> lockedSteps;

        GenSettings gen;      // bars / variation / stutter (seed per variation)
        float swing = 0.2f;
        double hostBpm = 0.0; // 0 = no host tempo (standalone): loops follow the sample's tempo
        double sampleRate = 48000.0;
    };

    struct Model
    {
        int version = 0;
        std::shared_ptr<const Sample> sample;
        Analysis analysis;            // with tempoMult applied
        juce::String sampleName;
        std::vector<Chop> chops;
        int effectiveGridDiv = 0;
        std::array<std::vector<Step>, kNumVariations> steps;
        std::vector<juce::AudioBuffer<float>> renderedChops;
        std::array<juce::AudioBuffer<float>, kNumVariations> loops;
        int bars = 2;
        double bpm = 120.0, sampleRate = 48000.0;

        // What this model was built from (lets the next build reuse work).
        ChopSettings chopUsed;
        double tempoMultUsed = 1.0, tempoRatioUsed = 1.0;
        std::vector<ChopFx> fxUsed;
        bool hasSample() const noexcept { return sample != nullptr && ! chops.empty(); }
    };

    class LoopLabCore : private juce::Thread, private juce::Timer
    {
    public:
        LoopLabCore();
        ~LoopLabCore() override;

        // ── Message thread ──
        Spec getSpec() const;
        void updateSpec (const std::function<void (Spec&)>& edit); // edits + schedules a rebuild
        // Async: decode, analyse, rebuild. resetSettings=false keeps the chop
        // effects, seeds and locks already in the Spec (restoring a project).
        void loadFile (const juce::File& file, bool resetSettings = true);
        juce::String getLoadError() const;
        bool isBusy() const noexcept { return busy.load() || loadPending.load(); }

        std::shared_ptr<const Model> getModel() const;               // newest finished model
        int getModelVersion() const noexcept { return modelVersion.load(); }

        // ── Audio thread ──
        const Model* getAudioModel() const noexcept { return audioModel.load (std::memory_order_acquire); }

        static juce::File libraryFolder();
        static juce::File exportsFolder();

    private:
        void run() override;
        void timerCallback() override;
        std::shared_ptr<Model> build (const Spec& spec, const std::shared_ptr<const Model>& previous);
        void publish (std::shared_ptr<Model> m);

        mutable juce::CriticalSection specLock;
        Spec spec;
        int specVersion = 0, builtVersion = -1;

        juce::File pendingFile;
        std::atomic<bool> loadPending { false }, busy { false }, loadResets { true };
        juce::String loadError;

        mutable juce::CriticalSection modelLock;
        std::shared_ptr<const Model> model;
        std::atomic<const Model*> audioModel { nullptr };
        std::atomic<int> modelVersion { 0 };

        // Models the audio thread may still be reading: freed by the timer
        // once they are at least a second old and no longer current.
        struct Retired { std::shared_ptr<const Model> m; juce::uint32 at; };
        std::vector<Retired> retired;

        juce::WaitableEvent wake;
        juce::AudioFormatManager formats;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopLabCore)
    };
}
