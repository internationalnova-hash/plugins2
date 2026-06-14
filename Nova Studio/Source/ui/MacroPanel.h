#pragma once

#include <JuceHeader.h>
#include "../StudioAudioEngine.h"
#include "Theme.h"

namespace NovaStudioUI
{
    // ─────────────────────────────────────────────────────────────────────────
    // MacroPanel — FL Studio-style macro controls: a small bank of knobs, each
    // of which can fan out to one or more mixer/plugin parameters across the
    // session (reusing the "volume"/"pan"/"sendN"/"plugin:<slot>:<paramIndex>"
    // addressing scheme already used by automation lanes).
    // ─────────────────────────────────────────────────────────────────────────
    // Rotary knob that also reports right-clicks (used for the per-macro context menu)
    class MacroKnob : public juce::Slider
    {
    public:
        std::function<void()> onRightClick;
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onRightClick) onRightClick();
                return;
            }
            juce::Slider::mouseDown(e);
        }
    };

    class MacroPanel : public juce::Component,
                       private juce::Slider::Listener,
                       private juce::Button::Listener
    {
    public:
        explicit MacroPanel(NovaStudio::StudioAudioEngine& engineRef);
        ~MacroPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void refresh();

    private:
        void sliderValueChanged(juce::Slider* slider) override;
        void buttonClicked(juce::Button* button) override;

        void showMacroContextMenu(int macroIndex);
        void renameMacro(int macroIndex);
        void addMappingTarget(int macroIndex, int trackIndex, const juce::String& parameterId);

        static constexpr int kMaxMacros = 8;

        NovaStudio::StudioAudioEngine& engine;

        juce::TextButton addBtn { "+ Macro" };

        struct MacroSlot
        {
            std::unique_ptr<MacroKnob> knob;
            juce::Label label;
        };
        juce::OwnedArray<MacroSlot> slots;

        void rebuildSlots();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroPanel)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // MacroPanelWindow — floating DocumentWindow wrapper
    // ─────────────────────────────────────────────────────────────────────────
    class MacroPanelWindow : public juce::DocumentWindow
    {
    public:
        explicit MacroPanelWindow(NovaStudio::StudioAudioEngine& engine)
            : juce::DocumentWindow("Macros",
                                   juce::Colour::fromRGB(18, 20, 28),
                                   DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            panel = std::make_unique<MacroPanel>(engine);
            setUsingNativeTitleBar(false);
            setContentNonOwned(panel.get(), false);
            setSize(420, 260);
            centreWithSize(420, 260);
            setResizable(true, false);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override { setVisible(false); }
        MacroPanel* getPanel() { return panel.get(); }

    private:
        std::unique_ptr<MacroPanel> panel;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroPanelWindow)
    };
}
