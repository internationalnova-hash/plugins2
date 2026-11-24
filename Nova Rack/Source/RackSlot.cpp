#include "RackSlot.h"

#include <map>

namespace nr
{

//==============================================================================
void DryDelay::prepare (int numChannels, int maxBlock)
{
    size = kMaxDelay + std::max (1, maxBlock) + 1;
    ring.setSize (std::max (1, numChannels), size);
    reset();
}

void DryDelay::reset() noexcept
{
    ring.clear();
    writePos = 0;
}

void DryDelay::process (const float* const* in, float* const* out, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, ring.getNumChannels());
    const int d = delay.load();

    if (size == 0)
    {
        for (int c = 0; c < numChannels; ++c)
            juce::FloatVectorOperations::copy (out[c], in[c], numSamples);
        return;
    }

    // Write-then-read, so a delay of 0 is a straight copy and the history stays current.
    for (int c = 0; c < numChannels; ++c)
    {
        auto* r = ring.getWritePointer (c);
        int w = writePos;
        int rd = w - d;
        if (rd < 0) rd += size;
        for (int i = 0; i < numSamples; ++i)
        {
            r[w] = in[c][i];
            out[c][i] = r[rd];
            if (++w == size) w = 0;
            if (++rd == size) rd = 0;
        }
    }
    writePos = (writePos + numSamples) % size;
}

//==============================================================================
void mixWetDry (juce::AudioBuffer<float>& wetBuf, const juce::AudioBuffer<float>& dryBuf,
                int numChannels, int numSamples, juce::LinearSmoothedValue<float>& wetGain) noexcept
{
    if (! wetGain.isSmoothing())
    {
        const float g = wetGain.getTargetValue();
        for (int c = 0; c < numChannels; ++c)
        {
            auto* w = wetBuf.getWritePointer (c);
            const auto* d = dryBuf.getReadPointer (c);
            if (g >= 1.0f) continue;
            if (g <= 0.0f) { juce::FloatVectorOperations::copy (w, d, numSamples); continue; }
            juce::FloatVectorOperations::multiply (w, g, numSamples);
            juce::FloatVectorOperations::addWithMultiply (w, d, 1.0f - g, numSamples);
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = wetGain.getNextValue();
        for (int c = 0; c < numChannels; ++c)
            wetBuf.setSample (c, i, wetBuf.getSample (c, i) * g + dryBuf.getSample (c, i) * (1.0f - g));
    }
}

//==============================================================================
Slot::~Slot()
{
    window.reset();      // the plugin's editor must go before the plugin
    if (instance != nullptr && prepared)
        instance->releaseResources();
}

void Slot::prepare (double sampleRate, int maxBlock, int numChannels)
{
    numChannels = std::max (1, numChannels);
    maxBlock = std::max (1, maxBlock);

    int pluginChannels = numChannels;
    if (instance != nullptr)
    {
        if (prepared)
            instance->releaseResources();

        // Main bus = our channel count; side-chains and extra buses off.
        auto layout = instance->getBusesLayout();
        const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        for (int i = 0; i < layout.inputBuses.size(); ++i)
            layout.inputBuses.getReference (i) = i == 0 ? set : juce::AudioChannelSet::disabled();
        for (int i = 0; i < layout.outputBuses.size(); ++i)
            layout.outputBuses.getReference (i) = i == 0 ? set : juce::AudioChannelSet::disabled();
        if (! instance->setBusesLayout (layout))
        {
            // Fall back to the plugin's own preference; we copy channels in and out to match.
            instance->disableNonMainBuses();
        }

        instance->setRateAndBufferSizeDetails (sampleRate, maxBlock);
        instance->prepareToPlay (sampleRate, maxBlock);
        prepared = true;
        pluginChannels = std::max (instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
    }

    work.setSize (std::max (numChannels, pluginChannels), maxBlock);
    dryBuf.setSize (numChannels, maxBlock);
    dry.prepare (numChannels, maxBlock);
    refreshLatency();
    wet.reset (sampleRate, 0.03);
    wet.setCurrentAndTargetValue (1.0f);
}

void Slot::release()
{
    if (instance != nullptr && prepared)
        instance->releaseResources();
    prepared = false;
}

void Slot::refreshLatency() noexcept
{
    dry.setDelay (instance != nullptr ? instance->getLatencySamples() : 0);
}

void Slot::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, float wetAmount,
                    juce::AudioPlayHead* playHead, bool nonRealtime) noexcept
{
    const int nc = std::min (buffer.getNumChannels(), dryBuf.getNumChannels());
    const int n = buffer.getNumSamples();
    if (n > dryBuf.getNumSamples() || nc == 0)
        return; // host broke its block-size promise: leave the audio untouched

    // The dry path always runs, delayed by the plugin's latency, so toggling bypass never shifts timing.
    dry.process (buffer.getArrayOfReadPointers(), dryBuf.getArrayOfWritePointers(), nc, n);

    wet.setTargetValue (juce::jlimit (0.0f, 1.0f, wetAmount));
    const bool silentWet = ! wet.isSmoothing() && wet.getTargetValue() <= 0.0f;

    if (instance == nullptr || ! prepared || silentWet)
    {
        for (int c = 0; c < nc; ++c)
            buffer.copyFrom (c, 0, dryBuf, c, 0, n);
        outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
        return;
    }

    // Run the plugin on a scratch buffer sized for its own channel count.
    const int pc = std::max (instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
    const int wc = std::min (work.getNumChannels(), std::max (pc, nc));
    for (int c = 0; c < wc; ++c)
    {
        if (c < nc) work.copyFrom (c, 0, buffer, c, 0, n);
        else        work.copyFrom (c, 0, buffer, nc - 1, 0, n); // mono in → feed the extra inputs
    }
    juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), wc, n);

    instance->setPlayHead (playHead);
    instance->setNonRealtime (nonRealtime);
    {
        const juce::ScopedLock pl (instance->getCallbackLock());
        if (instance->isSuspended())
            view.clear();
        else
            instance->processBlock (view, midi);
    }

    const int outCh = std::max (1, instance->getTotalNumOutputChannels());
    for (int c = 0; c < nc; ++c)
        buffer.copyFrom (c, 0, view, std::min (c, std::min (outCh, wc) - 1), 0, n);

    mixWetDry (buffer, dryBuf, nc, n, wet);
    outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
}

