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
        static constexpr int kWidth       = 96;
        static constexpr int kMasterWidth = 112;
        static constexpr int kHeight      = 580;
        static constexpr int kNumSends    = 4;

        ChannelStrip();
        ~ChannelStrip() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setTrackIndex(int index)    { trackIndex = index; }
        int  getTrackIndex() const       { return trackIndex; }
        void setTrackNumber(int n)       { trackNumber = n; repaint(); }
        void setMaster(bool master)      { isMaster = master; repaint(); }
        void setAux(bool aux)            { isAux = aux; repaint(); }

        void updateFromTrack(const NovaStudio::Track& track);
        void updateAsMaster();
        void updateAsAux(const juce::String& label);

        void setMeterLevel(float left, float right);
        float getMeterLeft()  const noexcept { return meterLeft; }
        float getMeterRight() const noexcept { return meterRight; }

        void setInsertSlotName(int slot, const juce::String& name)
        {
            if (isPositiveAndBelow(slot, 9)) { insertSlotNames[slot] = name; repaint(); }
        }
        void setSendName(int send, const juce::String& name)
        {
            if (isPositiveAndBelow(send, kNumSends)) { sendNames[send] = name; repaint(); }
        }
        void setSendLevel(int send, float db)
        {
            if (isPositiveAndBelow(send, kNumSends))
                sendKnobs[send].setValue(db, juce::dontSendNotification);
        }

        juce::String insertSlotNames[9];
        bool insertsExpanded = false;

        // Callbacks
        std::function<void(float dB)>         onVolumeChanged;
        std::function<void(float pan)>        onPanChanged;
        std::function<void(bool)>             onMuteToggled;
        std::function<void(bool)>             onSoloToggled;
        std::function<void(bool)>             onArmToggled;
        std::function<void(int slot)>         onInsertClicked;
        std::function<void(int slot)>         onInsertChangePlugin;
        std::function<void(int slot)>         onInsertRemovePlugin;
        std::function<void(int send, float)>  onSendChanged;  // send index, dB value

        void mouseDown(const juce::MouseEvent& e) override;

    private:
        void sliderValueChanged(juce::Slider* s) override;
        void buttonClicked(juce::Button* b) override;
        void drawInsertSlots(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawMeter(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawDbScale(juce::Graphics& g, juce::Rectangle<int> faderArea) const;

        int   trackIndex  = -1;
        int   trackNumber = 0;
        bool  isMaster    = false;
        bool  isAux       = false;
        float meterLeft   = 0.0f;
        float meterRight  = 0.0f;

        juce::Label      nameLabel;
        juce::TextButton muteBtn  { "M" };
        juce::TextButton soloBtn  { "S" };
        juce::TextButton armBtn   { "R" };
        juce::TextButton preBtn   { "PRE" };
        juce::TextButton readBtn  { "Read" };
        juce::Slider     fader;
        juce::Slider     panSlider;
        juce::Label      faderDbLabel;
        juce::Label      inputLabel;
        juce::Label      outputLabel;

        // Send controls — one knob + label per send (A B C D)
        juce::Slider     sendKnobs[kNumSends];
        juce::Label      sendDbLabels[kNumSends];
        juce::TextButton sendEnableBtns[kNumSends];
        juce::String     sendNames[kNumSends] { "A", "B", "C", "D" };

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
