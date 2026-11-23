#include "LinkSession.h"

#ifndef NOVA_LINK_SIGNAL_URL
 #define NOVA_LINK_SIGNAL_URL "https://nova-store-olive.vercel.app"
#endif

namespace NovaLink
{
    namespace
    {
        constexpr size_t kChunk = 60000;

        juce::var obj(std::initializer_list<std::pair<const char*, juce::var>> kv)
        {
            auto* o = new juce::DynamicObject();
            for (auto& [k, v] : kv) o->setProperty(k, v);
            return juce::var(o);
        }

        float peakOf(const juce::AudioBuffer<float>& b, int ch, int n)
        {
            return ch < b.getNumChannels() ? b.getMagnitude(ch, 0, n) : 0.0f;
        }
    }

    juce::File LinkSession::baseFolder()
    {
        return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Nova Link");
    }

    LinkSession::LinkSession() : juce::Thread("Nova Link network")
    {
        writerThread.startThread();
    }

    LinkSession::~LinkSession()
    {
        leave();
        writerThread.stopThread(3000);
    }

    juce::String LinkSession::getPeerName() const
    {
        const juce::ScopedLock sl(chatLock);
        return peerName;
    }

    void LinkSession::setPeerName(const juce::String& n)
    {
        const juce::ScopedLock sl(chatLock);
        peerName = n;
    }

    juce::String LinkSession::hostSession(juce::String displayName)
    {
        const auto c = makeSessionCode();
        startWithSignaling(Role::Host, std::make_shared<HttpSignaling>(HttpSignaling::defaultServer(), c, "host"), c, displayName);
        return c;
    }

    bool LinkSession::joinSession(juce::String typed, juce::String displayName)
    {
        const auto c = normaliseSessionCode(typed);
        if (c.isEmpty()) return false;
        startWithSignaling(Role::Artist, std::make_shared<HttpSignaling>(HttpSignaling::defaultServer(), c, "artist"), c, displayName);
        return true;
    }

    void LinkSession::startWithSignaling(Role r, std::shared_ptr<Signaling> sig, juce::String c, juce::String displayName)
    {
        leave();
        role = r;
        code = c;
        myName = displayName.isNotEmpty() ? displayName : (r == Role::Host ? juce::String("Studio") : juce::String("Artist"));
        sessionFolder = baseFolder().getChildFile(juce::Time::getCurrentTime().formatted("%Y-%m-%d ") + code.replace("NOVA-", ""));
        sessionFolder.createDirectory();

        encoder = std::make_unique<StreamEncoder>(r == Role::Host ? 192000 : 160000);
        decoder = std::make_unique<StreamDecoder>();
        transport = std::make_shared<LinkTransport>();
        auto* dec = decoder.get();
        transport->onAudio = [dec] (const uint8_t* d, size_t n) { dec->insert(d, n); };
        transport->onControl = [this] (const std::string& s) { onControl(s); };
        transport->onFile = [this] (const uint8_t* d, size_t n) { onFileChunk(d, n); };
        transport->onVideo = [this] (const uint8_t* d, size_t n) { if (videoSink) videoSink(d, n); };
        transport->onState = [this] (LinkTransport::State s)
        {
            if (s == LinkTransport::State::Connected)
                sendJson(obj({ { "t", "hi" }, { "name", myName }, { "role", role == Role::Host ? "host" : "artist" } }));
        };
        transport->start(r == Role::Host ? LinkTransport::Role::Host : LinkTransport::Role::Artist, std::move(sig));
        startThread(juce::Thread::Priority::high);
    }

    void LinkSession::leave()
    {
        signalThreadShouldExit();
        stopThread(4000);
        if (transport) transport->stop();
        {
            const juce::SpinLock::ScopedLockType sl(writerLock);
            localWriter.reset();
            liveWriter.reset();
        }
        transport.reset();
        outgoing.reset();
        { const std::lock_guard<std::mutex> g(incomingLock); incoming.clear(); }
        role = Role::None;
        activeTake.store(0);
    }

    void LinkSession::sendJson(const juce::var& v)
    {
        if (transport) transport->sendControl(juce::JSON::toString(v, true).toStdString());
    }

