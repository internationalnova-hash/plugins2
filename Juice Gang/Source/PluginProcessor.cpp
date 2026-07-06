#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

// Comb tunings (samples at 44100)
static const int kCombTunings[8]   = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int kAllpassTunings[4] = { 556,  441,  341,  225  };

JuiceGangProcessor::JuiceGangProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "JuiceGang", createParameterLayout())
{
}

JuiceGangProcessor::~JuiceGangProcessor() {}

// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout JuiceGangProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global bypass
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("bypass",       "Bypass",         false));

    // Filter
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("filterOn",     "Filter On",      true));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("filterMode",   "Filter Mode",    juce::StringArray{"Low Pass","High Pass","Band Pass","Notch"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("cutoff",       "Cutoff",         juce::NormalisableRange<float>(20.f, 20000.f, 0.f, 0.3f), 2000.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("resonance",    "Resonance",      juce::NormalisableRange<float>(0.f, 100.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("drive",        "Drive",          juce::NormalisableRange<float>(0.f, 100.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("filterMix",    "Filter Mix",     juce::NormalisableRange<float>(0.f, 100.f), 100.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("filterOut",    "Filter Output",  juce::NormalisableRange<float>(-24.f, 12.f), 0.f));

    // Delay
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("delayOn",      "Delay On",       false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delayTime",    "Delay Time",     juce::NormalisableRange<float>(0.01f, 2.f), 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("delaySync",    "Delay Sync",     false));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("delaySyncDiv", "Delay Div",      juce::StringArray{"1/4","1/8","1/16","1/32","Dotted","Triplet"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delayFeedback","Delay Feedback", juce::NormalisableRange<float>(0.f, 100.f), 30.f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("pingPong",     "Ping Pong",      false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delayLowCut",  "Delay Low Cut",  juce::NormalisableRange<float>(20.f, 2000.f, 0.f, 0.3f), 20.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delayHighCut", "Delay High Cut", juce::NormalisableRange<float>(1000.f, 20000.f, 0.f, 0.3f), 20000.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("delayMix",     "Delay Mix",      juce::NormalisableRange<float>(0.f, 100.f), 30.f));

    // Reverb
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("reverbOn",     "Reverb On",      false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverbSize",   "Reverb Size",    juce::NormalisableRange<float>(0.f, 100.f), 50.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverbDecay",  "Reverb Decay",   juce::NormalisableRange<float>(0.1f, 10.f, 0.f, 0.5f), 2.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("preDelay",     "Pre-Delay",      juce::NormalisableRange<float>(0.f, 100.f), 10.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverbDamp",   "Reverb Damp",    juce::NormalisableRange<float>(0.f, 100.f), 50.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("reverbMix",    "Reverb Mix",     juce::NormalisableRange<float>(0.f, 100.f), 20.f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("freeze",       "Freeze",         false));

    // Reverb EQ
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("reverbEqOn",   "Reverb EQ On",   true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("revLowCut",    "Rev Low Cut",    juce::NormalisableRange<float>(20.f, 1000.f, 0.f, 0.3f), 20.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("revLowShelf",  "Rev Low Shelf",  juce::NormalisableRange<float>(-12.f, 12.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("revMid",       "Rev Mid",        juce::NormalisableRange<float>(-12.f, 12.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("revHighShelf", "Rev High Shelf", juce::NormalisableRange<float>(-12.f, 12.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("revHighCut",   "Rev High Cut",   juce::NormalisableRange<float>(2000.f, 20000.f, 0.f, 0.3f), 20000.f));

    // LFO
    params.push_back (std::make_unique<juce::AudioParameterChoice>("lfoTarget",    "LFO Target",     juce::StringArray{"Cutoff","Delay Time","Reverb Mix"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("lfoShape",     "LFO Shape",      juce::StringArray{"Sine","Triangle","Square","Saw"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfoRate",      "LFO Rate",       juce::NormalisableRange<float>(0.01f, 20.f, 0.f, 0.4f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("lfoSync",      "LFO Sync",       false));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("lfoSyncDiv",   "LFO Sync Div",   juce::StringArray{"1/4","1/8","1/16","1/32","Dotted","Triplet"}, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("lfoDepth",     "LFO Depth",      juce::NormalisableRange<float>(0.f, 100.f), 0.f));

    // Master
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterIn",     "Master Input",   juce::NormalisableRange<float>(-24.f, 12.f), 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterMix",    "Master Mix",     juce::NormalisableRange<float>(0.f, 100.f), 100.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("masterOut",    "Master Output",  juce::NormalisableRange<float>(-24.f, 12.f), 0.f));

    // SIP macro — modulates cutoff, resonance, delay mix, reverb mix, drive simultaneously
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sip",          "SIP",            juce::NormalisableRange<float>(0.f, 100.f), 0.f));

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
void JuiceGangProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;

    for (auto& f : svf) f.reset();
    lfoPhase = 0.f;

    // Delay
    delayBufL.fill (0.f); delayBufR.fill (0.f);
    delayWrite = 0;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32)block, 1 };
    delayHpL.prepare (spec); delayHpR.prepare (spec);
    delayLpL.prepare (spec); delayLpR.prepare (spec);
    delayHpL.reset(); delayHpR.reset();
    delayLpL.reset(); delayLpR.reset();

    // Reverb combs + allpass
    double ratio = sr / 44100.0;
    for (int i = 0; i < kNumCombs; i++) {
        combL[i].prepare ((int)(kCombTunings[i] * ratio));
        combR[i].prepare ((int)(kCombTunings[i] * ratio + 23));
    }
    for (int i = 0; i < kNumAllpass; i++) {
        apL[i].prepare ((int)(kAllpassTunings[i] * ratio));
        apR[i].prepare ((int)(kAllpassTunings[i] * ratio + 23));
    }

    preDelayBufL.fill (0.f); preDelayBufR.fill (0.f);
    preDelayWrite = 0;

    juce::dsp::ProcessSpec specS { sr, (juce::uint32)block, 1 };
    revEqLowCutL.prepare(specS);   revEqLowCutR.prepare(specS);
    revEqLowShelfL.prepare(specS); revEqLowShelfR.prepare(specS);
    revEqMidL.prepare(specS);      revEqMidR.prepare(specS);
    revEqHighShelfL.prepare(specS);revEqHighShelfR.prepare(specS);
    revEqHighCutL.prepare(specS);  revEqHighCutR.prepare(specS);
    updateReverbEQ();

    // Smoothers
    smoothCutoff.reset    (sr, 0.02); smoothCutoff.setCurrentAndTargetValue (2000.f);
    smoothRes.reset       (sr, 0.02); smoothRes.setCurrentAndTargetValue (0.f);
    smoothDrive.reset     (sr, 0.02); smoothDrive.setCurrentAndTargetValue (0.f);
    smoothFilterMix.reset (sr, 0.02); smoothFilterMix.setCurrentAndTargetValue (1.f);
    smoothDelayFeedback.reset (sr, 0.05); smoothDelayFeedback.setCurrentAndTargetValue (0.3f);
    smoothDelayMix.reset      (sr, 0.02); smoothDelayMix.setCurrentAndTargetValue (0.3f);
    smoothReverbSize.reset    (sr, 0.1);  smoothReverbSize.setCurrentAndTargetValue (0.5f);
    smoothReverbDecay.reset   (sr, 0.1);  smoothReverbDecay.setCurrentAndTargetValue (2.5f);
    smoothReverbMix.reset     (sr, 0.02); smoothReverbMix.setCurrentAndTargetValue (0.2f);
    smoothReverbDamp.reset    (sr, 0.05); smoothReverbDamp.setCurrentAndTargetValue (0.5f);
    smoothMasterMix.reset     (sr, 0.02); smoothMasterMix.setCurrentAndTargetValue (1.f);
    smoothMasterOut.reset     (sr, 0.02); smoothMasterOut.setCurrentAndTargetValue (1.f);
    smoothMasterIn.reset      (sr, 0.02); smoothMasterIn.setCurrentAndTargetValue (1.f);

    // Decode voice tag on first prepare
    if (!tagDecoded)
        decodeVoiceTag (sr);
}

void JuiceGangProcessor::decodeVoiceTag (double sr)
{
    tagDecoded = true;
    tagPlayPos = 0;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

#if JUCE_MAC || JUCE_IOS
    formatManager.registerFormat (new juce::CoreAudioFormat(), false);
#endif

    juce::MemoryInputStream mis (BinaryData::JuiceTag_m4a,
                                  BinaryData::JuiceTag_m4aSize, false);
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (
        std::make_unique<juce::MemoryInputStream> (BinaryData::JuiceTag_m4a,
                                                    BinaryData::JuiceTag_m4aSize, false)));
    if (reader == nullptr) { tagPlayPos = -1; return; }

    int numSamples = (int)reader->lengthInSamples;
    int numCh      = (int)reader->numChannels;
    tagBuffer.setSize (2, numSamples, false, true, false);

    juce::AudioBuffer<float> tmp (numCh, numSamples);
    reader->read (&tmp, 0, numSamples, 0, true, true);

    // Copy to stereo and normalise to -12 dBFS so it's not too loud
    const float gain = juce::Decibels::decibelsToGain (-12.f);
    for (int s = 0; s < numSamples; ++s)
    {
        tagBuffer.setSample (0, s, tmp.getSample (0, s) * gain);
        tagBuffer.setSample (1, s, tmp.getSample (numCh > 1 ? 1 : 0, s) * gain);
    }

    // Resample if needed
    if (reader->sampleRate != sr && reader->sampleRate > 0)
    {
        double ratio = sr / reader->sampleRate;
        int newLen = (int)(numSamples * ratio);
        juce::AudioBuffer<float> resampled (2, newLen);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < newLen; ++s)
            {
                double src = s / ratio;
                int i0 = juce::jlimit (0, numSamples - 1, (int)src);
                int i1 = juce::jlimit (0, numSamples - 1, i0 + 1);
                float frac = (float)(src - i0);
                resampled.setSample (ch, s, tagBuffer.getSample (ch, i0) * (1.f - frac)
                                           + tagBuffer.getSample (ch, i1) * frac);
            }
        tagBuffer = resampled;
    }
}

void JuiceGangProcessor::releaseResources() {}

bool JuiceGangProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

// ─────────────────────────────────────────────────────────────────────────────
float JuiceGangProcessor::getLFOValue (int shape)
{
    switch (shape) {
        case 1: return 1.f - 4.f * std::abs (lfoPhase - 0.5f); // triangle
        case 2: return lfoPhase < 0.5f ? 1.f : -1.f;           // square
        case 3: return 2.f * lfoPhase - 1.f;                    // saw
        default: return std::sin (2.f * juce::MathConstants<float>::pi * lfoPhase); // sine
    }
}

float JuiceGangProcessor::tempoSyncedSeconds (int divIndex, double bpm) const
{
    double beat = 60.0 / bpm;
    switch (divIndex) {
        case 1: return (float)(beat * 0.5);    // 1/8
        case 2: return (float)(beat * 0.25);   // 1/16
        case 3: return (float)(beat * 0.125);  // 1/32
        case 4: return (float)(beat * 1.5);    // dotted 1/4
        case 5: return (float)(beat * 2.0/3.0);// triplet
        default: return (float)beat;           // 1/4
    }
}

void JuiceGangProcessor::updateReverbEQ()
{
    float lowCut   = *apvts.getRawParameterValue ("revLowCut");
    float lowShelf = *apvts.getRawParameterValue ("revLowShelf");
    float mid      = *apvts.getRawParameterValue ("revMid");
    float highShelf= *apvts.getRawParameterValue ("revHighShelf");
    float highCut  = *apvts.getRawParameterValue ("revHighCut");

    auto makeHP  = [&](float f) { return juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f); };
    auto makeLP  = [&](float f) { return juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, f); };
    auto makeLSh = [&](float g) { return juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 200.f, 0.707f, juce::Decibels::decibelsToGain(g)); };
    auto makeHSh = [&](float g) { return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 4000.f, 0.707f, juce::Decibels::decibelsToGain(g)); };
    auto makePeak= [&](float g) { return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1000.f, 1.0f, juce::Decibels::decibelsToGain(g)); };

    *revEqLowCutL.coefficients   = *makeHP(lowCut);   *revEqLowCutR.coefficients   = *makeHP(lowCut);
    *revEqLowShelfL.coefficients = *makeLSh(lowShelf);*revEqLowShelfR.coefficients = *makeLSh(lowShelf);
    *revEqMidL.coefficients      = *makePeak(mid);    *revEqMidR.coefficients      = *makePeak(mid);
    *revEqHighShelfL.coefficients= *makeHSh(highShelf);*revEqHighShelfR.coefficients=*makeHSh(highShelf);
    *revEqHighCutL.coefficients  = *makeLP(highCut);  *revEqHighCutR.coefficients  = *makeLP(highCut);
}

// ─────────────────────────────────────────────────────────────────────────────
void JuiceGangProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (*apvts.getRawParameterValue ("bypass") > 0.5f) return;

    const int numSamples  = buffer.getNumSamples();
    float* dataL = buffer.getWritePointer (0);
    float* dataR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : dataL;

    // ── Target param values ────────────────────────────────────────────────
    bool  filterOn   = *apvts.getRawParameterValue ("filterOn")  > 0.5f;
    int   filterMode = (int)*apvts.getRawParameterValue ("filterMode");
    float cutoffTgt  = *apvts.getRawParameterValue ("cutoff");
    float resTgt     = *apvts.getRawParameterValue ("resonance") / 100.f;
    float driveTgt   = *apvts.getRawParameterValue ("drive")     / 100.f;
    float fMixTgt    = *apvts.getRawParameterValue ("filterMix") / 100.f;
    float filterOutGain = juce::Decibels::decibelsToGain ((float)*apvts.getRawParameterValue ("filterOut"));

    bool  delayOn    = *apvts.getRawParameterValue ("delayOn")  > 0.5f;
    bool  delaySync  = *apvts.getRawParameterValue ("delaySync") > 0.5f;
    float delayTimeSecs = *apvts.getRawParameterValue ("delayTime");
    bool  pingPong   = *apvts.getRawParameterValue ("pingPong") > 0.5f;
    float delayLCHz  = *apvts.getRawParameterValue ("delayLowCut");
    float delayHCHz  = *apvts.getRawParameterValue ("delayHighCut");
    float delayFbTgt = *apvts.getRawParameterValue ("delayFeedback") / 100.f;
    float delayMixTgt= *apvts.getRawParameterValue ("delayMix")      / 100.f;

    bool  reverbOn   = *apvts.getRawParameterValue ("reverbOn")  > 0.5f;
    float revSizeTgt = *apvts.getRawParameterValue ("reverbSize")  / 100.f;
    float revDecayTgt= *apvts.getRawParameterValue ("reverbDecay");
    float preDelayMs = *apvts.getRawParameterValue ("preDelay");
    float revDampTgt = *apvts.getRawParameterValue ("reverbDamp")  / 100.f;
    float revMixTgt  = *apvts.getRawParameterValue ("reverbMix")   / 100.f;
    bool  freeze     = *apvts.getRawParameterValue ("freeze") > 0.5f;
    bool  reverbEqOn = *apvts.getRawParameterValue ("reverbEqOn") > 0.5f;

    int   lfoTarget  = (int)*apvts.getRawParameterValue ("lfoTarget");
    int   lfoShape   = (int)*apvts.getRawParameterValue ("lfoShape");
    float lfoRate    = *apvts.getRawParameterValue ("lfoRate");
    bool  lfoSync    = *apvts.getRawParameterValue ("lfoSync") > 0.5f;
    int   lfoDiv     = (int)*apvts.getRawParameterValue ("lfoSyncDiv");
    float lfoDepth   = *apvts.getRawParameterValue ("lfoDepth") / 100.f;

    float masterInGain  = juce::Decibels::decibelsToGain ((float)*apvts.getRawParameterValue ("masterIn"));
    float masterMixTgt  = *apvts.getRawParameterValue ("masterMix") / 100.f;
    float masterOutGain = juce::Decibels::decibelsToGain ((float)*apvts.getRawParameterValue ("masterOut"));

    // ── SIP macro modulation ───────────────────────────────────────────────
    {
        float sip = *apvts.getRawParameterValue ("sip") / 100.f;  // 0..1
        if (sip > 0.001f)
        {
            // Sweep cutoff upward (20Hz → up to 16kHz at full sip)
            cutoffTgt  = juce::jlimit (20.f, 20000.f, cutoffTgt  + sip * 14000.f);
            // Add resonance character
            resTgt     = juce::jlimit (0.f, 0.95f,   resTgt     + sip * 0.55f);
            // Blend in delay and reverb
            delayMixTgt= juce::jlimit (0.f, 1.f,     delayMixTgt+ sip * 0.55f);
            revMixTgt  = juce::jlimit (0.f, 1.f,     revMixTgt  + sip * 0.65f);
            // Drive for saturation character
            driveTgt   = juce::jlimit (0.f, 1.f,     driveTgt   + sip * 0.45f);
            // Turn on FX if SIP is up
            if (sip > 0.1f) { delayOn = true; reverbOn = true; filterOn = true; }
        }
    }

    // Tempo sync
    auto playHead = getPlayHead();
    double bpm = 120.0;
    if (playHead) {
        if (auto pos = playHead->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    }
    if (delaySync) delayTimeSecs = tempoSyncedSeconds ((int)*apvts.getRawParameterValue ("delaySyncDiv"), bpm);
    if (lfoSync)   lfoRate = 1.f / tempoSyncedSeconds (lfoDiv, bpm);

    // Update smoother targets
    smoothCutoff.setTargetValue (cutoffTgt);
    smoothRes.setTargetValue (resTgt);
    smoothDrive.setTargetValue (driveTgt);
    smoothFilterMix.setTargetValue (fMixTgt);
    smoothDelayFeedback.setTargetValue (delayFbTgt);
    smoothDelayMix.setTargetValue (delayMixTgt);
    smoothReverbSize.setTargetValue (revSizeTgt);
    smoothReverbDecay.setTargetValue (revDecayTgt);
    smoothReverbMix.setTargetValue (revMixTgt);
    smoothReverbDamp.setTargetValue (revDampTgt);
    smoothMasterMix.setTargetValue (masterMixTgt);
    smoothMasterOut.setTargetValue (masterOutGain);
    smoothMasterIn.setTargetValue (masterInGain);

    // Update delay filters
    *delayHpL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, delayLCHz);
    *delayHpR.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, delayLCHz);
    *delayLpL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, delayHCHz);
    *delayLpR.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, delayHCHz);

    updateReverbEQ();

    // Comb feedback from decay
    float combFb = std::exp (-0.001f / (smoothReverbDecay.getNextValue() / 60.f));

    int preDelaySamples = (int)(preDelayMs * 0.001f * (float)sampleRate);
    preDelaySamples = juce::jlimit (0, kPreDelayMax - 1, preDelaySamples);

    // ── Per-sample loop ────────────────────────────────────────────────────
    float peakIn = 0.f, peakOut = 0.f;

    for (int n = 0; n < numSamples; n++)
    {
        // LFO
        float lfoVal = getLFOValue (lfoShape) * lfoDepth;
        float lfoInc = lfoRate / (float)sampleRate;
        lfoPhase = std::fmod (lfoPhase + lfoInc, 1.f);

        float inL = dataL[n] * smoothMasterIn.getNextValue();
        float inR = dataR[n] * smoothMasterIn.getNextValue();
        peakIn = std::max (peakIn, std::abs (inL) + std::abs (inR));

        float cutoff = smoothCutoff.getNextValue();
        if (lfoTarget == 0) cutoff = juce::jlimit (20.f, 20000.f, cutoff * (1.f + lfoVal * 0.8f));

        // ── Filter ──────────────────────────────────────────────────────
        float filtL = inL, filtR = inR;
        if (filterOn)
        {
            float drive  = smoothDrive.getNextValue();
            float res    = smoothRes.getNextValue();
            float fMix   = smoothFilterMix.getNextValue();

            // Drive saturation
            float driveGain = 1.f + drive * 4.f;
            float preL = std::tanh (inL * driveGain) / driveGain;
            float preR = std::tanh (inR * driveGain) / driveGain;

            // SVF coefficients
            float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float)sampleRate);
            float R = 1.f - res * 0.97f; // keep away from full self-oscillation

            float svfOutL = svf[0].process (preL, g, R, filterMode);
            float svfOutR = svf[1].process (preR, g, R, filterMode);

            filtL = inL * (1.f - fMix) + svfOutL * fMix * filterOutGain;
            filtR = inR * (1.f - fMix) + svfOutR * fMix * filterOutGain;
        }

        // ── Delay ───────────────────────────────────────────────────────
        float wetL = filtL, wetR = filtR;

        if (delayOn)
        {
            float fb   = smoothDelayFeedback.getNextValue();
            float dMix = smoothDelayMix.getNextValue();

            float delayTimeMod = delayTimeSecs;
            if (lfoTarget == 1) delayTimeMod = juce::jlimit (0.01f, 2.f, delayTimeSecs * (1.f + lfoVal * 0.3f));

            int delaySamp = juce::jlimit (1, kDelayMaxSamples - 1, (int)(delayTimeMod * (float)sampleRate));

            int readL = (delayWrite - delaySamp + kDelayMaxSamples) % kDelayMaxSamples;
            int readR = pingPong ? (delayWrite - delaySamp / 2 + kDelayMaxSamples) % kDelayMaxSamples : readL;

            float dL = delayBufL[readL];
            float dR = delayBufR[readR];

            // Feedback filter
            dL = delayHpL.processSample (dL); dL = delayLpL.processSample (dL);
            dR = delayHpR.processSample (dR); dR = delayLpR.processSample (dR);

            delayBufL[delayWrite] = filtL + dR * fb; // ping-pong crosses
            delayBufR[delayWrite] = filtR + dL * fb;

            wetL = filtL * (1.f - dMix) + dL * dMix;
            wetR = filtR * (1.f - dMix) + dR * dMix;
        }

        // ── Reverb ──────────────────────────────────────────────────────
        float revOutL = 0.f, revOutR = 0.f;
        if (reverbOn)
        {
            float revMix  = smoothReverbMix.getNextValue();
            float size    = smoothReverbSize.getNextValue();
            float damp    = smoothReverbDamp.getNextValue();

            // Update comb params
            float fb = freeze ? 1.f : juce::jlimit (0.f, 0.98f, combFb * (0.5f + size * 0.5f));
            for (auto& c : combL) { c.feedback = fb; c.damp = damp; }
            for (auto& c : combR) { c.feedback = fb; c.damp = damp; }

            // Pre-delay
            preDelayBufL[preDelayWrite] = wetL;
            preDelayBufR[preDelayWrite] = wetR;
            int pdRead = (preDelayWrite - preDelaySamples + kPreDelayMax) % kPreDelayMax;
            float pdL = preDelayBufL[pdRead];
            float pdR = preDelayBufR[pdRead];
            if (++preDelayWrite >= kPreDelayMax) preDelayWrite = 0;

            // Combs in parallel
            float combOutL = 0.f, combOutR = 0.f;
            for (auto& c : combL) combOutL += c.process (pdL);
            for (auto& c : combR) combOutR += c.process (pdR);
            combOutL /= kNumCombs; combOutR /= kNumCombs;

            // Allpass cascade
            for (auto& a : apL) combOutL = a.process (combOutL);
            for (auto& a : apR) combOutR = a.process (combOutR);

            // Reverb EQ
            if (reverbEqOn) {
                combOutL = revEqLowCutL.processSample   (combOutL);
                combOutL = revEqLowShelfL.processSample (combOutL);
                combOutL = revEqMidL.processSample      (combOutL);
                combOutL = revEqHighShelfL.processSample(combOutL);
                combOutL = revEqHighCutL.processSample  (combOutL);

                combOutR = revEqLowCutR.processSample   (combOutR);
                combOutR = revEqLowShelfR.processSample (combOutR);
                combOutR = revEqMidR.processSample      (combOutR);
                combOutR = revEqHighShelfR.processSample(combOutR);
                combOutR = revEqHighCutR.processSample  (combOutR);
            }

            float revMixMod = revMix;
            if (lfoTarget == 2) revMixMod = juce::jlimit (0.f, 1.f, revMix + lfoVal * 0.3f);

            revOutL = wetL * (1.f - revMixMod) + combOutL * revMixMod;
            revOutR = wetR * (1.f - revMixMod) + combOutR * revMixMod;
        }
        else
        {
            revOutL = wetL;
            revOutR = wetR;
        }

        if (delayOn) {
            if (++delayWrite >= kDelayMaxSamples) delayWrite = 0;
        }

        // ── Master ──────────────────────────────────────────────────────
        float masterMix = smoothMasterMix.getNextValue();
        float outL = inL * (1.f - masterMix) + revOutL * masterMix;
        float outR = inR * (1.f - masterMix) + revOutR * masterMix;

        float outGain = smoothMasterOut.getNextValue();
        outL *= outGain;
        outR *= outGain;

        // Safety limiter
        outL = juce::jlimit (-1.f, 1.f, outL);
        outR = juce::jlimit (-1.f, 1.f, outR);

        peakOut = std::max (peakOut, std::abs (outL) + std::abs (outR));
        dataL[n] = outL;
        dataR[n] = outR;
    }

    inputLevel.store  (peakIn  * 0.5f);
    outputLevel.store (peakOut * 0.5f);

    // ── Voice tag playback — mix into output on first load ─────────────────
    if (tagPlayPos >= 0 && tagPlayPos < tagBuffer.getNumSamples())
    {
        int tagRemain = tagBuffer.getNumSamples() - tagPlayPos;
        int tagN      = juce::jmin (numSamples, tagRemain);
        for (int n = 0; n < tagN; ++n)
        {
            dataL[n] += tagBuffer.getSample (0, tagPlayPos + n);
            dataR[n] += tagBuffer.getSample (1, tagPlayPos + n);
        }
        tagPlayPos += tagN;
        if (tagPlayPos >= tagBuffer.getNumSamples())
            tagPlayPos = -1;  // done — won't play again this session
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void JuiceGangProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void JuiceGangProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* JuiceGangProcessor::createEditor()
{
    return new JuiceGangEditor (*this);
}

bool JuiceGangProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuiceGangProcessor();
}
