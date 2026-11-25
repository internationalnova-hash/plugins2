#include "NovaAiHarmony.h"

#include <cmath>
#include <cstdlib>

namespace
{
    inline int mod12 (int x) noexcept { return ((x % 12) + 12) % 12; }
}

void NovaAiHarmony::reset() noexcept
{
    weight.fill (0.f);
    curNote = 0;
    curDur = sinceChord = 0.0;
    chordDeg = -1;
    notes.fill (0);
    prev.fill (0);
    chordMeter.store (-1);
    degreeMeter.store (-1);
    for (auto& h : historyMeter) h.store (-1);
}

void NovaAiHarmony::buildScale (const Settings& s) noexcept
{
    keyPc = mod12 (s.keyPc);
    int degs[12]; int n = 0;
    for (int i = 0; i < 12; ++i) if (s.stepMask & (1 << i)) degs[n++] = i;
    static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
    if (n == 7) for (int i = 0; i < 7; ++i) scale[(size_t) i] = degs[i];
    else
    {
        // chromatic / pentatonic / blues: harmonise in the parent major or minor
        const bool m = (s.stepMask & (1 << 3)) && ! (s.stepMask & (1 << 4));
        for (int i = 0; i < 7; ++i) scale[(size_t) i] = m ? minor[i] : major[i];
    }
    minorKey = scale[2] == 3;
}

int NovaAiHarmony::tonePc (int off) const noexcept
{
    return mod12 (keyPc + scale[(size_t) ((chordDeg + off) % 7)]);
}

int NovaAiHarmony::quality (int deg) const noexcept
{
    const int third = mod12 (scale[(size_t) ((deg + 2) % 7)] - scale[(size_t) deg]);
    const int fifth = mod12 (scale[(size_t) ((deg + 4) % 7)] - scale[(size_t) deg]);
    if (fifth == 6) return qDim;
    return third == 4 ? qMajor : qMinor;
}

bool NovaAiHarmony::allowedPc (int pc, const Settings& s, bool& isColour) const noexcept
{
    isColour = false;
    if (chordDeg < 0) return false;
    if (pc == tonePc (0) || pc == tonePc (2) || pc == tonePc (4)) return true;
    if (s.colour >= 0.4f && pc == tonePc (6)) { isColour = true; return true; }   // 7th
    if (s.colour >= 0.75f && pc == tonePc (1)) { isColour = true; return true; }  // 9th
    return false;
}

void NovaAiHarmony::chooseChord (int lead, const Settings& s) noexcept
{
    // how likely each chord is in a song, before hearing anything
    static const float priorMajor[7] = { 0.6f, 0.3f, 0.1f, 0.5f, 0.5f, 0.4f, -0.6f };
    static const float priorMinor[7] = { 0.6f, -0.6f, 0.35f, 0.5f, 0.4f, 0.5f, 0.4f };
    const int lpc = mod12 (lead);

    float score[7];
    for (int d = 0; d < 7; ++d)
    {
        const int r = mod12 (keyPc + scale[(size_t) d]), t = mod12 (keyPc + scale[(size_t) ((d + 2) % 7)]),
                  f = mod12 (keyPc + scale[(size_t) ((d + 4) % 7)]), sv = mod12 (keyPc + scale[(size_t) ((d + 6) % 7)]),
                  nn = mod12 (keyPc + scale[(size_t) ((d + 1) % 7)]);
        auto inTriad = [&] (int pc) { return pc == r || pc == t || pc == f; };
        auto inColour = [&] (int pc) { return (s.colour >= 0.4f && pc == sv) || (s.colour >= 0.75f && pc == nn); };
        float sc = minorKey ? priorMinor[d] : priorMajor[d];
        for (int pc = 0; pc < 12; ++pc)
            if (weight[(size_t) pc] > 0.f)
                sc += weight[(size_t) pc] * (inTriad (pc) ? 1.f : inColour (pc) ? 0.4f : -0.5f);
        sc += inTriad (lpc) ? 2.5f : inColour (lpc) ? 1.2f : -3.f;   // the note being sung right now matters most
        score[d] = sc;
    }
    int best = 0;
    for (int d = 1; d < 7; ++d) if (score[d] > score[best]) best = d;

    bool change = chordDeg < 0;
    if (! change)
    {
        bool colour = false;
        const bool fits = allowedPc (lpc, s, colour);
        // 9ths and 6ths over the chord are sweet tensions, not reasons to change chord
        const int root = tonePc (0);
        const int iv = mod12 (lpc - root);
        const bool semitoneRub = mod12 (lpc - 1) == tonePc (0) || mod12 (lpc - 1) == tonePc (2) || mod12 (lpc - 1) == tonePc (4)
                              || mod12 (lpc + 1) == tonePc (0) || mod12 (lpc + 1) == tonePc (2) || mod12 (lpc + 1) == tonePc (4);
        const bool tension = ! fits && (iv == 2 || iv == 9) && ! semitoneRub;
        const bool clash = ! fits && ! tension;
        const float margin = 1.2f - 0.9f * s.movement;           // bolder = changes chord more readily
        const double minDur = 0.9 - 0.6 * s.movement;            // and more often
        if (clash && score[best] > score[chordDeg]) change = true;
        else if (score[best] > score[chordDeg] + margin && sinceChord > minDur) change = true;
    }
    if (change && best != chordDeg)
    {
        for (int i = 0; i < 3; ++i) historyMeter[(size_t) i].store (historyMeter[(size_t) i + 1].load());
        chordDeg = best;
        sinceChord = 0.0;
        const int code = mod12 (keyPc + scale[(size_t) best]) * 4 + quality (best);
        historyMeter[3].store (code);
        chordMeter.store (code);
        degreeMeter.store (best);
    }
}

