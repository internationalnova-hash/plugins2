#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <array>
#include <limits>

namespace
{
    template <typename T>
    T clampValue (T value, T minValue, T maxValue)
    {
        return juce::jlimit (minValue, maxValue, value);
    }

    float getObjectNumber (const juce::var& obj, const juce::Identifier& key, float fallback)
    {
        if (auto* dyn = obj.getDynamicObject())
        {
            const auto value = dyn->getProperty (key);
            if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
                return static_cast<float> (double (value));
        }

        return fallback;
    }

    float getBandFrequencyForIndex (int index)
    {
        const auto t = static_cast<float> (index) / static_cast<float> (NovaCurveAudioProcessor::maxBands - 1);
        return 20.0f * std::pow (1000.0f, t);
    }

    float strengthenGainDb (float gainDb)
    {
        const auto absGain = std::abs (gainDb);
        const auto normalized = juce::jlimit (0.0f, 1.0f, absGain / 30.0f);
        const auto emphasis = 1.10f + 0.52f * std::pow (normalized, 1.1f);
        return juce::jlimit (-36.0f, 36.0f, gainDb * emphasis);
    }

    float sharpenQ (float q)
    {
        const auto safeQ = clampValue (q, 0.10f, 10.0f);
        return juce::jlimit (0.20f, 36.0f, 0.35f + 0.55f * std::pow (safeQ, 1.8f));
    }

    float shelfQFromUser (float q)
    {
        const auto safeQ = clampValue (q, 0.10f, 10.0f);
        return juce::jlimit (0.45f, 1.35f, 0.52f + 0.08f * safeQ);
    }
}

NovaCurveAudioProcessor::NovaCurveAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    initialiseDefaultState();

    for (auto& bucket : preSpectrum)
        bucket.store (0.0f);

    for (auto& bucket : postSpectrum)
        bucket.store (0.0f);

    for (auto& bucket : reductionSpectrum)
        bucket.store (0.0f);
}

NovaCurveAudioProcessor::~NovaCurveAudioProcessor() = default;

void NovaCurveAudioProcessor::initialiseDefaultState()
{
    for (int i = 0; i < maxBands; ++i)
    {
        bands[static_cast<size_t> (i)].enabled.store (i < 6 ? 1.0f : 0.0f);
        bands[static_cast<size_t> (i)].type.store (i == 0 ? 3.0f : (i == 5 ? 2.0f : 0.0f));
        bands[static_cast<size_t> (i)].mode.store ((i == 3 || i == 4) ? 1.0f : 0.0f);
        bands[static_cast<size_t> (i)].channel.store (0.0f);
        bands[static_cast<size_t> (i)].frequency.store (getBandFrequencyForIndex (i));
        bands[static_cast<size_t> (i)].gainDb.store (0.0f);
        bands[static_cast<size_t> (i)].q.store (1.2f);
        bands[static_cast<size_t> (i)].slope.store (24.0f);
        bands[static_cast<size_t> (i)].dynRangeDb.store (-6.0f);
        bands[static_cast<size_t> (i)].thresholdMode.store (1.0f);
        bands[static_cast<size_t> (i)].thresholdDb.store (-22.0f);
        bands[static_cast<size_t> (i)].attackMs.store (10.0f);
        bands[static_cast<size_t> (i)].releaseMs.store (120.0f);
        bands[static_cast<size_t> (i)].ratio.store (2.2f);
        bands[static_cast<size_t> (i)].solo.store (0.0f);
    }

    // Quick-start bands inspired by the visual mockup layout.
    bands[0].frequency.store (34.0f);
    bands[0].type.store (3.0f);
    bands[1].frequency.store (110.0f);
    bands[1].gainDb.store (3.2f);
    bands[2].frequency.store (360.0f);
    bands[2].gainDb.store (-10.8f);
    bands[3].frequency.store (2730.0f);
    bands[3].gainDb.store (4.8f);
    bands[4].frequency.store (5000.0f);
    bands[4].gainDb.store (0.0f);
    bands[5].frequency.store (13000.0f);
    bands[5].type.store (2.0f);
    bands[5].gainDb.store (5.6f);
}

const juce::String NovaCurveAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaCurveAudioProcessor::acceptsMidi() const  { return false; }
bool NovaCurveAudioProcessor::producesMidi() const { return false; }
bool NovaCurveAudioProcessor::isMidiEffect() const { return false; }

double NovaCurveAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaCurveAudioProcessor::getNumPrograms() { return 1; }
int NovaCurveAudioProcessor::getCurrentProgram() { return 0; }
void NovaCurveAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String NovaCurveAudioProcessor::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void NovaCurveAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

void NovaCurveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec {
        currentSampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        1
    };

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int band = 0; band < maxBands; ++band)
        {
            eqFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
            eqFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
            eqFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass (currentSampleRate, 1000.0f);

                eqFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
                eqFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
                eqFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass (currentSampleRate, 1000.0f);

                eqFiltersStage3[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
                eqFiltersStage3[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
                eqFiltersStage3[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass (currentSampleRate, 1000.0f);

                eqFiltersStage4[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
                eqFiltersStage4[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
                eqFiltersStage4[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass (currentSampleRate, 1000.0f);

            detectorFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
            detectorFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
            detectorFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, getBandFrequencyForIndex (band), 1.0f);

            auditionFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
            auditionFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
            auditionFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, getBandFrequencyForIndex (band), 1.2f);

            auditionFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].prepare (spec);
            auditionFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].reset();
            auditionFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, getBandFrequencyForIndex (band), 1.2f);
        }
    }

    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock, false, false, true);
    soloSourceBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock, false, false, true);
    dryBuffer.clear();
    soloSourceBuffer.clear();

    std::fill (detectorEnvelopes.begin(), detectorEnvelopes.end(), 0.0f);
    std::fill (bandDynamicGainDb.begin(), bandDynamicGainDb.end(), 0.0f);
    std::fill (lastDynamicGainDb.begin(), lastDynamicGainDb.end(), 0.0f);
    std::fill (smoothedAppliedGainDb.begin(), smoothedAppliedGainDb.end(), 0.0f);

    transientDecay = 0.0f;

    fftFifoIndex = 0;
    fftReady = false;
    std::fill (preFifo.begin(), preFifo.end(), 0.0f);
    std::fill (postFifo.begin(), postFifo.end(), 0.0f);
    std::fill (preFftData.begin(), preFftData.end(), 0.0f);
    std::fill (postFftData.begin(), postFftData.end(), 0.0f);
}

void NovaCurveAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
    soloSourceBuffer.setSize (0, 0);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaCurveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

float NovaCurveAudioProcessor::mapMagnitudeToNormal (float magnitude) noexcept
{
    const auto db = juce::Decibels::gainToDecibels (magnitude + 1.0e-8f);
    return juce::jlimit (0.0f, 1.0f, (db + 84.0f) / 84.0f);
}

juce::dsp::IIR::Coefficients<float>::Ptr NovaCurveAudioProcessor::createBandCoefficients (
    int type,
    double sampleRate,
    float frequency,
    float q,
    float gainDb,
    float slopeDbPerOct)
{
    const auto hz = clampValue (frequency, 20.0f, 20000.0f);
    const auto safeQ = clampValue (q, 0.10f, 10.0f);
    const auto strongGainDb = strengthenGainDb (gainDb);
    const auto gain = juce::Decibels::decibelsToGain (strongGainDb);
    const auto bellQ = sharpenQ (safeQ);
    const auto shelfQ = shelfQFromUser (safeQ);

    // Phase behavior: natural phase for shelves/passes, minimal for peaking
    switch (type)
    {
        case 1: return juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, hz, shelfQ, gain);
        case 2: return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, hz, shelfQ, gain);
        case 3: return juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, hz, juce::jlimit (0.50f, 2.6f, 0.707f + (safeQ - 1.0f) * 0.18f));
        case 4: return juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, hz, juce::jlimit (0.50f, 2.6f, 0.707f + (safeQ - 1.0f) * 0.18f));
        case 5: return juce::dsp::IIR::Coefficients<float>::makeNotch (sampleRate, hz, bellQ);
        case 6: return juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, hz, bellQ);
        case 7:
        {
            // Tilt: smooth high-shelf with natural phase behavior
            const auto tiltGain = juce::Decibels::decibelsToGain (strongGainDb * 0.78f);
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, hz, 0.62f, tiltGain);
        }
        case 0:
        default: return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, hz, bellQ, gain);
    }
}

void NovaCurveAudioProcessor::pushSampleToAnalyzer (float preSample, float postSample) noexcept
{
    if (fftFifoIndex >= fftSize)
        return;

    preFifo[static_cast<size_t> (fftFifoIndex)] = preSample;
    postFifo[static_cast<size_t> (fftFifoIndex)] = postSample;
    ++fftFifoIndex;

    if (fftFifoIndex == fftSize)
    {
        std::fill (preFftData.begin(), preFftData.end(), 0.0f);
        std::fill (postFftData.begin(), postFftData.end(), 0.0f);

        std::copy (preFifo.begin(), preFifo.end(), preFftData.begin());
        std::copy (postFifo.begin(), postFifo.end(), postFftData.begin());

        fftReady = true;
        fftFifoIndex = 0;
    }
}

