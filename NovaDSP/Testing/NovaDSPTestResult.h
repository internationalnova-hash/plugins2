#pragma once

#include <cstdint>
#include <string>

// Common result record emitted by every NovaDSP regression suite's
// runAllResults() method.  The runner accumulates these and prints the report.

struct NovaDSPTestResult
{
    std::string suite;         // "NovaLevel", "NovaMotion", "NovaSpace", "NovaConsole"
    std::string scenario;      // scenario name / label
    bool        passed        = false;
    float       peakAbsDiff   = 0.0f;
    float       rmsAbsDiff    = 0.0f;

    // FNV-1a 64-bit hash of the engine output buffer samples (channel-interleaved).
    // Zero means the suite does not compute hashes for this scenario.
    // Non-zero: identical values across runs confirm deterministic behavior.
    uint64_t    outputHash    = 0;
};
