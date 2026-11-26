#pragma once

// Nova Notes core: offline note analysis, the note edit model, the PSOLA
// renderer and AI Assist. No plugin/host code here - everything works on a
// captured mono take so it can be tested offline (Tests/NotesTests.cpp).

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace NovaNotes
{
    // Frame-rate pitch analysis of a take (one frame per `hop` samples, ~5 ms).
    struct PitchTrack
    {
        double sampleRate = 48000.0;
        int hop = 240;
        std::vector<float> midi;   // detected pitch, 0 = unvoiced
        std::vector<float> conf;   // periodicity 0..1
        std::vector<float> db;     // frame level, dBFS
        int size() const noexcept { return (int) midi.size(); }
    };

    // One sung note (a "blob") plus everything the user/AI did to it.
    struct Note
    {
        int   id = 0;
        int   start = 0, end = 0;   // analysis frames [start, end)
        float center = 60.f;        // sung centre pitch (analysed, never edited)
        float pitch = 60.f;         // edited centre pitch
        float drift = 0.f;          // 0..1 of the slow pitch wander removed
        float vibrato = 1.f;        // 0..2 depth of the fast wobble (1 = as sung)
        float transition = 0.5f;    // 0..1 glide length into this note
        float formant = 0.f;        // cents
        float gainDb = 0.f;
        float timeShift = 0.f;      // seconds (moves the note, neighbours stretch)
        // Hand-drawn pitch line (Draw tool): one value per frame of the note, in semitones
        // relative to `pitch` (so it moves with the note); NaN where nothing is drawn.
        std::vector<float> drawn;

        bool isEdited() const noexcept;
        bool hasDrawn() const noexcept;
    };

    struct Key { int tonic = 0; bool minor = false; float confidence = 0.f; };

    // ── Analysis ────────────────────────────────────────────────────────────
    PitchTrack trackPitch (const float* x, int n, double sampleRate);
    std::vector<int> findEpochs (const float* x, int n, const PitchTrack& t);   // pitch marks for PSOLA
    std::vector<Note> segmentNotes (const PitchTrack& t);
    float measureCenter (const PitchTrack& t, int start, int end);
    Key detectKey (const std::vector<Note>& notes, const PitchTrack& t);
    int scaleMask (Key k);                       // 12-bit absolute pitch-class mask
    float snapToMask (float midi, int mask);     // nearest allowed semitone (mask 0 = chromatic)
    // Note deviation from its centre split into slow wander and fast wobble (one value per frame).
    void decompose (const PitchTrack& t, const Note& n, std::vector<float>& slow, std::vector<float>& fast);

    // ── Editing helpers ─────────────────────────────────────────────────────
    // Melodyne-style macro: pull centres toward the snap note and remove wander.
    void correctPitch (std::vector<Note>& notes, const std::vector<int>& ids,
                       float centerAmt, float driftAmt, int mask);
    // Snap note starts toward a 1/16 grid. secondsPerBeat/gridOffsetSec describe the host grid.
    void quantizeTime (std::vector<Note>& notes, const std::vector<int>& ids, const PitchTrack& t,
                       double secondsPerBeat, double firstBeatSec, float amount);
    // Split a note at a frame; returns the new note's id or -1.
    int splitNote (std::vector<Note>& notes, int id, int frame, const PitchTrack& t);
    int nextNoteId (const std::vector<Note>& notes);

    // ── AI Assist ───────────────────────────────────────────────────────────
    enum class AssistStyle { natural = 0, tight = 1, tuneReady = 2 };
    struct AssistReport { int fixed = 0, keptSlides = 0, keptVibrato = 0, leftAlone = 0, runs = 0; };
    AssistReport aiAssist (const PitchTrack& t, std::vector<Note>& notes, const std::vector<int>& ids,
                           AssistStyle style, Key key);

    // ── Rendering ───────────────────────────────────────────────────────────
    bool anyEdits (const std::vector<Note>& notes);
    // Renders the edited take (same length as the input) into `out`.
    void render (const float* x, int n, const PitchTrack& t, const std::vector<int>& epochs,
                 const std::vector<Note>& notes, float* out);

    // ── Serialisation ───────────────────────────────────────────────────────
    juce::String notesToJson (const std::vector<Note>& notes);
    std::vector<Note> notesFromJson (const juce::String& json);
    juce::String trackToJson (const PitchTrack& t, int decimate = 2);   // display data for the editor
    void applyPatch (Note& n, const juce::var& patch);                  // {pitch, drift, ...}

    inline float midiToHz (float m) noexcept { return 440.f * std::exp2 ((m - 69.f) / 12.f); }
    inline float hzToMidi (float hz) noexcept { return 69.f + 12.f * std::log2 (hz / 440.f); }
}
