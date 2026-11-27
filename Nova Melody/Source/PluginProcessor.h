#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MelodyCore.h"
#include "MelodySynth.h"
#include "NovaLicenseGate.h"
#include <atomic>
#include <map>
#include <memory>

class NovaMelodyProcessor : public juce::AudioProcessor,
                            private juce::AudioIODeviceCallback
{
public:
    NovaMelodyProcessor();
    ~NovaMelodyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Nova Melody"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ── actions (message thread) ────────────────────────────────────────────
    int  mode = 0;                                   // 0 Generate, 1 Hum it (UI page; saved)
    void setMode (int m) { mode = m; bump(); }
    void generateAll();                              // four new variations (keeps locked notes)
    void selectVariation (int i);
    void toggleLock (int noteIndex);
    void redoRest();                                 // new notes around the locked ones, current variation
    void clearLocks();
    void setPreview (bool play);
    bool isPreviewing() const noexcept { return preview.load(); }

    // hum
    void startRecording (bool useMic);
    void stopRecording();
    bool isRecording() const noexcept { return recording.load(); }
    void loadHumFile (const juce::File& f);
    void loadHumBytes (const juce::String& name, const juce::MemoryBlock& data);
    void remakeHum();                                // a hum setting moved
    void humExtra (int which);                       // 0 none, 1 Make it groove, 2 Answer it

    // what plays / exports right now
    std::vector<NovaMelody::MNote> currentNotes() const;
    double currentLengthBeats() const;
    juce::MemoryBlock currentMidiFile() const;
    juce::String suggestedMidiName() const;

    // ── for the editor ──────────────────────────────────────────────────────
    int getStateVersion() const noexcept { return stateVersion.load(); }
    juce::String getStateJson() const;               // notes, variations, hum, key, chords...
    juce::String getFrameJson() const;               // playhead, levels, recording
    double hostBpm() const noexcept { return bpm.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void bump() { ++stateVersion; }
    NovaMelody::Key pickedKey() const;
    NovaMelody::Key keyForGenerate() const;
    NovaMelody::GenParams genParams (juce::uint32 seed) const;
    NovaMelody::HumParams humParams() const;
    void publish();                                  // hand the notes to the audio thread
    void analyseCapture();                           // background: the recorded take -> notes

    // AudioIODeviceCallback (the Mac's mic, for humming when the host gives no input)
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int n, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void captureSamples (const float* const* in, int numIn, int n);

    // generate state
    std::array<std::vector<NovaMelody::MNote>, 4> variations;
    int selected = 0;
    juce::uint32 seedBase = 1;

    // hum state
    NovaMelody::HumResult hum;
    int humExtraMode = 0;
    std::vector<NovaMelody::MNote> humOut;           // what Hum it plays (after groove / answer)
    juce::String humStatus;
    std::atomic<bool> analysing { false };

    // chords heard on MIDI in (bar -> pitch classes), and a pitch-class histogram for the key
    std::map<int, std::vector<int>> heardChords;
    std::array<float, 12> pcWeights {};
    juce::CriticalSection chordLock;
    std::array<bool, 128> held {};
    int lastChordBar = -1;

    // audio thread playback
    struct PlayList { std::vector<NovaMelody::MNote> notes; double length = 32; };
    std::shared_ptr<const PlayList> playList;
    juce::SpinLock playLock;
    std::atomic<bool> preview { false };
    std::atomic<double> bpm { 120.0 }, playBeat { -1.0 };
    double previewBeat = 0, lastHostBeat = -1;
    bool wasPlaying = false;
    std::array<int, 128> soundingUntil {};          // pitches currently on (for clean note-offs)
    std::vector<std::pair<int, double>> active;      // pitch, off-beat (absolute running beat)
    double runningBeat = 0;
    NovaMelody::MelodySynth synth;
    double sampleRate = 48000;

    // capture
    juce::AudioBuffer<float> capture;                // mono, up to 60 s
    std::atomic<int> captureLen { 0 };
    std::atomic<bool> recording { false };
    std::atomic<float> inputLevel { 0.f };
    double captureRate = 48000, captureStartBeat = -1;
    bool captureFromMic = false;
    std::unique_ptr<juce::AudioDeviceManager> mic;
    std::atomic<int> stateVersion { 1 };

    NovaLicenseGate licenseGate { *this, "nova-melody", "Nova Melody" };

    JUCE_DECLARE_WEAK_REFERENCEABLE (NovaMelodyProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMelodyProcessor)
};
