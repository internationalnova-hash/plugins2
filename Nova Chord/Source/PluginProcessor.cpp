#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>
#include <numeric>

//==============================================================================
// Note name helpers
static const char* noteNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

static std::string noteName (int midiNote)
{
    return std::string (noteNames[midiNote % 12]);
}

//==============================================================================
NovaChordAudioProcessor::NovaChordAudioProcessor()
    : AudioProcessor (BusesProperties()),   // MIDI effect — no audio buses
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    triggerBuffer.resize (32);
}

NovaChordAudioProcessor::~NovaChordAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
NovaChordAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // key: 0=C, 1=C#, … 11=B
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("key",   "Key",   0, 11, 0));
    // scale: 0=Major, 1=Minor, 2=Dorian, 3=Mixolydian
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("scale", "Scale", 0, 3,  0));
    // style: 0=Pop, 1=Jazz, 2=Classical, 3=Blues
    params.push_back (std::make_unique<juce::AudioParameterInt>   ("style", "Style", 0, 3,  0));

    return { params.begin(), params.end() };
}

//==============================================================================
void NovaChordAudioProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = blockSize;
}

void NovaChordAudioProcessor::releaseResources() {}

//==============================================================================
bool NovaChordAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // MIDI effect — no audio buses expected
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::disabled()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::disabled()) return false;
    return true;
}

//==============================================================================
// Chord quality interval tables
ChordDef NovaChordAudioProcessor::buildChord (int rootMidi, const std::string& quality)
{
    ChordDef def;
    def.rootMidi = rootMidi;
    def.name = noteName (rootMidi) + quality;

    if      (quality == "maj")    def.intervals = {0, 4, 7};
    else if (quality == "min")    def.intervals = {0, 3, 7};
    else if (quality == "7")      def.intervals = {0, 4, 7, 10};
    else if (quality == "maj7")   def.intervals = {0, 4, 7, 11};
    else if (quality == "min7")   def.intervals = {0, 3, 7, 10};
    else if (quality == "dim")    def.intervals = {0, 3, 6};
    else if (quality == "dim7")   def.intervals = {0, 3, 6, 9};
    else if (quality == "m7b5")   def.intervals = {0, 3, 6, 10};
    else if (quality == "aug")    def.intervals = {0, 4, 8};
    else if (quality == "sus2")   def.intervals = {0, 2, 7};
    else if (quality == "sus4")   def.intervals = {0, 5, 7};
    else if (quality == "9")      def.intervals = {0, 4, 7, 10, 14};
    else if (quality == "maj9")   def.intervals = {0, 4, 7, 11, 14};
    else if (quality == "min9")   def.intervals = {0, 3, 7, 10, 14};
    else if (quality == "13")     def.intervals = {0, 4, 7, 10, 14, 21};
    else if (quality == "7b9")    def.intervals = {0, 4, 7, 10, 13};
    else                          def.intervals = {0, 4, 7}; // fallback to maj

    // Build nice display name
    if (quality == "maj") def.name = noteName (rootMidi);
    else                  def.name = noteName (rootMidi) + quality;

    return def;
}

int NovaChordAudioProcessor::detectChordRoot (const std::set<int>& notes)
{
    if (notes.empty()) return -1;
    // Return lowest note's pitch class as root
    return *notes.begin();
}

