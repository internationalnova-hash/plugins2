#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace NovaMirror;

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout l;
        auto pct = [] (const char* id, const char* name, float def)
        {
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 100.f, 0.1f), def,
                                                           AudioParameterFloatAttributes().withLabel ("%"));
        };
        auto hz = [] (const char* id, const char* name, float def)
        {
            NormalisableRange<float> r (20.f, 20000.f, [] (float a, float b, float v) { return a * std::pow (b / a, v); },
                                        [] (float a, float b, float x) { return std::log (x / a) / std::log (b / a); });
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel ("Hz"));
        };
        l.add (pct ("cut", "Cut", 50.f));
        l.add (pct ("boost", "Boost", 50.f));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "link", 1 }, "Link", true));
        l.add (pct ("speed", "Speed", 40.f));
        l.add (hz ("from", "From", 20.f));
        l.add (hz ("to", "To", 20000.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
                                                      AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Source", StringArray { "Vocal", "Beat", "Mix Bus", "Master" }, 2));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "stereo", 1 }, "Stereo", StringArray { "Stereo", "Mid", "Side", "M/S" }, 0));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "punch", 1 }, "Punch Guard", true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "peak", 1 }, "Peak Control", true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze to EQ", false));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "sections", 1 }, "Section Match", true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "listen", 1 }, "Listen", false));
        return l;
    }

    // SPEED knob 0..100 -> input smoothing 20 ms .. 1.5 s
    float speedToMs (float v) { return 20.f * std::pow (75.f, v / 100.f); }
}

NovaMirrorProcessor::NovaMirrorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    publishCurrentTarget();
    startTimerHz (10);
}

NovaMirrorProcessor::~NovaMirrorProcessor()
{
    alive->store (false);
    stopTimer();
    pool.removeAllJobs (true, 10000);
    live.store (nullptr);
}

bool NovaMirrorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void NovaMirrorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = std::abs (sampleRate - sr) > 0.5;
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock, std::max (1, getTotalNumOutputChannels()));
    latency = engine.getLatency();
    look = std::max (1, (int) std::lround (0.0015 * sampleRate));   // Peak Control look-ahead
    setLatencySamples (latency + look);
    wRing.assign (2, std::vector<float> ((size_t) look, 0.f));
    dRing.assign (2, std::vector<float> ((size_t) look, 0.f));
    reqRing.assign ((size_t) look + 1, 1.f);
    ringPos = reqPos = 0;
    peakCeil = 0.f;
    dryDelay.setSize (2, latency + std::max (samplesPerBlock, 512) * 2 + 1);
    dryDelay.clear();
    dryWrite = 0;
    dryScratchL.assign ((size_t) std::max (samplesPerBlock, 512) * 2, 0.f);
    dryScratchR.assign (dryScratchL.size(), 0.f);
    powerMix = *apvts.getRawParameterValue ("power") > 0.5f ? 1.f : 0.f;
    peakGain = 1.f;
    // reference audio for Listen is resampled to the session rate
    if (rateChanged && referenceFile.existsAsFile())
        juce::MessageManager::callAsync ([this, flag = alive] { if (flag->load()) startReferenceJob (referenceFile); });
}

