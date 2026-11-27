#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace NovaMelody;

namespace
{
    constexpr double kMaxCaptureSec = 60.0;
    const int kBarsChoices[] = { 2, 4, 8, 16 };

    juce::var notesToVar (const std::vector<MNote>& notes)
    {
        juce::Array<juce::var> a;
        for (auto& m : notes)
            a.add (juce::Array<juce::var> { std::round (m.start * 1000.0) / 1000.0, std::round (m.len * 1000.0) / 1000.0, m.pitch, m.vel,
                                            m.locked ? 1 : 0, m.glide ? 1 : 0 });
        return a;
    }

    std::vector<MNote> notesFromVar (const juce::var& v)
    {
        std::vector<MNote> out;
        if (auto* a = v.getArray())
            for (auto& n : *a)
                if (auto* x = n.getArray(); x != nullptr && x->size() >= 4)
                {
                    MNote m;
                    m.start = (double) (*x)[0]; m.len = (double) (*x)[1]; m.pitch = (int) (*x)[2]; m.vel = (int) (*x)[3];
                    m.locked = x->size() > 4 && (int) (*x)[4] != 0;
                    m.glide = x->size() > 5 && (int) (*x)[5] != 0;
                    out.push_back (m);
                }
        return out;
    }
}

NovaMelodyProcessor::NovaMelodyProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Hum Input", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    playList = std::make_shared<PlayList>();
    generateAll();
    startTimerHz (20);
}

NovaMelodyProcessor::~NovaMelodyProcessor()
{
    stopTimer();
    if (mic != nullptr) { mic->removeAudioCallback (this); mic->closeAudioDevice(); }
    while (analysing.load()) juce::Thread::sleep (5);
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaMelodyProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray genres, vibes, parts, scales, sounds;
    for (int i = 0; i < kNumGenres; ++i) genres.add (genreName (i));
    for (int i = 0; i < kNumVibes; ++i) vibes.add (vibeName (i));
    for (int i = 0; i < kNumParts; ++i) parts.add (partName (i));
    for (int i = 0; i < kNumScales; ++i) scales.add (scaleName (i));
    for (int i = 0; i < kNumSounds; ++i) sounds.add (soundName (i));
    sounds.add ("Off (MIDI only)");
    StringArray notes;
    for (int i = 0; i < 12; ++i) notes.add (noteName (i));

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 100.f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "genre", 1 }, "Genre", genres, LatinUrban));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "vibe", 1 }, "Vibe", vibes, Emotional));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "part", 1 }, "Part", parts, Lead));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "register", 1 }, "Register", StringArray { "Low", "Mid", "High" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Length", StringArray { "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
    l.add (pct ("density", "Notes", 55.f));
    l.add (pct ("rhythm", "Rhythm", 70.f));
    l.add (pct ("swing", "Swing", 20.f));
    l.add (pct ("repetition", "Repetition", 60.f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "followChords", 1 }, "Follow chords", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "stayInKey", 1 }, "Stay in key", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "leaveSpace", 1 }, "Leave space for vocals", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "keyTonic", 1 }, "Key", notes, 7));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "keyScale", 1 }, "Scale", scales, Minor));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "keyAutoGen", 1 }, "Key auto (Generate)", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "keyAutoHum", 1 }, "Key auto (Hum)", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "humAs", 1 }, "Hum becomes", StringArray { "Bassline", "808", "Melody", "Chords under it" }, 0));
    l.add (pct ("pitchSnap", "Pitch (exact to cleaned up)", 0.f));
    l.add (pct ("timeSnap", "Timing (exact to cleaned up)", 0.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "minBlip", 1 }, "Short blips", NormalisableRange<float> (10.f, 200.f), 40.f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "autoOctave", 1 }, "Auto range", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sound", 1 }, "Sound", sounds, SoftPluck));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "midiOut", 1 }, "MIDI out", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "playStyle", 1 }, "How you play",
                                                   StringArray { "Every key is a good note", "One finger chords", "Tap the melody" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "glow", 1 }, "Glow the notes that fit", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "fixTiming", 1 }, "Fix my timing", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "strum", 1 }, "Strum chords", false));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "playOctave", 1 }, "Play octave", -2, 2, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "recWhat", 1 }, "Record", StringArray { "Melody + chords", "Melody only" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-30.f, 6.f), -6.f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

bool NovaMelodyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NovaMelodyProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    synth.prepare (sr);
    active.clear();
    active.reserve (256);
    wasPlaying = false;
}

void NovaMelodyProcessor::releaseResources() {}

// ════════════════════════════════════════════════════════════════════════════
// Keys, parameters -> core settings
// ════════════════════════════════════════════════════════════════════════════
Key NovaMelodyProcessor::pickedKey() const
{
    return { (int) p ("keyTonic"), (int) p ("keyScale") };
}

Key NovaMelodyProcessor::keyForGenerate() const
{
    if (p ("keyAutoGen") > 0.5f)
    {
        const juce::ScopedLock sl (chordLock);
        float sum = 0;
        for (float w : pcWeights) sum += w;
        if (sum > 0.5f) return detectKey (pcWeights);
    }
    return pickedKey();
}

