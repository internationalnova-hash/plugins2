#include "MixPlanner.h"

#include <algorithm>
#include <cmath>

namespace NovaStudio::MixAI
{
namespace
{
    // Where each role should sit (dBFS RMS of its active parts, after the fader)
    float targetLevel (Role r)
    {
        switch (r)
        {
            case Role::Lead: return -17.f;   case Role::Double: return -25.f;  case Role::Harmony: return -26.f;
            case Role::Adlib: return -24.f;  case Role::Kick: return -19.f;    case Role::Snare: return -21.f;
            case Role::Clap: return -23.f;   case Role::HiHat: return -29.f;   case Role::Perc: return -28.f;
            case Role::Bass: return -20.f;   case Role::Keys: return -26.f;    case Role::Guitar: return -25.f;
            case Role::Pad: return -29.f;    case Role::Beat: return -18.f;    case Role::Fx: return -27.f;
            default: return -26.f;
        }
    }

    struct StyleMap { const char* style; const char* vibe; const char* master; float verbDecay; const char* loudness; };
    const StyleMap kStyles[] = {
        { "Nova Balance", "Raw Natural",      "Clean",  2.0f, "Streaming" },
        { "Trap",         "Melodic Trap",     "Punchy", 1.8f, "Balanced" },
        { "Drill",        "Drill Dark",       "Punchy", 1.6f, "Balanced" },
        { "R&B",          "R&B Smooth",       "Warm",   2.6f, "Streaming" },
        { "Reggaeton",    "Reggaeton Bright", "Punchy", 1.8f, "Balanced" },
        { "Afrobeats",    "Pop Polished",     "Open",   2.0f, "Streaming" },
        { "Pop",          "Pop Polished",     "Clean",  2.2f, "Streaming" },
        { "House",        "Pop Polished",     "Loud",   2.4f, "Balanced" },
    };
    const StyleMap& styleFor (const juce::String& s)
    {
        for (auto& m : kStyles) if (s == m.style) return m;
        return kStyles[0];
    }

    juce::String keyText (const SessionAnalysis& a)
    {
        static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (N[a.keyTonic]) + (a.keyMinor ? " Minor" : " Major");
    }

    juce::String num (float v, int dp = 1) { return juce::String (v, dp); }

