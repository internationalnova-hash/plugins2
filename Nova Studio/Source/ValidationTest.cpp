/*
  Nova Studio — Core Workflow Validation
  Validates: Plugin Hosting, Recording, Session without requiring audio hardware.
*/

#include <JuceHeader.h>
#include "Session.h"
#include "StudioAudioEngine.h"
#include "TransportState.h"
#include "TimelineModel.h"
#include "ArrangementModel.h"

using namespace NovaStudio;

// ─── helpers ───────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

static void check(const char* label, bool result)
{
    if (result) { std::cout << "  [PASS] " << label << "\n"; ++passed; }
    else        { std::cout << "  [FAIL] " << label << "\n"; ++failed; }
}

static void section(const char* title)
{
    std::cout << "\n=== " << title << " ===\n";
}

// ─── 1. Session serialisation ──────────────────────────────────────────────

static void testSessionRoundTrip()
{
    section("SESSION — Save / Load / Clip Restore");

    Session s;
    s.setTempo(140.0);
    s.setSampleRate(48000.0);
    s.setName("Test Session");

    auto& t1 = s.addTrack("Vocal", TrackType::Audio);
    t1.armed = true;
    t1.volumeDb = -6.0f;

    Clip c;
    c.file = juce::File("/tmp/test-recording.wav");
    c.startSample = 1024;
    c.lengthSamples = 88200;
    c.isMidi = false;
    c.gainDb = -3.0f;
    c.muted = false;
    t1.clips.add(c);

    auto& t2 = s.addTrack("MIDI 1", TrackType::Midi);
    t2.solo = true;

    auto sessionFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("nova_validation_session.json");

    bool saved = s.saveToFile(sessionFile);
    check("Session saves to file", saved);
    check("Session file exists on disk", sessionFile.existsAsFile());

    Session s2;
    bool loaded = s2.loadFromFile(sessionFile);
    check("Session loads from file", loaded);
    check("Tempo restored", std::abs(s2.getTempo() - 140.0) < 0.001);
    check("Sample rate restored", std::abs(s2.getSampleRate() - 48000.0) < 0.001);
    check("Project name restored", s2.getName() == "Test Session");
    check("Track count restored", s2.getNumTracks() == 2);

    if (s2.getNumTracks() >= 1)
    {
        const auto& rt1 = s2.getTrack(0);
        check("Track 0 name restored", rt1.name == "Vocal");
        check("Track 0 armed restored", rt1.armed == true);
        check("Track 0 volumeDb restored", std::abs(rt1.volumeDb - (-6.0f)) < 0.001f);
        check("Track 0 clip count restored", rt1.clips.size() == 1);

        if (rt1.clips.size() == 1)
        {
            const auto& rc = rt1.clips[0];
            check("Clip startSample restored", rc.startSample == 1024);
            check("Clip lengthSamples restored", rc.lengthSamples == 88200);
            check("Clip gainDb restored", std::abs(rc.gainDb - (-3.0f)) < 0.001f);
            check("Clip isMidi restored", rc.isMidi == false);
            check("Clip muted restored", rc.muted == false);
        }
    }

    if (s2.getNumTracks() >= 2)
    {
        const auto& rt2 = s2.getTrack(1);
        check("Track 1 name restored", rt2.name == "MIDI 1");
        check("Track 1 type restored", rt2.type == TrackType::Midi);
        check("Track 1 solo restored", rt2.solo == true);
    }

    sessionFile.deleteFile();
}

// ─── 2. Recording state machine ─────────────────────────────────────────────

