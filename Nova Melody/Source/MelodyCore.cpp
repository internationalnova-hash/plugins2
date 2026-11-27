#include "MelodyCore.h"
#include "NotesCore.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <map>

namespace NovaMelody
{
// ════════════════════════════════════════════════════════════════════════════
// Keys and scales
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    const std::vector<int> kSteps[kNumScales] = {
        { 0, 2, 4, 5, 7, 9, 11 },     // Major
        { 0, 2, 3, 5, 7, 8, 10 },     // Minor
        { 0, 2, 3, 5, 7, 8, 11 },     // Harmonic minor
        { 0, 2, 3, 5, 7, 9, 10 },     // Dorian
        { 0, 1, 3, 5, 7, 8, 10 },     // Phrygian
        { 0, 2, 4, 5, 7, 9, 10 },     // Mixolydian
        { 0, 3, 5, 7, 10 },           // Pentatonic minor
        { 0, 2, 4, 7, 9 },            // Pentatonic major
        { 0, 3, 5, 6, 7, 10 },        // Blues
    };
    const char* kScaleNames[kNumScales] = { "Major", "Minor", "Harmonic minor", "Dorian", "Phrygian", "Mixolydian",
                                            "Pentatonic minor", "Pentatonic major", "Blues" };
    const char* kNoteNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    // 7-note scale the chords are built from (pentatonic / blues borrow their parent)
    const std::vector<int>& chordScale (int scale)
    {
        if (scale == PentaMinor || scale == Blues) return kSteps[Minor];
        if (scale == PentaMajor) return kSteps[Major];
        return kSteps[scale];
    }

    struct Rng
    {
        explicit Rng (juce::uint32 seed) : r ((juce::int64) seed * 7919 + 17) {}
        float f() { return r.nextFloat(); }
        int i (int n) { return n <= 1 ? 0 : r.nextInt (n); }
        bool chance (float p) { return r.nextFloat() < p; }
        juce::Random r;
    };

    int wrapPc (int p) { return ((p % 12) + 12) % 12; }

    /// nearest pitch with the given pitch class to `near`
    int placePc (int pc, int near)
    {
        int p = near - wrapPc (near - pc);
        if (near - p > 6) p += 12;
        return p;
    }
}

const char* scaleName (int s) noexcept { return kScaleNames[juce::jlimit (0, kNumScales - 1, s)]; }
const std::vector<int>& scaleSteps (int s) noexcept { return kSteps[juce::jlimit (0, kNumScales - 1, s)]; }
const char* noteName (int pc) noexcept { return kNoteNames[wrapPc (pc)]; }
juce::String keyName (Key k) { return juce::String (noteName (k.tonic)) + " " + juce::String (scaleName (k.scale)).toLowerCase(); }
bool isMinorish (int s) noexcept { return s == Minor || s == HarmonicMinor || s == Dorian || s == Phrygian || s == PentaMinor || s == Blues; }

int scaleMask (Key k) noexcept
{
    int m = 0;
    for (int st : scaleSteps (k.scale)) m |= 1 << wrapPc (k.tonic + st);
    return m;
}

bool inKey (int pitch, Key k) noexcept { return (scaleMask (k) >> wrapPc (pitch)) & 1; }

int snapToKey (int pitch, Key k) noexcept
{
    const int m = scaleMask (k);
    for (int d = 0; d <= 6; ++d)
    {
        // prefer down on ties for minor-ish keys? keep it symmetric: try down then up
        if ((m >> wrapPc (pitch - d)) & 1) return pitch - d;
        if ((m >> wrapPc (pitch + d)) & 1) return pitch + d;
    }
    return pitch;
}

Key detectKey (const std::array<float, 12>& w, float* confidence)
{
    static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    auto corr = [&] (const float* prof, int tonic)
    {
        float mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += w[(size_t) i]; my += prof[wrapPc (i - tonic)]; }
        mx /= 12; my /= 12;
        float sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float a = w[(size_t) i] - mx, b = prof[wrapPc (i - tonic)] - my;
            sxy += a * b; sxx += a * a; syy += b * b;
        }
        return sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0.f;
    };
    Key best; float bestR = -2, second = -2;
    for (int t = 0; t < 12; ++t)
        for (int minor = 0; minor < 2; ++minor)
        {
            const float r = corr (minor ? mnr : maj, t);
            if (r > bestR) { second = bestR; bestR = r; best = { t, minor ? Minor : Major }; }
            else if (r > second) second = r;
        }
    if (confidence != nullptr) *confidence = juce::jlimit (0.f, 1.f, (bestR - second) * 4.f + bestR * 0.5f);
    return best;
}

// ════════════════════════════════════════════════════════════════════════════
// Chords
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    Chord triad (Key k, int degree, bool seventh, double start, double len)
    {
        const auto& st = chordScale (k.scale);
        const int n = (int) st.size();
        Chord c;
        c.start = start; c.len = len;
        c.root = wrapPc (k.tonic + st[(size_t) (degree % n)]);
        for (int j : { 0, 2, 4 }) c.pcs.push_back (wrapPc (k.tonic + st[(size_t) ((degree + j) % n)]));
        if (seventh) c.pcs.push_back (wrapPc (k.tonic + st[(size_t) ((degree + 6) % n)]));
        return c;
    }
}

