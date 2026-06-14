#pragma once

#include <JuceHeader.h>
#include "StudioAudioEngine.h"
#include "ui/StudioComponents.h"
#include "ui/EditWindow.h"
#include "ui/MixerWindow.h"
#include "ui/BeatWindow.h"

// ── FloatingPanelWindow ──────────────────────────────────────────────────────
// A DocumentWindow that wraps any Component without owning it. On close,
// it calls the provided re-dock callback so the component returns to its
// original parent.
class FloatingPanelWindow : public juce::DocumentWindow
{
public:
    FloatingPanelWindow(const juce::String& title,
                        juce::Component* content,
                        std::function<void(FloatingPanelWindow*)> onClosed)
        : juce::DocumentWindow(title,
                               juce::Colour::fromRGB(20, 22, 32),
                               juce::DocumentWindow::allButtons),
          onClosed(std::move(onClosed))
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setContentNonOwned(content, false);
        setSize(juce::jmax(400, content->getWidth()),
                juce::jmax(300, content->getHeight()));
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        // Remove content before closing so it isn't deleted
        setContentNonOwned(nullptr, false);
        if (onClosed) onClosed(this);
    }

private:
    std::function<void(FloatingPanelWindow*)> onClosed;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatingPanelWindow)
};

// ── MainComponent ────────────────────────────────────────────────────────────

class MainComponent : public juce::Component,
                      public juce::KeyListener,
                      public juce::ChangeListener,
                      public juce::MenuBarModel,
                      public juce::DragAndDropContainer
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

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // Panel pop-out helpers
    void popOutMixer();
    void popOutBeat();
    void popOutBrowser();
    void redockMixer();
    void redockBeat();
    void redockBrowser();

    juce::MenuBarComponent menuBar { this };
    static constexpr int kMenuH    = 24;
    static constexpr int kToggleW  = 10;  // thin sidebar collapse bar width

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
    NovaStudioUI::MixerPanel mixerPanel;
    NovaStudioUI::BottomDockPanel bottomDock;
    NovaStudioUI::BrowserPanel browserPanel;
    NovaStudioUI::PianoRollPanel pianoRollPanel;
    NovaStudioUI::StepSequencerPanel stepSequencerPanel;
    juce::Label statusLabel;
    juce::String statusMessage;
    std::unique_ptr<NovaStudioUI::AudioSettingsWindow> audioSettingsWindow;

    // Browser collapse state
    bool browserCollapsed = false;

    // Thin toggle bar for browser sidebar
    struct SidebarToggleBar : public juce::Component
    {
        bool collapsed = false;
        bool isHorizontal = false;  // true = bottom dock (up/down arrows), false = sidebar (left/right)
        std::function<void()> onClick;

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(18, 20, 30));
            g.setColour(juce::Colour::fromRGB(50, 55, 75));
            g.fillRect(getLocalBounds().reduced(1));

            g.setColour(juce::Colours::white.withAlpha(0.6f));
            const float cx = getWidth() * 0.5f;
            const float cy = getHeight() * 0.5f;
            const float hs = 5.0f;

            juce::Path arrow;
            if (isHorizontal)
            {
                // Bottom dock: collapsed = point up (show panel), expanded = point down (hide panel)
                if (collapsed)
                    arrow.addTriangle(cx - hs, cy + hs * 0.5f, cx, cy - hs * 0.5f, cx + hs, cy + hs * 0.5f);
                else
                    arrow.addTriangle(cx - hs, cy - hs * 0.5f, cx, cy + hs * 0.5f, cx + hs, cy - hs * 0.5f);
            }
            else
            {
                // Sidebar: collapsed = point right (open), expanded = point left (close)
                if (collapsed)
                    arrow.addTriangle(cx - hs * 0.5f, cy - hs, cx + hs * 0.5f, cy, cx - hs * 0.5f, cy + hs);
                else
                    arrow.addTriangle(cx + hs * 0.5f, cy - hs, cx - hs * 0.5f, cy, cx + hs * 0.5f, cy + hs);
            }
            g.fillPath(arrow);
        }

        void mouseUp(const juce::MouseEvent&) override { if (onClick) onClick(); }
        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { repaint(); }
    };

    SidebarToggleBar browserToggleBar;

    // Bottom dock collapse
    bool bottomDockCollapsed = false;
    SidebarToggleBar bottomDockToggleBar;

    // Floating windows (non-owning: content is still owned by unique_ptrs above)
    std::unique_ptr<FloatingPanelWindow> floatingMixer;
    std::unique_ptr<FloatingPanelWindow> floatingBeat;
    std::unique_ptr<FloatingPanelWindow> floatingBrowser;

    // Pop-out toolbar buttons
    juce::TextButton popMixerBtn  { "^" };
    juce::TextButton popBeatBtn   { "^" };
    juce::TextButton popBrowserBtn{ "^" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
};
