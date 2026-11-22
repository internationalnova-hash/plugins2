#include "MeterEngine.h"

#include <algorithm>

namespace tz
{

static constexpr double kPi = 3.14159265358979323846;

//==============================================================================
void Biquad::setHighPass (double fs, double f0, double q)
{
    const double w = 2.0 * kPi * f0 / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = (1.0 + c) / 2.0 / a0;
    b1 = -(1.0 + c) / a0;
    b2 = b0;
    a1 = -2.0 * c / a0;
    a2 = (1.0 - alpha) / a0;
}

void Biquad::setLowPass (double fs, double f0, double q)
{
    f0 = std::min (f0, fs * 0.45);
    const double w = 2.0 * kPi * f0 / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = (1.0 - c) / 2.0 / a0;
    b1 = (1.0 - c) / a0;
    b2 = b0;
    a1 = -2.0 * c / a0;
    a2 = (1.0 - alpha) / a0;
}

//==============================================================================
static double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

void TruePeakDetector::prepare (double sampleRate, int numChannels)
{
    // At >= 176.4 kHz the signal is already oversampled enough.
    factor = sampleRate >= 176400.0 ? 1 : kFactor;

    // Kaiser-windowed sinc interpolation filter, cutoff at the original Nyquist.
    const int len = kFactor * kTapsPerPhase;
    const double centre = (len - 1) / 2.0, beta = 8.0;
    std::array<double, kFactor * kTapsPerPhase> h {};

    for (int n = 0; n < len; ++n)
    {
        const double x = (n - centre) / (double) kFactor;
        const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (kPi * x) / (kPi * x);
        const double r = (n - centre) / centre;
        const double win = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
        h[(size_t) n] = sinc * win;
    }

    for (int p = 0; p < kFactor; ++p)
    {
        double sum = 0;
        for (int k = 0; k < kTapsPerPhase; ++k)
            sum += h[(size_t) (p + kFactor * k)];
        for (int k = 0; k < kTapsPerPhase; ++k) // unity gain per phase
            phases[(size_t) p][(size_t) k] = (float) (h[(size_t) (p + kFactor * k)] / sum);
    }

    history.assign ((size_t) numChannels, {});
    writePos.assign ((size_t) numChannels, 0);
}

void TruePeakDetector::reset()
{
    for (auto& h : history) h.fill (0.0f);
    std::fill (writePos.begin(), writePos.end(), 0);
}

float TruePeakDetector::process (float x, int ch) noexcept
{
    auto& h = history[(size_t) ch];
    auto& w = writePos[(size_t) ch];

    w = (w == 0 ? kTapsPerPhase : w) - 1;
    h[(size_t) w] = x;
    h[(size_t) (w + kTapsPerPhase)] = x;

    if (factor == 1)
        return std::abs (x);

    const float* hist = h.data() + w; // hist[k] = x[n - k]
    float peak = 0.0f;
    for (int p = 0; p < kFactor; ++p)
    {
        const auto& c = phases[(size_t) p];
        float y = 0.0f;
        for (int k = 0; k < kTapsPerPhase; ++k)
            y += c[(size_t) k] * hist[k];
        peak = std::max (peak, std::abs (y));
    }

    return peak;
}

//==============================================================================
MeterEngine::MeterEngine()
{
    histCount.assign (kHistBins, 0);
    histEnergy.assign (kHistBins, 0.0);

    for (auto& v : currentVals) v.store (kNoValue);
    for (auto& v : heldMaxVals) v.store (kNoValue);
    for (auto& v : heldMinVals) v.store (kNoValue);
}

void MeterEngine::prepare (double sr, int channels)
{
    sampleRate = sr > 0 ? sr : 48000.0;
    numCh = std::max (1, channels);
    subBlockLen = std::max (1, (int) std::lround (sampleRate * 0.1));

    // ITU-R BS.1770 K-weighting, re-derived for the actual sample rate.
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPi * f0 / sampleRate);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        kShelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
        kShelf.b1 = 2.0 * (K * K - Vh) / a0;
        kShelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        kShelf.a1 = 2.0 * (K * K - 1.0) / a0;
        kShelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPi * f0 / sampleRate);
        const double a0 = 1.0 + K / Q + K * K;
        kHighPass.b0 = 1.0;
        kHighPass.b1 = -2.0;
        kHighPass.b2 = 1.0;
        kHighPass.a1 = 2.0 * (K * K - 1.0) / a0;
        kHighPass.a2 = (1.0 - K / Q + K * K) / a0;
    }

    // Mid density band: roughly 250 Hz .. 4 kHz
    midHp.setHighPass (sampleRate, 250.0, 0.7071);
    midLp.setLowPass (sampleRate, 4000.0, 0.7071);

    for (auto* f : { &kShelf, &kHighPass, &midHp, &midLp })
        f->setNumChannels (numCh);

    truePeak.prepare (sampleRate, numCh);
    doReset();
}