void NovaCurveAudioProcessor::refreshAnalyzerData()
{
    if (! fftReady)
        return;

    fftReady = false;

    window.multiplyWithWindowingTable (preFftData.data(), fftSize);
    window.multiplyWithWindowingTable (postFftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (preFftData.data());
    fft.performFrequencyOnlyForwardTransform (postFftData.data());

    for (int bucket = 0; bucket < spectrumBins; ++bucket)
    {
        const auto startNorm = static_cast<float> (bucket) / static_cast<float> (spectrumBins);
        const auto endNorm = static_cast<float> (bucket + 1) / static_cast<float> (spectrumBins);

        auto startBin = juce::jlimit (1, (fftSize / 2) - 2,
                                      static_cast<int> (std::pow (startNorm, 1.85f) * ((fftSize / 2) - 2)) + 1);
        auto endBin = juce::jlimit (startBin + 1, (fftSize / 2) - 1,
                                    static_cast<int> (std::pow (endNorm, 1.85f) * ((fftSize / 2) - 1)) + 1);

        float prePeak = 0.0f;
        float postPeak = 0.0f;

        for (int bin = startBin; bin <= endBin; ++bin)
        {
            prePeak = juce::jmax (prePeak, preFftData[static_cast<size_t> (bin)]);
            postPeak = juce::jmax (postPeak, postFftData[static_cast<size_t> (bin)]);
        }

        preSpectrum[static_cast<size_t> (bucket)].store (mapMagnitudeToNormal (prePeak));
        postSpectrum[static_cast<size_t> (bucket)].store (mapMagnitudeToNormal (postPeak));
    }

    for (int bucket = 0; bucket < spectrumBins; ++bucket)
    {
        const auto t = static_cast<float> (bucket) / static_cast<float> (spectrumBins - 1);
        const auto hz = 20.0f * std::pow (1000.0f, t);

        float nearestReduction = 0.0f;
        float bestDistance = std::numeric_limits<float>::max();

        for (int band = 0; band < maxBands; ++band)
        {
            const auto freq = bands[static_cast<size_t> (band)].frequency.load();
            const auto distance = std::abs (std::log2 (juce::jmax (20.0f, hz) / juce::jmax (20.0f, freq)));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                nearestReduction = juce::jlimit (-30.0f, 30.0f, bandDynamicGainDb[static_cast<size_t> (band)]);
            }
        }

        reductionSpectrum[static_cast<size_t> (bucket)].store (juce::jlimit (0.0f, 1.0f, -nearestReduction / 30.0f));
    }
}

void NovaCurveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    dryBuffer.makeCopyOf (buffer, true);

    const auto bypass = bypassed.load() > 0.5f;
    const auto outputLinear = juce::Decibels::decibelsToGain (outputGainDb.load());
    const auto resonanceNorm = juce::jlimit (0.0f, 1.0f, resonanceAmount.load() / 100.0f);
    const auto harmonicLinkEnabled = harmonicLink.load() > 0.5f;

    // Detector pre-pass setup (per-block): avoid expensive coefficient/exp recompute per sample.
    std::array<bool, maxBands> detectorBandEnabled {};
    std::array<float, maxBands> detectorAttackCoeff {};
    std::array<float, maxBands> detectorReleaseCoeff {};

    for (int band = 0; band < maxBands; ++band)
    {
        const auto enabled = bands[static_cast<size_t> (band)].enabled.load() > 0.5f;
        detectorBandEnabled[static_cast<size_t> (band)] = enabled;
        if (! enabled)
            continue;

        const auto freq = clampValue (bands[static_cast<size_t> (band)].frequency.load(), 20.0f, 20000.0f);
        const auto q = clampValue (bands[static_cast<size_t> (band)].q.load(), 0.10f, 10.0f);
        detectorFilters[0][static_cast<size_t> (band)].coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, freq, q);

        const auto attackMs = clampValue (bands[static_cast<size_t> (band)].attackMs.load(), 0.1f, 200.0f);
        const auto releaseMs = clampValue (bands[static_cast<size_t> (band)].releaseMs.load(), 10.0f, 1000.0f);
        detectorAttackCoeff[static_cast<size_t> (band)] = std::exp (-1.0f / (0.001f * attackMs * static_cast<float> (currentSampleRate)));
        detectorReleaseCoeff[static_cast<size_t> (band)] = std::exp (-1.0f / (0.001f * releaseMs * static_cast<float> (currentSampleRate)));
    }

    // Detector pass for dynamic EQ.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto l = dryBuffer.getSample (0, sample);
        const auto r = dryBuffer.getNumChannels() > 1 ? dryBuffer.getSample (1, sample) : l;
        const auto mono = 0.5f * (l + r);

        for (int band = 0; band < maxBands; ++band)
        {
            if (! detectorBandEnabled[static_cast<size_t> (band)])
                continue;

            const auto detection = std::abs (detectorFilters[0][static_cast<size_t> (band)].processSample (mono));
            const auto attackCoeff = detectorAttackCoeff[static_cast<size_t> (band)];
            const auto releaseCoeff = detectorReleaseCoeff[static_cast<size_t> (band)];

            if (detection > detectorEnvelopes[static_cast<size_t> (band)])
                detectorEnvelopes[static_cast<size_t> (band)] = attackCoeff * detectorEnvelopes[static_cast<size_t> (band)]
                    + (1.0f - attackCoeff) * detection;
            else
                detectorEnvelopes[static_cast<size_t> (band)] = releaseCoeff * detectorEnvelopes[static_cast<size_t> (band)]
                    + (1.0f - releaseCoeff) * detection;
        }
    }

    float dynamicSum = 0.0f;

    for (int band = 0; band < maxBands; ++band)
    {
        auto dynamicGainDb = 0.0f;
        auto resonanceGainDb = 0.0f;

        if (bands[static_cast<size_t> (band)].enabled.load() > 0.5f)
        {
            const auto mode = static_cast<int> (std::round (bands[static_cast<size_t> (band)].mode.load()));
            const auto envDb = juce::Decibels::gainToDecibels (detectorEnvelopes[static_cast<size_t> (band)] + 1.0e-8f);

            // Dynamic EQ: active only in Dynamic mode.
            if (mode == 1)
            {
                const auto thresholdModeValue = static_cast<int> (std::round (bands[static_cast<size_t> (band)].thresholdMode.load()));
                const auto thresholdDb = thresholdModeValue == 0
                    ? juce::jlimit (-36.0f, -10.0f, envDb - 3.5f)
                    : clampValue (bands[static_cast<size_t> (band)].thresholdDb.load(), -60.0f, 0.0f);
                const auto ratio = clampValue (bands[static_cast<size_t> (band)].ratio.load(), 1.0f, 10.0f);
                const auto range = clampValue (bands[static_cast<size_t> (band)].dynRangeDb.load(), -30.0f, 30.0f);

                if (range < 0.0f)
                {
                    const auto over = juce::jmax (0.0f, envDb - thresholdDb);
                    const auto reduction = juce::jmin (std::abs (range), over * (1.0f - (1.0f / ratio)));
                    dynamicGainDb = -reduction;
                }
                else if (range > 0.0f)
                {
                    const auto under = juce::jmax (0.0f, thresholdDb - envDb);
                    const auto boost = juce::jmin (range, under * ((ratio - 1.0f) / ratio));
                    dynamicGainDb = boost;
                }
            }

            // Resonance Assist: independent helper layer for all modes.
            if (resonanceNorm > 0.001f)
            {
                const auto q = clampValue (bands[static_cast<size_t> (band)].q.load(), 0.10f, 10.0f);
                const auto qNorm = juce::jlimit (0.0f, 1.0f, (q - 0.10f) / 9.90f);
                const auto focusBoost = 0.70f + qNorm * 0.95f;
                const auto resonanceThresholdDb = -28.0f + qNorm * 7.0f;
                const auto overRes = juce::jmax (0.0f, envDb - resonanceThresholdDb);

                // Dedicated resonance mode is intentionally stronger, but still lighter than a full suppressor.
                const auto modeBoost = (mode == 2) ? 1.30f : 1.0f;
                const auto maxResCut = (3.0f + 3.0f * resonanceNorm) * modeBoost;
                const auto resCut = juce::jmin (maxResCut, overRes * (0.22f + 0.34f * resonanceNorm) * focusBoost);
                resonanceGainDb = -juce::jmax (0.0f, resCut);
            }

            auto totalMovementDb = dynamicGainDb + resonanceGainDb;

            // Smooth movement to avoid pumping/chatter and mode-switch clicks.
            const auto attackMs = clampValue (bands[static_cast<size_t> (band)].attackMs.load(), 0.1f, 200.0f);
            const auto releaseMs = clampValue (bands[static_cast<size_t> (band)].releaseMs.load(), 10.0f, 1000.0f);
            const auto attackCoeff = std::exp (-1.0f / (0.001f * attackMs * static_cast<float> (currentSampleRate)));
            const auto releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * static_cast<float> (currentSampleRate)));

            const auto lastGain = lastDynamicGainDb[static_cast<size_t> (band)];
            if (std::abs (totalMovementDb) > std::abs (lastGain))
                totalMovementDb = attackCoeff * lastGain + (1.0f - attackCoeff) * totalMovementDb;
            else
                totalMovementDb = releaseCoeff * lastGain + (1.0f - releaseCoeff) * totalMovementDb;

            lastDynamicGainDb[static_cast<size_t> (band)] = totalMovementDb;
            dynamicGainDb = totalMovementDb;
        }

        bandDynamicGainDb[static_cast<size_t> (band)] = dynamicGainDb;
        dynamicSum += std::abs (dynamicGainDb);
    }

    if (harmonicLinkEnabled)
    {
        std::array<float, maxBands> linkedDynamic {};
        for (int i = 0; i < maxBands; ++i)
            linkedDynamic[static_cast<size_t> (i)] = bandDynamicGainDb[static_cast<size_t> (i)];

        const auto dynamicLinkBlend = 0.08f + 0.06f * resonanceNorm;

        for (int band = 0; band < maxBands; ++band)
        {
            if (bands[static_cast<size_t> (band)].enabled.load() < 0.5f)
                continue;

            const auto centerFreq = clampValue (bands[static_cast<size_t> (band)].frequency.load(), 20.0f, 20000.0f);
            float sum = 0.0f;
            float weightSum = 0.0f;

            for (int n : { band - 1, band + 1 })
            {
                if (n < 0 || n >= maxBands)
                    continue;
                if (bands[static_cast<size_t> (n)].enabled.load() < 0.5f)
                    continue;

                const auto nFreq = clampValue (bands[static_cast<size_t> (n)].frequency.load(), 20.0f, 20000.0f);
                const auto octDist = std::abs (std::log2 (centerFreq / nFreq));
                const auto weight = std::exp (-1.7f * octDist);
                sum += bandDynamicGainDb[static_cast<size_t> (n)] * weight;
                weightSum += weight;
            }

            if (weightSum > 1.0e-5f)
            {
                const auto neighbourAvg = sum / weightSum;
                linkedDynamic[static_cast<size_t> (band)]
                    = linkedDynamic[static_cast<size_t> (band)] * (1.0f - dynamicLinkBlend)
                    + neighbourAvg * dynamicLinkBlend;
            }
        }

        for (int i = 0; i < maxBands; ++i)
            bandDynamicGainDb[static_cast<size_t> (i)] = linkedDynamic[static_cast<size_t> (i)];
    }

    dynamicActivity.store (juce::jlimit (0.0f, 1.0f, dynamicSum / static_cast<float> (maxBands * 8.0f)));

    std::array<int, maxBands> soloBands {};
    int soloCount = 0;
    for (int band = 0; band < maxBands; ++band)
    {
        if (bands[static_cast<size_t> (band)].enabled.load() > 0.5f
            && bands[static_cast<size_t> (band)].solo.load() > 0.5f)
        {
            soloBands[static_cast<size_t> (soloCount++)] = band;
        }
    }

    if (! bypass)
    {
        const auto gainLinkBlend = harmonicLinkEnabled ? (0.05f + 0.04f * resonanceNorm) : 0.0f;
        const auto qLinkBlend = harmonicLinkEnabled ? 0.055f : 0.0f;
        const auto freqLinkBlend = harmonicLinkEnabled ? 0.025f : 0.0f;

        for (int band = 0; band < maxBands; ++band)
        {
            if (bands[static_cast<size_t> (band)].enabled.load() < 0.5f)
                continue;

            const auto type = static_cast<int> (std::round (bands[static_cast<size_t> (band)].type.load()));
            const auto mode = static_cast<int> (std::round (bands[static_cast<size_t> (band)].mode.load()));
            auto freq = clampValue (bands[static_cast<size_t> (band)].frequency.load(), 20.0f, 20000.0f);
            auto q = clampValue (bands[static_cast<size_t> (band)].q.load(), 0.10f, 10.0f);
            auto targetGain = clampValue (bands[static_cast<size_t> (band)].gainDb.load() + bandDynamicGainDb[static_cast<size_t> (band)], -30.0f, 30.0f);

            // In static mode with no dynamic movement, apply gain directly so manual EQ edits are immediate.
            const auto hasDynamicMovement = std::abs (bandDynamicGainDb[static_cast<size_t> (band)]) > 0.001f;
            if (mode == 0 && ! hasDynamicMovement)
                smoothedAppliedGainDb[static_cast<size_t> (band)] = targetGain;
            else
                smoothedAppliedGainDb[static_cast<size_t> (band)] = smoothedAppliedGainDb[static_cast<size_t> (band)] * 0.40f
                    + targetGain * 0.60f;
            auto gain = clampValue (smoothedAppliedGainDb[static_cast<size_t> (band)], -30.0f, 30.0f);
            const auto slope = bands[static_cast<size_t> (band)].slope.load();
            const auto channelMode = static_cast<int> (std::round (bands[static_cast<size_t> (band)].channel.load()));

            auto stageCount = 1;
            if (type == 3 || type == 4)
            {
                const auto slopeClamped = juce::jlimit (6.0f, 96.0f, slope);
                stageCount = juce::jlimit (1, 4, static_cast<int> (std::round (slopeClamped / 12.0f)));
            }
            else if (type == 5 || type == 6)
            {
                stageCount = q >= 2.5f ? 2 : 1;
            }
            else if (type == 0)
            {
                stageCount = q >= 5.0f ? 2 : 1;
            }

            if (harmonicLinkEnabled)
            {
                float gainSum = 0.0f;
                float qSum = 0.0f;
                float logFreqSum = 0.0f;
                float weightSum = 0.0f;

                for (int n : { band - 1, band + 1 })
                {
                    if (n < 0 || n >= maxBands)
                        continue;
                    if (bands[static_cast<size_t> (n)].enabled.load() < 0.5f)
                        continue;

                    const auto nFreq = clampValue (bands[static_cast<size_t> (n)].frequency.load(), 20.0f, 20000.0f);
                    const auto nQ = clampValue (bands[static_cast<size_t> (n)].q.load(), 0.10f, 10.0f);
                    const auto nGain = clampValue (bands[static_cast<size_t> (n)].gainDb.load() + bandDynamicGainDb[static_cast<size_t> (n)], -30.0f, 30.0f);
                    const auto octDist = std::abs (std::log2 (freq / nFreq));
                    const auto weight = std::exp (-1.6f * octDist);

                    gainSum += nGain * weight;
                    qSum += nQ * weight;
                    logFreqSum += std::log2 (nFreq) * weight;
                    weightSum += weight;
                }

                if (weightSum > 1.0e-5f)
                {
                    const auto neighbourGain = gainSum / weightSum;
                    const auto neighbourQ = qSum / weightSum;
                    const auto neighbourLogFreq = logFreqSum / weightSum;

                    gain = clampValue (gain + (neighbourGain - gain) * gainLinkBlend, -30.0f, 30.0f);
                    q = clampValue (q + (neighbourQ - q) * qLinkBlend, 0.10f, 10.0f);
                    const auto linkedLogFreq = std::log2 (freq) + (neighbourLogFreq - std::log2 (freq)) * freqLinkBlend;
                    freq = clampValue (std::pow (2.0f, linkedLogFreq), 20.0f, 20000.0f);
                }
            }

            const auto coeff = createBandCoefficients (type, currentSampleRate, freq, q, gain, slope);
            const auto coeffStage2 = createBandCoefficients (type, currentSampleRate, freq, q, gain, slope);
            eqFilters[0][static_cast<size_t> (band)].coefficients = coeff;
            eqFilters[1][static_cast<size_t> (band)].coefficients = coeff;
            eqFiltersStage2[0][static_cast<size_t> (band)].coefficients = coeffStage2;
            eqFiltersStage2[1][static_cast<size_t> (band)].coefficients = coeffStage2;
            eqFiltersStage3[0][static_cast<size_t> (band)].coefficients = coeffStage2;
            eqFiltersStage3[1][static_cast<size_t> (band)].coefficients = coeffStage2;
            eqFiltersStage4[0][static_cast<size_t> (band)].coefficients = coeffStage2;
            eqFiltersStage4[1][static_cast<size_t> (band)].coefficients = coeffStage2;

            if (channelMode == 3)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    auto currentSample = buffer.getSample (0, sample);
                    currentSample = eqFilters[0][static_cast<size_t> (band)].processSample (currentSample);
                    if (stageCount >= 2)
                        currentSample = eqFiltersStage2[0][static_cast<size_t> (band)].processSample (currentSample);
                    if (stageCount >= 3)
                        currentSample = eqFiltersStage3[0][static_cast<size_t> (band)].processSample (currentSample);
                    if (stageCount >= 4)
                        currentSample = eqFiltersStage4[0][static_cast<size_t> (band)].processSample (currentSample);
                    buffer.setSample (0, sample, currentSample);
                }
            }
            else if (channelMode == 4)
            {
                if (buffer.getNumChannels() > 1)
                {
                    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    {
                        auto currentSample = buffer.getSample (1, sample);
                        currentSample = eqFilters[1][static_cast<size_t> (band)].processSample (currentSample);
                        if (stageCount >= 2)
                            currentSample = eqFiltersStage2[1][static_cast<size_t> (band)].processSample (currentSample);
                        if (stageCount >= 3)
                            currentSample = eqFiltersStage3[1][static_cast<size_t> (band)].processSample (currentSample);
                        if (stageCount >= 4)
                            currentSample = eqFiltersStage4[1][static_cast<size_t> (band)].processSample (currentSample);
                        buffer.setSample (1, sample, currentSample);
                    }
                }
            }
            else
            {
                for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
                {
                    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    {
                        auto currentSample = buffer.getSample (channel, sample);
                        currentSample = eqFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].processSample (currentSample);
                        if (stageCount >= 2)
                            currentSample = eqFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].processSample (currentSample);
                        if (stageCount >= 3)
                            currentSample = eqFiltersStage3[static_cast<size_t> (channel)][static_cast<size_t> (band)].processSample (currentSample);
                        if (stageCount >= 4)
                            currentSample = eqFiltersStage4[static_cast<size_t> (channel)][static_cast<size_t> (band)].processSample (currentSample);
                        buffer.setSample (channel, sample, currentSample);
                    }
                }
            }
        }
    }

    // True solo audition path: isolate selected regions with type-aware filters.
    if (soloCount > 0)
    {
        const auto auditionPost = ! bypass;
        if (auditionPost)
            soloSourceBuffer.makeCopyOf (buffer, true);
        else
            soloSourceBuffer.makeCopyOf (dryBuffer, true);

        std::array<float, maxBands> soloBandLinearGain {};

        for (int i = 0; i < soloCount; ++i)
        {
            const auto band = soloBands[static_cast<size_t> (i)];
            const auto freq = clampValue (bands[static_cast<size_t> (band)].frequency.load(), 20.0f, 20000.0f);
            const auto programQ = clampValue (bands[static_cast<size_t> (band)].q.load(), 0.10f, 10.0f);
            const auto type = static_cast<int> (std::round (bands[static_cast<size_t> (band)].type.load()));
            const auto slope = clampValue (bands[static_cast<size_t> (band)].slope.load(), 6.0f, 96.0f);
            const auto mode = static_cast<int> (std::round (bands[static_cast<size_t> (band)].mode.load()));
            const auto staticGainDb = clampValue (bands[static_cast<size_t> (band)].gainDb.load(), -30.0f, 30.0f);
            const auto dynamicGainDb = bandDynamicGainDb[static_cast<size_t> (band)];
            const auto movementDb = clampValue (staticGainDb + dynamicGainDb, -30.0f, 30.0f);

            juce::dsp::IIR::Coefficients<float>::Ptr coeff;

            // Solo audition should be focused but still retain some context (FabFilter-like).
            auto auditionFreq = freq;
            auto auditionQ = juce::jmax (3.0f, programQ * 1.55f);

            switch (type)
            {
                case 1: // Low shelf -> emphasize shelf knee region.
                    auditionQ = juce::jlimit (3.8f, 8.4f, 4.2f + 0.05f * std::abs (movementDb));
                    auditionFreq = clampValue (freq * 0.88f, 20.0f, 20000.0f);
                    break;

                case 2: // High shelf -> emphasize shelf knee region.
                    auditionQ = juce::jlimit (3.8f, 8.4f, 4.2f + 0.05f * std::abs (movementDb));
                    auditionFreq = clampValue (freq * 1.12f, 20.0f, 20000.0f);
                    break;

                case 3: // High-pass -> focus tightly around cutoff transition.
                case 4: // Low-pass -> focus tightly around cutoff transition.
                    auditionQ = juce::jlimit (3.6f, 8.6f, 3.7f + 0.20f * (slope / 24.0f));
                    break;

                case 5: // Notch -> very narrow whistle-like focus.
                    auditionQ = juce::jlimit (7.0f, 16.0f, juce::jmax (7.0f, programQ * 2.1f));
                    break;

                case 6: // Band-pass
                    auditionQ = juce::jlimit (3.8f, 9.5f, juce::jmax (3.8f, programQ * 1.65f));
                    break;

                case 7: // Tilt
                    auditionQ = 3.8f;
                    break;

                case 0:
                default: // Bell
                    auditionQ = juce::jlimit (3.4f, 9.5f, juce::jmax (3.4f, programQ * 1.7f));
                    break;
            }

            coeff = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, auditionFreq, auditionQ);
            const auto coeffStage2 = juce::dsp::IIR::Coefficients<float>::makeBandPass (
                currentSampleRate,
                auditionFreq,
                juce::jlimit (3.0f, 12.0f, auditionQ * 1.06f));

            for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
            {
                auditionFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = coeff;
                auditionFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].coefficients = coeffStage2;
            }

            // Keep solo audible and dynamic-aware while still heavily isolating the region.
            auto makeupDb = juce::jlimit (0.0f, 6.0f, 2.2f + 0.14f * std::abs (movementDb));
            if (mode == 1)
                makeupDb = juce::jlimit (0.0f, 7.2f, makeupDb + 0.4f * std::abs (dynamicGainDb));
            if (type == 5)
                makeupDb = juce::jlimit (0.0f, 8.0f, makeupDb + 0.6f);
            soloBandLinearGain[static_cast<size_t> (band)] = juce::Decibels::decibelsToGain (makeupDb);
        }

        buffer.clear();
        const auto gainComp = 1.0f / std::sqrt (static_cast<float> (juce::jmax (1, soloCount)));

        for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto src = soloSourceBuffer.getSample (channel, sample);
                float auditionSum = 0.0f;

                for (int i = 0; i < soloCount; ++i)
                {
                    const auto band = soloBands[static_cast<size_t> (i)];
                    const auto filteredStage1 = auditionFilters[static_cast<size_t> (channel)][static_cast<size_t> (band)].processSample (src);
                    const auto filteredStage2 = auditionFiltersStage2[static_cast<size_t> (channel)][static_cast<size_t> (band)].processSample (filteredStage1);
                    const auto filtered = filteredStage2 * 0.72f + filteredStage1 * 0.28f;
                    auditionSum += filtered * soloBandLinearGain[static_cast<size_t> (band)];
                }

                buffer.setSample (channel, sample, juce::jlimit (-1.5f, 1.5f, auditionSum * gainComp));
            }
        }
    }

    buffer.applyGain (outputLinear);

    float peak = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto preL = dryBuffer.getSample (0, sample);
        const auto preR = dryBuffer.getNumChannels() > 1 ? dryBuffer.getSample (1, sample) : preL;
        const auto postL = buffer.getSample (0, sample);
        const auto postR = buffer.getNumChannels() > 1 ? buffer.getSample (1, sample) : postL;

        pushSampleToAnalyzer (0.5f * (preL + preR), 0.5f * (postL + postR));
        peak = juce::jmax (peak, std::abs (postL));
        peak = juce::jmax (peak, std::abs (postR));
    }

    outputPeakLevel.store (peak);
    refreshAnalyzerData();
}