std::vector<ChordDef> NovaChordAudioProcessor::computeSuggestions (int rootMidi, int scale, int style)
{
    // rootMidi is the lowest detected note; we want its pitch class in octave 5 for building chords
    int rootPc = rootMidi % 12;
    // Anchor all chords in a comfortable mid-range octave (around C4=60)
    int base = 60 + rootPc; // place root in octave 4-5 range
    if (base > 71) base -= 12;

    int key   = (int) apvts.getRawParameterValue ("key")->load();

    // Scale degrees (semitones above key) for diatonic chord roots
    // Major: I ii iii IV V vi vii°
    // Minor: i ii° III iv V VI VII
    // Dorian: i ii III IV v vi° VII
    // Mixolydian: I ii iii° IV v vi VII

    // We'll build a set of 6 suggestions based on scale and style
    std::vector<ChordDef> result;

    auto addChord = [&](int semisAboveKey, const std::string& quality)
    {
        int root = 60 + ((key + semisAboveKey) % 12);
        if (root > 71) root -= 12;
        result.push_back (buildChord (root, quality));
    };

    if (style == 0) // Pop
    {
        if (scale == 0) // Major
        {
            addChord (5,  "maj");   // IV
            addChord (7,  "maj");   // V
            addChord (9,  "min");   // vi
            addChord (4,  "min");   // iii
            addChord (2,  "min");   // ii
            addChord (7,  "7");     // V7
        }
        else if (scale == 1) // Minor
        {
            addChord (3,  "maj");   // III
            addChord (7,  "maj");   // V (harmonic)
            addChord (8,  "maj");   // VI
            addChord (5,  "min");   // iv
            addChord (10, "maj");   // VII
            addChord (7,  "7");     // V7
        }
        else if (scale == 2) // Dorian
        {
            addChord (2,  "min");   // ii
            addChord (5,  "maj");   // IV
            addChord (7,  "min");   // v
            addChord (10, "maj");   // VII
            addChord (3,  "maj");   // III
            addChord (9,  "min");   // vi°->min
        }
        else // Mixolydian
        {
            addChord (5,  "maj");   // IV
            addChord (10, "maj");   // VII (flat)
            addChord (7,  "min");   // v
            addChord (2,  "min");   // ii
            addChord (9,  "min");   // vi
            addChord (5,  "7");     // IV7
        }
    }
    else if (style == 1) // Jazz
    {
        if (scale == 0) // Major
        {
            addChord (5,  "maj7");  // IVmaj7
            addChord (7,  "7");     // V7
            addChord (9,  "min7");  // vi7
            addChord (2,  "min7");  // ii7
            addChord (4,  "min7");  // iii7
            addChord (6,  "7");     // tritone sub of V (bII7 = 6 semis above key)
        }
        else if (scale == 1) // Minor
        {
            addChord (3,  "maj7");  // IIImaj7
            addChord (7,  "7");     // V7
            addChord (8,  "maj7");  // VImaj7
            addChord (5,  "min7");  // iv7
            addChord (2,  "m7b5"); // ii half-dim
            addChord (1,  "7");     // bII7 tritone sub
        }
        else if (scale == 2) // Dorian
        {
            addChord (2,  "min9");  // ii9
            addChord (5,  "9");     // IV9
            addChord (7,  "min9");  // v9
            addChord (0,  "min7");  // i7
            addChord (10, "7");     // VII7
            addChord (4,  "9");     // III9
        }
        else // Mixolydian
        {
            addChord (0,  "9");     // I9
            addChord (5,  "9");     // IV9
            addChord (10, "7");     // VII7 (flat)
            addChord (2,  "min9");  // ii9
            addChord (7,  "min9");  // v9
            addChord (6,  "7");     // tritone sub
        }
    }
    else if (style == 2) // Classical
    {
        if (scale == 0) // Major
        {
            addChord (5,  "maj");   // IV
            addChord (7,  "maj");   // V
            addChord (7,  "7");     // V7
            addChord (9,  "min");   // vi (relative minor)
            addChord (2,  "min");   // ii
            addChord (4,  "dim");   // vii° as leading tone chord... actually iii min
        }
        else if (scale == 1) // Minor
        {
            addChord (7,  "maj");   // V (harmonic)
            addChord (7,  "7");     // V7
            addChord (8,  "maj");   // VI
            addChord (3,  "maj");   // III (relative major)
            addChord (5,  "min");   // iv
            addChord (11, "dim");   // vii° leading tone
        }
        else if (scale == 2) // Dorian
        {
            addChord (5,  "maj");   // IV
            addChord (7,  "min");   // v
            addChord (2,  "min");   // ii
            addChord (10, "maj");   // VII
            addChord (3,  "maj");   // III
            addChord (0,  "min");   // i
        }
        else // Mixolydian
        {
            addChord (5,  "maj");   // IV
            addChord (10, "maj");   // VII
            addChord (7,  "min");   // v
            addChord (0,  "maj");   // I
            addChord (2,  "min");   // ii
            addChord (9,  "min");   // vi
        }
    }
    else // Blues (style==3)
    {
        // All dominant 7ths — classic 12-bar blues style
        addChord (0,  "7");     // I7
        addChord (5,  "7");     // IV7
        addChord (7,  "7");     // V7
        addChord (3,  "7");     // bIII7
        addChord (10, "7");     // bVII7
        addChord (2,  "7");     // II7 (secondary dominant to V)
    }

    // Trim to exactly 6
    if ((int) result.size() > 6) result.resize (6);
    return result;
}

