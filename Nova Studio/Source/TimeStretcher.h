#pragma once
#include <JuceHeader.h>

namespace NovaStudio
{
    // Nova Studio's own time stretcher (no third-party library).
    //
    // A phase vocoder with:
    //  - identity phase locking (Laroche & Dolson): bins around each spectral peak keep their
    //    phase relationship to the peak, which keeps tones clean instead of "phasey";
    //  - transient preservation: at detected onsets the phases are reset to the analysis
    //    phases, so drum hits stay sharp instead of smearing;
    //  - stereo coherence: phases are advanced on the mid signal and each channel keeps its
    //    own offset from it, so the stereo image doesn't wander.
    // Pitch is preserved; only duration changes.
    namespace TimeStretcher
    {
        enum class Mode
        {
            Mix,       // full mixes, pads, vocals (default)
            Beats      // drums/loops: shorter window, stronger transient handling
        };

        // ratio > 1 makes the audio longer (slower), < 1 shorter (faster). 0.25 .. 4.
        // Output length = round(input length * ratio). `progress` may return false to cancel.
        juce::AudioBuffer<float> stretch(const juce::AudioBuffer<float>& input, double sampleRate, double ratio,
                                         Mode mode = Mode::Mix,
                                         std::function<bool(float)> progress = {});

        // Tempo of a loop/song in BPM (70..180 range after octave folding), or 0 if unclear.
        double detectTempo(const juce::AudioBuffer<float>& input, double sampleRate);

        // "Funk Loop 92bpm.wav", "drums_140_BPM", "Keys 85 bpm" -> 92 / 140 / 85. 0 if none.
        double tempoFromFileName(const juce::String& fileName);
    }
}
