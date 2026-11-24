#include "MatchSession.h"
#include "NovaVocalTuner.h"   // estimateNovaKey

namespace nr::match
{

namespace
{
    const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::AudioBuffer<float> resampleTo (const juce::AudioBuffer<float>& in, double fromRate, double toRate)
    {
        if (in.getNumSamples() == 0 || std::abs (fromRate - toRate) < 1.0) return in;
        const double ratio = fromRate / toRate;
        const int outLen = (int) std::floor (in.getNumSamples() / ratio) - 4;
        juce::AudioBuffer<float> out (in.getNumChannels(), juce::jmax (0, outLen));
        for (int c = 0; c < in.getNumChannels(); ++c)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, in.getReadPointer (c), out.getWritePointer (c), out.getNumSamples());
        }
        return out;
    }

    // Level of the singing (loudest 30 % of 50 ms windows), so A/B is loudness-matched
    float singingRms (const juce::AudioBuffer<float>& b, double rate)
    {
        const int win = juce::jmax (1, (int) (0.05 * rate));
        std::vector<float> lv;
        for (int s = 0; s + win <= b.getNumSamples(); s += win)
        {
            double e = 0.0;
            for (int c = 0; c < b.getNumChannels(); ++c)
                for (int i = 0; i < win; ++i) { const float x = b.getSample (c, s + i); e += (double) x * x; }
            lv.push_back ((float) std::sqrt (e / (win * juce::jmax (1, b.getNumChannels()))));
        }
        if (lv.empty()) return 0.f;
        std::sort (lv.begin(), lv.end());
        double s = 0.0; int n = 0;
        for (size_t i = lv.size() * 7 / 10; i < lv.size(); ++i) { s += lv[i]; ++n; }
        return n > 0 ? (float) (s / n) : 0.f;
    }
}

MatchSession::MatchSession (PluginLibrary& lib) : juce::Thread ("Nova Match"), library (lib) {}

MatchSession::~MatchSession()
{
    cancelFlag = true;
    stopThread (10000);
    *alive = false;
}

void MatchSession::finishClip (Clip& c)
{
    // Waveform
    c.peaks.assign (240, 0.f);
    const int n = c.audio.getNumSamples();
    for (int p = 0; p < 240 && n > 0; ++p)
    {
        const int a = (int) ((int64_t) n * p / 240), b = (int) ((int64_t) n * (p + 1) / 240);
        float m = 0.f;
        for (int ch = 0; ch < c.audio.getNumChannels(); ++ch)
            m = juce::jmax (m, c.audio.getMagnitude (ch, a, juce::jmax (1, b - a)));
        c.peaks[(size_t) p] = m;
    }
    // Key (for the label only)
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        for (int ch = 0; ch < c.audio.getNumChannels(); ++ch) s += c.audio.getSample (ch, i);
        mono[(size_t) i] = s / (float) juce::jmax (1, c.audio.getNumChannels());
    }
    const auto k = estimateNovaKey (mono.data(), n, c.rate);
    c.key = k.valid ? juce::String (kKeyNames[k.keyPc]) + (k.minor ? " MINOR" : " MAJOR") : juce::String();
    preparePreviews();
}

juce::String MatchSession::loadFile (bool isRef, const juce::File& f)
{
    if (isRunning()) return "Wait for the match to finish first.";
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return "Couldn't open " + f.getFileName() + " (use WAV, AIFF, FLAC, MP3 or M4A).";
    const int len = (int) juce::jmin ((juce::int64) (60.0 * r->sampleRate), r->lengthInSamples);
    if (len < (int) (3.0 * r->sampleRate)) return "That file is too short. Use at least a few seconds of singing.";

    Clip c;
    c.rate = r->sampleRate;
    c.name = f.getFileNameWithoutExtension();
    c.audio.setSize (juce::jlimit (1, 2, (int) r->numChannels), len);
    r->read (&c.audio, 0, len, 0, true, c.audio.getNumChannels() > 1);
    (isRef ? ref : yours) = std::move (c);
    finishClip (isRef ? ref : yours);
    return {};
}

void MatchSession::prepare (double sampleRate, int)
{
    hostRate = sampleRate;
    if (captureBuf.getNumSamples() != (int) (kCaptureSeconds * sampleRate))
    {
        capturing = false;
        captureBuf.setSize (2, (int) (kCaptureSeconds * sampleRate));
        captureWrite = 0;
    }
    preparePreviews();
}

void MatchSession::startCapture()
{
    captureWrite = 0;
    capturing = true;
}

void MatchSession::stopCapture()
{
    capturing = false;
    const int n = captureWrite.load();
    if (n < (int) (3.0 * hostRate)) return;      // too short to use; keep what we had
    Clip c;
    c.rate = hostRate;
    c.name = "Captured from the track";
    c.audio.setSize (2, n);
    for (int ch = 0; ch < 2; ++ch) c.audio.copyFrom (ch, 0, captureBuf, ch, 0, n);
    yours = std::move (c);
    finishClip (yours);
}

