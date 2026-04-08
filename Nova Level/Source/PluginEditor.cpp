#include "PluginEditor.h"

NovaLevelAudioProcessorEditor::NovaLevelAudioProcessorEditor(NovaLevelAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processorRef(p)
{
    juce::ignoreUnused (processorRef);

    auto configureKnob = [] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 22);
        slider.setRange (-24.0, 24.0, 0.1);
        slider.setValue (0.0);
    };

    titleLabel.setText ("NOVA LEVEL", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (34.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    inputLabel.setText ("Input", juce::dontSendNotification);
    inputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (inputLabel);

    amountLabel.setText ("Level", juce::dontSendNotification);
    amountLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (amountLabel);

    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputLabel);

    configureKnob (inputSlider);
    configureKnob (amountSlider);
    configureKnob (outputSlider);

    addAndMakeVisible (inputSlider);
    addAndMakeVisible (amountSlider);
    addAndMakeVisible (outputSlider);

    bypassButton.setButtonText ("Bypass");
    addAndMakeVisible (bypassButton);

    setSize(980, 620);
}

NovaLevelAudioProcessorEditor::~NovaLevelAudioProcessorEditor() {
}

void NovaLevelAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(236, 226, 213));

    g.setColour (juce::Colour::fromRGB (23, 21, 18));
    g.drawRoundedRectangle (40.0f, 40.0f, 900.0f, 540.0f, 18.0f, 2.0f);
}

void NovaLevelAudioProcessorEditor::resized() {
    titleLabel.setBounds (270, 64, 440, 50);

    const int knobSize = 180;
    const int knobY = 190;
    inputSlider.setBounds (150, knobY, knobSize, knobSize);
    amountSlider.setBounds (400, knobY, knobSize, knobSize);
    outputSlider.setBounds (650, knobY, knobSize, knobSize);

    inputLabel.setBounds (150, 150, knobSize, 24);
    amountLabel.setBounds (400, 150, knobSize, 24);
    outputLabel.setBounds (650, 150, knobSize, 24);

    bypassButton.setBounds (438, 430, 104, 28);
}
