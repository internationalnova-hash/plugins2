#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginLibrary.h"
#include "RackPresets.h"
#include "RackSlot.h"

class NovaRackAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer,
                               private juce::AsyncUpdater
{
public:
    static constexpr int kMaxSlots = 8;

    NovaRackAudioProcessor();
    ~NovaRackAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return tailSeconds.load(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    // Rack editing: message thread only.
    juce::String addSlot (const juce::String& pluginName, int index, const nr::SlotData* initial = nullptr);
    void removeSlot (int index);
    void moveSlot (int from, int to);
    void clearRack();
    juce::StringArray loadRack (const nr::RackData&);      // returns plugins that aren't installed
    nr::RackData captureRack();
    nr::SlotData captureSlot (int index);
    void applySlotData (int index, const nr::SlotData&);

    bool getBypass (int index) const;
    void setBypass (int index, bool);
    float getMix (int index) const;
    void setMix (int index, float);
    float getGainDb (bool output) const;
    void setGainDb (bool output, float db);

    void openPluginWindow (int index);
    void closeAllWindows();

    int numSlots() const { return (int) slots.size(); }
    nr::Slot* slot (int index) { return juce::isPositiveAndBelow (index, numSlots()) ? slots[(size_t) index].get() : nullptr; }

    juce::SharedResourcePointer<nr::PluginLibrary> library;
    nr::PresetStore presets;
    juce::AudioProcessorValueTreeState apvts;

    juce::String rackName { "New Rack" };
    bool rackDirty = false;                 // changed since the rack preset was loaded/saved
    std::atomic<int> rackGeneration { 0 };  // bumped when the chain changes (editor refreshes)

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void publishChain();
    void prepareSlot (nr::Slot&);
    void restoreFromJson (const juce::String&);
    void resolveMissing();
    void updateLatency();
    int seenLibraryGeneration = -1;
    juce::String stateJson();

    std::vector<std::unique_ptr<nr::Slot>> slots;   // message-thread owner

    juce::CriticalSection structureLock;             // slots vector: message-thread edits vs. host state reads
    juce::CriticalSection audioLock;                 // guards audioSlots; held briefly by the message thread
    juce::Array<nr::Slot*> audioSlots;

    juce::CriticalSection pendingLock;
    juce::String pendingState;                       // setStateInformation from a non-message thread

    std::array<std::atomic<float>*, kMaxSlots> bypassParams {}, mixParams {};
    std::atomic<float>* inGainParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;
    juce::LinearSmoothedValue<float> inGain, outGain;

    double sampleRate = 44100.0;
    int blockSize = 512;
    bool isPrepared = false;
    std::atomic<double> tailSeconds { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRackAudioProcessor)
};
