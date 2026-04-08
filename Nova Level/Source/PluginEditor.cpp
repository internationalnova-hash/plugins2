#include "PluginEditor.h"

NovaLevelAudioProcessorEditor::NovaLevelAudioProcessorEditor(NovaLevelAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processorRef(p)
{
    webView = std::make_unique<SinglePageBrowser>();

    compressionAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*processorRef.apvts.getParameter("compression"), compressionRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*processorRef.apvts.getParameter("tone"), toneRelay, nullptr);
    outputAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*processorRef.apvts.getParameter("output"), outputRelay, nullptr);
    modeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*processorRef.apvts.getParameter("mode"), modeRelay, nullptr);
    magicAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*processorRef.apvts.getParameter("magic"), magicRelay, nullptr);
    meterAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*processorRef.apvts.getParameter("meter"), meterRelay, nullptr);

    addAndMakeVisible(*webView);
    const auto cacheBustedUrl = juce::WebBrowserComponent::getResourceProviderRoot() + "/index.html?v=" + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    webView->goToURL(cacheBustedUrl);

    setResizable(false, false);
    setSize(980, 620);
    startTimerHz(30);
}

NovaLevelAudioProcessorEditor::~NovaLevelAudioProcessorEditor() {
    stopTimer();
}

void NovaLevelAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(236, 226, 213));
}

void NovaLevelAudioProcessorEditor::resized() {
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}

void NovaLevelAudioProcessorEditor::timerCallback() {
    if (webView == nullptr || !webView->isVisible())
        return;
    const auto output = processorRef.outputPeakLevel.load();
    const auto gainReduction = processorRef.gainReductionLevel.load();
    const auto isHot = processorRef.outputIsHot.load();
    const auto meterJs = "if (window.updateMeters) { window.updateMeters(" + juce::String(gainReduction, 3)
        + ", " + juce::String(output, 3)
        + ", " + juce::String(isHot ? "true" : "false") + "); }";
    webView->evaluateJavascript(meterJs);
}