GenParams NovaMelodyProcessor::genParams (juce::uint32 seed) const
{
    GenParams g;
    g.genre = (int) p ("genre"); g.vibe = (int) p ("vibe"); g.part = (int) p ("part"); g.reg = (int) p ("register");
    g.density = p ("density") / 100.f; g.rhythm = p ("rhythm") / 100.f; g.swing = p ("swing") / 100.f; g.repetition = p ("repetition") / 100.f;
    g.bars = kBarsChoices[juce::jlimit (0, 3, (int) p ("bars"))];
    g.stayInKey = p ("stayInKey") > 0.5f;
    g.leaveSpace = p ("leaveSpace") > 0.5f;
    g.key = keyForGenerate();
    g.seed = seed;
    if (p ("followChords") > 0.5f)
    {
        const juce::ScopedLock sl (chordLock);
        if (! heardChords.empty())
        {
            // bars we heard chords on (in the song's bar order); gaps hold the previous chord
            std::vector<int> pcs;
            for (int b = 0; b < g.bars; ++b)
            {
                auto it = heardChords.find (b);
                if (it == heardChords.end() && heardChords.count (b % std::max (1, heardChords.rbegin()->first + 1)) > 0)
                    it = heardChords.find (b % (heardChords.rbegin()->first + 1));
                if (it != heardChords.end()) pcs = it->second;
                if (pcs.empty()) pcs = heardChords.begin()->second;
                Chord c; c.start = b * 4.0; c.len = 4.0; c.pcs = pcs;
                c.root = pcs.front();
                g.chords.push_back (c);
            }
        }
    }
    return g;
}

HumParams NovaMelodyProcessor::humParams() const
{
    HumParams h;
    h.as = (int) p ("humAs");
    h.pitchSnap = p ("pitchSnap") / 100.f;
    h.timeSnap = p ("timeSnap") / 100.f;
    h.minNoteMs = p ("minBlip");
    h.octave = (int) p ("octave");
    h.autoOctave = p ("autoOctave") > 0.5f;
    h.autoKey = p ("keyAutoHum") > 0.5f;
    h.key = pickedKey();
    h.bpm = bpm.load();
    h.genre = (int) p ("genre");
    return h;
}

// ════════════════════════════════════════════════════════════════════════════
// Generate
// ════════════════════════════════════════════════════════════════════════════
void NovaMelodyProcessor::generateAll()
{
    std::vector<MNote> keep;
    for (auto& m : variations[(size_t) selected]) if (m.locked) keep.push_back (m);
    std::vector<MNote> against;
    if ((int) p ("part") == Counter) against = variations[(size_t) selected];
    seedBase = (juce::uint32) juce::Random::getSystemRandom().nextInt (1 << 30);
    for (int i = 0; i < 4; ++i)
    {
        auto g = genParams (seedBase + (juce::uint32) i * 101u);
        g.keep = keep;
        if (! against.empty()) { g.against = against; for (auto& m : g.against) m.locked = false; }
        variations[(size_t) i] = generate (g);
    }
    publish();
    bump();
}

void NovaMelodyProcessor::selectVariation (int i)
{
    selected = juce::jlimit (0, 3, i);
    publish();
    bump();
}

void NovaMelodyProcessor::toggleLock (int idx)
{
    auto& v = variations[(size_t) selected];
    if (juce::isPositiveAndBelow (idx, (int) v.size())) v[(size_t) idx].locked = ! v[(size_t) idx].locked;
    bump();
}

void NovaMelodyProcessor::clearLocks()
{
    for (auto& m : variations[(size_t) selected]) m.locked = false;
    bump();
}

void NovaMelodyProcessor::redoRest()
{
    auto g = genParams ((juce::uint32) juce::Random::getSystemRandom().nextInt (1 << 30));
    for (auto& m : variations[(size_t) selected]) if (m.locked) g.keep.push_back (m);
    variations[(size_t) selected] = generate (g);
    publish();
    bump();
}

void NovaMelodyProcessor::setPreview (bool play)
{
    preview.store (play);
    bump();
}

// ════════════════════════════════════════════════════════════════════════════
// Hum
// ════════════════════════════════════════════════════════════════════════════
void NovaMelodyProcessor::startRecording (bool useMic)
{
    if (recording.load() || analysing.load()) return;
    captureLen.store (0);
    captureStartBeat = -1;
    captureFromMic = useMic;
    captureRate = sampleRate;
    if (useMic)
    {
        mic = std::make_unique<juce::AudioDeviceManager>();
        const auto err = mic->initialise (1, 0, nullptr, true);
        if (err.isNotEmpty() || mic->getCurrentAudioDevice() == nullptr)
        {
            humStatus = "Couldn't open the microphone (" + (err.isNotEmpty() ? err : juce::String ("no input device"))
                      + "). Allow microphone access for this app in System Settings > Privacy, or record on a track and use Track input.";
            mic.reset();
            bump();
            return;
        }
        captureRate = mic->getCurrentAudioDevice()->getCurrentSampleRate();
    }
    // room for 60 s at this rate (allocated here, never on the audio thread)
    if (capture.getNumSamples() < (int) (kMaxCaptureSec * captureRate) + 1)
        capture.setSize (1, (int) (kMaxCaptureSec * captureRate) + 1);
    if (mic != nullptr) mic->addAudioCallback (this);
    humStatus = useMic ? "Recording from the mic - hum, then press stop." : "Recording from the track input - hum, then press stop.";
    recording.store (true);
    bump();
}

void NovaMelodyProcessor::stopRecording()
{
    if (! recording.exchange (false)) return;
    if (mic != nullptr) { mic->removeAudioCallback (this); mic->closeAudioDevice(); mic.reset(); }
    analyseCapture();
}

void NovaMelodyProcessor::audioDeviceAboutToStart (juce::AudioIODevice* d) { if (d != nullptr) captureRate = d->getCurrentSampleRate(); }

void NovaMelodyProcessor::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                           int n, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c) if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);
    if (recording.load() && captureFromMic) captureSamples (in, numIn, n);
}