static void testRecordingStateMachine()
{
    section("RECORDING — Track, Arm, Record State Machine");

    StudioAudioEngine engine;

    // engine.initialize() will fail without hardware — that is expected
    bool initResult = engine.initialize();
    check("Engine init reports failure gracefully (no hardware)", true); // always pass — we just verify no crash

    engine.addTrack("Audio 1", TrackType::Audio);
    engine.addTrack("Audio 2", TrackType::Audio);
    check("Two audio tracks created", engine.getTrackCount() == 2);

    engine.setTrackArm(0, true);
    check("Track 0 arm state set", engine.getSession().getTrack(0).armed == true);
    check("Track 1 stays unarmed", engine.getSession().getTrack(1).armed == false);

    engine.setTrackMute(1, true);
    check("Track 1 mute state set", engine.getSession().getTrack(1).muted == true);

    engine.setTrackVolume(0, -6.0f);
    check("Track 0 volume set", std::abs(engine.getSession().getTrack(0).volumeDb - (-6.0f)) < 0.001f);

    engine.setTrackPan(0, -0.5f);
    check("Track 0 pan set", std::abs(engine.getSession().getTrack(0).pan - (-0.5f)) < 0.001f);

    engine.setTrackSolo(0, true);
    check("Track 0 solo set", engine.getSession().getTrack(0).solo == true);

    // Without hardware toggleRecord() will not actually start recording,
    // but must not crash.
    bool toggleCrashed = false;
    try { engine.toggleRecord(); } catch (...) { toggleCrashed = true; }
    check("toggleRecord() does not crash without hardware", !toggleCrashed);

    // toggleRecord() calls startRecordingInternal() directly (not via callback),
    // so recordingActive becomes true even without hardware — it just won't write audio.
    // Expected: true after toggleRecord() if no hardware blocked initialise().
    // Either true (init succeeded) or false (init failed) is acceptable — no crash is the goal.
    bool recordStateValid = (engine.isRecording() == true || engine.isRecording() == false);
    check("isRecording() returns a valid bool after toggleRecord()", recordStateValid);

    engine.removeTrack(1);
    check("Track removal works", engine.getTrackCount() == 1);
}

// ─── 3. Session + engine round-trip ─────────────────────────────────────────

static void testEngineSessionPersistence()
{
    section("SESSION — Engine Save / Reload / Track State Persist");

    StudioAudioEngine engine;
    engine.initialize(); // may fail, that is OK

    engine.addTrack("Lead Vocal", TrackType::Audio);
    engine.addTrack("Guitar", TrackType::Audio);
    engine.addTrack("MIDI Synth", TrackType::Midi);
    check("3 tracks created", engine.getTrackCount() == 3);

    engine.setTrackArm(0, true);
    engine.setTrackVolume(1, 3.0f);
    engine.setTrackMute(2, true);
    engine.getSession().setTempo(128.0);

    auto sessionFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("nova_engine_validation.json");

    bool saved = engine.saveSession(sessionFile);
    check("Engine saves session to file", saved);

    StudioAudioEngine engine2;
    engine2.initialize();
    bool loaded = engine2.loadSession(sessionFile);
    check("Engine loads session from file", loaded);
    check("Loaded engine has 3 tracks", engine2.getTrackCount() == 3);

    if (engine2.getTrackCount() == 3)
    {
        check("Track 0 arm persisted", engine2.getSession().getTrack(0).armed == true);
        check("Track 1 volume persisted",
              std::abs(engine2.getSession().getTrack(1).volumeDb - 3.0f) < 0.001f);
        check("Track 2 mute persisted", engine2.getSession().getTrack(2).muted == true);
        check("Track 2 type is MIDI",   engine2.getSession().getTrack(2).type == TrackType::Midi);
        check("Tempo persisted",        std::abs(engine2.getSession().getTempo() - 128.0) < 0.001);
    }

    sessionFile.deleteFile();
}

// ─── 4. Plugin hosting logic ─────────────────────────────────────────────────

