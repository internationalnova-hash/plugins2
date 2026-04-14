#include "PluginProcessor.h"
#include "PluginEditor.h"

NovaPitchAudioProcessorEditor::NovaPitchAudioProcessorEditor (NovaPitchAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (600, 400);

    // Key selector
    keyBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keyBox.setSelectedItemIndex (0);
    keyBox.addListener (this);
    addAndMakeVisible (keyBox);
    keyLabel.attachToComponent (&keyBox, true);
    addAndMakeVisible (keyLabel);

    // Scale selector
    scaleBox.addItemList (juce::StringArray { "Chromatic", "Major", "Minor", "Pentatonic", "Blues" }, 1);
    scaleBox.setSelectedItemIndex (0);
    scaleBox.addListener (this);
    addAndMakeVisible (scaleBox);
    scaleLabel.attachToComponent (&scaleBox, true);
    addAndMakeVisible (scaleLabel);

    // Tolerance slider
    toleranceSlider.setRange (0.0, 100.0, 1.0);
    toleranceSlider.setValue (50.0);
    toleranceSlider.addListener (this);
    addAndMakeVisible (toleranceSlider);
    toleranceLabel.attachToComponent (&toleranceSlider, true);
    addAndMakeVisible (toleranceLabel);

    // Amount slider
    amountSlider.setRange (0.0, 100.0, 1.0);
    amountSlider.setValue (85.0);
    amountSlider.addListener (this);
    addAndMakeVisible (amountSlider);
    amountLabel.attachToComponent (&amountSlider, true);
    addAndMakeVisible (amountLabel);

    // Confidence threshold slider
    confidenceSlider.setRange (0.0, 100.0, 1.0);
    confidenceSlider.setValue (70.0);
    confidenceSlider.addListener (this);
    addAndMakeVisible (confidenceSlider);
    confidenceLabel.attachToComponent (&confidenceSlider, true);
    addAndMakeVisible (confidenceLabel);

    // Detected pitch display
    detectedPitchDisplay.setText ("Detected: -- Hz", juce::dontSendNotification);
    addAndMakeVisible (detectedPitchDisplay);

    // Corrected pitch display
    correctedPitchDisplay.setText ("Corrected: -- Hz", juce::dontSendNotification);
    addAndMakeVisible (correctedPitchDisplay);

    startTimer (50); // Update UI every 50ms
}

NovaPitchAudioProcessorEditor::~NovaPitchAudioProcessorEditor()
{
    stopTimer();
}

void NovaPitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Nova Pitch - Automatic Pitch Correction", getLocalBounds(), juce::Justification::centredTop, 1);
}

void NovaPitchAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (20);
    bounds.removeFromTop (30);

    auto leftMargin = bounds.getX();

    auto row = bounds.removeFromTop (30);
    keyLabel.setBounds (leftMargin, row.getY(), 100, row.getHeight());
    keyBox.setBounds (leftMargin + 100, row.getY(), 150, row.getHeight());

    row = bounds.removeFromTop (30);
    scaleLabel.setBounds (leftMargin, row.getY(), 100, row.getHeight());
    scaleBox.setBounds (leftMargin + 100, row.getY(), 150, row.getHeight());

    row = bounds.removeFromTop (30);
    toleranceLabel.setBounds (leftMargin, row.getY(), 100, row.getHeight());
    toleranceSlider.setBounds (leftMargin + 100, row.getY(), 200, row.getHeight());

    row = bounds.removeFromTop (30);
    amountLabel.setBounds (leftMargin, row.getY(), 100, row.getHeight());
    amountSlider.setBounds (leftMargin + 100, row.getY(), 200, row.getHeight());

    row = bounds.removeFromTop (30);
    confidenceLabel.setBounds (leftMargin, row.getY(), 100, row.getHeight());
    confidenceSlider.setBounds (leftMargin + 100, row.getY(), 200, row.getHeight());

    row = bounds.removeFromTop (30);
    detectedPitchDisplay.setBounds (row);

    row = bounds.removeFromTop (30);
    correctedPitchDisplay.setBounds (row);
}

void NovaPitchAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &toleranceSlider)
        audioProcessor.apvts.getParameter ("tolerance")->setValueNotifyingHost (toleranceSlider.getValue() / 100.0f);
    else if (slider == &amountSlider)
        audioProcessor.apvts.getParameter ("amount")->setValueNotifyingHost (amountSlider.getValue() / 100.0f);
    else if (slider == &confidenceSlider)
        audioProcessor.apvts.getParameter ("confidenceThreshold")->setValueNotifyingHost (confidenceSlider.getValue() / 100.0f);
}

void NovaPitchAudioProcessorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &keyBox)
    {
        const float normalized = static_cast<float> (keyBox.getSelectedItemIndex()) / 11.0f;
        audioProcessor.apvts.getParameter ("key")->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
    }
    else if (comboBoxThatHasChanged == &scaleBox)
    {
        const float normalized = static_cast<float> (scaleBox.getSelectedItemIndex()) / 4.0f;
        audioProcessor.apvts.getParameter ("scale")->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
    }
}

void NovaPitchAudioProcessorEditor::timerCallback()
{
    float detected = audioProcessor.getDetectedPitch();
    float corrected = audioProcessor.getCorrectedPitch();

    detectedPitchDisplay.setText (juce::String::formatted ("Detected: %.1f Hz", detected), juce::dontSendNotification);
    correctedPitchDisplay.setText (juce::String::formatted ("Corrected: %.1f Hz", corrected), juce::dontSendNotification);
}
