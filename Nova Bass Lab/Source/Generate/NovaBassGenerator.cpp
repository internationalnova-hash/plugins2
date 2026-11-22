#include "NovaBassGenerator.h"

using namespace nova::bass;

namespace nova::basslab
{

const std::vector<NovaBassGeneratorProfile>& NovaBassGenerator::getProfiles()
{
    // Ranges derived from the Milestone-A hand-authored factory presets'
    // real values for each category (Rumble Low/Clean 808/Warm Bottom/
    // House Low for Clean Sub; Hard Knock/Slap 808/Punch 808 for Hard 808;
    // Reese Motion for Reese) - not arbitrary guesses.
    static const std::vector<NovaBassGeneratorProfile> profiles = {
        {
            "clean_sub", "Sub",
            /*sineTriBlend*/    { 0.0f, 0.2f },
            /*pitchEnvAmount*/  { 2.0f, 9.0f },
            /*pitchEnvDecay*/   { 0.04f, 0.12f },
            /*ampAttack*/       { 0.0005f, 0.02f },
            /*ampDecay*/        { 0.5f, 1.8f },
            /*ampSustain*/      { 0.55f, 0.8f },
            /*transientAmount*/ { 0.05f, 0.3f },
            /*filterCutoff*/    { 4000.0f, 8000.0f },
            /*filterResonance*/ { 0.5f, 0.9f },
            /*colorDrive*/      { 0.1f, 0.3f },
            { NovaColorMode::Clean, NovaColorMode::Warm },
        },
        {
            "hard_808", "Hard 808",
            /*sineTriBlend*/    { 0.0f, 0.1f },
            /*pitchEnvAmount*/  { 13.0f, 24.0f },
            /*pitchEnvDecay*/   { 0.05f, 0.28f },
            /*ampAttack*/       { 0.0003f, 0.001f },
            /*ampDecay*/        { 0.25f, 0.7f },
            /*ampSustain*/      { 0.3f, 0.55f },
            /*transientAmount*/ { 0.5f, 0.7f },
            /*filterCutoff*/    { 6000.0f, 12000.0f },
            /*filterResonance*/ { 0.6f, 1.2f },
            /*colorDrive*/      { 0.45f, 0.75f },
            { NovaColorMode::Gritty },
        },
        {
            "reese", "Reese",
            /*sineTriBlend*/    { 0.6f, 0.85f },
            /*pitchEnvAmount*/  { 2.0f, 6.0f },
            /*pitchEnvDecay*/   { 0.05f, 0.08f },
            /*ampAttack*/       { 0.01f, 0.03f },
            /*ampDecay*/        { 0.7f, 1.2f },
            /*ampSustain*/      { 0.65f, 0.8f },
            /*transientAmount*/ { 0.1f, 0.25f },
            /*filterCutoff*/    { 3000.0f, 7000.0f },
            /*filterResonance*/ { 1.2f, 2.2f },
            /*colorDrive*/      { 0.3f, 0.5f },
            { NovaColorMode::Warm, NovaColorMode::Gritty },
        },
    };
    return profiles;
}

const NovaBassGeneratorProfile* NovaBassGenerator::findProfile (const juce::String& profileId)
{
    for (auto& p : getProfiles())
        if (p.id == profileId)
            return &p;
    return nullptr;
}

NovaBassPreset NovaBassGenerator::generate (const juce::String& profileId, juce::Random& rng)
{
    NovaBassPreset preset;

    const auto* profile = findProfile (profileId);
    if (profile == nullptr)
        return preset; // caller checks findProfile() first; empty default is a safe non-crashing fallback

    preset.meta.name = "Generated";
    preset.meta.category = profile->category;
    preset.meta.tags = { "generated", profile->id };
    preset.sourceType = NovaBassSourceType::Synth;

    auto& v = preset.voiceParams;
    v.sineTriBlend = profile->sineTriBlend.sample (rng);
    v.pitchEnvAmountSemitones = profile->pitchEnvAmount.sample (rng);
    v.pitchEnvDecaySeconds = profile->pitchEnvDecay.sample (rng);
    v.ampAttackSeconds = profile->ampAttack.sample (rng);
    v.ampDecaySeconds = profile->ampDecay.sample (rng);
    v.ampSustain = profile->ampSustain.sample (rng);
    v.transientAmount = profile->transientAmount.sample (rng);
    v.filterCutoffHz = profile->filterCutoff.sample (rng);
    v.filterResonance = profile->filterResonance.sample (rng);

    preset.colorDrive = profile->colorDrive.sample (rng);
    preset.colorMode = profile->allowedColorModes[(size_t) rng.nextInt ((int) profile->allowedColorModes.size())];

    // macros intentionally left empty - see NovaBassLabAudioEngine::loadSound()

    return preset;
}

} // namespace nova::basslab
