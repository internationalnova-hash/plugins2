// AI Harmony brain checks: chords it hears, notes it picks, how the parts move.
#include "NovaAiHarmony.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int fails = 0, passes = 0;
static void check (bool ok, const std::string& what) { std::printf ("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); (ok ? passes : fails)++; }
static const char* N[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static std::string chordName (int code) { if (code < 0) return "-"; return std::string (N[code / 4]) + (code % 4 == 1 ? "m" : code % 4 == 2 ? "dim" : ""); }

// sing a melody: each note held `secs`, processed in 512-sample blocks at 48 kHz
static void sing (NovaAiHarmony& ai, const std::vector<int>& mel, const NovaAiHarmony::Settings& s, double secs = 0.4,
                  std::vector<std::vector<int>>* partsPerNote = nullptr, std::vector<int>* chords = nullptr)
{
    const double blk = 512.0 / 48000.0;
    for (int m : mel)
    {
        for (double t = 0; t < secs; t += blk) ai.update (m, blk, s);
        if (partsPerNote) { std::vector<int> v; for (int p = 0; p < 4; ++p) v.push_back (ai.partNote (p)); partsPerNote->push_back (v); }
        if (chords) chords->push_back (ai.getChord());
    }
}

int main()
{
    NovaAiHarmony::Settings cMaj;   // C major, parts 1-2 on (High, Low)
    const int majorMask = 0xAB5;

    // 1. Chords it hears
    struct Case { std::vector<int> mel; const char* expect; };
    const Case cases[] = {
        { { 60, 64, 67, 64, 60 }, "C" },     // C E G -> C
        { { 65, 69, 72, 69, 65 }, "F" },     // F A C -> F
        { { 67, 71, 74, 71, 67 }, "G" },     // G B D -> G
        { { 69, 72, 76, 72, 69 }, "Am" },    // A C E -> Am
    };
    for (auto& c : cases)
    {
        NovaAiHarmony ai; ai.reset();
        sing (ai, c.mel, cMaj);
        check (chordName (ai.getChord()) == c.expect, std::string ("hears ") + c.expect + " (got " + chordName (ai.getChord()) + ")");
    }

    // 2. A real-ish melody: every part note in the chord, never rubbing the lead, smooth lines
    {
        NovaAiHarmony ai; ai.reset();
        NovaAiHarmony::Settings s; s.on = { true, true, true, false };   // High, Low, Octave
        const std::vector<int> mel = { 64, 64, 67, 69, 67, 64, 62, 60, 62, 64, 64, 62, 62, 64, 62, 60, 65, 65, 64, 62, 60 };
        std::vector<std::vector<int>> parts; std::vector<int> chords;
        sing (ai, mel, s, 0.35, &parts, &chords);
        bool inKey = true, noRub = true, octaveOk = true;
        int bigJumps = 0;
        for (size_t i = 0; i < mel.size(); ++i)
        {
            for (int p = 0; p < 2; ++p)
            {
                const int n = parts[i][(size_t) p];
                inKey = inKey && (majorMask & (1 << (n % 12)));
                noRub = noRub && std::abs (n - mel[i]) >= 3;
                if (i > 0 && std::abs (n - parts[i - 1][(size_t) p]) > 5) ++bigJumps;
            }
            octaveOk = octaveOk && parts[i][2] == mel[i] - 12;
        }
        check (inKey, "every High/Low note is in C major");
        check (noRub, "no part sits within a 2nd of the lead");
        check (octaveOk, "Octave part doubles the lead an octave down");
        check (bigJumps <= 3, "parts move smoothly (" + std::to_string (bigJumps) + " leaps over a 4th in " + std::to_string (mel.size()) + " notes)");
        int changes = 0; for (size_t i = 1; i < chords.size(); ++i) if (chords[i] != chords[i - 1]) ++changes;
        check (changes >= 1 && changes <= 10, "chords change with the song, not every note (" + std::to_string (changes) + " changes)");
        std::printf ("   melody  : "); for (int m : mel) std::printf ("%s ", N[m % 12]); std::printf ("\n   chords  : ");
        for (int c : chords) std::printf ("%s ", chordName (c).c_str()); std::printf ("\n   high    : ");
        for (auto& v : parts) std::printf ("%s ", N[v[0] % 12]); std::printf ("\n   low     : ");
        for (auto& v : parts) std::printf ("%s ", N[v[1] % 12]); std::printf ("\n");
    }

    // 3. Colour: rich setting reaches 7ths, simple never does
    {
        NovaAiHarmony a, b; a.reset(); b.reset();
        NovaAiHarmony::Settings simple, rich; simple.colour = 0.f; rich.colour = 1.f;
        simple.on = rich.on = { true, true, true, true }; simple.role = rich.role = { 0, 1, 3, 4 };
        const std::vector<int> mel = { 67, 71, 74, 71, 72, 74, 76, 74, 72, 71, 69, 67 };
        std::vector<std::vector<int>> ps, pr; std::vector<int> cs, cr;
        sing (a, mel, simple, 0.4, &ps, &cs); sing (b, mel, rich, 0.4, &pr, &cr);
        auto countColour = [&] (std::vector<std::vector<int>>& parts, std::vector<int>& chords)
        {
            int c = 0;
            for (size_t i = 0; i < parts.size(); ++i)
            {
                const int root = chords[i] / 4;
                for (int p = 0; p < 3; ++p) { const int iv = ((parts[i][(size_t) p] - root) % 12 + 12) % 12; if (iv == 10 || iv == 11 || iv == 2) ++c; }
            }
            return c;
        };
        check (countColour (ps, cs) == 0, "Colour at Triads: only chord tones");
        check (countColour (pr, cr) > 0, "Colour at 7ths/9ths: richer notes appear (" + std::to_string (countColour (pr, cr)) + ")");
        bool bassRoot = true;
        for (size_t i = 0; i < pr.size(); ++i) bassRoot = bassRoot && (pr[i][3] % 12 == cr[i] / 4) && pr[i][3] < mel[i] - 11;
        check (bassRoot, "Bass part sings the chord root below the lead");
    }

    // 4. Minor key
    {
        NovaAiHarmony ai; ai.reset();
        NovaAiHarmony::Settings s; s.keyPc = 9; s.stepMask = 0x5AD;   // A natural minor (relative bits 0,2,3,5,7,8,10)
        sing (ai, { 69, 72, 76, 72, 69 }, s);
        check (chordName (ai.getChord()) == "Am", "A minor: hears Am (got " + chordName (ai.getChord()) + ")");
    }

    // 5. Changing a part's role mid-note re-voices immediately
    {
        NovaAiHarmony ai; ai.reset();
        NovaAiHarmony::Settings s;
        sing (ai, { 60, 64, 67 }, s);
        const int before = ai.partNote (0);
        s.role[0] = NovaAiHarmony::roleTop;
        ai.update (67, 0.01, s);
        check (ai.partNote (0) > before, "switching Part 1 to Top moves it up right away");
    }

    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
