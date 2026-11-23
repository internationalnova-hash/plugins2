#pragma once

#include <JuceHeader.h>

namespace NovaStudio
{
    enum class TrackType
    {
        Audio,
        Midi,
        Aux,
        Master
    };

    // A MIDI note inside a MIDI clip. Times are in beats (quarter notes) from the clip start,
    // so notes follow tempo changes.
    struct MidiNote
    {
        int    pitch = 60;
        double startBeat = 0.0;
        double lengthBeats = 1.0;
        float  velocity = 0.8f;   // 0..1
        bool operator== (const MidiNote& o) const noexcept
        {
            return pitch == o.pitch && startBeat == o.startBeat && lengthBeats == o.lengthBeats && velocity == o.velocity;
        }
    };

    struct Clip
    {
        juce::File file;
        int64_t startSample = 0;
        int64_t lengthSamples = 0;
        bool isMidi = false;
        float gainDb = 0.0f;
        int64_t fadeInSamples = 0;
        int64_t fadeOutSamples = 0;
        int64_t fileOffsetSamples = 0; // offset into the audio file where clip playback starts
        bool muted = false;
        bool locked = false;
        bool aligned = false;
        bool isPreview = false;
        int64_t alignmentOffsetSamples = 0;
        juce::File originalFile;
        int guideTrackIndex = -1;
        int guideClipIndex = -1;
        int sourceTrackIndex = -1;
        int sourceClipIndex = -1;
        int alignVersion = 0;
        juce::String alignmentTimestamp;
        float alignmentPhraseSensitivity = 0.75f;
        float alignmentConsonantPriority = 0.5f;
        juce::String alignmentSourceGuidePath;
        juce::Array<juce::var> alignmentWarpPoints;
        juce::Colour clipColor = juce::Colours::steelblue;
        juce::String name;                 // optional display name
        juce::Array<MidiNote> notes;       // MIDI clips only

        // Warp (time stretch). When warped, `file` is a rendered copy; the original stays here.
        bool    warped = false;
        juce::File warpSourceFile;
        int64_t warpSourceOffset = 0;      // original fileOffsetSamples
        int64_t warpSourceLength = 0;      // original lengthSamples
        double  warpSourceBpm = 0.0;       // 0 = manual stretch (doesn't follow tempo)
        double  warpRatio = 1.0;
        int     warpMode = 0;              // 0 Mix, 1 Beats
        bool    warpFollowTempo = false;
    };

    struct AutomationPoint
    {
        double timeSeconds = 0.0;
        float  value       = 0.0f;
        juce::var toVar() const;
        static AutomationPoint fromVar(const juce::var& var);
    };

    // An automation lane targets a single parameter on its owning track and
    // holds a time-ordered list of breakpoints; values are linearly
    // interpolated between points during playback (FL-style automation clips).
    struct AutomationLane
    {
        juce::String parameterId;   // "volume", "pan", "send1".."send6"
        bool  enabled = true;
        juce::Array<AutomationPoint> points;
        juce::var toVar() const;
        static AutomationLane fromVar(const juce::var& var);
    };

