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
                            private juce::AudioIODeviceCallback,
                            private juce::Timer
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
    int  mode = 0;                                   // 0 Generate, 1 Hum it, 2 Play (UI page; saved)
    void setMode (int m) { mode = m; modeNow.store (m); publish(); bump(); }

    // play (message thread -> audio thread). kind: 0 melody pad, 1 chord pad, 3 tap key
    void padDown (int kind, int idx, int vel);
    void padUp (int kind, int idx);
    void startPlayRecording();
    void stopPlayRecording();
    bool isPlayRecording() const noexcept { return playRec.load(); }
    void clearTake();
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

    // live playing (audio thread)
    struct Ev { int sample; int pitch; int vel; bool on; bool glide; };
    void liveDown (int src, int kind, int idx, int vel, int sample, Ev* evs, int& numEv);
    void liveUp (int src, int sample, Ev* evs, int& numEv);
    void recPush (double beat, int pitch, int vel, bool on, bool chord);
    void timerCallback() override;                   // message thread: key for playing, recorded notes
    void drainRecording();

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

    // play state
    std::atomic<int> modeNow { 0 }, playTonic { 7 }, playScale { 1 }, chordNow { -1 }, padsOnMask { 0 }, tapIdx { 0 };
    struct UiEv { int src, kind, idx, vel; bool down; };
    juce::AbstractFifo uiFifo { 256 };
    std::array<UiEv, 256> uiEvs {};
    struct Src { int n = 0; int pitch[5] {}; int pad = -1; bool chord = false; };
    std::array<Src, 170> srcs {};                    // 0-127 MIDI keys, 128+ melody pads, 140+ chord pads, 150+ tap keys
    struct Strum { int src, pitch, vel, samplesLeft; };
    std::array<Strum, 64> strums {};
    int numStrums = 0;
    std::shared_ptr<const std::vector<NovaMelody::MNote>> tapList;
    std::array<std::atomic<int>, 28> chordTab {};
    int tabOctave = 99;                               // forces the first build      // 7 chords x 4 notes, worked out on the message thread
    double freeClock = 0, clockAtBlock = 0, bpsNow = 0;
    // recording what you play
    std::atomic<bool> playRec { false };
    std::atomic<double> recStartBeat { -1.0 }, recNowBeat { 0.0 };
    struct RecEv { double beat; int pitch, vel; bool on, chord; };
    juce::AbstractFifo recFifo { 4096 };
    std::array<RecEv, 4096> recEvs {};
    std::map<int, std::pair<double, int>> openRec;   // pitch -> start, vel (message thread)
    std::vector<NovaMelody::MNote> recNotes;
    std::vector<bool> recIsChord;
    std::vector<NovaMelody::MNote> playTake;          // what Play recorded (plays / exports in the Play tab)
    double playTakeLen = 16;

    NovaLicenseGate licenseGate { *this, "nova-melody", "Nova Melody" };

    JUCE_DECLARE_WEAK_REFERENCEABLE (NovaMelodyProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMelodyProcessor)
};
