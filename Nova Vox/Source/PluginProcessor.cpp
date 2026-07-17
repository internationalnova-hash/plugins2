#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // ── Global ───────────────────────────────────────────────────────────────
    constexpr auto kGlobalBypass   = "global_bypass";
    constexpr auto kInputDb        = "global_input_db";
    constexpr auto kOutputDb       = "global_output_db";

    // ── Console ──────────────────────────────────────────────────────────────
    constexpr auto kConsoleOn      = "console_on";
    constexpr auto kConsoleAmount  = "console_amount";
    constexpr auto kConsoleMode    = "console_mode";

    // ── Curve ────────────────────────────────────────────────────────────────
    constexpr auto kCurveOn        = "curve_on";
    constexpr auto kCurveAmount    = "curve_amount";
    constexpr auto kCurveMode      = "curve_mode";
    constexpr auto kCurveLowDb     = "curve_low_db";
    constexpr auto kCurveMidDb     = "curve_mid_db";
    constexpr auto kCurveHighDb    = "curve_high_db";

    // ── Level ────────────────────────────────────────────────────────────────
    constexpr auto kLevelOn        = "level_on";
    constexpr auto kLevelAmount    = "level_amount";
    constexpr auto kLevelMode      = "level_mode";
    constexpr auto kLevelOutputDb  = "level_output_db";
    constexpr auto kLevelMagic     = "level_magic";

    // ── Motion ───────────────────────────────────────────────────────────────
    constexpr auto kMotionOn       = "motion_on";
    constexpr auto kMotionAmount   = "motion_amount";
    constexpr auto kMotionMode     = "motion_mode";
    constexpr auto kMotionRate     = "motion_rate";
    constexpr auto kMotionDepth    = "motion_depth";

    // ── Space ────────────────────────────────────────────────────────────────
    constexpr auto kSpaceOn        = "space_on";
    constexpr auto kSpaceAmount    = "space_amount";
    constexpr auto kSpaceMode      = "space_mode";
    constexpr auto kSpaceDecay     = "space_decay";
    constexpr auto kSpaceDamping   = "space_damping";
    constexpr auto kSpacePredelay  = "space_predelay";
}

NovaVoxAudioProcessor::NovaVoxAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaVox"), createParameterLayout())
{
}

NovaVoxAudioProcessor::~NovaVoxAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaVoxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Global
    layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { kGlobalBypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kInputDb,  1 }, "Input",
        juce::NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kOutputDb, 1 }, "Output",
        juce::NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f));

    // Console
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { kConsoleOn,     1 }, "Console On",   true));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kConsoleAmount,  1 }, "Console Amount",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 35.f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { kConsoleMode,    1 }, "Console Mode",
        juce::StringArray { "Clean", "British", "Tube/Tape", "Gold", "Modern" }, 1));

    // Curve
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { kCurveOn,     1 }, "Curve On",   true));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kCurveAmount,  1 }, "Curve Amount",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { kCurveMode,    1 }, "Curve Mode",
        juce::StringArray { "Flat", "Bright", "Warm", "Air" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kCurveLowDb,  1 }, "Curve Low",
        juce::NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kCurveMidDb,  1 }, "Curve Mid",
        juce::NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kCurveHighDb, 1 }, "Curve High",
        juce::NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));

    // Level
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { kLevelOn,      1 }, "Level On",     true));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kLevelAmount,   1 }, "Level Amount",
        juce::NormalisableRange<float> (0.f, 10.f, 0.01f), 3.f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { kLevelMode,     1 }, "Level Mode",
        juce::StringArray { "Smooth", "Punch", "Limit" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kLevelOutputDb, 1 }, "Level Output",
        juce::NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { kLevelMagic,    1 }, "Level Magic",  false));

    // Motion
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { kMotionOn,     1 }, "Motion On",    true));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kMotionAmount,  1 }, "Motion Amount",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 40.f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { kMotionMode,    1 }, "Motion Mode",
        juce::StringArray { "Filter", "Delay", "Reverb", "All" }, 3));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kMotionRate,    1 }, "Motion Rate",
        juce::NormalisableRange<float> (0.01f, 8.f, 0.01f, 0.4f), 0.25f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kMotionDepth,   1 }, "Motion Depth",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 75.f));

    // Space
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { kSpaceOn,      1 }, "Space On",     true));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kSpaceAmount,   1 }, "Space Amount",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 20.f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { kSpaceMode,     1 }, "Space Mode",
        juce::StringArray { "Studio", "Arena", "Dream", "Vintage" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kSpaceDecay,    1 }, "Space Decay",
        juce::NormalisableRange<float> (0.5f, 6.5f, 0.01f), 2.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kSpaceDamping,  1 }, "Space Damping",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 45.f));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { kSpacePredelay, 1 }, "Space Pre-Delay",
        juce::NormalisableRange<float> (0.f, 120.f, 0.1f), 22.f));

    return layout;
}

