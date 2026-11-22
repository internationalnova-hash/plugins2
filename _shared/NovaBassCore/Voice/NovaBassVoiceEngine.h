#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

#include "NovaBassVoiceParams.h"
#include "../Synth/NovaBassOscillator.h"
#include "../PitchEnvelope/NovaPitchEnvelope.h"
#include "../Transient/NovaTransientGenerator.h"
#include "../Filter/NovaBassFilter.h"

namespace nova::bass
{

// Single-voice-with-portamento monophonic synth core - the 808/lead voice
// path for Nova Bass. Deliberately NOT built on Nova Keys' voice-pool
// pattern (NovaKeysEngine): that pool always allocates a fresh voice per
// note-on and has no concept of reusing an already-sounding voice for
// legato, which is precisely the behavior a mono bass voice needs. Confirmed
// via research that nothing in this catalog implements mono/legato/glide -
// this class is net-new.
//
// A held-note stack gives correct "release back to the previous held note"
// mono-synth behavior: releasing the top note while another is still held
// glides back to it rather than going silent.
class NovaBassVoiceEngine
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setParameters (const NovaBassVoiceParams& params) noexcept;

    void noteOn (int midiNote, float velocity) noexcept;
    void noteOff (int midiNote) noexcept;

    // Advances the voice by one sample and returns its summed output
    // (oscillator through filter and amp envelope, plus the transient
    // layer). Realtime-safe: no allocation, no locks.
    float getNextSample() noexcept;

    bool isActive() const noexcept;
    bool isHoldingAnyNote() const noexcept { return ! heldNotes.empty(); }

private:
    static float midiNoteToHz (int midiNote, float fineTuneCents) noexcept;
    void beginGlideTo (float newTargetHz) noexcept;
    void updateGlideCoefficient() noexcept;

    double sampleRate = 44100.0;
    NovaBassVoiceParams currentParams;

    NovaBassOscillator oscillator;
    NovaPitchEnvelope pitchEnvelope;
    NovaTransientGenerator transientGenerator;
    NovaBassFilter filter;
    juce::ADSR ampEnvelope;

    std::vector<int> heldNotes;

    float currentFrequencyHz = 110.0f;
    float targetFrequencyHz = 110.0f;
    float glideCoefficient = 0.0f;
    bool hasPlayedAnyNote = false;
};

} // namespace nova::bass