void NovaMelodyProcessor::captureSamples (const float* const* in, int numIn, int n)
{
    if (numIn <= 0 || in == nullptr) return;
    const int pos = captureLen.load();
    const int room = capture.getNumSamples() - pos;
    const int k = std::min (n, room);
    float* dst = capture.getWritePointer (0) + pos;
    float pk = 0.f;
    int used = 0;
    for (int c = 0; c < numIn; ++c) if (in[c] != nullptr) ++used;
    for (int i = 0; i < k; ++i)
    {
        float s = 0.f;
        for (int c = 0; c < numIn; ++c) if (in[c] != nullptr) s += in[c][i];
        s /= (float) std::max (1, used);
        dst[i] = s;
        pk = std::max (pk, std::abs (s));
    }
    inputLevel.store (std::max (pk, inputLevel.load() * 0.9f));
    captureLen.store (pos + std::max (0, k));
    if (k < n) recording.store (false);      // full (60 s)
}

void NovaMelodyProcessor::analyseCapture()
{
    const int n = captureLen.load();
    if (n < (int) (captureRate * 0.3))
    {
        humStatus = n == 0 ? "Nothing came in - check the input (mic access, or the track's input)." : "That take is too short - hum for at least a second.";
        bump();
        return;
    }
    analysing.store (true);
    humStatus = "Listening to your take...";
    bump();
    auto take = std::make_shared<std::vector<float>> (capture.getReadPointer (0), capture.getReadPointer (0) + n);
    auto hp = humParams();
    hp.capturedAtBeat = captureStartBeat;
    const double rate = captureRate;
    juce::WeakReference<NovaMelodyProcessor> safe (this);
    juce::Thread::launch ([this, take, hp, rate, safe]
    {
        auto r = std::make_shared<HumResult> (fromHum (take->data(), (int) take->size(), rate, hp));
        juce::MessageManager::callAsync ([safe, r]
        {
            if (safe == nullptr) return;
            auto& self = *safe;
            self.hum = *r;
            self.humExtraMode = 0;
            self.humStatus = r->ok ? "Heard " + juce::String ((int) r->heard.size()) + " notes" : r->message;
            self.remakeHum();
            self.analysing.store (false);
        });
        if (safe == nullptr) analysing.store (false);
    });
}

void NovaMelodyProcessor::loadHumFile (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (f.loadFileAsData (mb)) loadHumBytes (f.getFileName(), mb);
}

void NovaMelodyProcessor::loadHumBytes (const juce::String& name, const juce::MemoryBlock& data)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd;
    for (auto* fmt : fm)
    {
        auto* in = new juce::MemoryInputStream (data, false);
        if (auto* r = fmt->createReaderFor (in, true)) { rd.reset (r); break; }
    }
    if (rd == nullptr) { humStatus = "Couldn't read \"" + name + "\" - use a WAV, AIFF, FLAC or MP3 voice memo."; bump(); return; }
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (kMaxCaptureSec * rd->sampleRate));
    juce::AudioBuffer<float> b ((int) rd->numChannels, n);
    rd->read (&b, 0, n, 0, true, true);
    captureRate = rd->sampleRate;
    if (capture.getNumSamples() < n) capture.setSize (1, n);
    auto* dst = capture.getWritePointer (0);
    for (int i = 0; i < n; ++i)
    {
        float s = 0;
        for (int c = 0; c < b.getNumChannels(); ++c) s += b.getSample (c, i);
        dst[i] = s / (float) std::max (1, b.getNumChannels());
    }
    captureLen.store (n);
    captureStartBeat = -1;
    analyseCapture();
}

void NovaMelodyProcessor::remakeHum()
{
    if (hum.ok)
    {
        const auto hp = humParams();
        // the key follows Auto (heard) or the picked key
        if (! hp.autoKey) hum.key = hp.key;
        remakeFromHeard (hum, hp);
        humOut = hum.notes;
        const bool bass = hp.as == AsBass || hp.as == As808;
        if (humExtraMode == 1) humOut = makeItGroove (hum.notes, hp.genre, hum.lengthBeats, bass, seedBase);
        if (humExtraMode == 2 && hp.as != AsChords) humOut = answerIt (hum.notes, hum.key, hp.genre, hum.lengthBeats, seedBase);
    }
    else
        humOut.clear();
    publish();
    bump();
}

void NovaMelodyProcessor::humExtra (int which)
{
    humExtraMode = humExtraMode == which ? 0 : which;     // pressing it again goes back to your notes
    seedBase = (juce::uint32) juce::Random::getSystemRandom().nextInt (1 << 30);
    remakeHum();
}

// ════════════════════════════════════════════════════════════════════════════
// What plays
// ════════════════════════════════════════════════════════════════════════════
std::vector<MNote> NovaMelodyProcessor::currentNotes() const
{
    return mode == 1 ? humOut : (mode == 2 ? playTake : variations[(size_t) selected]);
}

double NovaMelodyProcessor::currentLengthBeats() const
{
    if (mode == 1) return hum.ok ? hum.lengthBeats : 16.0;
    if (mode == 2) return playTakeLen;
    return kBarsChoices[juce::jlimit (0, 3, (int) p ("bars"))] * 4.0;
}

juce::String NovaMelodyProcessor::suggestedMidiName() const
{
    if (mode == 2) return "Nova Melody - played in " + keyName ({ playTonic.load(), playScale.load() });
    if (mode == 1) return "Nova Melody - hummed " + juce::String (juce::StringArray { "bassline", "808", "melody", "chords" }[(int) p ("humAs")]);
    return "Nova Melody - " + juce::String (genreName ((int) p ("genre"))) + " " + juce::String (vibeName ((int) p ("vibe"))).toLowerCase()
         + " " + juce::String (partName ((int) p ("part"))).toLowerCase() + " " + juce::String ("ABCD"[selected]);
}

