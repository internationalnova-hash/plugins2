#pragma once

// Mix Assistant, part 3: carries a Plan out on the live session, and keeps
// before/after snapshots so the whole thing is one undo step and can be A/B'd.

#include "MixPlanner.h"
#include "../StudioAudioEngine.h"

namespace NovaStudio::MixAI
{
    struct ApplyResult
    {
        bool ok = false;
        int pluginsLoaded = 0, busesMade = 0, tracksChanged = 0;
        juce::StringArray missingPlugins;   // not installed / not found
        juce::StringArray log;
        float masterLufsBefore = -100.f;    // the mix Nova AI Master analysed
    };

    /// Finds an installed plugin by name (scans the VST3 folders once if needed)
    bool findPlugin (StudioAudioEngine& e, const juce::String& name, juce::PluginDescription& out);

    /// progress (optional) gets 0..1 while the finished mix is rendered for Nova AI Master's analysis
    ApplyResult applyPlan (StudioAudioEngine& e, const Plan& p, std::function<void (float)> progress = {});

    /// The same in three steps, for the app (the middle one is slow and may run on a background thread):
    ///   1. message thread: everything except the analysed Nova AI Master
    ApplyResult applyPlanExceptMaster (StudioAudioEngine& e, const Plan& p);
    ///   2. any thread: render the mix as it now stands and let Nova AI Master's engine decide + calibrate.
    ///      Returns the plan as XML (empty if the render failed or was cancelled).
    juce::String analyseMixForMaster (StudioAudioEngine& e, const PluginStep& step, ApplyResult& r, std::function<bool (float)> progress);
    ///   3. message thread: load Nova AI Master holding that plan (as if Analyze + Master had been pressed)
    bool loadAnalysedMaster (StudioAudioEngine& e, const PluginStep& step, const juce::String& planXml, ApplyResult& r);

    /// Whole-session snapshots (plugins included) for undo and Before/After.
    /// Restoring keeps the session's own file name, so Save still goes to the right place.
    juce::File takeSnapshot (StudioAudioEngine& e, const juce::String& tag);
    bool restoreSnapshot (StudioAudioEngine& e, const juce::File& snapshot);
}