juce::MemoryBlock Slot::getState() const
{
    if (instance == nullptr)
        return savedState;
    juce::MemoryBlock mb;
    instance->getStateInformation (mb);
    return mb;
}

void Slot::setState (const juce::MemoryBlock& mb)
{
    if (instance == nullptr)
    {
        savedState = mb;
        return;
    }
    if (mb.getSize() > 0)
        instance->setStateInformation (mb.getData(), (int) mb.getSize());
}

// Hand-picked rack-unit knobs for the main Nova plugins (parameter names as the plugins report them).
// Plugins not listed get their first eight automatable controls.
// Each plugin's main controls, most important first (the first is the big knob). A module shows
// 7 knobs, 5 when it has a meter or display, 3 for the 670's tube display, so each list is sized
// to what's visible. Names must match the plugin's parameter names exactly; NovaRackParamDump
// --audit-faces checks every installed plugin against this list.
static const std::map<juce::String, juce::StringArray>& curatedFaces()
{
    static const std::map<juce::String, juce::StringArray> faces {
        // 7 knobs
        { "Nova 7even",      { "Preamp Gain", "Character", "HPF", "Low Gain", "Mid Frequency", "Mid Gain", "High Shelf (12k)" } },
        { "Nova Desk EQ",    { "Low Gain", "HPF Frequency", "Low Mid Frequency", "Low Mid Gain", "High Mid Frequency", "High Mid Gain", "High Gain" } },
        { "Nova Tone",       { "Low Boost", "Low Attenuation", "Low Frequency", "High Boost", "High Boost Frequency", "Bandwidth", "High Attenuation" } },
        // 3 knobs (tube display)
        { "Nova Royal 670",  { "Input", "AC Threshold", "Time Constant" } },
        // 5 knobs
        { "Nova 2-Track",    { "Vox Carve", "Low End", "Punch", "Width", "Ceiling" } },
        { "Nova AI Master",  { "Intensity", "Style", "Loudness" } },
        { "Nova AI Vocal",   { "Vibe", "Tune", "Clean", "De-Ess", "Space" } },
        { "Nova Align",      { "Align Strength", "Align Mode", "Max Shift", "Preserve Transients", "Smoothing" } },
        { "Nova Apex",       { "Drive", "Ceiling", "Output", "Release", "Style" } },
        { "Nova Aura",       { "Mid Aura", "High Aura", "Mix", "Wide", "Safe" } },
        { "Nova Autotune",   { "Retune Speed", "Key", "Scale", "Amount", "Tolerance" } },
        { "Nova Bend",       { "Pitch", "Formant", "Flatten", "Flatten Note", "Mix" } },
        { "Nova Clean",      { "Clean", "Mode", "Preserve", "Mix", "Output Gain" } },
        { "Nova Clip",       { "Drive", "Ceiling", "Clip Shape", "Tone", "Mix" } },
        { "Nova Console",    { "Drive", "Console Mode", "Threshold", "HPF", "Output" } },
        { "Nova Delay",      { "Delay Time Sync", "Feedback", "Mix", "Tone", "Mode" } },
        { "Nova Doubler",    { "Width", "Tightness", "Mix", "Voices" } },
        { "Nova Glow",       { "Peak Reduction", "Gain", "Glow", "Mix", "Compress/Limit" } },
        { "Nova Glue",       { "Glue", "Threshold", "Ratio", "Attack", "Release" } },
        { "Nova Harmony",    { "Voices", "Style", "Width", "Key", "Mix" } },
        { "Nova Heat",       { "Heat", "Drive", "Tone", "Mix", "Output Gain" } },
        { "Nova Level",      { "Compression", "Tone", "Mode", "Output", "Magic" } },
        { "Nova Master",     { "Finish", "Tone", "Glue", "Weight", "Air" } },
        { "Nova Mid",        { "Push", "Ceiling", "Output" } },
        { "Nova Motion FX",  { "Motion", "Cutoff", "Resonance", "Feedback", "Mix" } },
        { "Nova One",        { "Clean Amount", "Warm Amount", "Wide Amount", "Loud Amount", "Output" } },
        { "Nova Pitch",      { "Retune Speed", "Humanize", "Key", "Scale", "Natural Vibrato" } },
        { "Nova Punch",      { "Punch", "Body", "Focus", "Sensitivity", "Mix" } },
        { "Nova Ride",       { "Target", "Range", "Speed", "Sensitivity", "Output" } },
        { "Nova Silk",       { "Smooth", "Focus", "Air Preserve", "Body", "Output" } },
        { "Nova Space",      { "Space", "Decay", "Mix", "Pre-Delay", "Damping" } },
        { "Nova Sub",        { "Color", "Filter", "Mix", "Mode" } },
        { "Nova Tape",       { "Tape", "Input", "Tape Type", "IPS", "Output" } },
        { "Nova Translate",  { "Environment", "Environment Amount", "Position", "Room Amount", "Character Amount" } },
        { "Nova Velvet",     { "Threshold", "Ratio", "Attack", "Release", "Gain" } },
        { "Nova Voice",      { "Pitch", "Morph", "Texture", "Form", "Blend" } },
        { "Nova Vox",        { "Console Amount", "Curve Amount", "Level Amount", "Motion Amount", "Space Amount" } },
    };
    return faces;
}

