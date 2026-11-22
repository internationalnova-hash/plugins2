#include "NovaBassCandidateIndex.h"

namespace nova::basslab
{

juce::String NovaBassCandidateIndex::statusToString (NovaCandidateStatus status)
{
    switch (status)
    {
        case NovaCandidateStatus::Favorite: return "favorite";
        case NovaCandidateStatus::Approved: return "approved";
        case NovaCandidateStatus::Rejected: return "rejected";
        case NovaCandidateStatus::Candidate:
        default: return "candidate";
    }
}

NovaCandidateStatus NovaBassCandidateIndex::statusFromString (const juce::String& s)
{
    if (s == "favorite") return NovaCandidateStatus::Favorite;
    if (s == "approved") return NovaCandidateStatus::Approved;
    if (s == "rejected") return NovaCandidateStatus::Rejected;
    return NovaCandidateStatus::Candidate;
}

void NovaBassCandidateIndex::load()
{
    entries.clear();
    if (! indexFile.existsAsFile()) return;

    auto parsed = juce::JSON::parse (indexFile);
    if (auto* arr = parsed.getArray())
    {
        for (auto& v : *arr)
        {
            NovaCandidateEntry e;
            e.id = v.getProperty ("id", "").toString();
            e.status = statusFromString (v.getProperty ("status", "candidate").toString());
            e.generatorId = v.getProperty ("generatorId", "").toString();
            e.createdDate = v.getProperty ("createdDate", "").toString();
            e.mutationParentId = v.getProperty ("mutationParentId", "").toString();
            e.renderedAssetPath = v.getProperty ("renderedAssetPath", "").toString();
            if (e.id.isNotEmpty())
                entries.push_back (e);
        }
    }
}

bool NovaBassCandidateIndex::save() const
{
    juce::Array<juce::var> arr;
    for (auto& e : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", e.id);
        o->setProperty ("status", statusToString (e.status));
        o->setProperty ("generatorId", e.generatorId);
        o->setProperty ("createdDate", e.createdDate);
        o->setProperty ("mutationParentId", e.mutationParentId);
        o->setProperty ("renderedAssetPath", e.renderedAssetPath);
        arr.add (juce::var (o));
    }

    indexFile.getParentDirectory().createDirectory();
    return indexFile.replaceWithText (juce::JSON::toString (juce::var (arr), false));
}

juce::String NovaBassCandidateIndex::addCandidate (const juce::String& generatorId, const juce::String& mutationParentId)
{
    NovaCandidateEntry e;
    e.id = juce::Uuid().toString();
    e.status = NovaCandidateStatus::Candidate;
    e.generatorId = generatorId;
    const auto now = juce::Time::getCurrentTime();
    e.createdDate = juce::String::formatted ("%04d-%02d-%02d", now.getYear(), now.getMonth() + 1, now.getDayOfMonth());
    e.mutationParentId = mutationParentId;
    entries.push_back (e);
    return e.id;
}

bool NovaBassCandidateIndex::setStatus (const juce::String& id, NovaCandidateStatus status)
{
    for (auto& e : entries)
        if (e.id == id) { e.status = status; return true; }
    return false;
}

bool NovaBassCandidateIndex::setRenderedAssetPath (const juce::String& id, const juce::String& assetPath)
{
    for (auto& e : entries)
        if (e.id == id) { e.renderedAssetPath = assetPath; return true; }
    return false;
}

const NovaCandidateEntry* NovaBassCandidateIndex::find (const juce::String& id) const
{
    for (auto& e : entries)
        if (e.id == id) return &e;
    return nullptr;
}

std::vector<NovaCandidateEntry> NovaBassCandidateIndex::getByStatus (NovaCandidateStatus status) const
{
    std::vector<NovaCandidateEntry> result;
    for (auto& e : entries)
        if (e.status == status) result.push_back (e);
    return result;
}

} // namespace nova::basslab
