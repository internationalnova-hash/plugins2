// NovaConsoleAutomationStressTest.cpp
// Standalone automation stress test for Nova Console
// Place in Nova Console/Source/ and add to CMakeLists.txt for test builds

#include "PluginProcessor.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>
#include <iostream>
#include <cfenv>
#include <random>

// Helper: Log anomalies
void logAnomaly(const std::string& msg) {
    std::ofstream log("automation_stress_test.log", std::ios::app);
    log << msg << std::endl;
    std::cout << msg << std::endl;
}

// Helper: Check for NaN/Inf/denormal
inline bool isBadFloat(float v) {
    return std::isnan(v) || std::isinf(v) || (std::abs(v) < 1e-30 && v != 0.0f);
}

// Helper: CPU usage (Linux only, simple)
double getCPUUsage() {
    static long prevIdle = 0, prevTotal = 0;
    std::ifstream stat("/proc/stat");
    std::string cpu;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    stat >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    long idleTime = idle + iowait;
    long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;
    long deltaIdle = idleTime - prevIdle;
    long deltaTotal = totalTime - prevTotal;
    prevIdle = idleTime;
    prevTotal = totalTime;
    if (deltaTotal == 0) return 0.0;
    return 100.0 * (1.0 - (double)deltaIdle / deltaTotal);
}

// Helper: Enable flush-to-zero/denormal suppression
void enableDenormalProtection() {
#if defined(__SSE__)
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    std::feclearexcept(FE_ALL_EXCEPT);
}