bool NovaCurveAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaCurveAudioProcessor::createEditor()
{
    return new NovaCurveAudioProcessorEditor (*this);
}

juce::var NovaCurveAudioProcessor::createStateVar() const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("selectedBand", selectedBand.load());
    root->setProperty ("phaseMode", phaseMode.load());
    root->setProperty ("qualityMode", qualityMode.load());
    root->setProperty ("analyzerMode", analyzerMode.load());
    root->setProperty ("harmonicLink", harmonicLink.load());
    root->setProperty ("outputGainDb", outputGainDb.load());
    root->setProperty ("bypassed", bypassed.load());
    root->setProperty ("resonanceAmount", resonanceAmount.load());

    juce::Array<juce::var> bandList;
    bandList.ensureStorageAllocated (maxBands);

    for (int band = 0; band < maxBands; ++band)
    {
        auto item = std::make_unique<juce::DynamicObject>();
        const auto index = static_cast<size_t> (band);

        item->setProperty ("enabled", bands[index].enabled.load());
        item->setProperty ("type", bands[index].type.load());
        item->setProperty ("mode", bands[index].mode.load());
        item->setProperty ("channel", bands[index].channel.load());
        item->setProperty ("frequency", bands[index].frequency.load());
        item->setProperty ("gainDb", bands[index].gainDb.load());
        item->setProperty ("q", bands[index].q.load());
        item->setProperty ("slope", bands[index].slope.load());
        item->setProperty ("dynRangeDb", bands[index].dynRangeDb.load());
        item->setProperty ("thresholdMode", bands[index].thresholdMode.load());
        item->setProperty ("thresholdDb", bands[index].thresholdDb.load());
        item->setProperty ("attackMs", bands[index].attackMs.load());
        item->setProperty ("releaseMs", bands[index].releaseMs.load());
        item->setProperty ("ratio", bands[index].ratio.load());
        item->setProperty ("solo", bands[index].solo.load());

        bandList.add (juce::var (item.release()));
    }

    root->setProperty ("bands", juce::var (bandList));
    return juce::var (root.release());
}

