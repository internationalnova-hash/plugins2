#pragma once

#include <JuceHeader.h>
#include "../Session.h"
#include "../TransportState.h"
#include "../TimelineModel.h"
#include "../ArrangementModel.h"
#include "../StudioAudioEngine.h"
#include "WaveformCache.h"
#include "AnimationUtils.h"
#include "Theme.h"

namespace NovaStudioUI
{
    // ── BPMLabel ────────────────────────────────────────────────────────────────
    // A Label that also supports drag-up/down to change BPM and double-click
    // to type a value. Fires onTempoChanged(int bpm) on any change.
    class BPMLabel : public juce::Label
    {
    public:
        std::function<void(int)> onTempoChanged;

        BPMLabel()
        {
            setEditable(false, true); // double-click opens text editor
            onTextChange = [this]
            {
                int newBpm = getText().retainCharacters("0123456789").getIntValue();
                newBpm = juce::jlimit(20, 999, newBpm);
                currentBPM = newBpm;
                setText(juce::String(newBpm) + " BPM", juce::dontSendNotification);
                if (onTempoChanged) onTempoChanged(newBpm);
            };
        }

        void setBPM(int bpm)
        {
            currentBPM = juce::jlimit(20, 999, bpm);
            setText(juce::String(currentBPM) + " BPM", juce::dontSendNotification);
        }

        int getBPM() const { return currentBPM; }

        void mouseDown(const juce::MouseEvent& e) override
        {
            dragStartY   = e.y;
            dragStartBPM = currentBPM;
            isDragging   = false;
            juce::Label::mouseDown(e);
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            int delta = dragStartY - e.y; // drag up = increase
            if (std::abs(delta) > 3)
            {
                isDragging = true;
                int newBpm = juce::jlimit(20, 999, dragStartBPM + delta / 2);
                if (newBpm != currentBPM)
                {
                    currentBPM = newBpm;
                    setText(juce::String(currentBPM) + " BPM", juce::dontSendNotification);
                    if (onTempoChanged) onTempoChanged(currentBPM);
                }
            }
        }

        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            int newBpm = juce::jlimit(20, 999, currentBPM + (wheel.deltaY > 0 ? 1 : -1));
            if (newBpm != currentBPM)
            {
                currentBPM = newBpm;
                setText(juce::String(currentBPM) + " BPM", juce::dontSendNotification);
                if (onTempoChanged) onTempoChanged(currentBPM);
            }
        }