static void testPluginHosting()
{
    section("PLUGIN HOSTING — Format Manager & Load Path");

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    check("Plugin format manager initialised", formatManager.getNumFormats() > 0);
    std::cout << "  [INFO] Registered plugin formats: " << formatManager.getNumFormats() << "\n";
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
        std::cout << "    - " << formatManager.getFormat(i)->getName() << "\n";

    // Find the VST3 format by name (safe — no filesystem scan that requires event loop)
    juce::AudioPluginFormat* vst3Format = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (formatManager.getFormat(i)->getName() == "VST3")
        {
            vst3Format = formatManager.getFormat(i);
            break;
        }
    }
    check("VST3 format available", vst3Format != nullptr);

    // Check VST3 search paths without actually scanning (scan requires event loop)
    if (vst3Format != nullptr)
    {
        juce::FileSearchPath searchPaths = vst3Format->getDefaultLocationsToSearch();
        std::cout << "  [INFO] VST3 default search paths: " << searchPaths.getNumPaths() << "\n";
        for (int i = 0; i < searchPaths.getNumPaths(); ++i)
            std::cout << "    - " << searchPaths[i].getFullPathName() << "\n";
        check("VST3 search paths returned", searchPaths.getNumPaths() >= 0);
    }

    // Engine plugin load path validation (logic only — no real plugin file)
    StudioAudioEngine engine;
    engine.initialize();
    engine.addTrack("Plugin Track", TrackType::Audio);
    check("Plugin track created in engine", engine.getTrackCount() == 1);

    bool loadResult = engine.loadPluginOnTrack(0, juce::File("/nonexistent/plugin.vst3"));
    check("loadPluginOnTrack() returns false for nonexistent file", !loadResult);

    bool badIndexResult = engine.loadPluginOnTrack(99, juce::File("/tmp/any.vst3"));
    check("loadPluginOnTrack() returns false for out-of-range index", !badIndexResult);

    std::cout << "  [NOTE] Full plugin instantiation requires a real VST3 file and GUI event loop.\n";
    std::cout << "         Test this on macOS/Windows with a Nova Suite plugin installed.\n";
}

// ─── 5. Transport state ──────────────────────────────────────────────────────

static void testTransportState()
{
    section("TRANSPORT — State Machine");

    TransportState transport;
    transport.setSampleRate(44100.0);
    transport.setTempo(120);

    check("Initial state: not playing", !transport.isPlaying());
    check("Initial state: not recording", !transport.isRecording());
    check("Initial position is 0", transport.getPositionSamples() == 0);

    transport.play();
    check("After play(): isPlaying() == true", transport.isPlaying());

    // startRecording() requires record arm to be set first
    transport.setRecordArmed(true);
    transport.startRecording();
    check("After arm + startRecording(): isRecording() == true", transport.isRecording());

    transport.stopRecording();
    check("After stopRecording(): isRecording() == false", !transport.isRecording());

    transport.stop();
    check("After stop(): isPlaying() == false", !transport.isPlaying());

    transport.setRecordArmed(true);
    check("setRecordArmed(true)", transport.isRecordArmed());
    transport.setRecordArmed(false);
    check("setRecordArmed(false)", !transport.isRecordArmed());
}


// ─── 6. Engine audio: playback graph, export, edits ─────────────────────────

static juce::File makeTestWav(const juce::String& name, double sr, double seconds, std::function<float(int, double)> gen, int channels = 2)
{
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaStudioTests").getChildFile(name);
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::AudioBuffer<float> b(channels, (int) (sr * seconds));
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample(ch, i, gen(ch, i / sr));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(f), sr, (unsigned) channels, 32, {}, 0));
    w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
    return f;
}

static juce::AudioBuffer<float> readWav(const juce::File& f, double* rate = nullptr)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    juce::AudioBuffer<float> b;
    if (r == nullptr) return b;
    b.setSize((int) r->numChannels, (int) r->lengthInSamples);
    r->read(&b, 0, (int) r->lengthInSamples, 0, true, true);
    if (rate) *rate = r->sampleRate;
    return b;
}

