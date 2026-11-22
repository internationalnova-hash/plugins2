#include "Profiles.h"

namespace tz
{

const std::array<MeterSpec, NumMeters>& meterSpecs()
{
    static const std::array<MeterSpec, NumMeters> s {{
        { "peak",      "PEAK",        "dBFS", -30.0f,  3.0f,  -6.0f, -0.3f,
          "Loudest single sample. How close you are to the top." },
        { "truePeak",  "TRUE PEAK",   "dBTP", -30.0f,  3.0f,  -3.0f, -1.0f,
          "Inter-sample peaks (4x oversampled) that can clip after conversion or encoding." },
        { "lufsInt",   "LUFS INT",    "LUFS", -40.0f,  0.0f, -16.0f, -9.0f,
          "Gated average loudness of everything played since the last reset (BS.1770)." },
        { "lufsShort", "LUFS SHORT",  "LUFS", -40.0f,  0.0f, -14.0f, -7.0f,
          "Loudness over a moving 3-second window: how loud it feels right now." },
        { "rms",       "RMS",         "dBFS", -40.0f,  0.0f, -18.0f, -9.0f,
          "Sustained energy over 300 ms: the body of the sound rather than its spikes." },
        { "dynRange",  "DYN RANGE",   "PSR",    0.0f, 25.0f,   6.0f, 12.0f,
          "Peak-to-short-term-loudness ratio. Low = dense and limited, high = open and punchy." },
        { "midDens",   "MID DENSITY", "dB",   -50.0f,  0.0f, -24.0f, -12.0f,
          "Energy in the 250 Hz - 4 kHz band, where listeners feel weight and density." },
    }};
    return s;
}

//==============================================================================
Profile Profile::defaults (const juce::String& n)
{
    Profile p;
    p.name = n;
    for (int i = 0; i < NumMeters; ++i)
    {
        p.meters[(size_t) i].min = meterSpecs()[(size_t) i].defMin;
        p.meters[(size_t) i].max = meterSpecs()[(size_t) i].defMax;
    }
    return p;
}

juce::var Profile::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);

    auto* meterObj = new juce::DynamicObject();
    for (int i = 0; i < NumMeters; ++i)
    {
        const auto& m = meters[(size_t) i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("min", m.min);
        o->setProperty ("max", m.max);
        o->setProperty ("visible", m.visible);
        o->setProperty ("hold", m.hold == HoldMode::Lowest ? "lowest" : "highest");
        meterObj->setProperty (meterSpecs()[(size_t) i].key, juce::var (o));
    }
    obj->setProperty ("meters", juce::var (meterObj));
    return juce::var (obj);
}

Profile Profile::fromVar (const juce::var& v)
{
    Profile p = defaults (v.getProperty ("name", "Untitled").toString());
    const auto meters = v.getProperty ("meters", {});

    for (int i = 0; i < NumMeters; ++i)
    {
        const auto o = meters.getProperty (meterSpecs()[(size_t) i].key, {});
        if (! o.isObject())
            continue;

        auto& m = p.meters[(size_t) i];
        m.min = (float) (double) o.getProperty ("min", m.min);
        m.max = (float) (double) o.getProperty ("max", m.max);
        m.visible = (bool) o.getProperty ("visible", true);
        m.hold = o.getProperty ("hold", "highest").toString() == "lowest" ? HoldMode::Lowest : HoldMode::Highest;
        if (m.min > m.max) std::swap (m.min, m.max);
    }
    return p;
}

bool Profile::sameTargetsAs (const Profile& o) const
{
    for (int i = 0; i < NumMeters; ++i)
    {
        const auto& a = meters[(size_t) i];
        const auto& b = o.meters[(size_t) i];
        if (std::abs (a.min - b.min) > 1e-4f || std::abs (a.max - b.max) > 1e-4f
            || a.visible != b.visible || a.hold != b.hold)
            return false;
    }
    return true;
}

//==============================================================================
static juce::Array<Profile> factoryProfiles()
{
    auto make = [] (const char* name, std::initializer_list<std::pair<float, float>> ranges)
    {
        Profile p = Profile::defaults (name);
        p.factory = true;
        int i = 0;
        for (auto r : ranges)
        {
            p.meters[(size_t) i].min = r.first;
            p.meters[(size_t) i].max = r.second;
            ++i;
        }
        return p;
    };

    juce::Array<Profile> list;
    //                                Peak            TruePeak        LUFS int        LUFS short      RMS             PSR           Mid
    list.add (make ("Mix Bus",         { { -12, -3 },  { -10, -2 },    { -24, -16 },   { -22, -12 },   { -26, -14 },   { 8, 16 },    { -30, -18 } }));
    list.add (make ("Streaming Master",{ { -6, -1 },   { -3, -1 },     { -15, -13 },   { -16, -10 },   { -20, -11 },   { 7, 12 },    { -26, -14 } }));
    list.add (make ("Loud Master",     { { -2, -0.3f },{ -1.5f, -0.5f },{ -10, -7 },   { -10, -5 },    { -13, -7 },    { 5, 9 },     { -20, -10 } }));
    list.add (make ("Broadcast EBU R128",{ { -12, -2 },{ -6, -1 },     { -24, -22 },   { -28, -18 },   { -30, -18 },   { 8, 18 },    { -34, -20 } }));
    return list;
}

ProfileStore::ProfileStore() { reload(); }

juce::File ProfileStore::storageFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Nova Meter")
               .getChildFile ("Profiles.json");
}

void ProfileStore::reload()
{
    profiles = factoryProfiles();

    const auto parsed = juce::JSON::parse (storageFile());
    if (auto* arr = parsed.getArray())
    {
        for (const auto& v : *arr)
        {
            auto p = Profile::fromVar (v);
            if (p.name.isNotEmpty() && find (p.name) == nullptr)
                profiles.add (p);
        }
    }
}

const Profile* ProfileStore::find (const juce::String& name) const
{
    for (const auto& p : profiles)
        if (p.name == name)
            return &p;
    return nullptr;
}

bool ProfileStore::saveUser (const Profile& toSave)
{
    reload(); // pick up profiles saved by other instances

    for (auto& p : profiles)
    {
        if (p.name == toSave.name)
        {
            if (p.factory)
                return false;
            p = toSave;
            p.factory = false;
            writeFile();
            return true;
        }
    }

    auto p = toSave;
    p.factory = false;
    profiles.add (p);
    writeFile();
    return true;
}

bool ProfileStore::removeUser (const juce::String& name)
{
    reload();
    for (int i = 0; i < profiles.size(); ++i)
    {
        if (profiles[i].name == name && ! profiles[i].factory)
        {
            profiles.remove (i);
            writeFile();
            return true;
        }
    }
    return false;
}

void ProfileStore::writeFile() const
{
    juce::Array<juce::var> arr;
    for (const auto& p : profiles)
        if (! p.factory)
            arr.add (p.toVar());

    auto f = storageFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (juce::var (arr)));
}

} // namespace tz