    void LinkSession::sendChat(const juce::String& text)
    {
        if (text.trim().isEmpty()) return;
        { const juce::ScopedLock sl(chatLock); chat.add({ myName, text.trim(), -1.0, 0, juce::Time::currentTimeMillis() }); }
        sendJson(obj({ { "t", "chat" }, { "from", myName }, { "text", text.trim() } }));
    }

    void LinkSession::addMarker(const juce::String& text)
    {
        const double sec = posSec.load();
        const int take = activeTake.load();
        { const juce::ScopedLock sl(chatLock); chat.add({ myName, text.trim(), sec, take, juce::Time::currentTimeMillis() }); }
        sendJson(obj({ { "t", "marker" }, { "from", myName }, { "text", text.trim() }, { "sec", sec }, { "take", take } }));
    }

    void LinkSession::setSections(const juce::Array<Section>& s)
    {
        juce::Array<juce::var> arr;
        { const juce::ScopedLock sl(chatLock); sections = s; }
        for (auto& x : s) arr.add(obj({ { "sec", x.sec }, { "name", x.name } }));
        sendJson(obj({ { "t", "sections" }, { "list", arr } }));
    }

    //==========================================================================
    // Control messages (network thread)
    //==========================================================================
    void LinkSession::onControl(const std::string& json)
    {
        const auto v = juce::JSON::parse(juce::String(json));
        const auto t = v.getProperty("t", "").toString();

        if (t == "hi") setPeerName(v.getProperty("name", "").toString());
        else if (t == "ping") sendJson(obj({ { "t", "pong" }, { "at", v.getProperty("at", 0) } }));
        else if (t == "pong") rttMs.store((float) (juce::Time::getMillisecondCounter() - (juce::uint32) (juce::int64) v.getProperty("at", 0)));
        else if (t == "chat" || t == "marker")
        {
            const juce::ScopedLock sl(chatLock);
            chat.add({ v.getProperty("from", "").toString(), v.getProperty("text", "").toString(),
                       t == "marker" ? (double) v.getProperty("sec", 0.0) : -1.0, (int) v.getProperty("take", 0), juce::Time::currentTimeMillis() });
        }
        else if (t == "sections")
        {
            juce::Array<Section> s;
            if (auto* arr = v.getProperty("list", {}).getArray())
                for (auto& x : *arr) s.add({ (double) x.getProperty("sec", 0.0), x.getProperty("name", "").toString() });
            const juce::ScopedLock sl(chatLock);
            sections = s;
        }
        else if (t == "pos" && role == Role::Artist)
        {
            posSec.store((double) v.getProperty("sec", 0.0));
            posPpq.store((double) v.getProperty("ppq", 0.0));
            posBpm.store((double) v.getProperty("bpm", 120.0));
            posNum.store((int) v.getProperty("num", 4));
            posDen.store((int) v.getProperty("den", 4));
            posPlaying.store((bool) v.getProperty("playing", false));
            posRecording.store((bool) v.getProperty("rec", false));
        }
        else if (t == "take" && role == Role::Artist)
        {
            const int id = (int) v.getProperty("id", 0);
            if (v.getProperty("state", "").toString() == "start") startLocalTake(id, (double) v.getProperty("startSec", 0.0));
            else finishLocalTake(id);
        }
        else if (t == "takedel" && role == Role::Artist)
        {
            const int id = (int) v.getProperty("id", 0);
            juce::File f;
            {
                const juce::ScopedLock sl(takeLock);
                for (int i = takes.size(); --i >= 0;)
                    if (takes.getReference(i).id == id) { f = takes[i].file; takes.remove(i); }
            }
            if (f.existsAsFile() && (outgoing == nullptr || outgoing->id != id)) f.moveToTrash();
        }
        else if (t == "takeack" && role == Role::Artist)
        {
            const int id = (int) v.getProperty("id", 0);
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes) if (tk.id == id) { tk.state = "sent"; tk.progress = 1.0f; }
        }
    }

    // File channel messages: [take id: 4 bytes][type: 0 = header (JSON), 1 = data][payload].
    // Header and data travel on the same ordered channel, so they can't arrive out of order.
    void LinkSession::onFileChunk(const uint8_t* data, size_t size)
    {
        if (size < 5 || role != Role::Host) return;
        const int id = (int) (data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
        const uint8_t type = data[4];
        const std::lock_guard<std::mutex> g(incomingLock);
        if (type == 0)
        {
            const auto v = juce::JSON::parse(juce::String::fromUTF8((const char*) data + 5, (int) size - 5));
            Incoming in;
            in.id = id;
            in.size = (int64_t) (double) v.getProperty("bytes", 0.0);
            in.got = 0;
            in.startSec = (double) v.getProperty("startSec", 0.0);
            in.file = sessionFolder.getChildFile("Take " + juce::String(id) + ".wav").getNonexistentSibling();
            in.out = std::make_unique<juce::FileOutputStream>(in.file.withFileExtension("part"));
            in.out->setPosition(0); in.out->truncate();
            incoming[id] = std::move(in);
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes) if (tk.id == id) { tk.state = "transferring"; tk.progress = 0.0f; }
            return;
        }
        auto it = incoming.find(id);
        if (it == incoming.end() || it->second.out == nullptr) return;
        auto& in = it->second;
        in.out->write(data + 5, size - 5);
        in.got += (int64_t) (size - 5);
        {
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes) if (tk.id == id) tk.progress = in.size > 0 ? (float) in.got / (float) in.size : 1.0f;
        }
        if (in.got >= in.size)
        {
            in.out->flush();
            in.out.reset();
            in.file.withFileExtension("part").moveFileTo(in.file);
            Take done;
            {
                const juce::ScopedLock sl(takeLock);
                for (auto& tk : takes)
                    if (tk.id == id)
                    {
                        // The lossless file replaces the live-stream capture.
                        if (tk.file.existsAsFile() && tk.file != in.file) tk.file.deleteFile();
                        tk.file = in.file;
                        tk.startSec = in.startSec;
                        std::unique_ptr<juce::AudioFormatReader> r(juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(tk.file), true));
                        if (r) tk.lengthSec = (double) r->lengthInSamples / r->sampleRate;
                        tk.state = "ready";
                        tk.progress = 1.0f;
                        done = tk;
                    }
            }
            if (done.id != 0) loadTakeForPlayback(done);
            sendJson(obj({ { "t", "takeack" }, { "id", id } }));
            incoming.erase(it);
        }
    }

    //==========================================================================
    // Takes
    //==========================================================================
    void LinkSession::hostTakeStarted(double startSec)
    {
        const int id = nextTakeId++;
        Take tk;
        tk.id = id;
        tk.name = "Take " + juce::String(id);
        tk.startSec = startSec;
        tk.state = "recording";
        tk.file = sessionFolder.getChildFile("Take " + juce::String(id) + " (live).wav").getNonexistentSibling();
        { const juce::ScopedLock sl(takeLock); takes.add(tk); }

        // Live-stream capture as a fallback until the lossless file arrives.
        std::unique_ptr<juce::FileOutputStream> out(tk.file.createOutputStream());
        juce::WavAudioFormat wav;
        if (out != nullptr)
            if (auto* w = wav.createWriterFor(out.get(), sampleRate, 2, 24, {}, 0))
            {
                out.release();
                const juce::SpinLock::ScopedLockType sl(writerLock);
                liveWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(w, writerThread, 65536);
                liveStartSec.store(-1.0);
                liveFile = tk.file;
            }
        activeTake.store(id);
        sendJson(obj({ { "t", "take" }, { "id", id }, { "state", "start" }, { "startSec", startSec } }));
    }

    void LinkSession::hostTakeStopped()
    {
        const int id = activeTake.exchange(0);
        if (id == 0) return;
        double start = liveStartSec.load();
        {
            const juce::SpinLock::ScopedLockType sl(writerLock);
            liveWriter.reset();
        }
        {
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes)
                if (tk.id == id)
                {
                    if (start >= 0.0) tk.startSec = start;
                    std::unique_ptr<juce::AudioFormatReader> r(juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(tk.file), true));
                    if (r) tk.lengthSec = (double) r->lengthInSamples / r->sampleRate;
                    tk.state = lossless.load() ? "waiting" : "ready";
                }
        }
        sendJson(obj({ { "t", "take" }, { "id", id }, { "state", "stop" } }));
        if (!lossless.load())
        {
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes) if (tk.id == id) loadTakeForPlayback(tk);
        }
    }

    void LinkSession::startLocalTake(int id, double)
    {
        Take tk;
        tk.id = id;
        tk.name = "Take " + juce::String(id);
        tk.state = "recording";
        tk.file = sessionFolder.getChildFile("Take " + juce::String(id) + " (lossless).wav").getNonexistentSibling();
        std::unique_ptr<juce::FileOutputStream> out(tk.file.createOutputStream());
        juce::WavAudioFormat wav;
        if (out != nullptr)
            if (auto* w = wav.createWriterFor(out.get(), sampleRate, 2, 24, {}, 0))
            {
                out.release();
                const juce::SpinLock::ScopedLockType sl(writerLock);
                localWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(w, writerThread, 65536);
                localFile = tk.file;
                localStartSec.store(-1.0);
                localNeedsStart.store(true);
            }
        { const juce::ScopedLock sl(takeLock); takes.add(tk); }
        activeTake.store(id);
    }

    void LinkSession::finishLocalTake(int id)
    {
        activeTake.store(0);
        {
            const juce::SpinLock::ScopedLockType sl(writerLock);
            localWriter.reset();   // flushes
        }
        const double start = localStartSec.load();
        {
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes) if (tk.id == id) { tk.state = "sending"; tk.startSec = juce::jmax(0.0, start); tk.progress = 0.0f; }
        }
        if (lossless.load() && localFile.existsAsFile())
        {
            outgoingStarts[id] = juce::jmax(0.0, start);
            outgoingQueue.add(id);
        }
    }

    void LinkSession::pumpOutgoingFile()
    {
        if (!transport || transport->getState() != LinkTransport::State::Connected) return;
        if (outgoing == nullptr)
        {
            if (outgoingQueue.isEmpty()) return;
            const int id = outgoingQueue.removeAndReturn(0);
            juce::File f;
            { const juce::ScopedLock sl(takeLock); for (auto& tk : takes) if (tk.id == id) f = tk.file; }
            if (!f.existsAsFile()) return;
            outgoing = std::make_unique<Outgoing>();
            outgoing->id = id;
            outgoing->file = f;
            outgoing->startSec = outgoingStarts[id];
            outgoing->size = f.getSize();
            outgoing->sent = 0;
            outgoing->in = std::make_unique<juce::FileInputStream>(f);
            const auto header = juce::JSON::toString(obj({ { "bytes", (double) outgoing->size }, { "startSec", outgoing->startSec } }), true).toStdString();
            std::vector<uint8_t> hb(5 + header.size());
            hb[0] = (uint8_t) id; hb[1] = (uint8_t) (id >> 8); hb[2] = (uint8_t) (id >> 16); hb[3] = (uint8_t) (id >> 24); hb[4] = 0;
            std::memcpy(hb.data() + 5, header.data(), header.size());
            if (!transport->sendFile(hb.data(), hb.size())) { outgoing.reset(); outgoingQueue.insert(0, id); return; }
        }
        std::vector<uint8_t> buf(kChunk + 5);
        for (int k = 0; k < 16 && outgoing; ++k)
        {
            const int id = outgoing->id;
            buf[0] = (uint8_t) id; buf[1] = (uint8_t) (id >> 8); buf[2] = (uint8_t) (id >> 16); buf[3] = (uint8_t) (id >> 24); buf[4] = 1;
            const auto n = outgoing->in->read(buf.data() + 5, (int) kChunk);
            if (n <= 0) { outgoing.reset(); break; }
            if (!transport->sendFile(buf.data(), (size_t) n + 5))
            {
                outgoing->in->setPosition(outgoing->sent);   // buffer full: retry this chunk later
                break;
            }
            outgoing->sent += n;
            {
                const juce::ScopedLock sl(takeLock);
                for (auto& tk : takes) if (tk.id == id) tk.progress = (float) outgoing->sent / (float) juce::jmax((int64_t) 1, outgoing->size);
            }
            if (outgoing->sent >= outgoing->size) outgoing.reset();
        }
    }

    void LinkSession::loadTakeForPlayback(const Take& tk)
    {
        std::unique_ptr<juce::AudioFormatReader> r(juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(tk.file), true));
        if (r == nullptr) return;
        auto pt = std::make_shared<PlaybackTake>();
        pt->id = tk.id;
        pt->startSec = tk.startSec;
        pt->fileRate = r->sampleRate;
        pt->audio.setSize(2, (int) r->lengthInSamples);
        r->read(&pt->audio, 0, (int) r->lengthInSamples, 0, true, true);
        if (r->numChannels == 1) pt->audio.copyFrom(1, 0, pt->audio, 0, 0, pt->audio.getNumSamples());
        std::vector<std::shared_ptr<PlaybackTake>> keep;
        {
            const juce::SpinLock::ScopedLockType sl(playbackLock);
            // Newer takes over the same region replace older ones for playback.
            const double end = pt->startSec + pt->audio.getNumSamples() / pt->fileRate;
            for (auto& p : playback)
            {
                const double pe = p->startSec + p->audio.getNumSamples() / p->fileRate;
                if (pe <= pt->startSec || p->startSec >= end) keep.push_back(p);
            }
            keep.push_back(pt);
            playback.swap(keep);
        }
        // `keep` now holds the replaced takes; freed here, off the audio thread.
    }

    void LinkSession::playTake(int takeId)
    {
        Take tk;
        { const juce::ScopedLock sl(takeLock); for (auto& t : takes) if (t.id == takeId) tk = t; }
        if (tk.id != 0 && tk.file.existsAsFile() && (tk.state == "ready" || tk.state == "waiting"))
            loadTakeForPlayback(tk);
    }

    void LinkSession::deleteTake(int takeId)
    {
        if (takeId == activeTake.load()) return;   // can't delete the take being recorded
        Take tk;
        {
            const juce::ScopedLock sl(takeLock);
            for (int i = takes.size(); --i >= 0;)
                if (takes.getReference(i).id == takeId) { tk = takes[i]; takes.remove(i); }
        }
        std::vector<std::shared_ptr<PlaybackTake>> removed;
        {
            const juce::SpinLock::ScopedLockType sl(playbackLock);
            for (auto it = playback.begin(); it != playback.end();)
                if ((*it)->id == takeId) { removed.push_back(*it); it = playback.erase(it); } else ++it;
        }
        removed.clear();   // freed off the audio thread
        if (tk.file.existsAsFile()) tk.file.moveToTrash();
        const auto aligned = tk.file.getSiblingFile(tk.name + " (from 0-00).wav");
        if (aligned.existsAsFile()) aligned.moveToTrash();
        if (role == Role::Host) sendJson(obj({ { "t", "takedel" }, { "id", takeId } }));
    }

    juce::File LinkSession::exportTakeAligned(int takeId) const
    {
        Take tk;
        { const juce::ScopedLock sl(takeLock); for (auto& t : takes) if (t.id == takeId) tk = t; }
        if (!tk.file.existsAsFile()) return {};
        std::unique_ptr<juce::AudioFormatReader> r(juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(tk.file), true));
        if (r == nullptr) return {};
        // Pad with silence from 0:00 so dropping the file at the start of the song lines it up,
        // and write it at the host session's sample rate so the DAW never has to convert it.
        const double outRate = sampleRate > 0.0 ? sampleRate : r->sampleRate;
        const auto out = tk.file.getSiblingFile(tk.name + " (from 0-00, " + juce::String((int) std::lround(outRate)) + " Hz).wav");
        if (out.existsAsFile()) return out;
        std::unique_ptr<juce::FileOutputStream> os(out.createOutputStream());
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w(os ? wav.createWriterFor(os.get(), outRate, 2, 24, {}, 0) : nullptr);
        if (w == nullptr) return {};
        os.release();
        const int64_t pad = (int64_t) std::llround(tk.startSec * outRate);
        juce::AudioBuffer<float> silence(2, 8192);
        silence.clear();
        for (int64_t left = pad; left > 0; left -= 8192) w->writeFromAudioSampleBuffer(silence, 0, (int) juce::jmin((int64_t) 8192, left));

        // Whole take into RAM (vocal takes are small), then resample if the rates differ.
        juce::AudioBuffer<float> src(2, (int) r->lengthInSamples);
        r->read(&src, 0, src.getNumSamples(), 0, true, true);
        if (r->numChannels == 1) src.copyFrom(1, 0, src, 0, 0, src.getNumSamples());
        if (std::abs(r->sampleRate - outRate) < 0.5)
            w->writeFromAudioSampleBuffer(src, 0, src.getNumSamples());
        else
        {
            const double ratio = r->sampleRate / outRate;   // input samples per output sample
            const int outLen = (int) std::floor(src.getNumSamples() / ratio);
            juce::AudioBuffer<float> conv(2, juce::jmax(1, outLen));
            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.process(ratio, src.getReadPointer(ch), conv.getWritePointer(ch), outLen, src.getNumSamples(), 0);
            }
            w->writeFromAudioSampleBuffer(conv, 0, outLen);
        }
        return out;
    }

    //==========================================================================
    // Network worker
    //==========================================================================
    void LinkSession::run()
    {
        uint8_t pkt[kMaxPacket];
        juce::uint32 lastPing = 0;
        while (!threadShouldExit())
        {
            if (encoder)
                while (int n = encoder->pull(pkt, sizeof(pkt)))
                    if (transport) transport->sendAudio(pkt, (size_t) n);

            const auto now = juce::Time::getMillisecondCounter();
            if (role == Role::Host)
            {
                const double ps = pendingStartSec.exchange(-1.0);
                if (ps >= 0.0) hostTakeStarted(ps);
                if (pendingStop.exchange(false)) hostTakeStopped();
                if (now - lastPosSend.load() >= 100 && transport && transport->getState() == LinkTransport::State::Connected)
                {
                    lastPosSend.store(now);
                    sendJson(obj({ { "t", "pos" }, { "sec", posSec.load() }, { "ppq", posPpq.load() }, { "bpm", posBpm.load() },
                                   { "num", posNum.load() }, { "den", posDen.load() }, { "playing", posPlaying.load() }, { "rec", posRecording.load() } }));
                }
            }
            else if (role == Role::Artist)
                pumpOutgoingFile();

            if (now - lastPing > 1000 && transport && transport->getState() == LinkTransport::State::Connected)
            {
                lastPing = now;
                sendJson(obj({ { "t", "ping" }, { "at", (juce::int64) now } }));
            }
            wait(2);
        }
    }

    //==========================================================================
    // Audio thread
    //==========================================================================
    void LinkSession::prepare(double sr, int maxBlock)
    {
        sampleRate = sr;
        maxBlockSize = juce::jmax(64, maxBlock);
        sendBuf.setSize(2, maxBlockSize, false, true, false);
        recvBuf.setSize(2, maxBlockSize, false, true, false);
        if (decoder) decoder->reset();
    }

    void LinkSession::processBlock(juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>* sidechain,
                                   const HostPosition& pos, bool hasPosition)
    {
        const int n = io.getNumSamples();
        if (n > sendBuf.getNumSamples() || role == Role::None || encoder == nullptr || decoder == nullptr)
            return;
        const int ioCh = io.getNumChannels();
        for (int ch = 0; ch < 2; ++ch)
            sendBuf.copyFrom(ch, 0, io, juce::jmin(ch, ioCh - 1), 0, n);
        float* recvPtrs[2] = { recvBuf.getWritePointer(0), recvBuf.getWritePointer(1) };
        const float* sendPtrs[2] = { sendBuf.getReadPointer(0), sendBuf.getReadPointer(1) };

        if (role == Role::Host)
        {
            if (hasPosition)
            {
                posSec.store(pos.seconds); posPpq.store(pos.ppq); posBpm.store(pos.bpm);
                posNum.store(pos.num); posDen.store(pos.den); posPlaying.store(pos.playing); posRecording.store(pos.recording);
            }
            const bool talk = talkback.load();
            if (talk && sidechain != nullptr && sidechain->getNumChannels() > 0)
                for (int ch = 0; ch < 2; ++ch)
                    sendBuf.addFrom(ch, 0, *sidechain, juce::jmin(ch, sidechain->getNumChannels() - 1), 0, n);
            // Metronome for the artist (most DAWs keep their click off the master bus).
            if (sendClick.load() && hasPosition && pos.playing && pos.bpm > 0.0)
            {
                const double beatsPerSample = pos.bpm / 60.0 / sampleRate * (pos.den / 4.0);
                const double beat0 = pos.ppq * (pos.den / 4.0);
                for (int i = 0; i < n; ++i)
                {
                    const double b = beat0 + i * beatsPerSample;
                    if (std::floor(b) != std::floor(lastBeat) && lastBeat >= 0.0 && b > lastBeat)
                    {
                        const bool down = ((long long) std::floor(b) % juce::jmax(1, pos.num)) == 0;
                        clickLeft = (int) (sampleRate * 0.03);
                        clickAmp = down ? 0.5f : 0.32f;
                        clickPhase = down ? 2000.0 : 1300.0;   // reused as frequency
                    }
                    lastBeat = b;
                    if (clickLeft > 0)
                    {
                        const float env = (float) clickLeft / (float) (sampleRate * 0.03);
                        const float v = clickAmp * env * env * (float) std::sin(juce::MathConstants<double>::twoPi * clickPhase * (double) clickLeft / sampleRate);
                        sendBuf.addSample(0, i, v); sendBuf.addSample(1, i, v);
                        --clickLeft;
                    }
                }
            }
            else lastBeat = -1.0;
            const auto stamp = (int64_t) std::llround(pos.seconds * kNetRate);
            const uint8_t flags = (uint8_t) ((pos.playing ? 1 : 0) | (pos.recording ? 2 : 0) | (talk ? 4 : 0));
            encoder->push(sendPtrs, 2, n, sampleRate, stamp, flags);

            const auto st = decoder->render(recvPtrs, 2, n, sampleRate);

            const bool rec = hasPosition && pos.recording && recordEnabled.load();
            if (rec && !hostWasRecording.load()) pendingStartSec.store(pos.seconds);
            if (!rec && hostWasRecording.load()) pendingStop.store(true);
            hostWasRecording.store(rec);
            if (rec)
            {
                const juce::SpinLock::ScopedTryLockType tl(writerLock);
                if (tl.isLocked() && liveWriter != nullptr)
                {
                    if (liveStartSec.load() < 0.0 && st >= 0) liveStartSec.store((double) st / kNetRate);
                    if (liveStartSec.load() >= 0.0) liveWriter->write(recvBuf.getArrayOfReadPointers(), n);
                }
            }

            sendPeakL.store(peakOf(sendBuf, 0, n)); sendPeakR.store(peakOf(sendBuf, 1, n));
            recvPeakL.store(peakOf(recvBuf, 0, n)); recvPeakR.store(peakOf(recvBuf, 1, n));

            // Hear the artist live through this channel.
            const float g = monitorGain.load();
            for (int ch = 0; ch < ioCh; ++ch)
                io.addFrom(ch, 0, recvBuf, juce::jmin(ch, 1), 0, n, g);

            // Play finished takes back in sync with the song (when not recording).
            if (hasPosition && pos.playing && !pos.recording)
            {
                const juce::SpinLock::ScopedTryLockType tl(playbackLock);
                if (tl.isLocked())
                    for (auto& p : playback)
                    {
                        const double ratio = p->fileRate / sampleRate;
                        const double startIdx = (pos.seconds - p->startSec) * p->fileRate;
                        const int len = p->audio.getNumSamples();
                        if (startIdx + n * ratio < 0 || startIdx >= len) continue;
                        for (int ch = 0; ch < ioCh; ++ch)
                        {
                            const float* src = p->audio.getReadPointer(juce::jmin(ch, 1));
                            float* dst = io.getWritePointer(ch);
                            for (int i = 0; i < n; ++i)
                            {
                                const double x = startIdx + i * ratio;
                                const int k = (int) std::floor(x);
                                if (k < 0 || k + 1 >= len) continue;
                                const float f = (float) (x - k);
                                dst[i] += (src[k] + (src[k + 1] - src[k]) * f) * g;
                            }
                        }
                    }
            }
        }
        else   // Artist
        {
            uint8_t hostFlags = 0;
            const auto st = decoder->render(recvPtrs, 2, n, sampleRate, &hostFlags);
            const int64_t stamp = st >= 0 ? st : (int64_t) std::llround(posSec.load() * kNetRate);
            encoder->push(sendPtrs, 2, n, sampleRate, stamp, hostFlags);

            if (activeTake.load() != 0)
            {
                const juce::SpinLock::ScopedTryLockType tl(writerLock);
                if (tl.isLocked() && localWriter != nullptr)
                {
                    if (localNeedsStart.load() && st >= 0) { localStartSec.store((double) st / kNetRate); localNeedsStart.store(false); }
                    if (!localNeedsStart.load()) localWriter->write(sendBuf.getArrayOfReadPointers(), n);
                }
            }

            sendPeakL.store(peakOf(sendBuf, 0, n)); sendPeakR.store(peakOf(sendBuf, 1, n));
            recvPeakL.store(peakOf(recvBuf, 0, n)); recvPeakR.store(peakOf(recvBuf, 1, n));

            // What the artist hears: the engineer's mix (+ their own mic if they want it).
            const float g = monitorGain.load();
            const bool self = hearSelf.load();
            for (int ch = 0; ch < ioCh; ++ch)
            {
                if (!self) io.clear(ch, 0, n);
                io.addFrom(ch, 0, recvBuf, juce::jmin(ch, 1), 0, n, g);
            }
        }
    }

    //==========================================================================
    juce::var LinkSession::getStateJson() const
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("role", role == Role::Host ? "host" : role == Role::Artist ? "artist" : "none");
        o->setProperty("code", code);
        o->setProperty("state", transport ? juce::String(LinkTransport::stateName(transport->getState())) : juce::String("Idle"));
        o->setProperty("peer", getPeerName());
        o->setProperty("me", myName);
        o->setProperty("rtt", rttMs.load());
        if (decoder)
        {
            o->setProperty("jitter", decoder->getJitterMs());
            o->setProperty("loss", decoder->getLossPercent());
            o->setProperty("buffer", decoder->getBufferMs());
            o->setProperty("receiving", decoder->isReceiving());
        }
        o->setProperty("sendPeak", juce::Array<juce::var> { sendPeakL.load(), sendPeakR.load() });
        o->setProperty("recvPeak", juce::Array<juce::var> { recvPeakL.load(), recvPeakR.load() });
        o->setProperty("pos", obj({ { "sec", posSec.load() }, { "ppq", posPpq.load() }, { "bpm", posBpm.load() }, { "num", posNum.load() },
                                    { "den", posDen.load() }, { "playing", posPlaying.load() }, { "rec", posRecording.load() } }));
        o->setProperty("activeTake", activeTake.load());
        o->setProperty("talkback", talkback.load());
        o->setProperty("recordEnabled", recordEnabled.load());
        o->setProperty("hearSelf", hearSelf.load());
        o->setProperty("lossless", lossless.load());
        o->setProperty("sendClick", sendClick.load());
        o->setProperty("monitorGain", monitorGain.load());
        {
            juce::Array<juce::var> arr;
            std::vector<int> playingIds;
            {
                const juce::SpinLock::ScopedLockType pl(playbackLock);
                for (auto& p : playback) playingIds.push_back(p->id);
            }
            const juce::ScopedLock sl(takeLock);
            for (auto& tk : takes)
                arr.add(obj({ { "id", tk.id }, { "name", tk.name }, { "start", tk.startSec }, { "length", tk.lengthSec },
                              { "state", tk.state }, { "progress", tk.progress }, { "file", tk.file.getFullPathName() },
                              { "playing", std::find(playingIds.begin(), playingIds.end(), tk.id) != playingIds.end() } }));
            o->setProperty("takes", arr);
        }
        {
            juce::Array<juce::var> c, s;
            const juce::ScopedLock sl(chatLock);
            for (auto& l : chat)
                c.add(obj({ { "from", l.from }, { "text", l.text }, { "marker", l.markerSec }, { "take", l.take }, { "mine", l.from == myName } }));
            for (auto& x : sections) s.add(obj({ { "sec", x.sec }, { "name", x.name } }));
            o->setProperty("chat", c);
            o->setProperty("sections", s);
        }
        return juce::var(o);
    }
}