// ════════════════════════════════════════════════════════════════════════════
// Audio
// ════════════════════════════════════════════════════════════════════════════
void NovaMirrorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through

    const int N = buffer.getNumSamples();
    const int nCh = std::min (2, buffer.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, N);
    if (N == 0 || nCh == 0) return;
    if ((int) dryScratchL.size() < N) return;   // host broke its max block promise; skip safely

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    EngineParams ep;
    ep.cut = P ("cut") / 100.f;
    ep.boost = P ("boost") / 100.f;
    ep.speedMs = speedToMs (P ("speed"));
    ep.fromHz = P ("from");
    ep.toHz = std::max (ep.fromHz * 1.05f, P ("to"));
    ep.mode = (int) P ("mode");
    ep.stereo = (int) P ("stereo");
    ep.punchGuard = P ("punch") > 0.5f;
    ep.sections = P ("sections") > 0.5f;
    ep.freeze = P ("freeze") > 0.5f;
    const bool peakOn = P ("peak") > 0.5f, powerOn = P ("power") > 0.5f, listenOn = P ("listen") > 0.5f;
    const float outGain = juce::Decibels::decibelsToGain (P ("output"));
    engine.setParams (ep);
    auto* tgt = live.load (std::memory_order_acquire);
    engine.setProfile (tgt != nullptr ? &tgt->profile : nullptr);

    // dry, delayed by the engine latency (for bypass, Peak Control and level matching)
    const int dsz = dryDelay.getNumSamples();
    float* dl[2] = { dryScratchL.data(), dryScratchR.data() };
    for (int c = 0; c < 2; ++c)
    {
        const float* in = buffer.getReadPointer (std::min (c, nCh - 1));
        float* ring = dryDelay.getWritePointer (c);
        int w = dryWrite;
        for (int i = 0; i < N; ++i)
        {
            ring[w] = in[i];
            int r = w - latency; if (r < 0) r += dsz;
            dl[c][i] = ring[r];
            if (++w == dsz) w = 0;
        }
    }
    dryWrite = (dryWrite + N) % dsz;

    engine.process (buffer.getArrayOfWritePointers(), nCh, N);

    // block stats
    float wPk = 0.f, dPk = 0.f, wE = 0.f, dE = 0.f;
    for (int c = 0; c < nCh; ++c)
    {
        const float* w = buffer.getReadPointer (c);
        for (int i = 0; i < N; ++i)
        {
            wPk = std::max (wPk, std::abs (w[i])); dPk = std::max (dPk, std::abs (dl[c][i]));
            wE += w[i] * w[i]; dE += dl[c][i] * dl[c][i];
        }
    }
    wE /= (float) (N * nCh); dE /= (float) (N * nCh);
    const float slow = std::exp (-(float) N / (float) (sr * 3.0));
    if (dE > 1.0e-10f)
    {
        wetMs = slow * wetMs + (1.f - slow) * wE;
        dryMs = slow * dryMs + (1.f - slow) * dE;
        // Auto level: Nova Mirror changes tone, not volume (so on/off compares fairly too)
        if (wetMs > 1.0e-10f) matchGain = juce::jlimit (0.5f, 2.f, std::sqrt (dryMs / wetMs));
    }
    const float wetGain = matchGain * outGain;

    // Listen: the reference at matched loudness, in place of the track
    const ListenAudio* la = (tgt != nullptr && tgt->listen != nullptr && ! tgt->listen->L.empty()) ? tgt->listen.get() : nullptr;
    if (la != nullptr && la->ms > 1.0e-10f && dryMs > 1.0e-10f) listenGain = juce::jlimit (0.05f, 8.f, std::sqrt (dryMs / la->ms));

    const float pStep = 1.f / (float) (0.03 * sr), lStep = 1.f / (float) (0.02 * sr);
    const float pTarget = powerOn ? 1.f : 0.f, lTarget = (listenOn && la != nullptr) ? 1.f : 0.f;
    // Peak Control: a look-ahead peak limiter whose ceiling is the input's own peak level,
    // so matching never makes peaks louder than they were (and only the peaks are touched)
    const float ceilRel = std::exp (-1.f / (float) (0.3 * sr));
    const int holdN = (int) (0.04 * sr);
    const float att = 1.f - std::exp (-4.f / (float) std::max (1, look));
    const float rel = 1.f - std::exp (-1.f / (float) (0.06 * sr));
    float outPk = 0.f;
    for (int i = 0; i < N; ++i)
    {
        powerMix += juce::jlimit (-pStep, pStep, pTarget - powerMix);
        listenMix += juce::jlimit (-lStep, lStep, lTarget - listenMix);

        float wIn[2] = { 0.f, 0.f }, dIn[2] = { 0.f, 0.f }, wAbs = 0.f, dAbs = 0.f;
        for (int c = 0; c < nCh; ++c)
        {
            wIn[c] = buffer.getReadPointer (c)[i] * wetGain;
            dIn[c] = dl[c][i] * outGain;
            wAbs = std::max (wAbs, std::abs (wIn[c]));
            dAbs = std::max (dAbs, std::abs (dIn[c]));
        }
        // ceiling = the input's peak level around here: held 40 ms, then easing down
        if (dAbs >= peakCeil) { peakCeil = dAbs; ceilHold = holdN; }
        else if (ceilHold > 0) --ceilHold;
        else peakCeil *= ceilRel;
        reqRing[(size_t) reqPos] = (peakOn && wAbs > peakCeil && wAbs > 1.0e-9f) ? peakCeil / wAbs : 1.f;
        reqPos = (reqPos + 1) % (look + 1);
        float mn = 1.f;
        for (float r : reqRing) mn = std::min (mn, r);
        peakGain += (mn - peakGain) * (mn < peakGain ? att : rel);

        float refL = 0.f, refR = 0.f;
        if (la != nullptr && listenMix > 0.f)
        {
            const size_t k = (size_t) listenPos % la->L.size();
            refL = la->L[k] * listenGain; refR = (la->R.empty() ? la->L[k] : la->R[k]) * listenGain;
        }
        for (int c = 0; c < nCh; ++c)
        {
            const float wd = wRing[(size_t) c][(size_t) ringPos], dd = dRing[(size_t) c][(size_t) ringPos];
            wRing[(size_t) c][(size_t) ringPos] = wIn[c];
            dRing[(size_t) c][(size_t) ringPos] = dIn[c];
            float v = wd * peakGain * powerMix + dd * (1.f - powerMix);
            if (listenMix > 0.f) v = v * (1.f - listenMix) + (c == 0 ? refL : refR) * listenMix;
            v = std::isfinite (v) ? juce::jlimit (-4.f, 4.f, v) : 0.f;
            buffer.getWritePointer (c)[i] = v;
            outPk = std::max (outPk, std::abs (v));
        }
        ringPos = (ringPos + 1) % look;
        if (la != nullptr && listenOn) listenPos += 1.0;
    }
    if (la != nullptr) listenPosSec.store (std::fmod (listenPos, (double) la->L.size()) / sr);
    outPeakDb.store (juce::Decibels::gainToDecibels (outPk, -100.f));
}