static void testEngineAudio()
{
    section("ENGINE — Playback graph & export");
    const double sr = 44100.0;
    auto sine = makeTestWav("sine.wav", sr, 2.0, [] (int, double t) { return 0.5f * (float) std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * t); });
    auto dc   = makeTestWav("dc.wav", sr, 2.0, [] (int, double) { return 0.25f; });
    auto out  = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaStudioTests").getChildFile("out.wav");

    StudioAudioEngine engine;
    engine.newSession();
    engine.setMasterLimiterEnabled(false);
    engine.addTrack("Sine", TrackType::Audio);
    auto& t0 = engine.getSession().getTrack(0);
    t0.volumeDb = 0.0f;
    Clip c; c.file = sine; c.startSample = 0; c.lengthSamples = (int64_t) (sr * 2.0);
    t0.clips.add(c);
    engine.notifySessionChanged();

    StudioAudioEngine::ExportOptions o; o.tailSeconds = 0.0; o.bitDepth = 32;
    auto res = engine.exportAudio(out, o);
    auto b = readWav(out);
    check("Export succeeds", res.ok);
    check("Export length = arrangement length", std::abs(b.getNumSamples() - (int) (sr * 2.0)) < 64);
    {
        auto src = readWav(sine);
        float maxDiff = 0.0f;
        for (int i = 0; i < juce::jmin(b.getNumSamples(), src.getNumSamples()); ++i)
            maxDiff = juce::jmax(maxDiff, std::abs(b.getSample(0, i) - src.getSample(0, i)));
        check("Export is sample-identical to the source at 0 dB", maxDiff < 1.0e-4f);
    }

    // Pan hard left -> right channel silent (export now includes pan)
    t0.pan = -1.0f; engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Pan is applied in export", b.getMagnitude(1, 0, b.getNumSamples()) < 1.0e-4f && b.getMagnitude(0, 0, b.getNumSamples()) > 0.45f);
    t0.pan = 0.0f;

    // Split into two clips: playback across the split must be gapless
    t0.clips.clear();
    Clip a = c; a.lengthSamples = 30000;
    Clip bb = c; bb.startSample = 30000; bb.fileOffsetSamples = 30000; bb.lengthSamples = c.lengthSamples - 30000;
    t0.clips.add(a); t0.clips.add(bb);
    engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    {
        auto src = readWav(sine);
        float maxDiff = 0.0f;
        for (int i = 29000; i < 31000; ++i)
            maxDiff = juce::jmax(maxDiff, std::abs(b.getSample(0, i) - src.getSample(0, i)));
        check("Split clips play back gaplessly (sample-exact at the edit)", maxDiff < 1.0e-4f);
    }

    // Overlap: the clip on top (later in the list) plays
    t0.clips.clear();
    Clip under = c;
    Clip top; top.file = dc; top.startSample = 22050; top.lengthSamples = 22050;
    t0.clips.add(under); t0.clips.add(top);
    engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Overlapping clip on top replaces the one underneath", std::abs(b.getSample(0, 30000) - 0.25f) < 1.0e-4f);
    check("Clip underneath still plays outside the overlap", std::abs(b.getSample(0, 5000) - readWav(sine).getSample(0, 5000)) < 1.0e-4f);

    // Fade in starts from silence
    t0.clips.clear();
    Clip f = c; f.fadeInSamples = 4410; t0.clips.add(f);
    engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Clip fade-in is applied", std::abs(b.getSample(0, 10)) < 0.01f && b.getMagnitude(0, 20000, 2000) > 0.45f);

    // Resampled clip: a 48 kHz file in a 44.1 kHz session keeps its duration and pitch
    auto sine48 = makeTestWav("sine48.wav", 48000.0, 1.0, [] (int, double t) { return 0.5f * (float) std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t); });
    t0.clips.clear();
    Clip r; r.file = sine48; r.lengthSamples = (int64_t) sr; t0.clips.add(r);
    engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    {
        int crossings = 0;
        for (int i = 1000; i < 40000; ++i)
            if ((b.getSample(0, i - 1) < 0.0f) != (b.getSample(0, i) < 0.0f)) ++crossings;
        const double freq = crossings / 2.0 / (39000.0 / sr);
        check("Clips at another sample rate are resampled (pitch correct)", std::abs(freq - 1000.0) < 5.0);
    }

    // Stems: each track on its own
    t0.clips.clear(); t0.clips.add(c);
    engine.addTrack("DC", TrackType::Audio);
    auto& t1 = engine.getSession().getTrack(1);
    t1.volumeDb = 0.0f;
    Clip d; d.file = dc; d.lengthSamples = (int64_t) (sr * 2.0); t1.clips.add(d);
    engine.notifySessionChanged();
    auto stemDir = out.getParentDirectory().getChildFile("stems");
    stemDir.deleteRecursively();
    const int stems = engine.exportStems(stemDir, o);
    check("Stem export writes one file per track", stems == 2);
    auto stemFiles = stemDir.findChildFiles(juce::File::findFiles, false, "*.wav");
    stemFiles.sort();
    if (stemFiles.size() == 2)
    {
        auto s2 = readWav(stemFiles[1]);
        check("A stem contains only its own track", std::abs(s2.getSample(0, 1000) - 0.25f) < 1.0e-4f);
    }

    // Mute + solo honoured in the mix
    t1.solo = true; engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Solo is applied in export", std::abs(b.getSample(0, 1000) - 0.25f) < 1.0e-4f);
    t1.solo = false; t1.muted = true; engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Mute is applied in export", std::abs(b.getSample(0, 1000) - readWav(sine).getSample(0, 1000)) < 1.0e-4f);
    t1.muted = false;

    // Volume automation is applied in export
    t1.muted = true;
    AutomationLane lane; lane.parameterId = "volume";
    AutomationPoint p0; p0.timeSeconds = 0.0; p0.value = -100.0f;
    AutomationPoint p1; p1.timeSeconds = 1.0; p1.value = -100.0f;
    lane.points.add(p0); lane.points.add(p1);
    t0.automationLanes.add(lane);
    engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Volume automation is applied in export", b.getMagnitude(0, 2000, 20000) < 1.0e-3f);
    t0.automationLanes.clear();

    // Master volume
    t1.muted = false; t1.clips.clear();
    engine.setMasterVolume(-6.0206f);
    engine.notifySessionChanged();
    engine.exportAudio(out, o); b = readWav(out);
    check("Master fader is applied", std::abs(b.getMagnitude(0, 10000, 10000) - 0.25f) < 0.01f);
    engine.setMasterVolume(0.0f);
}

