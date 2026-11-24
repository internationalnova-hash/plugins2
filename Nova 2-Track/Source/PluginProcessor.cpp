#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
tt::Settings S (float in, float width, float carve, float punch, float low, float instr, float energy,
                bool clip, float ceiling, float out = 0.0f)
{
    tt::Settings s;
    s.inputDb = in; s.width = width; s.voxCarve = carve; s.punch = punch; s.lowEnd = low;
    s.instruments = instr; s.energy = energy; s.clipOn = clip; s.ceilingDb = ceiling; s.outputDb = out;
    return s;
}
}

const std::vector<TwoTrackPreset>& twoTrackPresets()
{
    //                                       in  width carve punch low instr energy clip  ceil
    static const std::vector<TwoTrackPreset> p {
        { "Default",                 S (0, 100,   0,   0,   0,   0,   0, false, -0.3f) },
        { "Vocal Over Beat",         S (0, 110,  45,  20,  35,  20,  15, true,  -1.0f) },
        { "Rap Pocket",              S (0, 105,  60,  30,  40,  10,  10, true,  -1.0f) },
        { "Singer Space",            S (0, 115,  40,  10,  30,  25,  10, true,  -1.0f) },
        { "Wide and Open",           S (0, 140,  15,   0,  30,  35,  15, false, -0.3f) },
        { "Punchy Drums",            S (0, 100,   0,  60,  25,   0,  35, true,  -0.5f) },
        { "Club Low End",            S (0, 100,  10,  35,  70,   0,   0, true,  -0.5f) },
        { "Flat Beat Revive",        S (0, 115,   0,  35,  20,  30,  55, true,  -0.5f) },
        { "Loud 2-Track",            S (4, 110,  20,  30,  40,  25,  25, true,  -0.3f) },
        { "Mono Lows, Wide Top",     S (0, 130,   0,   0, 100,  20,   0, false, -0.3f) },
        { "Mix Bus Polish",          S (0, 108,   0,  15,  25,  15,  15, false, -0.3f) },
        { "Master Push",             S (3, 105,   0,  20,  30,  10,  15, true,  -0.3f) },
    };
    return p;
}

const juce::StringArray& twoTrackParamIds()
{
    static const juce::StringArray ids { "input", "width", "vox_carve", "punch", "low_end", "instruments",
                                         "energy", "clip", "ceiling", "output" };
    return ids;
}

NovaTwoTrackAudioProcessor::NovaTwoTrackAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NovaTwoTrack", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaTwoTrackAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto fmt = [] (const char* unit) { return [unit] (float v, int) { return String (v, 1) + unit; }; };
    auto pct = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };

    auto add = [&l] (const char* id, const char* name, float lo, float hi, float step, float def,
                     const char* label, std::function<String (float, int)> f)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def,
                   AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (std::move (f))));
    };

    add ("input",       "Input",       -12.0f, 12.0f, 0.1f, 0.0f,   "dB", fmt (" dB"));
    add ("width",       "Width",         0.0f, 200.0f, 1.0f, 100.0f, "%", pct);
    add ("vox_carve",   "Vox Carve",     0.0f, 100.0f, 1.0f, 0.0f,  "%", pct);
    add ("punch",       "Punch",         0.0f, 100.0f, 1.0f, 0.0f,  "%", pct);
    add ("low_end",     "Low End",       0.0f, 100.0f, 1.0f, 0.0f,  "%", pct);
    add ("instruments", "Instruments",   0.0f, 100.0f, 1.0f, 0.0f,  "%", pct);
    add ("energy",      "Energy",        0.0f, 100.0f, 1.0f, 0.0f,  "%", pct);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "clip", 1 }, "Soft Clipper", false));
    add ("ceiling",     "Ceiling",     -12.0f, 0.0f, 0.1f, -0.3f,  "dBFS", fmt (" dBFS"));
    add ("output",      "Output",      -12.0f, 6.0f, 0.1f, 0.0f,   "dB", fmt (" dB"));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

tt::Settings NovaTwoTrackAudioProcessor::readSettings() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    tt::Settings s;
    s.inputDb = v ("input"); s.width = v ("width"); s.voxCarve = v ("vox_carve"); s.punch = v ("punch");
    s.lowEnd = v ("low_end"); s.instruments = v ("instruments"); s.energy = v ("energy");
    s.clipOn = v ("clip") > 0.5f; s.ceilingDb = v ("ceiling"); s.outputDb = v ("output");
    return s;
}

bool NovaTwoTrackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void NovaTwoTrackAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.set (readSettings());
    engine.prepare (sampleRate, samplesPerBlock);
    const int lat = engine.getLatencySamples();
    setLatencySamples (lat);

    const int nCh = juce::jmax (1, getTotalNumOutputChannels());
    dryBuffer.setSize (nCh, juce::jmax (1, samplesPerBlock), false, true, false);
    delayRing.setSize (nCh, lat + 1, false, true, false);
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock), false, true, false);
    delayRing.clear();
    ringPos = 0;

    wetAmount.reset (sampleRate, 0.03);
    wetAmount.setCurrentAndTargetValue (bypassParam->get() ? 0.0f : 1.0f);
    corrLL = corrRR = 1.0e-9; corrLR = 0.0;
}

void NovaTwoTrackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin (buffer.getNumChannels(), delayRing.getNumChannels());
    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (n == 0 || nCh == 0)
        return;

    engine.set (readSettings());
    wetAmount.setTargetValue (bypassParam->get() ? 0.0f : 1.0f);

    inL.store (juce::jmax (inL.load(), buffer.getMagnitude (0, 0, n)));
    inR.store (juce::jmax (inR.load(), buffer.getMagnitude (nCh > 1 ? 1 : 0, 0, n)));

    // Latency-matched dry copy for a click-free bypass.
    dryBuffer.setSize (nCh, n, false, false, true);
    const int ringLen = delayRing.getNumSamples();
    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        float* ring = delayRing.getWritePointer (ch);
        float* dry = dryBuffer.getWritePointer (ch);
        int p = ringPos;
        for (int i = 0; i < n; ++i)
        {
            ring[p] = in[i];
            p = (p + 1) % ringLen;
            dry[i] = ring[p];
        }
    }
    ringPos = (ringPos + n) % ringLen;

    float* L = buffer.getWritePointer (0);
    float* R = nCh > 1 ? buffer.getWritePointer (1) : L;
    if (nCh > 1)
        engine.process (L, R, n);
    else
    {
        // Mono: run as dual-mono through a scratch copy so both "channels" stay identical.
        monoScratch.setSize (1, n, false, false, true);
        float* tmp = monoScratch.getWritePointer (0);
        juce::FloatVectorOperations::copy (tmp, L, n);
        engine.process (L, tmp, n);
    }
    overDb.store (juce::jmax (overDb.load(), engine.getLastOverDb()));
    punchDb.store (juce::jmax (punchDb.load(), engine.getLastPunchDb()));
    energyDb.store (juce::jmax (energyDb.load(), engine.getLastEnergyDb()));

    if (wetAmount.isSmoothing() || wetAmount.getTargetValue() < 0.5f)
        for (int i = 0; i < n; ++i)
        {
            const float w = wetAmount.getNextValue();
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* o = buffer.getWritePointer (ch);
                o[i] = o[i] * w + dryBuffer.getSample (ch, i) * (1.0f - w);
            }
        }

    // Meters, correlation and scope.
    outL.store (juce::jmax (outL.load(), buffer.getMagnitude (0, 0, n)));
    outR.store (juce::jmax (outR.load(), buffer.getMagnitude (nCh > 1 ? 1 : 0, 0, n)));
    const float* oL = buffer.getReadPointer (0);
    const float* oR = buffer.getReadPointer (nCh > 1 ? 1 : 0);
    const double decay = std::exp (-(double) n / (0.3 * getSampleRate()));
    double ll = 0, rr = 0, lr = 0;
    int w = scopeWrite.load (std::memory_order_relaxed);
    int fw = fftWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        ll += (double) oL[i] * oL[i]; rr += (double) oR[i] * oR[i]; lr += (double) oL[i] * oR[i];
        fftL[(size_t) fw].store (oL[i], std::memory_order_relaxed);
        fftR[(size_t) fw].store (oR[i], std::memory_order_relaxed);
        fw = (fw + 1) % kFftRing;
        if (++scopeDecim >= 2)
        {
            scopeDecim = 0;
            scopeL[(size_t) w].store (oL[i], std::memory_order_relaxed);
            scopeR[(size_t) w].store (oR[i], std::memory_order_relaxed);
            w = (w + 1) % kScopeSize;
        }
    }
    scopeWrite.store (w, std::memory_order_release);
    fftWrite.store (fw, std::memory_order_release);
    corrLL = corrLL * decay + ll; corrRR = corrRR * decay + rr; corrLR = corrLR * decay + lr;
    const double denom = std::sqrt (corrLL * corrRR);
    correlation.store (denom > 1.0e-7 ? (float) juce::jlimit (-1.0, 1.0, corrLR / denom) : 1.0f);
}

//==============================================================================
int NovaTwoTrackAudioProcessor::getNumPrograms() { return (int) twoTrackPresets().size(); }

const juce::String NovaTwoTrackAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (twoTrackPresets()[(size_t) index].name) : juce::String();
}

void NovaTwoTrackAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    const auto& s = twoTrackPresets()[(size_t) index].s;
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("input", s.inputDb); set ("width", s.width); set ("vox_carve", s.voxCarve); set ("punch", s.punch);
    set ("low_end", s.lowEnd); set ("instruments", s.instruments); set ("energy", s.energy);
    set ("clip", s.clipOn ? 1.0f : 0.0f); set ("ceiling", s.ceilingDb); set ("output", s.outputDb);
}

void NovaTwoTrackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaTwoTrackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* NovaTwoTrackAudioProcessor::createEditor()
{
    return new NovaTwoTrackAudioProcessorEditor (*this);
}
