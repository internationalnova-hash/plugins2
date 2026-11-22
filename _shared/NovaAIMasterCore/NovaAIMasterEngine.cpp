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
