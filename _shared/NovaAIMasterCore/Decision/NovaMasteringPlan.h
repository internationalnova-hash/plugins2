#pragma once

// Plain POD output of the decision engine. No JUCE UI types, no APVTS.
// Every DSP stage below reads its own sub-struct only - keeps NovaMasteringChain's
// stages decoupled from how the plan was derived (analysis+style vs. hand-tuned).
// Field set matches Nova AI Master spec section 10-11.
struct NovaMasteringPlan
{
    struct CorrectiveEQ
    {
        bool  active        = false;
        float lowShelfFreq  = 80.f;
        float lowShelfDb    = 0.f;
        float lowMidFreq    = 350.f;
        float lowMidDb      = 0.f;
        float highMidFreq   = 3000.f;
        float highMidDb     = 0.f;
        float highShelfFreq = 10000.f;
        float highShelfDb   = 0.f;
    } correctiveEQ;

    struct ResonanceControl
    {
        bool  active   = false;
        float amount   = 0.f; // 0-1, overall suppression strength
    } resonanceControl;

    struct Compression
    {
        bool  active       = false;
        float thresholdDb  = 0.f;
        float ratio        = 1.f;
        float attackMs     = 10.f;
        float releaseMs    = 100.f;
        float makeupDb     = 0.f;
    } compression;

    struct Saturation
    {
        bool  active = false;
        float drive  = 0.f; // 0-1
        float mix    = 0.f; // 0-1
    } saturation;

    struct Stereo
    {
        bool  active       = false;
        float widthAmount  = 1.f; // 1 = unchanged, <1 narrower, >1 wider
        float bassMonoHz   = 150.f;
    } stereo;

    struct Clipping
    {
        bool  active     = false;
        float ceilingDb  = -0.3f;
    } clipping;

    struct Limiting
    {
        bool  active         = true;
        float thresholdDb    = -1.f;
        float truePeakCeilDb = -1.f;
        float releaseMs      = 50.f;
    } limiting;

    struct Output
    {
        float targetLufs = -14.f;
        float gainDb     = 0.f;
    } output;
};
