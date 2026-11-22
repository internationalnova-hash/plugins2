#include "LinkHub.h"

namespace NovaAlign
{
    struct LinkEndpoint::Hub
    {
        juce::CriticalSection lock;
        juce::Array<LinkEndpoint*> endpoints;
        std::atomic<int> armedTargets[numGroups] {};

        // Caller holds lock.
        LinkEndpoint* findGuide (int g) const
        {
            for (auto* e : endpoints)
                if (e->getRole() == Role::guide && e->getGroup() == g)
                    return e;
            return nullptr;
        }
    };

    LinkEndpoint::LinkEndpoint()
    {
        const juce::ScopedLock sl (hub->lock);
        hub->endpoints.add (this);
    }

    LinkEndpoint::~LinkEndpoint()
    {
        setArmed (false);
        const juce::ScopedLock sl (hub->lock);
        hub->endpoints.removeFirstMatchingValue (this);
    }

    void LinkEndpoint::chooseDefaultRole()
    {
        const juce::ScopedLock sl (hub->lock);
        bool otherTarget = false;
        for (auto* e : hub->endpoints)
            if (e != this && e->getGroup() == 0 && e->getRole() == Role::target)
                otherTarget = true;

        if (otherTarget && hub->findGuide (0) == nullptr)
            role.store (static_cast<int> (Role::guide));
        else
            role.store (static_cast<int> (Role::target));
    }

    void LinkEndpoint::setRole (Role newRole)
    {
        if (newRole == Role::guide)
            setArmed (false); // a guide never captures itself as a target

        role.store (static_cast<int> (newRole));
        allocateIfGuide();
    }

    void LinkEndpoint::setGroup (int newGroup)
    {
        const bool wasArmed = armed.load();
        setArmed (false);
        group.store (juce::jlimit (0, numGroups - 1, newGroup));
        if (wasArmed)
            setArmed (true);
    }

    void LinkEndpoint::setTrackName (const juce::String& name)
    {
        const juce::ScopedLock sl (nameLock);
        trackName = name;
    }

    juce::String LinkEndpoint::getTrackName() const
    {
        const juce::ScopedLock sl (nameLock);
        return trackName;
    }

    void LinkEndpoint::prepare (double sampleRate)
    {
        preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        wasPlaying = false;
        allocateIfGuide();
    }

    void LinkEndpoint::allocateIfGuide()
    {
        if (getRole() != Role::guide || preparedSampleRate <= 0.0)
            return;

        const int needed = static_cast<int> (kMaxSeconds * preparedSampleRate);
        const juce::SpinLock::ScopedLockType sl (bufferLock);
        if (buffer.getNumSamples() != needed)
        {
            buffer.setSize (2, needed, false, true, false);
            writePosition.store (0);
            startTimelineSample.store (-1);
            recording.store (false);
        }
    }

    void LinkEndpoint::processGuideBlock (const juce::AudioBuffer<float>& input, bool isPlaying,
                                          juce::int64 timelineSample, double sampleRate) noexcept
    {
        const int numSamples = input.getNumSamples();

        if (isPlaying && ! wasPlaying)
        {
            // Transport just started: begin a fresh guide take if any target
            // in this group is waiting for one.
            if (hub->armedTargets[group.load()].load() > 0)
            {
                writePosition.store (0);
                startTimelineSample.store (timelineSample);
                recordedSampleRate.store (sampleRate);
                recording.store (true);
            }
        }
        else if (! isPlaying && wasPlaying)
        {
            recording.store (false);
        }
        wasPlaying = isPlaying;

        if (! isPlaying || ! recording.load() || numSamples <= 0)
            return;

        const int pos = writePosition.load();
        const juce::SpinLock::ScopedTryLockType sl (bufferLock);

        if (! sl.isLocked())
        {
            // Message thread is reading/allocating: leave a gap rather than
            // block, but keep the timeline position honest.
            writePosition.store (pos + numSamples);
            return;
        }

        const int capacity = buffer.getNumSamples();
        const int toCopy = juce::jmin (numSamples, capacity - pos);
        const int inChannels = input.getNumChannels();

        if (toCopy > 0 && inChannels > 0)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.copyFrom (ch, pos, input, juce::jmin (ch, inChannels - 1), 0, toCopy);
        }

        writePosition.store (juce::jmin (capacity, pos + numSamples));
    }

    void LinkEndpoint::setArmed (bool shouldBeArmed)
    {
        if (armed.exchange (shouldBeArmed) == shouldBeArmed)
            return;

        auto& counter = hub->armedTargets[group.load()];
        if (shouldBeArmed) counter.fetch_add (1);
        else               counter.fetch_sub (1);
    }

    LinkEndpoint::GroupInfo LinkEndpoint::getGroupInfo() const
    {
        GroupInfo info;
        const juce::ScopedLock sl (hub->lock);
        const int g = getGroup();

        for (auto* e : hub->endpoints)
        {
            if (e->getGroup() != g)
                continue;

            if (e->getRole() == Role::guide)
            {
                if (info.guides++ == 0)
                {
                    info.guideTrackName = e->getTrackName();
                    info.guideRecording = e->recording.load();
                    info.guideSecondsRecorded = e->writePosition.load() / juce::jmax (1.0, e->recordedSampleRate.load());
                }
            }
            else
            {
                ++info.targets;
            }
        }
        return info;
    }

    bool LinkEndpoint::copyGuideTake (juce::int64 targetStartSample, int numSamples,
                                      juce::AudioBuffer<float>& dest, double& sampleRateOut) const
    {
        const juce::ScopedLock sl (hub->lock);
        auto* guide = hub->findGuide (getGroup());
        if (guide == nullptr || guide == this)
            return false;

        const juce::SpinLock::ScopedLockType bl (guide->bufferLock);

        const int written = juce::jmin (guide->writePosition.load(), guide->buffer.getNumSamples());
        if (written < 1024)
            return false;

        const auto guideStart = guide->startTimelineSample.load();
        juce::int64 offset = 0;
        if (targetStartSample >= 0 && guideStart >= 0)
            offset = targetStartSample - guideStart;

        const int length = numSamples > 0 ? numSamples : written;
        dest.setSize (2, length, false, true, false);
        dest.clear();

        for (int i = 0; i < length; ++i)
        {
            const juce::int64 src = offset + i;
            if (src < 0 || src >= written)
                continue;
            for (int ch = 0; ch < 2; ++ch)
                dest.setSample (ch, i, guide->buffer.getSample (ch, static_cast<int> (src)));
        }

        sampleRateOut = guide->recordedSampleRate.load();
        return true;
    }
}
