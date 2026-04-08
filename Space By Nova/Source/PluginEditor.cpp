#include "PluginEditor.h"
#include "BinaryData.h"

#include <cstring>

SpaceByNovaAudioProcessorEditor::SpaceByNovaAudioProcessorEditor (SpaceByNovaAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    webView = std::make_unique<SinglePageBrowser> (createWebOptions (*this));

    spaceAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("space"), spaceRelay, nullptr);
    airAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("air"), airRelay, nullptr);
    depthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("depth"), depthRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"), mixRelay, nullptr);
    widthAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("width"), widthRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment> (*processorRef.apvts.getParameter ("nova_mode"), modeRelay, nullptr);

    addAndMakeVisible (*webView);

    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot()
                              + "/index.html?v=" + juce::String (juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL (cacheBustedUrl);

    setResizable (false, false);
    setSize (960, 560);
}

SpaceByNovaAudioProcessorEditor::~SpaceByNovaAudioProcessorEditor() = default;

void SpaceByNovaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (243, 239, 232));
}

void SpaceByNovaAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

juce::WebBrowserComponent::Options SpaceByNovaAudioProcessorEditor::createWebOptions (SpaceByNovaAudioProcessorEditor& editor)
{
    auto options = juce::WebBrowserComponent::Options {};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (
                         juce::WebBrowserComponent::Options::WinWebView2 {}
                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("SpaceByNova")));
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([&editor] (const juce::String& url)
                     {
                         return editor.getResource (url);
                     })
                     .withOptionsFrom (editor.spaceRelay)
                     .withOptionsFrom (editor.airRelay)
                     .withOptionsFrom (editor.depthRelay)
                     .withOptionsFrom (editor.mixRelay)
                     .withOptionsFrom (editor.widthRelay)
                     .withOptionsFrom (editor.modeRelay);

    return options;
}

std::optional<juce::WebBrowserComponent::Resource> SpaceByNovaAudioProcessorEditor::getResource (const juce::String& url)
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

    auto resourcePath = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    resourcePath = resourcePath.upToFirstOccurrenceOf ("?", false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    if (resourcePath == "/index.html")
        return makeResource (space_by_nova_BinaryData::index_html,
                             space_by_nova_BinaryData::index_htmlSize,
                             "text/html");

    if (resourcePath == "/js/index.js")
        return makeResource (space_by_nova_BinaryData::index_js,
                             space_by_nova_BinaryData::index_jsSize,
                             "text/javascript");

    if (resourcePath == "/js/juce/index.js")
        return makeResource (space_by_nova_BinaryData::index_js2,
                             space_by_nova_BinaryData::index_js2Size,
                             "text/javascript");

    if (resourcePath == "/js/juce/check_native_interop.js")
        return makeResource (space_by_nova_BinaryData::check_native_interop_js,
                             space_by_nova_BinaryData::check_native_interop_jsSize,
                             "text/javascript");

    return std::nullopt;
}