std::vector<Chord> defaultProgression (Key k, int genre, int vibe, int bars)
{
    const bool minor = isMinorish (k.scale);
    std::vector<int> deg;
    if (minor)
    {
        if (vibe == Dark)            deg = { 0, 5, 3, 4 };      // i VI iv v
        else if (vibe == Dreamy)     deg = { 0, 6, 5, 6 };      // i VII VI VII
        else if (vibe == Hype)       deg = { 0, 0, 5, 6 };      // i i VI VII
        else if (vibe == Sexy)       deg = { 3, 4, 0, 0 };      // iv v i i
        else if (genre == Gospel)    deg = { 5, 6, 0, 3 };
        else                         deg = { 0, 5, 2, 6 };      // i VI III VII
    }
    else
    {
        if (vibe == Emotional)       deg = { 5, 3, 0, 4 };      // vi IV I V
        else if (vibe == Bright)     deg = { 0, 3, 4, 3 };      // I IV V IV
        else if (vibe == Dreamy)     deg = { 0, 2, 3, 3 };      // I iii IV IV
        else if (genre == Gospel)    deg = { 3, 2, 1, 0 };      // IV iii ii I
        else if (genre == Lofi)      deg = { 1, 4, 0, 5 };      // ii V I vi
        else                         deg = { 0, 4, 5, 3 };      // I V vi IV
    }
    const bool sevenths = genre == RnB || genre == Lofi || genre == Gospel || vibe == Sexy;
    std::vector<Chord> out;
    for (int b = 0; b < bars; ++b)
        out.push_back (triad (k, deg[(size_t) (b % (int) deg.size())], sevenths, b * 4.0, 4.0));
    return out;
}

juce::String chordName (const Chord& c)
{
    if (c.pcs.size() < 3) return noteName (c.root);
    const int third = wrapPc (c.pcs[1] - c.root), fifth = wrapPc (c.pcs[2] - c.root);
    juce::String q = third == 3 ? (fifth == 6 ? "dim" : "m") : "";
    if (c.pcs.size() >= 4) q << (wrapPc (c.pcs[3] - c.root) == 11 ? "maj7" : "7");
    return juce::String (noteName (c.root)) + q;
}

