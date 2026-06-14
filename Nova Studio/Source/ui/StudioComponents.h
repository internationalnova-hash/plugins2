#pragma once

#include <JuceHeader.h>
#include "../Session.h"
#include "../TransportState.h"
#include "../TimelineModel.h"
#include "../ArrangementModel.h"
#include "WaveformCache.h"
#include "AnimationUtils.h"
#include "Theme.h"

namespace NovaStudioUI
{
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
        std::function<void()> onArm;
        std::function<void()> onMonitor;
        std::function<void()> onLoop;

        void setTempo(int bpm);
        void setTimecode(const juce::String& timecode);
        void setPlayState(bool isPlaying, bool isRecording);
        void setLoopState(bool enabled);
        void setArmState(bool armed);
        void setMonitorState(bool enabled);
        void setPlaybackState(bool previewEnabled, bool hasPreview);
        std::function<void(bool)> onTogglePreview; // called when user toggles preview on/off

    private:
        void buttonClicked(juce::Button* button) override;
        void timerCallback() override;

        juce::TextButton rtzButton {"|<<"};
        juce::TextButton playButton {"Play"};
        juce::TextButton stopButton {"Stop"};
        juce::TextButton recordButton {"Rec"};
        juce::TextButton loopButton {"Loop"};
        juce::TextButton armButton {"Arm"};
        juce::TextButton monitorButton {"Monitor"};

        juce::Label tempoLabel;
        juce::Label timeLabel;
        juce::Label playbackLabel;
        juce::TextButton playbackToggleButton { "Toggle" };

