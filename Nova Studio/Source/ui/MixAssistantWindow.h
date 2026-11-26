#pragma once

// Mix Assistant window (AI > Mix Assistant): listens to every track, shows what it
// heard (each role can be changed), the buses / Nova chains / sends it'll build and
// why, then applies it in one go. Apply is one undo step, and Before / After swaps
// between the two versions of the session.

#include <JuceHeader.h>
#include "../StudioAudioEngine.h"
#include "../mix/MixAnalysis.h"
#include "../mix/MixPlanner.h"
#include "../mix/MixApplier.h"

namespace NovaStudioUI
{
    class MixAssistantPanel : public juce::Component,
                              public juce::FileDragAndDropTarget,
                              private juce::Timer
    {
    public:
        enum class State { Listening, Ready, Applying, Applied, Failed };

        struct Callbacks
        {
            std::function<void()> sessionChanged;                                       // refresh tracks, mixer, etc.
            std::function<void (const juce::File& before, const juce::File& after)> applied;   // register the undo step
            std::function<void (const juce::String&)> status;                           // main window status line
            std::function<void (bool)> setBusy;                                         // block the rest of the app while rendering
        };

        MixAssistantPanel (NovaStudio::StudioAudioEngine& engine, Callbacks callbacks);
        ~MixAssistantPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int, int) override;

        /// Before / After swap ended up outside the panel (e.g. ⌘Z): forget the A/B state
        void sessionSwappedElsewhere();
        bool isApplying() const noexcept { return state == State::Applying; }
        bool isReady() const noexcept { return state == State::Ready || state == State::Applied; }
        void select (int bus) { selected = bus; everSelected = true; repaint(); }
        void pressApply() { apply(); }                       // same as clicking Apply (tests)
        void pressBeforeAfter (bool after) { showVersion (after); }

        // pieces (public so the .cpp's helper components can reach them)
        struct RoleList;
        struct PlanView;
        struct DetailView;

    private:

        void startListening();
        void rebuildPlan();
        void apply();
        void finishApply (const juce::String& masterPlanXml);
        void showVersion (bool after);
        void setState (State s);
        void timerCallback() override;
        juce::String headerStatus() const;

        NovaStudio::StudioAudioEngine& engine;
        Callbacks cb;
        std::unique_ptr<juce::LookAndFeel_V4> lnf;

        State state = State::Listening;
        std::shared_ptr<std::atomic<float>> progress = std::make_shared<std::atomic<float>> (0.f);
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

        NovaStudio::MixAI::SessionAnalysis analysis;
        std::vector<NovaStudio::MixAI::Role> roles;
        NovaStudio::MixAI::Options options;
        NovaStudio::MixAI::Plan plan;
        int selected = -2;                         // bus index, -2 = mix bus, -3 = straight to Main Out
        bool everSelected = false;                 // the user clicked a card (keep their choice across re-plans)
        std::vector<const NovaStudio::MixAI::TrackMove*> directTracks() const;
        juce::File beforeSnap, afterSnap;
        bool showingAfter = true;
        NovaStudio::MixAI::ApplyResult applyResult;
        juce::String applyNote;

        // header
        juce::ComboBox styleBox;
        juce::TextButton refButton { "Drop a song..." };
        juce::TextButton howFar[3] { juce::TextButton ("Light"), juce::TextButton ("Balanced"), juce::TextButton ("Full mix") };
        // columns
        std::unique_ptr<RoleList> roleList;
        juce::Viewport roleViewport;
        std::unique_ptr<PlanView> planView;
        juce::Viewport planViewport;
        std::unique_ptr<DetailView> detailView;
        // footer
        juce::ToggleButton optRoute { "Group & route" }, optLevels { "Levels & pan" }, optChains { "Nova chains" },
                           optRoom { "Make room (EQ)" }, optTune { "Tune vocals" }, optPolish { "Mix bus polish" };
        juce::TextButton beforeBtn { "Before" }, afterBtn { "After" }, applyBtn { "Apply mix" };
        std::unique_ptr<juce::FileChooser> chooser;
        juce::Rectangle<int> leftCol, midCol, rightCol, footer;

        friend struct RoleList;
        friend struct PlanView;
        friend struct DetailView;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAssistantPanel)
    };

    class MixAssistantWindow : public juce::DocumentWindow
    {
    public:
        MixAssistantWindow (NovaStudio::StudioAudioEngine& engine, MixAssistantPanel::Callbacks callbacks, std::function<void()> onClose);
        void closeButtonPressed() override;
        bool isBusy() const;
        MixAssistantPanel& getPanel() { return *panel; }

    private:
        MixAssistantPanel* panel = nullptr;
        std::function<void()> onClose;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAssistantWindow)
    };
}