    struct Track
    {
        juce::String name;
        TrackType type = TrackType::Audio;
        bool isStereo  = true;   // true = stereo, false = mono
        float volumeDb = -18.0f;  // Pro Tools-style: -18 dB internal headroom by default
        float pan = 0.0f;
        bool muted = false;
        bool solo = false;
        bool armed = false;
        float sendLevels[6]    = { -100.0f,-100.0f,-100.0f,-100.0f,-100.0f,-100.0f };
        int   sendBusIndex[6]  = { -1,-1,-1,-1,-1,-1 };  // which send bus each slot routes to
        bool  sendPreFader[6]  = {};                      // true = pre-fader
        juce::String inputBus;   // e.g. "Bus 1-2", "Input 1" — empty = default
        juce::String outputBus;  // e.g. "Main Out", "Bus 3-4"
        int   auxInputBusIndex = -1;  // for Aux tracks: which send bus to read from
        juce::Colour colour = juce::Colours::steelblue;
        int   inputChannel = 0;       // first hardware input used when recording (stereo uses +1)
        bool  monitorInput = false;   // hear the input through this track's plugins while armed
        int   automationMode = 0;     // 0 Read, 1 Touch, 2 Latch, 3 Write, 4 Off
        bool  frozen = false;
        juce::File frozenFile;
        int64_t frozenOffsetSamples = 0;   // plugin latency trimmed off the freeze file
        bool  lockedBeforeFreeze = false;
        struct EQBand { bool enabled = false; float freq = 1000.0f; float gainDb = 0.0f; float q = 0.707f; };
        EQBand eq[6];
        juce::String groupName;  // empty = ungrouped; tracks sharing a name form a group
        bool  locked = false;    // locked tracks cannot be edited (clips, volume, etc.)
        juce::Array<AutomationLane> automationLanes;
        juce::Array<Clip> clips;

        Track() = default;
        explicit Track(const juce::String& trackName, TrackType trackType = TrackType::Audio)
            : name(trackName), type(trackType) {}

        juce::var toVar() const;
        static Track fromVar(const juce::var& var);
    };

    // FL-style macro control: a single knob that fans out to one or more target
    // parameters (addressed using the same scheme as AutomationLane::parameterId,
    // e.g. "volume", "pan", "send1".."send6", "plugin:<slot>:<paramIndex>"),
    // each scaled into its own [min,max] range as the macro value (0..1) sweeps.
    struct MacroTarget
    {
        int trackIndex = -1;
        juce::String parameterId;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        juce::var toVar() const;
        static MacroTarget fromVar(const juce::var& var);
    };

    struct MacroControl
    {
        juce::String name;
        float value = 0.5f;   // normalized 0..1
        juce::Array<MacroTarget> targets;
        juce::var toVar() const;
        static MacroControl fromVar(const juce::var& var);
    };

    struct Marker
    {
        juce::String name;
        double timeSeconds = 0.0;
        juce::var toVar() const;
        static Marker fromVar(const juce::var& var);
    };

    struct TempoPoint
    {
        double timeSeconds = 0.0;
        double bpm = 120.0;
        juce::var toVar() const;
        static TempoPoint fromVar(const juce::var& var);
    };

    class Session
    {
    public:
        Session();

        void setNovaAlignSettings(const juce::var& settings) { novaAlignSettings = settings; }
        juce::var getNovaAlignSettings() const { return novaAlignSettings; }
        void setPreviewPlaybackEnabled(bool enabled) { previewPlaybackEnabled = enabled; }
        bool isPreviewPlaybackEnabled() const { return previewPlaybackEnabled; }

        Track& addTrack(const juce::String& name, TrackType type = TrackType::Audio);
        void removeTrack(int index);
        void clear();

        int getNumTracks() const noexcept;
        const Track& getTrack(int index) const;
        Track& getTrack(int index);

        void setTempo(double bpm);
        double getTempo() const noexcept;

        void setSampleRate(double sampleRate) noexcept;
        double getSampleRate() const noexcept;

        void setName(const juce::String& projectName) noexcept;
        const juce::String& getName() const noexcept;

        int getNumMacros() const noexcept { return macros.size(); }
        MacroControl& getMacro(int index) { return macros.getReference(index); }
        const MacroControl& getMacro(int index) const { return macros.getReference(index); }
        MacroControl& addMacro(const juce::String& name);
        void removeMacro(int index);

        void addMarker(const Marker& marker);
        void removeMarker(int index);
        void renameMarker(int index, const juce::String& newName);
        void addTempoPoint(const TempoPoint& tempoPoint);

        int getNumMarkers() const noexcept;
        const Marker& getMarker(int index) const;

        bool saveToFile(const juce::File& file) const;
        bool loadFromFile(const juce::File& file);