// ════════════════════════════════════════════════════════════════════════════
// Generator
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    struct Style
    {
        float lead[16];
        float bass[16];
        float legato;      // how much of the gap to the next note a lead note fills
        float triplets;    // chance of a triplet flourish in a beat (trap)
        float swingAdd;    // the genre's own swing
    };
    const Style kStyles[kNumGenres] = {
        // Trap
        { { 1,.2f,.5f,.6f, .7f,.2f,.6f,.5f, .9f,.2f,.5f,.6f, .6f,.3f,.7f,.5f },
          { 1,0,0,.3f, 0,0,.5f,0, .4f,0,.6f,.2f, 0,.3f,.3f,0 }, .75f, .22f, 0 },
        // Drill (3+3+2)
        { { 1,.15f,.3f,.85f, .35f,.15f,.85f,.3f, .9f,.15f,.3f,.85f, .35f,.15f,.85f,.4f },
          { 1,0,0,.6f, 0,0,.7f,0, .2f,0,.6f,0, 0,.4f,0,.2f }, .8f, .08f, 0 },
        // R&B
        { { 1,.3f,.6f,.5f, .6f,.4f,.7f,.5f, .8f,.3f,.6f,.5f, .6f,.4f,.7f,.5f },
          { 1,0,.3f,.2f, 0,0,.5f,.3f, .7f,0,.3f,.2f, 0,.4f,.3f,.2f }, .92f, .05f, .12f },
        // Latin Urban (melodic reggaeton)
        { { 1,.2f,.4f,.9f, .3f,.3f,.9f,.3f, .8f,.2f,.4f,.9f, .3f,.3f,.9f,.4f },
          { 1,0,0,.8f, 0,0,.6f,0, .9f,0,0,.8f, 0,0,.6f,0 }, .82f, .05f, 0 },
        // Reggaeton (dembow)
        { { 1,.15f,.3f,.9f, .3f,.15f,.9f,.2f, .9f,.15f,.3f,.9f, .3f,.15f,.9f,.2f },
          { 1,0,0,.8f, 0,0,.6f,0, .9f,0,0,.8f, 0,0,.6f,0 }, .62f, 0, 0 },
        // Afrobeats
        { { 1,.2f,.5f,.8f, .3f,.4f,.8f,.3f, .6f,.3f,.8f,.4f, .7f,.3f,.6f,.3f },
          { 1,0,0,.6f, 0,0,.6f,0, .3f,0,.7f,0, .5f,0,.4f,0 }, .72f, 0, .14f },
        // Pop
        { { 1,.2f,.7f,.3f, .8f,.2f,.7f,.3f, .9f,.2f,.7f,.3f, .8f,.2f,.7f,.3f },
          { 1,0,.3f,0, .6f,0,.4f,0, .9f,0,.3f,0, .6f,0,.4f,0 }, .85f, 0, 0 },
        // House (offbeats)
        { { .7f,.2f,.9f,.3f, .6f,.2f,.9f,.3f, .7f,.2f,.9f,.3f, .6f,.2f,.9f,.4f },
          { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,.3f }, .5f, 0, 0 },
        // Lo-fi
        { { 1,.2f,.5f,.3f, .6f,.3f,.6f,.3f, .8f,.2f,.5f,.3f, .5f,.3f,.6f,.3f },
          { 1,0,0,.3f, 0,0,.4f,0, .7f,0,.2f,0, 0,.3f,.2f,0 }, .9f, 0, .3f },
        // Gospel
        { { 1,.4f,.6f,.5f, .7f,.5f,.6f,.5f, .9f,.4f,.6f,.5f, .7f,.5f,.6f,.5f },
          { 1,0,.3f,.3f, .5f,0,.4f,.3f, .8f,0,.3f,.3f, .5f,.3f,.4f,.3f }, .8f, .05f, .08f },
    };

    struct Ctx
    {
        const GenParams& p;
        Rng rng;
        std::vector<Chord> chords;
        int centre = 67, lo = 58, hi = 79;
        const Style& st;

        Ctx (const GenParams& gp)
            : p (gp), rng (gp.seed), st (kStyles[juce::jlimit (0, kNumGenres - 1, gp.genre)])
        {
            chords = p.chords.empty() ? defaultProgression (p.key, p.genre, p.vibe, p.bars) : p.chords;
        }

        const Chord& chordAt (double beat) const
        {
            const Chord* best = &chords.front();
            for (auto& c : chords)
            {
                // chords repeat if fewer bars were heard than the clip is long
                const double span = chords.back().start + chords.back().len;
                const double b = span > 0 ? std::fmod (beat, span) : beat;
                if (b >= c.start - 1e-6 && b < c.start + c.len) return c;
                if (c.start <= b) best = &c;
            }
            return *best;
        }

        bool isChordTone (int pitch, double beat) const
        {
            const auto& c = chordAt (beat);
            return std::find (c.pcs.begin(), c.pcs.end(), wrapPc (pitch)) != c.pcs.end();
        }

        int nearestChordTone (int near, double beat) const
        {
            const auto& c = chordAt (beat);
            int best = near, bd = 99;
            for (int pc : c.pcs)
            {
                const int q = placePc (pc, near);
                if (std::abs (q - near) < bd) { bd = std::abs (q - near); best = q; }
            }
            return best;
        }

        /// move n steps along the scale from `pitch`
        int scaleStep (int pitch, int n) const
        {
            int q = p.stayInKey ? snapToKey (pitch, p.key) : pitch;
            const int dir = n > 0 ? 1 : -1;
            for (int k = 0; k < std::abs (n); ++k)
            {
                q += dir;
                while (! inKey (q, p.key)) q += dir;
            }
            return q;
        }

        int keepInRange (int q) const
        {
            while (q > hi) q -= 12;
            while (q < lo) q += 12;
            return q;
        }
    };

    struct Slot { double start, len; bool strong; };

    float densityScale (const GenParams& p)
    {
        float d = 0.2f + 1.25f * p.density;
        if (p.vibe == Hype) d *= 1.3f;
        if (p.vibe == Dreamy) d *= 0.7f;
        if (p.vibe == Sexy) d *= 0.8f;
        if (p.leaveSpace) d *= 0.6f;
        return d;
    }

    /// onsets for `bars` bars starting at `from` (beats), from the genre groove
    std::vector<Slot> makeRhythm (Ctx& c, double from, int bars, bool bass)
    {
        const auto& w = bass ? c.st.bass : c.st.lead;
        const float dens = densityScale (c.p);
        std::vector<double> on;
        for (int b = 0; b < bars; ++b)
        {
            int count = 0;
            for (int s = 0; s < 16; ++s)
            {
                // "Rhythm" low = straight (on the beats/8ths), high = the genre's own syncopation
                const float straight = (s % 4 == 0) ? 1.f : (s % 2 == 0 ? .45f : .08f);
                const float wt = bass ? w[s] : juce::jmap (c.p.rhythm, straight, w[s]);
                float prob = wt * dens * 0.55f;
                if (bass && s == 0) prob = 1.f;
                if (c.p.leaveSpace && ! bass && (b % 2 == 1) && s >= 8) prob *= 0.25f;   // breathe at phrase ends
                if (c.rng.chance (prob)) { on.push_back (from + b * 4.0 + s * 0.25); ++count; }
            }
            if (count < 2 && ! bass)
            {
                on.push_back (from + b * 4.0);
                on.push_back (from + b * 4.0 + (c.rng.chance (.5f) ? 1.5 : 2.0));
            }
        }
        std::sort (on.begin(), on.end());
        on.erase (std::unique (on.begin(), on.end(), [] (double a, double b) { return std::abs (a - b) < 1e-6; }), on.end());

        // trap-style triplet flourishes: a beat turns into three notes
        if (! bass && c.st.triplets > 0)
            for (int beat = 0; beat < bars * 4; ++beat)
                if (c.rng.chance (c.st.triplets * (0.4f + c.p.density)))
                {
                    const double b0 = from + beat;
                    on.erase (std::remove_if (on.begin(), on.end(), [&] (double t) { return t >= b0 - 1e-6 && t < b0 + 1 - 1e-6; }), on.end());
                    for (int k = 0; k < 3; ++k) on.push_back (b0 + k / 3.0);
                    std::sort (on.begin(), on.end());
                }

        std::vector<Slot> out;
        const double end = from + bars * 4.0;
        for (size_t i = 0; i < on.size(); ++i)
        {
            const double next = i + 1 < on.size() ? on[i + 1] : end;
            double legato = c.st.legato;
            if (c.p.vibe == Hype) legato *= 0.65;
            if (c.p.vibe == Emotional || c.p.vibe == Dreamy || c.p.vibe == Sexy) legato = std::min (1.0, legato + 0.12);
            double len = std::max (0.125, (next - on[i]) * legato);
            if (i + 1 == on.size()) len = std::min (len, std::max (0.25, end - on[i] - 0.25));
            const double inBeat = std::fmod (on[i] - from, 1.0);
            out.push_back ({ on[i], len, inBeat < 1e-6 || len >= 0.74 });
        }
        return out;
    }

    /// pitches for one phrase along an arch, chord tones on strong slots, scale steps between
    std::vector<MNote> pitchPhrase (Ctx& c, const std::vector<Slot>& slots, int& prev)
    {
        std::vector<MNote> out;
        const int n = (int) slots.size();
        const float amp = c.p.vibe == Dark ? -2.5f : (c.p.vibe == Dreamy || c.p.vibe == Bright ? 5.f : 3.5f);
        const float leap = c.p.vibe == Dreamy || c.p.vibe == Bright ? .25f : (c.p.vibe == Hype ? .05f : .1f);
        const float repeatP = c.p.vibe == Hype ? .35f : (c.p.genre == Reggaeton || c.p.genre == LatinUrban ? .2f : .1f);
        for (int i = 0; i < n; ++i)
        {
            const auto& s = slots[(size_t) i];
            const float arch = (float) c.centre + amp * std::sin (juce::MathConstants<float>::pi * (n > 1 ? (float) i / (float) (n - 1) : 0.5f));
            const int target = juce::roundToInt (0.55f * (float) prev + 0.45f * arch);
            int q;
            const bool last = i == n - 1;
            if (last)
            {
                // end the phrase at rest: root or third of the chord
                const auto& ch = c.chordAt (s.start);
                const int pc = ch.pcs[(size_t) (c.rng.chance (.6f) ? 0 : 1)];
                q = placePc (pc, prev);
            }
            else if (i > 0 && c.rng.chance (repeatP))
                q = prev;
            else if (s.strong)
                q = c.nearestChordTone (target, s.start);
            else
            {
                const int dir = target > prev ? 1 : (target < prev ? -1 : (c.rng.chance (.5f) ? 1 : -1));
                const int size = c.rng.chance (leap) ? 3 + c.rng.i (2) : 1 + (c.rng.chance (.25f) ? 1 : 0);
                q = c.scaleStep (prev, dir * size);
                // R&B / sexy: the odd chromatic approach from below
                if (! c.p.stayInKey && (c.p.genre == RnB || c.p.vibe == Sexy) && c.rng.chance (.1f)) q = c.nearestChordTone (q, s.start) - 1;
            }
            q = c.keepInRange (q);
            if (c.p.stayInKey && ! inKey (q, c.p.key)) q = snapToKey (q, c.p.key);
            MNote m;
            m.start = s.start; m.len = s.len; m.pitch = q;
            m.vel = juce::jlimit (40, 127, (s.strong ? 104 : 86) + c.rng.i (13) - 6 + (c.p.vibe == Hype ? 8 : 0));
            out.push_back (m);
            prev = q;
        }
        return out;
    }

    /// a phrase moved to a new place: same rhythm and shape, re-fitted to the chords there
    std::vector<MNote> restate (Ctx& c, const std::vector<MNote>& src, double shift, float vary)
    {
        std::vector<MNote> out;
        if (src.empty()) return out;
        const int firstNew = c.nearestChordTone (src.front().pitch, src.front().start + shift);
        const int delta = firstNew - src.front().pitch;
        const int keepUntil = (int) std::lround ((double) src.size() * (1.0 - vary));
        int prev = firstNew;
        for (int i = 0; i < (int) src.size(); ++i)
        {
            auto m = src[(size_t) i];
            m.start += shift;
            m.locked = false;
            if (i < keepUntil)
            {
                m.pitch = src[(size_t) i].pitch + delta;
                if (c.p.stayInKey) m.pitch = snapToKey (m.pitch, c.p.key);
                if (std::fmod (m.start, 1.0) < 1e-6 && ! c.isChordTone (m.pitch, m.start)) m.pitch = c.nearestChordTone (m.pitch, m.start);
            }
            else
            {
                const int dir = c.rng.chance (.5f) ? 1 : -1;
                m.pitch = i == (int) src.size() - 1 ? c.nearestChordTone (prev, m.start) : c.scaleStep (prev, dir * (1 + c.rng.i (2)));
            }
            m.pitch = c.keepInRange (m.pitch);
            prev = m.pitch;
            out.push_back (m);
        }
        return out;
    }

    std::vector<MNote> writeLine (Ctx& c)
    {
        std::vector<MNote> out;
        const int phraseBars = c.p.part == Hook ? 1 : 2;
        const int phrases = std::max (1, (c.p.bars + phraseBars - 1) / phraseBars);
        int prev = c.nearestChordTone (c.centre, 0);
        std::vector<std::vector<MNote>> made;
        for (int ph = 0; ph < phrases; ++ph)
        {
            const double from = ph * phraseBars * 4.0;
            const int barsHere = std::min (phraseBars, c.p.bars - ph * phraseBars);
            std::vector<MNote> phrase;
            // which earlier phrase to echo: A A' B A'' (Hook: call, response, call, response...)
            const float rep = c.p.repetition;
            int echo = -1;
            float vary = 0.3f;
            if (c.p.part == Hook)
            {
                if (ph >= 2) { echo = ph % 2; vary = 1.f - rep; }
                else if (ph == 1) { echo = 0; vary = 0.5f; }    // response: same start, new answer
            }
            else if (ph > 0)
            {
                const int pos = ph % 4;
                if (pos == 1 && c.rng.chance (0.35f + rep)) { echo = ph - 1; vary = 0.5f - 0.35f * rep; }
                else if (pos == 3 && c.rng.chance (0.3f + rep)) { echo = ph - 3; vary = 0.45f - 0.3f * rep; }
                else if (pos == 0 && c.rng.chance (rep)) { echo = ph - 4; vary = 0.25f; }
            }
            if (echo >= 0 && echo < (int) made.size() && ! made[(size_t) echo].empty())
                phrase = restate (c, made[(size_t) echo], from - echo * phraseBars * 4.0, juce::jlimit (0.f, 1.f, vary));
            else
                phrase = pitchPhrase (c, makeRhythm (c, from, barsHere, false), prev);
            if (! phrase.empty()) prev = phrase.back().pitch;
            made.push_back (phrase);
            out.insert (out.end(), phrase.begin(), phrase.end());
        }
        return out;
    }

    std::vector<MNote> writeBass (Ctx& c, bool is808)
    {
        std::vector<MNote> out;
        std::vector<MNote> firstTwo;
        for (int b = 0; b < c.p.bars; ++b)
        {
            std::vector<Slot> slots;
            if (b >= 2 && c.rng.chance (0.4f + 0.55f * c.p.repetition) && ! firstTwo.empty())
            {
                // same rhythm as the first two bars, on this bar's chord
                for (auto& m : firstTwo)
                    if (m.start >= (b % 2) * 4.0 && m.start < (b % 2) * 4.0 + 4.0)
                        slots.push_back ({ b * 4.0 + std::fmod (m.start, 4.0), m.len, std::fmod (m.start, 1.0) < 1e-6 });
            }
            if (slots.empty()) slots = makeRhythm (c, b * 4.0, 1, true);
            const auto& ch = c.chordAt (b * 4.0);
            const auto& next = c.chordAt (b * 4.0 + 4.0);
            const int root = placePc (ch.root, c.centre);
            for (size_t i = 0; i < slots.size(); ++i)
            {
                auto s = slots[i];
                int q = root;
                const bool lastInBar = i + 1 == slots.size();
                if (i > 0 && ! lastInBar && c.rng.chance (0.15f + 0.25f * c.p.rhythm))
                    q = c.rng.chance (.5f) ? root + 12 : placePc (ch.pcs.size() > 2 ? ch.pcs[2] : ch.root, root + 5);
                if (lastInBar && i > 0 && next.root != ch.root && c.rng.chance (.35f))
                    q = c.scaleStep (placePc (next.root, root), next.root > ch.root ? -1 : 1);   // walk into the next chord
                q = c.keepInRange (q);
                MNote m;
                m.start = s.start;
                const double nextOn = i + 1 < slots.size() ? slots[i + 1].start : b * 4.0 + 4.0;
                m.len = std::max (0.125, (nextOn - s.start) * (is808 ? 0.98 : (c.p.genre == House ? 0.5 : 0.82)));
                m.pitch = q;
                m.vel = juce::jlimit (40, 127, (s.start == b * 4.0 ? 112 : 96) + c.rng.i (9) - 4);
                if (is808 && ! out.empty() && out.back().pitch != q && c.rng.chance (0.35f)
                    && std::abs (out.back().start + out.back().len - m.start) < 0.13)
                    m.glide = true;
                out.push_back (m);
                if (b < 2) firstTwo.push_back (m);
            }
        }
        return out;
    }

    std::vector<MNote> writeArp (Ctx& c)
    {
        std::vector<MNote> out;
        const double rate = c.p.density < 0.35f ? 0.5 : 0.25;
        const int pattern = c.p.vibe == Dreamy ? 1 : (c.p.vibe == Hype ? 2 : c.rng.i (3));   // 0 up, 1 up-down, 2 broken
        for (double t = 0; t < c.p.bars * 4.0 - 1e-6; t += rate)
        {
            const auto& ch = c.chordAt (t);
            std::vector<int> tones;
            for (int oct = 0; oct < 2; ++oct)
                for (int pc : ch.pcs) tones.push_back (placePc (pc, c.centre - 5) + 12 * oct);
            std::sort (tones.begin(), tones.end());
            const int k = (int) std::lround (std::fmod (t, 4.0) / rate);
            const int n = (int) tones.size();
            int idx = 0;
            if (pattern == 0) idx = k % n;
            else if (pattern == 1) { const int cyc = 2 * n - 2; const int m = k % std::max (1, cyc); idx = m < n ? m : cyc - m; }
            else { static const int br[4] = { 0, 2, 1, 3 }; idx = (br[k % 4] + (k / 4)) % n; }
            MNote m;
            m.start = t; m.len = rate * 0.9; m.pitch = c.keepInRange (tones[(size_t) idx]);
            m.vel = std::fmod (t, 1.0) < 1e-6 ? 104 : 82;
            out.push_back (m);
        }
        return out;
    }

    std::vector<MNote> writeCounter (Ctx& c)
    {
        if (c.p.against.empty()) return writeLine (c);   // nothing to answer yet: a quieter line underneath
        std::vector<MNote> out;
        auto sounding = [&] (double t) -> const MNote*
        {
            for (auto& m : c.p.against) if (t >= m.start - 1e-6 && t < m.start + m.len - 1e-6) return &m;
            return nullptr;
        };
        int prevAgainst = c.p.against.front().pitch;
        for (double t = 0; t < c.p.bars * 4.0 - 1e-6; t += 0.5)
        {
            const auto* a = sounding (t);
            if (a != nullptr) prevAgainst = a->pitch;
            const bool gap = a == nullptr;
            if (! gap && ! (a->len >= 1.5 && std::abs (a->start - t) < 1e-6)) continue;   // answer in gaps, or under long notes
            if (! c.rng.chance (gap ? 0.45f + 0.4f * c.p.density : 0.5f)) continue;
            // a third or sixth below what the melody last did, contrary-ish motion
            int q = c.scaleStep (prevAgainst, c.rng.chance (.6f) ? -2 : -5);
            q = c.nearestChordTone (q, t);
            q = c.keepInRange (q);
            MNote m; m.start = t; m.len = gap ? 0.45 : 1.4; m.pitch = q; m.vel = 80 + c.rng.i (12);
            out.push_back (m);
        }
        return out;
    }

    void applySwing (std::vector<MNote>& notes, float amount)
    {
        if (amount <= 0.001f) return;
        for (auto& m : notes)
        {
            if (m.locked) continue;
            const double pos16 = m.start * 4.0;
            if (std::abs (pos16 - std::round (pos16)) < 1e-6 && ((long long) std::llround (pos16)) % 2 == 1)
                m.start += amount * 0.0833;       // full swing = triplet feel
        }
    }
}

