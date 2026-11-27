#pragma once

// Nova Melody core: writes melodies / basslines / 808s / arps / hooks in a genre and
// vibe, and turns humming into notes that follow it exactly. No plugin or host code
// here, so everything is testable offline (Tests/MelodyTests.cpp). Times are in beats
// (quarter notes) from the start of the clip; pitches are MIDI note numbers.

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace NovaMelody
{
    struct MNote
    {
        double start = 0, len = 0.25;   // beats
        int pitch = 60;
        int vel = 96;
        bool locked = false;            // kept when the rest is redone
        bool glide = false;             // 808: slide into this note from the previous one
    };

    // ── keys and scales ─────────────────────────────────────────────────────
    enum Scale { Major, Minor, HarmonicMinor, Dorian, Phrygian, Mixolydian, PentaMinor, PentaMajor, Blues, kNumScales };
    const char* scaleName (int scale) noexcept;
    const std::vector<int>& scaleSteps (int scale) noexcept;       // semitones from the tonic
    const char* noteName (int pitchClass) noexcept;                 // "C", "C#", ...
    struct Key { int tonic = 0; int scale = Minor; };
    juce::String keyName (Key k);
    int  scaleMask (Key k) noexcept;                                // 12-bit pitch-class mask
    bool inKey (int pitch, Key k) noexcept;
    int  snapToKey (int pitch, Key k) noexcept;                     // nearest in-key note
    bool isMinorish (int scale) noexcept;

    /// Key from a pitch-class weight histogram (Krumhansl), e.g. what the chords played
    Key detectKey (const std::array<float, 12>& weights, float* confidence = nullptr);

    // ── chords (heard on MIDI in, or a default progression) ─────────────────
    struct Chord { double start = 0, len = 4; int root = 0; std::vector<int> pcs; };
    /// One chord per bar from the key (i-VI-III-VII, I-V-vi-IV ... by genre/vibe)
    std::vector<Chord> defaultProgression (Key k, int genre, int vibe, int bars);
    juce::String chordName (const Chord& c);

    // ── generator ───────────────────────────────────────────────────────────
    enum Genre { Trap, Drill, RnB, LatinUrban, Reggaeton, Afrobeats, Pop, House, Lofi, Gospel, kNumGenres };
    enum Vibe  { Dark, Emotional, Dreamy, Bright, Hype, Sexy, kNumVibes };
    enum Part  { Lead, Bassline, Bass808, Counter, Arp, Hook, kNumParts };
    enum Register { Low, Mid, High };
    const char* genreName (int) noexcept;
    const char* vibeName (int) noexcept;
    const char* partName (int) noexcept;

    struct GenParams
    {
        int genre = LatinUrban, vibe = Emotional, part = Lead, reg = Mid;
        float density = 0.55f, rhythm = 0.7f, swing = 0.2f, repetition = 0.6f;   // 0..1
        int bars = 8;
        Key key;
        bool stayInKey = true, leaveSpace = false;
        std::vector<Chord> chords;          // empty = defaultProgression
        std::vector<MNote> keep;            // locked notes: kept exactly, the rest is written around them
        std::vector<MNote> against;         // Counter: the melody to answer
        juce::uint32 seed = 1;
    };
    std::vector<MNote> generate (const GenParams& p);

    // ── humming ─────────────────────────────────────────────────────────────
    enum HumAs { AsBass, As808, AsMelody, AsChords };
    struct HumParams
    {
        int as = AsBass;
        float pitchSnap = 0.f;              // 0 = exactly as hummed (nearest semitone) .. 1 = always in key
        float timeSnap = 0.f;               // 0 = exactly as hummed .. 1 = on the 1/16 grid
        float minNoteMs = 40.f;             // shorter blips are dropped / merged
        int octave = 0;                     // added after the automatic range
        bool autoOctave = true;
        bool autoKey = true;                // key heard from the humming (else `key`)
        Key key;
        double bpm = 120.0;
        double capturedAtBeat = -1;         // >= 0: recorded in time with the song (keep its bar position)
        int genre = LatinUrban;             // for Make it groove / Answer it
    };
    struct HumResult
    {
        bool ok = false;
        juce::String message;               // plain-words problem ("nothing sung was heard")
        Key key;                            // key used (heard or picked)
        bool keyWasHeard = false;
        float keyConfidence = 0.f;          // low = the humming didn't say much about the key (pick one)
        double lengthBeats = 0;
        std::vector<MNote> heard;           // each hummed note as sung, rounded to the nearest semitone
        std::vector<float> heardCentre;     // ...and its exact sung pitch (fractional MIDI)
        std::vector<float> curveBeat, curvePitch;   // the raw pitch line (for drawing), voiced frames only
        std::vector<MNote> notes;           // the result (bassline / 808 / melody / chords)
    };
    HumResult fromHum (const float* mono, int n, double sampleRate, const HumParams& p);
    /// Re-run only the note-making part (fast - no pitch tracking), e.g. when a slider moves
    void remakeFromHeard (HumResult& r, const HumParams& p);

    /// Keeps the hummed pitches in order, re-places them on the genre's groove
    std::vector<MNote> makeItGroove (const std::vector<MNote>& notes, int genre, double lengthBeats, bool bass, juce::uint32 seed);
    /// Writes a melody that answers the hummed line in the gaps it leaves
    std::vector<MNote> answerIt (const std::vector<MNote>& hummed, Key k, int genre, double lengthBeats, juce::uint32 seed);

    // ── Play (no theory needed) ─────────────────────────────────────────────
    /// melody pad 0..7 = scale notes 1..8 from the tonic (pad 7 = the tonic an octave up)
    int padPitch (Key k, int pad, int octave) noexcept;
    /// chord pad 0..6 = the chord on that scale degree, voiced around the octave below middle C
    std::vector<int> chordPadPitches (Key k, int degree, int octave);
    /// pitch classes of the chord on a degree (for "glow the notes that fit")
    std::vector<int> chordPadClasses (Key k, int degree);
    /// MIDI keyboard: white keys walk up the scale from middle C (= the tonic); black keys
    /// play the white key below them, so every key is a good note. Returns the pad step
    /// counted from middle C's tonic (0 = 1, 7 = 1 an octave up, -1 = 7 below...).
    int whiteKeyStep (int midiNote) noexcept;
    int stepPitch (Key k, int step, int octave) noexcept;

    // ── MIDI file ───────────────────────────────────────────────────────────
    juce::MemoryBlock toMidiFile (const std::vector<MNote>& notes, double bpm, const juce::String& name);
}
