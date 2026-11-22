#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Offline stereo analyzer: correlation, width, low-frequency width.
// See spec section 6.
class NovaStereoAnalyzer
{
public:
    NovaStereoAnalyzer() = default;

    void prepare (double sampleRate);
    void reset() noexcept;
    void process (const juce::AudioBuffer<float>& buffer) noexcept;

    float getCorrelation() const noexcept     { return correlation; }
    float getWidth() const noexcept           { return width; }
    float getLowFrequencyWidth() const noexcept { return lowFrequencyWidth; }
    bool  isMono() const noexcept             { return monoDetected; }

private:
    double sampleRate = 44100.0;

    double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
    double sumMidSq = 0.0, sumSideSq = 0.0;

    // One-pole lowpass (~150Hz) feeding a second mid/side ratio, so bass
    // width can be judged independently of the full-band width.
    float lpCoeff = 0.f;
    float lpStateL = 0.f, lpStateR = 0.f;
    double sumLowMidSq = 0.0, sumLowSideSq = 0.0;

    float correlation      = 1.f;
    float width             = 0.f;
    float lowFrequencyWidth = 0.f;
    bool  monoDetected      = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaStereoAnalyzer)
};
