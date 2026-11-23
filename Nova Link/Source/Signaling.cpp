#include "Signaling.h"

namespace NovaLink
{
    std::pair<std::shared_ptr<LoopbackSignaling>, std::shared_ptr<LoopbackSignaling>> LoopbackSignaling::makePair()
    {
        auto a = std::make_shared<LoopbackSignaling>();
        auto b = std::make_shared<LoopbackSignaling>();
        a->peer = b;
        b->peer = a;
        return { a, b };
    }

    void LoopbackSignaling::send(const std::string& message)
    {
        if (auto p = peer.lock())
        {
            // Deliver asynchronously, like a network would.
            auto copy = message;
            std::weak_ptr<LoopbackSignaling> target = p;
            std::thread([target, copy] { if (auto t = target.lock()) if (t->onMessage) t->onMessage(copy); }).detach();
        }
    }

    juce::String makeSessionCode()
    {
        static const char* alphabet = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";   // no 0/O, 1/I/L
        juce::Random r(juce::Time::getHighResolutionTicks() ^ (juce::int64) juce::Time::currentTimeMillis());
        juce::String s = "NOVA-";
        for (int i = 0; i < 8; ++i)
        {
            if (i == 4) s << "-";
            s << alphabet[r.nextInt((int) std::strlen(alphabet))];
        }
        return s;
    }

    juce::String normaliseSessionCode(juce::String typed)
    {
        // Accepts "nova-7k2q-9mx4", "7K2Q 9MX4", "7k2q9mx4" ...
        typed = typed.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        if (typed.startsWith("NOVA") && typed.length() == 12) typed = typed.substring(4);
        if (typed.length() != 8 || !typed.containsOnly("23456789ABCDEFGHJKMNPQRSTUVWXYZ")) return {};
        return "NOVA-" + typed.substring(0, 4) + "-" + typed.substring(4);
    }

    //==========================================================================
    juce::String HttpSignaling::defaultServer()
    {
       #ifndef NOVA_LINK_SIGNAL_URL
        #define NOVA_LINK_SIGNAL_URL "https://nova-store-olive.vercel.app"
       #endif
        const auto env = juce::SystemStats::getEnvironmentVariable("NOVA_LINK_SIGNAL_URL", {});
        return env.isNotEmpty() ? env : juce::String(NOVA_LINK_SIGNAL_URL);
    }

    HttpSignaling::HttpSignaling(juce::String serverUrl, juce::String c, juce::String r)
        : juce::Thread("Nova Link matchmaking"), server(std::move(serverUrl)), code(std::move(c)), role(std::move(r))
    {
        other = role == "host" ? "artist" : "host";
    }

    HttpSignaling::~HttpSignaling() { stop(); }

    void HttpSignaling::start() { startThread(); }
    void HttpSignaling::stop() { signalThreadShouldExit(); notify(); stopThread(4000); }

    void HttpSignaling::send(const std::string& message)
    {
        { std::lock_guard<std::mutex> g(outLock); outbox.push_back(message); }
        notify();
    }

    void HttpSignaling::run()
    {
        int failures = 0;
        while (!threadShouldExit())
        {
            // Post anything queued, in order.
            std::vector<std::string> toSend;
            { std::lock_guard<std::mutex> g(outLock); toSend.swap(outbox); }
            for (auto& m : toSend)
            {
                auto* body = new juce::DynamicObject();
                body->setProperty("code", code);
                body->setProperty("from", role);
                body->setProperty("seq", nextSeqOut);
                body->setProperty("message", juce::String(m));
                const auto url = juce::URL(server + "/api/link/signal").withPOSTData(juce::JSON::toString(juce::var(body)));
                int status = 0;
                auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                                        .withExtraHeaders("Content-Type: application/json")
                                                        .withConnectionTimeoutMs(8000).withStatusCode(&status));
                if (stream != nullptr && status == 200) { stream->readEntireStreamAsString(); ++nextSeqOut; failures = 0; }
                else
                {
                    // Put it back and retry later.
                    std::lock_guard<std::mutex> g(outLock);
                    outbox.insert(outbox.begin(), m);
                    if (++failures == 5 && onError) onError("Can't reach the Nova Link matchmaker - check the internet connection.");
                    break;
                }
            }

            // Fetch the other side's new messages.
            const auto url = juce::URL(server + "/api/link/signal")
                                 .withParameter("code", code).withParameter("for", role).withParameter("after", juce::String(nextSeqIn - 1));
            int status = 0;
            auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                    .withConnectionTimeoutMs(8000).withStatusCode(&status));
            if (stream != nullptr && status == 200)
            {
                const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
                if (auto* arr = parsed.getProperty("messages", {}).getArray())
                    for (auto& m : *arr)
                    {
                        const int seq = (int) m.getProperty("seq", -1);
                        if (seq != nextSeqIn) continue;   // strictly in order
                        ++nextSeqIn;
                        if (onMessage) onMessage(m.getProperty("message", "").toString().toStdString());
                    }
            }
            wait(relaxed.load() ? 5000 : 500);
        }
    }
}
