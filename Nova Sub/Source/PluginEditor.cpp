#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

NovaSubAudioProcessorEditor::NovaSubAudioProcessorEditor (NovaSubAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    colorAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::color), colorRelay, nullptr);
    filterKnobAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::filterKnob), filterKnobRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::mix), mixRelay, nullptr);
    circuitModeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::circuitMode), circuitModeRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    buildUnlockOverlay();
    setUnlockedState (license.isUnlocked());

    setResizable (true, true);
    setResizeLimits (324, 277, 810, 693);
    setSize (540, 462);
    startTimerHz (30);
}

NovaSubAudioProcessorEditor::~NovaSubAudioProcessorEditor()
{
    stopTimer();
}

void NovaSubAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 10, 15));
}

void NovaSubAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());

    unlockOverlay.setBounds (getLocalBounds());

    auto area = unlockOverlay.getLocalBounds().reduced (60, 60).withSizeKeepingCentre (420, 220);
    unlockTitle.setBounds (area.removeFromTop (28));
    area.removeFromTop (12);
    licenseKeyInput.setBounds (area.removeFromTop (36));
    area.removeFromTop (12);
    activateButton.setBounds (area.removeFromTop (36).withWidth (140));
    area.removeFromTop (12);
    unlockStatus.setBounds (area.removeFromTop (48));
}

void NovaSubAudioProcessorEditor::buildUnlockOverlay()
{
    unlockOverlay.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (unlockOverlay);

    unlockTitle.setText ("Unlock Nova Sub", juce::dontSendNotification);
    unlockTitle.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    unlockTitle.setJustificationType (juce::Justification::centred);
    unlockTitle.setColour (juce::Label::textColourId, juce::Colours::white);
    unlockOverlay.addAndMakeVisible (unlockTitle);

    licenseKeyInput.setTextToShowWhenEmpty ("NOVA-XXXX-XXXX-XXXX", juce::Colours::grey);
    licenseKeyInput.setJustification (juce::Justification::centred);
    unlockOverlay.addAndMakeVisible (licenseKeyInput);

    activateButton.onClick = [this] { handleActivateClicked(); };
    unlockOverlay.addAndMakeVisible (activateButton);

    unlockStatus.setJustificationType (juce::Justification::centred);
    unlockStatus.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    unlockStatus.setFont (juce::FontOptions (13.0f));
    unlockOverlay.addAndMakeVisible (unlockStatus);
}

void NovaSubAudioProcessorEditor::handleActivateClicked()
{
    const auto key = licenseKeyInput.getText();
    if (key.trim().isEmpty())
    {
        unlockStatus.setText ("Enter a license key first.", juce::dontSendNotification);
        return;
    }

    activateButton.setEnabled (false);
    unlockStatus.setText ("Activating...", juce::dontSendNotification);

    license.activate (key, [this] (NovaLicense::ActivationResult result)
    {
        activateButton.setEnabled (true);

        switch (result)
        {
            case NovaLicense::ActivationResult::Success:
                setUnlockedState (true);
                return;
            case NovaLicense::ActivationResult::InvalidKey:
                unlockStatus.setText ("That license key wasn't recognised.", juce::dontSendNotification);
                break;
            case NovaLicense::ActivationResult::Revoked:
                unlockStatus.setText ("This license has been revoked.", juce::dontSendNotification);
                break;
            case NovaLicense::ActivationResult::SeatLimitReached:
                unlockStatus.setText ("This key is already active on its maximum number of machines.", juce::dontSendNotification);
                break;
            case NovaLicense::ActivationResult::NetworkError:
                unlockStatus.setText ("Couldn't reach the activation server. Check your connection.", juce::dontSendNotification);
                break;
            case NovaLicense::ActivationResult::InvalidResponse:
                unlockStatus.setText ("Unexpected response from the activation server.", juce::dontSendNotification);
                break;
        }
    });
}

void NovaSubAudioProcessorEditor::setUnlockedState (bool isUnlocked)
{
    unlocked = isUnlocked;
    unlockOverlay.setVisible (! isUnlocked);

    // WebBrowserComponent wraps a native platform view (WKWebView on macOS),
    // which can render above ordinary JUCE-painted components regardless of
    // Component z-order - so the overlay covering it isn't reliable. Hiding
    // the native view outright while locked sidesteps that entirely.
    if (webView != nullptr)
        webView->setVisible (isUnlocked);

    if (isUnlocked)
        unlockStatus.setText ("Unlocked", juce::dontSendNotification);
}

void NovaSubAudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! webView->isVisible())
        return;

    const auto inputLevel = processorRef.inputPeakLevel.load();
    const auto level = processorRef.outputPeakLevel.load();
    const auto isHot = processorRef.outputIsHot.load();

    const auto meterJs = "if (window.updateInputMeter) { window.updateInputMeter(" + juce::String (inputLevel, 3) + "); }"
                       + "if (window.updateOutputMeter) { window.updateOutputMeter(" + juce::String (level, 3)
                       + ", " + juce::String (isHot ? "true" : "false") + "); }";

    webView->evaluateJavascript (meterJs);
}

juce::WebBrowserComponent::Options NovaSubAudioProcessorEditor::createWebOptions (NovaSubAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaSub")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.colorRelay)
                     .withOptionsFrom (editor.filterKnobRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.circuitModeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaSubAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));

        return juce::WebBrowserComponent::Resource {
            std::move (bytes),
            juce::String (mime)
        };
    };

    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("index.html"))
        return makeResource (nova_sub_BinaryData::index_html,
                             nova_sub_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerUrl.contains ("n_logo.png"))
        return makeResource (nova_sub_BinaryData::n_logo_png,
                             nova_sub_BinaryData::n_logo_pngSize,
                             "image/png");

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (! resourcePath.startsWithChar ('/'))
        resourcePath = "/" + resourcePath;

    const auto lowerPath = resourcePath.toLowerCase();

    if (lowerPath == "/index.html" || lowerPath.endsWith ("/index.html"))
        return makeResource (nova_sub_BinaryData::index_html,
                             nova_sub_BinaryData::index_htmlSize,
                             "text/html");

    if (lowerPath == "/n_logo.png" || lowerPath.endsWith ("/n_logo.png"))
        return makeResource (nova_sub_BinaryData::n_logo_png,
                             nova_sub_BinaryData::n_logo_pngSize,
                             "image/png");

    return std::nullopt;
}
