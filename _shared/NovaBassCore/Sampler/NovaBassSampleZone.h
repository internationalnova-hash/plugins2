#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

namespace nova::bass
{

enum class NovaBassLoopMode { None, Forward, PingPong };

// Same shape as Nova Keys' NovaSampleZone (_shared/NovaKeysCore/Sampler/
// NovaSampleZone.h) with one addition: `category` lets Nova Bass Lab's
// constrained generators (built in Milestone B) filter/tag which zones
// belong to which bass category without a separate lookup table. Buffer is
// shared (not copied) for the same disk-streaming-friendly reason as Keys.
struct NovaBassSampleZone
{
    juce::String name;
    juce::String samplePath;
    juce::String category; // e.g. "808", "Sub", "Reese" - informational, not used for zone selection

    int rootNote = 36;
    int lowKey = 0, highKey = 127;
    int lowVelocity = 1, highVelocity = 127;

    float gainDb = 0.f;
    float pan = 0.f; // -1..1
    float tuningCents = 0.f;

    juce::int64 loopStart = 0, loopEnd = 0;
    bool loopEnabled = false;
    NovaBassLoopMode loopMode = NovaBassLoopMode::None;

    // Zones sharing a roundRobinGroup id AND an identical key/velocity range
    // rotate on successive triggers; zones covering different velocity
    // ranges for the same key range should use different group ids.
    int roundRobinGroup = 0;

    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    double sourceSampleRate = 44100.0;

    bool matches (int midiNote, int velocity) const noexcept
    {
        return midiNote >= lowKey && midiNote <= highKey
            && velocity >= lowVelocity && velocity <= highVelocity;
    }
};

} // namespace nova::bass
