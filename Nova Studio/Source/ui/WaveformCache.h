#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>

class WaveformCache
{
public:
    WaveformCache();
    ~WaveformCache();

    bool ensureCached(const juce::File& file, int samplesPerPixel = 512);
    void ensureCachedAsync(const juce::File& file, int samplesPerPixel = 512);
    bool isCached(const juce::File& file, int samplesPerPixel = 0) const;

    // Returns a shared_ptr — zero-copy, safe to hold across paint calls.
    // Layout per block: ch0_min, ch0_max, ch1_min, ch1_max, ...
    struct PeakData {
        std::shared_ptr<std::vector<float>> peaks;
        int numChannels = 1;
        float filePeak  = 1.0f;   // pre-computed max absolute value
    };
    PeakData getPeakData(const juce::File& file) const;

    // Legacy helpers kept for compatibility
    std::vector<float> getPeaks(const juce::File& file) const;
    int getNumChannels(const juce::File& file) const;

private:
    struct Entry
    {
        std::shared_ptr<std::vector<float>> peaks;
        int   numChannels    = 1;
        float filePeak       = 1.0f;
        int   samplesPerPixel = 0; // resolution this entry was built at
    };
    struct Job { juce::File file; int samplesPerPixel = 512; int priority = 0; };

    void processJobs();
    bool hasPendingJob(const juce::String& key, int samplesPerPixel) const;

    mutable std::mutex mutex;
    std::map<juce::String, Entry> cache;
    juce::AudioFormatManager formatManager;

    mutable std::mutex workerMutex;
    std::condition_variable workerCondition;
    std::deque<Job> jobQueue;
    bool stopWorker = false;
    std::thread workerThread;
};
