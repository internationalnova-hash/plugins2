#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Synthetic "sung" test vocals with exactly known pitch: a glottal-style
// harmonic source shaped by fixed vowel formants (so formants stay put when
// pitch moves, like a real voice), small natural drift, breath noise.
// All test melodies are in C major and deliberately sung off-key.

struct SynthVocal
{
    juce::String name;
    juce::String description;
    juce::AudioBuffer<float> audio; // stereo
};

std::vector<SynthVocal> makeSynthVocals (double sampleRate);