        NovaStudioUI::AnimatedValue hoverAlpha;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
    };

    class TrackPanel : public juce::Component
    {
    public:
        static constexpr int kTrackHeight = 64;

        explicit TrackPanel(NovaStudio::Session& session);
        ~TrackPanel() override;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void refresh() { repaint(); }

        std::function<void(int, bool)> onTrackArm;
        std::function<void(int, bool)> onTrackMute;
        std::function<void(int, bool)> onTrackSolo;

    private:
        enum class HitButton { None, Arm, Mute, Solo };
        HitButton hitTest(int trackIndex, juce::Point<int> pos) const;

        NovaStudio::Session& session;

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

    // Toolbar that sits directly above the arrangement view — edit modes, grid, zoom
    class EditModeToolbar : public juce::Component,
                            private juce::Button::Listener,
                            private juce::ComboBox::Listener
    {
    public:
        enum class EditMode { Slip, Grid, Spot, Shuffle };

        EditModeToolbar();
        ~EditModeToolbar() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // Callbacks → ArrangementView
        std::function<void(EditMode)> onEditModeChanged;
        std::function<void(double)>   onSnapResolutionChanged; // beats per snap unit
        std::function<void(double)>   onNudgeResolutionChanged;
        std::function<void(bool)>     onSnapEnabled;
        std::function<void(int)>      onHZoomChanged;  // +1 in / -1 out
        std::function<void(int)>      onVZoomChanged;

        void setEditMode(EditMode m);
        EditMode getEditMode() const  { return currentMode; }
        double getSnapBeats() const   { return snapBeats; }
        double getNudgeBeats() const  { return nudgeBeats; }
        bool isSnapOn() const         { return snapEnabled; }

    private:
        void buttonClicked(juce::Button* b) override;
        void comboBoxChanged(juce::ComboBox* c) override;
        static double beatsForResolutionId(int id);

        juce::TextButton slipBtn    {"SLIP"};
        juce::TextButton gridBtn    {"GRID"};
        juce::TextButton spotBtn    {"SPOT"};
        juce::TextButton shuffleBtn {"SHUFFLE"};

        juce::TextButton snapBtn    {"SNAP"};
        juce::ComboBox   gridBox;   // snap/grid resolution
        juce::ComboBox   nudgeBox;  // nudge resolution

        juce::TextButton hZoomInBtn  {"H+"};
        juce::TextButton hZoomOutBtn {"H-"};
        juce::TextButton vZoomInBtn  {"V+"};
        juce::TextButton vZoomOutBtn {"V-"};

        juce::Label gridLabel, nudgeLabel, zoomLabel;

        EditMode currentMode = EditMode::Slip;
        double   snapBeats   = 4.0;  // 1 bar (4 beats)
        double   nudgeBeats  = 0.25; // 1/16 note
        bool     snapEnabled = true;

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
        void setEditMode(EditModeToolbar::EditMode m) { editMode = m; }
        void setSnapResolution(double beats)          { snapBeats = beats; }
        void setNudgeResolution(double beats)         { nudgeBeats = beats; }
        void setSnapEnabled(bool on)                  { snapEnabled = on; }
        void adjustHZoom(int direction);
        void adjustVZoom(int direction);
        int  getTrackHeight() const { return trackHeightPx; }

        // Nudge selected clips by nudgeBeats (called from keyboard or toolbar)
        void nudgeSelected(int direction); // +1 right, -1 left

    private:
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        void timerCallback() override;
        int64_t snapToGrid(int64_t sample) const;
        void showSpotDialog(int trackIndex, int clipIndex);

        NovaStudio::TransportState& transportState;
        NovaStudio::TimelineModel& timelineModel;
        NovaStudio::ArrangementModel& arrangementModel;
        WaveformCache waveformCache;

        bool isDraggingClip      = false;
        int  dragStartX          = 0;
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

        // Loop brace dragging
        enum class LoopDragHandle { None, Start, End, Body };
        LoopDragHandle loopDragHandle = LoopDragHandle::None;
        int64_t loopDragStartSample = 0; // position at drag start
        int64_t loopOrigStart = 0;
        int64_t loopOrigEnd   = 0;

        EditModeToolbar::EditMode editMode = EditModeToolbar::EditMode::Slip;
        double  snapBeats   = 4.0;   // 1 bar
        double  nudgeBeats  = 0.25;  // 1/16
        bool    snapEnabled = true;

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
                            private juce::Button::Listener
    {
    public:
        BottomDockPanel();
        ~BottomDockPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

    private:
        void buttonClicked(juce::Button*) override {}
        void paintMixerStrips(juce::Graphics& g, juce::Rectangle<int> area);
        void paintPianoRoll(juce::Graphics& g, juce::Rectangle<int> area);
        void paintStepSequencer(juce::Graphics& g, juce::Rectangle<int> area);

        // Returns strip index and fader hit (-1 if none)
        int stripIndexAt(int x) const;
        bool faderHitTest(int stripIndex, juce::Point<int> pos, juce::Rectangle<int> mixerArea) const;

        juce::TextButton mixerTab{"MIXER"}, channelsTab{"CHANNELS"},
                         effectsTab{"EFFECTS"},  metersTab{"METERS"};

        static constexpr int kStripW = 68;
        static constexpr int kNumStrips = 13;

        // Interactive fader state — stored per strip (0.0 = top, 1.0 = bottom)
        float faderPositions[kNumStrips] = {
            0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f,
            0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.30f
        };
        int   activeFaderStrip  = -1;
        int   faderDragStartY   = 0;
        float faderDragStartPos = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomDockPanel)
    };

    class BrowserPanel : public juce::Component
    {
    public:
        BrowserPanel();
        ~BrowserPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

        void refresh();

    private:
        int fileIndexAt(int y) const;

        juce::Array<juce::File> files;
        int selectedIndex = -1;
        static constexpr int kHeaderH  = 32;
        static constexpr int kSearchH  = 28;
        static constexpr int kTabsH    = 24;
        static constexpr int kItemH    = 22;
        static constexpr int kPreviewH = 80;

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
        void resized() override;

        // Mode callbacks
        std::function<void(int)> onModeSelected; // 0=Edit,1=Mixer,2=Browse
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
        std::function<void(bool)> onTogglePreview;

        // Transport setters
        void setPlayState(bool isPlaying, bool isRecording);
        void setLoopState(bool enabled);
        void setArmState(bool armed);
        void setMonitorState(bool enabled);
        void setTempo(int bpm);
        void setTimecode(const juce::String& tc);
        void setPlaybackState(bool previewEnabled, bool hasPreview);

    private:
        void buttonClicked(juce::Button* b) override;

        // Mode buttons
        juce::TextButton editBtn {"EDIT"};
        juce::TextButton mixBtn {"MIX"};
        juce::TextButton browseBtn {"BROWSE"};

        // Transport buttons
        juce::TextButton rtzBtn {"|<<"};
        juce::TextButton playBtn {"Play"};
        juce::TextButton stopBtn {"Stop"};
        juce::TextButton recordBtn {"Rec"};
        juce::TextButton armBtn {"ARM"};
        juce::TextButton monitorBtn {"MON"};
        juce::TextButton loopBtn {"LOOP"};

        juce::Label timecodeLabel;
        juce::Label tempoLabel;

        juce::TextButton saveBtn {"Save"};
        juce::TextButton loadBtn {"Load"};
        juce::TextButton audioBtn {"Audio"};
        juce::TextButton novaAlignBtn {"Nova Align"};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkspaceToolbar)
    };
}