juce::MemoryBlock NovaMelodyProcessor::currentMidiFile() const
{
    auto notes = currentNotes();
    return toMidiFile (notes, bpm.load(), suggestedMidiName());
}

void NovaMelodyProcessor::publish()
{
    auto pl = std::make_shared<PlayList>();
    pl->notes = currentNotes();
    for (auto& m : pl->notes) m.locked = false;
    pl->length = std::max (1.0, currentLengthBeats());
    auto tl = std::make_shared<const std::vector<MNote>> (variations[(size_t) selected]);
    const juce::SpinLock::ScopedLockType sl (playLock);
    playList = pl;
    tapList = tl;
}

// ════════════════════════════════════════════════════════════════════════════
// Audio
// ════════════════════════════════════════════════════════════════════════════
void NovaMelodyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    // hum from the track input
    if (recording.load() && ! captureFromMic && numIn > 0)
    {
        const float* ins[2] = { buffer.getReadPointer (0), numIn > 1 ? buffer.getReadPointer (1) : nullptr };
        captureSamples (ins, numIn, n);
    }

    // chords played in (for Follow chords and the Auto key)
    bool heldChanged = false;
    const bool playMode = modeNow.load() == 2;
    struct In { int sample, note, vel; bool on; };
    std::array<In, 128> liveIn;
    int numIn2 = 0;
    for (const auto meta : midi)
    {
        if (playMode)
        {
            const auto m = meta.getMessage();
            if ((m.isNoteOn() || m.isNoteOff()) && numIn2 < (int) liveIn.size())
                liveIn[(size_t) numIn2++] = { meta.samplePosition, m.getNoteNumber(), m.getVelocity(), m.isNoteOn() };
            continue;
        }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { held[(size_t) m.getNoteNumber()] = true; heldChanged = true; }
        else if (m.isNoteOff()) { held[(size_t) m.getNoteNumber()] = false; heldChanged = true; }
        else if (m.isAllNotesOff()) { held.fill (false); heldChanged = true; }
    }
    midi.clear();

    if (licenseGate.blocks (buffer)) return;
    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    // position
    bool hostPlaying = false;
    double hostBeat = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm.store (*b);
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) hostBeat = *q;
        }
    const double beatsPerSample = bpm.load() / 60.0 / sampleRate;
    const double blockBeats = n * beatsPerSample;

    if (recording.load() && captureStartBeat < 0 && ! captureFromMic && hostPlaying) captureStartBeat = hostBeat;
    if (recording.load() && captureStartBeat < 0 && captureFromMic && hostPlaying) captureStartBeat = hostBeat;

    // chord capture (no allocation unless the chord changed)
    {
        int count = 0, lowest = -1;
        std::array<bool, 12> pcOn {};
        for (int k = 0; k < 128; ++k)
            if (held[(size_t) k]) { ++count; pcOn[(size_t) (k % 12)] = true; if (lowest < 0) lowest = k % 12; }
        const juce::ScopedTryLock sl (chordLock);
        if (sl.isLocked())
        {
            for (int k = 0; k < 12; ++k) if (pcOn[(size_t) k]) pcWeights[(size_t) k] += (float) (n / sampleRate);
            if (count >= 2 && heldChanged)
            {
                std::vector<int> pcs { lowest };          // lowest held note is the root
                for (int k = 0; k < 12; ++k) if (pcOn[(size_t) k] && k != lowest) pcs.push_back (k);
                const int bar = hostPlaying ? (int) std::floor (hostBeat / 4.0) % 16
                                            : (heardChords.empty() ? 0 : (heardChords.rbegin()->second == pcs ? heardChords.rbegin()->first : heardChords.rbegin()->first + 1) % 16);
                heardChords[bar] = std::move (pcs);
                lastChordBar = bar;
            }
        }
    }

    std::shared_ptr<const PlayList> pl;
    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        pl = playList;
    }

    const bool silent = (int) p ("sound") == kSoundOff;
    if (silent) synth.allOff(); else synth.setSound ((int) p ("sound"));
    const bool sendMidi = p ("midiOut") > 0.5f;
    const bool playing = hostPlaying || preview.load();

    auto allOff = [&] (int sample)
    {
        for (auto& a : active) { if (sendMidi) midi.addEvent (juce::MidiMessage::noteOff (1, a.first), sample); synth.noteOff (a.first); }
        active.clear();
    };

    double b0;
    if (hostPlaying)
    {
        b0 = hostBeat;
        if (! wasPlaying || std::abs (hostBeat - (lastHostBeat + blockBeats)) > 0.05) allOff (0);   // started or jumped
        lastHostBeat = hostBeat;
    }
    else if (preview.load())
    {
        if (wasPlaying && lastHostBeat >= 0) { allOff (0); previewBeat = 0; lastHostBeat = -1; }
        b0 = previewBeat;
        previewBeat += blockBeats;
    }
    else
    {
        if (wasPlaying || ! active.empty()) allOff (0);
        previewBeat = 0;
        lastHostBeat = -1;
        b0 = 0;
    }
    wasPlaying = playing;

    std::array<Ev, 512> evs;
    int numEv = 0;
    if (playing && pl != nullptr && ! pl->notes.empty())
    {
        const double L = pl->length, b1 = b0 + blockBeats;
        // note-offs due in this block
        for (size_t i = 0; i < active.size();)
        {
            if (active[i].second < b1)
            {
                if (numEv < (int) evs.size()) evs[(size_t) numEv++] = { (int) juce::jlimit (0.0, (double) n - 1, (active[i].second - b0) / beatsPerSample), active[i].first, 0, false, false };
                active.erase (active.begin() + (long) i);
            }
            else ++i;
        }
        // note-ons whose (looped) start falls in this block
        for (auto& m : pl->notes)
        {
            const double k = std::ceil ((b0 - m.start) / L - 1e-9);
            const double t = k * L + m.start;
            if (t >= b0 && t < b1 && numEv < (int) evs.size())
            {
                const int s = (int) juce::jlimit (0.0, (double) n - 1, (t - b0) / beatsPerSample);
                evs[(size_t) numEv++] = { s, m.pitch, m.vel, true, m.glide };
                const double off = t + std::max (0.03, m.len);
                if (off < b1 && numEv < (int) evs.size())
                    evs[(size_t) numEv++] = { (int) juce::jlimit ((double) s, (double) n - 1, (off - b0) / beatsPerSample), m.pitch, 0, false, false };
                else if (active.size() < 256)
                    active.push_back ({ m.pitch, off });
            }
        }
        playBeat.store (std::fmod (std::max (0.0, b0), L));
    }
    else
        playBeat.store (-1.0);

    // ── live playing (Play tab): pads, computer keys, your MIDI keyboard ──────
    bpsNow = beatsPerSample;
    clockAtBlock = hostPlaying ? hostBeat : freeClock;
    freeClock = clockAtBlock + blockBeats;
    if (playRec.load())
    {
        if (recStartBeat.load() < 0) recStartBeat.store (hostPlaying ? std::floor (hostBeat / 4.0) * 4.0 : clockAtBlock);
        recNowBeat.store (clockAtBlock + blockBeats);
    }
    {
        int s1, n1, s2, n2;
        uiFifo.prepareToRead (uiFifo.getNumReady(), s1, n1, s2, n2);
        auto handle = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto& u = uiEvs[(size_t) (start + i)];
                if (u.down) liveDown (u.src, u.kind, u.idx, u.vel, 0, evs.data(), numEv);
                else liveUp (u.src, 0, evs.data(), numEv);
            }
        };
        handle (s1, n1); handle (s2, n2);
        uiFifo.finishedRead (n1 + n2);
    }
    for (int i = 0; i < numIn2; ++i)
    {
        const auto& e = liveIn[(size_t) i];
        if (e.on) liveDown (e.note, 2, e.note, e.vel, e.sample, evs.data(), numEv);
        else liveUp (e.note, e.sample, evs.data(), numEv);
    }
    // strummed chord notes due in this block (counted from this block, including ones just queued)
    for (int i = 0; i < numStrums;)
    {
        auto& st = strums[(size_t) i];
        if (st.samplesLeft < n)
        {
            if (numEv < (int) evs.size()) evs[(size_t) numEv++] = { std::max (0, st.samplesLeft), st.pitch, st.vel, true, false };
            recPush (clockAtBlock + std::max (0, st.samplesLeft) * beatsPerSample, st.pitch, st.vel, true, true);
            strums[(size_t) i] = strums[(size_t) --numStrums];
        }
        else { st.samplesLeft -= n; ++i; }
    }

    std::sort (evs.begin(), evs.begin() + numEv, [] (const Ev& a, const Ev& b) { return a.sample != b.sample ? a.sample < b.sample : (! a.on && b.on); });

    // render in segments between events
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    const float gain = juce::Decibels::decibelsToGain (p ("volume"));
    int at = 0;
    for (int e = 0; e <= numEv; ++e)
    {
        const int until = e < numEv ? evs[(size_t) e].sample : n;
        if (until > at) { if (! silent) synth.render (L + at, R + at, until - at, gain); at = until; }
        if (e < numEv)
        {
            const auto& ev = evs[(size_t) e];
            if (ev.on)
            {
                if (! silent) synth.noteOn (ev.pitch, ev.vel / 127.f, ev.glide);
                if (sendMidi) midi.addEvent (juce::MidiMessage::noteOn (1, ev.pitch, (juce::uint8) juce::jlimit (1, 127, ev.vel)), ev.sample);
            }
            else
            {
                synth.noteOff (ev.pitch);
                if (sendMidi) midi.addEvent (juce::MidiMessage::noteOff (1, ev.pitch), ev.sample);
            }
        }
    }
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::isfinite (L[i]) ? juce::jlimit (-1.f, 1.f, L[i]) : 0.f;
        if (R != L) R[i] = std::isfinite (R[i]) ? juce::jlimit (-1.f, 1.f, R[i]) : 0.f;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Play: live notes
