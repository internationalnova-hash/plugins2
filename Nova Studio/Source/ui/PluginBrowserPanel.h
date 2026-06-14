#pragma once

#include <JuceHeader.h>
#include "../StudioAudioEngine.h"
#include "Theme.h"

namespace NovaStudioUI
{
    // ─────────────────────────────────────────────────────────────────────────
    // PluginBrowserPanel
    // A floating/embedded panel for scanning, browsing, and loading VST3/LV2 plugins.
    // ─────────────────────────────────────────────────────────────────────────
    class PluginBrowserPanel : public juce::Component,
                               private juce::ListBoxModel,
                               private juce::Button::Listener,
                               private juce::TextEditor::Listener
    {
    public:
        explicit PluginBrowserPanel(NovaStudio::StudioAudioEngine& engineRef);
        ~PluginBrowserPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // Set which track/slot the next load will target
        void setTargetTrackAndSlot(int trackIndex, int slotIndex);

        // Fired after a plugin is successfully loaded onto a track
        std::function<void(int trackIndex, int slotIndex, const juce::String& name)> onPluginLoaded;

    private:
        // ListBoxModel
        int  getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

        // Button::Listener
        void buttonClicked(juce::Button* b) override;

        // TextEditor::Listener
        void textEditorTextChanged(juce::TextEditor&) override;

        void startScan();
        void rebuildFilteredList();
        void loadSelectedPlugin();

        NovaStudio::StudioAudioEngine& engine;

        juce::TextButton    scanBtn     { "Scan Plugins" };
        juce::TextButton    loadBtn     { "Load to Insert" };
        juce::TextButton    browseBtn   { "Browse File..." };
        juce::TextEditor    searchBox;
        juce::ListBox       pluginList;
        juce::Label         statusLabel;
        juce::Label         targetLabel;
        double              scanProgress = 0.0;
        juce::ProgressBar   progressBar { scanProgress };

        juce::AudioPluginFormatManager formatManager;
        juce::KnownPluginList          knownPlugins;
        juce::Array<juce::PluginDescription> allPlugins;
        juce::Array<juce::PluginDescription> filteredPlugins;

        int targetTrack = 0;
        int targetSlot  = 0;
        bool scanning   = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrowserPanel)
    };

    // ─────────────────────────────────────────────────────────────────────────
    // PluginBrowserWindow — floating DocumentWindow wrapper
    // ─────────────────────────────────────────────────────────────────────────
    class PluginBrowserWindow : public juce::DocumentWindow
    {
    public:
        PluginBrowserWindow(NovaStudio::StudioAudioEngine& engine,
                            int targetTrack, int targetSlot,
                            std::function<void(int, int, const juce::String&)> onLoaded)
            : juce::DocumentWindow("Plugin Browser",
                                   juce::Colour::fromRGB(18, 20, 28),
                                   DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            panel = std::make_unique<PluginBrowserPanel>(engine);
            panel->setTargetTrackAndSlot(targetTrack, targetSlot);
            panel->onPluginLoaded = [this, onLoaded](int t, int s, const juce::String& name)
            {
                if (onLoaded) onLoaded(t, s, name);
            };
            setUsingNativeTitleBar(false);
            setContentNonOwned(panel.get(), false);
            setSize(520, 560);
            centreWithSize(520, 560);
            setResizable(true, false);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override { setVisible(false); }

        PluginBrowserPanel* getPanel() { return panel.get(); }

    private:
        std::unique_ptr<PluginBrowserPanel> panel;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrowserWindow)
    };
}
