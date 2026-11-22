#include "NovaSampleFolderLoader.h"
#include <algorithm>
#include <map>
#include <vector>

namespace
{
    // Parses a single "C4" / "G#1" / "A#3" style token into a MIDI note
    // number using C4 = MIDI 60 (matches this UI's own keyboard labeling).
    int noteTokenToMidi (const juce::String& token) noexcept
    {
        if (token.isEmpty()) return -1;

        static const std::map<juce::juce_wchar, int> base {
            { 'C', 0 }, { 'D', 2 }, { 'E', 4 }, { 'F', 5 }, { 'G', 7 }, { 'A', 9 }, { 'B', 11 }
        };

        const auto letter = juce::CharacterFunctions::toUpperCase (token[0]);
        auto it = base.find (letter);
        if (it == base.end()) return -1;

        int semitone = it->second;
        int i = 1;
        if (i < token.length() && token[i] == '#') { semitone += 1; ++i; }
        else if (i < token.length() && (token[i] == 'b' || token[i] == 'B') && i + 1 < token.length() && juce::CharacterFunctions::isDigit (token[i + 1]))
        {
            // flat notation, e.g. "Db4" - rare in this dataset but handled
            semitone -= 1; ++i;
        }

        if (i >= token.length()) return -1;

        const auto octavePart = token.substring (i);
        if (! (octavePart[0] == '-' || juce::CharacterFunctions::isDigit (octavePart[0]))) return -1;

        const int octave = octavePart.getIntValue();
        // Scientific pitch: C4 = MIDI 60 -> MIDI = (octave + 1) * 12 + semitone
        const int midi = (octave + 1) * 12 + semitone;
        return juce::jlimit (0, 127, midi);
    }

    // Velocity-tier hint: looks for "_vNN_" or "_vlNN_" tokens.
    int parseVelocityTier (const juce::StringArray& tokens)
    {
        for (auto& t : tokens)
        {
            auto lower = t.toLowerCase();
            if (lower.startsWith ("vl") && lower.substring (2).containsOnly ("0123456789") && lower.length() > 2)
                return lower.substring (2).getIntValue();
            if (lower.startsWith ("v") && lower.length() > 1 && lower.substring (1).containsOnly ("0123456789"))
                return lower.substring (1).getIntValue();
        }
        return -1;
    }

    // Round-robin index hint: looks for "_rrN_" tokens.
    int parseRoundRobinIndex (const juce::StringArray& tokens)
    {
        for (auto& t : tokens)
        {
            auto lower = t.toLowerCase();
            if (lower.startsWith ("rr") && lower.length() > 2 && lower.substring (2).containsOnly ("0123456789"))
                return lower.substring (2).getIntValue();
        }
        return 0;
    }
}

int NovaSampleFolderLoader::parseNoteNameFromFilename (const juce::String& filename)
{
    const auto stem = filename.upToLastOccurrenceOf (".", false, false);
    auto tokens = juce::StringArray::fromTokens (stem, "_", "");
    for (auto& t : tokens)
    {
        const int midi = noteTokenToMidi (t);
        if (midi >= 0) return midi;
    }
    return -1;
}

juce::AudioFormatManager& NovaSampleFolderLoader::getFormatManager()
{
    static juce::AudioFormatManager manager;
    static bool initialised = false;
    if (! initialised) { manager.registerBasicFormats(); initialised = true; }
    return manager;
}