// ════════════════════════════════════════════════════════════════════════════
void NovaMelodyProcessor::liveDown (int src, int kind, int idx, int vel, int sample, Ev* evs, int& numEv)
{
    if (! juce::isPositiveAndBelow (src, (int) srcs.size())) return;
    liveUp (src, sample, evs, numEv);                                  // pressed again: retrigger
    const Key k { playTonic.load(), playScale.load() };
    const int oct = (int) p ("playOctave");
    const int style = (int) p ("playStyle");
    Src s;
    auto chordFromTable = [&] (int d)
    {
        d = juce::jlimit (0, 6, d);
        for (int i = 0; i < 4; ++i) s.pitch[s.n++] = chordTab[(size_t) (d * 4 + i)].load();
        s.chord = true;
        chordNow.store (d);
    };
    if (style == 2 || kind == 3)
    {
        // Tap the melody: any key plays the next note of the chosen melody
        std::shared_ptr<const std::vector<MNote>> list;
        { const juce::SpinLock::ScopedLockType sl (playLock); list = tapList; }
        if (list == nullptr || list->empty()) return;
        const int i = tapIdx.load() % (int) list->size();
        tapIdx.store (i + 1);
        s.pitch[s.n++] = juce::jlimit (0, 127, (*list)[(size_t) i].pitch + 12 * oct);
    }
    else if (kind == 0) { s.pitch[s.n++] = padPitch (k, idx, oct); s.pad = idx; }
    else if (kind == 1) chordFromTable (idx);
    else if (kind == 2)
    {
        const int step = whiteKeyStep (idx);
        if (style == 1 && idx < 60) chordFromTable (((step % 7) + 7) % 7);        // left hand: one key = a chord
        else { s.pitch[s.n++] = stepPitch (k, step, oct); s.pad = ((step % 7) + 7) % 7; }
    }
    if (s.n == 0) return;
    const bool strum = s.chord && p ("strum") > 0.5f;
    for (int i = 0; i < s.n; ++i)
    {
        const int delay = strum ? (int) (i * 0.016 * sampleRate) : 0;
        if (delay > 0 && numStrums < (int) strums.size()) strums[(size_t) numStrums++] = { src, s.pitch[i], vel, sample + delay };
        else
        {
            if (numEv < 512) evs[numEv++] = { sample, s.pitch[i], vel, true, false };
            recPush (clockAtBlock + sample * bpsNow, s.pitch[i], vel, true, s.chord);
        }
    }
    srcs[(size_t) src] = s;
    if (s.pad >= 0) padsOnMask.store (padsOnMask.load() | (1 << s.pad));
}

