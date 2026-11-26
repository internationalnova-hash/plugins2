#include "NovaAIMasterEngine.h"

void NovaAIMasterEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    loudnessAnalyzer.prepare (spec.sampleRate);
    dynamicsAnalyzer.prepare (spec.sampleRate);
    spectralAnalyzer.prepare (spec.sampleRate);
    stereoAnalyzer.prepare (spec.sampleRate);
    masteringChain.prepare (spec);
    reset();
}

void NovaAIMasterEngine::reset() noexcept
{
    loudnessAnalyzer.reset();
    dynamicsAnalyzer.reset();
    spectralAnalyzer.reset();
    stereoAnalyzer.reset();
    masteringChain.reset();
    profile = NovaAudioProfile{};
    totalSamplesAnalyzed = 0;
}

void NovaAIMasterEngine::analyze (const juce::AudioBuffer<float>& buffer) noexcept
{
    loudnessAnalyzer.process (buffer);
    dynamicsAnalyzer.process (buffer);
    spectralAnalyzer.process (buffer);
    stereoAnalyzer.process (buffer);
    totalSamplesAnalyzed += buffer.getNumSamples();
}

void NovaAIMasterEngine::finishAnalysis() noexcept
{
    loudnessAnalyzer.finalize();
    dynamicsAnalyzer.finalize();
    spectralAnalyzer.finalize();

    profile.integratedLufs   = loudnessAnalyzer.getIntegratedLufs();
    profile.shortTermLufs    = loudnessAnalyzer.getShortTermLufs();
    profile.loudnessRangeLu  = loudnessAnalyzer.getLoudnessRangeLu();
    profile.truePeakDb       = loudnessAnalyzer.getTruePeakDb();
    profile.samplePeakDb     = loudnessAnalyzer.getSamplePeakDb();

    profile.crestFactorDb      = dynamicsAnalyzer.getCrestFactorDb();
    profile.macroDynamicRange  = dynamicsAnalyzer.getMacroDynamicRange();
    profile.microDynamics      = dynamicsAnalyzer.getMicroDynamics();
    profile.transientDensity   = dynamicsAnalyzer.getTransientDensity();
    profile.transientStrength  = dynamicsAnalyzer.getTransientStrength();
    profile.compressionAmount  = dynamicsAnalyzer.getCompressionAmount();

    profile.subEnergy      = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::Sub);
    profile.bassEnergy     = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::Bass);
    profile.lowMidEnergy   = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::LowMid);
    profile.midEnergy      = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::Mid);
    profile.upperMidEnergy = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::UpperMid);
    profile.presenceEnergy = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::Presence);
    profile.highEnergy     = spectralAnalyzer.getBandEnergy (NovaSpectralAnalyzer::High);
    profile.spectralTilt   = spectralAnalyzer.getSpectralTilt();
    profile.harshness      = spectralAnalyzer.getHarshness();
    profile.resonanceScore = spectralAnalyzer.getResonanceScore();

    profile.stereoWidth       = stereoAnalyzer.getWidth();
    profile.stereoCorrelation = stereoAnalyzer.getCorrelation();
    profile.lowFrequencyWidth = stereoAnalyzer.getLowFrequencyWidth();
    profile.isMono              = stereoAnalyzer.isMono();

    profile.headroomDb   = -profile.truePeakDb;
    profile.sampleRate    = sampleRate;
    profile.lengthSeconds = sampleRate > 0.0 ? (double) totalSamplesAnalyzed / sampleRate : 0.0;
    // Confidence gate input for the decision engine (spec section 39: "if
    // analysis confidence is low, do less") - treat as silent if there's
    // effectively no signal or not enough material to trust the analysis.
    profile.isSilent = profile.samplePeakDb < -60.f || profile.lengthSeconds < 0.5;
}

NovaMasteringPlan NovaAIMasterEngine::decide (const NovaMasterUserPrefs& prefs) const
{
    return decisionEngine.decide (profile, prefs);
}

void NovaAIMasterEngine::setPlan (const NovaMasteringPlan& plan) noexcept
{
    masteringChain.setPlan (plan);
}

void NovaAIMasterEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    masteringChain.process (buffer);
}

NovaMasteringPlan NovaAIMasterEngine::calibrateLoudness (NovaMasteringPlan plan, const juce::AudioBuffer<float>& source) const
{
    const int n = source.getNumSamples(), nCh = juce::jmax (1, source.getNumChannels());
    if (! plan.output.applyGain || n < (int) (sampleRate * 2.0) || profile.isSilent) return plan;

    constexpr int kBlock = 4096;
    auto chain = std::make_unique<NovaMasteringChain>();
    chain->prepare ({ sampleRate, (juce::uint32) kBlock, (juce::uint32) nCh });
    juce::AudioBuffer<float> work;

    auto measure = [&] (float gainDb)
    {
        plan.output.gainDb = gainDb;
        chain->reset();
        chain->setPlan (plan);
        work.makeCopyOf (source);
        for (int p = 0; p < n; p += kBlock)
        {
            juce::AudioBuffer<float> v (work.getArrayOfWritePointers(), work.getNumChannels(), p, juce::jmin (kBlock, n - p));
            chain->process (v);
        }
        NovaLoudnessAnalyzer meter;
        meter.prepare (sampleRate);
        meter.process (work);
        meter.finalize();
        return meter.getIntegratedLufs();
    };

    const float target = plan.output.targetLufs;
    constexpr float kMinGain = -12.f, kMaxGain = 12.f;   // the decision engine's own safety range (spec 39)
    float g0 = juce::jlimit (kMinGain, kMaxGain, plan.output.gainDb);
    float l0 = measure (g0);
    float bestG = g0, bestErr = std::abs (target - l0);
    // secant steps (more drive gives less and less loudness once the limiter works hard)
    float g1 = juce::jlimit (kMinGain, kMaxGain, g0 + (target - l0));
    for (int it = 0; it < 5 && bestErr > 0.2f && std::abs (g1 - g0) > 0.05f; ++it)
    {
        const float l1 = measure (g1);
        if (std::abs (target - l1) < bestErr) { bestErr = std::abs (target - l1); bestG = g1; }
        const float slope = (l1 - l0) / (g1 - g0);
        const float step = slope > 0.05f ? (target - l1) / slope : (target - l1);
        g0 = g1; l0 = l1;
        g1 = juce::jlimit (kMinGain, kMaxGain, g1 + juce::jlimit (-6.f, 6.f, step));
    }
    plan.output.gainDb = bestG;
    return plan;
}
