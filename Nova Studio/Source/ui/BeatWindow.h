#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../Session.h"
#include "../TransportState.h"

namespace NovaStudioUI
{
    // ── Beat palette ──────────────────────────────────────────────────────────
    namespace BeatTheme
    {
        inline juce::Colour bg()          { return juce::Colour::fromRGB(11, 13, 20); }
        inline juce::Colour panel()       { return juce::Colour::fromRGB(17, 19, 28); }
        inline juce::Colour accent()      { return juce::Colour::fromRGB( 80, 100, 255); }
        inline juce::Colour accentGlow()  { return juce::Colour::fromRGBA( 70,  90, 240, 140); }
        inline juce::Colour stepOn()      { return juce::Colour::fromRGB(110, 140, 255); }
        inline juce::Colour stepOff()     { return juce::Colour::fromRGB( 28,  32,  46); }
        inline juce::Colour stepCursor()  { return juce::Colour::fromRGBA(200, 220, 255, 60); }
        inline juce::Colour padBase()     { return juce::Colour::fromRGB( 22,  25,  38); }
        inline juce::Colour padHit()      { return juce::Colour::fromRGB( 90, 120, 255); }
        inline juce::Colour noteBlock()   { return juce::Colour::fromRGB( 90,  60, 200); }
        inline juce::Colour grid()        { return juce::Colour::fromRGBA(255,255,255, 12); }
        inline juce::Colour gridBeat()    { return juce::Colour::fromRGBA(255,255,255, 28); }
        inline juce::Colour edge()        { return juce::Colour::fromRGBA(255,255,255, 16); }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BeatBrowser — left panel
    // ─────────────────────────────────────────────────────────────────────────
    class BeatBrowser : public juce::Component,
                        private juce::Button::Listener
    {
    public:
        BeatBrowser();
        ~BeatBrowser() override;
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void buttonClicked(juce::Button* b) override;
        void rebuildList();

        juce::TextEditor searchField;
        juce::TextButton kitsBtn    { "Kits" };
        juce::TextButton loopsBtn   { "Loops" };
        juce::TextButton oneshotBtn { "One-shots" };
        juce::TextButton pluginsBtn { "Plugins" };
        juce::ListBox    contentList;
        juce::Label      favHeader;
        juce::ListBox    favList;

        int selectedCategory = 0; // 0=Kits,1=Loops,2=One-shots,3=Plugins

        struct ContentModel : public juce::ListBoxModel
        {
            juce::StringArray items;
            int getNumRows() override { return items.size(); }
            void paintListBoxItem(int row, juce::Graphics& g,
                                  int w, int h, bool selected) override;
        } contentModel, favModel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatBrowser)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // PatternPlaylist — top center
    // ─────────────────────────────────────────────────────────────────────────
    class PatternPlaylist : public juce::Component
    {
    public:
        PatternPlaylist();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        struct PatternBlock { int lane, startBar, lengthBars; juce::Colour colour; juce::String name; };
        juce::Array<PatternBlock> blocks;
        int numBars = 16;
        int selectedLane = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternPlaylist)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // PianoRollView — center
    // ─────────────────────────────────────────────────────────────────────────
    class PianoRollView : public juce::Component
    {
    public:
        PianoRollView();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        void drawPianoKeys(juce::Graphics& g, juce::Rectangle<int> keysArea) const;
        void drawGrid(juce::Graphics& g, juce::Rectangle<int> gridArea) const;
        void drawNotes(juce::Graphics& g, juce::Rectangle<int> gridArea) const;
        void drawVelocityLane(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawSnapControls(juce::Graphics& g, juce::Rectangle<int> area) const;

        static constexpr int kKeyWidth   = 36;
        static constexpr int kRowHeight  = 12;
        static constexpr int kVelHeight  = 40;
        static constexpr int kHeaderH    = 24;
        static constexpr int kNumOctaves = 5;
        static constexpr int kSnapH      = 22;

        struct NoteBlock { int pitch, startStep, lengthSteps; float velocity; };
        juce::Array<NoteBlock> notes;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollView)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // StepSequencerView — bottom center
    // ─────────────────────────────────────────────────────────────────────────
    class StepSequencerView : public juce::Component,
                              private juce::Timer
    {
    public:
        explicit StepSequencerView(NovaStudio::TransportState& transport);
        ~StepSequencerView() override;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

        static constexpr int kNumRows  = 4;
        static constexpr int kNumSteps = 16;
        static constexpr int kHeaderW  = 70;
        static constexpr int kRowH     = 36;
        static constexpr int kTopH     = 22;

    private:
        void timerCallback() override;
        void drawRowHeader(juce::Graphics& g, juce::Rectangle<int> r,
                           int row, bool muted, bool solo) const;
        void drawStep(juce::Graphics& g, juce::Rectangle<int> r,
                      bool active, bool isCursor) const;

        const char* kRowNames[kNumRows] = { "Kick", "Snare", "Hi-Hat", "Perc" };
        juce::Colour kRowColours[kNumRows] = {
            juce::Colour::fromRGB(255, 100,  60),
            juce::Colour::fromRGB( 60, 180, 255),
            juce::Colour::fromRGB(220, 200,  60),
            juce::Colour::fromRGB(160,  80, 255)
        };

        bool steps[kNumRows][kNumSteps] {};
        bool muted[kNumRows] {};
        bool soloed[kNumRows] {};
        int  cursorStep = -1;

        NovaStudio::TransportState& transportState;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerView)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // DrumRackPanel — right panel
    // ─────────────────────────────────────────────────────────────────────────
    class DrumRackPanel : public juce::Component
    {
    public:
        DrumRackPanel();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        void drawPad(juce::Graphics& g, juce::Rectangle<int> r,
                     const juce::String& name, bool selected, juce::Colour accent) const;
        void drawSoundControls(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawPluginChain(juce::Graphics& g, juce::Rectangle<int> area) const;

        static constexpr int kPadRows = 4;
        static constexpr int kPadCols = 4;
        int selectedPad = 0;

        const char* kPadNames[kPadRows * kPadCols] = {
            "Kick 1","Kick 2","Kick 3","Kick 4",
            "Snare 1","Snare 2","Clap","Rim",
            "HH Cl","HH Op","Cymbal","Ride",
            "Perc 1","Perc 2","Crash","FX"
        };
        juce::Colour kPadAccents[kPadRows] = {
            juce::Colour::fromRGB(255, 100,  60),
            juce::Colour::fromRGB( 60, 180, 255),
            juce::Colour::fromRGB(220, 200,  60),
            juce::Colour::fromRGB(160,  80, 255)
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumRackPanel)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // BeatWindow — top-level
    // ─────────────────────────────────────────────────────────────────────────
    class BeatWindow : public juce::Component
    {
    public:
        explicit BeatWindow(NovaStudio::TransportState& transport);
        ~BeatWindow() override;
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        BeatBrowser       browser;
        PatternPlaylist   playlist;
        PianoRollView     pianoRoll;
        StepSequencerView stepSeq;
        DrumRackPanel     drumRack;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatWindow)
    };
}