//==============================================================================
void NovaChordAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (buffer);

    // Process incoming MIDI — track active notes
    bool notesChanged = false;
    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            activeNotesMidi.insert (msg.getNoteNumber());
            notesChanged = true;
        }
        else if (msg.isNoteOff())
        {
            activeNotesMidi.erase (msg.getNoteNumber());
            notesChanged = true;
        }
        else if (msg.isAllNotesOff())
        {
            activeNotesMidi.clear();
            notesChanged = true;
        }
    }

    // Clear incoming MIDI — we are generating our own output
    midiMessages.clear();

    if (notesChanged)
    {
        if (activeNotesMidi.empty())
        {
            hasInput.store (false);
            detectedRootNote.store (-1);
        }
        else
        {
            hasInput.store (true);
            int root = detectChordRoot (activeNotesMidi);
            detectedRootNote.store (root);

            int scale = (int) apvts.getRawParameterValue ("scale")->load();
            int style = (int) apvts.getRawParameterValue ("style")->load();

            juce::ScopedLock sl (chordLock);
            suggestions = computeSuggestions (root, scale, style);
        }
    }

    // Countdown note-off for held chord
    if (heldNoteOffSampleCountdown > 0)
    {
        heldNoteOffSampleCountdown -= buffer.getNumSamples();
        if (heldNoteOffSampleCountdown <= 0)
        {
            heldNoteOffSampleCountdown = 0;
            // Send note-offs for held notes
            for (int note : heldNotes)
                midiMessages.addEvent (juce::MidiMessage::noteOff (1, note, (juce::uint8) 0), 0);
            heldNotes.clear();
        }
    }

    // Drain trigger FIFO
    int numReady = triggerFifo.getNumReady();
    if (numReady > 0)
    {
        int start1, size1, start2, size2;
        triggerFifo.prepareToRead (numReady, start1, size1, start2, size2);

        auto processEvent = [&](int idx)
        {
            auto& ev = triggerBuffer[(size_t) idx];
            if (ev.noteOn)
            {
                // First send note-offs for currently held notes
                for (int note : heldNotes)
                    midiMessages.addEvent (juce::MidiMessage::noteOff (1, note, (juce::uint8) 0), 0);
                heldNotes.clear();
                heldNoteOffSampleCountdown = 0;

                // Send note-ons for new chord
                for (int note : ev.midiNotes)
                {
                    midiMessages.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) ev.velocity), 0);
                    heldNotes.push_back (note);
                }
                heldNoteOffSampleCountdown = noteHoldSamples;
            }
        };

        for (int i = start1; i < start1 + size1; ++i) processEvent (i);
        for (int i = start2; i < start2 + size2; ++i) processEvent (i);

        triggerFifo.finishedRead (numReady);
    }
}

//==============================================================================
void NovaChordAudioProcessor::triggerChord (int chordIndex)
{
    juce::ScopedLock sl (chordLock);
    if (chordIndex < 0 || chordIndex >= (int) suggestions.size()) return;

    const auto& chord = suggestions[(size_t) chordIndex];

    // Build MIDI notes (root in octave 4)
    std::vector<int> notes;
    for (int interval : chord.intervals)
    {
        int note = chord.rootMidi + interval;
        note = juce::jlimit (21, 108, note);
        notes.push_back (note);
    }

    {
        int wS1, wN1, wS2, wN2;
        triggerFifo.prepareToWrite (1, wS1, wN1, wS2, wN2);
        if (wN1 > 0)
        {
            triggerBuffer[(size_t) wS1] = { notes, true, 100 };
            triggerFifo.finishedWrite (1);
        }
    }
}

void NovaChordAudioProcessor::triggerProgressionChord (int index)
{
    juce::ScopedLock sl (chordLock);
    if (index < 0 || index >= (int) progression.size()) return;

    const auto& chord = progression[(size_t) index];

    std::vector<int> notes;
    for (int interval : chord.intervals)
    {
        int note = chord.rootMidi + interval;
        note = juce::jlimit (21, 108, note);
        notes.push_back (note);
    }

    {
        int wS1, wN1, wS2, wN2;
        triggerFifo.prepareToWrite (1, wS1, wN1, wS2, wN2);
        if (wN1 > 0)
        {
            triggerBuffer[(size_t) wS1] = { notes, true, 100 };
            triggerFifo.finishedWrite (1);
        }
    }
}

//==============================================================================
void NovaChordAudioProcessor::addToProgression (int suggestedIndex)
{
    juce::ScopedLock sl (chordLock);
    if (suggestedIndex < 0 || suggestedIndex >= (int) suggestions.size()) return;
    if ((int) progression.size() >= 8) return;
    progression.push_back (suggestions[(size_t) suggestedIndex]);
}

void NovaChordAudioProcessor::removeFromProgression (int index)
{
    juce::ScopedLock sl (chordLock);
    if (index < 0 || index >= (int) progression.size()) return;
    progression.erase (progression.begin() + index);
}

