#pragma once

#include <juce_core/juce_core.h>
#include "MeterEngine.h"

namespace tz
{

struct MeterSpec
{
    const char* key;
    const char* name;
    const char* unit;
    float scaleMin, scaleMax;   // display range
    float defMin, defMax;       // default target range
    const char* help;
};

const std::array<MeterSpec, NumMeters>& meterSpecs();

enum class HoldMode { Highest, Lowest };

struct MeterTarget
{
    float min = -20.0f, max = -10.0f;
    bool visible = true;
    HoldMode hold = HoldMode::Highest;
};

enum class Grade { NoSignal, Below, InRange, Over };

inline Grade gradeOf (float v, const MeterTarget& t) noexcept
{
    if (! hasValue (v)) return Grade::NoSignal;
    if (v < t.min)      return Grade::Below;
    if (v > t.max)      return Grade::Over;
    return Grade::InRange;
}

struct Profile
{
    juce::String name;
    std::array<MeterTarget, NumMeters> meters {};
    bool factory = false;

    static Profile defaults (const juce::String& name);
    juce::var toVar() const;
    static Profile fromVar (const juce::var& v);
    bool sameTargetsAs (const Profile& other) const;
};

/** Factory profiles plus user profiles stored in a JSON file shared by all plugin instances. */
class ProfileStore
{
public:
    ProfileStore();

    void reload();
    const juce::Array<Profile>& all() const { return profiles; }
    const Profile* find (const juce::String& name) const;

    /** Saves or overwrites a user profile. Returns false for a factory name. */
    bool saveUser (const Profile& p);
    bool removeUser (const juce::String& name);

    static juce::File storageFile();

private:
    void writeFile() const;
    juce::Array<Profile> profiles;
};

} // namespace tz
