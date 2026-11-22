#include "NovaBassSamplePlayback.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace nova::bass
{

void NovaBassSamplePlayback::startLayer (Layer& layer, const NovaBassSampleZone* zone, int midiNote, double outputSampleRate, float extraCents) noexcept
{
    layer.zone = zone;
    layer.position = 0.0;
    layer.direction = 1;
    layer.finished = (zone == nullptr || zone->buffer == nullptr || zone->buffer->getNumSamples() == 0);

    if (! layer.finished)
    {
        const double semitoneOffset = (double) (midiNote - zone->rootNote) + (double) zone->tuningCents / 100.0 + (double) extraCents / 100.0;
        const double pitchRatio = std::pow (2.0, semitoneOffset / 12.0);
        layer.increment = pitchRatio * (zone->sourceSampleRate / outputSampleRate);
    }
}

void NovaBassSamplePlayback::start (const NovaBassSamplerEngine::ZoneSelection& selection, int midiNote, double outputSampleRate, float extraCents) noexcept
{
    startLayer (primary, selection.primary, midiNote, outputSampleRate, extraCents);
    startLayer (secondary, selection.secondary, midiNote, outputSampleRate, extraCents);
    secondaryBlend = selection.secondaryBlend;
    active = (! primary.finished) || (! secondary.finished);
}

float NovaBassSamplePlayback::readCatmullRom (const juce::AudioBuffer<float>& buf, int channel, double pos) noexcept
{
    const int numSamples = buf.getNumSamples();
    if (numSamples == 0) return 0.f;

    const int ch = juce::jlimit (0, buf.getNumChannels() - 1, channel);
    const int i1 = juce::jlimit (0, numSamples - 1, (int) std::floor (pos));
    const int i0 = juce::jmax (0, i1 - 1);
    const int i2 = juce::jmin (numSamples - 1, i1 + 1);
    const int i3 = juce::jmin (numSamples - 1, i1 + 2);
    const float t = (float) (pos - std::floor (pos));

    const float* d = buf.getReadPointer (ch);
    const float y0 = d[i0], y1 = d[i1], y2 = d[i2], y3 = d[i3];

    const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float a2 = -0.5f * y0 + 0.5f * y2;
    const float a3 = y1;
    return ((a0 * t + a1) * t + a2) * t + a3;
}

bool NovaBassSamplePlayback::advance (Layer& layer) noexcept
{
    if (layer.finished || layer.zone == nullptr || layer.zone->buffer == nullptr) return false;

    layer.position += layer.increment * (double) layer.direction;

    const auto& zone = *layer.zone;
    const double numSamples = (double) zone.buffer->getNumSamples();

    if (zone.loopEnabled && zone.loopEnd > zone.loopStart)
    {
        const double loopStart = (double) zone.loopStart;
        const double loopEnd = (double) zone.loopEnd;

        if (zone.loopMode == NovaBassLoopMode::PingPong)
        {
            if (layer.direction > 0 && layer.position >= loopEnd)
            {
                layer.position = loopEnd - (layer.position - loopEnd);
                layer.direction = -1;
            }
            else if (layer.direction < 0 && layer.position <= loopStart)
            {
                layer.position = loopStart + (loopStart - layer.position);
                layer.direction = 1;
            }
        }
        else
        {
            if (layer.position >= loopEnd)
                layer.position = loopStart + std::fmod (layer.position - loopEnd, juce::jmax (1.0, loopEnd - loopStart));
        }
        return true;
    }

    if (layer.position < 0.0 || layer.position >= numSamples - 1.0)
    {
        layer.finished = true;
        return false;
    }
    return true;
}

bool NovaBassSamplePlayback::renderAdding (float* outL, float* outR, int numSamples) noexcept
{
    if (! active) return false;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = 0.f, r = 0.f;

        if (! primary.finished && primary.zone != nullptr && primary.zone->buffer != nullptr)
        {
            const auto& z = *primary.zone;
            const auto& buf = *z.buffer;
            const float gain = juce::Decibels::decibelsToGain (z.gainDb) * (1.f - secondaryBlend);
            const float panL = juce::jmin (1.f, 1.f - juce::jmax (0.f, z.pan));
            const float panR = juce::jmin (1.f, 1.f + juce::jmin (0.f, z.pan));
            const bool stereoSrc = buf.getNumChannels() > 1;
            l += readCatmullRom (buf, 0, primary.position) * gain * panL;
            r += readCatmullRom (buf, stereoSrc ? 1 : 0, primary.position) * gain * panR;
            advance (primary);
        }

        if (! secondary.finished && secondary.zone != nullptr && secondary.zone->buffer != nullptr)
        {
            const auto& z = *secondary.zone;
            const auto& buf = *z.buffer;
            const float gain = juce::Decibels::decibelsToGain (z.gainDb) * secondaryBlend;
            const float panL = juce::jmin (1.f, 1.f - juce::jmax (0.f, z.pan));
            const float panR = juce::jmin (1.f, 1.f + juce::jmin (0.f, z.pan));
            const bool stereoSrc = buf.getNumChannels() > 1;
            l += readCatmullRom (buf, 0, secondary.position) * gain * panL;
            r += readCatmullRom (buf, stereoSrc ? 1 : 0, secondary.position) * gain * panR;
            advance (secondary);
        }

        outL[i] += l;
        outR[i] += r;
    }

    active = (! primary.finished) || (! secondary.finished);
    return active;
}

} // namespace nova::bass