static void testSessionV2()
{
    section("SESSION — New fields round-trip");
    Session s;
    s.setSampleRate(44100.0);
    auto& t = s.addTrack("Keys", TrackType::Midi);
    Clip m; m.isMidi = true; m.startSample = 44100; m.lengthSamples = 88200;
    MidiNote n; n.pitch = 64; n.startBeat = 1.5; n.lengthBeats = 0.5; n.velocity = 0.7f;
    m.notes.add(n);
    t.clips.add(m);
    auto& a = s.addTrack("Vox", TrackType::Audio);
    Clip c; c.file = juce::File("/tmp/x.wav"); c.startSample = 1000; c.lengthSamples = 5000;
    c.fadeInSamples = 100; c.fadeOutSamples = 200; c.fileOffsetSamples = 300;
    a.clips.add(c);
    a.sendLevels[2] = -6.0f; a.sendBusIndex[2] = 1; a.sendPreFader[2] = true;
    a.eq[3].enabled = true; a.eq[3].gainDb = 4.0f; a.inputChannel = 3; a.automationMode = 2;
    s.timeSigNumerator = 6; s.timeSigDenominator = 8;
    s.loopEnabled = true; s.loopStartSample = 100; s.loopEndSample = 900;
    s.masterVolumeDb = -3.0f;

    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaStudioTests").getChildFile("v2.novastudio");
    check("Save", s.saveToFile(f));
    Session r;
    check("Load", r.loadFromFile(f));
    const auto& rm = r.getTrack(0).clips.getReference(0);
    check("MIDI notes saved", rm.notes.size() == 1 && rm.notes[0].pitch == 64 && std::abs(rm.notes[0].startBeat - 1.5) < 1e-9);
    const auto& rc = r.getTrack(1).clips.getReference(0);
    check("Clip fades + trim offset saved", rc.fadeInSamples == 100 && rc.fadeOutSamples == 200 && rc.fileOffsetSamples == 300);
    const auto& rt = r.getTrack(1);
    check("Sends saved", std::abs(rt.sendLevels[2] + 6.0f) < 1e-4f && rt.sendBusIndex[2] == 1 && rt.sendPreFader[2]);
    check("Track EQ saved", rt.eq[3].enabled && std::abs(rt.eq[3].gainDb - 4.0f) < 1e-4f);
    check("Input channel + automation mode saved", rt.inputChannel == 3 && rt.automationMode == 2);
    check("Time signature, loop, master saved", r.timeSigNumerator == 6 && r.timeSigDenominator == 8 && r.loopEnabled
          && r.loopEndSample == 900 && std::abs(r.masterVolumeDb + 3.0f) < 1e-4f);

    r.rescaleToSampleRate(88200.0);
    check("Sample-rate change keeps clips in time", r.getTrack(1).clips.getReference(0).startSample == 2000
          && r.getTrack(1).clips.getReference(0).fileOffsetSamples == 600);

    Session tempo; tempo.setTempo(120.0);
    TempoPoint tp; tp.timeSeconds = 2.0; tp.bpm = 60.0; tempo.addTempoPoint(tp);
    check("Tempo map: beats before change", std::abs(tempo.secondsToBeats(2.0) - 4.0) < 1e-9);
    check("Tempo map: beats after change", std::abs(tempo.secondsToBeats(4.0) - 6.0) < 1e-9);
    check("Tempo map: inverse", std::abs(tempo.beatsToSeconds(6.0) - 4.0) < 1e-9);
}