void NovaMelodyProcessor::liveUp (int src, int sample, Ev* evs, int& numEv)
{
    if (! juce::isPositiveAndBelow (src, (int) srcs.size())) return;
    auto& s = srcs[(size_t) src];
    if (s.n == 0) return;
    for (int i = 0; i < numStrums;)
        if (strums[(size_t) i].src == src) strums[(size_t) i] = strums[(size_t) --numStrums]; else ++i;
    for (int i = 0; i < s.n; ++i)
    {
        if (numEv < 512) evs[numEv++] = { sample, s.pitch[i], 0, false, false };
        recPush (clockAtBlock + sample * bpsNow, s.pitch[i], 0, false, s.chord);
    }
    s = {};
    int mask = 0;
    for (auto& o : srcs) if (o.n > 0 && o.pad >= 0) mask |= 1 << o.pad;
    padsOnMask.store (mask);
}

void NovaMelodyProcessor::recPush (double beat, int pitch, int vel, bool on, bool chord)
{
    if (! playRec.load()) return;
    if (chord && (int) p ("recWhat") == 1) return;                         // Melody only
    int s1, n1, s2, n2;
    recFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 == 0) return;
    recEvs[(size_t) (n1 > 0 ? s1 : s2)] = { beat, pitch, vel, on, chord };
    recFifo.finishedWrite (1);
}

void NovaMelodyProcessor::padDown (int kind, int idx, int vel)
{
    const int src = kind == 0 ? 128 + idx : (kind == 1 ? 140 + idx : 150 + idx);
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 == 0) return;
    uiEvs[(size_t) (n1 > 0 ? s1 : s2)] = { src, kind, idx, juce::jlimit (1, 127, vel), true };
    uiFifo.finishedWrite (1);
}

void NovaMelodyProcessor::padUp (int kind, int idx)
{
    const int src = kind == 0 ? 128 + idx : (kind == 1 ? 140 + idx : 150 + idx);
    int s1, n1, s2, n2;
    uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 == 0) return;
    uiEvs[(size_t) (n1 > 0 ? s1 : s2)] = { src, kind, idx, 0, false };
    uiFifo.finishedWrite (1);
}

void NovaMelodyProcessor::timerCallback()
{
    // the key Play uses (picked, or heard from chords played in) and its 7 chords
    const auto k = keyForGenerate();
    const int oct = (int) p ("playOctave");
    if (k.tonic != playTonic.load() || k.scale != playScale.load() || oct != tabOctave)
    {
        tabOctave = oct;
        playTonic.store (k.tonic);
        playScale.store (k.scale);
        for (int d = 0; d < 7; ++d)
        {
            const auto ps = chordPadPitches (k, d, oct);
            for (int i = 0; i < 4; ++i) chordTab[(size_t) (d * 4 + i)].store (ps[(size_t) std::min (i, (int) ps.size() - 1)]);
        }
    }
    drainRecording();
}

void NovaMelodyProcessor::drainRecording()
{
    int s1, n1, s2, n2;
    recFifo.prepareToRead (recFifo.getNumReady(), s1, n1, s2, n2);
    auto take = [&] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& e = recEvs[(size_t) (start + i)];
            if (e.on) openRec[e.pitch] = { e.beat, e.vel * (e.chord ? -1 : 1) };
            else if (auto it = openRec.find (e.pitch); it != openRec.end())
            {
                MNote m;
                m.start = it->second.first; m.len = std::max (0.05, e.beat - it->second.first);
                m.pitch = e.pitch; m.vel = std::abs (it->second.second);
                recNotes.push_back (m);
                recIsChord.push_back (it->second.second < 0);
                openRec.erase (it);
            }
        }
    };
    take (s1, n1); take (s2, n2);
    recFifo.finishedRead (n1 + n2);
}

void NovaMelodyProcessor::startPlayRecording()
{
    drainRecording();
    openRec.clear(); recNotes.clear(); recIsChord.clear();
    recStartBeat.store (-1.0);
    playRec.store (true);
    bump();
}

