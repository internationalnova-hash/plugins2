#pragma once

#include <JuceHeader.h>
#include "../TransportState.h"
#include "../TimelineModel.h"
#include "../ArrangementModel.h"
#include "Theme.h"
#include "StudioComponents.h"

namespace NovaStudioUI
{
    class EditWindow : public juce::Component,
                       private juce::ChangeListener
    {
    public:
        EditWindow(NovaStudio::TransportState& transport,
                   NovaStudio::TimelineModel& timeline,
                   NovaStudio::ArrangementModel& arrangement);
        ~EditWindow() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setTrackCallbacks(std::function<void(int,bool)> onArm,
                               std::function<void(int,bool)> onMute,
                               std::function<void(int,bool)> onSolo,
                               std::function<void(int, const juce::String&)> onRenamed = nullptr);

        void zoomHorizontal(int direction);   // +1 = zoom in, -1 = zoom out

        // Set by MainComponent — called when a file drop needs a new track
        std::function<int(const juce::String&, bool)> onCreateAudioTrack;

        // Wire engine peak level reader to track header meters
        void setLevelCallback(std::function<float(int,int)> fn);


    private:
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;

        NovaStudio::TransportState& transportState;
        NovaStudio::TimelineModel& timelineModel;
        NovaStudio::ArrangementModel& arrangementModel;

        juce::Component leftPanel, centerPanel, bottomPanel, rightPanel, topPanel;
        std::unique_ptr<TransportBar> transportBar;
        std::unique_ptr<EditModeToolbar> editModeToolbar;
        std::unique_ptr<ArrangementView> arrangementView;
        std::unique_ptr<TrackPanel> trackPanel;
        std::unique_ptr<ProductionPanel> productionPanel;
    };
}