void MeterEngine::doReset() noexcept
{
    accK = accRaw = accMid = 0.0;
    accTp = 0.0f;
    subBlockPos = 0;
    ringK.fill (0.0);
    ringRaw.fill (0.0);
    ringMid.fill (0.0);
    ringTp.fill (0.0f);
    ringPos = ringFilled = 0;

    std::fill (histCount.begin(), histCount.end(), 0u);
    std::fill (histEnergy.begin(), histEnergy.end(), 0.0);
    totalBlocks = 0;
    totalEnergy = 0.0;

    for (auto& v : currentVals) v.store (kNoValue);
    for (auto& v : heldMaxVals) v.store (kNoValue);
    for (auto& v : heldMinVals) v.store (kNoValue);
    peakSinceRead.store (0.0f);
    truePeakSinceRead.store (0.0f);
}

float MeterEngine::takePeakSinceLastRead (MeterId id) noexcept
{
    auto& a = (id == TruePeak) ? truePeakSinceRead : peakSinceRead;
    return gainToDb (a.exchange (0.0f));
}

static void atomicMax (std::atomic<float>& a, float v) noexcept
{
    float prev = a.load (std::memory_order_relaxed);
    while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
}

void MeterEngine::updateHeld (MeterId id, float v, float minFloor) noexcept
{
    if (! hasValue (v))
        return;

    auto& mx = heldMaxVals[(size_t) id];
    if (! hasValue (mx.load()) || v > mx.load())
        mx.store (v);

    // Lowest reading ignores silence, otherwise every pause would pin it to -inf.
    if (v > minFloor)
    {
        auto& mn = heldMinVals[(size_t) id];
        if (! hasValue (mn.load()) || v < mn.load())
            mn.store (v);
    }
}

void MeterEngine::process (const float* const* channels, int channelsIn, int numSamples) noexcept
{
    if (resetRequested.exchange (false))
        doReset();

    const int nc = std::min (channelsIn, numCh);
    if (nc <= 0)
        return;

    int pos = 0;
    while (pos < numSamples)
    {
        const int n = std::min (numSamples - pos, subBlockLen - subBlockPos);
        float blockPeak = 0.0f, blockTp = 0.0f;

        for (int ch = 0; ch < nc; ++ch)
        {
            const float* x = channels[ch] + pos;
            double sk = 0, sr = 0, sm = 0;

            for (int i = 0; i < n; ++i)
            {
                const float s = x[i];
                const double d = s;

                blockPeak = std::max (blockPeak, std::abs (s));
                blockTp = std::max (blockTp, std::max (std::abs (s), truePeak.process (s, ch)));

                const double k = kHighPass.process (kShelf.process (d, ch), ch);
                sk += k * k;
                sr += d * d;

                const double m = midLp.process (midHp.process (d, ch), ch);
                sm += m * m;
            }

            accK += sk;   // BS.1770: channel weights 1.0 for L/R/C
            accRaw += sr;
            accMid += sm;
        }

        accTp = std::max (accTp, blockTp);
        atomicMax (peakSinceRead, blockPeak);
        atomicMax (truePeakSinceRead, blockTp);
        updateHeld (Peak, gainToDb (blockPeak), -70.0f);
        updateHeld (TruePeak, gainToDb (blockTp), -70.0f);

        subBlockPos += n;
        pos += n;

        if (subBlockPos >= subBlockLen)
            finishSubBlock();
    }
}

