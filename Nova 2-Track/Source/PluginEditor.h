#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

#if NOVA_2TRACK_REQUIRE_LICENSE
 #include "NovaLicense.h"
#endif

class NovaTwoTrackAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit NovaTwoTrackAudioProcessorEditor (NovaTwoTrackAudioProcessor&);
    ~NovaTwoTrackAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::var buildState() const;
    juce::String spectrumJson();

    // Spectrum: FFT of the newest 2048 output samples, as mid and side, on a log-frequency grid.
    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder, kBins = 120;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftM = std::vector<float> (2 * kFftSize), fftS = std::vector<float> (2 * kFftSize);
    std::array<float, kBins> smoothM {}, smoothS {};

    NovaTwoTrackAudioProcessor& processorRef;

   #if NOVA_2TRACK_REQUIRE_LICENSE
    NovaLicense license { "nova-2-track" };
   #endif

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL.startsWith (getResourceProviderRoot());
        }
    };

    std::unique_ptr<SinglePageBrowser> webView;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::WebBrowserComponent::Options createWebOptions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTwoTrackAudioProcessorEditor)
};
