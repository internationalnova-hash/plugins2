#pragma once

struct NovaConsoleParameters
{
    // ── Global ────────────────────────────────────────────────────────────────
    int  mode         = 1;       // 0=Clean 1=British 2=TubeTape 3=Gold 4=Modern
    int  quality      = 1;       // 0=Eco 1=Mix 2=Master
    int  oversampling = 1;       // 0=Off 1=2x 2=4x

    // ── Input / Output ────────────────────────────────────────────────────────
    float inputDb     = 0.0f;    // -18..+18 dB
    float outputDb    = 0.0f;    // -18..+18 dB

    // ── Preamp ────────────────────────────────────────────────────────────────
    bool  preampOn    = true;
    float drive       = 38.0f;   // 0..100
    float color       = 50.0f;   // 0..100
    float trimDb      = 0.0f;    // -12..+12 dB

    // ── Filters ───────────────────────────────────────────────────────────────
    bool  filterOn    = true;
    float hpfHz       = 20.0f;     // 20..1200, skew 0.35
    float lpfHz       = 20000.0f;  // 1800..20000, skew 0.35
    int   hpfSlope    = 1;         // 0=12 dB/oct  1=24 dB/oct  2=48 dB/oct
    int   lpfSlope    = 1;

    // ── EQ ────────────────────────────────────────────────────────────────────
    bool  eqOn            = true;
    float lowDb           = 0.0f;
    float lowFreqHz       = 90.0f;     // 35..180, skew 0.33
    float lowQ            = 0.62f;     // 0.40..1.40, skew 0.55
    int   lowMode         = 0;         // 0=Shelf  1=Bell
    float lowMidDb        = 0.0f;
    float lowMidFreqHz    = 420.0f;    // 180..1200, skew 0.38
    float lowMidQ         = 0.80f;     // 0.35..2.50, skew 0.45
    float highMidDb       = 0.0f;
    float highMidFreqHz   = 2800.0f;   // 1000..7000, skew 0.40
    float highMidQ        = 0.85f;     // 0.35..2.50, skew 0.45
    float highDb          = 0.0f;
    float highFreqHz      = 7600.0f;   // 3500..12000, skew 0.40
    float highQ           = 0.70f;     // 0.40..1.40, skew 0.55
    int   highMode        = 0;
    float airDb           = 0.0f;
    float airFreqHz       = 14500.0f;  // 8000..18000, skew 0.42
    float airQ            = 0.75f;     // 0.40..1.40, skew 0.55
    int   airMode         = 0;

    // ── Compressor ────────────────────────────────────────────────────────────
    bool  compOn          = true;
    float compThreshDb    = -16.0f;    // -40..0
    float compRatio       = 4.0f;      // 1..10
    float compAttackMs    = 15.0f;     // 0.5..60, skew 0.45
    float compReleaseMs   = 180.0f;    // 20..500, skew 0.45
    float compMix         = 100.0f;    // 0..100 %
    float compMakeupDb    = 0.0f;      // 0..24 dB
    float compPunch       = 35.0f;     // 0..100 %

    // ── Gate ──────────────────────────────────────────────────────────────────
    bool  gateOn          = false;
    float gateThreshDb    = -42.0f;    // -70..-10
    float gateAttackMs    = 8.0f;      // 0.1..100, skew 0.40
    float gateHoldMs      = 40.0f;     // 0..500
    float gateReleaseMs   = 120.0f;    // 20..500, skew 0.45
    float gateRangeDb     = -18.0f;    // -36..-3
    bool  gateSmooth      = true;      // true=expand  false=hard gate

    // ── Analog Engine ─────────────────────────────────────────────────────────
    bool  analogOn        = true;
    float heat            = 28.0f;     // 0..100
    float depth           = 24.0f;     // 0..100
    float width           = 52.0f;     // 0..100
    float drift           = 12.0f;     // 0..100
    float crosstalk       = 0.0f;      // 0..100
    float noise           = 0.0f;      // 0..100

    // ── Intelligence ─────────────────────────────────────────────────────────
    bool  smartGain       = false;
    bool  focusMode       = false;
    float mixAssist       = 50.0f;     // 0..100 %
    int   sidechainMode   = 0;         // 0=Off  1=Internal  2=External
};
