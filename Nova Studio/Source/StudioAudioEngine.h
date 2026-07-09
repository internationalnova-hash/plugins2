#pragma once

#include <JuceHeader.h>
#include "Session.h"
#include "TransportState.h"

namespace NovaStudio
{
    class StudioAudioEngine : private juce::AudioIODeviceCallback,
                              private juce::MidiInputCallback,
                              private juce::Timer
    {
    public:
        StudioAudioEngine();
        ~StudioAudioEngine() override;

        bool initialize();
        void shutdown();

        bool setSampleRate(int newSampleRate, int newBufferSize);
        bool loadPluginOnTrack(int trackIndex, const juce::File& pluginFile);
        bool loadAudioClip(int trackIndex, const juce::File& audioFile);
        bool loadPluginByDescription(const juce::PluginDescription& desc, int trackIndex = -1);
        bool removePluginFromTrack(int trackIndex, int pluginSlot);
        bool movePluginInTrack(int trackIndex, int fromSlot, int toSlot);
        void setPluginBypassed(int trackIndex, int pluginSlot, bool bypassed);
        bool isPluginBypassed(int trackIndex, int pluginSlot) const;

        void addTrack(const juce::String& name, TrackType type = TrackType::Audio);
        void removeTrack(int index);

        void setTrackVolume(int index, float volumeDb);
        void setTrackPan(int index, float pan);
        void setTrackMute(int index, bool muted);
        void setTrackSolo(int index, bool solo);
        void setTrackArm(int index, bool arm);

        // Track organization: colour-coding, named groups (FL-style), and locking
        void setTrackColour(int index, juce::Colour colour);
        juce::Colour getTrackColour(int index) const noexcept;
        void setTrackGroup(int index, const juce::String& groupName);
        juce::String getTrackGroup(int index) const noexcept;
        void setTrackLocked(int index, bool locked);
        bool isTrackLocked(int index) const noexcept;

        // Automation lanes — FL-style automation clips: a time-ordered list of
        // breakpoints linearly interpolated during playback and applied to the
        // named parameter ("volume", "pan", "send1".."send6") each audio block.
        int  addAutomationLane(int trackIndex, const juce::String& parameterId);
        void removeAutomationLane(int trackIndex, int laneIndex);
        void setAutomationLaneEnabled(int trackIndex, int laneIndex, bool enabled);
        int  getNumAutomationLanes(int trackIndex) const noexcept;
        const AutomationLane* getAutomationLane(int trackIndex, int laneIndex) const;
        int  addAutomationPoint(int trackIndex, int laneIndex, double timeSeconds, float value);
        void moveAutomationPoint(int trackIndex, int laneIndex, int pointIndex, double timeSeconds, float value);
        void removeAutomationPoint(int trackIndex, int laneIndex, int pointIndex);
        static float evaluateAutomationLane(const AutomationLane& lane, double timeSeconds);

        int getTrackCount() const noexcept;
        const Session& getSession() const noexcept;
        Session& getSession() noexcept;

        // Plugin management (public surface for UI)
        juce::AudioPluginInstance* getTrackPlugin(int trackIndex, int pluginSlot) const;
        int getTrackPluginCount(int trackIndex) const;
        void getTrackPluginState(int trackIndex, int pluginSlot, juce::MemoryBlock& dest) const;
        void setTrackPluginState(int trackIndex, int pluginSlot, const void* data, size_t size);

        // Master channel plugin chain.
        // Pass kMasterTrackIndex (-2) as trackIndex to getTrackPlugin /
        // removePluginFromTrack / loadPluginByDescription for master inserts.
        static constexpr int kMasterTrackIndex  = -2;
        static constexpr int kMaxMasterPlugins  = 10;
        bool addMasterPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin);
        bool removeMasterPlugin(int slot);
        juce::AudioPluginInstance* getMasterPlugin(int slot) const;
        int  getMasterPluginCount() const;

        // Output metering: returns peak since last call (resets after read), range 0-1
        float getTrackPeakLevel(int trackIndex, int channel) const noexcept;

        // Master output clip indicator: true if output hit 0 dBFS since last call.
        // Calling this resets the flag (Pro Tools style — light stays until you read it).
        bool readAndClearMasterClip() noexcept { return masterOutputClipped.exchange(false); }
        float getMasterOutputPeak() const noexcept { return masterOutputPeak.load(); }
        void  setTrackEQBand(int trackIndex, int band, bool enabled, float freq, float gainDb, float q);

