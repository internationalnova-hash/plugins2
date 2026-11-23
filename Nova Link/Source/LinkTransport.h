#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "Signaling.h"

namespace rtc { class PeerConnection; class DataChannel; }

namespace NovaLink
{
    // The direct connection between the two studios (WebRTC data channels: encrypted,
    // peer-to-peer, NAT traversal via STUN). Four channels:
    //   control - reliable, ordered: JSON (chat, markers, transport, take control, talkback)
    //   audio   - unreliable, unordered: Opus packets (late packets are useless, never resent)
    //   video   - unreliable, unordered: JPEG frames
    //   file    - reliable, ordered: lossless take transfer
    // Callbacks arrive on network threads.
    class LinkTransport : public std::enable_shared_from_this<LinkTransport>
    {
    public:
        enum class Role { Host, Artist };
        enum class State { Idle, Waiting, Connecting, Connected, Failed, Closed };

        LinkTransport();
        ~LinkTransport();

        void start(Role role, std::shared_ptr<Signaling> signaling, std::vector<std::string> extraIceServers = {});
        void stop();

        State getState() const noexcept { return state.load(); }
        Role getRole() const noexcept { return role; }

        bool sendControl(const std::string& json);
        bool sendAudio(const void* data, size_t size);
        bool sendVideo(const void* data, size_t size);
        bool sendFile(const void* data, size_t size);        // false = buffer full, try again shortly
        size_t fileBufferedAmount() const;
        size_t maxMessageSize() const;

        std::function<void(State)> onState;
        std::function<void(const std::string&)> onControl;
        std::function<void(const uint8_t*, size_t)> onAudio, onVideo, onFile;
        std::function<void(const juce::String&)> onError;

        static const char* stateName(State s);

    private:
        void setState(State s);
        void createPeer();
        void handleSignal(const std::string& message);
        void wireChannel(std::shared_ptr<rtc::DataChannel> dc);
        void checkAllOpen();

        Role role = Role::Host;
        std::atomic<State> state { State::Idle };
        std::shared_ptr<Signaling> signaling;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> ctl, audio, video, file;
        std::vector<std::string> iceServers;
        mutable std::mutex lock;
        std::vector<std::string> pendingCandidates;
        bool haveRemote = false;
    };
}
