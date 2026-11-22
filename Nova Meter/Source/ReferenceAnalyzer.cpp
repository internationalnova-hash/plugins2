#include "ReferenceAnalyzer.h"

#include <algorithm>

namespace tz
{

static float percentile (std::vector<float> v, float p)
{
    if (v.empty()) return kNoValue;
    const auto k = (size_t) std::clamp ((int) std::lround (p * (float) (v.size() - 1)), 0, (int) v.size() - 1);
    std::nth_element (v.begin(), v.begin() + (long) k, v.end());
    return v[k];
}

ReferenceStats analyzeReader (juce::AudioFormatReader& reader, const juce::String& name,
                              const std::function<bool (float)>& progress)
{
    ReferenceStats r;
    r.name = name;
    r.sampleRate = reader.sampleRate;
    r.channels = (int) std::min (2u, reader.numChannels);
    r.durationSec = reader.sampleRate > 0 ? (double) reader.lengthInSamples / reader.sampleRate : 0.0;

    if (r.sampleRate <= 0 || r.channels <= 0 || reader.lengthInSamples <= 0)
        return {};

    MeterEngine eng;
    eng.prepare (r.sampleRate, r.channels);

    // Feed exactly 100 ms per step so each step lines up with one engine sub-block.
    const int chunk = std::max (1, (int) std::lround (r.sampleRate * 0.1));
    juce::AudioBuffer<float> buf (r.channels, chunk);

    std::array<std::vector<float>, NumMeters> series;
    const auto total = reader.lengthInSamples;

    for (juce::int64 pos = 0; pos < total; pos += chunk)
    {
        const int n = (int) std::min<juce::int64> (chunk, total - pos);
        buf.clear();
        reader.read (&buf, 0, n, pos, true, r.channels > 1);
        eng.process (buf.getArrayOfReadPointers(), r.channels, n);

        series[Peak].push_back (eng.takePeakSinceLastRead (Peak));
        series[TruePeak].push_back (eng.takePeakSinceLastRead (TruePeak));
        for (auto id : { LufsShort, Rms, DynRange, MidDensity })
            series[(size_t) id].push_back (eng.current (id));

        if (progress && (pos / chunk) % 20 == 0 && ! progress ((float) ((double) pos / (double) total)))
            return {};
    }

    const float integrated = eng.current (LufsInt);

    // "Active" = the body of the song: short-term loudness within 20 LU of the
    // integrated value. Keeps intros, fades and silence out of the ranges.
    std::vector<bool> active (series[LufsShort].size(), false);
    for (size_t i = 0; i < active.size(); ++i)
    {
        const float st = series[LufsShort][i];
        active[i] = hasValue (st) && (! hasValue (integrated) || st > integrated - 20.0f);
    }

    auto activeValues = [&] (MeterId id)
    {
        std::vector<float> out;
        for (size_t i = 0; i < series[(size_t) id].size(); ++i)
            if (active[i] && hasValue (series[(size_t) id][i]))
                out.push_back (series[(size_t) id][i]);
        return out;
    };

    for (int i = 0; i < NumMeters; ++i)
    {
        const auto id = (MeterId) i;
        auto& s = r.m[(size_t) i];
        if (id == LufsInt)
        {
            s.headline = s.low = s.high = s.max = integrated;
            continue;
        }

        const auto vals = activeValues (id);
        s.low = percentile (vals, 0.10f);
        s.high = percentile (vals, 0.90f);
        s.max = vals.empty() ? kNoValue : *std::max_element (vals.begin(), vals.end());

        if (id == Peak || id == TruePeak)
            s.headline = s.max = eng.heldMax (id);
        else if (id == LufsShort)
            s.headline = s.max;                    // loudest 3 s
        else
            s.headline = percentile (vals, 0.5f);  // typical value
    }

    if (progress) progress (1.0f);
    return r;
}

Profile targetsFromReference (const ReferenceStats& ref, const Profile& base)
{
    Profile p = base;
    p.factory = false;
    p.name = "Reference: " + ref.name;

    auto set = [&] (MeterId id, float lo, float hi)
    {
        if (! hasValue (lo) || ! hasValue (hi)) return;
        const auto& spec = meterSpecs()[(size_t) id];
        lo = std::clamp (lo, spec.scaleMin, spec.scaleMax);
        hi = std::clamp (hi, spec.scaleMin, spec.scaleMax);
        p.meters[(size_t) id].min = std::round (std::min (lo, hi) * 10.0f) / 10.0f;
        p.meters[(size_t) id].max = std::round (std::max (lo, hi) * 10.0f) / 10.0f;
    };

    // Widen a range to at least +/- `half` around its centre value.
    auto atLeast = [] (float lo, float hi, float centre, float half)
    {
        return std::make_pair (std::min (lo, centre - half), std::max (hi, centre + half));
    };

    const auto& m = ref.m;
    set (Peak,      m[Peak].max - 2.0f,     m[Peak].max + 0.3f);
    set (TruePeak,  m[TruePeak].max - 1.5f, m[TruePeak].max + 0.3f);
    set (LufsInt,   m[LufsInt].headline - 1.0f, m[LufsInt].headline + 1.0f);

    { auto [lo, hi] = atLeast (m[LufsShort].low, m[LufsShort].max + 0.5f, m[LufsShort].max - 1.0f, 1.0f); set (LufsShort, lo, hi); }
    for (auto id : { Rms, DynRange, MidDensity })
    {
        auto [lo, hi] = atLeast (m[id].low, m[id].high, m[id].headline, 1.0f);
        set (id, lo, hi);
    }
    return p;
}

//==============================================================================
static juce::var statVar (float v) { return hasValue (v) ? juce::var (v) : juce::var(); }
static float statFrom (const juce::var& v) { return v.isVoid() || v.isUndefined() ? kNoValue : (float) (double) v; }

juce::var ReferenceStats::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("duration", durationSec);
    o->setProperty ("sampleRate", sampleRate);
    o->setProperty ("channels", channels);