double NovaMirrorProcessor::getListenPositionSeconds() const noexcept { return listenPosSec.load(); }

// ════════════════════════════════════════════════════════════════════════════
// Targets & references
// ════════════════════════════════════════════════════════════════════════════
juce::File NovaMirrorProcessor::userTargetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Nova Audio").getChildFile ("Nova Mirror").getChildFile ("Targets");
}

juce::File NovaMirrorProcessor::referencesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Nova Audio").getChildFile ("Nova Mirror").getChildFile ("References");
}

void NovaMirrorProcessor::publish (std::unique_ptr<MirrorTarget> t)
{
    live.store (t.get(), std::memory_order_release);
    if (liveOwner != nullptr) retired.push_back ({ std::move (liveOwner), juce::Time::getMillisecondCounterHiRes() });
    liveOwner = std::move (t);
}

void NovaMirrorProcessor::publishCurrentTarget()
{
    std::unique_ptr<MirrorTarget> t;
    {
        const juce::ScopedLock sl (stateLock);
        if (targetKind == "reference")
        {
            if (referenceProfile != nullptr)
            {
                t = std::make_unique<MirrorTarget>();
                t->profile = *referenceProfile;
                t->listen = referenceListen;
            }
        }
        else if (targetKind == "user")
        {
            const auto f = userTargetsFolder().getChildFile (juce::File::createLegalFileName (targetName) + ".novatarget");
            const auto v = juce::JSON::parse (f.loadFileAsString());
            if (v.isObject())
            {
                t = std::make_unique<MirrorTarget>();
                t->profile = Profile::fromVar (v);
                t->profile.kind = "user";
            }
        }
        else
        {
            t = std::make_unique<MirrorTarget>();
            t->profile = novaTarget (targetName);
        }
    }
    publish (std::move (t));
    ++infoVersion;
}

