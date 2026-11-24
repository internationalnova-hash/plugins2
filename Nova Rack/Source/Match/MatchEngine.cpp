#include "MatchEngine.h"
#include "../RackSlot.h"   // applyParamTexts

#include <cmath>
#include <optional>

namespace nr::match
{

namespace
{
    using Axis = std::pair<juce::String, juce::StringArray>;   // parameter, candidate values

    juce::StringArray nums (std::initializer_list<double> v, int decimals = 1)
    {
        juce::StringArray a;
        for (auto x : v) a.add (juce::String (x, decimals));
        return a;
    }

    juce::StringArray norms (std::initializer_list<double> v)
    {
        juce::StringArray a;
        for (auto x : v) a.add ("=" + juce::String (x, 3));
        return a;
    }

    float sumOther (int stage, const VocalProfile& r, const VocalProfile& t)
    {
        float s = 0.f;
        for (int i = 0; i < kNumStages; ++i) if (i != stage) s += stageDistance (i, r, t);
        return s;
    }

    // Trim to the loudest kMaxClipSeconds and add silence so tails ring out
    juce::AudioBuffer<float> prepareClip (const juce::AudioBuffer<float>& in, double rate, double maxSeconds)
    {
        const int len = juce::jmin (in.getNumSamples(), (int) (maxSeconds * rate));
        const int pad = (int) (1.2 * rate);
        juce::AudioBuffer<float> out (2, len + pad);
        out.clear();
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, 0, in, juce::jmin (c, in.getNumChannels() - 1), 0, len);
        return out;
    }

    juce::String fmtDb (float db) { return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB"; }
}

const char* stagePlugin (int stage) noexcept
{
    static const char* p[kNumStages] = { "Nova Pitch", "Nova Desk EQ", "Nova Royal 670", "Nova Silk",
                                         "Nova Tape", "Nova Doubler", "Nova Delay", "Nova Space" };
    return p[juce::jlimit (0, kNumStages - 1, stage)];
}

juce::StringArray MatchEngine::preparePlugins (const PluginFactory& factory, double sampleRate)
{
    sr = sampleRate;
    juce::StringArray missing;
    for (int s = 0; s < kNumStages; ++s)
    {
        const juce::String name (stagePlugin (s));
        if (plugins.count (name) != 0) continue;
        auto inst = factory (name, sampleRate, 512);
        if (inst == nullptr) { missing.add (name); continue; }
        inst->enableAllBuses();
        auto layout = inst->getBusesLayout();
        if (layout.inputBuses.size() > 0)  layout.inputBuses.getReference (0)  = juce::AudioChannelSet::stereo();
        if (layout.outputBuses.size() > 0) layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        inst->setBusesLayout (layout);
        inst->setNonRealtime (true);
        plugins[name] = std::move (inst);
    }
    return missing;
}

juce::AudioBuffer<float> MatchEngine::render (const juce::String& plugin, const juce::StringPairArray& params,
                                              const juce::AudioBuffer<float>& in)
{
    ++renders;
    auto it = plugins.find (plugin);
    if (it == plugins.end()) return in;
    auto& p = *it->second;

    constexpr int block = 512;
    p.setNonRealtime (true);
    p.prepareToPlay (sr, block);
    nr::applyParamTexts (p, params);
    p.prepareToPlay (sr, block);   // again, so smoothed parameters start at their new values
    p.reset();

    const int len = in.getNumSamples();
    const int lat = juce::jmax (0, p.getLatencySamples());
    const int chans = juce::jmax (2, juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()));
    juce::AudioBuffer<float> out (2, len), blk (chans, block);
    out.clear();
    juce::MidiBuffer midi;

    for (int pos = 0; pos < len + lat; pos += block)
    {
        const int n = juce::jmin (block, len + lat - pos);
        blk.setSize (chans, n, false, false, true);
        blk.clear();
        for (int c = 0; c < 2; ++c)
            if (pos < len) blk.copyFrom (c, 0, in, c, pos, juce::jmin (n, len - pos));
        midi.clear();
        p.processBlock (blk, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const int o = pos + i - lat;
                if (o >= 0 && o < len)
                {
                    const float v = blk.getSample (c, i);
                    out.setSample (c, o, std::isfinite (v) ? v : 0.f);
                }
            }
    }
    return out;
}

