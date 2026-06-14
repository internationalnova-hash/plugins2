#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../Session.h"
#include "../StudioAudioEngine.h"

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

        // Callbacks wired by MixerWindow after construction
        std::function<void(float dB)>  onVolumeChanged;
        std::function<void(float pan)> onPanChanged;
        std::function<void(bool)>      onMuteToggled;
        std::function<void(bool)>      onSoloToggled;
        std::function<void(bool)>      onArmToggled;

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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerWindow)
    };
}