std::vector<MNote> generate (const GenParams& gp)
{
    GenParams p = gp;
    p.bars = juce::jlimit (1, 32, p.bars);
    Ctx c (p);

    switch (p.part)
    {
        case Bassline: c.centre = 38; c.lo = 28; c.hi = 50; break;
        case Bass808:  c.centre = 33; c.lo = 24; c.hi = 45; break;
        case Arp:      c.centre = p.reg == Low ? 60 : (p.reg == High ? 76 : 68); c.lo = c.centre - 10; c.hi = c.centre + 14; break;
        case Counter:  c.centre = p.reg == Low ? 55 : (p.reg == High ? 67 : 60); c.lo = c.centre - 9; c.hi = c.centre + 8; break;
        default:       c.centre = p.reg == Low ? 60 : (p.reg == High ? 74 : 67); c.lo = c.centre - 9; c.hi = c.centre + 12; break;
    }
    if (p.part != Bassline && p.part != Bass808)
    {
        const int shift = p.vibe == Dark ? -2 : (p.vibe == Dreamy || p.vibe == Bright ? 3 : (p.vibe == Hype ? 2 : 0));
        c.centre += shift; c.lo += shift; c.hi += shift;
    }

    std::vector<MNote> notes;
    switch (p.part)
    {
        case Bassline: notes = writeBass (c, false); break;
        case Bass808:  notes = writeBass (c, true); break;
        case Arp:      notes = writeArp (c); break;
        case Counter:  notes = writeCounter (c); break;
        default:       notes = writeLine (c); break;
    }
    applySwing (notes, juce::jlimit (0.f, 1.f, p.swing + c.st.swingAdd));

    // locked notes stay exactly; generated notes that would play over them go
    if (! p.keep.empty())
    {
        notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const MNote& m)
        {
            for (auto& k : p.keep)
                if (m.start < k.start + k.len - 1e-6 && k.start < m.start + m.len - 1e-6) return true;
            return false;
        }), notes.end());
        for (auto k : p.keep) { k.locked = true; notes.push_back (k); }
    }

    const double end = p.bars * 4.0;
    notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const MNote& m) { return m.start >= end - 1e-6 || m.start < 0; }), notes.end());
    for (auto& m : notes) m.len = std::min (m.len, end - m.start);
    std::sort (notes.begin(), notes.end(), [] (const MNote& a, const MNote& b) { return a.start < b.start; });
    return notes;
}