MatchResult MatchEngine::run (const juce::AudioBuffer<float>& reference, double referenceRate,
                              const juce::AudioBuffer<float>& rawIn, double rawRate,
                              const std::array<bool, kNumStages>& locked, const Progress& progress)
{
    MatchResult res;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    renders = 0;
    auto report = [&] (float p, const juce::String& s) { return ! progress || progress (p, s); };

    if (std::abs (rawRate - sr) > 1.0) { res.error = "Internal: plugins prepared at a different rate"; return res; }

    report (0.01f, "Listening to the reference");
    res.reference = analyseVocal (reference, referenceRate);
    if (! res.reference.valid) { res.error = "The reference is too short or too quiet (need at least a few seconds of singing)."; return res; }

    const auto raw = prepareClip (rawIn, rawRate, kMaxClipSeconds);
    res.raw = analyseVocal (raw, rawRate);
    if (! res.raw.valid) { res.error = "Your vocal is too short or too quiet (need at least a few seconds of singing)."; return res; }

    res.rawScores = stageScores (res.reference, res.raw);
    res.rawOverall = overallScore (res.rawScores);

    const auto& ref = res.reference;
    auto curProf = res.raw;

    struct Plan { int stage; juce::StringPairArray fixed; std::vector<Axis> axes; int passes; bool wanted; };
    auto planFor = [&] (int s) -> Plan
    {
        Plan pl { s, {}, {}, 1, false };
        const auto& c = curProf;
        switch (s)
        {
            case Tuning:
                pl.wanted = ref.tuningCents < c.tuningCents - 2.f;
                pl.fixed.set ("Scale", "Chromatic");
                pl.fixed.set ("Mix", "100");
                pl.axes = { { "Retune Speed", nums ({ 70, 50, 35, 20, 10, 0 }, 0) }, { "Humanize", nums ({ 0, 25, 50 }, 0) } };
                break;
            case Tone:
            {
                pl.wanted = true;
                pl.passes = 2;
                pl.fixed.set ("Oversampling", "=0");
                pl.fixed.set ("Low Mid Q", "0.8");
                pl.fixed.set ("High Mid Q", "0.8");
                // Start from the difference curve so the search begins close
                auto diffAt = [&] (float lo, float hi)
                {
                    double d = 0.0; int m = 0;
                    for (int b = 0; b < VocalProfile::kBands; ++b)
                    {
                        const float f = VocalProfile::bandCentreHz (b);
                        if (f >= lo && f <= hi) { d += ref.spectrumDb[(size_t) b] - c.spectrumDb[(size_t) b]; ++m; }
                    }
                    return juce::jlimit (-9.f, 9.f, (float) (d / juce::jmax (1, m)));
                };
                auto gains = [] (float centre)
                {
                    juce::StringArray a;
                    for (int k = -6; k <= 6; ++k) a.add (juce::String (juce::jlimit (-12.f, 12.f, std::round ((centre + k * 1.5f) * 2.f) / 2.f), 1));
                    a.removeDuplicates (false);
                    return a;
                };
                pl.axes = {
                    { "HPF Frequency", norms ({ 0.0, 0.2, 0.4, 0.6 }) },
                    { "Low Gain", gains (diffAt (80.f, 160.f)) },
                    { "Low Mid Frequency", nums ({ 180, 280, 420, 650, 1000 }, 0) },
                    { "Low Mid Gain", gains (diffAt (200.f, 800.f)) },
                    { "High Mid Frequency", nums ({ 1500, 2500, 3500, 5000, 7000 }, 0) },
                    { "High Mid Gain", gains (diffAt (1500.f, 6000.f)) },
                    { "High Gain", gains (diffAt (8000.f, 16000.f)) } };
                break;
            }
            case Dynamics:
                pl.wanted = true;
                pl.fixed.set ("Auto Gain", "On");
                pl.fixed.set ("Oversampling", "=0");
                pl.fixed.set ("Analog Noise", "0");
                pl.fixed.set ("Mix", "100");
                pl.axes = { { "Input", nums ({ -12, -6, -2, 2, 6, 10, 14 }) }, { "Time Constant", norms ({ 0.0, 0.2, 0.4, 0.6 }) } };
                break;
            case Air:
                pl.wanted = ref.sibilanceDb < c.sibilanceDb - 1.f;
                pl.axes = { { "Smooth", nums ({ 15, 30, 45, 60, 75, 90 }, 0) } };
                break;
            case Warmth:
                pl.wanted = true;
                pl.fixed.set ("Oversampling", "=0");
                pl.fixed.set ("Noise", "0");
                pl.fixed.set ("Wow / Flutter", "0.5");
                pl.axes = { { "Tape Type", { "Studio", "Vintage" } }, { "Tape", nums ({ 15, 35, 55, 75, 95 }, 0) }, { "Mix", nums ({ 30, 55, 80 }, 0) } };
                break;
            case Width:
                pl.wanted = ref.widthDb > c.widthDb + 4.f;
                pl.axes = { { "Width", nums ({ 20, 40, 60, 80, 100 }, 0) }, { "Mix", nums ({ 20, 35, 50, 70 }, 0) } };
                break;
            case Delay:
                pl.wanted = ref.echoDb > 9.f && ref.echoDb > c.echoDb + 4.f;
                pl.fixed.set ("Sync", "Off");
                pl.fixed.set ("Ping Pong", "Off");
                pl.axes = { { "Delay Time Free", nums ({ ref.echoMs, ref.echoMs * 0.9, ref.echoMs * 1.1 }, 0) },
                            { "Mix", nums ({ 10, 18, 26, 35, 45 }, 0) }, { "Feedback", nums ({ 10, 25, 40 }, 0) } };
                break;
            case Space:
                pl.wanted = ref.phraseEnds >= 2 && ref.tailDb > c.tailDb + 3.f;
                pl.axes = { { "Decay", nums ({ 0.8, 1.2, 1.8, 2.6, 3.6, 5.0 }) }, { "Mix", nums ({ 8, 14, 20, 28, 36, 45 }, 0) } };
                break;
            default: break;
        }
        return pl;
    };

    auto objective = [&] (int s, const VocalProfile& p)
    {
        // The stage's own target first, but never at the cost of everything else
        float o = stageDistance (s, ref, p) + 0.2f * sumOther (s, ref, p);
        if (s == Warmth) o += 0.5f * stageDistance (Tone, ref, p) + 0.5f * stageDistance (Dynamics, ref, p);
        return o;
    };

    // Chosen settings per stage; the chain always renders in rack order
    std::array<std::optional<juce::StringPairArray>, kNumStages> chosen;
    auto renderRange = [&] (int from, int to, juce::AudioBuffer<float> in)
    {
        for (int k = from; k < to; ++k)
            if (chosen[(size_t) k].has_value())
                in = render (stagePlugin (k), *chosen[(size_t) k], in);
        return in;
    };
    // Whole chain with stage s set to `params` (prefix cached per search)
    juce::AudioBuffer<float> prefix;
    int prefixFor = -1;
    auto evaluate = [&] (int s, const juce::StringPairArray& params)
    {
        if (prefixFor != s) { prefix = renderRange (0, s, raw); prefixFor = s; }
        auto out = render (stagePlugin (s), params, prefix);
        out = renderRange (s + 1, kNumStages, out);
        return analyseVocal (out, sr);
    };
    auto refreshCurrent = [&]
    {
        curProf = analyseVocal (renderRange (0, kNumStages, raw), sr);
        prefixFor = -1;
    };

    // Search order: what shapes the sound most first. Time effects before width (stereo reverb
    // is width too), and the EQ last, corrected through the whole chain.
    const int order[] = { Tuning, Dynamics, Air, Warmth, Space, Delay, Width };
    int step = 0;
    const float totalSteps = 10.f;
    auto cancelled = [&] (const juce::String& msg)
    {
        return progress && ! progress (0.04f + 0.86f * (float) step / totalSteps, msg + " · " + juce::String (renders) + " test renders");
    };

    auto overallOf = [&] (const VocalProfile& p) { return overallScore (stageScores (ref, p)); };

    // Search one stage's settings with the rest of the chain in place. The stage is kept only
    // if it raises the overall match.
    auto searchStage = [&] (int s, bool refine) -> bool   // false = cancelled
    {
        const juce::String plugin (stagePlugin (s));
        if (cancelled (juce::String (refine ? "Refining " : "Matching ") + stageName (s))) return false;
        if (locked[(size_t) s] || plugins.count (plugin) == 0) return true;

        auto plan = planFor (s);
        if (! plan.wanted && ! chosen[(size_t) s].has_value()) return true;

        // What the chain sounds like without this stage
        auto had = chosen[(size_t) s];
        chosen[(size_t) s].reset();
        prefixFor = -1;
        const auto withoutProf = had.has_value() ? analyseVocal (renderRange (0, kNumStages, raw), sr) : curProf;
        const float withoutScore = overallOf (withoutProf);

        juce::StringPairArray best = had.has_value() ? *had : plan.fixed;
        if (! had.has_value()) for (auto& ax : plan.axes) best.set (ax.first, ax.second[0]);
        auto bestProf = evaluate (s, best);
        float bestObj = objective (s, bestProf);

        for (int pass = 0; pass < (refine ? 1 : plan.passes); ++pass)
            for (auto& ax : plan.axes)
                for (auto& v : ax.second)
                {
                    if (best[ax.first] == v) continue;
                    auto trial = best;
                    trial.set (ax.first, v);
                    const auto prof = evaluate (s, trial);
                    const float o = objective (s, prof);
                    if (o < bestObj) { bestObj = o; best = trial; bestProf = prof; }
                    if (cancelled (juce::String (refine ? "Refining " : "Matching ") + stageName (s))) return false;
                }

        if (overallOf (bestProf) > withoutScore + 0.5f) { chosen[(size_t) s] = best; curProf = bestProf; }
        else                                            { curProf = withoutProf; }
        prefixFor = -1;
        return true;
    };

    // Reverb and delay both fill the gaps after phrases, so they're chosen together
    auto searchTimeEffects = [&] () -> bool
    {
        const bool spaceOk = ! locked[Space] && plugins.count (stagePlugin (Space)) != 0;
        const bool delayOk = ! locked[Delay] && plugins.count (stagePlugin (Delay)) != 0;
        const auto spacePlan = planFor (Space), delayPlan = planFor (Delay);
        const bool wantSpace = spaceOk && spacePlan.wanted, wantDelay = delayOk && delayPlan.wanted;
        if (! wantSpace && ! wantDelay) return true;

        juce::StringPairArray sp = spacePlan.fixed, dl = delayPlan.fixed;
        dl.set ("Delay Time Free", juce::String ((int) ref.echoMs));
        dl.set ("Feedback", "20");

        const auto pre = renderRange (0, Delay, raw);
        auto tryPair = [&] (float decay, float spaceMix, float delayMix)
        {
            auto out = pre;
            if (delayMix > 0.f) { auto d = dl; d.set ("Mix", juce::String (delayMix, 0)); out = render (stagePlugin (Delay), d, out); }
            if (spaceMix > 0.f) { auto p = sp; p.set ("Decay", juce::String (decay, 1)); p.set ("Mix", juce::String (spaceMix, 0)); out = render (stagePlugin (Space), p, out); }
            return analyseVocal (out, sr);
        };

        float bestO = 1.0e9f, bD = 0.f, bS = 0.f, bM = 0.f;
        VocalProfile bestP = curProf;
        for (float decay : { 1.0f, 1.6f, 2.4f, 3.4f })
            for (float smix : wantSpace ? std::initializer_list<float> { 0.f, 12.f, 20.f, 28.f, 38.f } : std::initializer_list<float> { 0.f })
                for (float dmix : wantDelay ? std::initializer_list<float> { 0.f, 14.f, 24.f, 34.f } : std::initializer_list<float> { 0.f })
                {
                    if (smix == 0.f && decay != 1.0f) continue;
                    const auto p = tryPair (decay, smix, dmix);
                    const float o = stageDistance (Space, ref, p) + stageDistance (Delay, ref, p) + 0.2f * (sumOther (Space, ref, p) - stageDistance (Delay, ref, p));
                    if (o < bestO) { bestO = o; bD = decay; bS = smix; bM = dmix; bestP = p; }
                    if (cancelled ("Matching reverb and delay")) return false;
                }

        if (bS > 0.f) { auto p = sp; p.set ("Decay", juce::String (bD, 1)); p.set ("Mix", juce::String (bS, 0)); chosen[Space] = p; }
        if (bM > 0.f) { auto d = dl; d.set ("Mix", juce::String (bM, 0)); chosen[Delay] = d; }
        curProf = bestP;
        prefixFor = -1;
        return true;
    };

    for (int s : order)
    {
        ++step;
        if (s == Delay) continue;                       // done with Space
        if (s == Space) { if (! searchTimeEffects()) { res.error = "Cancelled"; return res; } continue; }
        if (! searchStage (s, false)) { res.error = "Cancelled"; return res; }
    }

    // Tone: iterate the EQ against what the whole chain actually sounds like
    ++step;
    if (! locked[Tone] && plugins.count (stagePlugin (Tone)) != 0)
    {
        if (cancelled ("Matching Tone (EQ)")) { res.error = "Cancelled"; return res; }
        auto diffAt = [&] (const VocalProfile& p, float lo, float hi)
        {
            double d = 0.0; int m = 0;
            for (int b = 0; b < VocalProfile::kBands; ++b)
            {
                const float f = VocalProfile::bandCentreHz (b);
                if (f >= lo && f <= hi) { d += ref.spectrumDb[(size_t) b] - p.spectrumDb[(size_t) b]; ++m; }
            }
            return (float) (d / juce::jmax (1, m));
        };
        auto peakIn = [&] (const VocalProfile& p, float lo, float hi)
        {
            float best = 0.f, at = std::sqrt (lo * hi);
            for (int b = 0; b < VocalProfile::kBands; ++b)
            {
                const float f = VocalProfile::bandCentreHz (b);
                const float d = std::abs (ref.spectrumDb[(size_t) b] - p.spectrumDb[(size_t) b]);
                if (f >= lo && f <= hi && d > best) { best = d; at = f; }
            }
            return at;
        };

        juce::StringPairArray eq;
        eq.set ("Oversampling", "=0");
        eq.set ("HPF Frequency", "=0");
        eq.set ("Low Frequency", "100");
        eq.set ("High Frequency", "10000");
        eq.set ("Low Mid Q", "0.9");
        eq.set ("High Mid Q", "0.9");
        const float lmF = peakIn (curProf, 180.f, 1000.f), hmF = peakIn (curProf, 1500.f, 7000.f);
        eq.set ("Low Mid Frequency", juce::String ((int) lmF));
        eq.set ("High Mid Frequency", juce::String ((int) hmF));
        float g[4] = { 0.f, 0.f, 0.f, 0.f };
        const std::pair<float, float> region[4] = { { 70.f, 160.f }, { lmF / 1.5f, lmF * 1.5f }, { hmF / 1.5f, hmF * 1.5f }, { 8000.f, 16000.f } };
        const char* gainName[4] = { "Low Gain", "Low Mid Gain", "High Mid Gain", "High Gain" };

        const float without = stageDistance (Tone, ref, curProf);
        juce::StringPairArray best;
        VocalProfile bestProf;
        float bestD = 1.0e9f;
        auto prof = curProf;   // starts from the chain without an EQ
        for (int it = 0; it < 6; ++it)
        {
            for (int k = 0; k < 4; ++k)
            {
                g[k] = juce::jlimit (-12.f, 12.f, g[k] + 0.85f * diffAt (prof, region[k].first, region[k].second));
                eq.set (gainName[k], juce::String (std::round (g[k] * 2.f) / 2.f, 1));
            }
            prof = evaluate (Tone, eq);
            const float d = stageDistance (Tone, ref, prof);
            if (d < bestD) { bestD = d; best = eq; bestProf = prof; }
            if (cancelled ("Matching Tone (EQ)")) { res.error = "Cancelled"; return res; }
        }
        // Low cut, if the reference is thin at the bottom
        for (auto hp : { "=0.2", "=0.4", "=0.6" })
        {
            auto trial = best;
            trial.set ("HPF Frequency", hp);
            const auto p = evaluate (Tone, trial);
            const float d = stageDistance (Tone, ref, p);
            if (d < bestD) { bestD = d; best = trial; bestProf = p; }
        }
        if (bestD < without * 0.95f - 0.02f) { chosen[Tone] = best; curProf = bestProf; prefixFor = -1; }
    }

    // Second look at every stage with the finished chain around it (a delay can be rejected
    // early because the reverb that sits after it wasn't there yet, and so on)
    ++step;
    for (int s : { Space, Delay, Dynamics, Warmth, Width, Air })
        if (! searchStage (s, true)) { res.error = "Cancelled"; return res; }

    // The chain, in rack order, with the plugins' own value texts for the UI
    for (int s = 0; s < kNumStages; ++s)
    {
        if (! chosen[(size_t) s].has_value()) continue;
        StageResult st;
        st.stage = s;
        st.plugin = stagePlugin (s);
        st.params = *chosen[(size_t) s];
        st.params.remove ("Oversampling");          // the rack runs the full-quality versions
        auto& inst = *plugins[st.plugin];
        nr::applyParamTexts (inst, st.params);
        for (auto& k : st.params.getAllKeys())
            for (auto* p : inst.getParameters())
                if (p->getName (128).trim().equalsIgnoreCase (k))
                    st.display.set (k, p->getCurrentValueAsText());
        res.chain.push_back (st);
        res.used[(size_t) s] = true;
    }

    // Final render through the chain at full quality, for the score and the A/B
    report (0.92f, "Rendering the matched vocal");
    auto out = raw;
    for (auto& st : res.chain) out = render (st.plugin, st.params, out);
    res.matched = analyseVocal (out, sr);
    res.matchedScores = stageScores (ref, res.matched);
    res.matchedOverall = overallScore (res.matchedScores);
    res.matchedAudio = std::move (out);

    // Short summaries for the chain cards
    for (auto& st : res.chain)
    {
        auto d = [&] (const char* k) { return st.display[k]; };
        switch (st.stage)
        {
            case Tuning:   st.summary = "Retune " + d ("Retune Speed").upToFirstOccurrenceOf (".", false, false); break;
            case Tone:     st.summary = "Low " + d ("Low Gain") + " · Hi-mid " + d ("High Mid Gain"); break;
            case Dynamics: st.summary = "Input " + d ("Input") + " · " + d ("Time Constant"); break;
            case Air:      st.summary = "Smooth " + d ("Smooth"); break;
            case Warmth:   st.summary = d ("Tape Type") + " · Tape " + d ("Tape"); break;
            case Width:    st.summary = "Width " + d ("Width") + " · Mix " + d ("Mix"); break;
            case Delay:    st.summary = d ("Delay Time Free") + " · Mix " + d ("Mix"); break;
            case Space:    st.summary = d ("Decay") + " · Mix " + d ("Mix"); break;
            default: break;
        }
    }

    // Plain-language notes
    juce::StringArray notes;
    if (ref.phraseEnds >= 2 && ref.tailDb > -40.f)
        notes.add ("The reference has a noticeable reverb tail (" + fmtDb (ref.tailDb) + " after phrases).");
    if (res.used[Delay]) notes.add ("It has an echo about " + juce::String ((int) ref.echoMs) + " ms after each phrase.");
    {
        float maxD = 0.f; int maxB = 0;
        for (int b = 0; b < VocalProfile::kBands; ++b)
        {
            const float dd = res.raw.spectrumDb[(size_t) b] - ref.spectrumDb[(size_t) b];
            if (std::abs (dd) > std::abs (maxD)) { maxD = dd; maxB = b; }
        }
        if (std::abs (maxD) > 3.f)
        {
            const float f = VocalProfile::bandCentreHz (maxB);
            notes.add ("Your voice was " + juce::String (std::abs (maxD), 1) + " dB " + (maxD > 0 ? "louder" : "quieter") + " around "
                       + (f >= 1000.f ? juce::String (f / 1000.f, 1) + " kHz" : juce::String ((int) f) + " Hz") + " than the reference.");
        }
    }
    if (ref.tuningCents < res.raw.tuningCents - 4.f) notes.add ("The reference is tuned tighter than your take.");
    if (ref.rangeDb < res.raw.rangeDb - 3.f) notes.add ("The reference is more compressed (" + juce::String (ref.rangeDb, 1) + " dB range vs your " + juce::String (res.raw.rangeDb, 1) + ").");
    res.notes = notes.joinIntoString (" ");

    res.renders = renders;
    res.seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    res.ok = true;
    report (1.f, "Matched");
    return res;
}

} // namespace nr::match
