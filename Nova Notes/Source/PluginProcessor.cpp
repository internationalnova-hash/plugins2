#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>

using namespace NovaNotes;

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
        return layout;
    }

    juce::MemoryBlock encodeFlac (const std::vector<float>& audio, double sr)
    {
        juce::MemoryBlock mb;
        juce::FlacAudioFormat flac;
        auto* stream = new juce::MemoryOutputStream (mb, false);
        std::unique_ptr<juce::AudioFormatWriter> w (flac.createWriterFor (stream, sr, 1, 24, {}, 0));
        if (w == nullptr) { delete stream; return {}; }
        const float* ch[] = { audio.data() };
        w->writeFromFloatArrays (ch, 1, (int) audio.size());
        w.reset();   // flushes into mb
        return mb;
    }

    std::vector<float> decodeFlac (const juce::MemoryBlock& mb, double& srOut)
    {
        juce::FlacAudioFormat flac;
        std::unique_ptr<juce::AudioFormatReader> r (flac.createReaderFor (new juce::MemoryInputStream (mb, false), true));
        if (r == nullptr) return {};
        srOut = r->sampleRate;
        const int n = (int) r->lengthInSamples;
        juce::AudioBuffer<float> buf (1, n);
        r->read (&buf, 0, n, 0, true, false);
        return std::vector<float> (buf.getReadPointer (0), buf.getReadPointer (0) + n);
    }

    std::vector<int> toIds (const std::vector<int>& v) { return v; }
}

NovaNotesProcessor::NovaNotesProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    startTimerHz (20);
}

NovaNotesProcessor::~NovaNotesProcessor()
{
    alive->store (false);
    stopTimer();
    pool.removeAllJobs (true, 10000);
    live.store (nullptr);
}

bool NovaNotesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void NovaNotesProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    hostRate.store (sampleRate);
}