        // Send buses: 8 stereo buses named "Bus 1-2" through "Bus 15-16"
        static constexpr int kNumSendBuses = 8;
        static juce::String sendBusName(int busIndex)
        {
            return "Bus " + juce::String(busIndex * 2 + 1) + "-" + juce::String(busIndex * 2 + 2);
        }

        // Macro controls — one knob fanning out to many target parameters,
        // addressed with the same scheme as AutomationLane::parameterId.
        void setMacroValue(int macroIndex, float normalizedValue);
        void applyMacroTargets(const MacroControl& macro);
        void applyParameterTarget(const MacroTarget& target, float normalizedValue);

        // ── MIDI Learn — bind incoming MIDI CC messages to macro knobs or
        // directly to track/plugin parameters (same addressing scheme as
        // automation lanes / macro targets). ────────────────────────────────
        struct MidiCCBinding
        {
            int ccNumber  = -1;
            int channel   = 0;       // 0 = any channel
            int macroIndex = -1;     // >= 0: drives a macro; otherwise uses `target`
            MacroTarget target;
            juce::var toVar() const;
            static MidiCCBinding fromVar(const juce::var& v);
        };

        void beginMidiLearnForMacro(int macroIndex);
        void beginMidiLearnForTarget(const MacroTarget& target);
        void cancelMidiLearn();
        bool isAwaitingMidiLearn() const noexcept { return midiLearnPending; }

        int  getNumMidiBindings() const noexcept { return midiBindings.size(); }
        const MidiCCBinding& getMidiBinding(int index) const { return midiBindings.getReference(index); }
        void removeMidiBinding(int index);
        juce::String describeMidiBinding(int index) const;

        std::function<void(int bindingIndex)> onMidiLearned;   // fired on UI thread when a binding is captured

        // Send routing per track
        void  setTrackSendLevel(int trackIndex, int sendIndex, float db);
        float getTrackSendLevel(int trackIndex, int sendIndex) const noexcept;
        void  setTrackSendBus(int trackIndex, int sendIndex, int busIndex);   // -1 = unassigned
        int   getTrackSendBus(int trackIndex, int sendIndex) const noexcept;
        void  setTrackSendPreFader(int trackIndex, int sendIndex, bool preFader);
        bool  getTrackSendPreFader(int trackIndex, int sendIndex) const noexcept;

        // Aux track management
        int   addAuxTrack(const juce::String& name, int busIndex);  // returns track index
        void  setAuxTrackBus(int trackIndex, int busIndex);

        // Step sequencer sample playback
        void setStepSeqSample(int row, const juce::String& filePath);
        void setStepSeqStep(int row, int step, bool active);

        // New step sequencer control methods
        void setStepSeqActiveStepCount(int count);          // 16, 32, or 64
        void setStepSeqSwing(float swing);                  // 0.0-1.0
        void setStepSeqGroove(int templateIndex);           // 0..StepSequencerState::kNumGrooveTemplates-1
        int  getStepSeqGroove() const noexcept;
        void setStepSeqVelocity(int row, int step, float velocity);
        void setStepSeqRowVolume(int row, float vol);
        void setStepSeqRowPan(int row, float pan);
        void setStepSeqRowPitch(int row, float semitones);
        void setStepSeqRowMuted(int row, bool muted);
        int  getStepSeqCurrentStep() const noexcept;        // returns which step is playing (-1 if stopped)

        // FL-style per-channel sampler engine: AHDSR amplitude envelope, filter,
        // LFO modulation, and sample start/end/loop region — all per row.
        void setStepSeqRowEnvelope(int row, bool enabled, float attackSecs, float holdSecs,
                                   float decaySecs, float sustainLevel, float releaseSecs);
        void setStepSeqRowFilter(int row, bool enabled, int filterType /*0=LP,1=HP*/,
                                 float cutoffHz, float resonance);
        void setStepSeqRowLFO(int row, bool enabled, int target /*0=pitch,1=volume,2=cutoff*/,
                              float rateHz, float depth);
        void setStepSeqRowSampleRegion(int row, float startFrac, float endFrac, bool loop);

