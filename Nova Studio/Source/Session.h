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
    };

    struct Track
    {
        juce::String name;
        TrackType type = TrackType::Audio;
        bool isStereo  = true;   // true = stereo, false = mono
        float volumeDb = 0.0f;
        float pan = 0.0f;
        bool muted = false;
        bool solo = false;
        bool armed = false;
        float sendLevels[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        juce::String inputBus;   // e.g. "Bus 1-2", "Input 1" — empty = default
        juce::String outputBus;  // e.g. "Main Out", "Bus 3-4"
        juce::Array<Clip> clips;

        Track() = default;
        explicit Track(const juce::String& trackName, TrackType trackType = TrackType::Audio)
            : name(trackName), type(trackType) {}

        juce::var toVar() const;
        static Track fromVar(const juce::var& var);
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

        void addMarker(const Marker& marker);
        void addTempoPoint(const TempoPoint& tempoPoint);

        int getNumMarkers() const noexcept;
        const Marker& getMarker(int index) const;

        bool saveToFile(const juce::File& file) const;
        bool loadFromFile(const juce::File& file);

    private:
        juce::String projectName;
        double tempoBpm = 120.0;
        double sampleRate = 44100.0;
        juce::Array<Track> tracks;
        juce::Array<Marker> markers;
        juce::Array<TempoPoint> tempoMap;
        juce::var novaAlignSettings;
        bool previewPlaybackEnabled = true;
    };
}
