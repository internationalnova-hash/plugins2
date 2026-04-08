#include "PluginProcessor.h"

// Keep the JUCE plugin factory in its own translation unit so the symbol is
// always emitted and easy for wrapper link stages to resolve.
JUCE_API juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaLevelAudioProcessor();
}