        // Converts every sample position (clips, fades, offsets) to a new sample rate,
        // so a session opened on a device at a different rate stays in time.
        void rescaleToSampleRate(double newRate);

        // Musical time
        int  timeSigNumerator = 4;
        int  timeSigDenominator = 4;
        double getTempoAtSeconds(double seconds) const noexcept;
        double secondsToBeats(double seconds) const noexcept;
        double beatsToSeconds(double beats) const noexcept;
        const juce::Array<TempoPoint>& getTempoMap() const noexcept { return tempoMap; }
        void clearTempoMap() { tempoMap.clear(); }
        void removeTempoPoint(int index) { if (juce::isPositiveAndBelow(index, tempoMap.size())) tempoMap.remove(index); }
        // Replaces the tempo map with changes given in beats (quarter notes from the start).
        void setTempoMapFromBeats(std::vector<std::pair<double, double>> beatBpm)
        {
            std::sort(beatBpm.begin(), beatBpm.end());
            tempoMap.clear();
            for (auto& [beat, bpm] : beatBpm)
            {
                if (beat <= 0.0) { tempoBpm = juce::jlimit(10.0, 999.0, bpm); continue; }
                TempoPoint p; p.timeSeconds = beatsToSeconds(beat); p.bpm = juce::jlimit(10.0, 999.0, bpm);
                tempoMap.add(p);   // earlier points are already in, so beatsToSeconds is correct
            }
        }

        // Musical grid that follows the tempo map. Beats are quarter notes.
        double beatsPerBar() const noexcept { return timeSigNumerator * 4.0 / juce::jmax(1, timeSigDenominator); }
        double beatUnit() const noexcept { return 4.0 / juce::jmax(1, timeSigDenominator); }   // one metronome beat, in quarters
        double samplesToBeats(int64_t samples) const noexcept { return secondsToBeats((double) samples / juce::jmax(1.0, sampleRate)); }
        int64_t beatsToSamples(double beats) const noexcept { return (int64_t) std::llround(beatsToSeconds(beats) * sampleRate); }
        int64_t snapToBeatGrid(int64_t sample, double gridBeats) const noexcept
        {
            if (gridBeats <= 0.0) return sample;
            return juce::jmax((int64_t) 0, beatsToSamples(std::round(samplesToBeats(sample) / gridBeats) * gridBeats));
        }
        juce::String barBeatString(int64_t samples) const
        {
            const double q = samplesToBeats(samples) + 1.0e-9;
            const double bpb = beatsPerBar(), unit = beatUnit();
            const int bar = (int) std::floor(q / bpb);
            const double inBar = q - bar * bpb;
            const int beat = (int) std::floor(inBar / unit);
            const int ticks = (int) ((inBar - beat * unit) / unit * 1000.0);
            return juce::String(bar + 1) + "." + juce::String(beat + 1) + "." + juce::String(ticks).paddedLeft('0', 3);
        }

        // Transport/recording settings saved with the session
        bool   metronomeEnabled = false;
        float  metronomeVolume = 0.6f;
        int    countInBars = 0;
        bool   loopEnabled = false;
        int64_t loopStartSample = 0, loopEndSample = 0;
        bool   punchEnabled = false;
        int64_t punchInSample = 0, punchOutSample = 0;
        float  masterVolumeDb = 0.0f;
        bool   masterLimiter = true;

        // Opaque blobs the engine fills (step sequencer pattern, mixer extras)
        juce::var stepSequencerState;
        juce::var engineExtras;

    private:
        juce::String projectName;
        double tempoBpm = 120.0;
        double sampleRate = 44100.0;
        juce::Array<Track> tracks;
        juce::Array<MacroControl> macros;
        juce::Array<Marker> markers;
        juce::Array<TempoPoint> tempoMap;
        juce::var novaAlignSettings;
        bool previewPlaybackEnabled = true;
    };
}