const char* genreName (int g) noexcept
{
    static const char* n[kNumGenres] = { "Trap", "Drill", "R&B", "Latin Urban", "Reggaeton", "Afrobeats", "Pop", "House", "Lo-fi", "Gospel" };
    return n[juce::jlimit (0, kNumGenres - 1, g)];
}
const char* vibeName (int v) noexcept
{
    static const char* n[kNumVibes] = { "Dark", "Emotional", "Dreamy", "Bright", "Hype", "Sexy" };
    return n[juce::jlimit (0, kNumVibes - 1, v)];
}
const char* partName (int p) noexcept
{
    static const char* n[kNumParts] = { "Lead melody", "Bassline", "808", "Counter-melody", "Arp", "Hook" };
    return n[juce::jlimit (0, kNumParts - 1, p)];
}

// ════════════════════════════════════════════════════════════════════════════
// Humming
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.f;
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    }

    std::vector<MNote> chordsUnder (const std::vector<MNote>& hummed, Key k, double length)
    {
        std::vector<MNote> out;
        const int bars = std::max (1, (int) std::ceil (length / 4.0 - 1e-6));
        int prevTop = 60;
        for (int b = 0; b < bars; ++b)
        {
            std::array<float, 12> w {};
            for (auto& m : hummed)
            {
                const double a = std::max (m.start, b * 4.0), e = std::min (m.start + m.len, b * 4.0 + 4.0);
                if (e > a) w[(size_t) wrapPc (m.pitch)] += (float) (e - a) * (std::fmod (m.start, 2.0) < 1e-6 ? 1.5f : 1.f);
            }
            int bestDeg = 0; float bestScore = -1;
            for (int d = 0; d < 7; ++d)
            {
                const auto ch = triad (k, d, false, 0, 4);
                float s = 0;
                for (int pc : ch.pcs) s += w[(size_t) pc];
                s += w[(size_t) ch.root] * 0.5f;
                if (d == 0) s += 0.01f;                 // tie: the home chord
                if (s > bestScore) { bestScore = s; bestDeg = d; }
            }
            const auto ch = triad (k, bestDeg, false, b * 4.0, 4.0);
            // voice it close, around middle C, moving as little as possible
            int base = placePc (ch.root, 50);
            std::vector<int> v;
            for (int pc : ch.pcs) { int q = placePc (pc, base + 4); while (q < base) q += 12; v.push_back (q); }
            std::sort (v.begin(), v.end());
            if (std::abs (v.back() - prevTop) > 7) for (auto& q : v) q += (v.back() > prevTop ? -12 : 12);
            while (v.front() < 45) for (auto& q : v) q += 12;
            prevTop = v.back();
            for (int q : v) { MNote m; m.start = b * 4.0; m.len = 3.9; m.pitch = q; m.vel = 84; out.push_back (m); }
        }
        return out;
    }
}

