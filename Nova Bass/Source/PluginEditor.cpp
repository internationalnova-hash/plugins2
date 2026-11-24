#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>
#include "NovaLicenseGate.h"

using namespace nova::bass;

NovaBassEditor::NovaBassEditor (NovaBassProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    shapeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::shape), shapeRelay, nullptr);
    decayAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::decay), decayRelay, nullptr);
    driveAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::drive), driveRelay, nullptr);
    widthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::width), widthRelay, nullptr);
    glideAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::glide), glideRelay, nullptr);
    bassAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bass), bassRelay, nullptr);
    colorModeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::colorMode), colorModeRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (*processorRef.apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    buildUnlockOverlay();
    setUnlockedState (license.isUnlocked());

    setResizable (true, true);
    setResizeLimits (900, 600, 1400, 920);
    setSize (1150, 720);
    startTimerHz (30);

    NovaLicenseOverlay::attachTo (*this);   // unlock screen until activated
}

NovaBassEditor::~NovaBassEditor()
{
    stopTimer();
}

void NovaBassEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0A0A0C));
}

void NovaBassEditor::resized()
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

void NovaBassEditor::buildUnlockOverlay()
{
    unlockOverlay.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (unlockOverlay);

    unlockTitle.setText ("Unlock Nova Bass", juce::dontSendNotification);
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

void NovaBassEditor::handleActivateClicked()
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

void NovaBassEditor::setUnlockedState (bool isUnlocked)
{
    unlocked = isUnlocked;
    unlockOverlay.setVisible (! isUnlocked);

    // WebBrowserComponent wraps a native platform view, which can render
    // above ordinary JUCE-painted components regardless of z-order - so
    // hide the native view outright while locked (same fix as Nova Sub).
    if (webView != nullptr)
        webView->setVisible (isUnlocked);

    if (isUnlocked)
        unlockStatus.setText ("Unlocked", juce::dontSendNotification);
}

juce::var NovaBassEditor::jsBuildLibraryJson()
{
    auto* root = new juce::DynamicObject();

    auto& vault = processorRef.getVault();
    root->setProperty ("vaultFound", vault.isVaultFound());
    root->setProperty ("currentIndex", processorRef.getCurrentPresetIndex());

    auto& pm = processorRef.getPresetManager();
    juce::Array<juce::var> presets;
    for (int i = 0; i < pm.getNumPresets(); ++i)
    {
        auto* preset = pm.getPreset (i);
        auto* po = new juce::DynamicObject();
        po->setProperty ("index", i);
        po->setProperty ("name", preset->meta.name);
        po->setProperty ("category", preset->meta.category);
        po->setProperty ("description", preset->meta.description);
        po->setProperty ("favorite", preset->meta.favorite);
        presets.add (juce::var (po));
    }
    root->setProperty ("presets", juce::var (presets));

    juce::Array<juce::var> cats;
    for (auto& c : pm.getCategories()) cats.add (c);
    root->setProperty ("categories", juce::var (cats));

    return juce::var (root);
}

void NovaBassEditor::pushStatusToJs()
{
    if (webView == nullptr || ! webView->isVisible()) return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("anyVoiceActive", processorRef.anyVoiceActive.load());
    obj->setProperty ("midiActivity", processorRef.midiActivity.exchange (false));
    obj->setProperty ("currentIndex", processorRef.getCurrentPresetIndex());

    const auto json = juce::JSON::toString (juce::var (obj), true);
    webView->evaluateJavascript ("window.updateStatus && window.updateStatus(" + json + ");", nullptr);
}

void NovaBassEditor::timerCallback()
{
    pushStatusToJs();
}

juce::WebBrowserComponent::Options NovaBassEditor::createWebOptions (NovaBassEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("NovaBass")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url) { return editor.getResource (url); })
                     .withOptionsFrom (editor.shapeRelay)
                     .withOptionsFrom (editor.decayRelay)
                     .withOptionsFrom (editor.driveRelay)
                     .withOptionsFrom (editor.widthRelay)
                     .withOptionsFrom (editor.glideRelay)
                     .withOptionsFrom (editor.bassRelay)
                     .withOptionsFrom (editor.colorModeRelay)
                     .withOptionsFrom (editor.bypassRelay)
                     .withNativeFunction ("getLibrary", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         completion (editor.jsBuildLibraryJson());
                     })
                     .withNativeFunction ("selectPreset", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             const int index = (int) args[0];
                             editor.processorRef.loadPresetByIndex (index);
                             editor.processorRef.getPresetManager().setSelectedIndex (index);
                         }
                         completion (editor.jsBuildLibraryJson());
                     })
                     .withNativeFunction ("nextPreset", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto& pm = editor.processorRef.getPresetManager();
                         pm.selectNext();
                         editor.processorRef.loadPresetByIndex (pm.getSelectedIndex());
                         completion (editor.jsBuildLibraryJson());
                     })
                     .withNativeFunction ("previousPreset", [&editor] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         auto& pm = editor.processorRef.getPresetManager();
                         pm.selectPrevious();
                         editor.processorRef.loadPresetByIndex (pm.getSelectedIndex());
                         completion (editor.jsBuildLibraryJson());
                     })
                     .withNativeFunction ("toggleFavorite", [&editor] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                         {
                             auto& pm = editor.processorRef.getPresetManager();
                             const int index = (int) args[0];
                             if (auto* preset = pm.getPreset (index))
                                 pm.setFavorite (index, ! preset->meta.favorite);
                         }
                         completion (editor.jsBuildLibraryJson());
                     })
                     .withNativeFunction ("jsError", [] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion)
                     {
                         if (! args.isEmpty())
                             juce::Logger::writeToLog ("[NovaBass][JS ERROR] " + args[0].toString());
                         completion (juce::var());
                     });

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> NovaBassEditor::getResource (const juce::String& url)
{
    auto makeResource = [] (const void* data, int size, const char* mime)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (size));
        std::memcpy (bytes.data(), data, static_cast<size_t> (size));
        return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
    };

    // Match by substring on the raw url - deliberately NOT root-stripped
    // (that was found to silently break JS loading in Nova AI Master).
    const auto lowerUrl = url.toLowerCase();

    if (lowerUrl.contains ("juce/index.js"))
        return makeResource (nova_bass_BinaryData::index_js2, nova_bass_BinaryData::index_js2Size, "text/javascript");

    if (lowerUrl.contains ("juce/check_native_interop.js"))
        return makeResource (nova_bass_BinaryData::check_native_interop_js, nova_bass_BinaryData::check_native_interop_jsSize, "text/javascript");

    if (lowerUrl.contains ("js/index.js"))
        return makeResource (nova_bass_BinaryData::index_js, nova_bass_BinaryData::index_jsSize, "text/javascript");

    if (lowerUrl.contains ("index.html") || lowerUrl.endsWith ("/") || lowerUrl.isEmpty())
        return makeResource (nova_bass_BinaryData::index_html, nova_bass_BinaryData::index_htmlSize, "text/html");

    return std::nullopt;
}