void NovaChordAudioProcessor::clearProgression()
{
    juce::ScopedLock sl (chordLock);
    progression.clear();
}

void NovaChordAudioProcessor::setProgressionFromAI (const std::vector<ChordDef>& chords)
{
    juce::ScopedLock sl (chordLock);
    progression.clear();
    for (const auto& c : chords)
    {
        if ((int) progression.size() >= 8) break;
        progression.push_back (c);
    }
}

//==============================================================================
void NovaChordAudioProcessor::exportMidi (const juce::File& file)
{
    juce::ScopedLock sl (chordLock);
    if (progression.empty()) return;

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (480);

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));
    track.addEvent (juce::MidiMessage::tempoMetaEvent (500000)); // 120 BPM

    int ticksPerChord = 480 * 2; // 2 beats per chord
    int tick = 0;

    for (const auto& chord : progression)
    {
        for (int interval : chord.intervals)
        {
            int note = juce::jlimit (21, 108, chord.rootMidi + interval);
            track.addEvent (juce::MidiMessage::noteOn  (1, note, (juce::uint8) 100), tick);
            track.addEvent (juce::MidiMessage::noteOff (1, note, (juce::uint8) 0),   tick + ticksPerChord - 10);
        }
        tick += ticksPerChord;
    }

    midiFile.addTrack (track);

    juce::FileOutputStream out (file);
    if (out.openedOk())
        midiFile.writeTo (out);
}

//==============================================================================
void NovaChordAudioProcessor::setApiKey (const juce::String& key)
{
    apiKey = key;
}