// ════════════════════════════════════════════════════════════════════════════
// Audio thread
// ════════════════════════════════════════════════════════════════════════════
void NovaNotesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through

    const int N = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    for (int ch = getTotalNumInputChannels(); ch < nCh; ++ch) buffer.clear (ch, 0, N);

    bool playing = false;
    juce::int64 pos = 0;
    double bpm = 0.0, ppq = 0.0;
    bool posValid = false;
    if (auto* ph = getPlayHead())
        if (auto info = ph->getPosition())
        {
            playing = info->getIsPlaying() || info->getIsRecording();
            if (auto t = info->getTimeInSamples()) { pos = *t; posValid = true; }
            if (auto b = info->getBpm()) bpm = *b;
            if (auto q = info->getPpqPosition()) ppq = *q;
        }
    hostPlaying.store (playing && posValid);
    hostPos.store (pos);

    // ── capture ───────────────────────────────────────────────────────────
    int cs = capState.load();
    if (cs == capArmed && playing && posValid)
    {
        capStart.store (pos);
        capLen.store (0);
        capBpm.store (bpm);
        capPpq.store (ppq);
        capState.store (capRecording);
        cs = capRecording;
    }
    if (cs == capRecording)
    {
        const juce::int64 len = capLen.load();
        if (! playing || ! posValid || pos != capStart.load() + len)
        {
            capState.store (capIdle);
            capFinished.store (true);
        }
        else
        {
            const int ready = chunksReady.load (std::memory_order_acquire);
            const float* in0 = buffer.getReadPointer (0);
            const float* in1 = nCh > 1 ? buffer.getReadPointer (1) : in0;
            const float g = nCh > 1 ? 0.5f : 1.f;
            for (int i = 0; i < N; ++i)
            {
                const juce::int64 idx = len + i;
                const int c = (int) (idx >> kChunkBits);
                if (c >= ready)
                {
                    capOverflow.store (true);
                    capLen.store (idx);
                    capState.store (capIdle);
                    capFinished.store (true);
                    return;
                }
                chunks[c][idx & (kChunk - 1)] = (in0[i] + in1[i]) * g;
            }
            capLen.store (len + N);
        }
        return;   // monitor the input while capturing
    }

    if (*apvts.getRawParameterValue ("bypass") > 0.5f) return;

    // ── playback of the edited take, locked to the host timeline ───────────
    auto* pb = live.load (std::memory_order_acquire);
    if (pb == nullptr || ! playing || ! posValid) return;
    const juce::int64 rel = pos - pb->start;
    if (rel + N <= 0 || rel >= pb->length) return;

    const float* src = showOriginal.load() ? pb->take->audio.data() : pb->edited.data();
    for (int i = 0; i < N; ++i)
    {
        const juce::int64 p = rel + i;
        if (p < 0 || p >= pb->length) continue;
        const float edge = (float) std::min (p, (juce::int64) pb->length - 1 - p);
        const float fade = juce::jmin (1.f, edge / 64.f);
        const float s = src[p];
        for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            const float v = d[i] * (1.f - fade) + s * fade;
            d[i] = std::isfinite (v) ? juce::jlimit (-2.f, 2.f, v) : 0.f;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Capture control
// ════════════════════════════════════════════════════════════════════════════
void NovaNotesProcessor::ensureCaptureChunks (int spare)
{
    const int used = (int) (capLen.load() >> kChunkBits) + 1;
    int ready = chunksReady.load();
    while (ready < kMaxChunks && ready < used + spare)
    {
        chunks[ready] = std::make_unique<float[]> ((size_t) kChunk);
        chunksReady.store (++ready, std::memory_order_release);
    }
}

void NovaNotesProcessor::freeCaptureChunks()
{
    if (capState.load() != capIdle) return;
    const int ready = chunksReady.exchange (0);
    for (int i = 0; i < ready; ++i) chunks[i].reset();
}

void NovaNotesProcessor::armCapture()
{
    if (capState.load() != capIdle) return;
    capLen.store (0);
    capOverflow.store (false);
    capFinished.store (false);
    ensureCaptureChunks (24);
    capState.store (capArmed);
    status = "Press play in your DAW to capture";
}

void NovaNotesProcessor::stopCapture()
{
    const int s = capState.exchange (capIdle);
    if (s == capRecording) capFinished.store (true);
    if (s == capArmed) { status = {}; freeCaptureChunks(); }
}

double NovaNotesProcessor::getCapturedSeconds() const noexcept
{
    return (double) capLen.load() / juce::jmax (1.0, hostRate.load());
}

void NovaNotesProcessor::clearTake()
{
    {
        const juce::ScopedLock sl (modelLock);
        take.reset();
        notes.clear();
        undoStack.clear();
        redoStack.clear();
    }
    publish (nullptr);
    ++analysisVersion;
    ++notesVersion;
    status = {};
}

// ════════════════════════════════════════════════════════════════════════════
// Message-thread housekeeping
// ════════════════════════════════════════════════════════════════════════════
void NovaNotesProcessor::timerCallback()
{
    const int cs = capState.load();
    if (cs != capIdle) ensureCaptureChunks (16);

    if (capFinished.exchange (false))
    {
        const juce::int64 len = capLen.load();
        const double sr = hostRate.load();
        if (len >= (juce::int64) (0.5 * sr))
        {
            std::vector<float> audio ((size_t) len);
            for (juce::int64 i = 0; i < len; ++i)
                audio[(size_t) i] = chunks[(size_t) (i >> kChunkBits)][(size_t) (i & (kChunk - 1))];
            status = capOverflow.load() ? "Capture memory full - take shortened" : "Detecting notes...";
            startAnalysis (std::move (audio), sr, capStart.load(), capBpm.load(), capPpq.load(), {}, {}, false);
        }
        else
            status = "Capture too short - play at least half a second";
        freeCaptureChunks();
    }

    // results from the worker
    std::unique_ptr<AnalysisResult> a;
    std::unique_ptr<NotesPlayback> r;
    int rSerial = 0;
    {
        std::lock_guard<std::mutex> lk (pendingLock);
        a = std::move (pendingAnalysis);
        r = std::move (pendingRender);
        rSerial = pendingRenderSerial;
    }
    if (a != nullptr)
    {
        {
            const juce::ScopedLock sl (modelLock);
            take = a->take;
            notes = std::move (a->notes);
            if (! keyFromUser) key = a->key;
            undoStack.clear();
            redoStack.clear();
        }
        analysing.store (false);
        status = {};
        ++analysisVersion;
        notesChanged();
    }
    if (r != nullptr && rSerial == renderSerial)
        publish (std::move (r));

    if (renderDirty && ! rendering.load() && ! analysing.load())
        requestRender();

    // free retired playbacks once the audio thread can't be holding them
    const double now = juce::Time::getMillisecondCounterHiRes();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (auto& p) { return now - p.second > 2000.0; }),
                   retired.end());
}