void MeterEngine::finishSubBlock() noexcept
{
    const double len = (double) subBlockLen;
    ringK[(size_t) ringPos] = accK / len;                // sum over channels of mean square
    ringRaw[(size_t) ringPos] = accRaw / (len * numCh);  // mean square, all channels
    ringMid[(size_t) ringPos] = accMid / (len * numCh);
    ringTp[(size_t) ringPos] = accTp;
    ringPos = (ringPos + 1) % kRingLen;
    ringFilled = std::min (ringFilled + 1, kRingLen);
    accK = accRaw = accMid = 0.0;
    accTp = 0.0f;
    subBlockPos = 0;

    auto meanOfLast = [this] (const std::array<double, kRingLen>& r, int count)
    {
        count = std::min (count, ringFilled);
        double s = 0;
        for (int i = 1; i <= count; ++i)
            s += r[(size_t) ((ringPos - i + kRingLen) % kRingLen)];
        return count > 0 ? s / count : 0.0;
    };

    // RMS: 300 ms window
    const float rms = powerToDb (meanOfLast (ringRaw, 3));
    currentVals[Rms].store (rms);
    updateHeld (Rms, rms, -70.0f);

    // Mid density: 400 ms window of band-limited energy
    const float mid = powerToDb (meanOfLast (ringMid, 4));
    currentVals[MidDensity].store (mid);
    updateHeld (MidDensity, mid, -70.0f);

    // Short-term loudness: 3 s window (shown from the first 400 ms onwards)
    float st = kNoValue;
    if (ringFilled >= 4)
    {
        st = energyToLufs (meanOfLast (ringK, kRingLen));
        if (st < -70.0f) st = kNoValue;
    }
    currentVals[LufsShort].store (st);
    updateHeld (LufsShort, st, -70.0f);

    // Dynamic range (PSR): max true peak over the short-term window minus short-term loudness
    float psr = kNoValue;
    if (hasValue (st))
    {
        float tpMax = 0.0f;
        for (int i = 0; i < ringFilled; ++i)
            tpMax = std::max (tpMax, ringTp[(size_t) i]);
        const float tpDb = gainToDb (tpMax);
        if (hasValue (tpDb))
            psr = std::max (0.0f, tpDb - st);
    }
    currentVals[DynRange].store (psr);
    updateHeld (DynRange, psr, 0.0f);

    // Integrated: gated 400 ms blocks with 75 % overlap (one every 100 ms)
    if (ringFilled >= 4)
    {
        const double e = meanOfLast (ringK, 4);
        const double l = -0.691 + 10.0 * std::log10 (std::max (e, 1e-20));

        if (l > kHistMin)
        {
            const int bin = std::clamp ((int) ((l - kHistMin) / kHistStep), 0, kHistBins - 1);
            ++histCount[(size_t) bin];
            histEnergy[(size_t) bin] += e;
            ++totalBlocks;
            totalEnergy += e;
            computeIntegrated();
        }
    }
}

void MeterEngine::computeIntegrated() noexcept
{
    if (totalBlocks == 0)
        return;

    const double relGate = -0.691 + 10.0 * std::log10 (totalEnergy / (double) totalBlocks) - 10.0;
    const int firstBin = std::clamp ((int) std::ceil ((relGate - kHistMin) / kHistStep), 0, kHistBins);

    double e = 0;
    unsigned long long count = 0;
    for (int b = firstBin; b < kHistBins; ++b)
    {
        e += histEnergy[(size_t) b];
        count += histCount[(size_t) b];
    }

    const float integrated = count > 0 ? energyToLufs (e / (double) count) : kNoValue;
    currentVals[LufsInt].store (integrated);
    updateHeld (LufsInt, integrated, -70.0f);
}

} // namespace tz
