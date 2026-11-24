#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::vector<FlipStyle>& flipStyles()
{
    static const std::vector<FlipStyle> styles {
        //  name              mode sens grid bars  var swing stut  fx
        { "Boom Bap Flip",    0,   60,  1,   1,    55,  30,   10,  FlipStyle::none },
        { "Soul Chop",        2,   50,  1,   2,    35,  20,   5,   FlipStyle::none },
        { "Classic Order",    0,   50,  1,   1,    10,  15,   0,   FlipStyle::none },
        { "Drill Chop",       1,   60,  1,   1,    70,  0,    45,  FlipStyle::none },
        { "Trap Bounce",      1,   60,  2,   1,    60,  10,   35,  FlipStyle::none },
        { "Lo-Fi Flip",       0,   55,  1,   1,    45,  45,   5,   FlipStyle::lofi },
        { "Reverse Dreams",   2,   60,  1,   2,    40,  15,   0,   FlipStyle::reverseAlternate },
        { "Slowed Flip",      2,   40,  1,   2,    30,  25,   0,   FlipStyle::slowed },
        { "Stutter Edit",     0,   75,  1,   0,    80,  0,    80,  FlipStyle::none },
        { "Chaos Lab",        0,   100, 1,   1,    100, 20,   60,  FlipStyle::none },
    };
    return styles;
}

const juce::StringArray& loopLabParamIds()
{
    static const juce::StringArray ids { "chop_mode", "sensitivity", "grid", "bars", "variation", "swing",
                                         "stutter", "pattern", "auto_play", "output" };
    return ids;
}

NovaLoopLabAudioProcessor::NovaLoopLabAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NovaLoopLab", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    syncSpecFromParams (true);
    startTimerHz (10);
}

NovaLoopLabAudioProcessor::~NovaLoopLabAudioProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaLoopLabAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chop_mode", 1 }, "Chop Mode", StringArray { "Transients", "Grid", "Musical" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "sensitivity", 1 }, "Sensitivity", NormalisableRange<float> (0, 100, 1), 60.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "grid", 1 }, "Grid", StringArray { "1/4", "1/8", "1/16" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Loop Length", StringArray { "1 Bar", "2 Bars", "4 Bars" }, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "variation", 1 }, "Variation", NormalisableRange<float> (0, 100, 1), 55.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (0, 100, 1), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "stutter", 1 }, "Stutter", NormalisableRange<float> (0, 100, 1), 15.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "pattern", 1 }, "Pattern", 1, ll::kNumVariations, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "auto_play", 1 }, "Play With Host", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

bool NovaLoopLabAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NovaLoopLabAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
    for (auto& v : voices) v = {};
    freePos = 0.0;
    loopGain = 0.0f;
}

// ── Params / tempo -> worker (message thread) ──────────────────────────────
void NovaLoopLabAudioProcessor::timerCallback()
{
    const int style = pendingStyleFx.exchange (-1);
    if (style >= 0)
        applyStyleFx (style);
    syncSpecFromParams (false);
}

void NovaLoopLabAudioProcessor::syncSpecFromParams (bool force)
{
    auto& ap = apvts;
    Pushed now;
    now.mode = (int) ap.getRawParameterValue ("chop_mode")->load();
    now.grid = (int) ap.getRawParameterValue ("grid")->load();
    now.bars = (int) ap.getRawParameterValue ("bars")->load();
    now.sens = ap.getRawParameterValue ("sensitivity")->load();
    now.variation = ap.getRawParameterValue ("variation")->load();
    now.stutter = ap.getRawParameterValue ("stutter")->load();
    now.swing = ap.getRawParameterValue ("swing")->load();
    const float hb = hostBpm.load();
    now.bpm = hb > 20.0f ? std::round (hb * 100.0) / 100.0 : 0.0; // 0 = no host tempo: follow the sample
    now.sr = currentSampleRate;

    const bool changed = force || now.mode != pushed.mode || now.grid != pushed.grid || now.bars != pushed.bars
                         || now.sens != pushed.sens || now.variation != pushed.variation || now.stutter != pushed.stutter
                         || now.swing != pushed.swing || std::abs (now.bpm - pushed.bpm) > 0.005 || now.sr != pushed.sr;
    if (! changed)
        return;

    pushed = now;
    core.updateSpec ([now] (ll::Spec& s)
    {
        s.chop.mode = static_cast<ll::ChopMode> (juce::jlimit (0, 2, now.mode));
        s.chop.sensitivity = now.sens / 100.0f;
        s.chop.gridDiv = now.grid == 0 ? 4 : now.grid == 2 ? 16 : 8;
        s.gen.bars = now.bars == 0 ? 1 : now.bars == 2 ? 4 : 2;
        s.gen.variation = now.variation / 100.0f;
        s.gen.stutter = now.stutter / 100.0f;
        s.swing = now.swing / 100.0f;
        s.hostBpm = now.bpm;
        s.sampleRate = now.sr;
    });
}