void Slot::chooseDefaultFace()
{
    face.clear();
    if (instance == nullptr)
        return;

    const auto& params = instance->getParameters();
    const auto& curated = curatedFaces();
    if (auto it = curated.find (pluginName); it != curated.end())
    {
        for (const auto& wanted : it->second)
            for (int i = 0; i < params.size(); ++i)
                if (params[i]->getName (64).equalsIgnoreCase (wanted)) { face.add (i); break; }
        if (! face.isEmpty())
            return;
    }

    for (int i = 0; i < params.size() && face.size() < 8; ++i)
    {
        auto* p = params[i];
        const auto name = p->getName (64).toLowerCase();
        if (! p->isAutomatable() || p->isMetaParameter() || name.isEmpty()
            || name.contains ("bypass") || name.startsWith ("midi cc") || name.contains ("program"))
            continue;
        face.add (i);
    }
}

juce::StringArray applyParamTexts (juce::AudioPluginInstance& inst, const juce::StringPairArray& values)
{
    juce::StringArray unmatched;
    const auto& params = inst.getParameters();
    for (const auto& key : values.getAllKeys())
    {
        const auto text = values[key].trim();
        juce::AudioProcessorParameter* target = nullptr;
        for (auto* p : params)
        {
            if (p->getName (128).trim().equalsIgnoreCase (key)) { target = p; break; }
            if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (p))
                if (h->getParameterID().equalsIgnoreCase (key)) { target = p; break; }
        }
        if (target == nullptr) { unmatched.add (key); continue; }

        // "=0.42" sets the normalised value directly; anything else goes through the plugin's own text parser.
        const float v = text.startsWithChar ('=') ? text.substring (1).getFloatValue() : target->getValueForText (text);
        target->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    }
    return unmatched;
}

} // namespace nr