void NovaMelodyProcessor::stopPlayRecording()
{
    if (! playRec.exchange (false)) return;
    juce::Thread::sleep (30);                        // let the last audio block's events land
    drainRecording();
    for (auto& [pitch, sv] : openRec)                // keys still held: end them now
    {
        MNote m; m.start = sv.first; m.len = std::max (0.05, recNowBeat.load() - sv.first); m.pitch = pitch; m.vel = std::abs (sv.second);
        recNotes.push_back (m); recIsChord.push_back (sv.second < 0);
    }
    openRec.clear();
    const double origin = recStartBeat.load();
    std::vector<MNote> out;
    for (size_t i = 0; i < recNotes.size(); ++i)
    {
        auto m = recNotes[i];
        m.start -= origin;
        if (m.start < -0.05) continue;
        m.start = std::max (0.0, m.start);
        if (p ("fixTiming") > 0.5f)
        {
            const double g = 0.25;
            const double end = std::round ((m.start + m.len) / g) * g;
            m.start = std::round (m.start / g) * g;
            m.len = std::max (g, end - m.start);
        }
        out.push_back (m);
    }
    std::sort (out.begin(), out.end(), [] (const MNote& a, const MNote& b) { return a.start < b.start; });
    double end = 0;
    for (auto& m : out) end = std::max (end, m.start + m.len);
    playTake = out;
    playTakeLen = std::max (4.0, std::ceil (end / 4.0 - 1e-6) * 4.0);
    recStartBeat.store (-1.0);
    publish();
    bump();
}

void NovaMelodyProcessor::clearTake()
{
    playTake.clear();
    playTakeLen = 16;
    publish();
    bump();
}

// ════════════════════════════════════════════════════════════════════════════
// Editor data
// ════════════════════════════════════════════════════════════════════════════
juce::String NovaMelodyProcessor::getStateJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("mode", mode);
    o->setProperty ("selected", selected);
    juce::Array<juce::var> vars;
    for (auto& v : variations) vars.add (notesToVar (v));
    o->setProperty ("vars", vars);
    o->setProperty ("bars", kBarsChoices[juce::jlimit (0, 3, (int) p ("bars"))]);
    o->setProperty ("bpm", bpm.load());

    // key in use on each tab
    const auto gk = keyForGenerate();
    {
        const juce::ScopedLock sl (chordLock);
        float sum = 0; for (float w : pcWeights) sum += w;
        auto* k = new juce::DynamicObject();
        k->setProperty ("tonic", gk.tonic); k->setProperty ("scale", gk.scale); k->setProperty ("name", keyName (gk));
        k->setProperty ("heard", p ("keyAutoGen") > 0.5f && sum > 0.5f);
        k->setProperty ("auto", p ("keyAutoGen") > 0.5f);
        o->setProperty ("genKey", juce::var (k));
        juce::Array<juce::var> ch;
        for (auto& [bar, pcs] : heardChords)
        {
            Chord c; c.root = pcs.front(); c.pcs = pcs;
            // name: a triad from the pitch classes if they make one
            juce::String name = noteName (c.root);
            const bool minor3 = std::find (pcs.begin(), pcs.end(), (c.root + 3) % 12) != pcs.end();
            name << (minor3 ? "m" : "");
            ch.add (juce::Array<juce::var> { bar, name });
        }
        o->setProperty ("chords", ch);
    }

    auto* h = new juce::DynamicObject();
    h->setProperty ("ok", hum.ok);
    h->setProperty ("status", humStatus);
    h->setProperty ("len", hum.lengthBeats);
    h->setProperty ("heard", notesToVar (hum.heard));
    h->setProperty ("notes", notesToVar (humOut));
    h->setProperty ("extra", humExtraMode);
    {
        auto* k = new juce::DynamicObject();
        k->setProperty ("tonic", hum.key.tonic); k->setProperty ("scale", hum.key.scale); k->setProperty ("name", keyName (hum.key));
        k->setProperty ("heard", hum.keyWasHeard && p ("keyAutoHum") > 0.5f);
        k->setProperty ("unsure", hum.keyWasHeard && hum.keyConfidence < 0.3f);
        h->setProperty ("key", juce::var (k));
    }
    juce::Array<juce::var> curve;
    const size_t stride = std::max<size_t> (1, hum.curveBeat.size() / 1500);
    for (size_t i = 0; i < hum.curveBeat.size(); i += stride)
        curve.add (juce::Array<juce::var> { std::round (hum.curveBeat[i] * 1000.f) / 1000.f, std::round (hum.curvePitch[i] * 100.f) / 100.f });
    h->setProperty ("curve", curve);
    o->setProperty ("hum", juce::var (h));
    o->setProperty ("previewing", preview.load());
    o->setProperty ("recording", recording.load());
    o->setProperty ("analysing", analysing.load());
    o->setProperty ("midiName", suggestedMidiName());
    o->setProperty ("take", notesToVar (playTake));
    o->setProperty ("takeLen", playTakeLen);
    {
        auto* k = new juce::DynamicObject();
        k->setProperty ("tonic", playTonic.load()); k->setProperty ("scale", playScale.load());
        juce::Array<juce::var> pads, chordNames, chordPcs;
        const Key pk { playTonic.load(), playScale.load() };
        for (int i = 0; i < 8; ++i) pads.add (padPitch (pk, i, 0));
        for (int d = 0; d < 7; ++d)
        {
            const auto pcs = chordPadClasses (pk, d);
            Chord c; c.root = pcs[0]; c.pcs = pcs;
            chordNames.add (chordName (c));
            juce::Array<juce::var> a; for (int pc : pcs) a.add (pc);
            chordPcs.add (a);
        }
        k->setProperty ("pads", pads); k->setProperty ("chords", chordNames); k->setProperty ("chordPcs", chordPcs);
        o->setProperty ("play", juce::var (k));
    }
    return juce::JSON::toString (juce::var (o), true);
}

