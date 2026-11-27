#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The page (Generate / Hum it) is a WebView; the footer is native because dragging
// a MIDI file out into the DAW has to start from a native view.
class NovaMelodyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NovaMelodyEditor (NovaMelodyProcessor&);
    ~NovaMelodyEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kW = 1240, kPageH = 690, kFootH = 70;

private:
    void timerCallback() override;
    juce::WebBrowserComponent::Options createWebOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::File writeTempMidi();
    void exportMidi();
    float scale() const { return (float) getWidth() / (float) kW; }

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override { return newURL.startsWith (getResourceProviderRoot()); }
    };

    struct DragHandle : juce::Component
    {
        explicit DragHandle (NovaMelodyEditor& e) : ed (e) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragging = false; }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        NovaMelodyEditor& ed;
        bool dragging = false;
    };

    struct PlayButton : juce::Button
    {
        PlayButton() : juce::Button ("play") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        bool playing = false;
    };

    NovaMelodyProcessor& proc;
    std::unique_ptr<SinglePageBrowser> webView;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::MemoryBlock dropData;
    juce::String dropName;
    int sentState = -1;
    juce::String sentParams;
    bool pageReady = false;

    PlayButton playBtn;
    juce::ComboBox soundBox;
    juce::ToggleButton midiOutBtn { "MIDI out to your instrument" };
    DragHandle dragHandle { *this };
    juce::TextButton exportBtn { "Export .mid" };
    std::unique_ptr<juce::ComboBoxParameterAttachment> soundAttach;
    std::unique_ptr<juce::ButtonParameterAttachment> midiAttach;
    std::unique_ptr<juce::LookAndFeel_V4> lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMelodyEditor)
};