void MatchSession::pushCapture (const juce::AudioBuffer<float>& in) noexcept
{
    if (! capturing.load()) return;
    const int w = captureWrite.load();
    const int n = juce::jmin (in.getNumSamples(), captureBuf.getNumSamples() - w);
    for (int ch = 0; ch < 2 && n > 0; ++ch)
        captureBuf.copyFrom (ch, w, in, juce::jmin (ch, in.getNumChannels() - 1), 0, n);
    captureWrite = w + juce::jmax (0, n);
    if (w + n >= captureBuf.getNumSamples()) { capturing = false; captureFull = true; }   // the editor finishes it
}

juce::String MatchSession::start (const std::array<bool, kNumStages>& locked)
{
    if (isRunning()) return "Already matching.";
    if (ref.empty()) return "Load a reference vocal first.";
    if (yours.empty()) return "Capture or load your vocal first.";

    const auto missing = engine.preparePlugins ([this] (const juce::String& name, double rate, int block)
    {
        std::unique_ptr<juce::AudioPluginInstance> inst;
        if (auto d = library.find (name))
        {
            juce::String err;
            inst = library.create (*d, rate, block, err);
        }
        return inst;
    }, yours.rate);
    if (missing.size() >= kNumStages) return "None of the Nova plugins Match uses are installed.";

    lockedStages = locked;
    cancelFlag = false;
    progressValue = 0.f;
    { const juce::ScopedLock sl (statusLock); statusText = missing.isEmpty() ? "Starting" : "Not installed (skipped): " + missing.joinIntoString (", "); }
    startThread (juce::Thread::Priority::low);
    return {};
}

void MatchSession::cancel() { cancelFlag = true; }

juce::String MatchSession::status() const
{
    const juce::ScopedLock sl (statusLock);
    return statusText;
}

void MatchSession::run()
{
    auto res = std::make_unique<MatchResult> (engine.run (ref.audio, ref.rate, yours.audio, yours.rate, lockedStages,
        [this] (float p, const juce::String& s)
        {
            progressValue = p;
            { const juce::ScopedLock sl (statusLock); statusText = s; }
            return ! cancelFlag.load() && ! threadShouldExit();
        }));

    auto flag = alive;
    auto* raw = res.release();
    juce::MessageManager::callAsync ([this, flag, raw]
    {
        std::unique_ptr<MatchResult> r (raw);
        if (! *flag) return;
        engine.releasePlugins();
        if (r->ok)
        {
            lastResult = std::move (r);
            preparePreviews();
            const juce::ScopedLock sl (statusLock);
            statusText = "Matched in " + juce::String (juce::roundToInt (lastResult->seconds)) + " s · " + juce::String (lastResult->renders) + " test renders";
        }
        else
        {
            const juce::ScopedLock sl (statusLock);
            statusText = r->error;
        }
        progressValue = 1.f;
        if (onFinished) onFinished();
    });
}

void MatchSession::preparePreviews()
{
    std::array<juce::AudioBuffer<float>, 3> next;
    next[0] = resampleTo (ref.audio, ref.rate, hostRate);
    next[1] = resampleTo (yours.audio, yours.rate, hostRate);
    if (lastResult != nullptr) next[2] = resampleTo (lastResult->matchedAudio, yours.rate, hostRate);

    // Same singing level for all three, so A/B compares sound, not loudness
    for (auto& b : next)
    {
        const float r = singingRms (b, hostRate);
        if (r > 1.0e-5f) b.applyGain (0.1f / r);
    }
    const juce::ScopedLock sl (previewLock);
    std::swap (previews, next);
    previewPos = 0;
}

void MatchSession::setPreview (int mode)
{
    const juce::ScopedLock sl (previewLock);
    if (mode != previewMode.load()) previewPos = 0;
    previewMode = juce::jlimit (0, 3, mode);
}

bool MatchSession::renderPreview (juce::AudioBuffer<float>& io) noexcept
{
    const int mode = previewMode.load();
    if (mode == 0) return false;
    const juce::ScopedTryLock sl (previewLock);
    io.clear();
    if (! sl.isLocked()) return true;
    const auto& src = previews[(size_t) (mode - 1)];
    const int len = src.getNumSamples();
    if (len == 0) return true;
    for (int i = 0; i < io.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < io.getNumChannels(); ++ch)
            io.setSample (ch, i, src.getSample (juce::jmin (ch, src.getNumChannels() - 1), previewPos));
        if (++previewPos >= len) previewPos = 0;
    }
    return true;
}

RackData MatchSession::buildRack (float strength) const
{
    RackData r;
    r.name = "Match · " + (ref.name.isNotEmpty() ? ref.name : juce::String ("Reference"));
    if (lastResult == nullptr) return r;
    for (const auto& st : lastResult->chain)
    {
        SlotData sd;
        sd.plugin = st.plugin;
        sd.params = st.params;
        sd.mix = juce::jlimit (0.f, 1.f, strength);
        sd.preset = "Matched";
        r.slots.add (sd);
    }
    return r;
}

} // namespace nr::match
