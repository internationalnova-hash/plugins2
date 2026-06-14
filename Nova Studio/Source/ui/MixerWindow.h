#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../Session.h"
#include "../StudioAudioEngine.h"
#include "PluginBrowserPanel.h"

namespace NovaStudioUI
{
    // ── ChannelStrip ──────────────────────────────────────────────────────────
    // One strip per track plus master. Owns all controls, fires callbacks.

    class ChannelStrip : public juce::Component,
                         private juce::Slider::Listener,
                         private juce::Button::Listener
    {
    public:
        static constexpr int kWidth       = 90;
        static constexpr int kMasterWidth = 108;
        static constexpr int kHeight      = 480;

        ChannelStrip();
        ~ChannelStrip() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setTrackIndex(int index)    { trackIndex = index; }
        void setMaster(bool master)      { isMaster = master; repaint(); }
        void setAux(bool aux)            { isAux = aux; repaint(); }

        void updateFromTrack(const NovaStudio::Track& track);
        void updateAsMaster();
        void updateAsAux(const juce::String& label);

        // Meter level: 0.0–1.0, called by timer
        void setMeterLevel(float left, float right);
        float getMeterLeft()  const noexcept { return meterLeft; }
        float getMeterRight() const noexcept { return meterRight; }

        // Update displayed insert slot names (called by MixerWindow when plugins change)
        void setInsertSlotName(int slot, const juce::String& name)
        {
            if (isPositiveAndBelow(slot, 9))
            {
                insertSlotNames[slot] = name;
                repaint();
            }
        }
        juce::String insertSlotNames[9];
        bool insertsExpanded = false;

        // Callbacks wired by MixerWindow after construction
        std::function<void(float dB)>  onVolumeChanged;
        std::function<void(float pan)> onPanChanged;
        std::function<void(bool)>      onMuteToggled;
        std::function<void(bool)>      onSoloToggled;
        std::function<void(bool)>      onArmToggled;
        std::function<void(int slot)>  onInsertClicked;       // open editor (slot has plugin) or browser (empty)
        std::function<void(int slot)>  onInsertChangePlugin;  // replace plugin in slot
        std::function<void(int slot)>  onInsertRemovePlugin;  // remove plugin from slot

        void mouseDown(const juce::MouseEvent& e) override;

    private:
        void sliderValueChanged(juce::Slider* s) override;
        void buttonClicked(juce::Button* b) override;
        void drawInsertSlots(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawSendSlots(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawMeter(juce::Graphics& g, juce::Rectangle<int> area) const;

        int   trackIndex = -1;
        bool  isMaster   = false;
        bool  isAux      = false;
        float meterLeft  = 0.0f;
        float meterRight = 0.0f;

        juce::Label      nameLabel;
        juce::TextButton muteBtn  { "M" };
        juce::TextButton soloBtn  { "S" };
        juce::TextButton armBtn   { "A" };
        juce::Slider     fader;
        juce::Slider     panSlider;
        juce::Label      faderDbLabel;
        juce::Label      routingLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStrip)
    };

    // ── PluginEditorWindow ────────────────────────────────────────────────────
    // Floating window that hosts a plugin's GUI.

    class PluginEditorWindow : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow(juce::AudioPluginInstance& instance, int trackIndex, int pluginSlot)
            : juce::DocumentWindow(instance.getName(),
                                   juce::Colour::fromRGB(24, 26, 36),
                                   DocumentWindow::closeButton | DocumentWindow::minimiseButton),
              trackIdx(trackIndex), pluginSlotIdx(pluginSlot)
        {
            setUsingNativeTitleBar(false);
            auto* editor = instance.createEditorIfNeeded();
            if (editor == nullptr)
                editor = new juce::GenericAudioProcessorEditor(instance);
            setContentOwned(editor, true);
            setResizable(true, false);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override { setVisible(false); }

        int getTrackIndex() const { return trackIdx; }
        int getPluginSlot() const { return pluginSlotIdx; }

    private:
        int trackIdx;
        int pluginSlotIdx;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
    };

    // ── MixerWindow ───────────────────────────────────────────────────────────

    class MixerWindow : public juce::Component,
                        private juce::Timer,
                        private juce::ChangeListener
    {
    public:
        explicit MixerWindow(NovaStudio::StudioAudioEngine& engineRef);
        ~MixerWindow() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // Call when the track list changes (tracks added/removed)
        void refresh();

        // Open the plugin editor for a given track/slot (creates floating window)
        void openPluginEditor(int trackIndex, int pluginSlot);

        // Open the plugin browser to load a plugin onto a track/slot
        void openPluginBrowser(int trackIndex, int slotIndex);

    private:
        void buildStrips();
        void timerCallback() override;
        void changeListenerCallback(juce::ChangeBroadcaster*) override;

        NovaStudio::StudioAudioEngine& engine;

        juce::Viewport  viewport;
        juce::Component stripsContainer;

        juce::OwnedArray<ChannelStrip> trackStrips;
        juce::OwnedArray<ChannelStrip> auxStrips;
        std::unique_ptr<ChannelStrip>  masterStrip;
        juce::OwnedArray<PluginEditorWindow>   editorWindows;
        std::unique_ptr<PluginBrowserWindow>   pluginBrowserWindow;

        void refreshInsertSlotNames();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerWindow)
    };
}