void NovaNotesProcessor::publish (std::unique_ptr<NotesPlayback> pb)
{
    live.store (pb.get(), std::memory_order_release);
    if (liveOwner != nullptr)
        retired.push_back ({ std::move (liveOwner), juce::Time::getMillisecondCounterHiRes() });
    liveOwner = std::move (pb);
}

void NovaNotesProcessor::startAnalysis (std::vector<float> audio, double sr, juce::int64 hostStart, double bpm, double ppq,
                                        juce::String restoredNotesJson, Key restoredKey, bool keyFromState)
{
    analysing.store (true);
    publish (nullptr);
    ++analysisVersion;
    auto flag = alive;
    pool.addJob ([this, flag, audio = std::move (audio), sr, hostStart, bpm, ppq, restoredNotesJson, restoredKey, keyFromState]() mutable
    {
        auto t = std::make_shared<NotesTake>();
        t->sampleRate = sr;
        t->hostStart = hostStart;
        t->bpm = bpm;
        t->ppqAtStart = ppq;
        // Store first, then analyse exactly what was stored: a reopened session
        // rebuilds the same notes and the same sound, bit for bit.
        t->flac = encodeFlac (audio, sr);
        double storedSr = sr;
        auto stored = decodeFlac (t->flac, storedSr);
        t->audio = stored.size() == audio.size() ? std::move (stored) : std::move (audio);
        t->track = trackPitch (t->audio.data(), (int) t->audio.size(), sr);
        t->epochs = findEpochs (t->audio.data(), (int) t->audio.size(), t->track);

        auto res = std::make_unique<AnalysisResult>();
        res->notes = restoredNotesJson.isNotEmpty() ? notesFromJson (restoredNotesJson) : segmentNotes (t->track);
        res->key = keyFromState ? restoredKey : detectKey (res->notes, t->track);
        res->take = std::move (t);
        if (! flag->load()) return;
        std::lock_guard<std::mutex> lk (pendingLock);
        pendingAnalysis = std::move (res);
    });
}

void NovaNotesProcessor::requestRender()
{
    std::shared_ptr<const NotesTake> t;
    std::vector<Note> snapshot;
    {
        const juce::ScopedLock sl (modelLock);
        t = take;
        snapshot = notes;
    }
    renderDirty = false;
    if (t == nullptr) return;
    rendering.store (true);
    const int serial = ++renderSerial;
    auto flag = alive;
    pool.addJob ([this, flag, t, snapshot, serial]
    {
        auto pb = std::make_unique<NotesPlayback>();
        pb->take = t;
        pb->start = t->hostStart;
        pb->length = (int) t->audio.size();
        pb->edited.resize (t->audio.size());
        render (t->audio.data(), pb->length, t->track, t->epochs, snapshot, pb->edited.data());
        if (! flag->load()) return;
        std::lock_guard<std::mutex> lk (pendingLock);
        pendingRender = std::move (pb);
        pendingRenderSerial = serial;
        rendering.store (false);
    });
}

void NovaNotesProcessor::notesChanged()
{
    ++notesVersion;
    renderDirty = true;
}

