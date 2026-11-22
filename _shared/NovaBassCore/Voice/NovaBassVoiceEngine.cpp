#include "NovaBassVoiceEngine.h"
#include <algorithm>
#include <cmath>

namespace nova::bass
{

void NovaBassVoiceEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;

    oscillator.prepare (sampleRate);
    pitchEnvelope.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
    transientGenerator.prepare (spec);
    filter.prepare (spec);

    ampEnvelope.setSampleRate (sampleRate);

    updateGlideCoefficient();
}

void NovaBassVoiceEngine::reset() noexcept
{
    oscillator.reset();
    pitchEnvelope.reset();
    transientGenerator.reset();
    filter.reset();
    ampEnvelope.reset();
    heldNotes.clear();
    hasPlayedAnyNote = false;
}

void NovaBassVoiceEngine::setParameters (const NovaBassVoiceParams& params) noexcept
{
    currentParams = params;

    pitchEnvelope.setParameters (params.pitchEnvAmountSemitones, params.pitchEnvDecaySeconds, params.pitchEnvCurve);
    transientGenerator.setParameters (params.transientAmount, params.transientToneHz, params.transientDecaySeconds);
    filter.setHighPass (params.filterHighPass);
    filter.setCutoffHz (params.filterCutoffHz);
    filter.setResonance (params.filterResonance);

    juce::ADSR::Parameters ampParams;
    ampParams.attack = juce::jmax (0.0001f, params.ampAttackSeconds);
    ampParams.decay = juce::jmax (0.0001f, params.ampDecaySeconds);
    ampParams.sustain = juce::jlimit (0.0f, 1.0f, params.ampSustain);
    ampParams.release = juce::jmax (0.0001f, params.ampReleaseSeconds);
    ampEnvelope.setParameters (ampParams);

    updateGlideCoefficient();
}

void NovaBassVoiceEngine::updateGlideCoefficient() noexcept
{
    const float glideTime = juce::jmax (0.0005f, currentParams.glideTimeSeconds);
    glideCoefficient = std::exp (-1.0f / (glideTime * (float) sampleRate));
}

float NovaBassVoiceEngine::midiNoteToHz (int midiNote, float fineTuneCents) noexcept
{
    const float noteWithTune = (float) midiNote + (fineTuneCents / 100.0f);
    return 440.0f * std::pow (2.0f, (noteWithTune - 69.0f) / 12.0f);
}

void NovaBassVoiceEngine::beginGlideTo (float newTargetHz) noexcept
{
    targetFrequencyHz = newTargetHz;
}

void NovaBassVoiceEngine::noteOn (int midiNote, float /*velocity*/) noexcept
{
    // Remove any existing occurrence so re-pressing an already-held note
    // moves it to the top of the stack rather than duplicating it.
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), midiNote), heldNotes.end());
    heldNotes.push_back (midiNote);

    const bool wasIdle = ! hasPlayedAnyNote;
    const float newFreq = midiNoteToHz (midiNote, currentParams.fineTuneCents);

    hasPlayedAnyNote = true;

    if (wasIdle)
    {
        // Nothing was sounding - snap directly to the new pitch (no glide
        // reference exists yet) and retrigger everything fresh.
        currentFrequencyHz = newFreq;
        targetFrequencyHz = newFreq;
        oscillator.reset();
        pitchEnvelope.noteOn();
        transientGenerator.noteOn();
        ampEnvelope.noteOn();
        return;
    }

    const bool shouldGlide = (currentParams.glideMode == NovaGlideMode::Always) ||
                              (currentParams.glideMode == NovaGlideMode::LegatoOnly);
    // Both glide modes glide here because we only reach this branch when a
    // note is already held (i.e. a genuine overlap) - LegatoOnly's whole
    // point is "only when overlapping", which is exactly this case.
    if (shouldGlide)
        beginGlideTo (newFreq);
    else
    {
        currentFrequencyHz = newFreq;
        targetFrequencyHz = newFreq;
    }

    if (currentParams.retrigger == NovaVoiceRetrigger::Retrigger)
    {
        pitchEnvelope.noteOn();
        transientGenerator.noteOn();
        ampEnvelope.noteOn();
    }
    // Legato: envelopes keep running uninterrupted, only pitch moves.
}

void NovaBassVoiceEngine::noteOff (int midiNote) noexcept
{
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), midiNote), heldNotes.end());

    if (! heldNotes.empty())
    {
        // Fall back to the next held note - always a glide, never a
        // retrigger, matching standard monophonic-synth behavior.
        const float fallbackFreq = midiNoteToHz (heldNotes.back(), currentParams.fineTuneCents);
        beginGlideTo (fallbackFreq);
        return;
    }

    ampEnvelope.noteOff();
    transientGenerator.noteOff();
}

float NovaBassVoiceEngine::getNextSample() noexcept
{
    if (std::abs (currentFrequencyHz - targetFrequencyHz) > 0.001f)
        currentFrequencyHz = targetFrequencyHz + (currentFrequencyHz - targetFrequencyHz) * glideCoefficient;
    else
        currentFrequencyHz = targetFrequencyHz;

    const float pitchOffsetSemitones = pitchEnvelope.getNextSample();
    const float effectiveFreqHz = currentFrequencyHz * std::pow (2.0f, pitchOffsetSemitones / 12.0f);

    const float oscSample = oscillator.getNextSample (effectiveFreqHz, currentParams.sineTriBlend);
    const float filtered = filter.processSample (oscSample);
    const float envValue = ampEnvelope.getNextSample();
    const float transientSample = transientGenerator.getNextSample();

    return (filtered * envValue) + transientSample;
}

bool NovaBassVoiceEngine::isActive() const noexcept
{
    return ampEnvelope.isActive() || ! heldNotes.empty();
}

} // namespace nova::bass
