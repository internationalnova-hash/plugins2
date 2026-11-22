#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace NovaAlign
{
    // Linked instances: put Nova Align on the Guide track too, set it to
    // GUIDE, and every Nova Align on a Target track in the same link group
    // can capture that Guide while it captures itself - one REC, one play
    // pass, no sidechain routing. Works in any host that runs plugins in one
    // process (all mainstream DAWs, FL Studio and Pro Tools included).
    //
    // Each processor owns one LinkEndpoint. Endpoints register with a
    // process-wide hub (SharedResourcePointer). The guide endpoint records
    // its own input into a buffer while the transport plays and at least one
    // target in its group is armed; the target copies that recording when it
    // finishes its capture, lined up by host timeline position.
    //
    // Threading: role/group/arm/copy are message-thread calls.
    // processGuideBlock() is audio-thread: no allocation, no blocking lock
    // (the buffer is guarded by a SpinLock it only try-locks).
    class LinkEndpoint
    {
    public:
        enum class Role { target = 0, guide = 1 };
        static constexpr int numGroups = 4;

        LinkEndpoint();
        ~LinkEndpoint();

        // Called once from the processor constructor, after registration.
        // Picks GUIDE if another instance in group 0 is already a Target
        // and that group has no Guide yet (the common "second insert goes
        // on the lead vocal" case). Saved project state overrides this.
        void chooseDefaultRole();

        void setRole (Role newRole);
        Role getRole() const noexcept { return static_cast<Role> (role.load()); }

        void setGroup (int newGroup);
        int getGroup() const noexcept { return group.load(); }

        void setTrackName (const juce::String& name);
        juce::String getTrackName() const;

        void prepare (double sampleRate);

        // Audio thread, GUIDE role only.
        void processGuideBlock (const juce::AudioBuffer<float>& input, bool isPlaying,
                                juce::int64 timelineSample, double sampleRate) noexcept;

        // Target side: tells this group's guide to record on the next play.
        void setArmed (bool shouldBeArmed);

        struct GroupInfo
        {
            int guides = 0;
            int targets = 0;
            juce::String guideTrackName;
            bool guideRecording = false;
            double guideSecondsRecorded = 0.0;
        };
        GroupInfo getGroupInfo() const;

        // Copies the guide take for this endpoint's group into dest, lined up
        // so dest sample 0 is timeline sample targetStartSample (pass -1 to
        // start from the guide's first recorded sample). numSamples <= 0 means
        // "everything the guide recorded". Returns false if there's no guide
        // or it recorded nothing.
        bool copyGuideTake (juce::int64 targetStartSample, int numSamples,
                            juce::AudioBuffer<float>& dest, double& sampleRateOut) const;

        static constexpr double kMaxSeconds = 180.0;

    private:
        struct Hub;
        juce::SharedResourcePointer<Hub> hub;

        void allocateIfGuide();

        std::atomic<int> role { static_cast<int> (Role::target) };
        std::atomic<int> group { 0 };
        std::atomic<bool> armed { false };

        mutable juce::CriticalSection nameLock;
        juce::String trackName;

        // Guide recording state.
        juce::SpinLock bufferLock;
        juce::AudioBuffer<float> buffer;
        double preparedSampleRate = 0.0;
        std::atomic<double> recordedSampleRate { 44100.0 };
        std::atomic<int> writePosition { 0 };
        std::atomic<juce::int64> startTimelineSample { -1 };
        std::atomic<bool> recording { false };
        bool wasPlaying = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinkEndpoint)
    };
}
