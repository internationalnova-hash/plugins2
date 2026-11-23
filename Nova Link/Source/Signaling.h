#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace NovaLink
{
    // Matchmaking: lets two studios find each other with a session code and exchange the
    // connection details (WebRTC offer/answer + network candidates). After that, all audio,
    // video and files go directly between the studios; the matchmaker sees none of it.
    class Signaling
    {
    public:
        virtual ~Signaling() = default;
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void send(const std::string& message) = 0;

        // Called on a background thread for each message from the other studio, in order.
        std::function<void(const std::string&)> onMessage;
        std::function<void(const juce::String&)> onError;
    };

    // In-memory pair for tests: what one side sends, the other receives.
    class LoopbackSignaling : public Signaling
    {
    public:
        static std::pair<std::shared_ptr<LoopbackSignaling>, std::shared_ptr<LoopbackSignaling>> makePair();
        void start() override {}
        void stop() override {}
        void send(const std::string& message) override;
    private:
        std::weak_ptr<LoopbackSignaling> peer;
    };

    // Uses the Nova store's /api/link endpoints. Each side posts its messages and polls for
    // the other side's. `code` is the session code ("NOVA-7K2Q-9MX4"); role is "host"/"artist".
    class HttpSignaling : public Signaling, private juce::Thread
    {
    public:
        HttpSignaling(juce::String serverUrl, juce::String code, juce::String role);
        ~HttpSignaling() override;
        void start() override;
        void stop() override;
        void send(const std::string& message) override;

    private:
        void run() override;
        juce::String server, code, role, other;
        std::mutex outLock;
        std::vector<std::string> outbox;
        int nextSeqOut = 0, nextSeqIn = 0;
    };

    // "NOVA-7K2Q-9MX4": 8 random characters (no look-alike letters), easy to read out.
    juce::String makeSessionCode();
    juce::String normaliseSessionCode(juce::String typed);
}
