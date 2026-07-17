#include "Visualizer.h"
#include <cmath>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Colour stops (left-to-right sweep): Tone gold → EQ blue → Comp purple → Space cyan → Motion magenta
// ---------------------------------------------------------------------------
const NovaVoxVisualizer::ColourStop NovaVoxVisualizer::kStops[5] = {
    { 0.00f, juce::Colour(0xFFD4A843) },   // Tone   — gold
    { 0.25f, juce::Colour(0xFF4A90FF) },   // EQ     — blue
    { 0.50f, juce::Colour(0xFF8B5CF6) },   // Comp   — purple
    { 0.75f, juce::Colour(0xFF06B6D4) },   // Space  — cyan
    { 1.00f, juce::Colour(0xFFEC4899) },   // Motion — magenta
};

// ---------------------------------------------------------------------------
juce::Colour NovaVoxVisualizer::lerpColour(const juce::Colour& a,
                                           const juce::Colour& b,
                                           float t) noexcept
{
    return a.interpolatedWith(b, juce::jlimit(0.f, 1.f, t));
}

juce::Colour NovaVoxVisualizer::colourAtX(float xNorm) const noexcept
{
    xNorm = juce::jlimit(0.f, 1.f, xNorm);

    // Find the two surrounding stops
    for (int i = 0; i < 4; ++i)
    {
        if (xNorm <= kStops[i + 1].pos)
        {
            const float span = kStops[i + 1].pos - kStops[i].pos;
            const float t    = (span > 0.f) ? (xNorm - kStops[i].pos) / span : 0.f;
            return lerpColour(kStops[i].colour, kStops[i + 1].colour, t);
        }
    }
    return kStops[4].colour;
}

// ---------------------------------------------------------------------------
NovaVoxVisualizer::NovaVoxVisualizer(VisualizerData& data)
    : vizData(data)
{
    startTimerHz(60);
}

NovaVoxVisualizer::~NovaVoxVisualizer()
{
    stopTimer();
}

// ---------------------------------------------------------------------------
void NovaVoxVisualizer::timerCallback()
{
    phase += 0.016f;
    if (phase > juce::MathConstants<float>::twoPi * 100.f)
        phase -= juce::MathConstants<float>::twoPi * 100.f;

    const float targetLevel = vizData.level.load(std::memory_order_relaxed);
    smoothLevel += (targetLevel - smoothLevel) * 0.12f;

    const float targets[5] = {
        vizData.tone .load(std::memory_order_relaxed),
        vizData.eq   .load(std::memory_order_relaxed),
        vizData.comp .load(std::memory_order_relaxed),
        vizData.space.load(std::memory_order_relaxed),
        vizData.motion.load(std::memory_order_relaxed),
    };
    for (int i = 0; i < 5; ++i)
        smoothAmounts[i] += (targets[i] - smoothAmounts[i]) * 0.08f;

    repaint();
}