    private:
        int  currentBPM  = 120;
        int  dragStartBPM = 120;
        int  dragStartY   = 0;
        bool isDragging   = false;
    };

    class TransportBar : public juce::Component,
                         private juce::Button::Listener,
                         private juce::Timer
    {
    public:
        TransportBar();
        ~TransportBar() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        std::function<void()> onPlay;
        std::function<void()> onStop;
        std::function<void()> onRecord;
        std::function<void()> onReturnToZero;
        std::function<void()> onMonitor;
        std::function<void()> onLoop;
        std::function<void()> onPunchToggle;

        void setTempo(int bpm);
        void setTimecode(const juce::String& timecode);
        void setPlayState(bool isPlaying, bool isRecording);
        void setLoopState(bool enabled);
        void setMonitorState(bool enabled);
        void setPlaybackState(bool previewEnabled, bool hasPreview);
        void setPunchState(bool enabled);
        std::function<void(bool)> onTogglePreview;
        std::function<void(int)>  onTempoChanged;

    private:
        void buttonClicked(juce::Button* button) override;
        void timerCallback() override;

        juce::TextButton rtzButton     {};
        juce::TextButton playButton    {};
        juce::TextButton stopButton    {};
        juce::TextButton recordButton  {};
        juce::TextButton loopButton    {};
        juce::TextButton monitorButton {};
        juce::TextButton punchButton    { "PUNCH" };

        BPMLabel tempoLabel;
        juce::Label timeLabel;
        juce::Label playbackLabel;
        juce::TextButton playbackToggleButton { "Toggle" };

        NovaStudioUI::AnimatedValue hoverAlpha;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
    };

    class TrackPanel : public juce::Component,
                       private juce::Timer
    {
    public:
        static constexpr int kTrackHeight = 64;

        explicit TrackPanel(NovaStudio::Session& session);
        ~TrackPanel() override;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
        void refresh() { repaint(); }

        void setScrollY(int y) { scrollY = y; repaint(); }
        int  getScrollY() const { return scrollY; }

        std::function<void(int, bool)> onTrackArm;
        std::function<void(int, bool)> onTrackMute;
        std::function<void(int, bool)> onTrackSolo;
        std::function<void(int trackIndex, const juce::String& newName)> onTrackRenamed;
        std::function<void(int scrollY)> onScrollChanged;
        std::function<void()> onAddTrackClicked;
        // Wire to engine.getTrackPeakLevel(trackIndex, channel) — 0=L, 1=R
        std::function<float(int trackIndex, int channel)> getTrackLevel;

        void mouseDoubleClick(const juce::MouseEvent& e) override;

    private:
        void timerCallback() override { repaint(); }

        enum class HitButton { None, Arm, Mute, Solo };
        HitButton hitTest(int trackIndex, juce::Point<int> pos) const;

        NovaStudio::Session& session;
        int scrollY = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackPanel)
    };

    class InspectorPanel : public juce::Component,
                           private juce::Slider::Listener,
                           private juce::Button::Listener,
                           private juce::ChangeListener
    {
    public:
        explicit InspectorPanel(NovaStudio::ArrangementModel& arrangementModel);
        ~InspectorPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    private:
        void refreshContent();
        void sliderValueChanged(juce::Slider* slider) override;
        void buttonClicked(juce::Button* button) override;

        NovaStudio::ArrangementModel& arrangementModel;
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        juce::Label titleLabel;
        juce::Label selectedClipLabel;
        juce::Label trackInfoLabel;
        juce::Slider gainSlider;
        juce::Label gainLabel;
        juce::Slider fadeInSlider;
        juce::Slider fadeOutSlider;
        juce::Label fadeInLabel;
        juce::Label fadeOutLabel;
        juce::ToggleButton muteToggle {"Mute"};
        juce::ToggleButton lockToggle {"Lock"};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
    };

    // =========================================================================
    // ProductionPanel — full channel strip replacing InspectorPanel
    // =========================================================================
    class ProductionPanel : public juce::Component,
                            private juce::Slider::Listener,
                            private juce::Button::Listener,
                            private juce::Timer
    {
    public:
        explicit ProductionPanel(NovaStudio::ArrangementModel& arrangementModel);
        ~ProductionPanel() override;

        void setMeterLevels(float L, float R)
        {
            meterLevelL = juce::jmax(meterLevelL, L);
            meterLevelR = juce::jmax(meterLevelR, R);
            repaint();
        }

        // Called every timer tick to pull live meter levels from the engine
        std::function<float(int)> getMeterLevel;  // arg: 0=L, 1=R
        std::function<void(float)> onVolumeChanged;

        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        // Called when selected track changes
        void updateFromTrack(const NovaStudio::Track& track);

        // Insert rack API
        void setInsertSlotName(int slot, const juce::String& name);

        // Callbacks — wire these up in EditWindow
        std::function<void(int slot)>                                     onInsertClicked;
        std::function<void(int slot)>                                     onInsertChangePlugin;
        std::function<void(int slot)>                                     onInsertRemovePlugin;
        std::function<void(int band, float freq, float gainDb, float q)>  onEQChanged;
        std::function<void(int send, float level)>                        onSendLevelChanged;
        std::function<void(int send, int busIndex)>                       onSendBusChanged;
        std::function<void(int send, bool preFader)>                      onSendPreFaderChanged;

    private:
        void timerCallback() override
        {
            if (getMeterLevel)
            {
                meterLevelL = juce::jmax(meterLevelL * 0.88f, getMeterLevel(0));
                meterLevelR = juce::jmax(meterLevelR * 0.88f, getMeterLevel(1));
            }
            else
            {
                meterLevelL *= 0.88f;
                meterLevelR *= 0.88f;
            }
            repaint();  // Always repaint at 30 Hz so the meter bar is always visible
        }

        float meterLevelL = 0.0f;
        float meterLevelR = 0.0f;

        struct EQBand { bool enabled = true; float freq = 1000.0f; float gain = 0.0f; float q = 0.707f; };
        static constexpr int kNumBands   = 6;
        static constexpr int kNumInserts = 8;
        static constexpr int kNumSends   = 4;

        void sliderValueChanged(juce::Slider* s) override;
        void buttonClicked(juce::Button* b) override;

        void paintSectionHeader(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title);
        void paintEQGraph(juce::Graphics& g, juce::Rectangle<int> r);
        void paintMeter(juce::Graphics& g, juce::Rectangle<int> r);
        double calcBiquadMagnitude(int band, double normFreq) const;

        NovaStudio::ArrangementModel& arrangementModel;

        // ---- Section 1: Track Channel ----
        juce::Label      trackNameLabel;
        juce::Label      inputLabel, outputLabel, gainDbLabel;
        juce::TextButton muteBtn  {"M"};
        juce::TextButton soloBtn  {"S"};
        juce::TextButton armBtn   {"R"};

        // ---- Section 2: Inserts ----
        struct InsertSlot
        {
            juce::TextButton bypassBtn  {""};
            juce::TextButton nameBtn    {"Empty Slot"};
            juce::String     pluginName;
            bool             bypassed = false;
        };
        std::array<InsertSlot, kNumInserts> insertSlots;

        // ---- Section 3: EQ ----
        std::array<EQBand, kNumBands> eqBands;
        struct BandControls
        {
            juce::Label    bandLabel;
            juce::Slider   freqSlider  { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
            juce::Slider   gainSlider  { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
            juce::Slider   qSlider     { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
            juce::TextButton enableBtn {"E"};
        };
        std::array<BandControls, kNumBands> bandControls;
        juce::Rectangle<int> eqGraphBounds;

        // ---- Section 4: Sends ----
        struct SendRow
        {
            juce::TextButton busBtn    { "—" };     // shows assigned bus, click to pick
            juce::Slider     levelSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
            juce::TextButton preBtn    { "POST" };  // toggles PRE/POST
            int  assignedBus = -1;
            bool isPreFader  = false;
        };
        std::array<SendRow, kNumSends> sendRows;

        // EQ drag state
        int   dragBandIndex = -1;
        float dragStartFreq = 0.0f;
        float dragStartGain = 0.0f;
        juce::Point<float> dragStartPos;

        // Scrollable content
        juce::Viewport viewport;
        juce::Component contentComp;

        juce::Slider volumeKnob { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

        // Bounds (in contentComp space) for the horizontal meter — used by paint() to blit it
        juce::Rectangle<int> meterBounds;

        // Current track state
        float currentVolDb  = 0.0f;
        bool  currentMuted  = false;
        bool  currentSoloed = false;
        bool  currentArmed  = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProductionPanel)
    };

    // Toolbar that sits directly above the arrangement view — edit modes, cursor tools, grid, zoom
    class EditModeToolbar : public juce::Component,
                            private juce::Button::Listener,
                            private juce::ComboBox::Listener
    {
    public:
        enum class EditMode  { Slip, Grid, Spot, Shuffle };
        enum class CursorTool { Pointer, Trim, Split, Fade, Smart };

        EditModeToolbar();
        ~EditModeToolbar() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // Callbacks → ArrangementView
        std::function<void(EditMode)>   onEditModeChanged;
        std::function<void(CursorTool)> onCursorToolChanged;
        std::function<void(double)>     onSnapResolutionChanged;
        std::function<void(double)>     onNudgeResolutionChanged;
        std::function<void(bool)>       onSnapEnabled;
        std::function<void(int)>        onHZoomChanged;
        std::function<void(int)>        onVZoomChanged;

        void setEditMode(EditMode m);
        void setCursorTool(CursorTool t);
        EditMode   getEditMode() const   { return currentMode; }
        CursorTool getCursorTool() const { return currentTool; }
        double getSnapBeats() const      { return snapBeats; }
        double getNudgeBeats() const     { return nudgeBeats; }
        bool isSnapOn() const            { return snapEnabled; }

    private:
        void buttonClicked(juce::Button* b) override;
        void comboBoxChanged(juce::ComboBox* c) override;
        static double beatsForResolutionId(int id);

        // Edit mode buttons
        juce::TextButton slipBtn    {"SLIP"};
        juce::TextButton gridBtn    {"GRID"};
        juce::TextButton spotBtn    {"SPOT"};
        juce::TextButton shuffleBtn {"SHFL"};

        // Cursor tool buttons
        juce::TextButton pointerBtn {"PTR"};
        juce::TextButton trimBtn    {"TRIM"};
        juce::TextButton splitBtn   {"SPLT"};
        juce::TextButton fadeBtn    {"FADE"};

        // Snap / grid
        juce::TextButton snapBtn {"SNAP"};
        juce::ComboBox   gridBox;
        juce::ComboBox   nudgeBox;

        // Zoom
        juce::TextButton hZoomInBtn  {"H+"};
        juce::TextButton hZoomOutBtn {"H-"};
        juce::TextButton vZoomInBtn  {"V+"};
        juce::TextButton vZoomOutBtn {"V-"};

        juce::Label modeLabel, toolLabel, gridLabel, nudgeLabel, zoomLabel;

        EditMode   currentMode = EditMode::Slip;
        CursorTool currentTool = CursorTool::Pointer;
        double     snapBeats   = 4.0;
        double     nudgeBeats  = 0.25;
        bool       snapEnabled = true;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditModeToolbar)
    };

    class ArrangementView : public juce::Component,
                            public juce::DragAndDropTarget,
                            private juce::ChangeListener,
                            private juce::Timer
    {
    public:
        ArrangementView(NovaStudio::TransportState& transportState,
                        NovaStudio::TimelineModel& timelineModel,
                        NovaStudio::ArrangementModel& arrangementModel);
        ~ArrangementView() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        bool keyPressed(const juce::KeyPress& key) override;

        // DragAndDropTarget
        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDropped(const SourceDetails& details) override;

        // Called from EditModeToolbar callbacks
        void setEditMode(EditModeToolbar::EditMode m)    { editMode = m; updateCursorForTool(); }
        void setCursorTool(EditModeToolbar::CursorTool t){ cursorTool = t; updateCursorForTool(); }
        void setSnapResolution(double beats)             { snapBeats = beats; }
        void setNudgeResolution(double beats)            { nudgeBeats = beats; }
        void setSnapEnabled(bool on)                     { snapEnabled = on; }
        void adjustHZoom(int direction);
        void adjustVZoom(int direction);
        int  getTrackHeight() const { return trackHeightPx; }

        void nudgeSelected(int direction); // +1 right, -1 left

    private:
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        void timerCallback() override;
        int64_t snapToGrid(int64_t sample) const;
        void showSpotDialog(int trackIndex, int clipIndex);
        void updateCursorForTool();

        // Classify what's under the mouse for smart-tool behaviour
        enum class HoverZone { None, ClipBody, ClipLeftEdge, ClipRightEdge, ClipFadeIn, ClipFadeOut };
        HoverZone hoverZone(juce::Point<float> pos, int& outTrack, int& outClip) const;

        NovaStudio::TransportState& transportState;
        NovaStudio::TimelineModel& timelineModel;
        NovaStudio::ArrangementModel& arrangementModel;
        WaveformCache waveformCache;

        bool isDraggingClip      = false;
        int  dragStartX          = 0;
        int  dragSourceTrackIndex = -1;
        int  dragTargetTrackIndex = -1;
        int64_t originalClipStartSample = 0;
        int64_t originalClipLength      = 0;
        bool isDraggingTrimLeft  = false;
        bool isDraggingTrimRight = false;
        int64_t currentTrimSample = 0;
        double  zoomFactor        = 1.0;
        int     trackHeightPx     = 64; // vertical zoom
        bool isMarqueeSelecting   = false;
        juce::Point<float> marqueeStart {0.0f, 0.0f};
        juce::Rectangle<float> marqueeRect {0.0f, 0.0f, 0.0f, 0.0f};

        // Playhead scrubbing
        bool isDraggingPlayhead = false;

        // Timeline range / loop selection — click-drag in ruler
        bool    isDraggingRangeSelect = false;
        int64_t rangeAnchorSample     = 0;

        // Loop brace dragging
        enum class LoopDragHandle { None, Start, End, Body };
        LoopDragHandle loopDragHandle = LoopDragHandle::None;
        int64_t loopDragStartSample = 0; // position at drag start
        int64_t loopOrigStart = 0;
        int64_t loopOrigEnd   = 0;

        EditModeToolbar::EditMode   editMode   = EditModeToolbar::EditMode::Slip;
        EditModeToolbar::CursorTool cursorTool = EditModeToolbar::CursorTool::Pointer;
        double  snapBeats   = 4.0;
        double  nudgeBeats  = 0.25;
        bool    snapEnabled = true;

        // Fade dragging state
        bool    isDraggingFadeIn  = false;
        bool    isDraggingFadeOut = false;
        int64_t fadeOrigIn  = 0;
        int64_t fadeOrigOut = 0;

        // Punch range drag (Cmd+drag in ruler)
        bool    isDraggingPunchRange = false;
        int64_t punchAnchorSample    = 0;

        // Vertical scroll (shared with TrackPanel via onScrollChanged)
        int trackScrollY = 0;

    public:
        void setTrackScrollY(int y) { trackScrollY = y; repaint(); }
        int  getTrackScrollY() const { return trackScrollY; }
        std::function<void(int)> onScrollChanged;  // fires when ArrangementView scrolls
        // Called when a file drop needs a new track: (name, isStereo) -> new track index
        std::function<int(const juce::String&, bool)> onCreateAudioTrack;
        // Called when Cmd+drag in ruler sets a punch range (in, out samples)
        std::function<void(int64_t, int64_t)> onPunchRangeChanged;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangementView)
    };

    class MixerPanel : public juce::Component
    {
    public:
        MixerPanel();
        ~MixerPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
    };

    // Bottom dock: mixer channels + piano roll + step sequencer all visible simultaneously
    class BottomDockPanel : public juce::Component,
                            private juce::Button::Listener,
                            private juce::Timer
    {
    public:
        BottomDockPanel();
        ~BottomDockPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        // Wire up engine so the dock can show real session channels and live levels.
        void setEngine(NovaStudio::StudioAudioEngine& e);

    private:
        void buttonClicked(juce::Button*) override {}
        void timerCallback() override;
        void paintMixerStrips(juce::Graphics& g, juce::Rectangle<int> area);
        void paintPianoRoll(juce::Graphics& g, juce::Rectangle<int> area);
        void paintStepSequencer(juce::Graphics& g, juce::Rectangle<int> area);

        // Returns strip index and fader hit (-1 if none)
        int stripIndexAt(int x) const;
        bool faderHitTest(int stripIndex, juce::Point<int> pos, juce::Rectangle<int> mixerArea) const;

        juce::TextButton mixerTab{"MIXER"}, channelsTab{"CHANNELS"},
                         effectsTab{"EFFECTS"},  metersTab{"METERS"};

        static constexpr int kStripW   = 68;
        static constexpr int kMaxStrips = 32;  // max session tracks we'll show

        NovaStudio::StudioAudioEngine* enginePtr = nullptr;

        // Per-strip state (sized to kMaxStrips; actual count from session)
        float faderPositions[kMaxStrips] = {};
        float panPositions  [kMaxStrips] = {};
        float peakLevelL    [kMaxStrips] = {};
        float peakLevelR    [kMaxStrips] = {};

        int   activeFaderStrip  = -1;
        int   faderDragStartY   = 0;
        float faderDragStartPos = 0.0f;
        int   activePanStrip    = -1;
        int   panDragStartX     = 0;
        float panDragStartPos   = 0.5f;

        bool stepStates[6][16] = {};  // step sequencer toggle state

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomDockPanel)
    };

    // Filter: shows directories + audio files only
    class AudioDirFilter : public juce::FileFilter
    {
    public:
        AudioDirFilter() : FileFilter("Audio") {}
        bool isFileSuitable(const juce::File& f) const override
        {
            static const char* exts[] = {".wav",".aif",".aiff",".mp3",".flac",".ogg",".m4a",nullptr};
            auto ext = f.getFileExtension().toLowerCase();
            for (int i = 0; exts[i]; ++i) if (ext == exts[i]) return true;
            return false;
        }
        bool isDirectorySuitable(const juce::File&) const override { return true; }
    };

    // FileTreeComponent subclass that initiates DnD on drag of an audio file
    class DraggableFileTree : public juce::FileTreeComponent
    {
    public:
        explicit DraggableFileTree(juce::DirectoryContentsList& d)
            : juce::FileTreeComponent(d) {}
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() > 5)
            {
                const auto sel = getSelectedFile(0);
                if (sel.existsAsFile())
                {
                    if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor(this))
                    {
                        dc->startDragging(sel.getFullPathName(), this);
                        return;
                    }
                }
            }
            juce::FileTreeComponent::mouseDrag(e);
        }
    };

    class BrowserPanel : public juce::Component,
                         public juce::FileBrowserListener,
                         public juce::Button::Listener
    {
    public:
        BrowserPanel();
        ~BrowserPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void refresh();

        // FileBrowserListener
        void selectionChanged() override;
        void fileClicked(const juce::File&, const juce::MouseEvent&) override {}
        void fileDoubleClicked(const juce::File& f) override;
        void browserRootChanged(const juce::File&) override {}

        // Button::Listener
        void buttonClicked(juce::Button* b) override;

    private:
        void navigateTo(const juce::File& dir);

        AudioDirFilter audioFilter;
        juce::TimeSliceThread dirThread { "NovaBrowserThread" };
        juce::DirectoryContentsList dirContents { &audioFilter, dirThread };
        DraggableFileTree fileTree { dirContents };

        juce::TextEditor searchBox;
        juce::TextButton homeBtn   { "Home" };
        juce::TextButton desktopBtn{ "Desktop" };
        juce::TextButton docsBtn   { "Docs" };
        juce::TextButton musicBtn  { "Music" };
        juce::TextButton recBtn    { "Rec" };

        juce::File selectedFile;
        static constexpr int kHeaderH   = 28;
        static constexpr int kSearchH   = 26;
        static constexpr int kBookmarkH = 24;
        static constexpr int kPreviewH  = 54;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserPanel)
    };

    class PianoRollPanel : public juce::Component
    {
    public:
        PianoRollPanel();
        ~PianoRollPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
    };

    class StepSequencerPanel : public juce::Component
    {
    public:
        StepSequencerPanel();
        ~StepSequencerPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
    };

    class NovaAlignPanel : public juce::Component,
                           private juce::Button::Listener,
                           private juce::Slider::Listener,
                           private juce::ListBoxModel,
                           private juce::ChangeListener
    {
    public:
        explicit NovaAlignPanel(NovaStudio::ArrangementModel& arrangementModel);
        ~NovaAlignPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        std::function<void(const juce::String&)> onStatusMessage;

    private:
        void buttonClicked(juce::Button* button) override;
        void sliderValueChanged(juce::Slider* slider) override;
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        void refreshContent();

        NovaStudio::ArrangementModel& arrangementModel;
        juce::Label titleLabel;
        juce::Label guideLabel;
        juce::ListBox targetClipList;
        juce::Label alignAmountLabel;
        juce::Slider alignAmountSlider;
        juce::Label naturalnessLabel;
        juce::Slider naturalnessSlider;
        juce::Label tightnessLabel;
        juce::Slider tightnessSlider;
        juce::Label phraseSensitivityLabel;
        juce::Slider phraseSensitivitySlider;
        juce::Label consonantPriorityLabel;
        juce::Slider consonantPrioritySlider;
        juce::ToggleButton createNewVersionToggle;
        juce::ToggleButton bypassPreviewToggle {"Bypass Preview"};
        juce::TextButton revertButton {"Revert"};
        juce::Label statusLabel;
        juce::TextButton previewButton {"Preview"};
        juce::TextButton commitButton {"Commit"};
        juce::TextButton resetButton {"Reset"};
        juce::Label shortcutLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaAlignPanel)
    };

    // Floating window containing JUCE's built-in audio device selector.
    // Owns the selector component; caller passes in the engine's AudioDeviceManager.
    class AudioSettingsWindow : public juce::DocumentWindow
    {
    public:
        AudioSettingsWindow(juce::AudioDeviceManager& dm)
            : juce::DocumentWindow("Audio Settings",
                                   juce::Colour::fromRGB(18, 20, 28),
                                   juce::DocumentWindow::closeButton)
        {
            selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
                dm,
                /*minInputChannels*/  0,
                /*maxInputChannels*/  32,
                /*minOutputChannels*/ 0,
                /*maxOutputChannels*/ 32,
                /*showMidiInputOptions*/   false,
                /*showMidiOutputSelector*/ false,
                /*showChannelsAsStereoPairs*/ true,
                /*hideAdvancedOptionsWithButton*/ false);

            selector->setSize(500, 420);
            setContentOwned(selector.release(), true);
            setUsingNativeTitleBar(true);
            setResizable(true, false);
            centreWithSize(500, 450);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override { setVisible(false); }

    private:
        std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsWindow)
    };

    class WorkspaceToolbar : public juce::Component,
                             private juce::Button::Listener
    {
    public:
        WorkspaceToolbar();
        ~WorkspaceToolbar() override;

        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;
        void resized() override;

        // Mode callbacks
        std::function<void(int)> onModeSelected; // 0=Edit,1=Mixer,2=Beat
        std::function<void(NovaStudio::ArrangementModel::EditMode)> onEditModeSelected;
        std::function<void()> onNovaAlign;
        std::function<void()> onSave;
        std::function<void()> onLoad;
        std::function<void()> onAudioSettings;

        // Transport callbacks
        std::function<void()> onPlay;
        std::function<void()> onStop;
        std::function<void()> onRecord;
        std::function<void()> onReturnToZero;
        std::function<void()> onArm;
        std::function<void()> onMonitor;
        std::function<void()> onLoop;
        std::function<void()> onPunchToggle;
        std::function<void(bool)> onTogglePreview;

        // Transport setters
        void setPlayState(bool isPlaying, bool isRecording);
        void setLoopState(bool enabled);
        void setArmState(bool armed);
        void setMonitorState(bool enabled);
        void setPunchState(bool enabled);
        void setTempo(int bpm);
        void setTimecode(const juce::String& tc);
        void setPlaybackState(bool previewEnabled, bool hasPreview);
        std::function<void(int)> onTempoChanged;

    private:
        void buttonClicked(juce::Button* b) override;

        juce::TextButton editBtn {"EDIT"};
        juce::TextButton mixBtn {"MIX"};
        juce::TextButton beatBtn {"BEAT"};

        juce::TextButton rtzBtn        {};
        juce::TextButton rewindBtn     {};
        juce::TextButton playBtn       {};
        juce::TextButton stopBtn       {};
        juce::TextButton recordBtn     {};
        juce::TextButton ffBtn         {};
        juce::TextButton loopBtn       {};
        juce::TextButton punchBtn      { "PUNCH" };

        juce::Label timecodeLabel;
        BPMLabel    tempoLabel;


        juce::TextButton novaAlignBtn {"Nova Align"};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkspaceToolbar)
    };
}
