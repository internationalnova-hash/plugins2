#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include "NovaLicense.h"

class NovaBassEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit NovaBassEditor (NovaBassProcessor&);
    ~NovaBassEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NovaBassProcessor& processorRef;

    // CRITICAL: order is Relays -> WebView -> Attachments (matches every
    // other Nova plugin's WebView editor in this catalog).
    juce::WebSliderRelay shapeRelay { ParameterIDs::shape };
    juce::WebSliderRelay decayRelay { ParameterIDs::decay };
    juce::WebSliderRelay driveRelay { ParameterIDs::drive };
    juce::WebSliderRelay widthRelay { ParameterIDs::width };
    juce::WebSliderRelay glideRelay { ParameterIDs::glide };
    juce::WebSliderRelay bassRelay { ParameterIDs::bass };
    juce::WebComboBoxRelay colorModeRelay { ParameterIDs::colorMode };
    juce::WebToggleButtonRelay bypassRelay { ParameterIDs::bypass };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot()) || newURL == getResourceProviderRoot();
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> shapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> decayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> driveAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> widthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> glideAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bassAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> colorModeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::WebBrowserComponent::Options createWebOptions (NovaBassEditor& editor);

    // ---- Preset browser native-function RPC surface (JS -> C++), matching
    // Nova Keys' established pattern (richer than a simple param relay can
    // express: search text, category filter, favorites, prev/next). ----
    juce::var jsGetPresetLibrary (const juce::Array<juce::var>&);
    juce::var jsSelectPreset (const juce::Array<juce::var>&);
    juce::var jsNextPreset (const juce::Array<juce::var>&);
    juce::var jsPreviousPreset (const juce::Array<juce::var>&);
    juce::var jsToggleFavorite (const juce::Array<juce::var>&);
    juce::var jsBuildLibraryJson();
    void pushStatusToJs();

    // ── License gate ─────────────────────────────────────────────────
    // Same plain-native-overlay pattern proven on Nova Sub: simpler and
    // lower-risk than routing activation through the WebView bridge.
    NovaLicense license { "nova-bass" };
    bool unlocked = false;

    juce::Label unlockTitle;
    juce::Label unlockStatus;
    juce::TextEditor licenseKeyInput;
    juce::TextButton activateButton { "Activate" };

    struct UnlockOverlay : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGBA (5, 4, 8, 235));
        }
    };
    UnlockOverlay unlockOverlay;

    void buildUnlockOverlay();
    void handleActivateClicked();
    void setUnlockedState (bool isUnlocked);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBassEditor)
};
