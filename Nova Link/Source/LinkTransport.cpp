#include "LinkTransport.h"
#include <rtc/rtc.hpp>

namespace NovaLink
{
    namespace
    {
        std::string jsonMessage(const juce::String& type, const juce::String& key = {}, const juce::String& value = {},
                                const juce::String& key2 = {}, const juce::String& value2 = {})
        {
            auto* o = new juce::DynamicObject();
            o->setProperty("t", type);
            if (key.isNotEmpty()) o->setProperty(key, value);
            if (key2.isNotEmpty()) o->setProperty(key2, value2);
            return juce::JSON::toString(juce::var(o), true).toStdString();
        }
    }

    const char* LinkTransport::stateName(State s)
    {
        switch (s)
        {
            case State::Idle: return "Idle";
            case State::Waiting: return "Waiting";
            case State::Connecting: return "Connecting";
            case State::Connected: return "Connected";
            case State::Failed: return "Failed";
            case State::Closed: return "Closed";
        }
        return "?";
    }

    LinkTransport::LinkTransport()
    {
        static std::once_flag logOnce;
        std::call_once(logOnce, [] { rtc::InitLogger(rtc::LogLevel::Warning); });
    }

    LinkTransport::~LinkTransport() { stop(); }

    void LinkTransport::setState(State s)
    {
        if (state.exchange(s) != s && onState) onState(s);
    }

    void LinkTransport::start(Role r, std::shared_ptr<Signaling> sig, std::vector<std::string> extraIceServers)
    {
        stop();
        role = r;
        signaling = std::move(sig);
        iceServers = { "stun:stun.l.google.com:19302", "stun:stun.cloudflare.com:3478" };
        for (auto& s : extraIceServers) iceServers.push_back(s);

        std::weak_ptr<LinkTransport> weak = weak_from_this();
        signaling->onMessage = [weak] (const std::string& m) { if (auto self = weak.lock()) self->handleSignal(m); };
        signaling->onError = [weak] (const juce::String& e) { if (auto self = weak.lock()) if (self->onError) self->onError(e); };
        signaling->start();
        setState(State::Waiting);

        // The artist announces itself; the host then makes the offer.
        if (role == Role::Artist)
            signaling->send(jsonMessage("hello"));
    }

    void LinkTransport::stop()
    {
        if (signaling) { signaling->onMessage = nullptr; signaling->stop(); }
        std::shared_ptr<rtc::PeerConnection> oldPc;
        {
            std::lock_guard<std::mutex> g(lock);
            for (auto* dc : { &ctl, &audio, &video, &file })
                if (*dc) { (*dc)->resetCallbacks(); (*dc)->close(); dc->reset(); }
            oldPc = std::move(pc);
            pendingCandidates.clear();
            haveRemote = false;
        }
        if (oldPc) { oldPc->resetCallbacks(); oldPc->close(); }
        if (state.load() != State::Idle) setState(State::Closed);
    }

    void LinkTransport::createPeer()
    {
        rtc::Configuration config;
        for (auto& s : iceServers) config.iceServers.emplace_back(s);
        config.maxMessageSize = 256 * 1024;
        auto peer = std::make_shared<rtc::PeerConnection>(config);
        std::weak_ptr<LinkTransport> weak = weak_from_this();

        peer->onLocalDescription([weak] (rtc::Description d)
        {
            if (auto self = weak.lock())
                self->signaling->send(jsonMessage("sdp", "kind", d.typeString(), "sdp", std::string(d)));
        });
        peer->onLocalCandidate([weak] (rtc::Candidate c)
        {
            if (auto self = weak.lock())
                self->signaling->send(jsonMessage("cand", "cand", std::string(c), "mid", c.mid()));
        });
        peer->onStateChange([weak] (rtc::PeerConnection::State s)
        {
            auto self = weak.lock();
            if (!self) return;
            if (s == rtc::PeerConnection::State::Failed) self->setState(State::Failed);
            else if (s == rtc::PeerConnection::State::Disconnected || s == rtc::PeerConnection::State::Closed)
            {
                if (self->state.load() == State::Connected) self->setState(State::Failed);
            }
            else if (s == rtc::PeerConnection::State::Connecting) self->setState(State::Connecting);
        });
        peer->onDataChannel([weak] (std::shared_ptr<rtc::DataChannel> dc)
        {
            if (auto self = weak.lock()) self->wireChannel(dc);
        });

        {
            std::lock_guard<std::mutex> g(lock);
            pc = peer;
        }

        if (role == Role::Host)
        {
            rtc::DataChannelInit reliable;
            rtc::DataChannelInit lossy;
            lossy.reliability.unordered = true;
            lossy.reliability.maxRetransmits = 0;
            wireChannel(peer->createDataChannel("control", reliable));
            wireChannel(peer->createDataChannel("audio", lossy));
            wireChannel(peer->createDataChannel("video", lossy));
            wireChannel(peer->createDataChannel("file", reliable));
        }
    }