// ---------------------------------------------------------------------------
void NovaVoxVisualizer::paint(juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float W      = bounds.getWidth();
    const float H      = bounds.getHeight();
    const float cx     = W * 0.5f;
    const float cy     = H * 0.5f;

    // -----------------------------------------------------------------------
    // Background
    // -----------------------------------------------------------------------
    g.setColour(juce::Colour(0xFF080810));
    g.fillRect(bounds);

    // -----------------------------------------------------------------------
    // Distant ambient radial glow (very faint, always on)
    // -----------------------------------------------------------------------
    {
        const float glowR = juce::jmin(W, H) * 0.55f;
        juce::ColourGradient ambGlow(
            juce::Colour(0xFF8B5CF6).withAlpha(0.06f), cx, cy,
            juce::Colours::transparentBlack, cx + glowR, cy, true);
        g.setGradientFill(ambGlow);
        g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.f, glowR * 2.f);
    }

    // -----------------------------------------------------------------------
    // Flowing particle ribbons
    //
    // We draw N_WAVES overlapping sine waves.
    // Each wave has a phase offset and slight frequency/amplitude variation.
    // Along each wave we sample X_STEPS positions and draw a dot cluster.
    // X position maps to colour via colourAtX().
    // -----------------------------------------------------------------------
    static constexpr int   N_WAVES   = 6;
    static constexpr int   X_STEPS   = 220;   // dots per wave pass
    static constexpr int   SCATTER   = 3;     // scatter passes per step

    // Base amplitude: level-reactive, always at least a gentle ambient amount
    const float baseAmplitude = H * (0.06f + smoothLevel * 0.16f);

    // Per-wave parameters
    struct WaveParams {
        float phaseOffset;
        float freqMult;
        float ampScale;
        float yOffset;    // fraction of H
        float dotSize;
        float alphaScale;
    };

    const WaveParams waves[N_WAVES] = {
        { 0.00f,  1.00f, 1.00f,  0.00f,  1.8f,  0.90f },
        { 0.52f,  1.05f, 0.75f,  0.03f,  1.4f,  0.65f },
        { 1.05f,  0.95f, 0.80f, -0.03f,  1.4f,  0.60f },
        { 1.57f,  1.10f, 0.55f,  0.06f,  1.1f,  0.45f },
        { 2.09f,  0.90f, 0.60f, -0.06f,  1.1f,  0.40f },
        { 2.62f,  1.03f, 0.45f,  0.02f,  1.0f,  0.30f },
    };

    // Pseudo-random scatter offsets (static, computed once)
    // Use a simple fixed pattern so no heap allocation needed
    static const float kScatterX[SCATTER] = { -2.5f,  1.5f, 3.0f };
    static const float kScatterY[SCATTER] = {  1.5f, -2.0f, 1.0f };

    for (int wi = 0; wi < N_WAVES; ++wi)
    {
        const WaveParams& wp = waves[wi];
        const float amp = baseAmplitude * wp.ampScale;
        const float yBase = cy + H * wp.yOffset;

        for (int xi = 0; xi < X_STEPS; ++xi)
        {
            const float xNorm = (float)xi / (float)(X_STEPS - 1);
            const float xPx   = xNorm * W;

            // Sine wave Y
            const float sineArg = xNorm * juce::MathConstants<float>::twoPi
                                   * 2.2f * wp.freqMult
                                   + phase * 1.5f + wp.phaseOffset;
            const float yPx = yBase + std::sin(sineArg) * amp
                              + std::sin(sineArg * 0.5f + phase) * amp * 0.25f;

            // Colour at this X position
            const juce::Colour baseCol = colourAtX(xNorm);

            // Alpha varies along X (fade at edges) and by wave rank
            const float edgeFade  = std::sin(xNorm * juce::MathConstants<float>::pi);
            const float dotAlpha  = wp.alphaScale * edgeFade * 0.82f;
            const float dotSize   = wp.dotSize;

            // Core dot
            g.setColour(baseCol.withAlpha(dotAlpha));
            g.fillEllipse(xPx - dotSize * 0.5f,
                          yPx - dotSize * 0.5f,
                          dotSize, dotSize);

            // Scatter sparkle dots (every other step to save CPU)
            if ((xi & 1) == 0)
            {
                const float sparkAlpha = dotAlpha * 0.4f;
                g.setColour(baseCol.brighter(0.4f).withAlpha(sparkAlpha));
                for (int s = 0; s < SCATTER; ++s)
                {
                    const float sx = xPx + kScatterX[s] * 3.f;
                    const float sy = yPx + kScatterY[s] * 3.f;
                    g.fillEllipse(sx - 1.f, sy - 1.f, 2.f, 2.f);
                }
            }

            // Soft halo around core (every 4 steps)
            if ((xi & 3) == 0)
            {
                const float haloR = dotSize * 3.5f;
                juce::ColourGradient halo(
                    baseCol.withAlpha(dotAlpha * 0.15f), xPx, yPx,
                    juce::Colours::transparentBlack, xPx + haloR, yPx,
                    true);
                g.setGradientFill(halo);
                g.fillEllipse(xPx - haloR, yPx - haloR, haloR * 2.f, haloR * 2.f);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Central energy glow — level-reactive
    // -----------------------------------------------------------------------
    if (smoothLevel > 0.01f)
    {
        const float glowR = juce::jmin(W, H) * (0.08f + smoothLevel * 0.22f);
        juce::ColourGradient cGlow(
            juce::Colours::white.withAlpha(0.15f * smoothLevel), cx, cy,
            juce::Colours::transparentBlack, cx + glowR, cy, true);
        g.setGradientFill(cGlow);
        g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.f, glowR * 2.f);
    }

    // -----------------------------------------------------------------------
    // Module amount tints — coloured soft radial bursts at various positions
    // -----------------------------------------------------------------------
    const float modulePosX[5] = { W * 0.10f, W * 0.28f, W * 0.50f, W * 0.72f, W * 0.90f };
    const juce::uint32 moduleColours[5] = {
        0xFFD4A843, 0xFF4A90FF, 0xFF8B5CF6, 0xFF06B6D4, 0xFFEC4899
    };

    for (int i = 0; i < 5; ++i)
    {
        const float amt = smoothAmounts[i];
        if (amt < 0.01f) continue;

        const float mr = juce::jmin(W, H) * (0.06f + amt * 0.12f);
        const float mx = modulePosX[i];
        const float my = cy;
        juce::ColourGradient mg(
            juce::Colour(moduleColours[i]).withAlpha(amt * 0.18f), mx, my,
            juce::Colours::transparentBlack, mx + mr, my, true);
        g.setGradientFill(mg);
        g.fillEllipse(mx - mr, my - mr, mr * 2.f, mr * 2.f);
    }

    // -----------------------------------------------------------------------
    // Scanline overlay for depth / CRT feel
    // -----------------------------------------------------------------------
    g.setColour(juce::Colours::black.withAlpha(0.035f));
    for (float scanY = 0.f; scanY < H; scanY += 4.f)
        g.drawHorizontalLine((int)scanY, 0.f, W);
}
