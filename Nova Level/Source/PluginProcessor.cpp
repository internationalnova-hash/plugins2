#include "PluginProcessor.h"
#include "PluginEditor.h"

NovaLevelAudioProcessor::NovaLevelAudioProcessor()
	: AudioProcessor (BusesProperties()
		.withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
		.withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
	  apvts (*this, nullptr, juce::Identifier ("NovaLevel"), createParameterLayout())
{
}

NovaLevelAudioProcessor::~NovaLevelAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaLevelAudioProcessor::createParameterLayout()
{
	juce::AudioProcessorValueTreeState::ParameterLayout layout;
	layout.add (std::make_unique<juce::AudioParameterFloat> ("compression", "Compression", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.0f));
	layout.add (std::make_unique<juce::AudioParameterFloat> ("tone", "Tone", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));
	layout.add (std::make_unique<juce::AudioParameterFloat> ("output", "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
	layout.add (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode", juce::StringArray { "SMOOTH", "PUNCH", "LIMIT" }, 0));
	layout.add (std::make_unique<juce::AudioParameterBool> ("magic", "Magic", false));
	layout.add (std::make_unique<juce::AudioParameterChoice> ("meter", "Meter", juce::StringArray { "GR", "OUT" }, 0));
	return layout;
void NovaLevelAudioProcessor::applyPreset(const juce::String& presetName) {
	if (presetName == "VOCAL") {
		apvts.getParameter("mode")->setValueNotifyingHost(0.0f);
		apvts.getParameter("compression")->setValueNotifyingHost(0.45f); // 4.5
		apvts.getParameter("tone")->setValueNotifyingHost(0.55f); // 5.5
		apvts.getParameter("output")->setValueNotifyingHost(0.5833f); // 2.0 dB
	} else if (presetName == "BASS") {
		apvts.getParameter("mode")->setValueNotifyingHost(0.0f);
		apvts.getParameter("compression")->setValueNotifyingHost(0.55f); // 5.5
		apvts.getParameter("tone")->setValueNotifyingHost(0.4f); // 4.0
		apvts.getParameter("output")->setValueNotifyingHost(0.625f); // 3.0 dB
	} else if (presetName == "DRUMS") {
		apvts.getParameter("mode")->setValueNotifyingHost(0.5f); // PUNCH
		apvts.getParameter("compression")->setValueNotifyingHost(0.6f); // 6.0
		apvts.getParameter("tone")->setValueNotifyingHost(0.5f); // 5.0
		apvts.getParameter("output")->setValueNotifyingHost(0.6042f); // 1.5 dB
	} else if (presetName == "MASTER") {
		apvts.getParameter("mode")->setValueNotifyingHost(0.0f);
		apvts.getParameter("compression")->setValueNotifyingHost(0.3f); // 3.0
		apvts.getParameter("tone")->setValueNotifyingHost(0.5f); // 5.0
		apvts.getParameter("output")->setValueNotifyingHost(0.5f); // 0.0 dB
	}
}
}

void NovaLevelAudioProcessor::prepareToPlay(double, int) {}
void NovaLevelAudioProcessor::releaseResources() {}
void NovaLevelAudioProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) {}

juce::AudioProcessorEditor* NovaLevelAudioProcessor::createEditor() { return new NovaLevelAudioProcessorEditor(*this); }
bool NovaLevelAudioProcessor::hasEditor() const { return true; }
const juce::String NovaLevelAudioProcessor::getName() const { return "Nova Level"; }

bool NovaLevelAudioProcessor::acceptsMidi() const { return false; }
bool NovaLevelAudioProcessor::producesMidi() const { return false; }
bool NovaLevelAudioProcessor::isMidiEffect() const { return false; }
double NovaLevelAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaLevelAudioProcessor::getNumPrograms() { return 1; }
int NovaLevelAudioProcessor::getCurrentProgram() { return 0; }
void NovaLevelAudioProcessor::setCurrentProgram(int) {}
const juce::String NovaLevelAudioProcessor::getProgramName(int) { return {}; }
void NovaLevelAudioProcessor::changeProgramName(int, const juce::String&) {}

void NovaLevelAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
	if (auto state = apvts.copyState()) {
		std::unique_ptr<juce::XmlElement> xml (state.createXml());
		copyXmlToBinary (*xml, destData);
	}
}
void NovaLevelAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
	std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
	if (xmlState && xmlState->hasTagName (apvts.state.getType()))
		apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
