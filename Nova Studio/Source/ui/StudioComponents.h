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

    class ArrangementView : public juce::Component,
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

    private:
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        void timerCallback() override;

        NovaStudio::TransportState& transportState;
        NovaStudio::TimelineModel& timelineModel;
        NovaStudio::ArrangementModel& arrangementModel;
        WaveformCache waveformCache;
        bool isDraggingClip = false;
        int dragStartX = 0;
        int64_t originalClipStartSample = 0;
        int64_t originalClipLength = 0;
        bool isDraggingTrimLeft = false;
        bool isDraggingTrimRight = false;
        int64_t currentTrimSample = 0;
        double zoomFactor = 1.0;
        bool isMarqueeSelecting = false;
        juce::Point<float> marqueeStart {0.0f, 0.0f};
        juce::Rectangle<float> marqueeRect {0.0f, 0.0f, 0.0f, 0.0f};

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

    class BrowserPanel : public juce::Component
    {
    public:
        BrowserPanel();
        ~BrowserPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
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

        std::function<void(int)> onModeSelected; // 0=Edit,1=Mixer,2=Split,3=Beat,4=Rack
        std::function<void(NovaStudio::ArrangementModel::EditMode)> onEditModeSelected;
        std::function<void()> onNovaAlign;
        std::function<void()> onSave;
        std::function<void()> onLoad;
        std::function<void()> onAudioSettings;

    private:
        void buttonClicked(juce::Button* b) override;

        juce::TextButton editBtn {"Edit"};
        juce::TextButton mixerBtn {"Mixer"};
        juce::TextButton splitBtn {"Split"};
        juce::TextButton beatBtn {"Beat"};
        juce::TextButton rackBtn {"Rack"};
        juce::TextButton slipBtn {"Slip"};
        juce::TextButton gridBtn {"Grid"};
        juce::TextButton shuffleBtn {"Shuffle"};
        juce::TextButton spotBtn {"Spot"};
        juce::TextButton novaAlignBtn {"Nova Align"};
        juce::TextButton saveBtn {"Save"};
        juce::TextButton loadBtn {"Load"};
        juce::TextButton audioBtn {"Audio"};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkspaceToolbar)
    };
}
