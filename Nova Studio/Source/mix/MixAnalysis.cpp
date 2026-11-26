#include "MixAnalysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>

namespace NovaStudio::MixAI
{
namespace
{
    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.f;
        auto m = v.begin() + (std::ptrdiff_t) (v.size() / 2);
        std::nth_element (v.begin(), m, v.end());
        return *m;
    }
    inline float dbOf (double ms) { return (float) (10.0 * std::log10 (ms + 1.0e-12)); }
    inline float clamp01 (float v) { return juce::jlimit (0.f, 1.f, v); }
    inline float between (float v, float lo, float hi) { return v >= lo && v <= hi ? 1.f : 0.f; }
}

// ════════════════════════════════════════════════════════════════════════════
// Roles
// ════════════════════════════════════════════════════════════════════════════
const char* roleName (Role r) noexcept
{
    switch (r)
    {
        case Role::Lead: return "Lead vocal";     case Role::Double: return "Double";
        case Role::Harmony: return "Harmony";     case Role::Adlib: return "Ad-lib";
        case Role::Kick: return "Kick";           case Role::Snare: return "Snare";
        case Role::Clap: return "Clap";           case Role::HiHat: return "Hi-hats";
        case Role::Perc: return "Percussion";     case Role::Bass: return "808 / Bass";
        case Role::Keys: return "Keys";           case Role::Guitar: return "Guitar";
        case Role::Pad: return "Pad";             case Role::Beat: return "Beat (2-track)";
        case Role::Fx: return "FX";               case Role::Other: return "Other";
        case Role::Skip: return "Leave alone";
    }
    return "Other";
}

Family familyOf (Role r) noexcept
{
    switch (r)
    {
        case Role::Lead: case Role::Double: case Role::Harmony: case Role::Adlib: return Family::Vocals;
        case Role::Kick: case Role::Snare: case Role::Clap: case Role::HiHat: case Role::Perc: return Family::Drums;
        case Role::Bass: return Family::Bass;
        case Role::Keys: case Role::Guitar: case Role::Pad: case Role::Beat: return Family::Music;
        case Role::Fx: return Family::Fx;
        default: return Family::Other;
    }
}

juce::Colour roleColour (Role r) noexcept
{
    switch (familyOf (r))
    {
        case Family::Vocals: return r == Role::Lead ? juce::Colour (0xffa46bff) : r == Role::Adlib ? juce::Colour (0xff4fb6ff) : juce::Colour (0xff7f8cff);
        case Family::Drums: return juce::Colour (0xffff9a3c);
        case Family::Bass: return juce::Colour (0xffff5d5d);
        case Family::Music: return r == Role::Beat ? juce::Colour (0xffffb347) : juce::Colour (0xffffd04a);
        case Family::Fx: return juce::Colour (0xff3fd1b5);
        default: return juce::Colour (0xff8b8fa3);
    }
}

juce::StringArray allRoleNames()
{
    juce::StringArray s;
    for (int i = 0; i <= (int) Role::Skip; ++i) s.add (roleName ((Role) i));
    return s;
}

Role roleFromName (const juce::String& name) noexcept
{
    for (int i = 0; i <= (int) Role::Skip; ++i) if (name == roleName ((Role) i)) return (Role) i;
    return Role::Other;
}