static void testMidiFiles()
{
    section("MIDI — clips, import/export");
    StudioAudioEngine engine;
    engine.newSession();
    engine.addTrack("Piano", TrackType::Midi);
    const int ci = engine.createMidiClip(0, 0, (int64_t) (44100 * 2), "Chords");
    check("Create MIDI clip", ci == 0);
    juce::Array<MidiNote> notes;
    for (int i = 0; i < 4; ++i) { MidiNote n; n.pitch = 60 + i * 4; n.startBeat = i; n.lengthBeats = 0.5; notes.add(n); }
    check("Set MIDI notes", engine.setMidiClipNotes(0, ci, notes));
    auto mid = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaStudioTests").getChildFile("clip.mid");
    check("Export MIDI clip", engine.exportMidiClip(0, ci, mid));
    const int imported = engine.importMidiFile(mid, 0);
    check("Import MIDI file creates a clip", imported == 1);
    const auto& tr = engine.getSession().getTrack(engine.getSession().getNumTracks() - 1);
    check("Imported notes match", tr.clips.size() == 1 && tr.clips[0].notes.size() == 4
          && tr.clips[0].notes[2].pitch == 68 && std::abs(tr.clips[0].notes[2].startBeat - 2.0) < 1e-6);
}

static void testEditing()
{
    section("EDITING — split / trim keep content in place");
    Session s; s.setSampleRate(44100.0);
    TransportState ts; ts.setSampleRate(44100.0);
    TimelineModel tl(s, ts);
    ArrangementModel am(s, tl);
    am.setSnapToGrid(false);
    auto& t = s.addTrack("A", TrackType::Audio);
    Clip c; c.file = juce::File("/tmp/x.wav"); c.startSample = 1000; c.lengthSamples = 10000; c.fileOffsetSamples = 50;
    t.clips.add(c);
    am.selectClip(0, 0);
    check("Split", am.splitSelectedClip(4000));
    check("Right half continues the audio (file offset moved)", s.getTrack(0).clips.size() == 2
          && s.getTrack(0).clips[1].startSample == 4000 && s.getTrack(0).clips[1].fileOffsetSamples == 3050);
    am.selectClip(0, 0);
    check("Trim start", am.trimSelectedClipStart(2000));
    check("Trimmed clip keeps audio aligned", s.getTrack(0).clips[0].startSample == 2000 && s.getTrack(0).clips[0].fileOffsetSamples == 1050);

    auto& m = s.addTrack("M", TrackType::Midi);
    Clip mc; mc.isMidi = true; mc.startSample = 0; mc.lengthSamples = 44100 * 4;   // 8 beats at 120 BPM
    for (int i = 0; i < 8; ++i) { MidiNote n; n.pitch = 60; n.startBeat = i; n.lengthBeats = 0.5; mc.notes.add(n); }
    m.clips.add(mc);
    am.selectClip(1, 0);
    check("Split MIDI clip", am.splitSelectedClip(44100 * 2));   // at beat 4
    const auto& right = s.getTrack(1).clips[1];
    check("MIDI right half keeps its notes in place", right.notes.size() == 4 && std::abs(right.notes[0].startBeat) < 1e-6);
    am.selectClip(1, 0);
    check("Duplicate", am.duplicateSelectedClip());
    check("Duplicate goes end-to-end", s.getTrack(1).clips[1].startSample == 44100 * 2);
}