void remakeFromHeard (HumResult& r, const HumParams& p)
{
    r.notes.clear();
    if (! r.ok) return;
    const Key k = r.key;

    // 1. clean notes: drop blips, keep timing / pitch as hummed (or pulled to key / grid)
    std::vector<MNote> line;
    for (size_t i = 0; i < r.heard.size(); ++i)
    {
        auto m = r.heard[i];
        const float centre = i < r.heardCentre.size() ? r.heardCentre[i] : (float) m.pitch;
        const double ms = m.len * 60000.0 / std::max (1.0, p.bpm);
        int q = juce::roundToInt (centre);
        if (! inKey (q, k))
        {
            const int s = snapToKey (q, k);
            if (std::abs (centre - (float) s) <= 0.5f + p.pitchSnap * 1.6f) q = s;
        }
        m.pitch = q;
        if (p.timeSnap > 0.f)
        {
            const double g = 0.25, a = m.start, e = m.start + m.len;
            const double a2 = a + (std::round (a / g) * g - a) * p.timeSnap;
            const double e2 = e + (std::round (e / g) * g - e) * p.timeSnap;
            m.start = std::max (0.0, a2);
            m.len = std::max (0.0625, e2 - m.start);
        }
        if (ms < p.minNoteMs)
        {
            // a blip: fold it into the previous note when it's the same pitch, otherwise drop it
            if (! line.empty() && line.back().pitch == q) line.back().len = std::max (line.back().len, m.start + m.len - line.back().start);
            continue;
        }
        if (! line.empty() && line.back().pitch == q && m.start - (line.back().start + line.back().len) < 0.03)
        {
            line.back().len = m.start + m.len - line.back().start;      // a scoop split the same note in two
            continue;
        }
        line.push_back (m);
    }
    if (line.empty()) return;

    // 2. range: a hummed bassline goes down into bass range, etc.
    std::vector<float> ps;
    for (auto& m : line) ps.push_back ((float) m.pitch);
    const float med = median (ps);
    int shift = 0;
    if (p.autoOctave)
    {
        const float target = p.as == AsBass ? 40.f : (p.as == As808 ? 36.f : 67.f);
        if (p.as != AsMelody || med < 55.f || med > 80.f)
            shift = 12 * juce::roundToInt ((target - med) / 12.f);
    }
    shift += 12 * p.octave;
    for (auto& m : line) m.pitch = juce::jlimit (12, 115, m.pitch + shift);

    if (p.as == AsChords)
    {
        r.notes = chordsUnder (line, k, r.lengthBeats);
        return;
    }
    if (p.as == As808)
        for (size_t i = 0; i < line.size(); ++i)
        {
            if (i + 1 < line.size())
            {
                const double gap = line[i + 1].start - (line[i].start + line[i].len);
                if (gap < 0.25) line[i].len = line[i + 1].start - line[i].start;     // 808s ring into each other
                if (gap < 0.06 && line[i + 1].pitch != line[i].pitch) line[i + 1].glide = true;
            }
        }
    for (auto& m : line) m.vel = p.as == AsMelody ? 96 : 108;
    r.notes = line;
}

