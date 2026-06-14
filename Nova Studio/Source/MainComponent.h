#pragma once

#include <JuceHeader.h>
#include "StudioAudioEngine.h"
#include "ui/StudioComponents.h"
#include "ui/EditWindow.h"
#include "ui/MixerWindow.h"
#include "ui/BeatWindow.h"

class MainComponent : public juce::Component, public juce::KeyListener, public juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setWorkspaceMode(int mode);

private:
    void updateStatusMessage(const juce::String& message);
    void refreshTrackList();
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    NovaStudio::StudioAudioEngine engine;
    NovaStudio::TransportState& transportState;
    NovaStudio::TimelineModel timelineModel;
    NovaStudio::ArrangementModel arrangementModel;
    NovaStudioUI::WorkspaceToolbar workspaceToolbar;
    NovaStudioUI::TrackPanel trackPanel;
    NovaStudioUI::ArrangementView arrangementView;
    std::unique_ptr<NovaStudioUI::EditWindow>   editWindow;
    std::unique_ptr<NovaStudioUI::MixerWindow>  mixerWindow;
    std::unique_ptr<NovaStudioUI::BeatWindow>   beatWindow;
    NovaStudioUI::NovaAlignPanel alignPanel;
    NovaStudioUI::MixerPanel mixerPanel; // side-panel stub kept for split/rack modes
    juce::TabbedComponent bottomTabs { juce::TabbedButtonBar::TabsAtTop };
    NovaStudioUI::BrowserPanel browserPanel;
    NovaStudioUI::PianoRollPanel pianoRollPanel;
    NovaStudioUI::StepSequencerPanel stepSequencerPanel;
    juce::Label statusLabel;
    juce::String statusMessage;
    std::unique_ptr<NovaStudioUI::AudioSettingsWindow> audioSettingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
};