// ════════════════════════════════════════════════════════════════════════════
// Reading a track: its clips mixed to one buffer at 22.05 kHz
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    bool readTrack (const Track& t, double sessionRate, double maxSeconds, std::vector<float>& L, std::vector<float>& R, bool& stereo)
    {
        static juce::AudioFormatManager fm;
        static bool registered = false;
        if (! registered) { fm.registerBasicFormats(); registered = true; }

        int64_t endS = 0;
        for (const auto& c : t.clips) if (! c.isMidi && ! c.muted) endS = std::max (endS, c.startSample + c.lengthSamples);
        const int total = (int) (std::min ((double) endS / sessionRate, maxSeconds) * kRate) + 1;
        if (total <= 1) return false;
        L.assign ((size_t) total, 0.f);
        R.assign ((size_t) total, 0.f);
        stereo = false;

        for (const auto& c : t.clips)
        {
            if (c.isMidi || c.muted || ! c.file.existsAsFile()) continue;
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (c.file));
            if (r == nullptr) continue;
            const double fileRate = r->sampleRate;
            const int64_t off = (int64_t) ((double) c.fileOffsetSamples * fileRate / sessionRate);
            const int64_t len = (int64_t) ((double) c.lengthSamples * fileRate / sessionRate);
            if (len <= 0 || off >= r->lengthInSamples) continue;
            const int n = (int) std::min<int64_t> (len, r->lengthInSamples - off);
            const int nch = (int) std::min (2u, r->numChannels);
            if (nch > 1) stereo = true;
            // padded: the resampler reads a few samples past the end
            juce::AudioBuffer<float> buf (nch, n + 16);
            buf.clear();
            r->read (&buf, 0, n, off, true, nch > 1);
            const float g = juce::Decibels::decibelsToGain (c.gainDb);
            const double ratio = fileRate / kRate;
            const int outN = std::max (0, (int) ((double) n / ratio) - 2);
            const int at = (int) ((double) c.startSample / sessionRate * kRate);
            std::vector<float> tmp ((size_t) outN + 4);
            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator li;
                li.process (ratio, buf.getReadPointer (std::min (ch, nch - 1)), tmp.data(), outN);
                auto& dst = ch == 0 ? L : R;
                for (int i = 0; i < outN; ++i)
                {
                    const int p = at + i;
                    if (p < 0 || p >= total) continue;
                    dst[(size_t) p] += tmp[(size_t) i] * g;
                }
            }
        }
        return true;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Measuring
