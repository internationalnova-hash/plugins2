#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "MeterEngine.h"
#include "Profiles.h"

namespace tz
{

/** One meter's behaviour across a whole reference track. */
struct MeterStat
{
    float headline = kNoValue; // the number to compare against (see analyzeReader)
    float low = kNoValue;      // 10th percentile over the active part of the song
    float high = kNoValue;     // 90th percentile
    float max = kNoValue;
};

struct ReferenceStats
{
    juce::String name;
    double durationSec = 0, sampleRate = 0;
    int channels = 0;
    std::array<MeterStat, NumMeters> m {};

    bool valid() const { return name.isNotEmpty() && hasValue (m[LufsInt].headline); }
    juce::var toVar() const;
    static ReferenceStats fromVar (const juce::var&);
};

/** Runs the reference through the same MeterEngine the live meters use.
    `progress` gets 0..1 and returns false to cancel (result is then invalid). */
ReferenceStats analyzeReader (juce::AudioFormatReader& reader, const juce::String& name,
                              const std::function<bool (float)>& progress = {});

/** Target ranges centred on the reference, keeping `base`'s visibility and hold modes. */
Profile targetsFromReference (const ReferenceStats& ref, const Profile& base);

//==============================================================================
/** Background analysis of one file at a time. */
class ReferenceAnalyzer : private juce::Thread
{
public:
    ReferenceAnalyzer() : juce::Thread ("Nova Meter reference analysis") {}
    ~ReferenceAnalyzer() override { stopThread (4000); }

    enum class Status { Idle, Analyzing, Done, Failed };

    /** Starts analysing `file` (cancelling any analysis in progress). When
        `deleteWhenDone` is set the file is removed afterwards (uploaded temp copies). */
    void start (const juce::File& file, const juce::String& displayName, bool deleteWhenDone);
    void cancel() { stopThread (4000); status = Status::Idle; }

    Status getStatus() const { return status.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getError() const { const juce::ScopedLock sl (lock); return error; }

    /** If an analysis finished since the last call, moves its result out. Message thread. */
    bool takeResult (ReferenceStats& out);

private:
    void run() override;

    juce::CriticalSection lock;
    juce::File file;
    juce::String displayName, error;
    bool deleteFile = false;
    ReferenceStats result;
    bool hasNewResult = false;
    std::atomic<Status> status { Status::Idle };
    std::atomic<float> progress { 0.0f };
};

} // namespace tz