// ── Programs (flip styles) ─────────────────────────────────────────────────
int NovaLoopLabAudioProcessor::getNumPrograms() { return (int) flipStyles().size(); }

const juce::String NovaLoopLabAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) flipStyles().size()) ? juce::String (flipStyles()[(size_t) index].name) : juce::String();
}

void NovaLoopLabAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) flipStyles().size()))
        return;
    currentProgram = index;
    const auto& st = flipStyles()[(size_t) index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set ("chop_mode", (float) st.mode);
    set ("sensitivity", st.sensitivity);
    set ("grid", (float) st.grid);
    set ("bars", (float) st.bars);
    set ("variation", st.variation);
    set ("swing", st.swing);
    set ("stutter", st.stutter);
    pendingStyleFx.store (index); // per-chop effects are applied on the message thread
}

void NovaLoopLabAudioProcessor::applyStyleFx (int styleIndex)
{
    if (! juce::isPositiveAndBelow (styleIndex, (int) flipStyles().size()))
        return;
    const auto kind = flipStyles()[(size_t) styleIndex].fx;
    core.updateSpec ([kind] (ll::Spec& s)
    {
        s.fx.assign ((size_t) ll::kMaxChops, ll::ChopFx {});
        for (size_t i = 0; i < s.fx.size(); ++i)
        {
            auto& f = s.fx[i];
            switch (kind)
            {
                case FlipStyle::lofi:             f.tone = 45; f.filter = -30; break;
                case FlipStyle::reverseAlternate: f.reverse = i % 2 == 1; break;
                case FlipStyle::slowed:           f.pitch = -3; break;
                case FlipStyle::none:             break;
            }
        }
    });
}

// ── Audio ──────────────────────────────────────────────────────────────────
void NovaLoopLabAudioProcessor::startVoice (int chop, float velocity)
{
    // Choke the same chop (MPC style), then take a free voice or the oldest.
    for (auto& v : voices)
        if (v.chop == chop && v.fade == 0) v.fade = 64;

    Voice* target = nullptr;
    for (auto& v : voices)
        if (v.chop < 0) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.pos > target->pos) target = &v;
    }
    *target = { chop, 0, velocity, 0 };
}

void NovaLoopLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    buffer.clear();

    const auto* model = core.getAudioModel();
    if (model != lastModel)
    {
        // A new render arrived: crossfade from the old one (kept alive by the
        // core for at least a second) so edits never click.
        xfadeModel = lastModel;
        modelXfade = xfadeModel != nullptr ? 512 : 0;
        lastModel = model;
    }

    bool playing = false;
    double ppq = 0.0, bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            ppq = pos->getPpqPosition().orFallback (0.0);
            bpm = pos->getBpm().orFallback (0.0);
        }
    if (bpm > 20.0) hostBpm.store ((float) bpm);
    hostPlaying.store (playing);

    if (bypassParam != nullptr && bypassParam->get())
    {
        playhead.store (-1.0f);
        return;
    }

    const int ui = auditionChop.exchange (-1);
    if (ui >= 0) startVoice (ui, 0.9f);

    const bool autoPlay = apvts.getRawParameterValue ("auto_play")->load() > 0.5f;
    const int paramPattern = juce::jlimit (0, ll::kNumVariations - 1, (int) apvts.getRawParameterValue ("pattern")->load() - 1);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));

    const bool hasLoop = model != nullptr && model->hasSample();
    const double loopBeats = model != nullptr ? model->bars * 4.0 : 8.0;
    const double effBpm = bpm > 20.0 ? bpm : (model != nullptr ? model->bpm : 120.0);
    const double beatsPerSample = effBpm / 60.0 / currentSampleRate;
    const float gainStep = 1.0f / (0.005f * (float) currentSampleRate);

    auto midiIt = midi.cbegin();
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float lastFrac = -1.0f;

    auto sampleAt = [] (const juce::AudioBuffer<float>& b, int ch, double idx)
    {
        const int n = b.getNumSamples();
        if (n < 2) return 0.0f;
        const int i0 = (int) idx % n, i1 = (i0 + 1) % n;
        const float fr = (float) (idx - std::floor (idx));
        const auto* d = b.getReadPointer (std::min (ch, b.getNumChannels() - 1));
        return d[i0] + (d[i1] - d[i0]) * fr;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        // MIDI at its sample position.
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            const int note = msg.getNoteNumber();
            if (msg.isNoteOn())
            {
                if (note >= 36 && note < 36 + ll::kMaxChops) startVoice (note - 36, msg.getFloatVelocity());
                else if (note >= 60 && note < 60 + ll::kNumVariations) { midiPattern = note - 60; ++gateNotes; }
            }
            else if (msg.isNoteOff() && note >= 60 && note < 60 + ll::kNumVariations)
                gateNotes = std::max (0, gateNotes - 1);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                gateNotes = 0;
                for (auto& v : voices) if (v.chop >= 0) v.fade = 64;
            }
            ++midiIt;
        }

        const int pattern = midiPattern >= 0 ? midiPattern : paramPattern;
        if (pattern != curPattern)
        {
            prevPattern = curPattern;
            curPattern = pattern;
            patternXfade = 256;
        }

        const bool loopOn = hasLoop && ((playing && (autoPlay || gateNotes > 0)) || (! playing && auditionLoop.load()));
        loopGain = loopOn ? std::min (1.0f, loopGain + gainStep) : std::max (0.0f, loopGain - gainStep);

        float l = 0.0f, r = 0.0f;
        if (loopGain > 0.0f && hasLoop)
        {
            double frac;
            if (playing)
            {
                const double beats = ppq + i * beatsPerSample;
                frac = std::fmod (std::fmod (beats, loopBeats) + loopBeats, loopBeats) / loopBeats;
            }
            else
            {
                const double len = loopBeats / beatsPerSample;
                freePos = std::fmod (freePos + 1.0, len);
                frac = freePos / len;
            }
            lastFrac = (float) frac;

            auto readModel = [&] (const ll::Model& m, int pat, float& ol, float& orr)
            {
                const auto& b = m.loops[(size_t) pat];
                const double idx = frac * b.getNumSamples();
                ol = sampleAt (b, 0, idx);
                orr = sampleAt (b, 1, idx);
            };

            readModel (*model, curPattern, l, r);
            if (patternXfade > 0)
            {
                float pl, pr;
                readModel (*model, prevPattern, pl, pr);
                const float t = patternXfade / 256.0f;
                l = l * (1.0f - t) + pl * t; r = r * (1.0f - t) + pr * t;
                --patternXfade;
            }
            if (modelXfade > 0 && xfadeModel != nullptr && xfadeModel->hasSample() && xfadeModel->bars == model->bars)
            {
                float ml, mr;
                readModel (*xfadeModel, curPattern, ml, mr);
                const float t = modelXfade / 512.0f;
                l = l * (1.0f - t) + ml * t; r = r * (1.0f - t) + mr * t;
            }
            l *= loopGain; r *= loopGain;
        }
        if (modelXfade > 0 && --modelXfade == 0) xfadeModel = nullptr;
        if (! playing && ! auditionLoop.load()) freePos = 0.0;

        // One-shot chop voices (pads, MIDI, UI audition).
        if (model != nullptr)
            for (auto& v : voices)
            {
                if (v.chop < 0) continue;
                if (v.chop >= (int) model->renderedChops.size()) { v.chop = -1; continue; }
                const auto& b = model->renderedChops[(size_t) v.chop];
                if (v.pos >= b.getNumSamples()) { v.chop = -1; continue; }
                float g = v.gain;
                if (v.fade > 0) { g *= v.fade / 64.0f; if (--v.fade == 0) { v.chop = -1; continue; } }
                l += b.getSample (0, v.pos) * g;
                r += b.getSample (std::min (1, b.getNumChannels() - 1), v.pos) * g;
                ++v.pos;
            }

        const float og = outGain.getNextValue();
        l *= og; r *= og;
        l = std::isfinite (l) ? juce::jlimit (-1.0f, 1.0f, l) : 0.0f;
        r = std::isfinite (r) ? juce::jlimit (-1.0f, 1.0f, r) : 0.0f;
        L[i] = R != nullptr ? l : 0.5f * (l + r);
        if (R != nullptr) R[i] = r;
    }

    playhead.store (loopGain > 0.0f ? lastFrac : -1.0f);
    playingPattern.store (curPattern);

    auto peak = [] (const float* d, int n) { float p = 0; for (int i = 0; i < n; ++i) p = std::max (p, std::abs (d[i])); return p; };
    const float pl = peak (L, numSamples), pr = R != nullptr ? peak (R, numSamples) : pl;
    if (pl > outL.load()) outL.store (pl);
    if (pr > outR.load()) outR.store (pr);
}

juce::AudioProcessorEditor* NovaLoopLabAudioProcessor::createEditor()
{
    return new NovaLoopLabAudioProcessorEditor (*this);
}

