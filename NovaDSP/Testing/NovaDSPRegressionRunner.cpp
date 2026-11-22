// NovaDSP Regression Runner
//
// Runs all NovaDSP regression suites and produces:
//   regression_results.json   — machine-readable per-scenario detail
//   regression_summary.csv    — flat table for CI dashboards
//   performance_baselines.json— suite timing baselines (created/updated on each run)
//
// Exits 0 on full pass, 1 on any correctness failure.
// Performance regressions (>20% slower than baseline) produce warnings, not failures.
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
// Run:   build/NovaDSPRegressionRunner [--update-baselines]

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <cstring>
#include <chrono>
#include <vector>
#include <string>

#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#define JUCE_STANDALONE_APPLICATION 0
#define JUCE_USE_CURL 0
#define JUCE_WEB_BROWSER 0

#include "NovaDSPTestResult.h"

#include "../Dynamics/NovaLevelRegressionTest.h"
#include "../Modulation/NovaMotionRegressionTest.h"
#include "../Spatial/NovaSpaceRegressionTest.h"
#include "../Console/NovaConsoleRegressionTest.h"

// ── Suite descriptor ──────────────────────────────────────────────────────────
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

static constexpr int NUM_SUITES = (int)(sizeof(suites) / sizeof(suites[0]));

// ── Per-suite run summary ─────────────────────────────────────────────────────
struct SuiteSummary
{
    std::string suite;
    int   total    = 0;
    int   passed   = 0;
    int   failed   = 0;
    float maxPeak  = 0.0f;
    double elapsedMs = 0.0;
};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void printLine (char c = '-', int width = 72)
{
    for (int i = 0; i < width; ++i) std::putchar (c);
    std::putchar ('\n');
}

static std::string escapeJson (const std::string& s)
{
    std::string out;
    out.reserve (s.size() + 4);
    for (char c : s)
    {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else                out += c;
    }
    return out;
}

