#include "NovaVelvetTubeStage.h"
#include "NovaVelvetCalibration.h"
#include <cmath>

namespace nova::velvet
{

void NovaVelvetTubeStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    dcBlockR = std::exp (-2.0f * juce::MathConstants<float>::pi * kDcBlockHz / (float) sampleRate);
    reset();
}

void NovaVelvetTubeStage::reset() noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
    }
}

void NovaVelvetTubeStage::setDrive (float drive01) noexcept
{
    drive = juce::jlimit (0.0f, 1.0f, drive01) * NovaVelvetCalibration::kTubeDriveRangeMax;
}

void NovaVelvetTubeStage::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const int N = buffer.getNumSamples();
    if (nCh == 0 || N == 0)
        return;

    if (drive <= 1.0e-4f)
        return; // 0 drive = bypass-transparent, no waveshaping, no DC-block coloration either.

    // Effective drive gain. NOTE: tanh(driveGain*x) alone has small-signal
    // slope == driveGain (tanh's derivative at 0 is 1, so tanh(k*x) has
    // derivative k there) - i.e. a bare tanh(driveGain*x) is NOT unity-gain
    // for quiet signals at any nonzero drive, it's a linear driveGain-times
    // boost until the signal gets large enough to reach the curve's bend.
    // Dividing the OUTPUT by the SAME driveGain restores exact unity
    // small-signal gain (slope 1 at the origin, by the tanh Taylor
    // expansion) while still moving the saturation knee to a LOWER input
    // level as drive rises - so THD at a fixed test level still grows
    // monotonically with drive (verified directly via
    // NovaVelvetHarmonicValidation/FrequencyResponseValidation after this
    // fix). This is NOT the same shape as the makeup-gain approach already
    // rejected elsewhere in this catalog (dividing by tanh(driveGain*0.3),
    // a DIFFERENT, mismatched scale from the driveGain*x actually applied
    // to the signal) - dividing by the matching driveGain instead is the
    // standard, provably-unity-small-signal-gain drive-control shape.
    const float driveGain = 1.0f + drive * 3.0f;
    const float invDriveGain = 1.0f / driveGain;

    // Push-pull-specific: even-harmonic bias starts low (real push-pull
    // stages cancel even harmonics at idle) and grows slightly with signal
    // level, modeling a real stage's bias imbalance under load - gradual,
    // no sudden threshold.
    const float evenHarmonicBias = juce::jlimit (0.0f, 1.0f,
        evenHarmonicBiasScale * (NovaVelvetCalibration::kTubeEvenHarmonicBiasBaseline
        + NovaVelvetCalibration::kTubeLevelDependentBiasGrowth * levelEnvelope01));

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float x1 = dcX1[ch];
        float y1 = dcY1[ch];

        for (int i = 0; i < N; ++i)
        {
            const float x = data[i];
            const float driven = driveGain * x;

            const float symmetric = std::tanh (driven);
            const float asymmetric = std::tanh (driven + kAsymmetryShift) - std::tanh (kAsymmetryShift);

            float y = (symmetric * (1.0f - evenHarmonicBias) + asymmetric * evenHarmonicBias) * invDriveGain;

            // One-pole DC blocker.
            const float blocked = y - x1 + dcBlockR * y1;
            x1 = y;
            y1 = blocked;

            data[i] = blocked;
        }

        dcX1[ch] = x1;
        dcY1[ch] = y1;
    }
}

} // namespace nova::velvet
