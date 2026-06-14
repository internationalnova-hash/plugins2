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

    // Ensure a cached downsampled peak buffer exists for the file at the given samplesPerPixel resolution.
    // This call is synchronous and may read the entire file.
    bool ensureCached(const juce::File& file, int samplesPerPixel = 512);

    // Queue a background task to create the cache asynchronously. Returns immediately.
    void ensureCachedAsync(const juce::File& file, int samplesPerPixel = 512);

    // Check if cached
    bool isCached(const juce::File& file) const;

    // Get peaks (min/max interleaved: min0,max0,min1,max1,...). Empty vector if not cached.
    std::vector<float> getPeaks(const juce::File& file) const;

private:
    struct Entry { std::vector<float> peaks; };
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
