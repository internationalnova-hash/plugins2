#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace nova::basslab
{

enum class NovaCandidateStatus { Candidate, Favorite, Approved, Rejected };

// A shipped NovaBassPreset never carries workflow-only state (spec
// sections 37, and the Milestone-A-era design note this plan inherited:
// "Candidate/Favorite/Approved/Rejected... has no meaning once a preset
// ships"). This sidecar index is entirely independent of NovaBassPreset -
// it can track raw generated/mutated sounds that never become preset
// files at all, right up until a sound designer discards them.
struct NovaCandidateEntry
{
    juce::String id;
    NovaCandidateStatus status = NovaCandidateStatus::Candidate;
    juce::String generatorId;       // e.g. "hard_808" - empty if not Generate-originated
    juce::String createdDate;       // ISO date string
    juce::String mutationParentId;  // empty if not a Mutate result
    juce::String renderedAssetPath; // vault-relative - empty until Render actually runs
};

// Persists Metadata/candidate_index.json. Decoupled from NovaBassVault
// (takes a plain juce::File) so it's independently testable via an
// isolated temp path, same convention as NovaBassVaultValidation.
class NovaBassCandidateIndex
{
public:
    void setIndexFileLocation (const juce::File& file) noexcept { indexFile = file; }

    void load();
    bool save() const;

    // Returns the new entry's generated id.
    juce::String addCandidate (const juce::String& generatorId, const juce::String& mutationParentId = {});

    bool setStatus (const juce::String& id, NovaCandidateStatus status);
    bool setRenderedAssetPath (const juce::String& id, const juce::String& assetPath);

    const NovaCandidateEntry* find (const juce::String& id) const;
    const std::vector<NovaCandidateEntry>& getAll() const noexcept { return entries; }
    std::vector<NovaCandidateEntry> getByStatus (NovaCandidateStatus status) const;

    static juce::String statusToString (NovaCandidateStatus status);
    static NovaCandidateStatus statusFromString (const juce::String& s);

private:
    juce::File indexFile;
    std::vector<NovaCandidateEntry> entries;
};

} // namespace nova::basslab
