#pragma once

#include <JuceHeader.h>
#include "../TransportState.h"
#include "../TimelineModel.h"
#include "../ArrangementModel.h"
#include "Theme.h"
#include "StudioComponents.h"
#include "../StudioAudioEngine.h"

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

        std::function<int(const juce::String&, bool)> onCreateAudioTrack;
        std::function<void()> onAddTrackClicked;

        // Link engine so insert chain mutations are routed through a single path
        void setEngine(NovaStudio::StudioAudioEngine& e);

        // Wire engine peak level reader to track header meters
        void setLevelCallback(std::function<float(int,int)> fn);

        // Panel collapse state
        void setLeftPanelCollapsed(bool collapsed);
        void setRightPanelCollapsed(bool collapsed);
        bool isLeftPanelCollapsed()  const { return leftCollapsed; }
        bool isRightPanelCollapsed() const { return rightCollapsed; }

    private:
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;

        NovaStudio::TransportState& transportState;
        NovaStudio::TimelineModel& timelineModel;
        NovaStudio::ArrangementModel& arrangementModel;
        NovaStudio::StudioAudioEngine* enginePtr = nullptr;

        juce::Component leftPanel, centerPanel, bottomPanel, rightPanel, topPanel;
        std::unique_ptr<TransportBar> transportBar;
        std::unique_ptr<EditModeToolbar> editModeToolbar;
        std::unique_ptr<ArrangementView> arrangementView;
        std::unique_ptr<TrackPanel> trackPanel;
        std::unique_ptr<ProductionPanel> productionPanel;

        // Collapse state
        bool leftCollapsed  = false;
        bool rightCollapsed = false;

        // Toggle bars (thin clickable strips)
        static constexpr int kToggleBarW = 10;

        // Helper: a thin toggle button drawn inline in paint/resized
        struct CollapseBar : public juce::Component
        {
            bool isLeft = true;
            bool collapsed = false;
            std::function<void()> onClick;

            void paint(juce::Graphics& g) override
            {
                g.fillAll(juce::Colour::fromRGB(25, 28, 38));
                g.setColour(juce::Colour::fromRGB(60, 65, 90));
                g.fillRect(getLocalBounds().reduced(1));

                // Draw arrow indicator
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                const float cx = getWidth() * 0.5f;
                const float cy = getHeight() * 0.5f;
                const float hs = 5.0f;

                // Arrow points inward (toward center) when expanded, outward when collapsed
                bool pointRight = (isLeft && collapsed) || (!isLeft && !collapsed);

                juce::Path arrow;
                if (pointRight)
                {
                    arrow.addTriangle(cx - hs * 0.5f, cy - hs,
                                      cx + hs * 0.5f, cy,
                                      cx - hs * 0.5f, cy + hs);
                }
                else
                {
                    arrow.addTriangle(cx + hs * 0.5f, cy - hs,
                                      cx - hs * 0.5f, cy,
                                      cx + hs * 0.5f, cy + hs);
                }
                g.fillPath(arrow);
            }

            void mouseUp(const juce::MouseEvent&) override
            {
                if (onClick) onClick();
            }

            void mouseEnter(const juce::MouseEvent&) override { repaint(); }
            void mouseExit(const juce::MouseEvent&) override  { repaint(); }
        };

        CollapseBar leftToggleBar, rightToggleBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditWindow)
    };
}