HumResult fromHum (const float* x, int n, double sr, const HumParams& p)
{
    HumResult r;
    if (x == nullptr || n < (int) (sr * 0.3))
    {
        r.message = "That take is too short - hum for at least a second.";
        return r;
    }
    const auto track = NovaNotes::trackPitch (x, n, sr);
    auto notes = NovaNotes::segmentNotes (track);
    // ignore very quiet "notes" (room noise, breaths)
    float loud = -120.f;
    for (float d : track.db) loud = std::max (loud, d);
    notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const NovaNotes::Note& nt)
    {
        float m = -120.f;
        for (int f = nt.start; f < nt.end; ++f) m = std::max (m, track.db[(size_t) f]);
        return m < loud - 30.f;
    }), notes.end());
    if (notes.empty())
    {
        r.message = "No sung notes were heard - hum a little louder or closer to the mic.";
        return r;
    }

    const double secPerFrame = track.hop / sr;
    const double beatsPerSec = std::max (1.0, p.bpm) / 60.0;
    double offset;                               // beats to subtract so the line starts at the clip start
    if (p.capturedAtBeat >= 0)
        offset = -(std::fmod (p.capturedAtBeat, 4.0));                     // keep where it fell in the bar
    else
        offset = notes.front().start * secPerFrame * beatsPerSec;   // the first hummed note starts the clip

    for (auto& nt : notes)
    {
        MNote m;
        m.start = std::max (0.0, nt.start * secPerFrame * beatsPerSec - offset);
        m.len = std::max (0.03, (nt.end - nt.start) * secPerFrame * beatsPerSec);
        m.pitch = juce::roundToInt (nt.center);
        m.vel = 100;
        r.heard.push_back (m);
        r.heardCentre.push_back (nt.center);
    }
    for (int f = 0; f < track.size(); f += 2)
        if (track.midi[(size_t) f] > 0.f)
        {
            const double b = f * secPerFrame * beatsPerSec - offset;
            if (b < 0) continue;
            r.curveBeat.push_back ((float) b);
            r.curvePitch.push_back (track.midi[(size_t) f]);
        }

    const auto& last = r.heard.back();
    r.lengthBeats = std::max (4.0, std::ceil ((last.start + last.len) / 4.0 - 1e-6) * 4.0);
    if (p.autoKey)
    {
        const auto hk = NovaNotes::detectKey (notes, track);
        r.key = { hk.tonic, hk.minor ? Minor : Major };
        r.keyWasHeard = true;
        r.keyConfidence = hk.confidence;
        std::array<bool, 12> used {};
        for (auto& m : r.heard) used[(size_t) wrapPc (m.pitch)] = true;
        if (std::count (used.begin(), used.end(), true) < 4) r.keyConfidence = std::min (r.keyConfidence, 0.2f);   // 2-3 notes can't name a key
    }
    else
        r.key = p.key;
    r.ok = true;
    remakeFromHeard (r, p);
    if (r.notes.empty())
    {
        r.ok = false;
        r.message = "Only very short sounds were heard - hum the notes a little longer.";
    }
    return r;
}

std::vector<MNote> makeItGroove (const std::vector<MNote>& notes, int genre, double length, bool bass, juce::uint32 seed)
{
    if (notes.empty()) return notes;
    const auto& st = kStyles[juce::jlimit (0, kNumGenres - 1, genre)];
    const auto& w = bass ? st.bass : st.lead;
    Rng rng (seed);
    std::vector<MNote> out;
    double minNext = 0;
    for (auto m : notes)
    {
        // move each onset to the strongest groove step close by (within an eighth)
        double best = m.start; float bestW = -1;
        for (int d = -2; d <= 2; ++d)
        {
            const double t = std::round (m.start * 4.0) / 4.0 + d * 0.25;
            if (t < minNext - 1e-6 || t < 0) continue;
            const int step = ((int) std::llround (t * 4.0)) % 16;
            const float score = w[step] - 0.12f * (float) std::abs (d) + rng.f() * 0.05f;
            if (score > bestW) { bestW = score; best = t; }
        }
        m.start = std::max (best, minNext);
        minNext = m.start + 0.25;
        out.push_back (m);
    }
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double next = i + 1 < out.size() ? out[i + 1].start : length;
        out[i].len = std::max (0.125, (next - out[i].start) * (bass ? 0.85 : st.legato));
    }
    return out;
}

