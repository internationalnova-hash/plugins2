#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../Session.h"
#include "../StudioAudioEngine.h"
#include "../ArrangementModel.h"
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
        static constexpr int kWidth       = 88;
        static constexpr int kMasterWidth = 100;
        static constexpr int kHeight      = 820;  // taller to fit 10 inserts + 6 sends
        static constexpr int kNumSends    = 6;

        ChannelStrip();
        ~ChannelStrip() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setTrackIndex(int index)    { trackIndex = index; }
        int  getTrackIndex() const       { return trackIndex; }
        void setTrackNumber(int n)       { trackNumber = n; repaint(); }
        void setMaster(bool master)      { isMaster = master; repaint(); }
        void setAux(bool aux)            { isAux = aux; repaint(); }
        void setTrackColour(juce::Colour c) { trackColour = c; repaint(); }

        void updateFromTrack(const NovaStudio::Track& track);
        void updateAsMaster();
        void updateAsAux(const juce::String& label);

        void setMeterLevel(float left, float right);
        float getMeterLeft()  const noexcept { return meterLeft; }
        float getMeterRight() const noexcept { return meterRight; }

        void setInsertSlotName(int slot, const juce::String& name)
        {
            if (isPositiveAndBelow(slot, 10)) { insertSlotNames[slot] = name; repaint(); }
        }
        // sendBusName: destination bus (e.g. "Verb", "Delay")
        void setSendBusName(int send, const juce::String& busName)
        {
            if (isPositiveAndBelow(send, kNumSends)) { sendBusNames[send] = busName; repaint(); }
        }
        void setSendLevel(int send, float db)
        {
            if (isPositiveAndBelow(send, kNumSends))
                sendKnobs[send].setValue(db, juce::dontSendNotification);
        }
        void setInputName (const juce::String& s) { inputLabel .setText(s, juce::dontSendNotification); }
        void setOutputName(const juce::String& s) { outputLabel.setText(s, juce::dontSendNotification); }

        juce::String insertSlotNames[10];
        bool insertsExpanded = false;

        // Callbacks
        std::function<void(float dB)>          onVolumeChanged;
        std::function<void(float pan)>         onPanChanged;
        std::function<void(bool)>              onMuteToggled;
        std::function<void(bool)>              onSoloToggled;
        std::function<void(bool)>              onArmToggled;
        std::function<void(int slot)>          onInsertClicked;
        std::function<void(int slot)>          onInsertChangePlugin;
        std::function<void(int slot)>          onInsertRemovePlugin;
        std::function<void(int send, float)>   onSendChanged;
        std::function<void(const juce::String&)> onInputChanged;
        std::function<void(const juce::String&)> onOutputChanged;
        // Supply available routing names for the I/O popup menus
        std::function<juce::StringArray()>     getAvailableInputs;
        std::function<juce::StringArray()>     getAvailableOutputs;

        void mouseDown(const juce::MouseEvent& e) override;

    private:
        void sliderValueChanged(juce::Slider* s) override;
        void buttonClicked(juce::Button* b) override;
        void drawInsertSlots(juce::Graphics& g, juce::Rectangle<int> area) const;
        void drawMeter(juce::Graphics& g, juce::Rectangle<int> area) const;
        void showIOPopup(bool isInput);

        // Layout helpers — returns rects matching resized() allocation
        juce::Rectangle<int> getFaderRect()  const;
        juce::Rectangle<int> getMeterRect()  const;
        juce::Rectangle<int> getInsertRect() const;
        juce::Rectangle<int> getSendsRect()  const;

        int          trackIndex  = -1;
        int          trackNumber = 0;
        bool         isMaster    = false;
        bool         isAux       = false;
        float        meterLeft   = 0.0f;
        float        meterRight  = 0.0f;
        juce::Colour trackColour { 100, 80, 200 };

        juce::Label      nameLabel;
        juce::TextButton muteBtn  { "M" };
        juce::TextButton soloBtn  { "S" };
        juce::TextButton armBtn   { "R" };
        juce::TextButton preBtn   { "PRE" };
        juce::TextButton readBtn  { "Read" };
        juce::Slider     fader;
        juce::Slider     panKnob;     // rotary
        juce::Label      faderDbLabel;
        juce::Label      inputLabel;   // text storage only — not added as child
        juce::Label      outputLabel;  // text storage only — not added as child
        juce::Rectangle<int> inputLabelBounds;
        juce::Rectangle<int> outputLabelBounds;

        // Sends — one rotary knob + dB label per send; bus name empty = unassigned
        juce::Slider     sendKnobs[kNumSends];
        juce::Label      sendDbLabels[kNumSends];
        juce::String     sendBusNames[kNumSends];   // empty = unassigned

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

        // Link to the ArrangementModel so plugin mutations broadcast to all views
        void setArrangementModel(NovaStudio::ArrangementModel& model);

        // Open the plugin editor for a given track/slot (creates floating window)
        void openPluginEditor(int trackIndex, int pluginSlot);

        // Open the plugin browser to load a plugin onto a track/slot
        void openPluginBrowser(int trackIndex, int slotIndex);

    private:
        void buildStrips();
        void timerCallback() override;
        void changeListenerCallback(juce::ChangeBroadcaster*) override;
        void notifyPluginChainChanged();
        // Returns list of internal buses (one per aux track) + optionally hardware outputs
        juce::StringArray buildBusList(bool includeHardware) const;

        NovaStudio::StudioAudioEngine& engine;
        NovaStudio::ArrangementModel*  arrangementModelPtr = nullptr;

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
