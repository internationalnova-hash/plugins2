#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "StudioComponents.h"
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
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        // Set the current pattern name/color that gets stamped on click
        void setActivePattern(const juce::String& name, juce::Colour colour)
            { activeName = name; activeColour = colour; }

    private:
        struct PatternBlock { int lane, startBar, lengthBars; juce::Colour colour; juce::String name; };
        juce::Array<PatternBlock> blocks;
        int numBars      = 32;
        int selectedLane = -1;

        juce::String activeName   { "Pattern 1" };
        juce::Colour activeColour { juce::Colour::fromRGB(80, 100, 255) };

        // Drag state
        int  dragBlockIdx   = -1;
        int  dragOffsetBars = 0;

        // Hit-test helpers
        static constexpr int kHeaderH = 20;
        static constexpr int kLaneH   = 28;
        static constexpr int kLabelW  = 50;
        static constexpr int kNumLanes = 4;

        int  blockAtPoint(int x, int y) const;
        void laneBarFromPoint(int x, int y, int& lane, int& bar) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternPlaylist)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // PianoRollView — center
    // ─────────────────────────────────────────────────────────────────────────
    class PianoRollView : public juce::Component
    {
    public:
        struct NoteBlock { int pitch, startStep, lengthSteps; float velocity; };

        PianoRollView();
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

        // ── FL-style note tools (operate on the current selection; act as no-ops
        // when nothing is selected) ──────────────────────────────────────────
        void toolStrum(bool ascending, int stepsBetweenNotes = 1);
        void toolFlam();
        void toolChop(int numSlices);
        void toolArpeggiate(int pattern);  // 0=up, 1=down, 2=up-down, 3=down-up, 4=random
        void toolChordStamp(int chordShape); // 0=major,1=minor,2=dim,3=aug,4=maj7,5=min7,6=dom7,7=sus2,8=sus4
        void toolQuantizeSelection();

        static constexpr int kNumScaleTypes = 5;
        static const char* scaleTypeName(int type);
        void setScale(int rootPitchClass, int scaleType);  // rootPitchClass: -1 = off (chromatic), 0-11 = C..B
        void setSnapMode(int mode);                         // 0=1/4 1=1/8 2=1/16 3=1/32 4=Free

        // Ghost notes — faint reference copies of notes from another channel/pattern,
        // drawn behind the editable notes so the user can align melodies against them.
        void setGhostNotes(juce::Array<NoteBlock> ghosts);
        void setShowGhostNotes(bool shouldShow);
        bool isShowingGhostNotes() const noexcept { return showGhostNotes; }

    private:
        void drawPianoKeys(juce::Graphics& g, juce::Rectangle<int> keysArea) const;
        void drawGrid(juce::Graphics& g, juce::Rectangle<int> gridArea) const;
        void drawNotes(juce::Graphics& g, juce::Rectangle<int> gridArea) const;
        void drawVelocityLane(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawSnapControls(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawToolsRow(juce::Graphics& g, juce::Rectangle<int> area) const;

        void showNoteContextMenu(int noteIndex);
        void showGridContextMenu();
        void showScalePickerMenu();

        bool isPitchInScale(int pitch) const;
        int  snapDivisorSteps() const;
        int  hitTestNote(int pitch, int step) const; // -1 if none

        static constexpr int kKeyWidth   = 36;
        static constexpr int kRowHeight  = 12;
        static constexpr int kVelHeight  = 40;
        static constexpr int kHeaderH    = 24;
        static constexpr int kNumOctaves = 5;
        static constexpr int kSnapH      = 22;
        static constexpr int kToolsH     = 24;

        juce::Array<NoteBlock> notes;
        juce::Array<int> selectedNotes;   // indices into `notes`

        juce::Array<NoteBlock> ghostNotes;
        bool showGhostNotes = false;

        int snapMode   = 2;     // index into {1/4, 1/8, 1/16, 1/32, Free}
        int scaleRoot  = -1;    // -1 = chromatic (no highlight), 0-11 = pitch class of root (C=0)
        int scaleType  = 0;     // index into scale interval table

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollView)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // FloatingSubPanel — a small draggable/resizable internal "window" that
    // hosts another component, FL Studio-style (Channel rack / Mixer float
    // over the main view rather than being permanently docked).
    // ─────────────────────────────────────────────────────────────────────────
    class FloatingSubPanel : public juce::Component
    {
    public:
        explicit FloatingSubPanel(const juce::String& title);
        ~FloatingSubPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

        // Hosts another component without owning it
        void setContent(juce::Component* c);

        std::function<void()> onClose;

        static constexpr int kTitleBarH = 24;

    private:
        juce::String        titleText;
        juce::Component*    content = nullptr;
        juce::ComponentDragger dragger;
        juce::ComponentBoundsConstrainer constrainer;
        std::unique_ptr<juce::ResizableCornerComponent> resizer;
        juce::TextButton closeBtn { "x" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatingSubPanel)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // CompactMixerView — small in-window mixer (FL Studio-style), shown inside
    // the beat production window so users don't need to leave it to adjust
    // track levels/pan/mute.
    // ─────────────────────────────────────────────────────────────────────────
    class CompactMixerView : public juce::Component,
                             private juce::Slider::Listener,
                             private juce::Button::Listener
    {
    public:
        CompactMixerView();
        ~CompactMixerView() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // Rebuild strips for the given track count (called when session changes)
        void setNumTracks(int numTracks);

        // Push current state from the engine/session into a strip
        void setTrackInfo(int index, const juce::String& name, juce::Colour colour,
                          float volumeDb, float pan, bool muted);

        std::function<void(int track, float dB)>  onVolumeChanged;
        std::function<void(int track, float pan)> onPanChanged;
        std::function<void(int track, bool)>      onMuteToggled;

    private:
        void sliderValueChanged(juce::Slider* s) override;
        void buttonClicked(juce::Button* b) override;

        struct Strip
        {
            juce::Label      nameLabel;
            juce::Slider     volumeFader;
            juce::Slider     panKnob;
            juce::TextButton muteBtn { "M" };
            juce::Colour     colour { 100, 80, 200 };
        };

        static constexpr int kStripWidth = 56;

        juce::OwnedArray<Strip> strips;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompactMixerView)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // PatternLaunchGrid — Performance Mode launch pads. Click a pad to queue
    // its pattern; it fires (and the pad highlights as "playing") at the next
    // loop boundary. A second click on the playing pad re-queues it (re-launch).
    // ─────────────────────────────────────────────────────────────────────────
    class PatternLaunchGrid : public juce::Component
    {
    public:
        PatternLaunchGrid();
        ~PatternLaunchGrid() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

        // Pad labels/colours mirror the pattern list (name per pad)
        void setPatterns(const juce::StringArray& names, const juce::Array<juce::Colour>& colours);

        void setQueuedIndex(int index);   // pad waiting to launch on next loop boundary (-1 = none)
        void setPlayingIndex(int index);  // pad currently driving the engine (-1 = none)

        std::function<void(int patternIndex)> onPadClicked;

    private:
        struct Pad { juce::String name; juce::Colour colour; juce::Rectangle<int> bounds; };
        juce::Array<Pad> pads;
        int queuedIndex  = -1;
        int playingIndex = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternLaunchGrid)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // StepSequencerView — FL Studio-style beat producer
    // ─────────────────────────────────────────────────────────────────────────
    class StepSequencerView : public juce::Component,
                              public juce::DragAndDropTarget,
                              private juce::Timer
    {
    public:
        explicit StepSequencerView(NovaStudio::TransportState& transport);
        ~StepSequencerView() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        // DragAndDropTarget — for sample file drops
        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDragEnter(const SourceDetails& details) override;
        void itemDragMove(const SourceDetails& details) override;
        void itemDragExit(const SourceDetails& details) override;
        void itemDropped(const SourceDetails& details) override;

        // Layout constants
        static constexpr int kNumRows   = 8;
        static constexpr int kMaxSteps  = 64;
        static constexpr int kChannelW  = 130;   // left channel strip width
        static constexpr int kRowH      = 26;   // compact, professional row height (was 34 — too tall/cartoony)
        static constexpr int kToolbarH  = 0;    // internal toolbar removed; BeatPatternToolbar handles this now
        static constexpr int kGraphH    = 70;    // graph editor height

        // ── Data model ───────────────────────────────────────────────────────
        struct ChannelData
        {
            juce::String name;
            juce::String samplePath;
            juce::Colour colour;
            float volume = 1.0f;
            float pan    = 0.0f;
            float pitch  = 0.0f;    // semitones, -24..+24 — FL-style per-channel pitch knob
            bool  muted  = false;
            int   mixerTrack = 0;   // routing: 0 = Master, 1..N = mixer track index
            juce::String group;     // FL-style channel group tag — "" = ungrouped

            // FL-style "channel settings" engine — persisted per channel so the
            // Engine tab in the sample editor reflects/restores the right state
            bool  envEnabled  = false;
            float envAttack   = 0.001f, envHold = 0.0f, envDecay = 0.1f, envSustain = 1.0f, envRelease = 0.05f;
            bool  filterEnabled   = false;
            int   filterType      = 0;        // 0 = lowpass, 1 = highpass
            float filterCutoff    = 8000.0f;
            float filterResonance = 0.707f;
            bool  lfoEnabled = false;
            int   lfoTarget  = 0;             // 0 = pitch, 1 = volume, 2 = cutoff
            float lfoRate    = 4.0f;
            float lfoDepth   = 0.0f;
            float sampleStart = 0.0f, sampleEnd = 1.0f;
            bool  loopEnabled = false;

            bool  steps[kMaxSteps] {};
            float velocities[kMaxSteps];
            ChannelData() { std::fill(velocities, velocities + kMaxSteps, 1.0f); }
        };

        struct Pattern
        {
            juce::String name;
            int   stepCount = 16;
            float swing     = 0.0f;
            int   grooveTemplate = 0;
            std::array<ChannelData, kNumRows> channels;

            Pattern()
            {
                static const char* names[] =
                    { "Kick","Snare","Hi-Hat","Open HH","Clap","808","Perc","FX" };
                static const juce::Colour cols[] = {
                    juce::Colour::fromRGB(255, 100,  60),
                    juce::Colour::fromRGB( 60, 180, 255),
                    juce::Colour::fromRGB(220, 200,  60),
                    juce::Colour::fromRGB(160,  80, 255),
                    juce::Colour::fromRGB( 80, 220, 120),
                    juce::Colour::fromRGB(255, 160,  40),
                    juce::Colour::fromRGB(200,  80, 200),
                    juce::Colour::fromRGB( 60, 200, 200)
                };
                for (int i = 0; i < kNumRows; ++i)
                {
                    channels[i].name   = names[i];
                    channels[i].colour = cols[i];
                }
                // Default kick pattern
                channels[0].steps[0] = channels[0].steps[4] =
                channels[0].steps[8] = channels[0].steps[12] = true;
                // Default snare
                channels[1].steps[4] = channels[1].steps[12] = true;
                // Default hi-hat (every 2)
                for (int s = 0; s < 16; s += 2) channels[2].steps[s] = true;
            }
        };

        // ── Engine callbacks ─────────────────────────────────────────────────
        std::function<void(int row, const juce::String& path)> onSampleAssigned;
        std::function<void(int row, int step, bool active)>    onStepChanged;
        std::function<void(int row, int step, float velocity)> onVelocityChanged;
        std::function<void(int row, float volume)>             onRowVolumeChanged;
        std::function<void(int row, float pan)>                onRowPanChanged;
        std::function<void(int row, float pitch)>              onRowPitchChanged;
        std::function<void(int row, bool enabled, float attack, float hold, float decay, float sustain, float release)> onRowEnvelopeChanged;
        std::function<void(int row, bool enabled, int filterType, float cutoffHz, float resonance)>                     onRowFilterChanged;
        std::function<void(int row, bool enabled, int target, float rateHz, float depth)>                               onRowLFOChanged;
        std::function<void(int row, float startFrac, float endFrac, bool loop)>                                         onRowSampleRegionChanged;
        std::function<void(int row, bool muted)>               onRowMutedChanged;
        std::function<void(int row, int mixerTrack)>           onRowMixerTrackChanged;
        std::function<void(const juce::String& filePath)>      onPreviewSample;
        std::function<void(int stepCount)>                     onStepCountChanged;
        std::function<void(float swing)>                       onSwingChanged;
        std::function<void(int grooveTemplate)>                onGrooveChanged;

        // Number of mixer inserts available for routing (set by BeatWindow/MainComponent)
        int numMixerInserts = 8;

        // External step cursor update (called from MainComponent timer or getStepSeqCurrentStep)
        void setPlayCursor(int step) { if (step != cursorStep) { cursorStep = step; repaint(); } }

        // Called by inline rename editor when done
        void finishInlineRename(bool accepted);

    private:
        // ── Timer ────────────────────────────────────────────────────────────
        void timerCallback() override;

        // ── Drawing helpers ──────────────────────────────────────────────────
        void paintChannelStrip(juce::Graphics& g, juce::Rectangle<int> r, int row);
        void paintStepGrid(juce::Graphics& g, juce::Rectangle<int> r);
        void paintGraphEditor(juce::Graphics& g, juce::Rectangle<int> r);

        // ── Hit-testing ──────────────────────────────────────────────────────
        // Returns the step grid area (excluding channel strip)
        juce::Rectangle<int> stepGridArea() const;
        juce::Rectangle<int> channelStripArea(int row) const;
        juce::Rectangle<int> toolbarArea() const;
        juce::Rectangle<int> graphArea() const;
        juce::Rectangle<int> allRowsArea() const; // toolbar to bottom of rows (excl graph)

        // Given a point inside the step grid area, return row and step (-1 if invalid)
        bool hitTestGrid(juce::Point<int> p, int& outRow, int& outStep) const;
        // Given a point in the graph area, return step index (-1 if invalid)
        int  hitTestGraph(juce::Point<int> p) const;
        // Which part of channel strip was clicked: 0=mute, 1=pan, 2=vol, 3=name
        int  hitTestChannelStrip(juce::Point<int> localInStrip) const;

        float graphYToVelocity(int y) const;

        // ── Interaction helpers ──────────────────────────────────────────────
        void handleStepRightClickMenu(int row, int step);
        void handleChannelRightClickMenu(int row);
        void renameChannel(int row);

        // Step pixel metrics
        float stepCellWidth() const;   // width of one step cell in the grid

        static bool isAudioFile(const juce::String& path);
        int rowAtY(int y) const;       // for drag-and-drop

        // Inline rename helpers
        void showInlineRename(juce::Rectangle<int> bounds, const juce::String& initial, bool isPattern);

        // ── State ────────────────────────────────────────────────────────────
        juce::Array<Pattern> patterns;
        int  currentPatternIdx = 0;
        int  selectedRow       = 0;
        bool showGraphEditor   = true;
        int  cursorStep        = -1;

        // FL-style channel group filter — "" shows all rows; otherwise dims rows
        // whose ChannelData::group doesn't match. Toggled via Ctrl/Cmd-click on a channel name.
        juce::String groupFilter;

        // Drag state
        bool isDragging      = false;
        bool dragActivating  = false;  // true = activating steps, false = deactivating
        int  dragStartRow    = -1;
        int  dragStartStep   = -1;
        bool isDraggingGraph = false;  // drag inside velocity graph
        int  dragHighlightRow = -1;    // for sample D&D

        // Swing scrub drag
        bool   isSwingDragging  = false;
        int    swingDragStartX  = 0;
        float  swingDragStartVal = 0.0f;

        // Pan/vol knob drag (compact custom knobs)
        enum class KnobDragType { None, Pan, Volume };
        KnobDragType knobDragType = KnobDragType::None;
        int   knobDragRow   = -1;
        int   knobDragStartY = 0;
        float knobDragStartVal = 0.0f;

        NovaStudio::TransportState& transportState;

        // Inline rename
        std::unique_ptr<juce::TextEditor> inlineEditor;
        int  inlineRenameRow = -1;   // -1 = pattern name
        bool inlineIsPattern = false;

        // Accessors to current pattern
        Pattern& currentPattern() { return patterns.getReference(currentPatternIdx); }
        const Pattern& currentPattern() const { return patterns.getReference(currentPatternIdx); }

        friend class BeatWindow;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerView)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // DrumRackPanel — right panel
    // ─────────────────────────────────────────────────────────────────────────
    class DrumRackPanel : public juce::Component,
                          public juce::DragAndDropTarget,
                          private juce::Timer
    {
    public:
        DrumRackPanel();
        ~DrumRackPanel() override;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

        // DragAndDropTarget
        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDragEnter(const SourceDetails& details) override;
        void itemDragMove(const SourceDetails& details) override;
        void itemDragExit(const SourceDetails& details) override;
        void itemDropped(const SourceDetails& details) override;

    private:
        void timerCallback() override;
        void drawPad(juce::Graphics& g, juce::Rectangle<int> r,
                     const juce::String& name, const juce::String& sampleFile,
                     bool selected, bool pressed, bool dragOver, juce::Colour accent) const;
        void drawSoundControls(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawPluginChain(juce::Graphics& g, juce::Rectangle<int> area) const;
        int padAtPoint(juce::Point<int> pos) const;
        static bool isAudioFile(const juce::String& path);

        static constexpr int kPadRows = 4;
        static constexpr int kPadCols = 4;
        int selectedPad      = 0;
        int pressedPad       = -1;
        int dragHighlightPad = -1;

        juce::String kPadNames[kPadRows * kPadCols] = {
            "Kick 1","Kick 2","Kick 3","Kick 4",
            "Snare 1","Snare 2","Clap","Rim",
            "HH Cl","HH Op","Cymbal","Ride",
            "Perc 1","Perc 2","Crash","FX"
        };
        juce::String padSampleFiles[kPadRows * kPadCols]; // empty = no file assigned
        juce::Colour kPadAccents[kPadRows] = {
            juce::Colour::fromRGB(255, 100,  60),
            juce::Colour::fromRGB( 60, 180, 255),
            juce::Colour::fromRGB(220, 200,  60),
            juce::Colour::fromRGB(160,  80, 255)
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumRackPanel)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // BeatTransportBar — transport controls at top of BeatWindow
    // ─────────────────────────────────────────────────────────────────────────
    // Icon button drawn with juce::Path — no font dependency
    class IconButton : public juce::Component
    {
    public:
        enum class Icon { Play, Stop, Record, ReturnToZero, StepGrid, PianoRoll, Mixer,
                          Waveform, Envelope, Functions };
        Icon icon;
        bool toggled = false;
        std::function<void()> onClick;

        explicit IconButton(Icon i) : icon(i) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

        void paint(juce::Graphics& g) override
        {
            const bool over = isMouseOver();
            juce::Colour base;
            switch (icon)
            {
                case Icon::Play:          base = juce::Colour::fromRGB(50, 200, 90);  break;
                case Icon::Stop:          base = juce::Colour::fromRGB(70, 90, 220);  break;
                case Icon::Record:        base = juce::Colour::fromRGB(210, 45, 45);  break;
                case Icon::ReturnToZero:  base = juce::Colour::fromRGB(55, 65, 100); break;
                case Icon::StepGrid:
                case Icon::PianoRoll:
                case Icon::Mixer:
                case Icon::Waveform:
                case Icon::Envelope:
                case Icon::Functions:     base = juce::Colour::fromRGB(60, 70, 95);  break;
            }
            juce::Colour bg = toggled ? base : base.darker(0.5f);
            if (over) bg = bg.brighter(0.25f);

            g.setColour(bg);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

            g.setColour(juce::Colours::white.withAlpha(0.9f));
            const float cx = getWidth()  * 0.5f;
            const float cy = getHeight() * 0.5f;
            const float s  = juce::jmin(getWidth(), getHeight()) * 0.32f;

            switch (icon)
            {
                case Icon::Play:
                {
                    juce::Path p;
                    p.addTriangle(cx - s * 0.7f, cy - s,
                                  cx - s * 0.7f, cy + s,
                                  cx + s,        cy);
                    g.fillPath(p);
                    break;
                }
                case Icon::Stop:
                {
                    g.fillRect(juce::Rectangle<float>(cx - s, cy - s, s * 2.0f, s * 2.0f));
                    break;
                }
                case Icon::Record:
                {
                    g.setColour(toggled ? juce::Colours::white : juce::Colour::fromRGB(255, 80, 80));
                    g.fillEllipse(cx - s, cy - s, s * 2.0f, s * 2.0f);
                    break;
                }
                case Icon::ReturnToZero:
                {
                    // Vertical bar + left-pointing triangle
                    g.fillRect(juce::Rectangle<float>(cx - s, cy - s, s * 0.35f, s * 2.0f));
                    juce::Path p;
                    p.addTriangle(cx + s * 0.25f, cy - s,
                                  cx + s * 0.25f, cy + s,
                                  cx - s * 0.5f,  cy);
                    g.fillPath(p);
                    break;
                }
                case Icon::StepGrid:
                {
                    // 4x2 grid of small squares — channel rack / step sequencer
                    const float cell = s * 0.62f;
                    const float gapC = s * 0.32f;
                    const float totalW = cell * 4.0f + gapC * 3.0f;
                    const float totalH = cell * 2.0f + gapC;
                    float gx = cx - totalW * 0.5f;
                    float gy = cy - totalH * 0.5f;
                    for (int row = 0; row < 2; ++row)
                        for (int col = 0; col < 4; ++col)
                            g.fillRoundedRectangle(gx + col * (cell + gapC),
                                                   gy + row * (cell + gapC),
                                                   cell, cell, 1.0f);
                    break;
                }
                case Icon::PianoRoll:
                {
                    // Stylised piano keys — white key outlines with black key overlays
                    const float kw = s * 0.72f;
                    const float kh = s * 2.0f;
                    juce::Rectangle<float> keys(cx - kw * 1.5f, cy - kh * 0.5f, kw * 3.0f, kh);
                    g.drawRoundedRectangle(keys, 1.5f, 1.2f);
                    g.drawLine(keys.getX() + kw,       keys.getY(), keys.getX() + kw,       keys.getBottom(), 1.2f);
                    g.drawLine(keys.getX() + kw * 2.0f, keys.getY(), keys.getX() + kw * 2.0f, keys.getBottom(), 1.2f);
                    g.fillRect(juce::Rectangle<float>(keys.getX() + kw * 0.62f, keys.getY(), kw * 0.5f, kh * 0.6f));
                    g.fillRect(juce::Rectangle<float>(keys.getX() + kw * 1.62f, keys.getY(), kw * 0.5f, kh * 0.6f));
                    break;
                }
                case Icon::Waveform:
                {
                    juce::Path wave;
                    const int n = 7;
                    const float ampl[n] = { 0.3f, 0.9f, 0.5f, 1.0f, 0.4f, 0.8f, 0.35f };
                    const float ww = s * 2.4f / (float)(n - 1);
                    for (int i = 0; i < n; ++i)
                    {
                        float bx = cx - s * 1.2f + i * ww;
                        float bh = s * ampl[i];
                        g.fillRect(juce::Rectangle<float>(bx - ww * 0.3f, cy - bh, ww * 0.6f, bh * 2.0f));
                    }
                    break;
                }
                case Icon::Envelope:
                {
                    juce::Path env;
                    env.startNewSubPath(cx - s, cy + s * 0.7f);
                    env.lineTo(cx - s * 0.3f, cy - s);
                    env.lineTo(cx + s * 0.2f, cy - s * 0.3f);
                    env.lineTo(cx + s,        cy + s * 0.7f);
                    g.strokePath(env, juce::PathStrokeType(1.6f));
                    break;
                }
                case Icon::Functions:
                {
                    // Simple cog: circle with small teeth
                    const float r1 = s * 0.55f;
                    g.drawEllipse(cx - r1, cy - r1, r1 * 2.0f, r1 * 2.0f, 1.6f);
                    g.fillEllipse(cx - r1 * 0.35f, cy - r1 * 0.35f, r1 * 0.7f, r1 * 0.7f);
                    for (int i = 0; i < 6; ++i)
                    {
                        float ang = (float)i / 6.0f * juce::MathConstants<float>::twoPi;
                        float tx = cx + std::cos(ang) * r1 * 1.35f;
                        float ty = cy + std::sin(ang) * r1 * 1.35f;
                        g.fillRect(juce::Rectangle<float>(tx - 1.2f, ty - 1.2f, 2.4f, 2.4f));
                    }
                    break;
                }
                case Icon::Mixer:
                {
                    // Three vertical fader strips with knobs at different heights
                    const float fw = s * 0.32f;
                    const float fh = s * 2.0f;
                    const float gapF = s * 0.55f;
                    const float startX = cx - (fw * 3.0f + gapF * 2.0f) * 0.5f;
                    const float top = cy - fh * 0.5f;
                    const float knobYs[3] = { 0.25f, 0.6f, 0.4f };
                    for (int i = 0; i < 3; ++i)
                    {
                        float fx = startX + i * (fw + gapF);
                        g.fillRect(juce::Rectangle<float>(fx + fw * 0.42f, top, fw * 0.16f, fh));
                        g.fillRoundedRectangle(fx, top + fh * knobYs[i], fw, fw, 1.0f);
                    }
                    break;
                }
            }
        }

        void mouseDown(const juce::MouseEvent&) override { repaint(); }
        void mouseUp(const juce::MouseEvent& e) override
        {
            if (getLocalBounds().contains(e.getPosition()) && onClick)
                onClick();
            repaint();
        }
        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&)  override { repaint(); }
    };

    // ─────────────────────────────────────────────────────────────────────────
    // BeatPatternToolbar — second toolbar row (pattern controls, step count, etc.)
    // ─────────────────────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────────
    // SampleEditorPopup — shown when right-clicking an instrument name
    // ─────────────────────────────────────────────────────────────────────────
    class SampleEditorPopup : public juce::Component,
                              private juce::Button::Listener
    {
    public:
        SampleEditorPopup(const juce::String& channelName, const juce::String& filePath,
                          float volume, float pan, float pitch, int mixerTrack, int numMixerTracks);
        ~SampleEditorPopup() override;
        void paint(juce::Graphics& g) override;
        void resized() override;

        // Callbacks
        std::function<void(float)> onVolumeChanged;
        std::function<void(float)> onPanChanged;
        std::function<void(float)> onPitchChanged;
        std::function<void()>      onLoadSample;
        std::function<void()>      onPreviewSample;     // play/stop the loaded sample once
        std::function<void(int)>   onMixerTrackChanged; // routing target changed

        // FL-style "channel settings" engine — fired whenever the user edits a
        // control on the Engine tab. Each callback ships the full parameter set
        // for its section so the host can forward it straight to the audio engine.
        std::function<void(bool enabled, float attack, float hold, float decay, float sustain, float release)> onEnvelopeChanged;
        std::function<void(bool enabled, int filterType, float cutoffHz, float resonance)>                     onFilterChanged;
        std::function<void(bool enabled, int target, float rateHz, float depth)>                               onLFOChanged;
        std::function<void(float startFrac, float endFrac, bool loop)>                                         onSampleRegionChanged;

        // Initialise the Engine tab controls from persisted channel state without
        // firing the on*Changed callbacks (call right after construction).
        void initEngineState(bool envEnabled, float attack, float hold, float decay, float sustain, float release,
                             bool filterEnabled, int filterType, float cutoffHz, float resonance,
                             bool lfoEnabled, int lfoTarget, float lfoRate, float lfoDepth,
                             float sampleStart, float sampleEnd, bool loopEnabled);

    private:
        void buttonClicked(juce::Button* b) override;
        void showTab(int index);
        void styleKnob(juce::Slider& s, double lo, double hi, double val);
        void styleSmallKnob(juce::Slider& s, double lo, double hi, double val);
        void fireEnvelopeChanged();
        void fireFilterChanged();
        void fireLFOChanged();
        void fireSampleRegionChanged();

        enum { kTabSample = 0, kTabEnvelope = 1, kTabFunctions = 2, kTabEngine = 3 };

        juce::String name, path;
        float vol, panVal, pitchVal;
        int   trackRouting, numTracks;
        int   activeTab = kTabSample;

        // Tab bar (FL Studio style icon tabs)
        IconButton sampleTabBtn   { IconButton::Icon::Waveform };
        IconButton envelopeTabBtn { IconButton::Icon::Envelope };
        IconButton functionsTabBtn{ IconButton::Icon::Functions };
        IconButton engineTabBtn   { IconButton::Icon::Mixer };
        IconButton previewBtn     { IconButton::Icon::Play };

        // Tab 0: Sample — waveform + file + load
        juce::Label  fileLabel;
        juce::TextButton loadBtn { "Load Sample..." };

        // Tab 1: Envelope — knobs (replaces sliders)
        juce::Slider volumeKnob, panKnob, pitchKnob;
        juce::Label  volLabel, panLabel, pitchLabel;

        // Tab 2: Functions — mixer routing
        juce::ComboBox routingBox;
        juce::Label    routingLabel;

        // Tab 3: Engine — FL-style AHDSR / filter / LFO / sample region
        juce::ToggleButton envEnableToggle    { "AHDSR Envelope" };
        juce::Slider attackKnob, holdKnob, decayKnob, sustainKnob, releaseKnob;
        juce::Label  attackLabel, holdLabel, decayLabel, sustainLabel, releaseLabel;

        juce::ToggleButton filterEnableToggle { "Filter" };
        juce::ComboBox     filterTypeBox;
        juce::Slider cutoffKnob, resonanceKnob;
        juce::Label  cutoffLabel, resonanceLabel;

        juce::ToggleButton lfoEnableToggle    { "LFO" };
        juce::ComboBox     lfoTargetBox;
        juce::Slider lfoRateKnob, lfoDepthKnob;
        juce::Label  lfoRateLabel, lfoDepthLabel;

        juce::ToggleButton loopEnableToggle   { "Loop region" };
        juce::Slider sampleStartKnob, sampleEndKnob;
        juce::Label  sampleStartLabel, sampleEndLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleEditorPopup)
    };

    class BeatPatternToolbar : public juce::Component,
                               private juce::Button::Listener
    {
    public:
        BeatPatternToolbar();
        ~BeatPatternToolbar() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;

        void setPatternName(const juce::String& name);
        void setStepCount(int steps);
        void setSwing(float pct);
        void setGroove(int templateIndex);

        // Callbacks
        std::function<void()>                    onPrevPattern;
        std::function<void()>                    onNextPattern;
        std::function<void()>                    onAddPattern;
        std::function<void()>                    onAddChannel;
        std::function<void(int)>                 onStepCountChanged;
        std::function<void(float)>               onSwingChanged;
        std::function<void(int)>                 onGrooveChanged;   // index into StudioAudioEngine::StepSequencerState groove templates
        std::function<void(bool)>                onShowVelocityGraph;
        std::function<void(bool)>                onPatSongToggle;
        std::function<void()>                    onPerformModeToggled;
        // Pattern dropdown actions: "rename","color","randomcolor","insert","clone","delete","moveup","movedown","findempty"
        std::function<void(const juce::String&)> onPatternMenuAction;

    private:
        void buttonClicked(juce::Button* b) override;
        void showPatternDropdown();

        juce::TextButton prevPatBtn  { "<" };
        juce::TextButton nextPatBtn  { ">" };
        juce::TextButton addPatBtn   { "+" };
        juce::TextButton patDropBtn  { "v" };  // dropdown arrow on pattern name
        juce::Label      patNameLabel;

        juce::TextButton steps16Btn  { "16" };
        juce::TextButton steps32Btn  { "32" };
        juce::TextButton steps64Btn  { "64" };

        juce::Label      swingLabel;
        juce::TextButton swingDnBtn  { "-" };
        juce::TextButton swingUpBtn  { "+" };
        juce::TextButton grooveBtn   { "GROOVE: Straight" };
        juce::TextButton performBtn  { "PERFORM" };

        juce::TextButton addChanBtn  { "+ Chan" };
        juce::TextButton velGraphBtn { "VEL" };
        juce::TextButton patSongBtn  { "PAT" };

        int   currentSteps = 16;
        float currentSwing = 0.0f;
        int   currentGroove = 0;
        void  showGrooveMenu();
        bool  showVel      = true;
        bool  isPat        = true;

        void styleToggleBtn(juce::TextButton& btn);
        void updateStepButtons();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatPatternToolbar)
    };

    class BeatTransportBar : public juce::Component
    {
    public:
        BeatTransportBar();
        ~BeatTransportBar() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // Update displayed state from outside
        void setPlayState(bool playing, bool recording);
        void setTimecode(const juce::String& tc) { timecodeLabel.setText(tc, juce::dontSendNotification); }
        void setBpm(double bpm)                  { bpmLabel.setBPM(static_cast<int>(bpm)); }

        // Callbacks wired by BeatWindow owner
        std::function<void()>    onPlay;
        std::function<void()>    onStop;
        std::function<void()>    onRecord;
        std::function<void()>    onReturnToZero;
        std::function<void(int)> onTempoChanged;

        // View-switcher callbacks (FL Studio-style top icon row)
        std::function<void()>    onShowStepSequencer;
        std::function<void()>    onShowPianoRoll;
        std::function<void()>    onShowMixer;

        // Set which view-switcher icon is highlighted (0 = step seq, 1 = piano roll)
        void setActiveView(int viewIndex);

    private:
        IconButton rtzBtn  { IconButton::Icon::ReturnToZero };
        IconButton playBtn { IconButton::Icon::Play };
        IconButton stopBtn { IconButton::Icon::Stop };
        IconButton recBtn  { IconButton::Icon::Record };
        juce::Label timecodeLabel;
        BPMLabel    bpmLabel;

        // FL Studio-style view-switcher icons (right side of bar)
        IconButton stepGridViewBtn { IconButton::Icon::StepGrid };
        IconButton pianoRollViewBtn{ IconButton::Icon::PianoRoll };
        IconButton mixerViewBtn    { IconButton::Icon::Mixer };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatTransportBar)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // BeatWindow — top-level
    // ─────────────────────────────────────────────────────────────────────────
    class BeatWindow : public juce::Component,
                       private juce::Timer
    {
    public:
        explicit BeatWindow(NovaStudio::TransportState& transport);
        ~BeatWindow() override;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        // Engine wiring — call after construction
        void setOnSampleAssigned(std::function<void(int, const juce::String&)> fn)
            { stepSeq.onSampleAssigned = std::move(fn); }
        void setOnStepChanged(std::function<void(int, int, bool)> fn)
            { stepSeq.onStepChanged = std::move(fn); }
        void setOnVelocityChanged(std::function<void(int,int,float)> fn)
            { stepSeq.onVelocityChanged = std::move(fn); }
        void setOnRowVolumeChanged(std::function<void(int,float)> fn)
            { stepSeq.onRowVolumeChanged = std::move(fn); }
        void setOnRowPanChanged(std::function<void(int,float)> fn)
            { stepSeq.onRowPanChanged = std::move(fn); }
        void setOnRowPitchChanged(std::function<void(int,float)> fn)
            { stepSeq.onRowPitchChanged = std::move(fn); }
        void setOnRowEnvelopeChanged(std::function<void(int,bool,float,float,float,float,float)> fn)
            { stepSeq.onRowEnvelopeChanged = std::move(fn); }
        void setOnRowFilterChanged(std::function<void(int,bool,int,float,float)> fn)
            { stepSeq.onRowFilterChanged = std::move(fn); }
        void setOnRowLFOChanged(std::function<void(int,bool,int,float,float)> fn)
            { stepSeq.onRowLFOChanged = std::move(fn); }
        void setOnRowSampleRegionChanged(std::function<void(int,float,float,bool)> fn)
            { stepSeq.onRowSampleRegionChanged = std::move(fn); }
        void setOnRowMutedChanged(std::function<void(int,bool)> fn)
            { stepSeq.onRowMutedChanged = std::move(fn); }
        void setOnRowMixerTrackChanged(std::function<void(int,int)> fn)
            { stepSeq.onRowMixerTrackChanged = std::move(fn); }
        void setOnPreviewSample(std::function<void(const juce::String&)> fn)
            { stepSeq.onPreviewSample = std::move(fn); }
        void setNumMixerInserts(int n) { stepSeq.numMixerInserts = n; }

        // Compact in-window mixer — populated by MainComponent from the session
        void setMixerTrackCount(int n) { compactMixer.setNumTracks(n); }
        void setMixerTrackInfo(int index, const juce::String& name, juce::Colour colour,
                               float volumeDb, float pan, bool muted)
            { compactMixer.setTrackInfo(index, name, colour, volumeDb, pan, muted); }
        void setOnMixerVolumeChanged(std::function<void(int,float)> fn)
            { compactMixer.onVolumeChanged = std::move(fn); }
        void setOnMixerPanChanged(std::function<void(int,float)> fn)
            { compactMixer.onPanChanged = std::move(fn); }
        void setOnMixerMuteToggled(std::function<void(int,bool)> fn)
            { compactMixer.onMuteToggled = std::move(fn); }
        void setOnStepCountChanged(std::function<void(int)> fn)
            { stepSeq.onStepCountChanged = std::move(fn); }

        // Performance Mode — host provides a way to read the engine's current
        // step index so pattern launches can be quantized to the loop boundary.
        std::function<int()> getCurrentStep;
        void setOnSwingChanged(std::function<void(float)> fn)
            { stepSeq.onSwingChanged = std::move(fn); }
        void setOnGrooveChanged(std::function<void(int)> fn)
            { stepSeq.onGrooveChanged = std::move(fn); }

        // Transport callbacks — wire from MainComponent
        std::function<void()>    onPlay;
        std::function<void()>    onStop;
        std::function<void()>    onRecord;
        std::function<void()>    onReturnToZero;
        std::function<void(int)> onTempoChanged;
        std::function<void()>    onShowMixer;

        // PAT = loop step sequencer, SONG = play full arrangement
        bool isPatMode() const { return patMode; }

        // Update displayed transport state
        void setPlayState(bool playing, bool recording);
        void setTimecode(const juce::String& tc);
        void setBpm(double bpm);

        // Update playback cursor (call from MainComponent timer)
        void setPlayCursor(int step) { stepSeq.setPlayCursor(step); }

        // Show the same File/Edit/... menu bar that the edit window uses
        // (so the beat window feels consistent whether docked or floating)
        void setMenuBarModel(juce::MenuBarModel* model);

    private:
        std::unique_ptr<juce::MenuBarComponent> menuBar;
        static constexpr int kMenuBarH = 24;

        BeatTransportBar   transportBar;
        BeatPatternToolbar patternToolbar;
        BrowserPanel       browser;
        PatternPlaylist    playlist;
        StepSequencerView  stepSeq;
        PianoRollView      pianoRoll;
        CompactMixerView   compactMixer;

        // Step sequencer ("Channel Rack") and Mixer float over the main view,
        // FL Studio-style, instead of being permanently docked.
        FloatingSubPanel   stepSeqPanel { "Channel Rack" };
        FloatingSubPanel   mixerPanel   { "Mixer" };
        FloatingSubPanel   performPanel { "Performance" };
        PatternLaunchGrid  launchGrid;

        juce::Rectangle<int> canvasArea;     // area below the playlist where panels float
        bool               showingPianoRoll  = false;
        bool               stepSeqPanelOpen  = true;   // Channel Rack floats open by default
        bool               mixerPanelOpen    = false;
        bool               performPanelOpen  = false;
        bool               stepSeqPanelPositioned = false;
        bool               mixerPanelPositioned   = false;
        bool               performPanelPositioned = false;
        bool               patMode = true;  // PAT=true, SONG=false

        // Performance Mode: queue a pattern switch to fire on the next loop
        // boundary (detected by polling the engine's current step for wrap-to-0).
        int  queuedPatternIndex  = -1;
        int  lastSeenStep        = -1;
        void checkPerformanceLaunch();
        void firePatternLaunch(int patternIndex);
        void refreshLaunchGrid();

        void toggleFloatingPanel(FloatingSubPanel& panel, bool& flag,
                                 juce::Rectangle<int> defaultBounds);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatWindow)
    };
}