std::vector<MNote> answerIt (const std::vector<MNote>& hummed, Key k, int genre, double length, juce::uint32 seed)
{
    std::vector<MNote> out = hummed;
    if (hummed.empty()) return out;
    Rng rng (seed);
    std::vector<float> ps;
    for (auto& m : hummed) ps.push_back ((float) m.pitch);
    const int med = juce::roundToInt (median (ps));
    const int home = med < 55 ? med + 24 : med;          // answer a bassline up in melody range
    const auto& st = kStyles[juce::jlimit (0, kNumGenres - 1, genre)];

    // gaps of a beat or more (and the space after the last note)
    std::vector<std::pair<double, double>> gaps;
    double t = 0;
    for (auto& m : hummed)
    {
        if (m.start - t >= 1.0) gaps.push_back ({ t, m.start });
        t = std::max (t, m.start + m.len);
    }
    if (length - t >= 1.0) gaps.push_back ({ t, length });

    size_t idx = 0;
    for (auto [a, b] : gaps)
    {
        // the last few hummed notes before this gap, mirrored: up becomes down
        std::vector<int> lastFew;
        for (auto& m : hummed) if (m.start < a) lastFew.push_back (m.pitch);
        if (lastFew.size() > 4) lastFew.erase (lastFew.begin(), lastFew.end() - 4);
        int prev = lastFew.empty() ? home : lastFew.back() + (home - med);
        int prevSrc = lastFew.empty() ? 0 : lastFew.front();
        const double start = std::ceil ((a + 0.25) * 4.0) / 4.0;
        for (double s = start; s < b - 0.25; s += 0.25)
        {
            const int step = ((int) std::llround (s * 4.0)) % 16;
            if (! rng.chance (st.lead[step] * 0.55f)) continue;
            int q;
            if (! lastFew.empty() && idx < lastFew.size())
            {
                q = prev - (lastFew[idx] - prevSrc);
                prevSrc = lastFew[idx++];
            }
            else
                q = prev + (rng.chance (.5f) ? -2 : 2);
            q = snapToKey (q, k);
            while (q > home + 9) q -= 12;
            while (q < home - 9) q += 12;
            MNote m; m.start = s; m.len = 0.4; m.pitch = q; m.vel = 88;
            out.push_back (m);
            prev = q;
        }
        idx = 0;
    }
    std::sort (out.begin(), out.end(), [] (const MNote& x, const MNote& y) { return x.start < y.start; });
    return out;
}

// ════════════════════════════════════════════════════════════════════════════
// Play
// ════════════════════════════════════════════════════════════════════════════
int stepPitch (Key k, int step, int octave) noexcept
{
    const auto& st = scaleSteps (k.scale);
    const int n = (int) st.size();
    const int base = 60 + wrapPc (k.tonic) - (wrapPc (k.tonic) > 5 ? 12 : 0);   // the tonic nearest middle C (F#3..F4)
    const int oct = step >= 0 ? step / n : -((-step + n - 1) / n);
    const int idx = step - oct * n;
    return juce::jlimit (0, 127, base + st[(size_t) idx] + 12 * (oct + octave));
}

int padPitch (Key k, int pad, int octave) noexcept
{
    // 8 pads: the scale's notes up to the tonic an octave higher (5-note scales keep climbing)
    const int n = (int) scaleSteps (k.scale).size();
    return stepPitch (k, pad == 7 && n == 7 ? 7 : pad, octave);
}

std::vector<int> chordPadClasses (Key k, int degree)
{
    return triad (k, juce::jlimit (0, 6, degree), false, 0, 4).pcs;
}

std::vector<int> chordPadPitches (Key k, int degree, int octave)
{
    const auto pcs = chordPadClasses (k, degree);
    int root = 48 + pcs[0];
    if (root > 54) root -= 12;                       // roots from F#2 to F3: warm, not muddy
    std::vector<int> out { root };
    for (size_t i = 1; i < pcs.size(); ++i) { int q = root + wrapPc (pcs[i] - pcs[0]); out.push_back (q); }
    out.push_back (root + 12);                       // the root on top too: fuller
    for (auto& q : out) q = juce::jlimit (0, 127, q + 12 * octave);
    return out;
}

int whiteKeyStep (int midiNote) noexcept
{
    static const int white[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };   // black keys = the white key below
    const int oct = (int) std::floor ((midiNote - 60) / 12.0);
    return oct * 7 + white[wrapPc (midiNote)];
}

// ════════════════════════════════════════════════════════════════════════════
// MIDI file
// ════════════════════════════════════════════════════════════════════════════
juce::MemoryBlock toMidiFile (const std::vector<MNote>& notes, double bpm, const juce::String& name)
{
    constexpr int tpq = 960;
    juce::MidiMessageSequence seq;
    auto meta = juce::MidiMessage::textMetaEvent (3, name);
    meta.setTimeStamp (0);
    seq.addEvent (meta);
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / std::max (20.0, bpm)));
    tempo.setTimeStamp (0);
    seq.addEvent (tempo);
    auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    ts.setTimeStamp (0);
    seq.addEvent (ts);
    for (auto& m : notes)
    {
        auto on = juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, m.pitch), (juce::uint8) juce::jlimit (1, 127, m.vel));
        on.setTimeStamp (std::round (m.start * tpq));
        auto off = juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, m.pitch));
        off.setTimeStamp (std::round ((m.start + m.len) * tpq) - 1);
        seq.addEvent (on);
        seq.addEvent (off);
    }
    seq.updateMatchedPairs();
    juce::MidiFile f;
    f.setTicksPerQuarterNote (tpq);
    f.addTrack (seq);
    juce::MemoryBlock mb;
    juce::MemoryOutputStream os (mb, false);
    f.writeTo (os, 1);
    os.flush();
    return mb;
}
}
