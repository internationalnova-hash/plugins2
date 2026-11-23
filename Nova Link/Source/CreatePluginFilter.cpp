#include "PluginProcessor.h"

JUCE_API juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaLinkAudioProcessor();
}