int NovaSampleFolderLoader::loadFolderIntoEngine (const juce::File& folder, NovaSamplerEngine& engine, int roundRobinGroup)
{
    if (! folder.isDirectory()) return 0;

    struct FileInfo
    {
        juce::File file;
        int rootNote = -1;
        int velocityTier = -1;
        int roundRobinIndex = 0;
    };

    std::vector<FileInfo> files;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.wav;*.aiff;*.aif", juce::File::findFiles))
    {
        FileInfo info;
        info.file = entry.getFile();
        const auto stem = info.file.getFileNameWithoutExtension();
        auto tokens = juce::StringArray::fromTokens (stem, "_", "");
        info.rootNote = parseNoteNameFromFilename (stem);
        info.velocityTier = parseVelocityTier (tokens);
        info.roundRobinIndex = parseRoundRobinIndex (tokens);
        files.push_back (info);
    }

    if (files.empty()) return 0;

    const bool allPitched = std::all_of (files.begin(), files.end(), [] (const FileInfo& f) { return f.rootNote >= 0; });

    auto loadBuffer = [] (const juce::File& f) -> std::shared_ptr<juce::AudioBuffer<float>>
    {
        std::unique_ptr<juce::AudioFormatReader> reader (getFormatManager().createReaderFor (f));
        if (reader == nullptr) return nullptr;
        auto buf = std::make_shared<juce::AudioBuffer<float>> ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (buf.get(), 0, (int) reader->lengthInSamples, 0, true, true);
        return buf;
    };

    int zonesAdded = 0;
    double sourceSampleRateForLastFile = 44100.0;

    if (allPitched)
    {
        // Group files by root note, then compute key-range boundaries at
        // the midpoints between adjacent distinct notes.
        std::map<int, std::vector<FileInfo>> byNote;
        for (auto& f : files) byNote[f.rootNote].push_back (f);

        std::vector<int> notes;
        for (auto& kv : byNote) notes.push_back (kv.first);
        std::sort (notes.begin(), notes.end());

        for (size_t i = 0; i < notes.size(); ++i)
        {
            const int note = notes[i];
            const int lowKey = (i == 0) ? 0 : (notes[i - 1] + note) / 2 + 1;
            const int highKey = (i + 1 == notes.size()) ? 127 : (note + notes[i + 1]) / 2;

            auto& tierFiles = byNote[note];
            std::sort (tierFiles.begin(), tierFiles.end(), [] (const FileInfo& a, const FileInfo& b) { return a.velocityTier < b.velocityTier; });

            const int numTiers = (int) tierFiles.size();
            for (int t = 0; t < numTiers; ++t)
            {
                std::unique_ptr<juce::AudioFormatReader> reader (getFormatManager().createReaderFor (tierFiles[(size_t) t].file));
                if (reader == nullptr) continue;

                auto buf = loadBuffer (tierFiles[(size_t) t].file);
                if (buf == nullptr) continue;

                NovaSampleZone zone;
                zone.name = tierFiles[(size_t) t].file.getFileNameWithoutExtension();
                zone.rootNote = note;
                zone.lowKey = lowKey;
                zone.highKey = highKey;
                zone.buffer = buf;
                zone.sourceSampleRate = reader->sampleRate;
                sourceSampleRateForLastFile = reader->sampleRate;

                if (numTiers <= 1)
                {
                    zone.lowVelocity = 1; zone.highVelocity = 127;
                    zone.roundRobinGroup = roundRobinGroup + (int) i * 4;
                }
                else
                {
                    // Even split across tiers with a modest overlap window
                    // for crossfading (spec section 3).
                    const float overlap = 20.f;
                    const float span = 127.f / (float) numTiers;
                    const float lo = juce::jmax (1.f, (float) t * span - overlap * 0.5f);
                    const float hi = juce::jmin (127.f, (float) (t + 1) * span + overlap * 0.5f);
                    zone.lowVelocity = (int) lo; zone.highVelocity = (int) hi;
                    zone.roundRobinGroup = roundRobinGroup + (int) i * 4 + t; // distinct group per tier -> crossfade, not rotation
                }

                engine.addZone (zone);
                ++zonesAdded;
            }
        }
    }
    else
    {
        // Unpitched (percussion): whole key range triggers it. Build
        // velocity layers from tier hints if present, round-robin within
        // each tier from rr hints if present.
        std::map<int, std::vector<FileInfo>> byTier;
        for (auto& f : files) byTier[juce::jmax (0, f.velocityTier)].push_back (f);

        std::vector<int> tiers;
        for (auto& kv : byTier) tiers.push_back (kv.first);
        std::sort (tiers.begin(), tiers.end());

        const int numTiers = (int) tiers.size();
        for (int ti = 0; ti < numTiers; ++ti)
        {
            int loVel = 1, hiVel = 127;
            if (numTiers > 1)
            {
                const float overlap = 15.f;
                const float span = 127.f / (float) numTiers;
                loVel = (int) juce::jmax (1.f, (float) ti * span - overlap * 0.5f);
                hiVel = (int) juce::jmin (127.f, (float) (ti + 1) * span + overlap * 0.5f);
            }

            const int groupId = roundRobinGroup + ti; // all round-robin siblings within a tier share this group

            for (auto& f : byTier[tiers[(size_t) ti]])
            {
                auto buf = loadBuffer (f.file);
                if (buf == nullptr) continue;
                std::unique_ptr<juce::AudioFormatReader> reader (getFormatManager().createReaderFor (f.file));
                if (reader == nullptr) continue;

                NovaSampleZone zone;
                zone.name = f.file.getFileNameWithoutExtension();
                zone.rootNote = 60;
                zone.lowKey = 0; zone.highKey = 127;
                zone.lowVelocity = loVel; zone.highVelocity = hiVel;
                zone.roundRobinGroup = groupId;
                zone.buffer = buf;
                zone.sourceSampleRate = reader->sampleRate;
                sourceSampleRateForLastFile = reader->sampleRate;

                engine.addZone (zone);
                ++zonesAdded;
            }
        }
    }

    juce::ignoreUnused (sourceSampleRateForLastFile);
    return zonesAdded;
}
