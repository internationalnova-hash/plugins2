#include "PluginEditor.h"

NovaLevelAudioProcessorEditor::NovaLevelAudioProcessorEditor(NovaLevelAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processorRef(p)
{
    juce::ignoreUnused (processorRef);
    setSize(980, 620);
}

NovaLevelAudioProcessorEditor::~NovaLevelAudioProcessorEditor() {
}

void NovaLevelAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(236, 226, 213));
}

void NovaLevelAudioProcessorEditor::resized() {
}
