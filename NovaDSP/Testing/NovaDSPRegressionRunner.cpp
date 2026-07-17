// NovaDSP Regression Runner
//
// Compiles and runs all NovaDSP regression suites in one pass.
// Prints a structured report to stdout.
// Exits 0 on full pass, 1 on any failure.
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
// Run:   build/NovaDSPRegressionRunner

#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <string>
#include <cstring>

// ── JUCE initialisation ────────────────────────────────────────────────────
// AudioBuffer and dsp::ProcessSpec pull in juce_audio_basics / juce_dsp.
// The JUCE modules need exactly one translation unit that defines
// JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED.  We define the required macros here.
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#define JUCE_STANDALONE_APPLICATION 0
#define JUCE_USE_CURL 0
#define JUCE_WEB_BROWSER 0

// ── Shared result type ─────────────────────────────────────────────────────
#include "NovaDSPTestResult.h"

// ── Engine source files ────────────────────────────────────────────────────
// Each engine .cpp must be compiled into this target (see CMakeLists.txt).

// ── Regression suites ─────────────────────────────────────────────────────
#include "../Dynamics/NovaLevelRegressionTest.h"
#include "../Modulation/NovaMotionRegressionTest.h"
#include "../Spatial/NovaSpaceRegressionTest.h"
#include "../Console/NovaConsoleRegressionTest.h"

// ── Suite descriptor ──────────────────────────────────────────────────────
struct Suite
{
    const char* name;
    std::vector<NovaDSPTestResult> (*run)();
};

static const Suite suites[] =
{
    { "NovaLevel",   NovaLevelRegressionTest::runAllResults   },
    { "NovaMotion",  NovaMotionRegressionTest::runAllResults  },
    { "NovaSpace",   NovaSpaceRegressionTest::runAllResults   },
    { "NovaConsole", NovaConsoleRegressionTest::runAllResults },
};

// ── Formatting helpers ────────────────────────────────────────────────────
static void printLine (char c = '-', int width = 72)
{
    for (int i = 0; i < width; ++i) std::putchar (c);
    std::putchar ('\n');
}

static const char* passFail (bool p) { return p ? "PASS" : "FAIL"; }

int main()
{
    using Clock = std::chrono::steady_clock;

    printLine ('=');
    std::printf ("  NovaDSP Regression Runner\n");
    printLine ('=');

    std::vector<NovaDSPTestResult> allResults;
    allResults.reserve (256);

    // ── Per-suite timing and collection ──────────────────────────────────
    for (auto& suite : suites)
    {
        std::printf ("\nRunning suite: %s\n", suite.name);
        printLine ();

        auto t0 = Clock::now();
        std::vector<NovaDSPTestResult> results = suite.run();
        auto t1 = Clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        int passed = 0, failed = 0;
        float suiteMax = 0.0f;
        const NovaDSPTestResult* firstFail = nullptr;

        for (auto& r : results)
        {
            if (r.passed)
                ++passed;
            else
            {
                ++failed;
                if (firstFail == nullptr) firstFail = &r;
            }
            if (r.peakAbsDiff > suiteMax)
                suiteMax = r.peakAbsDiff;
        }

        // Per-scenario detail (only failures printed individually)
        for (auto& r : results)
        {
            if (!r.passed)
                std::printf ("  [FAIL] %-55s  peak=%e\n", r.scenario.c_str(), (double)r.peakAbsDiff);
        }

        std::printf ("  Scenarios : %d\n", (int)results.size());
        std::printf ("  Passed    : %d\n", passed);
        std::printf ("  Failed    : %d\n", failed);
        std::printf ("  Max peak  : %e\n", (double)suiteMax);
        if (firstFail)
            std::printf ("  First fail: %s\n", firstFail->scenario.c_str());
        std::printf ("  Time      : %.1f ms\n", elapsedMs);
        std::printf ("  Result    : %s\n", passFail (failed == 0));

        for (auto& r : results)
            allResults.push_back (r);
    }

    // ── Grand total ───────────────────────────────────────────────────────
    int totalPassed = 0, totalFailed = 0;
    float globalMax = 0.0f;
    std::string firstFailScenario;
    std::string firstFailSuite;

    for (auto& r : allResults)
    {
        if (r.passed)
            ++totalPassed;
        else
        {
            ++totalFailed;
            if (firstFailScenario.empty())
            {
                firstFailScenario = r.scenario;
                firstFailSuite    = r.suite;
            }
        }
        if (r.peakAbsDiff > globalMax)
            globalMax = r.peakAbsDiff;
    }

    std::printf ("\n");
    printLine ('=');
    std::printf ("  GRAND TOTAL\n");
    printLine ('=');
    std::printf ("  Suites    : %d\n", (int)(sizeof(suites)/sizeof(suites[0])));
    std::printf ("  Scenarios : %d\n", (int)allResults.size());
    std::printf ("  Passed    : %d\n", totalPassed);
    std::printf ("  Failed    : %d\n", totalFailed);
    std::printf ("  Max peak  : %e  (0.0 = sample-identical)\n", (double)globalMax);

    if (!firstFailScenario.empty())
        std::printf ("  First fail: [%s] %s\n",
                     firstFailSuite.c_str(), firstFailScenario.c_str());

    std::printf ("  Result    : %s\n", passFail (totalFailed == 0));
    printLine ('=');

    return totalFailed == 0 ? 0 : 1;
}
