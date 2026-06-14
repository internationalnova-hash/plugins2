#include "WaveformCache.h"
#include <algorithm>

WaveformCache::WaveformCache()
    : stopWorker(false)
{
    formatManager.registerBasicFormats();
    workerThread = std::thread([this] { processJobs(); });
}

WaveformCache::~WaveformCache()
{
    {
        const std::lock_guard<std::mutex> lock(workerMutex);
        stopWorker = true;
    }
    workerCondition.notify_all();
    if (workerThread.joinable())
        workerThread.join();
}

bool WaveformCache::isCached(const juce::File& file) const
{
    const std::lock_guard<std::mutex> lock(mutex);
    return cache.find(file.getFullPathName()) != cache.end();
}

std::vector<float> WaveformCache::getPeaks(const juce::File& file) const
{
    const std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(file.getFullPathName());
    if (it == cache.end()) return {};
    return it->second.peaks;
}

int WaveformCache::getNumChannels(const juce::File& file) const
{
    const std::lock_guard<std::mutex> lock(mutex);
    auto it = cache.find(file.getFullPathName());
    if (it == cache.end()) return 1;
    return it->second.numChannels;
}

bool WaveformCache::hasPendingJob(const juce::String& key, int samplesPerPixel) const
{
    for (const auto& job : jobQueue)
        if (job.file.getFullPathName() == key && job.samplesPerPixel == samplesPerPixel)
            return true;
    return false;
}

void WaveformCache::processJobs()
{
    while (true)
    {
        Job nextJob;
        {
            std::unique_lock<std::mutex> lock(workerMutex);
            workerCondition.wait(lock, [this] { return stopWorker || !jobQueue.empty(); });
            if (stopWorker && jobQueue.empty()) return;

            auto bestIt = std::min_element(jobQueue.begin(), jobQueue.end(),
                [](const Job& a, const Job& b) { return a.priority < b.priority; });
            nextJob = *bestIt;
            jobQueue.erase(bestIt);
        }

        if (!isCached(nextJob.file))
            ensureCached(nextJob.file, nextJob.samplesPerPixel);
    }
}

bool WaveformCache::ensureCached(const juce::File& file, int samplesPerPixel)
{
    if (!file.existsAsFile()) return false;

    const juce::String key = file.getFullPathName();
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (cache.find(key) != cache.end()) return true;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return false;

    const int64_t totalSamples = reader->lengthInSamples;
    if (totalSamples <= 0) return false;

    const int numCh     = (int)juce::jmin<int>((int)reader->numChannels, 2); // cap at stereo
    const int numBlocks = (int)((totalSamples + samplesPerPixel - 1) / samplesPerPixel);

    // Layout: per block → [ch0_min, ch0_max, ch1_min, ch1_max, ...]
    std::vector<float> peaks;
    peaks.reserve(numBlocks * numCh * 2);

    juce::AudioBuffer<float> buffer(numCh, samplesPerPixel);

    for (int block = 0; block < numBlocks; ++block)
    {
        const int64_t startSample = (int64_t)block * samplesPerPixel;
        const int readCount = (int)juce::jmin<int64_t>(samplesPerPixel, totalSamples - startSample);

        if (readCount <= 0)
        {
            for (int ch = 0; ch < numCh; ++ch) { peaks.push_back(0.0f); peaks.push_back(0.0f); }
            continue;
        }

        buffer.clear();
        reader->read(&buffer, 0, readCount, startSample, true, true);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float minV = 0.0f, maxV = 0.0f;
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < readCount; ++i)
            {
                const float v = data[i];
                if (v < minV) minV = v;
                if (v > maxV) maxV = v;
            }
            peaks.push_back(minV);
            peaks.push_back(maxV);
        }
    }

    {
        const std::lock_guard<std::mutex> lock(mutex);
        cache[key].peaks.swap(peaks);
        cache[key].numChannels = numCh;
    }

    return true;
}

void WaveformCache::ensureCachedAsync(const juce::File& file, int samplesPerPixel)
{
    if (!file.existsAsFile()) return;

    const juce::String key = file.getFullPathName();
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (cache.find(key) != cache.end()) return;
    }

    {
        const std::lock_guard<std::mutex> lock(workerMutex);
        if (hasPendingJob(key, samplesPerPixel)) return;
        jobQueue.push_back({ file, samplesPerPixel, samplesPerPixel });
    }
    workerCondition.notify_one();
}
