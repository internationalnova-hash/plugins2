#pragma once

#include <JuceHeader.h>
#include "Session.h"
#include "TransportState.h"

namespace NovaStudio
{
    class StudioAudioEngine : private juce::AudioIODeviceCallback
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

        void addTrack(const juce::String& name, TrackType type = TrackType::Audio);
        void removeTrack(int index);

        void setTrackVolume(int index, float volumeDb);
        void setTrackPan(int index, float pan);
        void setTrackMute(int index, bool muted);
        void setTrackSolo(int index, bool solo);
        void setTrackArm(int index, bool arm);

        int getTrackCount() const noexcept;
        const Session& getSession() const noexcept;
        Session& getSession() noexcept;

        // Plugin management (public surface for UI)
        juce::AudioPluginInstance* getTrackPlugin(int trackIndex, int pluginSlot) const;
        int getTrackPluginCount(int trackIndex) const;
        void getTrackPluginState(int trackIndex, int pluginSlot, juce::MemoryBlock& dest) const;
        void setTrackPluginState(int trackIndex, int pluginSlot, const void* data, size_t size);

        // Output metering: returns peak since last call (resets after read), range 0-1
        float getTrackPeakLevel(int trackIndex, int channel) const noexcept;

        void play();
        void stop();
        void toggleRecord();
        bool isRecording() const noexcept;

        bool saveSession(const juce::File& file) const;
        bool loadSession(const juce::File& file);
        bool exportStereoMix(const juce::File& destinationFile);

        const TransportState& getTransportState() const noexcept;
        TransportState& getTransportState() noexcept;

        double getCurrentSampleRate() const noexcept;
        int getCurrentBufferSize() const noexcept;

        juce::File getRecordingFolder()  const noexcept { return recordingFolder; }
        juce::File getLastRecordingFile() const noexcept { return currentRecordingFile; }
        int getActiveInputChannelCount() const noexcept { return cachedActiveInputChannels; }
        juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

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
            bool loadClip(const juce::File& file, double sampleRate);
            bool addPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin);
            bool removePlugin(int index);
            juce::AudioPluginInstance* getPlugin(int index) const;
            int getNumPlugins() const { return pluginChain.size(); }
            void getPluginState(int index, juce::MemoryBlock& dest) const;
            void setPluginState(int index, const void* data, size_t size);

            juce::AudioTransportSource transportSource;
                std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
                Session* sessionPtr = nullptr;
                int trackIndex = -1;
                juce::File loadedFile;
            juce::AudioBuffer<float> scratchBuffer;
            juce::AudioFormatManager formatManager;
            juce::Array<std::unique_ptr<juce::AudioPluginInstance>> pluginChain;
            void setSessionTrack(Session* s, int index) { sessionPtr = s; trackIndex = index; }

            float volumeDb = 0.0f;
            float pan = 0.0f;
            bool muted = false;
            bool solo = false;
            bool armed = false;
            bool soloModeActive = false;
            bool isPlaying = false;
            int trackChannels = 2;
            double clipStartSeconds = 0.0;
            std::atomic<float> peakLevelLeft { 0.0f };
            std::atomic<float> peakLevelRight { 0.0f };
        };

        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                              int numInputChannels,
                              float* const* outputChannelData,
                              int numOutputChannels,
                              int numSamples,
                              const juce::AudioIODeviceCallbackContext& context) override;
        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void buildTrackPlayers();
        void refreshTrackPlaybackStates();
        void updateSoloStates();
        void startRecordingInternal();
        void stopRecordingInternal();
        void createRecordingClipIfNeeded();

        juce::AudioDeviceManager deviceManager;
        juce::MixerAudioSource mixerSource;
        juce::CriticalSection playerLock;  // guards trackPlayers across audio+main threads
        juce::AudioPluginFormatManager pluginFormatManager;
        juce::KnownPluginList knownPlugins;
        Session session;
        juce::Array<std::unique_ptr<TrackPlayer>> trackPlayers;

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
    };
}