void NovaMirrorProcessor::startReferenceJob (juce::File f)
{
    loading.store (true);
    { const juce::ScopedLock sl (stateLock); status = "Analysing " + f.getFileNameWithoutExtension() + "..."; }
    ++infoVersion;
    const double rate = sr;
    auto flag = alive;
    pool.addJob ([this, flag, f, rate]
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        std::shared_ptr<const Profile> prof;
        std::shared_ptr<const ListenAudio> listen;
        juce::String err;
        if (r == nullptr) err = "Couldn't open " + f.getFileName() + " (use WAV, AIFF, FLAC, MP3 or M4A)";
        else
        {
            const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 600.0));
            juce::AudioBuffer<float> buf ((int) std::min (2u, r->numChannels), len);
            r->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);
            prof = std::make_shared<const Profile> (analyseReference (buf, r->sampleRate, f.getFileNameWithoutExtension()));

            auto la = std::make_shared<ListenAudio>();
            const double ratio = r->sampleRate / rate;
            const int outLen = (int) ((double) len / ratio);
            for (int c = 0; c < buf.getNumChannels(); ++c)
            {
                auto& dst = c == 0 ? la->L : la->R;
                dst.resize ((size_t) outLen);
                juce::LagrangeInterpolator interp;
                interp.process (ratio, buf.getReadPointer (c), dst.data(), outLen);
            }
            double e = 0.0;
            for (size_t i = 0; i < la->L.size(); ++i)
            {
                const float rr = la->R.empty() ? la->L[i] : la->R[i];
                e += 0.5 * ((double) la->L[i] * la->L[i] + (double) rr * rr);
            }
            la->ms = la->L.empty() ? 0.f : (float) (e / (double) la->L.size());
            la->sampleRate = rate;
            listen = la;
        }
        if (! flag->load()) return;
        std::lock_guard<std::mutex> lk (pendingLock);
        pendingProfile = prof;
        pendingListen = listen;
        pendingError = err;
        pendingReady = true;
    });
}

void NovaMirrorProcessor::loadReferenceFile (const juce::File& f)
{
    {
        const juce::ScopedLock sl (stateLock);
        referenceFile = f;
        targetKind = "reference";
        targetName = f.getFileNameWithoutExtension();
    }
    apvts.getParameter ("listen")->setValueNotifyingHost (0.f);
    startReferenceJob (f);
}

void NovaMirrorProcessor::loadReferenceBytes (const juce::String& name, const juce::MemoryBlock& data)
{
    auto folder = referencesFolder();
    folder.createDirectory();
    auto f = folder.getNonexistentChildFile (juce::File::createLegalFileName (name).upToLastOccurrenceOf (".", false, false),
                                             "." + name.fromLastOccurrenceOf (".", false, false), false);
    if (f.replaceWithData (data.getData(), data.getSize())) loadReferenceFile (f);
    else { { const juce::ScopedLock sl (stateLock); status = "Couldn't save the dropped file"; } ++infoVersion; }
}

void NovaMirrorProcessor::clearReference()
{
    {
        const juce::ScopedLock sl (stateLock);
        referenceFile = juce::File();
        referenceProfile.reset();
        referenceListen.reset();
        targetKind = "nova";
        targetName = "Nova Balance";
        status = {};
    }
    apvts.getParameter ("listen")->setValueNotifyingHost (0.f);
    publishCurrentTarget();
}

void NovaMirrorProcessor::setTarget (const juce::String& kind, const juce::String& name)
{
    {
        const juce::ScopedLock sl (stateLock);
        targetKind = kind;
        targetName = kind == "reference" && referenceProfile != nullptr ? referenceProfile->name : name;
    }
    publishCurrentTarget();
}

bool NovaMirrorProcessor::saveCurrentAsUserTarget (const juce::String& name)
{
    std::shared_ptr<const Profile> p;
    { const juce::ScopedLock sl (stateLock); p = referenceProfile; }
    if (p == nullptr || name.trim().isEmpty()) return false;
    Profile copy = *p;
    copy.name = name.trim();
    copy.kind = "user";
    copy.peaks.clear();
    auto folder = userTargetsFolder();
    folder.createDirectory();
    const bool ok = folder.getChildFile (juce::File::createLegalFileName (copy.name) + ".novatarget")
                          .replaceWithText (juce::JSON::toString (copy.toVar()));
    ++infoVersion;
    return ok;
}

void NovaMirrorProcessor::deleteUserTarget (const juce::String& name)
{
    userTargetsFolder().getChildFile (juce::File::createLegalFileName (name) + ".novatarget").deleteFile();
    bool wasCurrent;
    { const juce::ScopedLock sl (stateLock); wasCurrent = targetKind == "user" && targetName == name; }
    if (wasCurrent) setTarget ("nova", "Nova Balance");
    ++infoVersion;
}

void NovaMirrorProcessor::selectAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    abState[abSlot] = apvts.copyState();
    if (abState[slot].isValid()) apvts.replaceState (abState[slot].createCopy());
    abSlot = slot;
    ++infoVersion;
}