const juce::String NovaVoxAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaVoxAudioProcessor::acceptsMidi() const  { return false; }
bool NovaVoxAudioProcessor::producesMidi() const { return false; }
bool NovaVoxAudioProcessor::isMidiEffect() const { return false; }
double NovaVoxAudioProcessor::getTailLengthSeconds() const { return 2.0; }

int NovaVoxAudioProcessor::getNumPrograms()  { return 1; }
int NovaVoxAudioProcessor::getCurrentProgram() { return 0; }
void NovaVoxAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaVoxAudioProcessor::getProgramName (int) { return {}; }
void NovaVoxAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaVoxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    // Console: seed initial params from APVTS
    NovaConsoleParameters cp;
    cp.mode      = (int) apvts.getRawParameterValue (kConsoleMode)->load();
    cp.drive     = apvts.getRawParameterValue (kConsoleAmount)->load() * 0.7f;
    cp.preampOn  = apvts.getRawParameterValue (kConsoleOn)->load() > 0.5f;
    cp.analogOn  = true;
    cp.filterOn  = false;
    cp.eqOn      = false;
    cp.compOn    = false;
    cp.gateOn    = false;
    cp.mixAssist = 0.0f;
    cp.focusMode = false;
    consoleEngine.prepare (spec, cp);

    curveEngine.prepare (spec);
    levelEngine.prepare (spec);
    motionEngine.prepare (spec);
    spaceEngine.prepare (spec);
}

void NovaVoxAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == out;
}
#endif

void NovaVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0) return;

    auto load = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    if (load (kGlobalBypass) > 0.5f) return;

    const int N  = buffer.getNumSamples();
    const int ch = juce::jmin (2, buffer.getNumChannels());

    // Input gain
    const float inGain = juce::Decibels::decibelsToGain (load (kInputDb));
    for (int c = 0; c < ch; ++c)
        juce::FloatVectorOperations::multiply (buffer.getWritePointer (c), inGain, N);

    // Input peak meter
    float inPk = 0.f;
    for (int c = 0; c < ch; ++c)
    {
        const auto* d = buffer.getReadPointer (c);
        for (int i = 0; i < N; ++i) inPk = juce::jmax (inPk, std::abs (d[i]));
    }
    inputPeak.store (0.85f * inputPeak.load (std::memory_order_relaxed)
                     + 0.15f * juce::jlimit (0.f, 1.5f, inPk),
                     std::memory_order_relaxed);

    // ── Console ──────────────────────────────────────────────────────────────
    if (load (kConsoleOn) > 0.5f)
    {
        NovaConsoleParameters cp;
        const float consAmt = load (kConsoleAmount);
        cp.mode      = juce::jlimit (0, 4, (int) load (kConsoleMode));
        cp.preampOn  = true;
        cp.drive     = consAmt * 0.7f;          // 0..70 drive (0..100 APVTS → 0..70)
        cp.color     = 50.f;
        cp.trimDb    = 0.f;
        cp.filterOn  = false;
        cp.eqOn      = false;
        cp.compOn    = false;
        cp.gateOn    = false;
        cp.analogOn  = true;
        cp.heat      = consAmt * 0.3f;
        cp.depth     = 24.f;
        cp.width     = 52.f;
        cp.drift     = consAmt * 0.12f;
        cp.noise     = 0.f;
        cp.crosstalk = 0.f;
        cp.smartGain = false;
        cp.mixAssist = 0.f;
        cp.focusMode = false;
        cp.inputDb   = 0.f;
        cp.outputDb  = 0.f;
        cp.quality   = 1;
        cp.oversampling = 1;
        cp.sidechainMode = 0;
        consoleEngine.setParameters (cp);
        consoleEngine.process (buffer);
    }

    // Visualizer tone amount
    vizData.tone.store (load (kConsoleOn) > 0.5f ? load (kConsoleAmount) / 100.f : 0.f,
                        std::memory_order_relaxed);

    // ── Curve ────────────────────────────────────────────────────────────────
    if (load (kCurveOn) > 0.5f)
    {
        NovaCurveParameters curvP;
        curvP.curveOn = true;
        curvP.amount  = load (kCurveAmount);
        curvP.mode    = juce::jlimit (0, 3, (int) load (kCurveMode));
        curvP.lowDb   = load (kCurveLowDb);
        curvP.midDb   = load (kCurveMidDb);
        curvP.highDb  = load (kCurveHighDb);
        curveEngine.setParameters (curvP);
        curveEngine.process (buffer);
    }

    vizData.eq.store (load (kCurveOn) > 0.5f ? load (kCurveAmount) / 100.f : 0.f,
                      std::memory_order_relaxed);

    // ── Level ────────────────────────────────────────────────────────────────
    if (load (kLevelOn) > 0.5f)
    {
        NovaLevelParameters lp;
        lp.compression = load (kLevelAmount);
        lp.outputDb    = load (kLevelOutputDb);
        lp.mode        = juce::jlimit (0, 2, (int) load (kLevelMode));
        lp.magic       = load (kLevelMagic) > 0.5f ? 1.f : 0.f;
        levelEngine.setParameters (lp);
        levelEngine.process (buffer);
    }

    vizData.comp.store (load (kLevelOn) > 0.5f ? load (kLevelAmount) / 10.f : 0.f,
                        std::memory_order_relaxed);

    // ── Motion ───────────────────────────────────────────────────────────────
    if (load (kMotionOn) > 0.5f)
    {
        const float motAmt  = load (kMotionAmount) / 100.f;
        const int   motMode = juce::jlimit (0, 3, (int) load (kMotionMode));
        NovaMotionParameters mp;
        mp.motion    = motAmt;
        mp.drive     = 0.f;
        mp.lfoRate   = load (kMotionRate);
        mp.lfoDepth  = load (kMotionDepth) / 100.f;
        mp.inputDb   = 0.f;
        mp.outputDb  = 0.f;
        mp.mix       = 1.f;
        // Route per mode: Filter / Delay / Reverb / All
        mp.delayMix  = (motMode == 1 || motMode == 3) ? motAmt * 0.4f : 0.f;
        mp.reverbMix = (motMode == 2 || motMode == 3) ? motAmt * 0.3f : 0.f;
        const float cutoffBase = 20000.f * (1.f - motAmt * 0.6f);
        mp.cutoff    = (motMode == 0 || motMode == 3)
                       ? juce::jlimit (200.f, 20000.f, cutoffBase) : 20000.f;
        mp.resonance = 0.707f;
        mp.feedback  = 0.f;
        mp.size      = 0.6f;
        mp.decay     = 2.85f;
        motionEngine.setParameters (mp);
        motionEngine.process (buffer);
    }

    vizData.motion.store (load (kMotionOn) > 0.5f ? load (kMotionAmount) / 100.f : 0.f,
                          std::memory_order_relaxed);

    // ── Space ────────────────────────────────────────────────────────────────
    if (load (kSpaceOn) > 0.5f)
    {
        const float spAmt = load (kSpaceAmount);
        NovaSpaceParameters sp;
        sp.mix       = spAmt;
        sp.mode      = juce::jlimit (0, 3, (int) load (kSpaceMode));
        sp.space     = spAmt / 10.f;
        sp.air       = spAmt / 30.f;
        sp.depth     = spAmt / 40.f;
        sp.width     = spAmt / 30.f;
        sp.decay     = load (kSpaceDecay);
        sp.damping   = load (kSpaceDamping);
        sp.preDelayMs = load (kSpacePredelay);
        sp.early     = 35.f;
        spaceEngine.setParameters (sp);
        spaceEngine.process (buffer);
    }

    vizData.space.store (load (kSpaceOn) > 0.5f ? load (kSpaceAmount) / 100.f : 0.f,
                         std::memory_order_relaxed);

    // Output gain
    const float outGain = juce::Decibels::decibelsToGain (load (kOutputDb));
    for (int c = 0; c < ch; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < N; ++i)
        {
            const float s = d[i] * outGain;
            d[i] = std::isfinite (s) ? juce::jlimit (-1.35f, 1.35f, s) : 0.f;
        }
    }

    // Output peak meter + visualizer level
    float outPk = 0.f;
    for (int c = 0; c < ch; ++c)
    {
        const auto* d = buffer.getReadPointer (c);
        for (int i = 0; i < N; ++i) outPk = juce::jmax (outPk, std::abs (d[i]));
    }
    outputPeak.store (0.85f * outputPeak.load (std::memory_order_relaxed)
                      + 0.15f * juce::jlimit (0.f, 1.5f, outPk),
                      std::memory_order_relaxed);
    vizData.level.store (outPk, std::memory_order_relaxed);

    levelGainReduction.store   (levelEngine.getGainReductionDb(),   std::memory_order_relaxed);
    consoleGainReduction.store (consoleEngine.getGainReductionDb(), std::memory_order_relaxed);
}

bool NovaVoxAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NovaVoxAudioProcessor::createEditor()
{
    return new NovaVoxAudioProcessorEditor (*this);
}

void NovaVoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaVoxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaVoxAudioProcessor();
}
