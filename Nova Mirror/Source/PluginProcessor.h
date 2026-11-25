#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <mutex>

#include "MirrorCore.h"
#include "NovaLicenseGate.h"

// Reference audio at the session rate, for Listen
struct ListenAudio
{
    std::vector<float> L, R;
    float ms = 0.f;          // mean square, for level-matched Listen
    double sampleRate = 48000.0;
};

// What the audio thread matches to (and can play for Listen). Immutable once published.
struct MirrorTarget
{
    NovaMirror::Profile profile;
    std::shared_ptr<const ListenAudio> listen;   // reference only
};

class NovaMirrorProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    NovaMirrorProcessor();
    ~NovaMirrorProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Nova Mirror"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    NovaLicenseGate licenseGate { *this, "nova-mirror", "Nova Mirror" };

    // ── Editor API (message thread) ─────────────────────────────────────────
    void loadReferenceFile (const juce::File& f);
    void loadReferenceBytes (const juce::String& name, const juce::MemoryBlock& data);
    void clearReference();
    void setTarget (const juce::String& kind, const juce::String& name);
    bool saveCurrentAsUserTarget (const juce::String& name);
    void deleteUserTarget (const juce::String& name);
    void selectAB (int slot);
    int  getABSlot() const noexcept { return abSlot; }

    juce::String getInfoJson() const;      // targets, current target, reference summary
    int getInfoVersion() const noexcept { return infoVersion.load(); }
    void getMeters (NovaMirror::EngineMeters& m) const { engine.getMeters (m); }
    double getListenPositionSeconds() const noexcept;
    float getOutputPeakDb() const noexcept { return outPeakDb.load(); }

    static juce::File userTargetsFolder();
    static juce::File referencesFolder();

private:
    void timerCallback() override;
    void publish (std::unique_ptr<MirrorTarget> t);
    void startReferenceJob (juce::File f);
    void publishCurrentTarget();

    NovaMirror::MatchEngine engine;
    std::atomic<MirrorTarget*> live { nullptr };
    std::unique_ptr<MirrorTarget> liveOwner;
    std::vector<std::pair<std::unique_ptr<MirrorTarget>, double>> retired;

    // target selection (message thread)
    mutable juce::CriticalSection stateLock;
    juce::String targetKind = "nova", targetName = "Nova Balance";
    juce::File referenceFile;
    std::shared_ptr<const NovaMirror::Profile> referenceProfile;   // last analysed reference
    std::shared_ptr<const ListenAudio> referenceListen;
    juce::String status;
    std::atomic<bool> loading { false };
    std::atomic<int> infoVersion { 1 };

    // A/B
    int abSlot = 0;
    juce::ValueTree abState[2];

    // audio-thread state
    double sr = 48000.0;
    juce::AudioBuffer<float> dryDelay;
    int dryWrite = 0, latency = 0;
    float powerMix = 1.f, peakGain = 1.f, wetMs = 0.f, dryMs = 0.f, matchGain = 1.f, listenGain = 1.f, listenMix = 0.f;
    double listenPos = 0.0;
    std::atomic<double> listenPosSec { 0.0 };
    std::atomic<float> outPeakDb { -100.f };
    std::vector<float> dryScratchL, dryScratchR;
    int look = 72, ringPos = 0, reqPos = 0;
    float peakCeil = 0.f;
    int ceilHold = 0;
    std::vector<std::vector<float>> wRing, dRing;
    std::vector<float> reqRing;

    juce::ThreadPool pool { 1 };
    std::mutex pendingLock;
    bool pendingReady = false;
    std::shared_ptr<const NovaMirror::Profile> pendingProfile;
    std::shared_ptr<const ListenAudio> pendingListen;
    juce::String pendingError;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMirrorProcessor)
};