juce::String NovaMelodyProcessor::getFrameJson() const
{
    juce::String s ("{\"beat\":");
    s << juce::String (playBeat.load(), 3) << ",\"rec\":" << (recording.load() ? "true" : "false")
      << ",\"recSec\":" << juce::String (captureLen.load() / std::max (1.0, captureRate), 1)
      << ",\"level\":" << juce::String (inputLevel.load(), 3)
      << ",\"bpm\":" << juce::String (bpm.load(), 1)
      << ",\"pads\":" << padsOnMask.load() << ",\"chordNow\":" << chordNow.load() << ",\"tapIdx\":" << tapIdx.load()
      << ",\"playRec\":" << (playRec.load() ? "true" : "false")
      << ",\"recBeats\":" << juce::String (playRec.load() && recStartBeat.load() >= 0 ? recNowBeat.load() - recStartBeat.load() : 0.0, 2)
      << ",\"recCount\":" << (int) recNotes.size() << "}";
    return s;
}

// ════════════════════════════════════════════════════════════════════════════
// State
// ════════════════════════════════════════════════════════════════════════════
void NovaMelodyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto* o = new juce::DynamicObject();
    o->setProperty ("mode", mode);
    o->setProperty ("selected", selected);
    o->setProperty ("seed", (int) seedBase);
    juce::Array<juce::var> vars;
    for (auto& v : variations) vars.add (notesToVar (v));
    o->setProperty ("vars", vars);
    o->setProperty ("humOk", hum.ok);
    o->setProperty ("humHeard", notesToVar (hum.heard));
    juce::Array<juce::var> centres;
    for (float c : hum.heardCentre) centres.add (c);
    o->setProperty ("humCentre", centres);
    juce::Array<juce::var> curve;
    for (size_t i = 0; i < hum.curveBeat.size(); i += 2) curve.add (juce::Array<juce::var> { hum.curveBeat[i], hum.curvePitch[i] });
    o->setProperty ("humCurve", curve);
    o->setProperty ("humLen", hum.lengthBeats);
    o->setProperty ("humKey", juce::Array<juce::var> { hum.key.tonic, hum.key.scale, hum.keyWasHeard ? 1 : 0, hum.keyConfidence });
    o->setProperty ("humExtra", humExtraMode);
    o->setProperty ("take", notesToVar (playTake));
    o->setProperty ("takeLen", playTakeLen);
    {
        const juce::ScopedLock sl (chordLock);
        juce::Array<juce::var> ch;
        for (auto& [bar, pcs] : heardChords)
        {
            juce::Array<juce::var> a { bar };
            for (int pc : pcs) a.add (pc);
            ch.add (a);
        }
        o->setProperty ("chords", ch);
        juce::Array<juce::var> w;
        for (float x : pcWeights) w.add (x);
        o->setProperty ("pcWeights", w);
    }
    state.setProperty ("melody", juce::JSON::toString (juce::var (o), true), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void NovaMelodyProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto json = juce::JSON::parse (tree.getProperty ("melody").toString());
    tree.removeProperty ("melody", nullptr);
    apvts.replaceState (tree);
    if (auto* o = json.getDynamicObject())
    {
        mode = (int) o->getProperty ("mode");
        selected = juce::jlimit (0, 3, (int) o->getProperty ("selected"));
        seedBase = (juce::uint32) (int) o->getProperty ("seed");
        if (auto* a = o->getProperty ("vars").getArray())
            for (int i = 0; i < std::min (4, a->size()); ++i) variations[(size_t) i] = notesFromVar ((*a)[i]);
        hum = {};
        hum.ok = (bool) o->getProperty ("humOk");
        hum.heard = notesFromVar (o->getProperty ("humHeard"));
        if (auto* c = o->getProperty ("humCentre").getArray()) for (auto& x : *c) hum.heardCentre.push_back ((float) (double) x);
        if (auto* c = o->getProperty ("humCurve").getArray())
            for (auto& x : *c) if (auto* pr = x.getArray(); pr != nullptr && pr->size() >= 2) { hum.curveBeat.push_back ((float) (double) (*pr)[0]); hum.curvePitch.push_back ((float) (double) (*pr)[1]); }
        hum.lengthBeats = (double) o->getProperty ("humLen");
        if (auto* k = o->getProperty ("humKey").getArray(); k != nullptr && k->size() >= 4)
        {
            hum.key = { (int) (*k)[0], (int) (*k)[1] };
            hum.keyWasHeard = (int) (*k)[2] != 0;
            hum.keyConfidence = (float) (double) (*k)[3];
        }
        humExtraMode = (int) o->getProperty ("humExtra");
        playTake = notesFromVar (o->getProperty ("take"));
        playTakeLen = o->hasProperty ("takeLen") ? (double) o->getProperty ("takeLen") : 16.0;
        modeNow.store (mode);
        {
            const juce::ScopedLock sl (chordLock);
            heardChords.clear();
            if (auto* ch = o->getProperty ("chords").getArray())
                for (auto& c : *ch)
                    if (auto* a = c.getArray(); a != nullptr && a->size() >= 2)
                    {
                        std::vector<int> pcs;
                        for (int i = 1; i < a->size(); ++i) pcs.push_back ((int) (*a)[i]);
                        heardChords[(int) (*a)[0]] = pcs;
                    }
            if (auto* w = o->getProperty ("pcWeights").getArray())
                for (int i = 0; i < std::min (12, w->size()); ++i) pcWeights[(size_t) i] = (float) (double) (*w)[i];
        }
        humStatus = hum.ok ? "Heard " + juce::String ((int) hum.heard.size()) + " notes" : juce::String();
        remakeHum();
    }
    publish();
    bump();
}

juce::AudioProcessorEditor* NovaMelodyProcessor::createEditor() { return new NovaMelodyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaMelodyProcessor(); }