// Main test routine
void runAutomationStressTest(NovaConsoleAudioProcessor& proc) {
    using namespace std::chrono_literals;
    auto& apvts = proc.getAPVTS();
    enableDenormalProtection();

    // Parameter IDs (from PluginProcessor.cpp)
    const char* eqFreqs[] = {"lowFreq", "lowMidFreq", "highMidFreq", "highFreq", "airFreq"};
    const char* eqQs[] = {"lowQ", "lowMidQ", "highMidQ", "highQ", "airQ"};
    const char* eqGains[] = {"low", "lowMid", "highMid", "high", "air"};
    const char* params[] = {"threshold", "drive", "analog_heat", "analog_width", "output", "hpf", "lpf"};
    const char* oversampling = "oversampling";
    const char* analogParams[] = {"analog_heat", "analog_depth", "analog_width", "analog_drift", "analog_crosstalk"};

    // CPU logging
    auto logCPU = [](const std::string& label) {
        double cpu = getCPUUsage();
        logAnomaly("CPU [" + label + "]: " + std::to_string(cpu) + "%");
    };

    // Simulate realistic test signals (randomized buffer)
    std::vector<float> testBuffer(512);
    std::mt19937 rng(42);
    std::normal_distribution<float> music(0.0f, 0.2f); // pseudo-music
    for (auto& s : testBuffer) s = music(rng);

    // 1. Denormal protection check (silence/tails)
    for (auto id : analogParams) apvts.getParameter(id)->setValueNotifyingHost(0.5f);
    for (auto& s : testBuffer) s = 0.0f; // silence
    proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
    logCPU("idle/silence");
    for (auto& s : testBuffer) if (isBadFloat(s)) logAnomaly("Denormal detected in silence!");

    // 2. Oversampling toggle safety
    for (int o = 0; o < 3; ++o) {
        apvts.getParameter(oversampling)->setValueNotifyingHost((float)o / 2.0f);
        proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
        logCPU("oversampling_toggle_" + std::to_string(o));
    }

    // 3. Preset gain normalization (simulate preset recall)
    float presetOuts[] = {0.7f, 0.8f, 0.7f, 0.9f, 0.6f, 0.8f};
    for (int i = 0; i < 6; ++i) {
        apvts.getParameter("output")->setValueNotifyingHost(presetOuts[i]);
        proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
        logCPU("preset_" + std::to_string(i));
    }

    // 4. Automation extreme cases
    for (int step = 0; step < 100; ++step) {
        for (auto id : eqFreqs) apvts.getParameter(id)->setValueNotifyingHost((float)step / 100.0f);
        for (auto id : eqQs) apvts.getParameter(id)->setValueNotifyingHost((float)step / 100.0f);
        for (auto id : eqGains) apvts.getParameter(id)->setValueNotifyingHost((float)step / 100.0f);
        for (auto id : params) apvts.getParameter(id)->setValueNotifyingHost((float)step / 100.0f);
        apvts.getParameter(oversampling)->setValueNotifyingHost((step % 3) / 2.0f);
        proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
        if (step % 10 == 0) logCPU("automation_step_" + std::to_string(step));
    }

    // 5. Analog engine stack test (simulate 4 chained instances)
    std::vector<NovaConsoleAudioProcessor> stack(4);
    for (auto& inst : stack) enableDenormalProtection();
    for (int i = 0; i < 10; ++i) {
        for (auto& inst : stack) {
            inst.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
        }
        logCPU("analog_stack_iter_" + std::to_string(i));
    }

    // 6. Simultaneous 10+ parameter changes
    for (int step = 0; step < 50; ++step) {
        for (auto id : eqFreqs) apvts.getParameter(id)->setValueNotifyingHost((float)step / 50.0f);
        for (auto id : eqQs) apvts.getParameter(id)->setValueNotifyingHost((float)step / 50.0f);
        for (auto id : eqGains) apvts.getParameter(id)->setValueNotifyingHost((float)step / 50.0f);
        for (auto id : analogParams) apvts.getParameter(id)->setValueNotifyingHost((float)step / 50.0f);
        proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
    }

    // 7. Realistic test signals (simulate music, drums, vocals)
    std::normal_distribution<float> drums(0.0f, 0.4f);
    std::normal_distribution<float> vocals(0.0f, 0.15f);
    std::normal_distribution<float> mix(0.0f, 0.25f);
    for (auto& s : testBuffer) s = drums(rng);
    proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
    for (auto& s : testBuffer) s = vocals(rng);
    proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
    for (auto& s : testBuffer) s = mix(rng);
    proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());

    // 8. Check for anomalies in meters and state
    float meters[] = {proc.getInputMeter(), proc.getOutputMeter(), proc.getGainReductionMeter()};
    for (float m : meters) {
        if (isBadFloat(m)) logAnomaly("Meter anomaly: " + std::to_string(m));
    }

    // 9. Long session stability simulation (shortened for test, extend for real run)
    logAnomaly("--- BEGIN LONG SESSION STABILITY TEST ---");
    size_t longSessionMinutes = 1; // Set to 60+ for real validation
    size_t cycles = longSessionMinutes * 60; // 1 cycle per second
    double startMem = 0, endMem = 0;
    auto getMem = []() -> double {
        std::ifstream statm("/proc/self/statm");
        double pages = 0; statm >> pages;
        return pages * 4096 / (1024.0 * 1024.0); // MB
    };
    startMem = getMem();
    for (size_t i = 0; i < cycles; ++i) {
        // Simulate automation, preset, oversampling, idle/active
        apvts.getParameter("output")->setValueNotifyingHost((float)(i % 100) / 100.0f);
        apvts.getParameter("drive")->setValueNotifyingHost((float)((i * 3) % 100) / 100.0f);
        apvts.getParameter("analog_heat")->setValueNotifyingHost((float)((i * 7) % 100) / 100.0f);
        apvts.getParameter("threshold")->setValueNotifyingHost((float)((i * 11) % 100) / 100.0f);
        apvts.getParameter("oversampling")->setValueNotifyingHost((i % 3) / 2.0f);
        if (i % 10 == 0) {
            // Simulate preset switch
            apvts.getParameter("output")->setValueNotifyingHost(0.7f);
            apvts.getParameter("drive")->setValueNotifyingHost(0.5f);
        }
        if (i % 30 == 0) {
            // Simulate idle (no parameter change)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        proc.processBlock(juce::AudioBuffer<float>(&testBuffer[0], 1, testBuffer.size()), juce::MidiBuffer());
        if (i % 60 == 0) {
            logCPU("long_session_minute_" + std::to_string(i / 60));
            double mem = getMem();
            logAnomaly("Memory usage: " + std::to_string(mem) + " MB");
        }
    }
    endMem = getMem();
    logAnomaly("Long session memory delta: " + std::to_string(endMem - startMem) + " MB");
    logAnomaly("--- END LONG SESSION STABILITY TEST ---");
    logAnomaly("Automation stress test completed.");
}

// Entrypoint for test build
int main() {
    NovaConsoleAudioProcessor proc;
    runAutomationStressTest(proc);
    return 0;
}