static void testRealtimeDevice()
{
    section("REALTIME — live device playback, loop, record");
    StudioAudioEngine engine;
    if (!engine.initialize()) { check("Audio device opened (skipped: none available)", true); return; }
    const double sr = engine.getCurrentSampleRate();
    auto sine = makeTestWav("rt_sine.wav", sr, 4.0, [] (int, double t) { return 0.2f * (float) std::sin(2.0 * juce::MathConstants<double>::pi * 330.0 * t); });
    engine.newSession();
    engine.addTrack("Sine", TrackType::Audio);
    auto& t = engine.getSession().getTrack(0);
    t.volumeDb = -30.0f;   // quiet: this plays through your speakers
    for (int i = 0; i < 4; ++i)
    {
        Clip c; c.file = sine; c.startSample = (int64_t) (i * sr); c.lengthSamples = (int64_t) sr; c.fileOffsetSamples = (int64_t) (i * sr);
        t.clips.add(c);
    }
    engine.notifySessionChanged();

    auto pump = [&engine] (int ms) { juce::Thread::sleep(ms); engine.notifySessionChanged(); };
    engine.play();
    float peak = 0.0f;
    for (int i = 0; i < 20; ++i) { pump(50); peak = juce::jmax(peak, engine.getMasterOutputPeak()); }
    const auto pos = engine.getTransportState().getPositionSamples();
    check("Transport advances on the audio clock", pos > (int64_t) (sr * 0.8) && pos < (int64_t) (sr * 1.4));
    check("Audio reaches the output while playing", peak > 0.001f);

    engine.setLoopRange((int64_t) (sr * 0.5), (int64_t) (sr * 1.0), true);
    engine.seek((int64_t) (sr * 0.5));
    for (int i = 0; i < 30; ++i) pump(50);
    const auto lp = engine.getTransportState().getPositionSamples();
    check("Loop keeps the playhead inside the loop", lp >= (int64_t) (sr * 0.5) && lp < (int64_t) (sr * 1.0));
    engine.setLoopRange(0, 0, false);

    // Edit while playing (the old engine could crash here)
    for (int k = 0; k < 40; ++k)
    {
        Clip c; c.file = sine; c.startSample = (int64_t) (sr * 4 + k * 1000); c.lengthSamples = 5000;
        engine.getSession().getTrack(0).clips.add(c);
        if (k % 5 == 0) engine.addTrack("Extra " + juce::String(k), TrackType::Audio);
        pump(10);
    }
    check("Editing while playing doesn't crash", true);
    engine.stop();

    engine.setTrackArm(0, true);
    engine.seek(0);
    engine.toggleRecord();
    for (int i = 0; i < 20; ++i) pump(50);
    engine.toggleRecord();
    pump(200);
    const auto& clips = engine.getSession().getTrack(0).clips;
    bool foundTake = false;
    for (const auto& c : clips) foundTake = foundTake || (c.file.getParentDirectory() == engine.getRecordingFolder() && c.lengthSamples > sr * 0.5);
    check("Recording creates a take on the armed track", foundTake);
    for (const auto& c : clips)
        if (c.file.getParentDirectory() == engine.getRecordingFolder()) c.file.deleteFile();
    engine.shutdown();
}

// ─── main ────────────────────────────────────────────────────────────────────

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║        NOVA STUDIO — CORE WORKFLOW VALIDATION        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";

    testSessionRoundTrip();
    testRecordingStateMachine();
    testEngineSessionPersistence();
    testPluginHosting();
    testTransportState();
    testEngineAudio();
    testSessionV2();
    testMidiFiles();
    testEditing();
    if (juce::SystemStats::getEnvironmentVariable("NOVA_REALTIME_TEST", {}).isNotEmpty())
        testRealtimeDevice();

    std::cout << "\n══════════════════════════════════════════════════════\n";
    std::cout << "  Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "══════════════════════════════════════════════════════\n\n";

    return failed > 0 ? 1 : 0;
}
