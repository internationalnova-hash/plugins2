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
        static constexpr int kRowH      = 34;
        static constexpr int kToolbarH  = 32;
        static constexpr int kGraphH    = 70;    // graph editor height

        // ── Data model ───────────────────────────────────────────────────────
        struct ChannelData
        {
            juce::String name;
            juce::Colour colour;
            float volume = 1.0f;
            float pan    = 0.0f;
            bool  muted  = false;
            bool  steps[kMaxSteps] {};
            float velocities[kMaxSteps];
            ChannelData() { std::fill(velocities, velocities + kMaxSteps, 1.0f); }
        };

        struct Pattern
        {
            juce::String name;
            int   stepCount = 16;
            float swing     = 0.0f;
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
        std::function<void(int row, bool muted)>               onRowMutedChanged;
        std::function<void(int stepCount)>                     onStepCountChanged;
        std::function<void(float swing)>                       onSwingChanged;

        // External step cursor update (called from MainComponent timer or getStepSeqCurrentStep)
        void setPlayCursor(int step) { if (step != cursorStep) { cursorStep = step; repaint(); } }

        // Called by inline rename editor when done
        void finishInlineRename(bool accepted);

    private:
        // ── Timer ────────────────────────────────────────────────────────────
        void timerCallback() override;

        // ── Drawing helpers ──────────────────────────────────────────────────
        void paintToolbar(juce::Graphics& g, juce::Rectangle<int> r);
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
        enum class Icon { Play, Stop, Record, ReturnToZero };
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
    class SampleEditorPopup : public juce::Component
    {
    public:
        SampleEditorPopup(const juce::String& channelName, const juce::String& filePath,
                          float volume, float pan, float pitch);
        void paint(juce::Graphics& g) override;
        void resized() override;

        // Callbacks
        std::function<void(float)> onVolumeChanged;
        std::function<void(float)> onPanChanged;
        std::function<void(float)> onPitchChanged;
        std::function<void()>      onLoadSample;

    private:
        juce::String name, path;
        float vol, panVal, pitchVal;

        juce::Slider volumeSlider, panSlider, pitchSlider;
        juce::Label  volLabel, panLabel, pitchLabel, fileLabel;
        juce::TextButton loadBtn { "Load Sample..." };

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

        // Callbacks
        std::function<void()>                    onPrevPattern;
        std::function<void()>                    onNextPattern;
        std::function<void()>                    onAddPattern;
        std::function<void()>                    onAddChannel;
        std::function<void(int)>                 onStepCountChanged;
        std::function<void(float)>               onSwingChanged;
        std::function<void(bool)>                onShowVelocityGraph;
        std::function<void(bool)>                onPatSongToggle;
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

        juce::TextButton addChanBtn  { "+ Chan" };
        juce::TextButton velGraphBtn { "VEL" };
        juce::TextButton patSongBtn  { "PAT" };

        int   currentSteps = 16;
        float currentSwing = 0.0f;
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

    private:
        IconButton rtzBtn  { IconButton::Icon::ReturnToZero };
        IconButton playBtn { IconButton::Icon::Play };
        IconButton stopBtn { IconButton::Icon::Stop };
        IconButton recBtn  { IconButton::Icon::Record };
        juce::Label timecodeLabel;
        BPMLabel    bpmLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatTransportBar)
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
        void setOnRowMutedChanged(std::function<void(int,bool)> fn)
            { stepSeq.onRowMutedChanged = std::move(fn); }
        void setOnStepCountChanged(std::function<void(int)> fn)
            { stepSeq.onStepCountChanged = std::move(fn); }
        void setOnSwingChanged(std::function<void(float)> fn)
            { stepSeq.onSwingChanged = std::move(fn); }

        // Transport callbacks — wire from MainComponent
        std::function<void()>    onPlay;
        std::function<void()>    onStop;
        std::function<void()>    onRecord;
        std::function<void()>    onReturnToZero;
        std::function<void(int)> onTempoChanged;

        // PAT = loop step sequencer, SONG = play full arrangement
        bool isPatMode() const { return patMode; }

        // Update displayed transport state
        void setPlayState(bool playing, bool recording);
        void setTimecode(const juce::String& tc);
        void setBpm(double bpm);

        // Update playback cursor (call from MainComponent timer)
        void setPlayCursor(int step) { stepSeq.setPlayCursor(step); }

    private:
        BeatTransportBar   transportBar;
        BeatPatternToolbar patternToolbar;
        BrowserPanel       browser;
        PatternPlaylist    playlist;
        StepSequencerView  stepSeq;
        bool               patMode = true;  // PAT=true, SONG=false

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeatWindow)
    };
}