void NovaMirrorProcessor::timerCallback()
{
    bool ready = false;
    std::shared_ptr<const Profile> prof;
    std::shared_ptr<const ListenAudio> listen;
    juce::String err;
    {
        std::lock_guard<std::mutex> lk (pendingLock);
        if (pendingReady) { ready = true; prof = pendingProfile; listen = pendingListen; err = pendingError; pendingReady = false; }
    }
    if (ready)
    {
        loading.store (false);
        {
            const juce::ScopedLock sl (stateLock);
            if (prof != nullptr) { referenceProfile = prof; referenceListen = listen; status = {}; }
            else status = err;
        }
        publishCurrentTarget();
    }
    const double now = juce::Time::getMillisecondCounterHiRes();
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (auto& p) { return now - p.second > 2000.0; }), retired.end());
}

juce::String NovaMirrorProcessor::getInfoJson() const
{
    auto* o = new juce::DynamicObject();
    const juce::ScopedLock sl (stateLock);
    o->setProperty ("kind", targetKind);
    o->setProperty ("name", targetName);
    juce::Array<juce::var> nova, user;
    for (auto& n : novaTargetNames()) nova.add (n);
    for (auto& f : userTargetsFolder().findChildFiles (juce::File::findFiles, false, "*.novatarget"))
        user.add (f.getFileNameWithoutExtension());
    o->setProperty ("nova", nova);
    o->setProperty ("user", user);
    o->setProperty ("loading", loading.load());
    o->setProperty ("status", status);
    o->setProperty ("ab", abSlot);
    o->setProperty ("listenReady", referenceListen != nullptr);
    if (referenceProfile != nullptr)
    {
        auto v = referenceProfile->toVar();
        if (auto* d = v.getDynamicObject()) { d->removeProperty ("mono"); d->removeProperty ("side"); }
        o->setProperty ("ref", v);
        o->setProperty ("refFile", referenceFile.getFileName());
    }
    return juce::JSON::toString (juce::var (o), true);
}

// ════════════════════════════════════════════════════════════════════════════
// Session state
// ════════════════════════════════════════════════════════════════════════════
void NovaMirrorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("NovaMirror");
    root.setAttribute ("v", 1);
    if (auto x = apvts.copyState().createXml()) root.addChildElement (x.release());
    {
        const juce::ScopedLock sl (stateLock);
        root.setAttribute ("kind", targetKind);
        root.setAttribute ("name", targetName);
        root.setAttribute ("refPath", referenceFile.getFullPathName());
        root.setAttribute ("ab", abSlot);
        if (referenceProfile != nullptr)
            root.createNewChildElement ("Reference")->addTextElement (juce::JSON::toString (referenceProfile->toVar(), true));
    }
    for (int s = 0; s < 2; ++s)
        if (abState[s].isValid())
            if (auto x = abState[s].createXml())
            {
                auto* e = root.createNewChildElement ("AB" + juce::String (s));
                e->addChildElement (x.release());
            }
    copyXmlToBinary (root, destData);
}

void NovaMirrorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("NovaMirror")) return;
    if (auto* p = xml->getChildByName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*p));
    for (int s = 0; s < 2; ++s)
        if (auto* e = xml->getChildByName ("AB" + juce::String (s)))
            if (auto* x = e->getFirstChildElement()) abState[s] = juce::ValueTree::fromXml (*x);
    abSlot = xml->getIntAttribute ("ab", 0);

    juce::File ref (xml->getStringAttribute ("refPath"));
    {
        const juce::ScopedLock sl (stateLock);
        targetKind = xml->getStringAttribute ("kind", "nova");
        targetName = xml->getStringAttribute ("name", "Nova Balance");
        referenceFile = ref;
        referenceListen.reset();
        referenceProfile.reset();
        if (auto* r = xml->getChildByName ("Reference"))
        {
            const auto v = juce::JSON::parse (r->getAllSubText());
            if (v.isObject()) referenceProfile = std::make_shared<const Profile> (Profile::fromVar (v));
        }
    }
    apvts.getParameter ("listen")->setValueNotifyingHost (0.f);
    // the saved analysis works right away; the audio (for Listen) reloads if the file is still there
    juce::MessageManager::callAsync ([this, flag = alive, ref]
    {
        if (! flag->load()) return;
        publishCurrentTarget();
        if (ref.existsAsFile()) startReferenceJob (ref);
    });
}

juce::AudioProcessorEditor* NovaMirrorProcessor::createEditor() { return new NovaMirrorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaMirrorProcessor(); }
