#pragma once

#include <JuceHeader.h>
#include "Session.h"
#include "TransportState.h"
#include "PlaybackSnapshot.h"
#include <bitset>
#include <map>

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
        /// configure (optional) runs on the new instance after it's prepared and before it goes live,
        /// so state/parameters are never changed while the audio thread is processing it.
        bool loadPluginByDescription(const juce::PluginDescription& desc, int trackIndex = -1,
                                     const std::function<void(juce::AudioPluginInstance&)>& configure = {});
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

        // ── Offline export (bounce) ─────────────────────────────────────────
        // Renders through exactly the same graph as playback (clips, MIDI, plugins, EQ,
        // sends/aux, automation, master chain), faster than real time.
        struct ExportOptions
        {
            int64_t startSample = 0;
            int64_t endSample = -1;          // -1 = end of the arrangement
            double  tailSeconds = 3.0;       // let reverbs/delays ring out (stops early on silence)
            int     bitDepth = 24;           // 16, 24 or 32 (float)
            int     soloTrack = -1;          // >= 0: render just this track (a stem)
            bool    includeMaster = true;    // master plugins + fader + limiter
            std::function<bool(float)> progress;   // return false to cancel
        };
        struct ExportResult
        {
            bool ok = false;
            juce::String error;
            float peakDb = -100.0f;
            bool clipped = false;
            double seconds = 0.0;
        };
        ExportResult exportAudio(const juce::File& destinationFile, const ExportOptions& options);
        // One file per audio/MIDI/aux track into `folder` ("<session> - <track>.wav").
        int exportStems(const juce::File& folder, const ExportOptions& options, juce::StringArray* errors = nullptr);
        int64_t getArrangementEndSample() const;

        // ── Transport helpers ───────────────────────────────────────────────
        void seek(int64_t samplePosition);
        void setTempo(double bpm);

        // Tempo changes (tempo map), positioned in bars so they stay musical.
        struct TempoChange { double bar = 1.0; double bpm = 120.0; };   // bar is 1-based, may be fractional
        std::vector<TempoChange> getTempoChanges() const;
        void setTempoChanges(const std::vector<TempoChange>& changes);
        double getTempoAt(int64_t samplePosition) const;
        void setTimeSignature(int numerator, int denominator);
        void setLoopRange(int64_t start, int64_t end, bool enabled);
        void setPunchRange(int64_t in, int64_t out, bool enabled);
        void setMetronomeEnabled(bool on);
        bool isMetronomeEnabled() const noexcept { return session.metronomeEnabled; }
        void setMetronomeVolume(float gain0to1);
        void setCountInBars(int bars);                       // 0..4
        int  getCountInBars() const noexcept { return session.countInBars; }
        bool isCountingIn() const noexcept { return countInRemaining.load() > 0; }
        void setRecordingOffsetSamples(int samples);         // manual latency trim (+ = earlier)
        int  getRecordingOffsetSamples() const noexcept { return recordingOffsetSamples.load(); }
        int  getRoundTripLatencySamples() const;

        // ── Master section ──────────────────────────────────────────────────
        void  setMasterVolume(float db);
        float getMasterVolume() const noexcept { return session.masterVolumeDb; }
        void  setMasterLimiterEnabled(bool on);
        bool  isMasterLimiterEnabled() const noexcept { return session.masterLimiter; }

        // ── Track input / monitoring / automation mode ──────────────────────
        juce::StringArray getInputChannelNames() const;
        void setTrackInputChannel(int trackIndex, int firstInputChannel);
        int  getTrackInputChannel(int trackIndex) const noexcept;
        void setTrackStereo(int trackIndex, bool stereo);
        // Output routing: "Main Out" or a bus name ("Bus 3-4 (Verb)"). Aux returns are processed
        // in dependency order, so tracks -> aux -> aux chains work.
        void setTrackOutputBus(int trackIndex, const juce::String& busName);
        static int parseBusIndex(const juce::String& busName);   // -1 for Main Out
        juce::String getBusDisplayName(int busIndex) const;       // "Bus 1-2 (Verb)"
        juce::StringArray getOutputChoices(int forTrack) const;   // Main Out + buses (no loops)

        // Freeze: renders the track's instrument/inserts to audio and plays that instead,
        // freeing the CPU. Fader, pan, EQ and sends stay live.
        bool freezeTrack(int trackIndex, juce::String* error = nullptr);
        void unfreezeTrack(int trackIndex);
        bool isTrackFrozen(int trackIndex) const noexcept;

        // ── Warp (time stretch, pitch preserved) ────────────────────────────
        // Warp a clip from its own tempo to the session tempo. followTempo: re-stretch
        // automatically (and keep its bar position) whenever the session tempo changes.
        bool warpClipToTempo(int trackIndex, int clipIndex, double sourceBpm, bool followTempo, int mode, juce::String* error = nullptr);
        // Manual stretch by a ratio (>1 longer/slower). Doesn't follow tempo.
        bool stretchClip(int trackIndex, int clipIndex, double ratio, int mode, juce::String* error = nullptr);
        void unwarpClip(int trackIndex, int clipIndex);
        // Best guess at a clip's tempo: the file name ("92bpm"), else audio analysis. 0 if unknown.
        double suggestClipTempo(int trackIndex, int clipIndex) const;
        void setTrackMonitorInput(int trackIndex, bool on);
        bool getTrackMonitorInput(int trackIndex) const noexcept;
        void setTrackAutomationMode(int trackIndex, int mode);   // 0 Read, 1 Touch, 2 Latch, 3 Write, 4 Off
        int  getTrackAutomationMode(int trackIndex) const noexcept;
        static juce::StringArray getAutomationModeNames() { return { "Read", "Touch", "Latch", "Write", "Off" }; }
        // Current (possibly automated) values, for meters/faders that follow automation.
        float getTrackDisplayVolume(int trackIndex) const noexcept;
        float getTrackDisplayPan(int trackIndex) const noexcept;

        // ── MIDI ────────────────────────────────────────────────────────────
        // Live MIDI keyboard input goes to armed MIDI tracks, or (if none) to this track.
        void setMidiInputTrack(int trackIndex) noexcept { midiInputTrack.store(trackIndex); }
        int  getMidiInputTrack() const noexcept { return midiInputTrack.load(); }
        // Creates an empty MIDI clip; returns its clip index (or -1).
        int  createMidiClip(int trackIndex, int64_t startSample, int64_t lengthSamples, const juce::String& name = {});
        // Replaces a MIDI clip's notes (beats from clip start).
        bool setMidiClipNotes(int trackIndex, int clipIndex, const juce::Array<MidiNote>& notes);
        // Imports a Standard MIDI File. Each MIDI track in the file becomes a clip on a new
        // (or the given) MIDI track. Returns the number of clips created.
        int  importMidiFile(const juce::File& midiFile, int64_t startSample, int targetTrack = -1);
        bool exportMidiClip(int trackIndex, int clipIndex, const juce::File& destination) const;
        // Sends a note to the target MIDI track's instrument (on-screen keyboards, piano roll).
        void sendLiveNote(int midiNoteNumber, float velocity, bool noteOn);

        // ── Session lifecycle ───────────────────────────────────────────────
        // Call after editing the Session directly; playback picks the change up immediately.
        // (Edits are also detected automatically within ~50 ms.)
        void notifySessionChanged();
        juce::File getAutosaveFile() const;
        juce::File findNewestAutosave() const;
        void setCurrentSessionFile(const juce::File& f) { currentSessionFile = f; }
        juce::File getCurrentSessionFile() const { return currentSessionFile; }
        bool hasUnsavedChanges() const noexcept { return lastSnapshotHash != savedHash; }
        void markSaved() noexcept { savedHash = lastSnapshotHash; }

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
        struct TrackPlayer
        {
            TrackPlayer();
            ~TrackPlayer();

            void prepare(double sampleRate, int maxBlockSize);
            void release();
            void setTrackMetadata(const Track& trackInfo);
            void setSoloMode(bool soloActive) noexcept { soloModeActive.store(soloActive); }

            bool addPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin);   // caller holds playerLock
            bool removePlugin(int index);
            bool movePlugin(int fromIndex, int toIndex);
            void setPluginBypassed(int index, bool bypassed);
            bool isPluginBypassed(int index) const;
            juce::AudioPluginInstance* getPlugin(int index) const;
            int  getPluginChainLatencySamples() const;
            void setPdcDelaySamples(int samples);
            int  getNumPlugins() const { return pluginChain.size(); }
            void getPluginState(int index, juce::MemoryBlock& dest) const;
            void setPluginState(int index, const void* data, size_t size);
            int  requiredChannels() const;

            int trackIndex = -1;
            TrackType type = TrackType::Audio;
            bool isAuxTrack = false;
            int  auxInputBusIndex = -1;

            // Mixer state (UI writes, audio reads)
            std::atomic<float> volumeDb { 0.0f }, pan { 0.0f };
            std::atomic<bool>  muted { false }, solo { false }, armed { false }, soloModeActive { false };
            std::atomic<bool>  monitorInput { false }, isStereo { true };
            std::atomic<int>   inputChannel { 0 };
            std::atomic<int>   outputBusIndex { -1 };   // -1 = master, else a send/aux bus
            std::atomic<bool>  frozen { false };        // plays its freeze file; plugins skipped
            // What's actually applied (after automation), for the UI
            std::atomic<float> displayVolumeDb { 0.0f }, displayPan { 0.0f };
            std::atomic<bool>  writingAutomation { false };   // fader/pan come from the user, not the lane

            juce::Array<std::unique_ptr<juce::AudioPluginInstance>> pluginChain;
            juce::Array<bool> pluginBypassed;
            juce::dsp::DelayLine<float> pdcDelayLine { 192000 };
            int pdcDelaySamples = 0;

            // Audio-thread working buffers (sized in prepare)
            juce::AudioBuffer<float> buf;        // track signal, requiredChannels() wide
            juce::AudioBuffer<float> clipTmp;    // file-rate samples for one clip segment
            juce::MidiBuffer midi;
            std::bitset<128> heldNotes;
            bool allNotesOffPending = false;
            float lastGainL = -1.0f, lastGainR = -1.0f;   // < 0: jump straight to the fader level
            int procChannels = 2;
            double sampleRate = 44100.0;
            int maxBlock = 512;

            std::atomic<float> peakLevelLeft  { 0.0f };
            std::atomic<float> peakLevelRight { 0.0f };

            struct EQBand {
                std::atomic<bool>  enabled { false };
                std::atomic<float> freq    { 1000.0f };
                std::atomic<float> gainDb  { 0.0f };
                std::atomic<float> q       { 0.707f };
                double b0=1,b1=0,b2=0,a1=0,a2=0;
                double z1L=0,z2L=0,z1R=0,z2R=0;
            };
            static constexpr int kNumEQBands = 6;
            std::array<EQBand, kNumEQBands> eqBands;
            std::atomic<bool> eqDirty { true };

            static constexpr int kMaxSends = 6;
            juce::AudioBuffer<float>* sendBusBuffers[8] {};
            int   sendBusIndex[kMaxSends]  = { -1,-1,-1,-1,-1,-1 };
            float sendLevels[kMaxSends]    = { -100.f,-100.f,-100.f,-100.f,-100.f,-100.f };
            bool  sendPreFader[kMaxSends]  = {};
        };

        // One armed track being recorded.
        struct RecordTake
        {
            int trackIndex = -1;
            int firstInput = 0;
            int numChannels = 2;
            juce::File file;
            std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
        };

        struct LiveMidiEvent { juce::uint8 data[3]; int size; };
        struct RecordedMidiEvent { int64_t timelinePos; int trackIndex; juce::uint8 data[3]; };

        void renderBlock(const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                         int numSamples, bool offline);
        void renderSegment(const float* const* inputs, int numInputs, int inputOffset,
                           int numSamples, int64_t position, bool playing, bool offline,
                           const PlaySnapshot* snap, juce::MidiBuffer& liveMidi);
        void renderTrack(TrackPlayer& p, const TrackPlayData* td, const float* const* inputs, int numInputs,
                         int inputOffset, int numSamples, int64_t position, bool playing, bool offline,
                         bool anySolo, const juce::MidiBuffer& liveMidi, bool isLiveMidiTarget);
        void renderClips(TrackPlayer& p, const TrackPlayData& td, int numSamples, int64_t position, bool offline);
        void renderStepSequencer(float* const* outputChannelData, int numOutputChannels, int numSamples, int64_t positionSamples);
        void renderClicks(float* const* out, int numOut, int numSamples, int64_t position, bool countingIn, const PlaySnapshot* snap);
        void writeRecording(const float* const* inputs, int numInputs, int inputOffset, int numSamples);

        // Snapshot management (message thread)
        uint64_t computeSessionHash() const;
        void publishSnapshot(bool force);
        std::shared_ptr<ClipStreamer> getStreamer(const juce::File& file, int64_t startSessionSamples, int64_t pinSessionSamples);
        std::unique_ptr<PlaySnapshot> buildSnapshot(uint64_t hash);
        void installSnapshot(std::unique_ptr<PlaySnapshot> snap);
        void handleControllerOnMessageThread(int ccNumber, int channel, int value);
        void pushLiveMidi(const juce::MidiMessage& m);
        void shiftTrackReferencesAfterRemoval(int removedIndex);
        void retireOldSnapshots(bool all);
        void prefetchAt(int64_t timelinePos);
        void applySessionToPlayers();
        void stepSequencerToSession();
        void sessionToStepSequencer();
        void autosaveIfNeeded();
        void writeAutomationTick();

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
        void createRecordingClips();
        void createMidiRecordingClips();

        juce::AudioDeviceManager deviceManager;
        juce::TimeSliceThread readAheadThread { "nova-readahead" }; // streams clip audio from disk
        juce::AudioFormatManager clipFormats;

        // Published playback snapshot (audio thread reads, message thread swaps)
        std::atomic<const PlaySnapshot*> liveSnapshot { nullptr };
        std::unique_ptr<PlaySnapshot> currentSnapshot;
        struct Retired { std::unique_ptr<PlaySnapshot> snap; juce::uint32 time; };
        std::vector<Retired> retiredSnapshots;
        std::map<juce::String, std::weak_ptr<ClipStreamer>> streamerCache;
        std::vector<std::shared_ptr<ClipStreamer>> registeredStreamers;
        uint64_t lastSnapshotHash = 0, savedHash = 0, lastAutosaveHash = 0;
        juce::uint32 lastAutosaveTime = 0;
        juce::File currentSessionFile;

        // Audio-thread buffers
        juce::AudioBuffer<float> mixBuffer;
        juce::AudioBuffer<float> silenceBuffer;
        juce::MidiBuffer liveMidiBlock;
        // Optional master safety ceiling (off by default). Transparent: does nothing until
        // a sample would pass -0.3 dBFS, then turns down just enough (instant, no latency),
        // and recovers over ~60 ms. No compression, no make-up gain, no clipping.
        struct SafetyCeiling
        {
            float ceiling = 0.966f, gain = 1.f, rel = 0.f;
            void prepare (double sr) { rel = 1.f - std::exp (-1.f / (float) (0.06 * sr)); reset(); }
            void reset() noexcept { gain = 1.f; }
            void process (float* l, float* r, int n) noexcept
            {
                for (int i = 0; i < n; ++i)
                {
                    const float pk = juce::jmax (std::abs (l[i]), std::abs (r[i]));
                    const float need = pk * gain > ceiling ? ceiling / pk : 1.f;
                    gain = need < gain ? need : gain + (1.f - gain) * rel;
                    l[i] *= gain; r[i] *= gain;
                }
            }
        } masterLimiter;
        juce::SmoothedValue<float> masterGain { 1.0f };
        std::atomic<bool> offlineRendering { false };
        int offlineSoloTrack = -1;
        int offlineFreezeTrack = -1;
        std::atomic<int> trackMaxLatency { 0 }, masterChainLatency { 0 };   // for overall delay compensation      // render this track pre-fader, no PDC (freeze)
        std::vector<int> processOrder;    // player indices: tracks, then aux in dependency order
        void updateRoutingOrder();
        bool renderWarp(Clip& clip, double ratio, int mode, juce::String* error);
        void rewarpFollowingClips(double oldTempo, double newTempo);
        // Applies a tempo edit while keeping MIDI clips, follow-tempo warped clips, loop and
        // punch ranges on the same bars (plain audio stays where it is in time).
        void applyTempoEdit(const std::function<void()>& edit);
        const PlaySnapshot* stepSnapshot = nullptr;
        bool offlineIncludeMaster = true;
        int maxBlockSize = 512;

        // Live MIDI (MIDI thread -> audio thread) and MIDI recording (audio -> message thread)
        juce::AbstractFifo liveMidiFifo { 1024 };
        std::array<LiveMidiEvent, 1024> liveMidiData;
        juce::SpinLock liveMidiWriteLock;           // several MIDI devices may push at once
        juce::AbstractFifo recMidiFifo { 8192 };
        std::array<RecordedMidiEvent, 8192> recMidiData;
        std::atomic<int> midiInputTrack { -1 };
        juce::Array<RecordedMidiEvent> recordedMidi;   // message thread, drained during recording

        // Recording
        juce::OwnedArray<RecordTake> recordTakes;
        juce::SpinLock recordLock;
        std::atomic<int64_t> recordedSamples { 0 };
        std::atomic<int64_t> recordStartPos { 0 };
        std::atomic<bool> pendingRecordStart { false };
        int64_t lastRenderEnd = -1;
        juce::AudioBuffer<float> exportBuffer;
        juce::MidiBuffer masterMidi;
        std::atomic<int> loopPassCount { 0 };
        std::array<int64_t, 64> loopPassEnds {};    // recordedSamples at each loop wrap
        std::atomic<int> recordingOffsetSamples { 0 };
        int64_t recordLoopStart = 0, recordLoopEnd = 0;

        // Metronome / count-in
        std::atomic<int64_t> countInRemaining { 0 };
        int64_t countInTotal = 0;
        double clickPhase = 0.0, clickFreq = 1000.0;
        int clickSamplesLeft = 0;
        float clickGain = 0.0f;
        std::atomic<float> metronomeGain { 0.6f };
        std::atomic<bool>  metronomeOn { false }, limiterOn { false };
        std::atomic<float> masterVolumeDbAtomic { 0.0f };

        // Automation writing (message thread)
        struct AutoWriteState { float lastVol = 0, lastPan = 0; bool latched = false; juce::uint32 lastMoveMs = 0; double lastWriteSec = -1; };
        std::vector<AutoWriteState> autoWrite;

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
        TransportState transportState;
        juce::File recordingFolder;
        juce::File currentRecordingFile;
        juce::TimeSliceThread recordingThread { "nova-recorder" };
        std::unique_ptr<juce::FileLogger> diagLogger;

        // Step sequencer playback state (audio thread writes, UI thread reads)
        std::atomic<int> currentPlayingStep { -1 };
        int stepSeqLastStep = -1;
        int64_t stepSeqExpectedPos = -1;

        // Master output clip state (audio thread writes, UI thread reads)
        std::atomic<bool>  masterOutputClipped { false };
        std::atomic<float> masterOutputPeak    { 0.0f };
    };
}