// ════════════════════════════════════════════════════════════════════════════
Features measure (const std::vector<float>& L, const std::vector<float>& R, double bpm,
                  std::vector<float>& envOut, std::vector<float>& pitchOut)
{
    Features f;
    const int n = (int) L.size();
    const int W = 2048, order = 11;
    const int frames = std::max (0, (n - W) / kHop + 1);
    f.seconds = (float) n / (float) kRate;
    envOut.assign ((size_t) frames, -120.f);
    pitchOut.assign ((size_t) frames, 0.f);
    if (frames < 4) return f;

    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) W), buf ((size_t) W * 2), prevMag ((size_t) W / 2 + 1, 0.f);
    for (int i = 0; i < W; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) W);
    std::vector<float> flux ((size_t) frames, 0.f), centroid ((size_t) frames, 0.f);
    std::vector<std::array<double, 6>> bands ((size_t) frames);
    double sideE = 0, midE = 0;
    const double binHz = kRate / W;
    const double edges[7] = { 0, 60, 250, 1000, 4000, 8000, kRate / 2 };

    // YIN via FFT for pitch (tau 65..1000 Hz)
    const int tauMin = (int) (kRate / 1000.0), tauMax = (int) (kRate / 65.0), Wy = 1024;
    const int Ny = 4096;
    juce::dsp::FFT fftY (12);
    std::vector<float> ya ((size_t) Ny * 2), yb ((size_t) Ny * 2), seg ((size_t) (Wy + tauMax + 1));
    std::vector<double> e2 (seg.size() + 1);
    std::vector<float> dn ((size_t) tauMax + 2);

    for (int fr = 0; fr < frames; ++fr)
    {
        const int s = fr * kHop;
        double e = 0;
        for (int i = 0; i < W; ++i)
        {
            const float m = 0.5f * (L[(size_t) (s + i)] + R[(size_t) (s + i)]);
            const float sd = 0.5f * (L[(size_t) (s + i)] - R[(size_t) (s + i)]);
            buf[(size_t) i] = m * win[(size_t) i];
            e += (double) m * m;
            midE += (double) m * m; sideE += (double) sd * sd;
        }
        envOut[(size_t) fr] = dbOf (e / W);
        std::fill (buf.begin() + W, buf.end(), 0.f);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        double cnum = 0, cden = 0, fl = 0;
        std::array<double, 6> be {};
        for (int k = 1; k <= W / 2; ++k)
        {
            const double mag = buf[(size_t) k], hz = k * binHz, p = mag * mag;
            cnum += hz * mag; cden += mag;
            for (int b = 0; b < 6; ++b) if (hz >= edges[b] && hz < edges[b + 1]) { be[(size_t) b] += p; break; }
            const double lm = std::log1p (mag), lp = std::log1p (prevMag[(size_t) k]);
            if (lm > lp) fl += lm - lp;
            prevMag[(size_t) k] = (float) mag;
        }
        flux[(size_t) fr] = (float) fl;
        centroid[(size_t) fr] = cden > 0 ? (float) (cnum / cden) : 0.f;
        bands[(size_t) fr] = be;

        // pitch (mono mid signal)
        const int s0 = s + W / 2 - Wy / 2;
        for (size_t i = 0; i < seg.size(); ++i)
        {
            const int k = s0 + (int) i;
            seg[i] = (k >= 0 && k < n) ? 0.5f * (L[(size_t) k] + R[(size_t) k]) : 0.f;
        }
        e2[0] = 0;
        for (size_t i = 0; i < seg.size(); ++i) e2[i + 1] = e2[i] + (double) seg[i] * seg[i];
        const double e0 = e2[(size_t) Wy];
        if (e0 < 1.0e-7) continue;
        std::fill (ya.begin(), ya.end(), 0.f); std::fill (yb.begin(), yb.end(), 0.f);
        std::copy (seg.begin(), seg.begin() + Wy, ya.begin());
        std::copy (seg.begin(), seg.end(), yb.begin());
        fftY.performRealOnlyForwardTransform (ya.data(), true);
        fftY.performRealOnlyForwardTransform (yb.data(), true);
        for (int k = 0; k <= Ny / 2; ++k)
        {
            const float ar = ya[(size_t) 2 * k], ai = ya[(size_t) 2 * k + 1], br = yb[(size_t) 2 * k], bi = yb[(size_t) 2 * k + 1];
            ya[(size_t) 2 * k] = ar * br + ai * bi; ya[(size_t) 2 * k + 1] = ar * bi - ai * br;
        }
        fftY.performRealOnlyInverseTransform (ya.data());
        const double scale = ya[0] != 0.f ? e0 / ya[0] : 1.0;
        double run = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const double d = std::max (0.0, e0 + (e2[(size_t) (tau + Wy)] - e2[(size_t) tau]) - 2.0 * ya[(size_t) tau] * scale);
            run += d;
            dn[(size_t) tau] = run > 0 ? (float) (d * tau / run) : 1.f;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (dn[(size_t) tau] < 0.2f) { while (tau + 1 < tauMax && dn[(size_t) tau + 1] < dn[(size_t) tau]) ++tau; best = tau; break; }
        if (best > 0 && dn[(size_t) best] < 0.25f)
            pitchOut[(size_t) fr] = 69.f + 12.f * std::log2 ((float) (kRate / best) / 440.f);
    }

    // active frames
    const float peak = *std::max_element (envOut.begin(), envOut.end());
    const float floorDb = std::max (-55.f, peak - 40.f);
    std::vector<int> act;
    for (int fr = 0; fr < frames; ++fr) if (envOut[(size_t) fr] > floorDb) act.push_back (fr);
    if (act.empty()) return f;
    f.activeFrac = (float) act.size() / (float) frames;
    f.peakDb = peak;
    double ms = 0;
    for (int fr : act) ms += std::pow (10.0, envOut[(size_t) fr] / 10.0);
    f.loudDb = dbOf (ms / act.size());

    std::array<double, 6> bt {};
    double ct = 0, cw = 0;
    int voiced = 0, steady = 0;
    std::vector<float> pv, wob;
    for (int fr : act)
    {
        for (int b = 0; b < 6; ++b) bt[(size_t) b] += bands[(size_t) fr][(size_t) b];
        const double w = std::pow (10.0, envOut[(size_t) fr] / 20.0);
        ct += centroid[(size_t) fr] * w; cw += w;
        if (pitchOut[(size_t) fr] > 0) { ++voiced; pv.push_back (pitchOut[(size_t) fr]); }
        if (fr > 0 && pitchOut[(size_t) fr] > 0 && pitchOut[(size_t) fr - 1] > 0)
            wob.push_back (std::abs (pitchOut[(size_t) fr] - pitchOut[(size_t) fr - 1]));
        if (fr > 0 && std::abs (envOut[(size_t) fr] - envOut[(size_t) fr - 1]) < 1.5f) ++steady;
    }
    // pitch frames outside active stretches don't count
    for (int fr = 0; fr < frames; ++fr) if (envOut[(size_t) fr] <= floorDb) pitchOut[(size_t) fr] = 0.f;
    const double btot = std::accumulate (bt.begin(), bt.end(), 0.0);
    for (int b = 0; b < 6; ++b) f.band[b] = btot > 0 ? (float) (bt[(size_t) b] / btot) : 0.f;
    f.centroidHz = cw > 0 ? (float) (ct / cw) : 0.f;
    f.voicedFrac = (float) voiced / (float) act.size();
    f.pitchMedian = median (pv);
    std::vector<float> smallWob;
    for (float w : wob) if (w < 1.0f) smallWob.push_back (w);       // ignore note changes, keep the wobble
    f.pitchWobble = median (smallWob);
    f.sustain = (float) steady / (float) act.size();
    f.width = (float) (sideE / std::max (1.0e-9, midE + sideE));

    // onsets: peaks of spectral flux above the local average
    std::vector<int> onsets;
    for (int fr = 2; fr < frames - 2; ++fr)
    {
        if (envOut[(size_t) fr] <= floorDb) continue;
        double loc = 0; int c = 0;
        for (int k = std::max (0, fr - 8); k <= std::min (frames - 1, fr + 8); ++k) { loc += flux[(size_t) k]; ++c; }
        loc /= c;
        if (flux[(size_t) fr] > loc * 1.6 + 1.0 && flux[(size_t) fr] >= flux[(size_t) fr - 1] && flux[(size_t) fr] > flux[(size_t) fr + 1]
            && (onsets.empty() || fr - onsets.back() >= 2))
            onsets.push_back (fr);
    }
    const float activeSec = (float) act.size() * kHop / (float) kRate;
    f.onsetRate = activeSec > 0 ? (float) onsets.size() / activeSec : 0.f;

    // decay after each onset: frames until 12 dB down
    std::vector<float> dec;
    for (int o : onsets)
    {
        int k = o;
        const float top = envOut[(size_t) o];
        while (k + 1 < frames && k - o < 30 && envOut[(size_t) k + 1] > top - 12.f) ++k;
        dec.push_back ((float) (k - o + 1) * kHop / (float) kRate * 1000.f);
    }
    f.decayMs = median (dec);

    // timing against the beat grid (bar 1 at time 0)
    if (bpm > 0 && ! onsets.empty())
    {
        const double beatFrames = 60.0 / bpm * kRate / kHop;
        int on = 0, back = 0;
        for (int o : onsets)
        {
            const double b = o / beatFrames;
            const double d = std::abs (b - std::round (b));
            if (d < 0.15) { ++on; if (((long long) std::llround (b)) % 2 == 1) ++back; }
        }
        f.onBeat = (float) on / (float) onsets.size();
        f.backbeat = on > 0 ? (float) back / (float) on : 0.f;
    }
    return f;
}

