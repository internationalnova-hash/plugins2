#include "RackPresets.h"

// Factory racks: whole Nova chains, dialled in.
//
// Like Nova AI Vocal's presets, vocal racks are named by vibe/wordplay and the rack
// never shows an artist's real name. The "inspired by" notes are for us only.
//
// Each unit lists parameter values as the plugin's own text ("-3", "1/8D", "Vintage"),
// matched by parameter name. Anything not listed stays at the plugin's default.
// Autotune's Key and Scale are never set: the key belongs to the song, not the preset.
// Check every name/value against the installed plugins with:
//   NovaRackParamDump --verify-factory
//
// Typical vocal order: Clean → Tune → Desk EQ → Compressor → Silk (de-ess) → Color → Delay → Space.

namespace nr
{
namespace
{
using Params = std::vector<std::pair<const char*, const char*>>;

struct Unit
{
    const char* plugin;
    Params params;
    float mix = 1.0f;
};

RackData rack (const char* name, const char* group, const char* note, std::initializer_list<Unit> units,
               float inDb = 0.0f, float outDb = 0.0f)
{
    RackData r;
    r.name = juce::String::fromUTF8 (name);   // group names use "•"
    r.group = juce::String::fromUTF8 (group);
    r.note = juce::String::fromUTF8 (note);
    r.factory = true;
    r.inGainDb = inDb;
    r.outGainDb = outDb;
    for (const auto& u : units)
    {
        SlotData s;
        s.plugin = u.plugin;
        s.mix = u.mix;
        for (const auto& [k, v] : u.params)
            s.params.set (juce::String::fromUTF8 (k), juce::String::fromUTF8 (v));
        r.slots.add (s);
    }
    return r;
}

// ── Building blocks shared by several racks ───────────────────────────────
// Desk EQ: HPF choices are Off/20/50/100/200/400.
Unit eq (const char* hpf, const char* mudHz, const char* mudDb, const char* presHz, const char* presDb,
         const char* airDb, const char* drive = "0")
{
    return { "Nova Desk EQ", { { "HPF Frequency", hpf }, { "Low Gain", "0" },
                               { "Low Mid Frequency", mudHz }, { "Low Mid Gain", mudDb }, { "Low Mid Q", "1.2" },
                               { "High Mid Frequency", presHz }, { "High Mid Gain", presDb }, { "High Mid Q", "0.9" },
                               { "High Frequency", "12000" }, { "High Gain", airDb }, { "High Type", "Shelf" },
                               { "Drive", drive } } };
}

Unit tune (const char* speed, const char* amount = "100")
{
    return { "Nova Autotune", { { "Retune Speed", speed }, { "Amount", amount }, { "Formant Preserve", "On" } } };
}

Unit deEss (const char* smooth, const char* focus = "60", const char* air = "75")
{
    return { "Nova Silk", { { "Smooth", smooth }, { "Focus", focus }, { "Air Preserve", air }, { "Body", "62" }, { "Magic", "On" } } };
}

Unit level (const char* amount, const char* mode = "SMOOTH", const char* outDb = "0")
{
    return { "Nova Level", { { "Compression", amount }, { "Mode", mode }, { "Output", outDb }, { "Magic", "Off" } } };
}

Unit delay (const char* note, const char* feedback, const char* mix, const char* lpHz, const char* model = "Modern Analog",
            const char* pingPong = "Off")
{
    return { "Nova Delay", { { "Sync", "On" }, { "Delay Time Sync", note }, { "Feedback", feedback }, { "Mix", mix },
                             { "HP Filter", "250" }, { "LP Filter", lpHz }, { "Ducking", "45" },
                             { "Delay Model", model }, { "Ping Pong", pingPong }, { "Wow Flutter", "12" }, { "Saturation", "20" } } };
}

Unit space (const char* mode, const char* decay, const char* mix, const char* preDelay = "25", const char* width = "60",
            const char* damping = "45")
{
    // Space's Width runs 0-10 internally (shown as %), so it's set as a normalised value.
    static std::vector<juce::String> keep; // owns the "=0.60" strings for the static table
    keep.push_back ("=" + juce::String (juce::String (width).getFloatValue() / 100.0f, 2));
    return { "Nova Space", { { "Nova Mode", mode }, { "Decay", decay }, { "Mix", mix }, { "Pre-Delay", preDelay },
                             { "Width", keep.back().toRawUTF8() }, { "Damping", damping } } };
}

Unit clean (const char* amount = "45")
{
    return { "Nova Clean", { { "Mode", "Vocal" }, { "Clean", amount }, { "Preserve", "80" }, { "Mix", "100" } } };
}
} // namespace

const juce::Array<RackData>& factoryRacks()
{
    static const juce::Array<RackData> racks {
        // ── Starters ─────────────────────────────────────────────────────
        rack ("Vocal Starter", "VOCAL • STARTER", "Clean lead chain: gentle tune, EQ, smooth comp, de-ess, short room",
              { tune ("35", "80"), eq ("100", "300", "-2", "3000", "2", "3"), level ("4"), deEss ("40"),
                space ("Studio", "1.4", "12") }),
        rack ("Rap Starter", "VOCAL • STARTER", "Dry, upfront rap vocal: tight comp, bite, no tune",
              { eq ("100", "250", "-3", "3500", "3", "2"), level ("6", "PUNCH"), deEss ("45"),
                { "Nova Punch", { { "Punch", "45" }, { "Body", "40" }, { "Focus", "Mid" }, { "Mix", "60" } } } }),
        rack ("Podcast Voice", "VOCAL • STARTER", "Spoken word: noise clean-up, warmth, steady level",
              { clean ("60"), eq ("100", "350", "-2", "2500", "1.5", "1"), level ("5", "SMOOTH"), deEss ("35") }),

        // ── Latin ────────────────────────────────────────────────────────
        // Bad Bunny: deep baritone kept full, mid-forward, short slap, medium plate, fast tune
        rack ("Conejo Malo", "VOCAL • LATIN", "Deep baritone kept full, mid-forward bite, quick tune, short slap and plate",
              { tune ("12"), eq ("100", "280", "-2", "2800", "4", "3"), level ("5", "PUNCH"), deEss ("50"),
                { "Nova Heat", { { "Drive", "3" }, { "Heat", "3" }, { "Mix", "22" }, { "Mode", "Medium" } } },
                delay ("1/8", "18", "14", "4500", "Vintage Tape"), space ("Studio", "1.6", "14", "20") }),
        // Ozuna / Rauw: bright airy tenor, heavy tune, lush echoes, big bright verb
        rack ("Falsete de Oro", "VOCAL • LATIN", "Bright, airy high tenor: hard tune, sparkling top, lush dotted echoes",
              { tune ("6"), eq ("200", "320", "-3", "4500", "3", "6"), { "Nova Royal 670", { { "Input", "2" }, { "Time Constant", "0.2 sec" }, { "Auto Gain", "On" } } },
                deEss ("55", "70", "85"), { "Nova Aura", { { "Mid Aura", "35" }, { "High Aura", "70" }, { "Mix", "100" } } },
                delay ("1/8D", "30", "18", "6000", "Modern Analog", "On"), space ("Dream", "2.8", "20", "35", "80") }),
        // Karol G: glossy bright female top, smooth tune
        rack ("Bichota Gloss", "VOCAL • LATIN", "Glossy pop-reggaeton lead: smooth tune, bright top, dotted-8th echoes",
              { tune ("15"), eq ("100", "300", "-2", "5000", "2.5", "5"), level ("4"), deEss ("55", "70", "80"),
                { "Nova Aura", { { "Mid Aura", "40" }, { "High Aura", "60" } } },
                delay ("1/8D", "25", "14", "6000"), space ("Studio", "1.8", "15", "30", "65") }),
        // Daddy Yankee era: aggressive mids, hard comp, dry
        rack ("Perreo Viejo", "VOCAL • LATIN", "2000s reggaeton: aggressive mids, hard comp, 16th slap, nearly dry",
              { eq ("200", "300", "-3", "2500", "5", "2", "3"), level ("7", "PUNCH"), deEss ("45"),
                { "Nova Heat", { { "Drive", "5" }, { "Heat", "4" }, { "Mix", "30" }, { "Mode", "Hard" } } },
                delay ("1/16", "10", "10", "5000", "Vintage Tape") }),
        // Peso Pluma / corridos: raw mid-forward, slapback, almost untuned
        rack ("Corrido Crudo", "VOCAL • LATIN", "Corridos tumbados: raw nasal mids, slapback, barely tuned",
              { tune ("60", "40"), eq ("100", "250", "-1", "1800", "4", "1"), level ("5"), deEss ("40"),
                { "Nova Tape", { { "Tape", "55" }, { "Tape Type", "Vintage" }, { "Mix", "45" } } },
                delay ("1/16", "8", "16", "4000", "Vintage Tape") }),

        // ── Hip-hop ──────────────────────────────────────────────────────
        // Kendrick: dry upfront crisp delivery, punchy
        rack ("Section Eighty", "VOCAL • HIP-HOP", "Dry, upfront and crisp: punchy comp, bright bite, almost no space",
              { eq ("100", "280", "-3", "3500", "4", "3"), level ("6", "PUNCH"), deEss ("50"),
                { "Nova Punch", { { "Punch", "50" }, { "Body", "45" }, { "Focus", "Mid" }, { "Mix", "55" } } },
                space ("Studio", "0.9", "6", "10", "40") }),
        // J. Cole: warm, dry, mid-heavy, soulful
        rack ("Dreamville Warmth", "VOCAL • HIP-HOP", "Warm, soulful storyteller: mid-heavy, opto-smooth, tape glue",
              { eq ("100", "220", "-1", "2500", "2", "1.5"), { "Nova Velvet", { { "Threshold", "-24" }, { "Ratio", "3" }, { "Analog Mode", "Tube" }, { "Tube Drive", "35" } } },
                deEss ("35"), { "Nova Tape", { { "Tape", "60" }, { "Tape Type", "Vintage" }, { "IPS", "15" }, { "Mix", "50" } } },
                space ("Vintage", "1.2", "8") }),
        // Drake: soft, intimate, dark verb tail
        rack ("Six Side Mood", "VOCAL • HIP-HOP", "Soft, close and moody: gentle tune, rounded top, dark wide tail",
              { tune ("30", "70"), eq ("100", "300", "-2", "2200", "1.5", "1"), { "Nova Royal 670", { { "Input", "1" }, { "Time Constant", "1.2 sec" }, { "Auto Gain", "On" } } },
                deEss ("45"), delay ("1/4", "22", "10", "3200", "Warm Bucket"), space ("Dream", "3.2", "18", "40", "85", "70") }),
        // Old-school boom bap
        rack ("Boom Bap Booth", "VOCAL • HIP-HOP", "90s boom-bap: hard comp, gritty tape, tight room",
              { eq ("200", "300", "-2", "3000", "3", "0", "4"), { "Nova Royal 670", { { "Input", "5" }, { "Time Constant", "0.2 sec" }, { "Tube Saturation", "55" } } },
                deEss ("40"), { "Nova Tape", { { "Tape", "75" }, { "Tape Type", "Hot" }, { "IPS", "7.5" }, { "Mix", "60" } } },
                space ("Studio", "0.7", "7", "8", "30") }),

        // ── Trap ─────────────────────────────────────────────────────────
        // Future / melodic trap: hard tune, dark echoes
        rack ("Toxic Melody", "VOCAL • TRAP", "Melodic trap: hard robotic tune, saturated, dark tape echoes",
              { tune ("0"), eq ("200", "300", "-3", "3000", "3", "3"), level ("6", "PUNCH"), deEss ("55"),
                { "Nova Heat", { { "Drive", "5" }, { "Heat", "5" }, { "Mix", "28" } } },
                delay ("1/4", "35", "16", "3500", "Vintage Tape", "On"), space ("Dream", "2.6", "15", "30", "80") }),
        // Travis: wet, wide, psychedelic
        rack ("Astro Haze", "VOCAL • TRAP", "Wet and psychedelic: hard tune, doubled, ping-pong echoes, huge dark space",
              { tune ("2"), eq ("200", "320", "-3", "3200", "2", "4"), level ("6"), deEss ("50"),
                { "Nova Doubler", { { "Width", "75" }, { "Tightness", "35" }, { "Mix", "60" } } },
                delay ("1/8D", "45", "22", "4000", "Echo Chamber", "On"), space ("Dream", "4.5", "26", "45", "100", "60") }),
        // Young Thug / Gunna: airy, bright, playful tune
        rack ("Slime Silk", "VOCAL • TRAP", "Airy and playful: fast tune, bright silky top, quarter-note echoes",
              { tune ("8"), eq ("100", "300", "-2", "4000", "3", "5"), level ("5"), deEss ("55", "70", "85"),
                { "Nova Aura", { { "Mid Aura", "30" }, { "High Aura", "65" } } },
                delay ("1/4", "28", "14", "5000"), space ("Studio", "2.0", "14", "25", "70") }),

        // ── Drill ────────────────────────────────────────────────────────
        rack ("Bando Cold", "VOCAL • DRILL", "UK/NY drill: dark and aggressive, hard comp, gritty, sliding echo",
              { tune ("25", "60"), eq ("200", "300", "-3", "2400", "4", "1", "3"), level ("7", "PUNCH"), deEss ("50"),
                { "Nova Heat", { { "Drive", "6" }, { "Heat", "4" }, { "Mix", "30" }, { "Mode", "Hard" } } },
                delay ("1/8", "25", "12", "3000", "Vintage Tape"), space ("Vintage", "1.4", "9") }),

        // ── R&B ──────────────────────────────────────────────────────────
        // The Weeknd: glossy 80s, big verb
        rack ("After Hours Gloss", "VOCAL • R&B", "Glossy, cinematic R&B lead: silky top, 80s echoes, big bright hall",
              { tune ("18", "85"), eq ("100", "300", "-2", "3500", "2", "5"), { "Nova Royal 670", { { "Input", "2" }, { "Auto Gain", "On" } } },
                deEss ("55", "70", "85"), { "Nova Aura", { { "Mid Aura", "40" }, { "High Aura", "70" } } },
                delay ("1/4", "35", "16", "5500", "Modern Analog", "On"), space ("Arena", "3.4", "22", "40", "90") }),
        // SZA: breathy, intimate, warm
        rack ("Velvet Diary", "VOCAL • R&B", "Breathy and intimate: soft opto comp, warm body, dreamy tail",
              { tune ("40", "70"), eq ("100", "250", "-1.5", "3000", "1.5", "4"), { "Nova Velvet", { { "Threshold", "-22" }, { "Ratio", "3" }, { "Analog Mode", "Tube" } } },
                deEss ("45", "60", "80"), delay ("1/8D", "25", "10", "4500", "Warm Bucket"), space ("Dream", "2.6", "18", "35", "80", "55") }),
        // Brent / acoustic R&B: natural, warm, untuned
        rack ("Sunday Soul", "VOCAL • R&B", "Natural soul vocal: no tune, warm tape, plate-style space",
              { eq ("100", "250", "-1", "2800", "1.5", "2.5"), level ("4"), deEss ("35"),
                { "Nova Tape", { { "Tape", "55" }, { "Tape Type", "Studio" }, { "Mix", "40" } } },
                space ("Vintage", "2.0", "15", "30", "60") }),

        // ── Pop ──────────────────────────────────────────────────────────
        rack ("Radio Polish", "VOCAL • POP", "Modern pop lead: transparent tune, bright and forward, stacked comp",
              { tune ("20", "90"), eq ("100", "300", "-2", "3500", "2.5", "5"), level ("5"), deEss ("55", "65", "85"),
                { "Nova Glow", { { "Peak Reduction", "30" }, { "Analog Mode", "Clean" }, { "Mix", "60" } } },
                { "Nova Aura", { { "Mid Aura", "35" }, { "High Aura", "55" } } },
                delay ("1/8D", "20", "10", "6000"), space ("Studio", "1.8", "14", "30", "60") }),
        rack ("Bedroom Pop", "VOCAL • POP", "Soft, lo-fi bedroom vocal: no tune, tape-worn, wide dreamy space",
              { eq ("200", "300", "-2", "2500", "1", "1.5"), level ("4"), deEss ("40"),
                { "Nova Tape", { { "Tape", "70" }, { "Tape Type", "Vintage" }, { "IPS", "7.5" }, { "Wow / Flutter", "5" }, { "Mix", "55" } } },
                { "Nova Doubler", { { "Width", "60" }, { "Mix", "40" } } },
                space ("Dream", "3.0", "22", "30", "90", "60") }),

        // Nova Mid: midrange weight into a soft-clip ceiling, for a heavier, denser rap lead
        rack ("Heavyweight Bars", "VOCAL • HIP-HOP", "Heavy, dense rap lead: midrange weight pushed into a soft-clip ceiling",
              { eq ("100", "300", "-2", "3500", "2", "2"), level ("6", "PUNCH"), deEss ("40"),
                { "Nova Mid", { { "Push", "7" }, { "Ceiling", "-4" } } },
                delay ("1/8", "10", "8", "4000"), space ("Studio", "1.0", "8", "20", "60") }),

        // ── Afro ─────────────────────────────────────────────────────────
        // Burna / Wizkid: warm, smooth, airy, rhythmic echoes
        rack ("Lagos Glow", "VOCAL • AFRO", "Afrobeats lead: smooth tune, warm body, airy top, rhythmic dotted echoes",
              { tune ("15", "90"), eq ("100", "250", "-1.5", "3000", "2", "4"), level ("4"), deEss ("50"),
                { "Nova Glow", { { "Peak Reduction", "25" }, { "Glow", "55" }, { "Analog Mode", "Vintage" } } },
                delay ("1/8D", "28", "14", "5000", "Warm Bucket"), space ("Studio", "1.8", "15", "25", "70") }),

        // ── Backgrounds & FX ─────────────────────────────────────────────
        rack ("Stack Wide", "VOCAL • BACKGROUND", "Background stacks: darker, wider, tucked behind the lead",
              { tune ("10"), eq ("200", "300", "-3", "2500", "-1", "3"), level ("6"), deEss ("60"),
                { "Nova Doubler", { { "Width", "85" }, { "Tightness", "50" }, { "Mix", "70" } } },
                space ("Arena", "2.4", "24", "30", "100") }),
        rack ("Ad-Lib Throw", "VOCAL • BACKGROUND", "Ad-libs: hard tune, mid-heavy, long ping-pong throws",
              { tune ("0"), eq ("400", "300", "-2", "2000", "4", "0"), level ("6", "PUNCH"),
                delay ("1/4D", "40", "30", "4000", "Echo Chamber", "On"), space ("Dream", "3.0", "25", "40", "100") }),
        rack ("Telephone Hook", "VOCAL • FX", "Filtered 'phone' hook: band-limited, crunchy, short echo",
              { { "Nova Desk EQ", { { "HPF Frequency", "400" }, { "LPF Frequency", "5k" }, { "High Mid Frequency", "1800" }, { "High Mid Gain", "6" }, { "Drive", "6" } } },
                { "Nova Heat", { { "Drive", "7" }, { "Heat", "6" }, { "Mix", "45" }, { "Mode", "Hard" } } },
                level ("7", "LIMIT"), delay ("1/8", "20", "15", "3500", "Vintage Tape") }),

        // ── Analog: only the hardware-modelled Nova units ───────────────
        // 7even (1073-style pre/EQ), Tone (Pultec-style), Desk EQ (E/G console), Royal 670 (vari-mu),
        // Velvet (opto), Glow, Glue (bus comp), Console, Tape, Heat, and the Vintage Tape delay model.
        // Neve → 670 → Pultec → tape: the classic all-hardware vocal path
        rack ("Golden Era Chain", "ANALOG • VOCAL", "1073-style pre and EQ, vari-mu glue, Pultec air, then tape",
              { { "Nova 7even", { { "Character", "Classic" }, { "Preamp Gain", "4" }, { "HPF", "80" },
                                  { "Mid Frequency", "3200" }, { "Mid Gain", "3" }, { "High Shelf (12k)", "2" } } },
                { "Nova Royal 670", { { "Input", "3" }, { "Time Constant", "0.6 sec" }, { "Tube Saturation", "45" }, { "Auto Gain", "On" } } },
                { "Nova Tone", { { "High Boost Frequency", "12k" }, { "High Boost", "3" }, { "Low Frequency", "100" }, { "Low Attenuation", "2" } } },
                { "Nova Tape", { { "Tape", "55" }, { "Tape Type", "Studio" }, { "IPS", "15" }, { "Mix", "45" } } } }),
        // LA-2A-style opto lead
        rack ("Opto Velvet Lead", "ANALOG • VOCAL", "Smooth opto lead: 1073 colour, slow tube opto, silky Pultec top",
              { { "Nova 7even", { { "Character", "Classic" }, { "Preamp Gain", "3" }, { "HPF", "80" },
                                  { "Low Frequency", "220" }, { "Low Gain", "-2" }, { "High Shelf (12k)", "3" } } },
                { "Nova Velvet", { { "Threshold", "-24" }, { "Ratio", "3" }, { "Time Select", "Fixed" }, { "Analog Mode", "Tube" }, { "Tube Drive", "30" } } },
                { "Nova Tone", { { "High Boost Frequency", "10k" }, { "High Boost", "4" }, { "Bandwidth", "6" }, { "High Attenuation Frequency", "20k" }, { "High Attenuation", "1" } } },
                { "Nova Heat", { { "Drive", "2" }, { "Heat", "2" }, { "Mix", "20" }, { "Mode", "Soft" } } } }),
        // Big-desk vocal: console strip, opto, tape slap
        rack ("Desk Session", "ANALOG • VOCAL", "Console channel strip, opto levelling, tape and a tape slap",
              { { "Nova Console", { { "Console Mode", "British" }, { "Drive", "40" }, { "HPF", "90" },
                                    { "Low Mid", "-2" }, { "Low Mid Freq", "320" }, { "High Mid", "2" }, { "High Mid Freq", "3200" },
                                    { "Air", "2" }, { "Threshold", "-18" }, { "Ratio", "3" }, { "Comp Mix", "100" } } },
                { "Nova Velvet", { { "Threshold", "-20" }, { "Ratio", "2" }, { "Analog Mode", "Vintage" } } },
                { "Nova Tape", { { "Tape", "60" }, { "Tape Type", "Vintage" }, { "IPS", "15" }, { "Mix", "50" } } },
                delay ("1/16", "12", "10", "4000", "Vintage Tape") }),
        // 70s soul: warm, rounded, worn tape
        rack ("Seventies Soul", "ANALOG • VOCAL", "Warm and rounded 70s soul: hot pre, opto, 7.5 ips tape, tape echo",
              { { "Nova 7even", { { "Character", "Hot" }, { "Preamp Gain", "5" }, { "HPF", "80" }, { "Mid Frequency", "1600" }, { "Mid Gain", "2" } } },
                { "Nova Velvet", { { "Threshold", "-22" }, { "Ratio", "4" }, { "Analog Mode", "Vintage" }, { "Tube Drive", "40" } } },
                { "Nova Tape", { { "Tape", "75" }, { "Tape Type", "Vintage" }, { "IPS", "7.5" }, { "Wow / Flutter", "3.5" }, { "Mix", "60" } } },
                delay ("1/8", "20", "12", "3500", "Vintage Tape") }),
        // Rock: driven pre, fast limiting, grit
        rack ("Grit and Glory", "ANALOG • VOCAL", "Rock lead: driven pre, fast hard levelling, saturated grit",
              { { "Nova 7even", { { "Character", "Hot" }, { "Preamp Gain", "6" }, { "HPF", "160" }, { "Mid Frequency", "1600" }, { "Mid Gain", "4" } } },
                { "Nova Glow", { { "Compress/Limit", "Limit" }, { "Peak Reduction", "45" }, { "Analog Mode", "Hot" } } },
                { "Nova Heat", { { "Drive", "5" }, { "Heat", "5" }, { "Mix", "35" }, { "Mode", "Hard" } } },
                { "Nova Tape", { { "Tape", "70" }, { "Tape Type", "Hot" }, { "Mix", "45" } } } }),
        // Vari-mu double-stage for rap
        rack ("Tube Bars", "ANALOG • VOCAL", "Rap through tubes: E-console EQ, vari-mu, opto, light tape",
              { { "Nova Desk EQ", { { "EQ Type", "E" }, { "HPF Frequency", "100" }, { "Low Mid Frequency", "280" }, { "Low Mid Gain", "-3" },
                                    { "High Mid Frequency", "3500" }, { "High Mid Gain", "3" }, { "Drive", "3" } } },
                { "Nova Royal 670", { { "Input", "4" }, { "Time Constant", "0.2 sec" }, { "Auto Gain", "On" } } },
                { "Nova Velvet", { { "Threshold", "-18" }, { "Ratio", "2" } } },
                { "Nova Tape", { { "Tape", "50" }, { "Tape Type", "Studio" }, { "Mix", "35" } } } }),

        rack ("Analog Mix Bus", "ANALOG • BUS", "Console summing, bus glue, Pultec lift, tape: the all-hardware 2-bus",
              { { "Nova Console", { { "Console Mode", "British" }, { "Quality", "Master" }, { "Drive", "30" }, { "Compressor Enabled", "Off" }, { "Gate Enabled", "Off" } } },
                { "Nova Glue", { { "Threshold", "-14" }, { "Ratio", "2:1" }, { "Release", "Auto" }, { "Auto Gain", "On" } } },
                { "Nova Tone", { { "Low Frequency", "60" }, { "Low Boost", "2" }, { "High Boost Frequency", "12k" }, { "High Boost", "2" } } },
                { "Nova Tape", { { "Tape", "45" }, { "Tape Type", "Studio" }, { "IPS", "30" }, { "Mix", "40" } } } }),
        rack ("Analog Master", "ANALOG • BUS", "Hardware-style master: Pultec tone, vari-mu in M/S, bus glue, tape",
              { { "Nova Tone", { { "Mode Preset", "Master" }, { "High Boost Frequency", "16k" }, { "High Boost", "1.5" } } },
                { "Nova Royal 670", { { "Stereo Mode", "M/S" }, { "Input", "1" }, { "Time Constant", "1.2 sec" }, { "Auto Gain", "On" } } },
                { "Nova Glue", { { "Threshold", "-12" }, { "Ratio", "2:1" }, { "Release", "Auto" }, { "Auto Gain", "On" } } },
                { "Nova Tape", { { "Tape", "35" }, { "Tape Type", "Studio" }, { "IPS", "30" }, { "Mix", "30" } } },
                { "Nova Meter", {} } }),
        rack ("Drum Smash", "ANALOG • BUS", "Drum bus: G-console EQ, smashed bus comp blended in, hot tape",
              { { "Nova Desk EQ", { { "EQ Type", "G" }, { "Low Frequency", "80" }, { "Low Gain", "3" }, { "High Frequency", "8000" }, { "High Gain", "2" }, { "Drive", "4" } } },
                { "Nova Glue", { { "Threshold", "-22" }, { "Ratio", "10:1" }, { "Release", "Auto" }, { "Mix", "45" }, { "Auto Gain", "On" } } },
                { "Nova Heat", { { "Drive", "4" }, { "Mix", "30" } } },
                { "Nova Tape", { { "Tape", "70" }, { "Tape Type", "Hot" }, { "Mix", "50" } } } }),

        // ── Drums & 808 ──────────────────────────────────────────────────
        rack ("808 Weight", "DRUMS & 808", "808s that read on small speakers: warm drive, then midrange weight into the ceiling",
              { { "Nova Heat", { { "Drive", "3" }, { "Heat", "3" }, { "Mix", "25" }, { "Mode", "Soft" } } },
                { "Nova Mid", { { "Push", "10" }, { "Ceiling", "-4" } } } }),
        rack ("Drum Bus Weight", "DRUMS & 808", "Drum bus: punch, bus glue, then midrange density without higher peaks",
              { { "Nova Punch", { { "Punch", "40" }, { "Body", "35" }, { "Mix", "60" } } },
                { "Nova Glue", { { "Threshold", "-18" }, { "Ratio", "4:1" }, { "Release", "Auto" }, { "Mix", "60" }, { "Auto Gain", "On" } } },
                { "Nova Mid", { { "Push", "6.5" }, { "Ceiling", "-4" } } } }),
        rack ("Loop Weight", "DRUMS & 808", "Samples and loops: tape warmth and midrange weight so they sit forward",
              { { "Nova Tape", { { "Tape", "45" }, { "Tape Type", "Studio" }, { "Mix", "40" } } },
                { "Nova Mid", { { "Push", "5" }, { "Ceiling", "-2" } } } }),

        // ── 2-Track: finishing a stereo beat with no stems ───────────────
        rack ("Beat Prep for Vocals", "2-TRACK", "Put this on the beat: a pocket for the vocal, centred low end, wider sides, soft-clipped",
              { { "Nova 2-Track", { { "Width", "110" }, { "Vox Carve", "50" }, { "Low End", "35" }, { "Punch", "20" },
                                    { "Instruments", "20" }, { "Energy", "15" }, { "Soft Clipper", "On" }, { "Ceiling", "-1" } } } }),
        rack ("Beat Revive", "2-TRACK", "Flat or over-processed beat: transients back, wider instruments, light tape",
              { { "Nova 2-Track", { { "Width", "115" }, { "Punch", "35" }, { "Energy", "55" }, { "Low End", "20" },
                                    { "Instruments", "30" }, { "Soft Clipper", "On" }, { "Ceiling", "-0.5" } } },
                { "Nova Tape", { { "Tape", "35" }, { "Tape Type", "Studio" }, { "Mix", "30" } } } }),
        rack ("Loud 2-Track", "2-TRACK", "Finished stereo mix, louder: tone, 2-Track shaping into the clipper, then the meter",
              { { "Nova Tone", { { "Mode Preset", "Master" } } },
                { "Nova 2-Track", { { "Input", "3" }, { "Width", "108" }, { "Punch", "25" }, { "Low End", "40" }, { "Instruments", "20" },
                                    { "Energy", "20" }, { "Soft Clipper", "On" }, { "Ceiling", "-0.3" } } },
                { "Nova Meter", {} } }),

        // ── Mix bus & master ─────────────────────────────────────────────
        rack ("Mix Bus Glue", "MIX BUS", "Two-bus glue: gentle bus comp, tape weight, nothing hyped",
              { { "Nova Glue", {} }, { "Nova Tape", { { "Tape", "40" }, { "Tape Type", "Studio" }, { "Mix", "35" } } } }),
        rack ("Mix Bus Weight", "MIX BUS", "Two-bus glue with a touch of midrange weight held under the ceiling",
              { { "Nova Glue", {} }, { "Nova Mid", { { "Push", "3" }, { "Ceiling", "-1" } } },
                { "Nova Tape", { { "Tape", "35" }, { "Tape Type", "Studio" }, { "Mix", "30" } } } }),
        rack ("Master Chain", "MASTER", "Mastering: tone, glue, loudness, and a meter last to check the targets",
              { { "Nova Tone", { { "Mode Preset", "Master" } } }, { "Nova Glue", {} }, { "Nova Master", {} }, { "Nova Meter", {} } }),
    };
    return racks;
}

} // namespace nr