void NovaAiHarmony::voice (int lead, const Settings& s) noexcept
{
    const int extra = (int) std::lround (s.spread * 5.f);
    const float follow = 1.2f - s.movement;   // how much a part prefers to stay near its last note
    std::array<int, kParts> chosen {};
    for (int p = 0; p < kParts; ++p)
    {
        if (! s.on[(size_t) p] || chordDeg < 0) { notes[(size_t) p] = 0; prev[(size_t) p] = 0; continue; }
        const int role = s.role[(size_t) p];

        if (role == roleOctave) { chosen[(size_t) p] = notes[(size_t) p] = prev[(size_t) p] = lead - 12; continue; }

        int lo, hi, centre;
        switch (role)
        {
            case roleLow:  lo = lead - 9 - extra;  hi = lead - 3;          centre = lead - 5;  break;
            case roleTop:  lo = lead + 7;          hi = lead + 14 + extra; centre = lead + 10; break;
            case roleBass: lo = lead - 24 - extra; hi = lead - 12;         centre = lead - 17; break;
            default:       lo = lead + 3;          hi = lead + 9 + extra;  centre = lead + 5;  break;   // High
        }

        int best = 0;
        float bestCost = 1.0e9f;
        for (int n = lo; n <= hi; ++n)
        {
            const int pc = mod12 (n);
            bool colour = false;
            if (role == roleBass) { if (pc != tonePc (0)) continue; }
            else if (! allowedPc (pc, s, colour)) continue;
            float cost = std::abs ((float) (n - centre)) * 0.35f;
            if (prev[(size_t) p] > 0) cost += std::abs ((float) (n - prev[(size_t) p])) * follow;
            if (colour) cost += 0.6f - 0.5f * s.colour;
            if (pc == mod12 (lead) && role != roleBass) cost += 2.5f;          // doubling the melody adds nothing
            if (std::abs (n - lead) < 3) cost += 5.f;                           // seconds rub against the lead
            for (int q = 0; q < p; ++q)
            {
                if (chosen[(size_t) q] == n) cost += 4.f;                                   // two parts on one note
                else if (chosen[(size_t) q] != 0 && mod12 (chosen[(size_t) q]) == pc) cost += 2.5f;   // same note, other octave: thin
            }
            if (cost < bestCost) { bestCost = cost; best = n; }
        }
        if (best == 0) best = role == roleLow || role == roleBass ? lead - 5 : lead + 4;   // nothing fits: a safe interval
        chosen[(size_t) p] = notes[(size_t) p] = prev[(size_t) p] = best;
    }
}

void NovaAiHarmony::update (int leadNote, double seconds, const Settings& s) noexcept
{
    buildScale (s);
    const float decay = (float) std::exp (-seconds / 2.5);
    for (auto& w : weight) w *= decay;
    sinceChord += seconds;

    // changed a role / part / colour / spread mid-note: re-voice right away
    int sig = (int) std::lround (s.colour * 20.f) * 7 + (int) std::lround (s.spread * 20.f) * 131;
    for (int p = 0; p < kParts; ++p) sig = sig * 31 + (s.on[(size_t) p] ? 1 + s.role[(size_t) p] : 0);
    const bool settingsChanged = sig != lastSig;
    lastSig = sig;

    if (leadNote <= 0) return;                 // between phrases: hold the last chord and voicing
    if (settingsChanged && leadNote == curNote && chordDeg >= 0) voice (leadNote, s);
    if (leadNote == curNote)
    {
        curDur += seconds;
        weight[(size_t) mod12 (leadNote)] += (float) seconds * 0.5f;   // long notes count more
        // a stepwise note outside the chord that turns out to be held is a real chord change
        if (passingPending && curDur > 0.3)
        {
            passingPending = false;
            chooseChord (leadNote, s);
            voice (leadNote, s);
        }
        return;
    }
    const bool stepwise = curNote > 0 && std::abs (leadNote - curNote) <= 2;
    curNote = leadNote;
    curDur = 0.0;
    weight[(size_t) mod12 (leadNote)] += 0.35f;
    passingPending = false;
    bool colour = false;
    if (chordDeg >= 0 && stepwise && ! allowedPc (mod12 (leadNote), s, colour) && sinceChord < 0.6)
        passingPending = true;               // passing note: keep the chord for now
    else
        chooseChord (leadNote, s);
    voice (leadNote, s);
}