    // Mirror's target lives in its saved state (not a parameter): build that state
    juce::MemoryBlock mirrorState (const Options& o)
    {
        juce::XmlElement x ("NovaMirror");
        x.setAttribute ("v", 1);
        if (o.reference.existsAsFile())
        {
            x.setAttribute ("kind", "reference");
            x.setAttribute ("name", o.reference.getFileNameWithoutExtension());
            x.setAttribute ("refPath", o.reference.getFullPathName());
        }
        else
        {
            x.setAttribute ("kind", "nova");
            x.setAttribute ("name", o.style);
        }
        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (x, mb);
        return mb;
    }
}

juce::StringArray styleNames()
{
    juce::StringArray s;
    for (auto& m : kStyles) s.add (m.style);
    return s;
}

int Plan::changeCount() const
{
    int n = (int) master.size();
    for (auto& b : buses) n += 1 + (int) b.chain.size() + (int) b.sends.size() + (int) b.members.size();
    for (auto& t : tracks) if (t.change) n += 2 + (int) t.chain.size() + (int) t.sends.size() + (int) t.eq.size();
    return n;
}

Plan makePlan (const SessionAnalysis& a, const std::vector<Role>& roles, const Session& s, const Options& o)
{
    Plan p;
    const auto& st = styleFor (o.style);
    const bool light = o.intensity == 0, full = o.intensity == 2;

    // buses already taken by aux tracks in the session
    int used = 0;
    for (int i = 0; i < s.getNumTracks(); ++i) if (s.getTrack (i).type == TrackType::Aux) ++used;
    p.busesAvailable = std::max (0, 8 - used);

    // group tracks by role
    std::vector<int> lead, bgv, adlib, drums, bass, music, beat;
    for (size_t i = 0; i < a.tracks.size(); ++i)
    {
        const Role r = i < roles.size() ? roles[i] : a.tracks[i].role;
        const int ti = a.tracks[i].trackIndex;
        switch (r)
        {
            case Role::Lead: lead.push_back (ti); break;
            case Role::Double: case Role::Harmony: bgv.push_back (ti); break;
            case Role::Adlib: adlib.push_back (ti); break;
            case Role::Bass: bass.push_back (ti); break;
            case Role::Beat: beat.push_back (ti); break;
            default:
                if (familyOf (r) == Family::Drums) drums.push_back (ti);
                else if (familyOf (r) == Family::Music) music.push_back (ti);
        }
    }
    const bool vocals = ! lead.empty() || ! bgv.empty() || ! adlib.empty();

    // ── decide which buses we can afford (8 in total, shared with sends) ─────
    struct Want { juce::String name; bool ok; };
    std::vector<Want> wants = {
        { "Vocal Verb",  o.route && vocals },
        { "Vocal Delay", o.route && vocals && ! light },
        { "BG Vocals",   o.route && bgv.size() >= 2 },
        { "Drums",       o.route && drums.size() >= 2 },
        { "Lead Vocal",  o.route && lead.size() >= 2 },
        { "Ad-libs",     o.route && adlib.size() >= 2 },
        { "Music",       o.route && music.size() >= 2 },
        { "808 / Bass",  o.route && bass.size() >= 2 },
        { "Drum Crush",  o.route && full && drums.size() >= 2 },
    };
    std::vector<juce::String> granted;
    for (auto& w : wants)
    {
        if (! w.ok) continue;
        if ((int) granted.size() < p.busesAvailable) granted.push_back (w.name);
        else p.notes.add ("Not enough free buses for \"" + w.name + "\" (Nova Studio has 8) - those tracks stay on their own.");
    }
    auto has = [&] (const juce::String& n) { return std::find (granted.begin(), granted.end(), n) != granted.end(); };
    if (has ("Drum Crush") && ! has ("Drums")) granted.erase (std::find (granted.begin(), granted.end(), "Drum Crush"));
    p.busesUsed = (int) granted.size();

    // ── chains ───────────────────────────────────────────────────────────────
    auto leadChain = [&]()
    {
        std::vector<PluginStep> c;
        PluginStep v { "Nova AI Vocal", { { "Vibe", st.vibe }, { "Tune", o.tune ? "60" : "0" }, { "Space", "12" },
                                          { "BPM", num ((float) a.bpm) }, { "Tempo Sync", "On" } }, "", {} };
        if (a.keyKnown) v.settings.push_back ({ "Key", keyText (a) });
        v.note = juce::String (st.vibe) + (o.tune ? " - light tuning" : "") + (a.keyKnown ? " - key " + keyText (a) : "");
        c.push_back (v);
        return c;
    };
    std::vector<PluginStep> bgvChain, adlibChain, drumChain, bassChain, musicChain;
    if (! light)
    {
        bgvChain = { { "Nova Silk", { { "Smooth", "50" } }, "tames harsh build-up in the stack", {} },
                     { "Nova Glue", { { "Threshold", "-14" }, { "Ratio", "4:1" }, { "Glue", "50" } }, "glues the stack together", {} } };
        adlibChain = { { "Nova Vox", { { "Curve On", "On" }, { "Curve Mode", "Air" }, { "Level On", "On" } }, "bright, controlled ad-libs", {} } };
        drumChain = { { "Nova Punch", { { "Punch", full ? "65" : "55" } }, "sharper hits", {} },
                      { "Nova Glue", { { "Threshold", "-12" }, { "Ratio", "4:1" } }, "one tight kit", {} } };
        bassChain = { { "Nova Sub", { { "Mix", "35" } }, "fuller low end", {} },
                      { "Nova Clip", { { "Drive", full ? "6" : "4" } }, "controlled peaks, louder 808", {} } };
        musicChain = { { "Nova Glow", { { "Peak Reduction", "25" } }, "smooth, glued music", {} } };
    }

    // ── buses ────────────────────────────────────────────────────────────────
    auto addBus = [&] (const juce::String& name, Family fam, juce::Colour col, const std::vector<int>& members,
                       std::vector<PluginStep> chain, const juce::String& why, bool isReturn = false)
    {
        PlannedBus b;
        b.name = name; b.family = fam; b.colour = col; b.members = members; b.chain = std::move (chain);
        b.why = why; b.isReturn = isReturn;
        p.buses.push_back (b);
        return (int) p.buses.size() - 1;
    };
    if (has ("Vocal Verb"))
        addBus ("Vocal Verb", Family::Fx, juce::Colour (0xff5ad1ff), {},
                { { "Nova Space", { { "Mix", "100" }, { "Decay", num (st.verbDecay, 2) }, { "Pre-Delay", "30" } }, "one shared room for every vocal", {} } },
                "One shared vocal reverb - every voice sits in the same space, so they sound like one performance.", true);
    if (has ("Vocal Delay"))
        addBus ("Vocal Delay", Family::Fx, juce::Colour (0xff5ad1ff), {},
                { { "Nova Delay", { { "Mix", "100" }, { "Sync", "On" }, { "Delay Time Sync", "1/4" }, { "Feedback", "28" } }, "quarter-note echo, in time", {} } },
                "A quarter-note delay synced to " + num ((float) a.bpm, 0) + " BPM for throws and ad-libs.", true);
    if (has ("Lead Vocal"))
    {
        const int b = addBus ("Lead Vocal", Family::Vocals, roleColour (Role::Lead), lead, o.chains ? leadChain() : std::vector<PluginStep> {},
                              "Your lead is split over " + juce::String ((int) lead.size()) + " tracks - one bus treats them as one voice.");
        if (has ("Vocal Verb")) p.buses[(size_t) b].sends.push_back ({ "Vocal Verb", light ? -18.f : -16.f });
        if (has ("Vocal Delay")) p.buses[(size_t) b].sends.push_back ({ "Vocal Delay", -19.f });
    }
    if (has ("BG Vocals"))
    {
        const int b = addBus ("BG Vocals", Family::Vocals, roleColour (Role::Double), bgv, o.chains ? bgvChain : std::vector<PluginStep> {},
                              juce::String ((int) bgv.size()) + " backing parts (doubles and harmonies), spread wide and tucked under the lead so it stays in the middle.");
        if (has ("Vocal Verb")) p.buses[(size_t) b].sends.push_back ({ "Vocal Verb", -12.f });
    }
    if (has ("Ad-libs"))
    {
        const int b = addBus ("Ad-libs", Family::Vocals, roleColour (Role::Adlib), adlib, o.chains ? adlibChain : std::vector<PluginStep> {},
                              "Short phrases between lead lines, panned apart with a delay throw.");
        if (has ("Vocal Delay")) p.buses[(size_t) b].sends.push_back ({ "Vocal Delay", -14.f });
        if (has ("Vocal Verb")) p.buses[(size_t) b].sends.push_back ({ "Vocal Verb", -16.f });
    }
    if (has ("Drums"))
    {
        const int b = addBus ("Drums", Family::Drums, roleColour (Role::Kick), drums, o.chains ? drumChain : std::vector<PluginStep> {},
                              "Kick, snare and percussion together, so the kit hits as one.");
        if (has ("Drum Crush")) p.buses[(size_t) b].sends.push_back ({ "Drum Crush", -6.f });
    }
    if (has ("Drum Crush"))
    {
        const int b = addBus ("Drum Crush", Family::Drums, roleColour (Role::Kick), {},
                              { { "Nova Heat", { { "Drive", "7" }, { "Mix", "100" }, { "Mode", "Hard" } }, "heavily driven copy of the kit", {} } },
                              "A crushed copy of the drums blended in low - weight without losing the hits.", true);
        p.buses[(size_t) b].volumeDb = -9.f;
    }
    if (has ("808 / Bass"))
        addBus ("808 / Bass", Family::Bass, roleColour (Role::Bass), bass, o.chains ? bassChain : std::vector<PluginStep> {},
                "Low end in one place: fuller sub, controlled peaks.");
    if (has ("Music"))
    {
        const int b = addBus ("Music", Family::Music, roleColour (Role::Keys), music, o.chains ? musicChain : std::vector<PluginStep> {},
                              "Keys, guitars and pads together" + juce::String (o.makeRoom && vocals ? ", with a little space carved out where the lead vocal sits." : "."));
        if (o.makeRoom && vocals) p.buses[(size_t) b].eq.push_back ({ 2800.f, full ? -2.5f : -1.8f, 0.9f });
    }

    // ── tracks ───────────────────────────────────────────────────────────────
    int dblSide = 0, harmSide = 0, adSide = 0, gtrSide = 0;
    std::vector<size_t> levelled;          // moves whose fader the plan sets (index into p.tracks)
    for (size_t i = 0; i < a.tracks.size(); ++i)
    {
        const auto& t = a.tracks[i];
        const Role r = i < roles.size() ? roles[i] : t.role;
        TrackMove m;
        m.track = t.trackIndex;
        m.role = r;
        const auto& tr = s.getTrack (t.trackIndex);
        m.volumeDb = tr.volumeDb;
        m.pan = tr.pan;
        if (r == Role::Skip || r == Role::Other) { m.change = false; p.tracks.push_back (m); continue; }

        if (o.levels && ! t.midi && t.f.loudDb > -100.f)
        {
            m.volumeDb = targetLevel (r) - t.f.loudDb;     // range is fixed below, keeping the balance
            levelled.push_back (p.tracks.size());
        }
        if (o.levels)
        {
            const auto lower = t.name.toLowerCase();
            auto side = [] (int& k) { return (k++ % 2 == 0) ? -1.f : 1.f; };
            switch (r)
            {
                case Role::Double:  m.pan = 0.6f * side (dblSide); break;
                case Role::Harmony: m.pan = 0.85f * side (harmSide); break;
                case Role::Adlib:   m.pan = 0.35f * side (adSide); break;
                case Role::HiHat:   m.pan = 0.15f; break;
                case Role::Perc:    m.pan = -0.25f; break;
                case Role::Guitar:  m.pan = lower.contains ("left") || lower.endsWith (".l") ? -0.7f
                                          : lower.contains ("right") || lower.endsWith (".r") ? 0.7f : 0.45f * side (gtrSide); break;
                default:            m.pan = 0.f;
            }
        }

        // which bus it goes to
        auto busFor = [&]() -> juce::String
        {
            switch (r)
            {
                case Role::Lead: return has ("Lead Vocal") ? "Lead Vocal" : "";
                case Role::Double: case Role::Harmony: return has ("BG Vocals") ? "BG Vocals" : "";
                case Role::Adlib: return has ("Ad-libs") ? "Ad-libs" : "";
                case Role::Bass: return has ("808 / Bass") ? "808 / Bass" : "";
                default:
                    if (familyOf (r) == Family::Drums) return has ("Drums") ? "Drums" : "";
                    if (familyOf (r) == Family::Music && r != Role::Beat) return has ("Music") ? "Music" : "";
                    return "";
            }
        };
        m.toBus = busFor();

        // processing on the track itself when it isn't on a bus that does it
        if (o.chains && m.toBus.isEmpty())
        {
            if (r == Role::Lead) m.chain = leadChain();
            else if ((r == Role::Double || r == Role::Harmony) && ! light) m.chain = { bgvChain.empty() ? PluginStep {} : bgvChain[0] };
            else if (r == Role::Adlib) m.chain = adlibChain;
            else if (r == Role::Bass) m.chain = bassChain;
            m.chain.erase (std::remove_if (m.chain.begin(), m.chain.end(), [] (const PluginStep& ps) { return ps.plugin.isEmpty(); }), m.chain.end());
        }
        // sends straight from the track when there's no vocal bus doing it
        if (m.toBus.isEmpty())
        {
            if (r == Role::Lead) { if (has ("Vocal Verb")) m.sends.push_back ({ "Vocal Verb", light ? -18.f : -16.f }); if (has ("Vocal Delay")) m.sends.push_back ({ "Vocal Delay", -19.f }); }
            if (r == Role::Double || r == Role::Harmony) { if (has ("Vocal Verb")) m.sends.push_back ({ "Vocal Verb", -12.f }); }
            if (r == Role::Adlib) { if (has ("Vocal Delay")) m.sends.push_back ({ "Vocal Delay", -14.f }); if (has ("Vocal Verb")) m.sends.push_back ({ "Vocal Verb", -16.f }); }
        }
        // make room for the lead vocal in a 2-track beat or loose music tracks
        if (o.makeRoom && vocals && (r == Role::Beat || (familyOf (r) == Family::Music && m.toBus.isEmpty())))
            m.eq.push_back ({ 2800.f, r == Role::Beat ? (full ? -2.0f : -1.5f) : -1.8f, 0.9f });
        p.tracks.push_back (m);
    }

    // Quiet recordings can ask for more than a fader should give. Instead of clamping
    // one track (which breaks the balance), move every planned fader down together;
    // the mix bus brings the loudness back.
    if (! levelled.empty())
    {
        float top = -100.f;
        for (auto k : levelled) top = juce::jmax (top, p.tracks[k].volumeDb);
        const float shift = juce::jmax (0.f, top - 3.f);
        for (auto k : levelled) p.tracks[k].volumeDb = juce::jlimit (-30.f, 6.f, p.tracks[k].volumeDb - shift);
        if (shift > 0.5f)
            p.notes.add ("Your recordings are quiet, so every fader sits " + juce::String (shift, 1)
                         + " dB lower to keep the balance - the mix bus makes up the level.");
    }

    // ── mix bus ──────────────────────────────────────────────────────────────
    if (o.masterPolish)
    {
        PluginStep mirror { "Nova Mirror", { { "Cut", light ? "40" : full ? "70" : "55" }, { "Boost", light ? "30" : full ? "55" : "45" },
                                             { "Source", "Mix Bus" } }, "", mirrorState (o) };
        mirror.note = o.reference.existsAsFile() ? "toward your reference: " + o.reference.getFileNameWithoutExtension()
                                                 : "toward the " + o.style + " target";
        p.master.push_back (mirror);
        PluginStep master { "Nova AI Master", { { "Style", st.master }, { "Intensity", light ? "0.35" : full ? "0.7" : "0.5" },
                                                { "Loudness", st.loudness } },
                            juce::String (st.master) + " master, " + juce::String (st.loudness).toLowerCase() + " loudness - analysed on the finished mix", {} };
        master.analyseMix = true;
        p.master.push_back (master);
        if (s.masterLimiter)
        {
            p.builtInLimiterOff = true;
            p.notes.add ("Nova AI Master limits the mix bus, so the master safety ceiling is switched off (Undo turns it back on).");
        }
    }

    if (! vocals) p.notes.add ("No vocals found - vocal buses and effects skipped.");
    return p;
}
}
