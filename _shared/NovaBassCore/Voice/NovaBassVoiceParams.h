#pragma once

namespace nova::bass
{

enum class NovaVoiceRetrigger
{
    Retrigger,  // a repeated note-on while held always restarts envelopes
    Legato      // an overlapping note-on only glides pitch, never retriggers
};

enum class NovaGlideMode
{
    Always,       // every new note glides from the previous pitch
    LegatoOnly    // glide only applies when notes overlap (true legato-glide)
};

// Full deep parameter set for one synth voice - this is exactly what
// Nova Bass Lab's Synth page will expose directly, and what a
// NovaBassPreset stores. The customer plugin never sets these fields
// directly; it goes through NovaMacroEngine, which resolves the six simple
// knobs into a struct shaped like this one (see NovaMacroEngine).
struct NovaBassVoiceParams
{
    // Oscillator
    float sineTriBlend = 0.0f;     // 0 = sine, 1 = triangle
    float fineTuneCents = 0.0f;

    // Pitch envelope (808 drop)
    float pitchEnvAmountSemitones = 12.0f;
    float pitchEnvDecaySeconds = 0.25f;
    float pitchEnvCurve = 0.3f;

    // Amp envelope
    float ampAttackSeconds = 0.001f;
    float ampDecaySeconds = 0.3f;
    float ampSustain = 0.0f;
    float ampReleaseSeconds = 0.15f;

    // Transient
    float transientAmount = 0.3f;
    float transientToneHz = 1200.0f;
    float transientDecaySeconds = 0.03f;

    // Filter
    bool filterHighPass = false;
    float filterCutoffHz = 8000.0f;
    float filterResonance = 0.7f;

    // Voicing
    NovaVoiceRetrigger retrigger = NovaVoiceRetrigger::Retrigger;
    NovaGlideMode glideMode = NovaGlideMode::LegatoOnly;
    float glideTimeSeconds = 0.05f;
};

} // namespace nova::bass