    juce::Array<juce::var> meters;
    for (const auto& s : m)
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("headline", statVar (s.headline));
        so->setProperty ("low", statVar (s.low));
        so->setProperty ("high", statVar (s.high));
        so->setProperty ("max", statVar (s.max));
        meters.add (juce::var (so));
    }
    o->setProperty ("meters", meters);
    return juce::var (o);
}

ReferenceStats ReferenceStats::fromVar (const juce::var& v)
{
    ReferenceStats r;
    if (! v.isObject()) return r;
    r.name = v.getProperty ("name", "").toString();
    r.durationSec = (double) v.getProperty ("duration", 0.0);
    r.sampleRate = (double) v.getProperty ("sampleRate", 0.0);
    r.channels = (int) v.getProperty ("channels", 0);

    if (auto* arr = v.getProperty ("meters", {}).getArray())
        for (int i = 0; i < std::min (arr->size(), (int) NumMeters); ++i)
        {
            const auto& s = arr->getReference (i);
            r.m[(size_t) i] = { statFrom (s.getProperty ("headline", {})), statFrom (s.getProperty ("low", {})),
                                statFrom (s.getProperty ("high", {})), statFrom (s.getProperty ("max", {})) };
        }
    return r;
}

//==============================================================================
void ReferenceAnalyzer::start (const juce::File& f, const juce::String& name, bool deleteWhenDone)
{
    stopThread (4000);
    {
        const juce::ScopedLock sl (lock);
        file = f;
        displayName = name;
        deleteFile = deleteWhenDone;
        error.clear();
    }
    progress = 0.0f;
    status = Status::Analyzing;
    startThread (juce::Thread::Priority::low);
}

bool ReferenceAnalyzer::takeResult (ReferenceStats& out)
{
    const juce::ScopedLock sl (lock);
    if (! hasNewResult) return false;
    out = result;
    hasNewResult = false;
    return true;
}

void ReferenceAnalyzer::run()
{
    juce::File f;
    juce::String name;
    bool del;
    { const juce::ScopedLock sl (lock); f = file; name = displayName; del = deleteFile; }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    ReferenceStats r;
    juce::String err;

    if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (f) })
    {
        r = analyzeReader (*reader, name, [this] (float p) { progress = p; return ! threadShouldExit(); });
        if (threadShouldExit()) { if (del) f.deleteFile(); return; }
        if (! r.valid())
            err = "That file has no audible audio to measure.";
    }
    else
    {
        err = "Couldn't read that file. Use WAV, AIFF, FLAC, MP3"
             #if JUCE_MAC
              ", M4A/AAC"
             #endif
              " or OGG.";
    }

    if (del) f.deleteFile();

    const juce::ScopedLock sl (lock);
    if (err.isEmpty())
    {
        result = r;
        hasNewResult = true;
        status = Status::Done;
    }
    else
    {
        error = err;
        status = Status::Failed;
    }
}

} // namespace tz
