// Headless validation harness for NovaBassCandidateIndex. Uses an
// isolated temp file throughout - never touches the real vault's
// Metadata/candidate_index.json. Confirms lifecycle transitions persist
// across a fresh instance (load/save round trip).

#include "Lifecycle/NovaBassCandidateIndex.h"
#include <juce_core/juce_core.h>
#include <cstdio>

using namespace nova::basslab;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

int main()
{
    std::printf ("=== NovaBassCandidateIndex Validation ===\n");

    auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("NovaBassLabCandidateIndexValidation_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json");
    tempFile.deleteFile();

    std::printf ("-- Basic add/status/render-path tracking --\n");
    {
        NovaBassCandidateIndex index;
        index.setIndexFileLocation (tempFile);
        index.load(); // no file yet - should not crash

        check (index.getAll().empty(), "starts empty when no file exists yet");

        const auto id1 = index.addCandidate ("hard_808");
        const auto id2 = index.addCandidate ("", id1); // a mutation of id1

        check (id1.isNotEmpty() && id2.isNotEmpty() && id1 != id2, "addCandidate returns distinct non-empty ids");
        check (index.getAll().size() == 2, "both candidates are tracked");

        auto* e1 = index.find (id1);
        check (e1 != nullptr && e1->generatorId == "hard_808", "generatorId is tracked for a Generate-originated candidate");

        auto* e2 = index.find (id2);
        check (e2 != nullptr && e2->mutationParentId == id1, "mutationParentId correctly links a mutated candidate to its parent");

        check (index.setStatus (id1, NovaCandidateStatus::Approved), "setStatus succeeds for a known id");
        check (! index.setStatus ("not-a-real-id", NovaCandidateStatus::Approved), "setStatus fails gracefully for an unknown id");

        check (index.setRenderedAssetPath (id1, "NVB_808_Hard_C1_v001.wav"), "setRenderedAssetPath succeeds");

        auto approved = index.getByStatus (NovaCandidateStatus::Approved);
        check (approved.size() == 1 && approved[0].id == id1, "getByStatus(Approved) returns exactly the approved candidate");

        check (index.save(), "save() succeeds");
    }

    std::printf ("-- Persistence across a fresh instance --\n");
    {
        NovaBassCandidateIndex fresh;
        fresh.setIndexFileLocation (tempFile);
        fresh.load();

        check (fresh.getAll().size() == 2, "a fresh instance loading the same file sees both candidates");

        bool foundApprovedWithRenderPath = false;
        for (auto& e : fresh.getAll())
            if (e.status == NovaCandidateStatus::Approved && e.renderedAssetPath == "NVB_808_Hard_C1_v001.wav")
                foundApprovedWithRenderPath = true;

        check (foundApprovedWithRenderPath, "status and renderedAssetPath both persisted correctly to disk and back");
    }

    tempFile.deleteFile();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
