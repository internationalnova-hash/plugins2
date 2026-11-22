#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

enum class NovaLoopMode { None, Forward, PingPong };

// One multisample zone (spec section 1-2): a region of key range x velocity
// range mapped to one source sample. `buffer` is shared (not copied) so many
// zones/voices can reference the same loaded sample data - the ownership
// model a future disk-streaming engine can slot into without changing this
// struct's shape (spec section 5: "do not create architecture that makes
// future streaming impossible").
struct NovaSampleZone
{
    juce::String name;
    juce::String samplePath;

    int rootNote = 60;
    int lowKey = 0, highKey = 127;
    int lowVelocity = 1, highVelocity = 127;

    float gainDb = 0.f;
    float pan = 0.f; // -1..1
    float tuningCents = 0.f;

    juce::int64 loopStart = 0, loopEnd = 0;
    bool loopEnabled = false;
    NovaLoopMode loopMode = NovaLoopMode::None;

    // Zones sharing a roundRobinGroup id AND an identical key/velocity range
    // rotate on successive triggers (spec section 4). Zones covering
    // different (overlapping) velocity ranges for the same key range should
    // use different group ids - the sampler engine crossfades ACROSS groups
    // and rotates WITHIN a group.
    int roundRobinGroup = 0;

    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    double sourceSampleRate = 44100.0;

    bool matches (int midiNote, int velocity) const noexcept
    {
        return midiNote >= lowKey && midiNote <= highKey
            && velocity >= lowVelocity && velocity <= highVelocity;
    }
};
