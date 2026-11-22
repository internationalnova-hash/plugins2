#include "NovaKeysEngine.h"
#include <algorithm>

void NovaKeysEngine::prepare (double outputSampleRate, int maxBlockSize, int numVoices)
{
    sampleRate = outputSampleRate;
    activeVoiceCount = juce::jlimit (1, kMaxPossibleVoices, numVoices);

    for (auto& v : voices)
    {
        for (auto& layer : v.layers)
            layer.prepare (sampleRate, maxBlockSize);
        v.currentNote = -1;
        v.held = false;
        v.lastPeak = 0.f;
    }

    mixScratchL.assign ((size_t) maxBlockSize, 0.f);
    mixScratchR.assign ((size_t) maxBlockSize, 0.f);

    nextOrder = 0;
}

void NovaKeysEngine::configureLayerAsSample (int layerIndex, NovaSamplerEngine* samplerEngine, const NovaLayerParams& p)
{
    if (layerIndex < 0 || layerIndex >= kNumLayerSlots) return;
    auto& t = layerTemplates[(size_t) layerIndex];
    t.configured = true;
    t.type = NovaLayerSourceType::Sample;
    t.samplerEngine = samplerEngine;
    t.layerParams = p;
}

void NovaKeysEngine::configureLayerAsSynth (int layerIndex, const NovaSynthVoiceParams& synthParams, const NovaLayerParams& p)
{
    if (layerIndex < 0 || layerIndex >= kNumLayerSlots) return;
    auto& t = layerTemplates[(size_t) layerIndex];
    t.configured = true;
    t.type = NovaLayerSourceType::Synth;
    t.synthParams = synthParams;
    t.layerParams = p;
}

void NovaKeysEngine::setLayerUnconfigured (int layerIndex)
{
    if (layerIndex < 0 || layerIndex >= kNumLayerSlots) return;
    layerTemplates[(size_t) layerIndex].configured = false;
}

bool NovaKeysEngine::isVoiceSounding (const Voice& v) const noexcept
{
    for (auto& layer : v.layers)
        if (layer.isActive())
            return true;
    return false;
}

int NovaKeysEngine::findFreeOrStealVoice() noexcept
{
    // 1) A fully free, silent, not-held voice.
    for (int i = 0; i < activeVoiceCount; ++i)
        if (! voices[(size_t) i].held && ! isVoiceSounding (voices[(size_t) i]))
            return i;

    // 2) Steal the quietest voice that has already been released (spec
    //    section 8: prefer quietest/oldest, avoid audible clicks).
    int quietestReleased = -1;
    float quietestLevel = std::numeric_limits<float>::max();
    for (int i = 0; i < activeVoiceCount; ++i)
    {
        if (voices[(size_t) i].held) continue;
        if (voices[(size_t) i].lastPeak < quietestLevel)
        {
            quietestLevel = voices[(size_t) i].lastPeak;
            quietestReleased = i;
        }
    }
    if (quietestReleased >= 0) return quietestReleased;

    // 3) Every voice is still physically held - steal the oldest.
    int oldest = 0;
    juce::int64 oldestOrder = voices[0].startOrder;
    for (int i = 1; i < activeVoiceCount; ++i)
    {
        if (voices[(size_t) i].startOrder < oldestOrder)
        {
            oldestOrder = voices[(size_t) i].startOrder;
            oldest = i;
        }
    }
    return oldest;
}

void NovaKeysEngine::noteOn (int midiNote, int velocity) noexcept
{
    const int idx = findFreeOrStealVoice();
    auto& v = voices[(size_t) idx];

    for (int L = 0; L < kNumLayerSlots; ++L)
    {
        auto& t = layerTemplates[(size_t) L];
        auto& layer = v.layers[(size_t) L];
        if (! t.configured) { layer.reset(); continue; }

        if (t.type == NovaLayerSourceType::Sample)
            layer.configureAsSample (t.samplerEngine, t.layerParams);
        else
            layer.configureAsSynth (t.synthParams, t.layerParams);

        layer.noteOn (midiNote, velocity);
    }

    v.currentNote = midiNote;
    v.held = true;
    v.startOrder = nextOrder++;
    v.lastPeak = 1.f; // assume loud immediately after trigger; refined on next render
}

void NovaKeysEngine::noteOff (int midiNote) noexcept
{
    for (int i = 0; i < activeVoiceCount; ++i)
    {
        auto& v = voices[(size_t) i];
        if (v.held && v.currentNote == midiNote)
        {
            v.held = false;
            for (auto& layer : v.layers) layer.noteOff();
        }
    }
}

void NovaKeysEngine::allNotesOff() noexcept
{
    for (int i = 0; i < activeVoiceCount; ++i)
    {
        auto& v = voices[(size_t) i];
        v.held = false;
        for (auto& layer : v.layers) layer.noteOff();
    }
}

void NovaKeysEngine::hardResetAllVoices() noexcept
{
    for (int i = 0; i < activeVoiceCount; ++i)
    {
        auto& v = voices[(size_t) i];
        v.held = false;
        v.currentNote = -1;
        v.lastPeak = 0.f;
        for (auto& layer : v.layers) layer.reset();
    }
}

void NovaKeysEngine::renderNextBlock (float* outL, float* outR, int numSamples) noexcept
{
    std::fill (outL, outL + numSamples, 0.f);
    std::fill (outR, outR + numSamples, 0.f);

    const int n = juce::jmin (numSamples, (int) mixScratchL.size());
    if (n <= 0) return;

    for (int i = 0; i < activeVoiceCount; ++i)
    {
        auto& v = voices[(size_t) i];
        if (! isVoiceSounding (v))
        {
            if (! v.held) v.currentNote = -1;
            v.lastPeak = 0.f;
            continue;
        }

        std::fill (mixScratchL.begin(), mixScratchL.begin() + n, 0.f);
        std::fill (mixScratchR.begin(), mixScratchR.begin() + n, 0.f);

        for (auto& layer : v.layers)
            if (layer.isActive())
                layer.renderAdding (mixScratchL.data(), mixScratchR.data(), n);

        float peak = 0.f;
        for (int s = 0; s < n; ++s)
        {
            outL[s] += mixScratchL[(size_t) s];
            outR[s] += mixScratchR[(size_t) s];
            peak = juce::jmax (peak, std::abs (mixScratchL[(size_t) s]));
        }
        v.lastPeak = peak;

        if (! isVoiceSounding (v) && ! v.held)
            v.currentNote = -1;
    }
}

int NovaKeysEngine::getNumActiveVoices() const noexcept
{
    int count = 0;
    for (int i = 0; i < activeVoiceCount; ++i)
        if (voices[(size_t) i].held || isVoiceSounding (voices[(size_t) i]))
            ++count;
    return count;
}