// ── State ──────────────────────────────────────────────────────────────────
namespace
{
    juce::String fxToString (const std::vector<ll::ChopFx>& fx)
    {
        juce::StringArray parts;
        for (const auto& f : fx)
            parts.add (juce::String (f.pitch) + "," + juce::String (f.stretch) + "," + juce::String (f.filter) + ","
                       + juce::String (f.tone) + "," + (f.reverse ? "1" : "0") + "," + (f.use ? "1" : "0"));
        return parts.joinIntoString (";");
    }

    std::vector<ll::ChopFx> fxFromString (const juce::String& s)
    {
        std::vector<ll::ChopFx> out;
        for (const auto& part : juce::StringArray::fromTokens (s, ";", ""))
        {
            const auto v = juce::StringArray::fromTokens (part, ",", "");
            if (v.size() < 6) continue;
            ll::ChopFx f;
            f.pitch = juce::jlimit (-12, 12, v[0].getIntValue());
            f.stretch = juce::jlimit (50, 200, v[1].getIntValue());
            f.filter = juce::jlimit (-100, 100, v[2].getIntValue());
            f.tone = juce::jlimit (0, 100, v[3].getIntValue());
            f.reverse = v[4] == "1";
            f.use = v[5] == "1";
            out.push_back (f);
        }
        out.resize ((size_t) ll::kMaxChops);
        return out;
    }

    juce::String stepsToString (const std::vector<ll::Step>& steps)
    {
        juce::StringArray parts;
        for (const auto& s : steps)
            parts.add (juce::String (s.t) + "," + juce::String (s.len) + "," + juce::String (s.chop) + "," + (s.stutter ? "1" : "0"));
        return parts.joinIntoString (";");
    }

    std::vector<ll::Step> stepsFromString (const juce::String& str)
    {
        std::vector<ll::Step> out;
        for (const auto& part : juce::StringArray::fromTokens (str, ";", ""))
        {
            const auto v = juce::StringArray::fromTokens (part, ",", "");
            if (v.size() < 4) continue;
            out.push_back ({ v[0].getIntValue(), std::max (1, v[1].getIntValue()), juce::jlimit (0, ll::kMaxChops - 1, v[2].getIntValue()), v[3] == "1" });
        }
        return out;
    }
}

void NovaLoopLabAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);

    const auto s = core.getSpec();
    juce::ValueTree lab ("LoopLab");
    lab.setProperty ("samplePath", s.samplePath, nullptr);
    lab.setProperty ("sampleName", s.sampleName, nullptr);
    lab.setProperty ("tempoMult", s.tempoMult, nullptr);
    lab.setProperty ("fx", fxToString (s.fx), nullptr);
    juce::StringArray seeds, locked;
    for (int v = 0; v < ll::kNumVariations; ++v)
    {
        seeds.add (juce::String ((juce::int64) s.seeds[(size_t) v]));
        locked.add (s.locked[(size_t) v] ? "1" : "0");
        lab.setProperty ("steps" + juce::String (v), stepsToString (s.lockedSteps[(size_t) v]), nullptr);
    }
    lab.setProperty ("seeds", seeds.joinIntoString (","), nullptr);
    lab.setProperty ("locked", locked.joinIntoString (","), nullptr);

    auto existing = state.getChildWithName ("LoopLab");
    if (existing.isValid()) state.removeChild (existing, nullptr);
    state.addChild (lab, -1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaLoopLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto lab = state.getChildWithName ("LoopLab");
    currentProgram = (int) state.getProperty ("program", 0);
    if (lab.isValid())
        state.removeChild (lab, nullptr);
    apvts.replaceState (state);

    if (! lab.isValid())
        return;

    const auto path = lab.getProperty ("samplePath").toString();
    core.updateSpec ([lab] (ll::Spec& s)
    {
        s.sampleName = lab.getProperty ("sampleName").toString();
        s.tempoMult = (double) lab.getProperty ("tempoMult", 1.0);
        s.fx = fxFromString (lab.getProperty ("fx").toString());
        const auto seeds = juce::StringArray::fromTokens (lab.getProperty ("seeds").toString(), ",", "");
        const auto locked = juce::StringArray::fromTokens (lab.getProperty ("locked").toString(), ",", "");
        for (int v = 0; v < ll::kNumVariations; ++v)
        {
            if (v < seeds.size()) s.seeds[(size_t) v] = (juce::uint32) seeds[v].getLargeIntValue();
            s.locked[(size_t) v] = v < locked.size() && locked[v] == "1";
            s.lockedSteps[(size_t) v] = stepsFromString (lab.getProperty ("steps" + juce::String (v)).toString());
        }
    });
    if (path.isNotEmpty())
        core.loadFile (juce::File (path), false);
}
