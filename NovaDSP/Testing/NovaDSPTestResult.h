#pragma once

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
};