void NovaChordAudioProcessor::generateFromPrompt (const juce::String& prompt,
                                                   std::function<void(const std::vector<ChordDef>&)> callback)
{
    if (apiKey.isEmpty())
    {
        juce::MessageManager::callAsync ([callback]()
        {
            callback ({});
        });
        return;
    }

    // Build request body asking Claude for chord names
    juce::String systemMsg = "You are a music theory expert. When asked for a chord progression, "
                             "respond with ONLY a JSON array of chord names, no explanation. "
                             "Example: [\"Cmaj7\",\"Am7\",\"Dm9\",\"G13\"]. "
                             "Use standard chord notation. Return exactly 6-8 chords.";

    juce::String userMsg = "Give me a chord progression for: " + prompt;

    juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
    bodyObj->setProperty ("model", juce::var ("claude-opus-4-5"));
    bodyObj->setProperty ("max_tokens", juce::var (256));

    juce::Array<juce::var> messages;
    juce::DynamicObject::Ptr msgObj = new juce::DynamicObject();
    msgObj->setProperty ("role", juce::var ("user"));
    msgObj->setProperty ("content", juce::var (userMsg));
    messages.add (juce::var (msgObj.get()));

    bodyObj->setProperty ("system", juce::var (systemMsg));
    bodyObj->setProperty ("messages", juce::var (messages));

    juce::String bodyStr = juce::JSON::toString (juce::var (bodyObj.get()));

    juce::URL url ("https://api.anthropic.com/v1/messages");
    url = url.withPOSTData (bodyStr);

    juce::StringPairArray headers;
    headers.set ("x-api-key",          apiKey);
    headers.set ("anthropic-version",  "2023-06-01");
    headers.set ("content-type",       "application/json");

    // Run async HTTP request on background thread
    auto capturedCallback = callback;
    auto capturedKey = apiKey;

    juce::Thread::launch ([url, headers, capturedCallback, capturedKey]()
    {
        int statusCode = 0;
        juce::StringPairArray responseHeaders;

        auto stream = url.createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                .withExtraHeaders ("x-api-key: "         + capturedKey + "\r\n"
                                   "anthropic-version: 2023-06-01\r\n"
                                   "content-type: application/json")
                .withConnectionTimeoutMs (15000)
                .withStatusCode (&statusCode)
                .withResponseHeaders (&responseHeaders));

        std::vector<ChordDef> results;

        if (stream != nullptr && statusCode == 200)
        {
            juce::String responseBody = stream->readEntireStreamAsString();

            // Parse the Claude API response to extract text content
            auto parsed = juce::JSON::parse (responseBody);
            if (parsed.isObject())
            {
                auto* obj = parsed.getDynamicObject();
                if (obj != nullptr)
                {
                    auto content = obj->getProperty ("content");
                    if (content.isArray() && content.getArray()->size() > 0)
                    {
                        auto firstContent = (*content.getArray())[0];
                        if (firstContent.isObject())
                        {
                            auto* contentObj = firstContent.getDynamicObject();
                            if (contentObj != nullptr)
                            {
                                juce::String text = contentObj->getProperty ("text").toString();

                                // Extract JSON array from the text
                                int arrayStart = text.indexOfChar ('[');
                                int arrayEnd   = text.lastIndexOfChar (']');
                                if (arrayStart >= 0 && arrayEnd > arrayStart)
                                {
                                    juce::String jsonArray = text.substring (arrayStart, arrayEnd + 1);
                                    auto parsedArray = juce::JSON::parse (jsonArray);

                                    if (parsedArray.isArray())
                                    {
                                        for (const auto& item : *parsedArray.getArray())
                                        {
                                            juce::String chordName = item.toString().trim();
                                            if (chordName.isEmpty()) continue;

                                            // Parse chord name: root + quality
                                            juce::String root    = chordName.substring (0, 1);
                                            bool isSharp = chordName.length() > 1 && chordName[1] == '#';
                                            juce::String quality = chordName.substring (isSharp ? 2 : 1);

                                            // Map root name to MIDI note (octave 4)
                                            static const char* noteNms[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                                            juce::String fullRoot = isSharp ? (root + "#") : root;
                                            int rootMidi = 60;
                                            for (int i = 0; i < 12; ++i)
                                            {
                                                if (fullRoot == noteNms[i])
                                                {
                                                    rootMidi = 60 + i;
                                                    break;
                                                }
                                            }

                                            // Map quality string to our internal quality names
                                            juce::String q = quality;
                                            std::string qStd;
                                            if      (q == "maj7" || q == "M7")       qStd = "maj7";
                                            else if (q == "m7" || q == "min7")       qStd = "min7";
                                            else if (q == "7")                       qStd = "7";
                                            else if (q == "m" || q == "min")         qStd = "min";
                                            else if (q == "maj" || q == "M" || q.isEmpty()) qStd = "maj";
                                            else if (q == "dim")                     qStd = "dim";
                                            else if (q == "dim7")                    qStd = "dim7";
                                            else if (q == "m7b5" || q == "min7b5")  qStd = "m7b5";
                                            else if (q == "aug")                     qStd = "aug";
                                            else if (q == "sus2")                    qStd = "sus2";
                                            else if (q == "sus4")                    qStd = "sus4";
                                            else if (q == "9")                       qStd = "9";
                                            else if (q == "maj9")                    qStd = "maj9";
                                            else if (q == "m9" || q == "min9")      qStd = "min9";
                                            else if (q == "13")                      qStd = "13";
                                            else if (q == "7b9")                     qStd = "7b9";
                                            else                                     qStd = "maj";

                                            ChordDef def;
                                            // Use the NovaChordAudioProcessor static helper logic inline
                                            def.rootMidi = rootMidi;
                                            def.name = chordName.toStdString();

                                            if      (qStd == "maj")  def.intervals = {0,4,7};
                                            else if (qStd == "min")  def.intervals = {0,3,7};
                                            else if (qStd == "7")    def.intervals = {0,4,7,10};
                                            else if (qStd == "maj7") def.intervals = {0,4,7,11};
                                            else if (qStd == "min7") def.intervals = {0,3,7,10};
                                            else if (qStd == "dim")  def.intervals = {0,3,6};
                                            else if (qStd == "dim7") def.intervals = {0,3,6,9};
                                            else if (qStd == "m7b5") def.intervals = {0,3,6,10};
                                            else if (qStd == "aug")  def.intervals = {0,4,8};
                                            else if (qStd == "sus2") def.intervals = {0,2,7};
                                            else if (qStd == "sus4") def.intervals = {0,5,7};
                                            else if (qStd == "9")    def.intervals = {0,4,7,10,14};
                                            else if (qStd == "maj9") def.intervals = {0,4,7,11,14};
                                            else if (qStd == "min9") def.intervals = {0,3,7,10,14};
                                            else if (qStd == "13")   def.intervals = {0,4,7,10,14,21};
                                            else if (qStd == "7b9")  def.intervals = {0,4,7,10,13};
                                            else                     def.intervals = {0,4,7};

                                            results.push_back (def);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        juce::MessageManager::callAsync ([capturedCallback, results]()
        {
            capturedCallback (results);
        });
    });
}

//==============================================================================
juce::AudioProcessorEditor* NovaChordAudioProcessor::createEditor()
{
    return new NovaChordAudioProcessorEditor (*this);
}

bool NovaChordAudioProcessor::hasEditor() const { return true; }

const juce::String NovaChordAudioProcessor::getName() const { return JucePlugin_Name; }

//==============================================================================
void NovaChordAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaChordAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaChordAudioProcessor();
}