static std::string currentTimestamp()
{
    std::time_t t = std::time (nullptr);
    char buf[32];
    struct tm tm_info;
#ifdef _WIN32
    gmtime_s (&tm_info, &t);
#else
    gmtime_r (&t, &tm_info);
#endif
    std::strftime (buf, sizeof (buf), "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    return buf;
}

// ── JSON output ───────────────────────────────────────────────────────────────
static void writeJson (const char* path,
                       const std::string& timestamp,
                       const std::vector<SuiteSummary>& suiteStats,
                       const std::vector<NovaDSPTestResult>& all,
                       int totalPassed, int totalFailed,
                       float globalMax, double totalMs)
{
    FILE* f = std::fopen (path, "w");
    if (!f) { std::fprintf (stderr, "Warning: cannot write %s\n", path); return; }

    std::fprintf (f, "{\n");
    std::fprintf (f, "  \"format\": \"NovaDSP_RegressionResults\",\n");
    std::fprintf (f, "  \"date\": \"%s\",\n", timestamp.c_str());

    // Per-suite summary
    std::fprintf (f, "  \"suites\": [\n");
    for (int i = 0; i < (int)suiteStats.size(); ++i)
    {
        const auto& s = suiteStats[i];
        std::fprintf (f,
            "    { \"suite\": \"%s\", \"scenarios\": %d, \"passed\": %d, "
            "\"failed\": %d, \"maxPeakAbsDiff\": %.6e, \"elapsedMs\": %.2f }%s\n",
            s.suite.c_str(), s.total, s.passed, s.failed,
            (double)s.maxPeak, s.elapsedMs,
            i + 1 < (int)suiteStats.size() ? "," : "");
    }
    std::fprintf (f, "  ],\n");

    // Per-scenario detail
    std::fprintf (f, "  \"scenarios\": [\n");
    for (int i = 0; i < (int)all.size(); ++i)
    {
        const auto& r = all[i];
        char hashStr[24];  // "0x" + 16 hex + 2 quotes + null = 22 bytes
        if (r.outputHash != 0)
            std::snprintf (hashStr, sizeof (hashStr), "\"0x%016llx\"",
                           (unsigned long long)r.outputHash);
        else
            std::snprintf (hashStr, sizeof (hashStr), "null");

        std::fprintf (f,
            "    { \"suite\": \"%s\", \"scenario\": \"%s\", \"passed\": %s, "
            "\"peakAbsDiff\": %.6e, \"rmsAbsDiff\": %.6e, \"outputHash\": %s }%s\n",
            escapeJson (r.suite).c_str(),
            escapeJson (r.scenario).c_str(),
            r.passed ? "true" : "false",
            (double)r.peakAbsDiff, (double)r.rmsAbsDiff,
            hashStr,
            i + 1 < (int)all.size() ? "," : "");
    }
    std::fprintf (f, "  ],\n");

    // Grand total
    std::fprintf (f,
        "  \"summary\": {\n"
        "    \"totalScenarios\": %d,\n"
        "    \"passed\": %d,\n"
        "    \"failed\": %d,\n"
        "    \"maxPeakAbsDiff\": %.6e,\n"
        "    \"totalElapsedMs\": %.2f,\n"
        "    \"result\": \"%s\"\n"
        "  }\n}\n",
        totalPassed + totalFailed, totalPassed, totalFailed,
        (double)globalMax, totalMs,
        totalFailed == 0 ? "PASS" : "FAIL");

    std::fclose (f);
}

// ── CSV output ────────────────────────────────────────────────────────────────
static void writeCsv (const char* path,
                      const std::string& timestamp,
                      const std::vector<NovaDSPTestResult>& all)
{
    FILE* f = std::fopen (path, "w");
    if (!f) { std::fprintf (stderr, "Warning: cannot write %s\n", path); return; }

    std::fprintf (f, "date,suite,scenario,passed,peakAbsDiff,rmsAbsDiff,outputHash\n");
    for (const auto& r : all)
    {
        // Quote the scenario field (may contain commas in Level labels)
        std::fprintf (f, "%s,%s,\"%s\",%s,%.6e,%.6e",
            timestamp.c_str(),
            r.suite.c_str(),
            r.scenario.c_str(),
            r.passed ? "true" : "false",
            (double)r.peakAbsDiff,
            (double)r.rmsAbsDiff);
        if (r.outputHash != 0)
            std::fprintf (f, ",0x%016llx\n", (unsigned long long)r.outputHash);
        else
            std::fprintf (f, ",\n");
    }
    std::fclose (f);
}

// ── Performance baselines ─────────────────────────────────────────────────────
// Format (hand-written / hand-parsed JSON-subset, no external libraries):
//   { "NovaLevel": 78.8, "NovaMotion": 34.9, "NovaSpace": 145.3, "NovaConsole": 387.6 }

static bool loadBaseline (const char* path, const char* suiteName, double& outMs)
{
    FILE* f = std::fopen (path, "r");
    if (!f) return false;

    char buf[2048];
    size_t n = std::fread (buf, 1, sizeof (buf) - 1, f);
    std::fclose (f);
    buf[n] = '\0';

    // Search for "SuiteName": <number>
    char key[128];
    std::snprintf (key, sizeof (key), "\"%s\":", suiteName);
    const char* pos = std::strstr (buf, key);
    if (!pos) return false;

    pos += std::strlen (key);
    while (*pos == ' ' || *pos == '\t') ++pos;
    outMs = std::strtod (pos, nullptr);
    return outMs > 0.0;
}

static void saveBaselines (const char* path, const std::vector<SuiteSummary>& stats)
{
    FILE* f = std::fopen (path, "w");
    if (!f) { std::fprintf (stderr, "Warning: cannot write %s\n", path); return; }

    std::fprintf (f, "{\n");
    std::fprintf (f, "  \"format\": \"NovaDSP_PerformanceBaselines\",\n");
    std::fprintf (f, "  \"date\": \"%s\"", currentTimestamp().c_str());
    for (const auto& s : stats)
        std::fprintf (f, ",\n  \"%s\": %.2f", s.suite.c_str(), s.elapsedMs);
    std::fprintf (f, "\n}\n");
    std::fclose (f);
}

// ── Determinism double-run (NovaConsole) ──────────────────────────────────────
static int checkDeterminism (bool verbose)
{
    if (verbose) std::printf ("\nDeterminism check: running NovaConsole twice...\n");

    auto run1 = NovaConsoleRegressionTest::runAllResults();
    auto run2 = NovaConsoleRegressionTest::runAllResults();

    if (run1.size() != run2.size())
    {
        std::printf ("  [FAIL] scenario count differs between runs (%zu vs %zu)\n",
                     run1.size(), run2.size());
        return 1;
    }

    int mismatches = 0;
    for (size_t i = 0; i < run1.size(); ++i)
    {
        const auto& a = run1[i];
        const auto& b = run2[i];

        bool hashMatch   = (a.outputHash  == b.outputHash);
        bool peakMatch   = (a.peakAbsDiff == b.peakAbsDiff);
        bool rmsMatch    = (a.rmsAbsDiff  == b.rmsAbsDiff);
        bool passedMatch = (a.passed      == b.passed);

        if (!hashMatch || !peakMatch || !rmsMatch || !passedMatch)
        {
            ++mismatches;
            std::printf ("  [FAIL] non-determinism in scenario %zu: %s\n",
                         i, a.scenario.c_str());
            if (!hashMatch)
                std::printf ("         hash   run1=0x%016llx  run2=0x%016llx\n",
                             (unsigned long long)a.outputHash,
                             (unsigned long long)b.outputHash);
            if (!peakMatch)
                std::printf ("         peak   run1=%.6e  run2=%.6e\n",
                             (double)a.peakAbsDiff, (double)b.peakAbsDiff);
            if (!rmsMatch)
                std::printf ("         rms    run1=%.6e  run2=%.6e\n",
                             (double)a.rmsAbsDiff, (double)b.rmsAbsDiff);
        }
    }

    if (mismatches == 0)
    {
        std::printf ("  Determinism: %zu scenarios IDENTICAL across both runs\n",
                     run1.size());
        std::printf ("  Hash check : all output hashes stable\n");
    }
    return mismatches;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main (int argc, char** argv)
{
    using Clock = std::chrono::steady_clock;

    bool updateBaselines = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], "--update-baselines") == 0)
            updateBaselines = true;

    const std::string timestamp = currentTimestamp();
    const char* baselinesPath = "performance_baselines.json";

    printLine ('=');
    std::printf ("  NovaDSP Regression Runner\n");
    std::printf ("  %s\n", timestamp.c_str());
    printLine ('=');

    std::vector<NovaDSPTestResult> allResults;
    allResults.reserve (512);
    std::vector<SuiteSummary> suiteStats;

    // ── Run suites ────────────────────────────────────────────────────────────
    for (const auto& suite : suites)
    {
        std::printf ("\nRunning suite: %s\n", suite.name);
        printLine ();

        auto t0 = Clock::now();
        std::vector<NovaDSPTestResult> results = suite.run();
        double elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        SuiteSummary ss;
        ss.suite     = suite.name;
        ss.total     = (int)results.size();
        ss.elapsedMs = elapsedMs;

        const NovaDSPTestResult* firstFail = nullptr;

        for (const auto& r : results)
        {
            if (r.passed) ++ss.passed; else { ++ss.failed; if (!firstFail) firstFail = &r; }
            if (r.peakAbsDiff > ss.maxPeak) ss.maxPeak = r.peakAbsDiff;
        }

        for (const auto& r : results)
            if (!r.passed)
                std::printf ("  [FAIL] %-55s  peak=%.6e\n",
                             r.scenario.c_str(), (double)r.peakAbsDiff);

        // ── Performance baseline comparison ───────────────────────────────────
        double baselineMs = 0.0;
        bool hasBaseline  = loadBaseline (baselinesPath, suite.name, baselineMs);

        std::printf ("  Scenarios : %d\n", ss.total);
        std::printf ("  Passed    : %d\n", ss.passed);
        std::printf ("  Failed    : %d\n", ss.failed);
        std::printf ("  Max peak  : %.6e\n", (double)ss.maxPeak);
        if (firstFail)
            std::printf ("  First fail: %s\n", firstFail->scenario.c_str());
        std::printf ("  Time      : %.1f ms", elapsedMs);

        if (hasBaseline)
        {
            double pct = (elapsedMs - baselineMs) / baselineMs * 100.0;
            if (pct > 20.0)
                std::printf ("  [PERF WARN] +%.0f%% vs baseline (%.1f ms)", pct, baselineMs);
            else if (pct < -5.0)
                std::printf ("  (%.0f%% faster than baseline)", -pct);
        }
        else
        {
            std::printf ("  (no baseline yet)");
        }
        std::printf ("\n");
        std::printf ("  Result    : %s\n", ss.failed == 0 ? "PASS" : "FAIL");

        for (auto& r : results) allResults.push_back (r);
        suiteStats.push_back (ss);
    }

    // ── Determinism double-run ────────────────────────────────────────────────
    std::printf ("\n");
    printLine ('-');
    int deterMismatches = checkDeterminism (true);

    // ── Grand total ───────────────────────────────────────────────────────────
    int totalPassed = 0, totalFailed = 0;
    float globalMax = 0.0f;
    double totalMs  = 0.0;
    std::string firstFailScenario, firstFailSuite;

    for (const auto& r : allResults)
    {
        if (r.passed) ++totalPassed; else {
            ++totalFailed;
            if (firstFailScenario.empty()) { firstFailScenario = r.scenario; firstFailSuite = r.suite; }
        }
        if (r.peakAbsDiff > globalMax) globalMax = r.peakAbsDiff;
    }
    for (const auto& s : suiteStats) totalMs += s.elapsedMs;

    std::printf ("\n");
    printLine ('=');
    std::printf ("  GRAND TOTAL\n");
    printLine ('=');
    std::printf ("  Suites       : %d\n",   NUM_SUITES);
    std::printf ("  Scenarios    : %d\n",   totalPassed + totalFailed);
    std::printf ("  Passed       : %d\n",   totalPassed);
    std::printf ("  Failed       : %d\n",   totalFailed);
    std::printf ("  Max peak     : %.6e  (0.0 = sample-identical)\n", (double)globalMax);
    std::printf ("  Determinism  : %s\n",   deterMismatches == 0 ? "PASS" : "FAIL");
    if (!firstFailScenario.empty())
        std::printf ("  First fail   : [%s] %s\n",
                     firstFailSuite.c_str(), firstFailScenario.c_str());
    std::printf ("  Total time   : %.1f ms\n", totalMs);
    std::printf ("  Result       : %s\n",   totalFailed == 0 && deterMismatches == 0 ? "PASS" : "FAIL");
    printLine ('=');

    // ── File outputs ──────────────────────────────────────────────────────────
    writeJson ("regression_results.json", timestamp,
               suiteStats, allResults, totalPassed, totalFailed, globalMax, totalMs);
    writeCsv  ("regression_summary.csv", timestamp, allResults);

    bool wroteNew = !updateBaselines;  // if not updating, don't overwrite silently
    (void)wroteNew;
    if (updateBaselines)
    {
        saveBaselines (baselinesPath, suiteStats);
        std::printf ("\nBaselines updated: %s\n", baselinesPath);
    }
    else
    {
        // On first run (no baselines exist), create them automatically.
        FILE* check = std::fopen (baselinesPath, "r");
        if (!check)
        {
            saveBaselines (baselinesPath, suiteStats);
            std::printf ("\nBaselines created: %s\n", baselinesPath);
        }
        else
        {
            std::fclose (check);
        }
    }

    std::printf ("Written: regression_results.json\n");
    std::printf ("Written: regression_summary.csv\n");

    const bool overallPass = (totalFailed == 0) && (deterMismatches == 0);
    return overallPass ? 0 : 1;
}