// ════════════════════════════════════════════════════════════════════════════
// Deciding a role
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    struct Hint { const char* word; Role role; };
    // order matters: more specific first ("lead synth" before "lead")
    const Hint kHints[] = {
        { "lead synth", Role::Keys }, { "synth lead", Role::Keys }, { "bass synth", Role::Bass },
        { "adlib", Role::Adlib }, { "ad lib", Role::Adlib }, { "ad-lib", Role::Adlib }, { "adl", Role::Adlib }, { "hype", Role::Adlib },
        { "dbl", Role::Double }, { "double", Role::Double }, { "dub ", Role::Double },
        { "harm", Role::Harmony }, { "hrm", Role::Harmony }, { "bgv", Role::Harmony }, { "backing", Role::Harmony }, { "stack", Role::Harmony }, { "choir", Role::Harmony },
        { "lead", Role::Lead }, { "main vox", Role::Lead }, { "ld vox", Role::Lead },
        { "kick", Role::Kick }, { "kik", Role::Kick }, { "bd ", Role::Kick },
        { "snare", Role::Snare }, { "snr", Role::Snare }, { "rim", Role::Snare },
        { "clap", Role::Clap }, { "clp", Role::Clap }, { "snap", Role::Clap },
        { "hihat", Role::HiHat }, { "hi hat", Role::HiHat }, { "hi-hat", Role::HiHat }, { "hat", Role::HiHat }, { "hh", Role::HiHat },
        { "perc", Role::Perc }, { "shaker", Role::Perc }, { "conga", Role::Perc }, { "bongo", Role::Perc }, { "tamb", Role::Perc },
        { "guiro", Role::Perc }, { "cowbell", Role::Perc }, { "tom", Role::Perc }, { "timbal", Role::Perc },
        { "808", Role::Bass }, { "bass", Role::Bass }, { "sub", Role::Bass },
        { "piano", Role::Keys }, { "rhodes", Role::Keys }, { "keys", Role::Keys }, { "key", Role::Keys }, { "organ", Role::Keys }, { "synth", Role::Keys }, { "pluck", Role::Keys },
        { "guitar", Role::Guitar }, { "gtr", Role::Guitar }, { "acoustic", Role::Guitar },
        { "pad", Role::Pad }, { "string", Role::Pad }, { "atmos", Role::Pad }, { "texture", Role::Pad },
        { "beat", Role::Beat }, { "instrumental", Role::Beat }, { "inst", Role::Beat }, { "2trk", Role::Beat }, { "2 trk", Role::Beat },
        { "2track", Role::Beat }, { "2 track", Role::Beat }, { "riddim", Role::Beat }, { "prod", Role::Beat }, { "ruff", Role::Beat },
        { "fx", Role::Fx }, { "riser", Role::Fx }, { "sweep", Role::Fx }, { "impact", Role::Fx }, { "whoosh", Role::Fx }, { "reverse", Role::Fx },
        { "vox", Role::Lead }, { "vocal", Role::Lead }, { "voc", Role::Lead }, { "rap", Role::Lead }, { "hook", Role::Lead }, { "verse", Role::Lead },
    };

    bool hintFor (const juce::String& name, Role& out, bool& genericVocal)
    {
        const auto n = " " + name.toLowerCase() + " ";
        genericVocal = false;
        for (const auto& h : kHints)
            if (n.contains (h.word))
            {
                out = h.role;
                const juce::String w (h.word);
                genericVocal = w == "vox" || w == "vocal" || w == "voc" || w == "rap" || w == "hook" || w == "verse";
                return true;
            }
        return false;
    }

    juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; }
}