void NovaCurveAudioProcessor::applyStateVar (const juce::var& parsed)
{
    selectedBand.store (getObjectNumber (parsed, "selectedBand", selectedBand.load()));
    phaseMode.store (getObjectNumber (parsed, "phaseMode", phaseMode.load()));
    qualityMode.store (getObjectNumber (parsed, "qualityMode", qualityMode.load()));
    analyzerMode.store (getObjectNumber (parsed, "analyzerMode", analyzerMode.load()));
    harmonicLink.store (getObjectNumber (parsed, "harmonicLink", harmonicLink.load()));
    outputGainDb.store (clampValue (getObjectNumber (parsed, "outputGainDb", outputGainDb.load()), -24.0f, 24.0f));
    bypassed.store (getObjectNumber (parsed, "bypassed", bypassed.load()));
    resonanceAmount.store (clampValue (getObjectNumber (parsed, "resonanceAmount", resonanceAmount.load()), 0.0f, 100.0f));

    if (auto* dyn = parsed.getDynamicObject())
    {
        const auto bandsVar = dyn->getProperty ("bands");
        if (auto* array = bandsVar.getArray())
        {
            const auto count = juce::jmin (maxBands, array->size());
            for (int band = 0; band < count; ++band)
            {
                const auto& item = array->getReference (band);
                const auto index = static_cast<size_t> (band);
                bands[index].enabled.store (getObjectNumber (item, "enabled", bands[index].enabled.load()));
                bands[index].type.store (getObjectNumber (item, "type", bands[index].type.load()));
                bands[index].mode.store (getObjectNumber (item, "mode", bands[index].mode.load()));
                bands[index].channel.store (getObjectNumber (item, "channel", bands[index].channel.load()));
                bands[index].frequency.store (clampValue (getObjectNumber (item, "frequency", bands[index].frequency.load()), 20.0f, 20000.0f));
                bands[index].gainDb.store (clampValue (getObjectNumber (item, "gainDb", bands[index].gainDb.load()), -30.0f, 30.0f));
                bands[index].q.store (clampValue (getObjectNumber (item, "q", bands[index].q.load()), 0.10f, 10.0f));
                bands[index].slope.store (clampValue (getObjectNumber (item, "slope", bands[index].slope.load()), 6.0f, 120.0f));
                bands[index].dynRangeDb.store (clampValue (getObjectNumber (item, "dynRangeDb", bands[index].dynRangeDb.load()), -30.0f, 30.0f));
                bands[index].thresholdMode.store (getObjectNumber (item, "thresholdMode", bands[index].thresholdMode.load()));
                bands[index].thresholdDb.store (clampValue (getObjectNumber (item, "thresholdDb", bands[index].thresholdDb.load()), -60.0f, 6.0f));
                bands[index].attackMs.store (clampValue (getObjectNumber (item, "attackMs", bands[index].attackMs.load()), 0.1f, 200.0f));
                bands[index].releaseMs.store (clampValue (getObjectNumber (item, "releaseMs", bands[index].releaseMs.load()), 10.0f, 1000.0f));
                bands[index].ratio.store (clampValue (getObjectNumber (item, "ratio", bands[index].ratio.load()), 1.0f, 10.0f));
                bands[index].solo.store (getObjectNumber (item, "solo", bands[index].solo.load()));
            }
        }
    }

    const auto selectedIndex = juce::jlimit (0, maxBands - 1, static_cast<int> (std::round (selectedBand.load())));
    const auto selected = static_cast<size_t> (selectedIndex);

    uiStateApplyCount.fetch_add (1, std::memory_order_relaxed);
    uiStateLastApplyMs.store (static_cast<int> (juce::Time::getMillisecondCounter()), std::memory_order_relaxed);
    uiStateDiagSelectedBand.store (selectedIndex, std::memory_order_relaxed);
    uiStateDiagFrequency.store (bands[selected].frequency.load(), std::memory_order_relaxed);
    uiStateDiagGainDb.store (bands[selected].gainDb.load(), std::memory_order_relaxed);
    uiStateDiagQ.store (bands[selected].q.load(), std::memory_order_relaxed);
    uiStateDiagEnabled.store (bands[selected].enabled.load() > 0.5f ? 1 : 0, std::memory_order_relaxed);
    uiStateDiagSolo.store (bands[selected].solo.load() > 0.5f ? 1 : 0, std::memory_order_relaxed);
}

juce::String NovaCurveAudioProcessor::getUiStateAsJson() const
{
    return juce::JSON::toString (createStateVar());
}

void NovaCurveAudioProcessor::applyUiStateFromJson (const juce::String& jsonText)
{
    if (jsonText.isEmpty())
        return;

    const auto parsed = juce::JSON::parse (jsonText);
    if (! parsed.isVoid())
        applyStateVar (parsed);
}

void NovaCurveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto jsonText = getUiStateAsJson();
    juce::MemoryOutputStream (destData, true).writeString (jsonText);
}

void NovaCurveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto jsonText = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);
    applyUiStateFromJson (jsonText);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaCurveAudioProcessor();
}