        // FL-style "channel settings" engine — AHDSR envelope, filter, LFO and
        // sample region/loop, all per step-sequencer row (one instance per channel).
        struct ChannelEngine
        {
            // AHDSR amplitude envelope (Attack-Hold-Decay-Sustain-Release)
            bool   envEnabled = false;
            float  envAttack  = 0.001f;   // seconds
            float  envHold    = 0.0f;     // seconds
            float  envDecay   = 0.1f;     // seconds
            float  envSustain = 1.0f;     // 0..1 sustain level
            float  envRelease = 0.05f;    // seconds
            int    envStage   = 0;        // 0=idle 1=attack 2=hold 3=decay 4=sustain 5=release
            double envPos     = 0.0;      // samples elapsed within current stage
            float  envLevel   = 0.0f;     // current envelope output, 0..1

            // Filter — single lowpass/highpass biquad with cutoff + resonance
            bool   filterEnabled   = false;
            int    filterType      = 0;       // 0 = lowpass, 1 = highpass
            float  filterCutoffHz  = 8000.0f;
            float  filterResonance = 0.707f;
            bool   filterDirty     = true;    // recompute coefficients on next process
            double fb0=1, fb1=0, fb2=0, fa1=0, fa2=0;     // cached biquad coefficients
            double fz1L=0, fz2L=0, fz1R=0, fz2R=0;        // Direct Form II Transposed state

            // LFO — modulates pitch, volume, or filter cutoff
            bool   lfoEnabled = false;
            int    lfoTarget  = 0;     // 0 = pitch, 1 = volume, 2 = filter cutoff
            float  lfoRateHz  = 4.0f;
            float  lfoDepth   = 0.0f;  // 0..1
            double lfoPhase   = 0.0;   // 0..1, wraps

            // Sample region — start/end as a 0..1 fraction of the loaded sample,
            // with optional looping within that region
            float sampleStart = 0.0f;
            float sampleEnd   = 1.0f;
            bool  loopEnabled = false;
        };

        struct StepSequencerState {
            static constexpr int kNumRows  = 8;
            static constexpr int kNumSteps = 64;  // max supported
            int   activeStepCount = 16;            // current pattern length: 16, 32, or 64
            float swing = 0.0f;                    // 0.0 = no swing, 0.5 = heavy

            // FL-style groove/accent template — applies a per-step velocity
            // multiplier on top of the per-step velocity the user drew in.
            // 0 = Straight (no accent), see kGrooveAccentTables in the .cpp.
            int grooveTemplate = 0;
            static constexpr int kNumGrooveTemplates = 5;
            static const char* grooveTemplateName(int index);
            static float grooveAccentForStep(int templateIndex, int stepIndex);

            std::array<ChannelEngine, kNumRows> channelEngines;

            juce::String             sampleFiles[kNumRows];
            bool                     steps[kNumRows][kNumSteps] {};
            float                    velocities[kNumRows][kNumSteps];
            float                    rowVolumes[kNumRows];
            float                    rowPans[kNumRows];
            float                    rowPitches[kNumRows] {};   // semitones, FL-style per-channel pitch
            bool                     rowMuted[kNumRows] {};

            juce::AudioBuffer<float> sampleBuffers[kNumRows];
            bool                     sampleLoaded[kNumRows]   {};
            int64_t                  playPositions[kNumRows]  {};
            double                   playPosFrac[kNumRows]    {};  // fractional read position for pitch-shifted playback
            bool                     rowTriggered[kNumRows]   {};

            StepSequencerState()
            {
                for (int r = 0; r < kNumRows; ++r)
                {
                    rowVolumes[r] = 1.0f;
                    rowPans[r]    = 0.0f;
                    for (int s = 0; s < kNumSteps; ++s)
                        velocities[r][s] = 1.0f;
                }
            }
        };
        StepSequencerState stepSeq;

        void play();
        void stop();
        void toggleRecord();
        bool isRecording() const noexcept;

        // One-shot sample audition/preview (mixed directly into the main output)
        void previewSample(const juce::File& file);
        void stopPreviewSample();

        // On-screen piano-key preview — a small built-in sine synth so clicking
        // keys in the Piano Roll/Channel Rack produces audible feedback, FL
        // Studio-style, without requiring full MIDI routing into hosted plugins.
        void previewPianoKey(int midiNoteNumber, bool noteOn);