void classify (TrackAnalysis& t)
{
    const auto& f = t.f;
    std::map<Role, float> sc;
    if (t.midi)
    {
        // MIDI: decide from the notes
        if (f.midiNotes == 0) { t.role = Role::Other; t.confidence = 0.3f; t.heard = "MIDI track with no notes"; return; }
        sc[Role::Bass] = f.midiAvgPitch < 48 ? 3.f : 0.f;
        sc[Role::Pad] = f.midiAvgBeats > 2.f ? 2.f : 0.f;
        sc[Role::Keys] = 1.5f;
        sc[Role::Perc] = (f.midiAvgPitch >= 35 && f.midiAvgPitch <= 60 && f.midiAvgBeats < 0.3f) ? 1.f : 0.f;
    }
    else if (f.activeFrac <= 0.f || (f.activeFrac < 0.03f && f.loudDb < -35.f))
    {
        t.role = Role::Skip; t.confidence = 0.9f; t.heard = "silent - nothing to mix"; return;
    }
    else
    {
        const float low = f.band[0] + f.band[1], highs = f.band[4] + f.band[5], mids = f.band[2] + f.band[3];
        const float voiced = clamp01 (f.voicedFrac / 0.5f);
        sc[Role::Lead] = voiced * 1.6f + between (f.pitchWobble, 0.08f, 0.9f) + between (f.centroidHz, 600.f, 3800.f) * 0.5f
                         + (f.band[3] > 0.2f ? 0.5f : 0.f) - (f.onsetRate > 5.f ? 1.f : 0.f) - (low > 0.55f ? 2.f : 0.f)
                         - (f.sustain > 0.85f && f.activeFrac > 0.9f ? 1.f : 0.f) + between (f.pitchMedian, 40.f, 84.f) * 0.3f;
        sc[Role::Kick] = (low > 0.55f ? 2.f : 0.f) + between (f.onsetRate, 0.4f, 4.5f) + (f.decayMs < 260.f ? 1.f : 0.f)
                         + (f.onBeat > 0.55f ? 1.f : 0.f) - (voiced > 0.8f ? 1.f : 0.f) - (f.sustain > 0.8f ? 1.f : 0.f);
        sc[Role::Snare] = (mids > 0.45f ? 1.f : 0.f) + between (f.onsetRate, 0.4f, 4.5f) + (f.decayMs < 320.f ? 0.5f : 0.f)
                          + (f.backbeat > 0.5f ? 1.5f : 0.f) + (f.voicedFrac < 0.3f ? 0.5f : 0.f) + (f.band[0] < 0.12f ? 0.5f : 0.f);
        sc[Role::Clap] = sc[Role::Snare] - 0.4f + (highs > 0.3f ? 0.3f : 0.f);
        sc[Role::HiHat] = (highs > 0.5f ? 2.f : 0.f) + (f.onsetRate > 2.5f ? 1.f : 0.f) + (f.decayMs < 220.f ? 0.5f : 0.f) - (voiced > 0.6f ? 1.f : 0.f);
        sc[Role::Perc] = 1.f + (f.onsetRate > 1.f ? 0.5f : 0.f) + (f.decayMs < 320.f ? 0.5f : 0.f) + (f.band[3] + f.band[4] > 0.4f ? 0.5f : 0.f)
                         - (voiced > 0.6f ? 1.f : 0.f);
        sc[Role::Bass] = (low > 0.7f ? 2.5f : low > 0.55f ? 1.f : 0.f) + (f.pitchMedian > 0 && f.pitchMedian < 50 ? 1.f : 0.f)
                         + (f.decayMs > 250.f ? 1.f : 0.f) + (f.centroidHz < 280.f ? 1.f : 0.f);
        sc[Role::Keys] = between (f.voicedFrac, 0.15f, 0.75f) * 0.5f + between (f.onsetRate, 0.4f, 4.f) * 0.5f
                         + between (f.decayMs, 180.f, 1500.f) * 0.5f + (f.pitchWobble < 0.08f ? 1.f : 0.f) + (mids > 0.5f ? 0.5f : 0.f);
        sc[Role::Guitar] = sc[Role::Keys] - 0.3f + between (f.centroidHz, 700.f, 2600.f) * 0.3f;
        sc[Role::Pad] = (f.sustain > 0.7f ? 1.5f : 0.f) + (f.activeFrac > 0.6f ? 0.5f : 0.f) + (f.onsetRate < 0.6f ? 1.f : 0.f)
                        + (f.pitchWobble < 0.08f ? 0.5f : 0.f) - (low > 0.5f ? 1.f : 0.f);
        sc[Role::Beat] = (f.activeFrac > 0.6f ? 1.f : 0.f) + (low > 0.25f && highs > 0.06f ? 1.5f : 0.f) + (f.onsetRate > 1.2f ? 0.5f : 0.f)
                         + (f.onBeat > 0.45f ? 0.5f : 0.f) + (f.width > 0.05f ? 0.4f : 0.f) + (f.seconds > 60.f ? 0.3f : 0.f);
        sc[Role::Fx] = (f.activeFrac < 0.15f ? 1.5f : 0.f) + (f.onsetRate < 0.5f ? 0.5f : 0.f);

        // Drums are (almost) never pitched: a mostly-pitched track isn't a drum
        const float pitched = clamp01 ((f.voicedFrac - 0.25f) / 0.3f);
        for (Role r : { Role::Kick, Role::Snare, Role::Clap, Role::HiHat, Role::Perc }) sc[r] -= 3.f * pitched;
        // Voices waver in pitch; instruments hold it dead steady
        if (f.voicedFrac > 0.3f && f.pitchWobble < 0.06f) sc[Role::Lead] -= 2.5f;
        // A whole-song, full-range track is a beat, not one drum or the bass
        if (f.activeFrac > 0.85f && f.seconds > 45.f && low > 0.3f && low < 0.88f && mids > 0.12f && f.voicedFrac < 0.4f)
            sc[Role::Beat] += 2.5f;
        if (f.activeFrac > 0.85f && f.sustain > 0.5f) sc[Role::Kick] -= 2.f;
        if (low > 0.88f) sc[Role::Bass] += 1.f;
    }

    Role hint = Role::Other;
    bool generic = false;
    bool hasHint = hintFor (t.name, hint, generic);
    if (! hasHint && t.clipHint.isNotEmpty()) hasHint = hintFor (t.clipHint, hint, generic);
    bool ambiguousLead = hasHint && hint == Role::Lead && (" " + (t.name + " " + t.clipHint).toLowerCase() + " ").contains ("lead");
    if (ambiguousLead)
    {
        // "guitar lead", "synth lead": the instrument word wins over "lead"
        Role other; bool g2;
        const auto stripped = (t.name + " " + t.clipHint).toLowerCase().replace ("lead", " ");
        if (hintFor (stripped, other, g2) && familyOf (other) != Family::Vocals) { hint = other; generic = false; ambiguousLead = false; }
    }
    if (hasHint) sc[hint] += ambiguousLead ? 1.5f : generic ? 2.0f : 3.0f;

    Role best = Role::Other; float b1 = -1e9f, b2 = -1e9f;
    for (auto& [r, v] : sc)
    {
        if (v > b1) { b2 = b1; b1 = v; best = r; }
        else if (v > b2) b2 = v;
    }
    t.role = best;
    t.confidence = 0.5f + 0.5f * std::tanh ((b1 - b2) / 1.5f);
    if (hasHint && hint == best) t.confidence = std::max (t.confidence, generic ? 0.8f : 0.9f);

    // a plain-words reason
    const auto& g = t.f;
    switch (familyOf (best))
    {
        case Family::Vocals: t.heard = "sung - " + percent (g.activeFrac) + " of the song"; break;
        case Family::Bass:   t.heard = t.midi ? "MIDI, low notes" : "sub-heavy" + juce::String (g.pitchMedian > 0 ? ", pitched" : "") + ", long notes"; break;
        case Family::Fx:     t.heard = "occasional hits / sweeps"; break;
        default: break;
    }
    switch (best)
    {
        case Role::Kick:   t.heard = "low thump" + juce::String (g.onBeat > 0.55f ? ", on the beat" : ""); break;
        case Role::Snare:  t.heard = g.backbeat > 0.5f ? "crack on beats 2 & 4" : "short mid-range hits"; break;
        case Role::Clap:   t.heard = "clap / snap hits"; break;
        case Role::HiHat:  t.heard = "fast, bright hits"; break;
        case Role::Perc:   t.heard = "percussion pattern"; break;
        case Role::Keys:   t.heard = t.midi ? "MIDI instrument" : "pitched, steady notes"; break;
        case Role::Guitar: t.heard = "plucked / strummed"; break;
        case Role::Pad:    t.heard = "sustained, wide"; break;
        case Role::Beat:   t.heard = "full instrumental (drums + bass + music)"; break;
        default: break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Vocals: lead, doubles, harmonies, ad-libs
// ════════════════════════════════════════════════════════════════════════════
void sortVocals (SessionAnalysis& a)
{
    std::vector<TrackAnalysis*> v;
    for (auto& t : a.tracks) if (familyOf (t.role) == Family::Vocals) v.push_back (&t);
    if (v.empty()) return;

    auto hinted = [] (const TrackAnalysis& t, Role r)
    {
        Role h; bool generic;
        return hintFor (t.name, h, generic) && h == r && ! generic;
    };
    // names engineers give the main vocal parts
    auto leadName = [] (const TrackAnalysis& t)
    {
        const auto n = " " + t.name.toLowerCase() + " ";
        for (auto* w : { "lead", "main", "verse", "hook", "chorus", "intro", "outro", "bridge", "pre" })
            if (n.contains (w) && ! n.contains ("dub") && ! n.contains ("dbl") && ! n.contains ("double") && ! n.contains ("stack")
                && ! n.contains ("adlib") && ! n.contains ("harm")) return true;
        return false;
    };
    // lead: the voice that's there the most and loudest (a "lead" name wins)
    TrackAnalysis* lead = nullptr;
    double bestScore = -1;
    for (auto* t : v)
    {
        double s = t->f.activeFrac * std::pow (10.0, t->f.loudDb / 20.0);
        if (leadName (*t)) s *= 3.0;
        if (hinted (*t, Role::Double) || hinted (*t, Role::Harmony) || hinted (*t, Role::Adlib)) s *= 0.3;
        if (s > bestScore) { bestScore = s; lead = t; }
    }
    lead->role = Role::Lead;
    lead->heard = "sung - " + juce::String (juce::roundToInt (lead->f.activeFrac * 100)) + "% of the song - the main voice";

    for (auto* t : v)
    {
        if (t == lead) continue;
        if (hinted (*t, Role::Double) || hinted (*t, Role::Harmony) || hinted (*t, Role::Adlib))
        {
            Role h; bool g; hintFor (t->name, h, g);
            t->role = h; t->confidence = std::max (t->confidence, 0.9f);
            continue;
        }
        int both = 0, mine = 0;
        std::vector<float> diffs;
        const size_t n = std::min (t->pitch.size(), lead->pitch.size());
        for (size_t i = 0; i < n; ++i)
        {
            if (t->pitch[i] <= 0) continue;
            ++mine;
            if (lead->pitch[i] > 0) { ++both; diffs.push_back (t->pitch[i] - lead->pitch[i]); }
        }
        const float overlap = mine > 0 ? (float) both / (float) mine : 0.f;
        const float loudGap = lead->f.loudDb - t->f.loudDb;
        if (overlap < 0.35f)
        {
            if ((t->f.activeFrac > 0.25f && loudGap < 3.f) || (leadName (*t) && loudGap < 8.f))
            {
                t->role = Role::Lead; t->confidence = 0.65f;
                t->heard = "sings where the main lead doesn't - a second lead";
            }
            else
            {
                t->role = Role::Adlib; t->confidence = 0.6f + 0.3f * (0.35f - overlap);
                t->heard = "short phrases in the gaps between lead lines";
            }
            continue;
        }
        float d = median (diffs);
        float a12 = std::fmod (std::abs (d), 12.f);
        a12 = std::min (a12, 12.f - a12);
        if (a12 < 0.7f)
        {
            t->role = Role::Double; t->confidence = 0.85f;
            t->heard = std::abs (d) > 6.f ? "same melody, an octave away" : "same melody as the lead, quieter";
        }
        else
        {
            t->role = Role::Harmony; t->confidence = a12 >= 2.5f && a12 <= 9.5f ? 0.8f : 0.6f;
            static const char* iv[] = { "unison", "2nd", "2nd", "3rd", "3rd", "4th", "4th", "5th", "6th", "6th", "7th", "7th" };
            t->heard = a12 < 2.5f ? juce::String ("stacked with the lead, close in pitch")
                                  : juce::String ("sung, a ") + iv[juce::jlimit (0, 11, juce::roundToInt (a12))] + (d > 0 ? " above" : " below") + " the lead";
        }
    }
}

void detectKey (SessionAnalysis& a)
{
    static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    std::array<double, 12> h {};
    double tot = 0;
    for (const auto& t : a.tracks)
    {
        if (familyOf (t.role) != Family::Vocals) continue;
        const double w = t.role == Role::Lead ? 2.0 : 1.0;
        for (float p : t.pitch) if (p > 0) { h[(size_t) (((int) std::lround (p)) % 12 + 12) % 12] += w; tot += w; }
    }
    if (tot < 50) return;
    auto corr = [&] (const float* prof, int rot)
    {
        double mh = tot / 12.0, mp = 0;
        for (int i = 0; i < 12; ++i) mp += prof[i];
        mp /= 12;
        double num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double x = h[(size_t) ((i + rot) % 12)] - mh, y = prof[i] - mp;
            num += x * y; dh += x * x; dp += y * y;
        }
        return dh > 0 && dp > 0 ? num / std::sqrt (dh * dp) : 0.0;
    };
    double best = -2;
    for (int r = 0; r < 12; ++r)
    {
        if (const double c = corr (major, r); c > best) { best = c; a.keyTonic = r; a.keyMinor = false; }
        if (const double c = corr (minor, r); c > best) { best = c; a.keyTonic = r; a.keyMinor = true; }
    }
    a.keyKnown = true;
}

// ════════════════════════════════════════════════════════════════════════════
SessionAnalysis analyse (const Session& s, std::function<bool (float)> progress)
{
    SessionAnalysis a;
    a.bpm = s.getTempo() > 0 ? s.getTempo() : 120.0;
    const double sr = s.getSampleRate() > 0 ? s.getSampleRate() : 48000.0;
    const int n = s.getNumTracks();
    for (int i = 0; i < n; ++i)
    {
        const auto& t = s.getTrack (i);
        if (progress && ! progress ((float) i / (float) std::max (1, n))) break;
        if (t.type == TrackType::Aux || t.type == TrackType::Master) continue;
        TrackAnalysis ta;
        ta.trackIndex = i;
        ta.name = t.name;
        ta.midi = t.type == TrackType::Midi;
        for (const auto& c : t.clips) if (c.file != juce::File()) { ta.clipHint = c.file.getFileNameWithoutExtension(); break; }
        if (ta.midi)
        {
            double pitchSum = 0, lenSum = 0;
            for (const auto& c : t.clips)
                for (const auto& note : c.notes) { pitchSum += note.pitch; lenSum += note.lengthBeats; ++ta.f.midiNotes; }
            if (ta.f.midiNotes > 0) { ta.f.midiAvgPitch = (float) (pitchSum / ta.f.midiNotes); ta.f.midiAvgBeats = (float) (lenSum / ta.f.midiNotes); }
            ta.f.activeFrac = ta.f.midiNotes > 0 ? 1.f : 0.f;
        }
        else
        {
            std::vector<float> L, R;
            bool stereo = false;
            if (readTrack (t, sr, 720.0, L, R, stereo))
                ta.f = measure (L, R, a.bpm, ta.env, ta.pitch);
            ta.stereo = stereo;
            a.seconds = std::max (a.seconds, (double) ta.f.seconds);
        }
        classify (ta);
        a.tracks.push_back (std::move (ta));
    }
    sortVocals (a);
    detectKey (a);
    if (progress) progress (1.f);
    return a;
}
}
