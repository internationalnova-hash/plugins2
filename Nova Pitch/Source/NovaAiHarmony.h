#pragma once

#include <array>
#include <atomic>

// AI Harmony: listens to the lead melody (the note Nova Pitch is tuning it to),
// works out which chord of the key the melody implies, and picks a note for
// each harmony part by its role (High / Low / Octave / Top / Bass) with smooth
// voice leading - the way a session singer would stack a part.
//
// Runs on the audio thread: no allocation, no locks. update() once per block.
class NovaAiHarmony
{
public:
    static constexpr int kParts = 4;
    enum Role { roleHigh = 0, roleLow, roleOctave, roleTop, roleBass, kRoles };
    enum Quality { qMajor = 0, qMinor = 1, qDim = 2 };

    struct Settings
    {
        int keyPc = 0;
        int stepMask = 0xAB5;              // scale, 12 bits relative to keyPc
        float colour = 0.3f;               // 0 triads .. 1 add 7ths + 9ths
        float movement = 0.35f;            // 0 smooth/steady .. 1 bold
        float spread = 0.25f;              // 0 close .. 1 open
        std::array<int, kParts>  role { { roleHigh, roleLow, roleOctave, roleTop } };
        std::array<bool, kParts> on   { { true, true, false, false } };
    };

    void reset() noexcept;
    // leadNote: the MIDI note the lead is being tuned to (<= 0 while not singing)
    void update (int leadNote, double seconds, const Settings& s) noexcept;

    int partNote (int p) const noexcept { return notes[(size_t) p]; }

    // For the UI: chord = root pc * 4 + quality, degree 0..6 (-1 = none yet)
    int getChord() const noexcept   { return chordMeter.load (std::memory_order_relaxed); }
    int getDegree() const noexcept  { return degreeMeter.load (std::memory_order_relaxed); }
    int getHistory (int i) const noexcept { return historyMeter[(size_t) i].load (std::memory_order_relaxed); }   // oldest first

private:
    void buildScale (const Settings& s) noexcept;
    void chooseChord (int lead, const Settings& s) noexcept;
    void voice (int lead, const Settings& s) noexcept;
    int  tonePc (int degreeOffset) const noexcept;       // absolute pc of scale degree (chordDeg + offset)
    bool allowedPc (int pc, const Settings& s, bool& isColour) const noexcept;
    int  quality (int deg) const noexcept;

    std::array<int, 7> scale {};          // relative semitones of the 7 degrees
    int keyPc = 0;
    bool minorKey = false;
    std::array<float, 12> weight {};      // recent melody pitch classes (absolute)
    int curNote = 0;
    double curDur = 0.0, sinceChord = 0.0;
    int chordDeg = -1;
    std::array<int, kParts> notes {}, prev {};
    int lastSig = 0;
    bool passingPending = false;   // a stepwise out-of-chord note: wait and see if it's held

    std::atomic<int> chordMeter { -1 }, degreeMeter { -1 };
    std::array<std::atomic<int>, 4> historyMeter {};
};
