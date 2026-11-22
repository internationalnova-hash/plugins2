#include "TwoTrackEngine.h"

namespace tt
{
void Biquad::peak (double sr, double f0, double q, float gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = juce::MathConstants<double>::twoPi * f0 / sr;
    const double alpha = std::sin (w0) / (2.0 * q), cw = std::cos (w0), a0 = 1.0 + alpha / A;
    b0 = (float) ((1.0 + alpha * A) / a0); b1 = (float) (-2.0 * cw / a0); b2 = (float) ((1.0 - alpha * A) / a0);
    a1 = (float) (-2.0 * cw / a0);         a2 = (float) ((1.0 - alpha / A) / a0);
}

void Biquad::lowShelf (double sr, double f0, float gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = juce::MathConstants<double>::twoPi * f0 / sr;
    const double cw = std::cos (w0), alpha = std::sin (w0) / 2.0 * std::sqrt (2.0); // S = 1
    const double sA = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1) + (A - 1) * cw + sA;
    b0 = (float) (A * ((A + 1) - (A - 1) * cw + sA) / a0);
    b1 = (float) (2 * A * ((A - 1) - (A + 1) * cw) / a0);
    b2 = (float) (A * ((A + 1) - (A - 1) * cw - sA) / a0);
    a1 = (float) (-2 * ((A - 1) + (A + 1) * cw) / a0);
    a2 = (float) (((A + 1) + (A - 1) * cw - sA) / a0);
}

void TransientBand::prepare (double sr) noexcept
{
    auto coef = [sr] (double ms) { return (float) (1.0 - std::exp (-1.0 / (sr * ms * 0.001))); };
    aFastAtk = coef (0.5);  aFastRel = coef (25.0);
    aSlowAtk = coef (25.0); aSlowRel = coef (180.0);
    aGain    = coef (2.0);
    reset();
}

void TwoTrackEngine::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (32, maxBlockSize);

    juce::dsp::ProcessSpec spec { fs, (juce::uint32) maxBlock, 2 };
    sideSplit.prepare (spec);  sideSplit.setCutoffFrequency ((float) kLowSplitHz);
    midAllpass.prepare (spec); midAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass); midAllpass.setCutoffFrequency ((float) kLowSplitHz);
    punchLow.prepare (spec); punchLow.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); punchLow.setCutoffFrequency ((float) kPunchHz);
    energyBand.prepare (spec);
    energyBand.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    energyBand.setCutoffFrequency ((float) kEnergyHz);
    energyBand.setResonance (0.9f);
    punchEnv.prepare (fs);
    energyEnv.prepare (fs);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    latency = (int) std::lround (oversampler->getLatencyInSamples());
    work.setSize (2, maxBlock);

    for (auto* s : { &inGain, &outGain, &widthGain, &lowFold })
        s->reset (fs, 0.03);
    ceilingGain.reset (fs * 4.0, 0.03);
    firstUpdate = true;
    updateFilters();
    reset();
}

void TwoTrackEngine::reset()
{
    lowShelfM.reset(); carveM.reset(); instrS.reset();
    sideSplit.reset(); midAllpass.reset(); punchLow.reset(); energyBand.reset();
    punchEnv.reset(); energyEnv.reset();
    if (oversampler != nullptr) oversampler->reset();
    lastOverDb = 0.0f;
}

void TwoTrackEngine::updateFilters() noexcept
{
    const auto& t = target;
    auto changed = [] (float a, float b) { return std::abs (a - b) > 1.0e-4f; };

    if (firstUpdate || changed (t.lowEnd, applied.lowEnd))
        lowShelfM.lowShelf (fs, kLowShelfHz, kLowShelfMaxDb * t.lowEnd * 0.01f);
    if (firstUpdate || changed (t.voxCarve, applied.voxCarve))
        carveM.peak (fs, kCarveHz, kCarveQ, kCarveMaxDb * t.voxCarve * 0.01f);
    if (firstUpdate || changed (t.instruments, applied.instruments))
        instrS.peak (fs, kInstrHz, kInstrQ, kInstrMaxDb * t.instruments * 0.01f);

    const auto db = [] (float v) { return juce::Decibels::decibelsToGain (v); };
    auto setT = [this] (juce::LinearSmoothedValue<float>& s, float v)
    {
        if (firstUpdate) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
    };
    setT (inGain, db (t.inputDb));
    setT (outGain, db (t.outputDb));
    setT (widthGain, juce::jlimit (0.0f, 2.0f, t.width * 0.01f));
    setT (lowFold, 1.0f - juce::jlimit (0.0f, 1.0f, t.lowEnd * 0.01f));
    setT (ceilingGain, db (juce::jlimit (-12.0f, 0.0f, t.ceilingDb)));

    applied = t;
    firstUpdate = false;
}

void TwoTrackEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (oversampler == nullptr || numSamples <= 0)
        return;
    updateFilters();
    lastOverDb = lastPunchDb = lastEnergyDb = 0.0f;
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);
        processChunk (left + start, right + start, n);
    }
}

void TwoTrackEngine::processChunk (float* l, float* r, int n) noexcept
{
    const float punchAmt  = juce::jlimit (0.0f, 1.0f, applied.punch * 0.01f);
    const float energyAmt = juce::jlimit (0.0f, 1.0f, applied.energy * 0.01f);
    float maxPg = 0.0f, maxEg = 0.0f;
    float* wl = work.getWritePointer (0);
    float* wr = work.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        const float xl = (std::isfinite (l[i]) ? l[i] : 0.0f) * g;
        const float xr = (std::isfinite (r[i]) ? r[i] : 0.0f) * g;

        // ── Mid / side shaping ──
        float m = 0.5f * (xl + xr);
        float s = 0.5f * (xl - xr);

        m = midAllpass.processSample (0, m);    // phase-match the side crossover
        m = lowShelfM.process (0, m);           // Low End: centre weight
        m = carveM.process (0, m);              // Vox Carve: pocket in the centre

        float sLow = 0.0f, sHigh = 0.0f;
        sideSplit.processSample (0, s, sLow, sHigh);
        s = sLow * lowFold.getNextValue()       // Low End: fold side lows to mono
          + sHigh * widthGain.getNextValue();   // Width: side level above the lows
        s = instrS.process (0, s);              // Instruments: side body

        float yl = m + s, yr = m - s;

        // ── Transients ── (linked detector so the image never shifts)
        const float bl = punchLow.processSample (0, yl), br = punchLow.processSample (1, yr);
        const float pg = punchEnv.next (0.5f * (bl + br), punchAmt) - 1.0f;
        yl += bl * pg; yr += br * pg;
        maxPg = juce::jmax (maxPg, pg);

        const float el = energyBand.processSample (0, yl), er = energyBand.processSample (1, yr);
        const float eg = energyEnv.next (0.5f * (el + er), energyAmt) - 1.0f;
        yl += el * eg; yr += er * eg;
        maxEg = juce::jmax (maxEg, eg);

        wl[i] = yl; wr[i] = yr;
    }

    lastPunchDb  = juce::jmax (lastPunchDb,  juce::Decibels::gainToDecibels (1.0f + maxPg));
    lastEnergyDb = juce::jmax (lastEnergyDb, juce::Decibels::gainToDecibels (1.0f + maxEg));

    // ── Soft clipper at 4x (always oversampled so latency never changes) ──
    float* chans[] { wl, wr };
    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    auto up = oversampler->processSamplesUp (block);
    const bool clip = applied.clipOn;
    float maxRatio = 0.0f;
    float cHold = ceilingGain.getCurrentValue();
    for (size_t i = 0; i < up.getNumSamples(); ++i)
    {
        const float c = ceilingGain.getNextValue();
        cHold = juce::jmax (cHold, c);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float* p = up.getChannelPointer (ch) + i;
            if (clip)
            {
                maxRatio = juce::jmax (maxRatio, std::abs (*p) / c);
                *p = softClip (*p, c);
            }
        }
    }
    oversampler->processSamplesDown (block);
    if (maxRatio > 1.0f)
        lastOverDb = juce::jmax (lastOverDb, juce::Decibels::gainToDecibels (maxRatio));

    // ── Output + NaN-safe final limit ──
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        float a = wl[i], b = wr[i];
        if (clip) { a = juce::jlimit (-cHold, cHold, a); b = juce::jlimit (-cHold, cHold, b); }
        a *= g; b *= g;
        l[i] = std::isfinite (a) ? juce::jlimit (-1.0f, 1.0f, a) : 0.0f;
        r[i] = std::isfinite (b) ? juce::jlimit (-1.0f, 1.0f, b) : 0.0f;
    }
}
} // namespace tt