        bool saveSession(const juce::File& file) const;
        bool loadSession(const juce::File& file);
        void newSession();
        bool exportStereoMix(const juce::File& destinationFile);

        const TransportState& getTransportState() const noexcept;
        TransportState& getTransportState() noexcept;

        double getCurrentSampleRate() const noexcept;
        int getCurrentBufferSize() const noexcept;

        juce::File getRecordingFolder()  const noexcept { return recordingFolder; }
        juce::File getLastRecordingFile() const noexcept { return currentRecordingFile; }
        int getActiveInputChannelCount() const noexcept { return cachedActiveInputChannels; }
        juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

        // Exposed so the UI can host a juce::PluginListComponent (Plugin Manager)
        // directly against the engine's existing format manager / known-plugin list.
        juce::AudioPluginFormatManager& getPluginFormatManager() noexcept { return pluginFormatManager; }
        juce::KnownPluginList& getKnownPluginList() noexcept { return knownPlugins; }

        static juce::File getDefaultSessionsFolder()
        {
            return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("NovaStudio")
                       .getChildFile("Sessions");
        }

    private:
        struct TrackPlayer : public juce::AudioSource
        {
            TrackPlayer();
            ~TrackPlayer() override;

            void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
            void releaseResources() override;
            void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

            void setTrackMetadata(const Track& trackInfo);
            void setPlaying(bool isPlaying);
            void setSoloMode(bool soloActive);
            double currentTransportSeconds = 0.0;
            void setPlaybackPosition(double transportSeconds) { currentTransportSeconds = transportSeconds; }
            void setLoopActive(bool looping);
            bool loadClip(const juce::File& file, double sampleRate); // call from main thread only
            void preloadClipAtPosition(double transportSeconds);      // safe to call before play()
            bool addPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin);
            bool removePlugin(int index);
            bool movePlugin(int fromIndex, int toIndex);
            void setPluginBypassed(int index, bool bypassed);
            bool isPluginBypassed(int index) const;
            juce::AudioPluginInstance* getPlugin(int index) const;
            int  getPluginChainLatencySamples() const;
            void setPdcDelaySamples(int samples);
            int getNumPlugins() const { return pluginChain.size(); }
            void getPluginState(int index, juce::MemoryBlock& dest) const;
            void setPluginState(int index, const void* data, size_t size);

            juce::TimeSliceThread* readAheadThread = nullptr; // shared, owned by engine
            juce::AudioTransportSource transportSource;
                std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
                Session* sessionPtr = nullptr;
                int trackIndex = -1;
                juce::File loadedFile;
            juce::AudioBuffer<float> scratchBuffer;
            juce::AudioFormatManager formatManager;
            juce::Array<std::unique_ptr<juce::AudioPluginInstance>> pluginChain;
            juce::Array<bool> pluginBypassed;

            // Plugin delay compensation — aligns this track's output with the
            // slowest (highest-latency) track's plugin chain in the session.
            juce::dsp::DelayLine<float> pdcDelayLine { 192000 };
            int pdcDelaySamples = 0;
            void setSessionTrack(Session* s, int index) { sessionPtr = s; trackIndex = index; }

            float volumeDb = 0.0f;
            float pan = 0.0f;
            bool muted = false;
            bool solo = false;
            bool armed = false;
            bool soloModeActive = false;
            std::atomic<bool> isPlaying { false };
            std::atomic<bool> needsClipLoad { false }; // set by audio callback, cleared by main-thread timer
            int trackChannels = 2;
            double clipStartSeconds = 0.0;
            std::atomic<float> peakLevelLeft  { 0.0f };
            std::atomic<float> peakLevelRight { 0.0f };

            struct EQBand {
                std::atomic<bool>  enabled { false };  // disabled by default — flat until user enables
                std::atomic<float> freq    { 1000.0f };
                std::atomic<float> gainDb  { 0.0f };
                std::atomic<float> q       { 0.707f };
                // Biquad state (audio thread only — not atomic)
                double b0=1,b1=0,b2=0,a1=0,a2=0;
                double z1L=0,z2L=0,z1R=0,z2R=0;
            };
            static constexpr int kNumEQBands = 6;
            std::array<EQBand, kNumEQBands> eqBands;
            bool eqDirty { true };