    void LinkTransport::wireChannel(std::shared_ptr<rtc::DataChannel> dc)
    {
        const auto label = dc->label();
        std::weak_ptr<LinkTransport> weak = weak_from_this();
        {
            std::lock_guard<std::mutex> g(lock);
            if (label == "control") ctl = dc;
            else if (label == "audio") audio = dc;
            else if (label == "video") video = dc;
            else if (label == "file") file = dc;
        }
        dc->onOpen([weak] { if (auto self = weak.lock()) self->checkAllOpen(); });
        dc->onClosed([weak]
        {
            if (auto self = weak.lock())
                if (self->state.load() == State::Connected) self->setState(State::Failed);
        });
        dc->onMessage([weak, label] (rtc::message_variant data)
        {
            auto self = weak.lock();
            if (!self) return;
            if (const auto* str = std::get_if<std::string>(&data))
            {
                if (label == "control" && self->onControl) self->onControl(*str);
                return;
            }
            const auto& bin = std::get<rtc::binary>(data);
            const auto* bytes = reinterpret_cast<const uint8_t*>(bin.data());
            if (label == "audio" && self->onAudio) self->onAudio(bytes, bin.size());
            else if (label == "video" && self->onVideo) self->onVideo(bytes, bin.size());
            else if (label == "file" && self->onFile) self->onFile(bytes, bin.size());
        });
        if (dc->isOpen()) checkAllOpen();
    }

    void LinkTransport::checkAllOpen()
    {
        bool all = false;
        {
            std::lock_guard<std::mutex> g(lock);
            all = ctl && audio && video && file && ctl->isOpen() && audio->isOpen() && video->isOpen() && file->isOpen();
        }
        if (all) setState(State::Connected);
    }

    void LinkTransport::handleSignal(const std::string& message)
    {
        const auto v = juce::JSON::parse(juce::String(message));
        const auto t = v.getProperty("t", "").toString();
        try
        {
            if (t == "hello" && role == Role::Host)
            {
                // A (new) artist arrived: start a fresh connection; this sends the offer.
                {
                    std::shared_ptr<rtc::PeerConnection> old;
                    { std::lock_guard<std::mutex> g(lock); old = std::move(pc); haveRemote = false; pendingCandidates.clear(); }
                    if (old) { old->resetCallbacks(); old->close(); }
                }
                setState(State::Connecting);
                createPeer();
            }
            else if (t == "sdp")
            {
                if (role == Role::Artist && pc == nullptr) { setState(State::Connecting); createPeer(); }
                std::shared_ptr<rtc::PeerConnection> peer;
                { std::lock_guard<std::mutex> g(lock); peer = pc; }
                if (!peer) return;
                peer->setRemoteDescription(rtc::Description(v.getProperty("sdp", "").toString().toStdString(),
                                                            v.getProperty("kind", "").toString().toStdString()));
                std::vector<std::string> pend;
                { std::lock_guard<std::mutex> g(lock); haveRemote = true; pend.swap(pendingCandidates); }
                for (auto& c : pend)
                {
                    const auto cv = juce::JSON::parse(juce::String(c));
                    peer->addRemoteCandidate(rtc::Candidate(cv.getProperty("cand", "").toString().toStdString(),
                                                            cv.getProperty("mid", "").toString().toStdString()));
                }
            }
            else if (t == "cand")
            {
                std::shared_ptr<rtc::PeerConnection> peer;
                bool ready = false;
                {
                    std::lock_guard<std::mutex> g(lock);
                    peer = pc; ready = haveRemote;
                    if (!ready || !peer) { pendingCandidates.push_back(message); return; }
                }
                peer->addRemoteCandidate(rtc::Candidate(v.getProperty("cand", "").toString().toStdString(),
                                                        v.getProperty("mid", "").toString().toStdString()));
            }
        }
        catch (const std::exception& e)
        {
            if (onError) onError(juce::String("Connection error: ") + e.what());
            setState(State::Failed);
        }
    }

    namespace
    {
        bool sendOn(const std::shared_ptr<rtc::DataChannel>& dc, const void* data, size_t size)
        {
            if (!dc || !dc->isOpen()) return false;
            // send() returns false when the data was queued rather than sent at once;
            // either way it's on its way. Only an exception means it was rejected.
            try { dc->send(reinterpret_cast<const std::byte*>(data), size); return true; }
            catch (...) { return false; }
        }
    }

    bool LinkTransport::sendControl(const std::string& json)
    {
        std::shared_ptr<rtc::DataChannel> dc;
        { std::lock_guard<std::mutex> g(lock); dc = ctl; }
        if (!dc || !dc->isOpen()) return false;
        try { dc->send(json); return true; } catch (...) { return false; }
    }

    bool LinkTransport::sendAudio(const void* d, size_t n)
    {
        std::shared_ptr<rtc::DataChannel> dc;
        { std::lock_guard<std::mutex> g(lock); dc = audio; }
        return sendOn(dc, d, n);
    }

    bool LinkTransport::sendVideo(const void* d, size_t n)
    {
        std::shared_ptr<rtc::DataChannel> dc;
        { std::lock_guard<std::mutex> g(lock); dc = video; }
        if (dc && dc->bufferedAmount() > 512 * 1024) return false;   // drop frames rather than lag
        return sendOn(dc, d, n);
    }

    bool LinkTransport::sendFile(const void* d, size_t n)
    {
        std::shared_ptr<rtc::DataChannel> dc;
        { std::lock_guard<std::mutex> g(lock); dc = file; }
        if (!dc || dc->bufferedAmount() > 4 * 1024 * 1024) return false;
        return sendOn(dc, d, n);
    }

    size_t LinkTransport::fileBufferedAmount() const
    {
        std::lock_guard<std::mutex> g(lock);
        return file ? file->bufferedAmount() : 0;
    }

    size_t LinkTransport::maxMessageSize() const
    {
        std::lock_guard<std::mutex> g(lock);
        return ctl ? ctl->maxMessageSize() : 16384;
    }
}