// ════════════════════════════════════════════════════════════════════════════
// Editing
// ════════════════════════════════════════════════════════════════════════════
void NovaNotesProcessor::pushUndoSnapshot()
{
    undoStack.push_back (notes);
    if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void NovaNotesProcessor::applyPatches (const juce::var& patches, bool pushUndo)
{
    {
        const juce::ScopedLock sl (modelLock);
        if (pushUndo) pushUndoSnapshot();
        if (auto* arr = patches.getArray())
            for (const auto& p : *arr)
            {
                const int id = (int) p["id"];
                for (auto& n : notes)
                    if (n.id == id) { applyPatch (n, p); break; }
            }
    }
    notesChanged();
}

void NovaNotesProcessor::splitNote (int id, int frame)
{
    {
        const juce::ScopedLock sl (modelLock);
        if (take == nullptr) return;
        pushUndoSnapshot();
        if (NovaNotes::splitNote (notes, id, frame, take->track) < 0) undoStack.pop_back();
    }
    notesChanged();
}

void NovaNotesProcessor::correctPitch (const std::vector<int>& ids, float centerAmt, float driftAmt, int snapMode)
{
    {
        const juce::ScopedLock sl (modelLock);
        pushUndoSnapshot();
        NovaNotes::correctPitch (notes, ids, centerAmt, driftAmt, snapMode == 1 ? 0 : scaleMask (key));
    }
    notesChanged();
}

juce::String NovaNotesProcessor::runAssist (int style, const std::vector<int>& ids)
{
    AssistReport rep;
    {
        const juce::ScopedLock sl (modelLock);
        if (take == nullptr) return {};
        pushUndoSnapshot();
        rep = aiAssist (take->track, notes, toIds (ids), (AssistStyle) juce::jlimit (0, 2, style), key);
    }
    notesChanged();
    juce::String s;
    s << "Tuned " << rep.fixed << " note" << (rep.fixed == 1 ? "" : "s");
    if (rep.keptSlides > 0)  s << " · kept " << rep.keptSlides << " slide" << (rep.keptSlides == 1 ? "" : "s");
    if (rep.keptVibrato > 0) s << " · vibrato kept on " << rep.keptVibrato;
    if (rep.runs > 0)        s << " · " << rep.runs << " fast run note" << (rep.runs == 1 ? "" : "s") << " eased";
    if (rep.leftAlone > 0)   s << " · " << rep.leftAlone << " already in tune";
    return s;
}

void NovaNotesProcessor::quantizeTime (const std::vector<int>& ids, float amount)
{
    {
        const juce::ScopedLock sl (modelLock);
        if (take == nullptr || take->bpm <= 0.0) return;
        pushUndoSnapshot();
        const double spb = 60.0 / take->bpm;
        const double firstBeat = (std::ceil (take->ppqAtStart) - take->ppqAtStart) * spb;
        NovaNotes::quantizeTime (notes, ids, take->track, spb, firstBeat, amount);
    }
    notesChanged();
}

void NovaNotesProcessor::resetNotes (const std::vector<int>& ids)
{
    {
        const juce::ScopedLock sl (modelLock);
        pushUndoSnapshot();
        for (auto& n : notes)
            if (ids.empty() || std::find (ids.begin(), ids.end(), n.id) != ids.end())
            {
                n.pitch = n.center; n.drift = 0.f; n.vibrato = 1.f; n.transition = 0.5f;
                n.formant = 0.f; n.gainDb = 0.f; n.timeShift = 0.f;
            }
    }
    notesChanged();
}

void NovaNotesProcessor::undo()
{
    {
        const juce::ScopedLock sl (modelLock);
        if (undoStack.empty()) return;
        redoStack.push_back (notes);
        notes = std::move (undoStack.back());
        undoStack.pop_back();
    }
    notesChanged();
}

void NovaNotesProcessor::redo()
{
    {
        const juce::ScopedLock sl (modelLock);
        if (redoStack.empty()) return;
        undoStack.push_back (notes);
        notes = std::move (redoStack.back());
        redoStack.pop_back();
    }
    notesChanged();
}

bool NovaNotesProcessor::canUndo() const { const juce::ScopedLock sl (modelLock); return ! undoStack.empty(); }
bool NovaNotesProcessor::canRedo() const { const juce::ScopedLock sl (modelLock); return ! redoStack.empty(); }

void NovaNotesProcessor::setKey (int tonic, bool minor)
{
    {
        const juce::ScopedLock sl (modelLock);
        key.tonic = ((tonic % 12) + 12) % 12;
        key.minor = minor;
        key.confidence = 1.f;
        keyFromUser = true;
    }
    ++notesVersion;
}

// ════════════════════════════════════════════════════════════════════════════
// Queries
// ════════════════════════════════════════════════════════════════════════════
bool NovaNotesProcessor::hasTake() const { const juce::ScopedLock sl (modelLock); return take != nullptr; }

double NovaNotesProcessor::getTakePlayheadSeconds() const noexcept
{
    auto* pb = live.load();
    if (pb == nullptr) return -1.0;
    const juce::int64 rel = hostPos.load() - pb->start;
    if (rel < 0 || rel >= pb->length) return -1.0;
    return (double) rel / pb->take->sampleRate;
}

juce::String NovaNotesProcessor::getStatusMessage() const { return status; }

juce::String NovaNotesProcessor::getTrackJson() const
{
    const juce::ScopedLock sl (modelLock);
    return take != nullptr ? trackToJson (take->track, 2) : juce::String ("null");
}

juce::String NovaNotesProcessor::getNotesJson() const
{
    const juce::ScopedLock sl (modelLock);
    return notesToJson (notes);
}

juce::String NovaNotesProcessor::getInfoJson() const
{
    const juce::ScopedLock sl (modelLock);
    juce::String s;
    s << "{\"tonic\":" << key.tonic << ",\"minor\":" << (key.minor ? "true" : "false")
      << ",\"keyConf\":" << juce::String (key.confidence, 2)
      << ",\"userKey\":" << (keyFromUser ? "true" : "false")
      << ",\"seconds\":" << (take != nullptr ? juce::String ((double) take->audio.size() / take->sampleRate, 3) : "0")
      << ",\"bpm\":" << (take != nullptr ? juce::String (take->bpm, 3) : "0")
      << ",\"ppq\":" << (take != nullptr ? juce::String (take->ppqAtStart, 4) : "0")
      << ",\"srMismatch\":" << (take != nullptr && std::abs (take->sampleRate - currentSampleRate) > 1.0 ? "true" : "false")
      << "}";
    return s;
}

// ════════════════════════════════════════════════════════════════════════════
// Session state: params + the take (FLAC) + every note edit
// ════════════════════════════════════════════════════════════════════════════
void NovaNotesProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("NovaNotes");
    root.setAttribute ("v", 1);
    if (auto params = apvts.copyState().createXml()) root.addChildElement (params.release());

    const juce::ScopedLock sl (modelLock);
    if (take != nullptr && take->flac.getSize() > 0)
    {
        auto* t = root.createNewChildElement ("Take");
        t->setAttribute ("sr", take->sampleRate);
        t->setAttribute ("start", juce::String (take->hostStart));
        t->setAttribute ("bpm", take->bpm);
        t->setAttribute ("ppq", take->ppqAtStart);
        t->setAttribute ("tonic", key.tonic);
        t->setAttribute ("minor", key.minor);
        t->setAttribute ("keyConf", key.confidence);
        t->addTextElement (take->flac.toBase64Encoding());
        root.createNewChildElement ("Notes")->addTextElement (notesToJson (notes));
    }
    root.setAttribute ("userKey", keyFromUser);
    root.setAttribute ("tonic", key.tonic);
    root.setAttribute ("minor", key.minor);
    copyXmlToBinary (root, destData);
}

void NovaNotesProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NovaNotes")) return;
    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    {
        const juce::ScopedLock sl (modelLock);
        keyFromUser = xml->getBoolAttribute ("userKey", false);
        if (keyFromUser) key = { xml->getIntAttribute ("tonic"), xml->getBoolAttribute ("minor"), 1.f };
    }
    ++notesVersion;
    auto* t = xml->getChildByName ("Take");
    if (t == nullptr) return;
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (t->getAllSubText().trim())) return;
    double sr = t->getDoubleAttribute ("sr", 48000.0);
    auto audio = decodeFlac (mb, sr);
    if (audio.empty()) return;
    Key k { t->getIntAttribute ("tonic"), t->getBoolAttribute ("minor"), (float) t->getDoubleAttribute ("keyConf", 0.5) };
    juce::String notesJson;
    if (auto* n = xml->getChildByName ("Notes")) notesJson = n->getAllSubText();
    startAnalysis (std::move (audio), sr, t->getStringAttribute ("start").getLargeIntValue(),
                   t->getDoubleAttribute ("bpm"), t->getDoubleAttribute ("ppq"), notesJson, k, true);
}

juce::AudioProcessorEditor* NovaNotesProcessor::createEditor()
{
    return new NovaNotesEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaNotesProcessor();
}