            // Send bus wiring (set by engine after buildTrackPlayers)
            static constexpr int kMaxSends = 6;
            juce::AudioBuffer<float>* sendBusBuffers[8] {};   // pointers to engine's bus buffers
            int   sendBusIndex[kMaxSends]  = { -1,-1,-1,-1,-1,-1 };
            float sendLevels[kMaxSends]    = { -100.f,-100.f,-100.f,-100.f,-100.f,-100.f };
            bool  sendPreFader[kMaxSends]  = {};

            // Aux track: if true, reads from auxInputBus instead of clips
            bool isAuxTrack = false;
            int  auxInputBusIndex = -1;  // which bus to read from
        };

        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                              int numInputChannels,
                              float* const* outputChannelData,
                              int numOutputChannels,
                              int numSamples,
                              const juce::AudioIODeviceCallbackContext& context) override;
        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
        // FL-style per-channel sampler engine helpers (AHDSR envelope + filter)
        static float  advanceChannelEnvelope(ChannelEngine& ce, double sampleRate);
        static double applyChannelFilter(ChannelEngine& ce, int channel, double sample,
                                          double sampleRate, float lfoValue);

        void timerCallback() override;
        void buildTrackPlayers();
        void updatePluginDelayCompensation();
        void refreshTrackPlaybackStates();
        void updateSoloStates();
        void startRecordingInternal();
        void stopRecordingInternal();
        void createRecordingClipIfNeeded();

        juce::AudioDeviceManager deviceManager;
        juce::MixerAudioSource mixerSource;
        juce::TimeSliceThread readAheadThread { "nova-readahead" }; // shared across all TrackPlayers

        // MIDI Learn state
        juce::Array<MidiCCBinding> midiBindings;
        std::atomic<bool> midiLearnPending { false };
        int midiLearnMacroIndex = -1;
        MacroTarget midiLearnTarget;
        bool midiLearnIsForMacro = false;

        // Sample audition / preview — independent one-shot transport mixed into output
        juce::AudioFormatManager previewFormatManager;
        juce::AudioTransportSource previewTransport;
        std::unique_ptr<juce::AudioFormatReaderSource> previewReaderSource;
        juce::AudioBuffer<float> previewScratchBuffer;
        std::atomic<bool> previewActive { false };

        // Piano-key preview synth — single-voice sine with a short attack/release
        std::atomic<bool> pianoKeyActive     { false };
        std::atomic<bool> pianoKeyReleasing  { false };
        double pianoKeyPhase            = 0.0;
        double pianoKeyPhaseIncrement   = 0.0;
        float  pianoKeyEnvelope         = 0.0f;

        juce::CriticalSection playerLock;  // guards trackPlayers across audio+main threads
        juce::AudioPluginFormatManager pluginFormatManager;
        juce::KnownPluginList knownPlugins;
        Session session;
        juce::Array<std::unique_ptr<TrackPlayer>> trackPlayers;

        // Master channel plugin chain — processed on the final mixed buffer
        juce::OwnedArray<juce::AudioPluginInstance> masterPlugins;
        juce::CriticalSection masterPluginLock;

        // Send bus buffers — cleared each block, written by regular TrackPlayers,
        // read by Aux TrackPlayers.
        juce::AudioBuffer<float> sendBusBuffers[kNumSendBuses];
        void prepareSendBuses(int numChannels, int numSamples);
        void clearSendBuses(int numSamples);
        void syncTrackPlayerSends(int trackIndex);

        double currentSampleRate = 44100.0;
        int currentBufferSize = 512;
        std::atomic<bool> recordingActive { false };
        int cachedActiveInputChannels = 0;
        int64_t recordingStartSample = 0;
        int recordingWriterChannels = 1;  // actual channel count used by the writer
        TransportState transportState;
        juce::File recordingFolder;
        juce::File currentRecordingFile;
        juce::TimeSliceThread recordingThread { "nova-recorder" };
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recordingWriter;
        int64_t recordingSampleCount = 0;
        std::unique_ptr<juce::FileLogger> diagLogger;
        std::atomic<int64_t> diagBlocksReceived { 0 };  // blocks seen while recordingActive

        // Step sequencer playback state (audio thread writes, UI thread reads)
        std::atomic<int> currentPlayingStep { -1 };

        // Master output clip state (audio thread writes, UI thread reads)
        std::atomic<bool>  masterOutputClipped { false };
        std::atomic<float> masterOutputPeak    { 0.0f };
    };
}
