#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "LinkTransport.h"
#include "LinkAudio.h"
#include <map>

namespace NovaLink
{
    // Where the host's song is right now (sent to the artist ~10x a second).
    struct HostPosition
    {
        double seconds = 0.0, ppq = 0.0, bpm = 120.0;
        int num = 4, den = 4;
        bool playing = false, recording = false;
    };

    struct Take
    {
        int id = 0;
        juce::String name;
        double startSec = 0.0, lengthSec = 0.0;
        juce::String state;          // "recording", "transferring", "ready", "live"
        float progress = 0.0f;       // transfer 0..1
        juce::File file;
        juce::String note;
    };

    struct ChatLine { juce::String from, text; double markerSec = -1.0; int take = 0; juce::int64 time = 0; };
    struct Section { double sec = 0.0; juce::String name; };

    // One studio's side of a Nova Link session. The host (engineer) sends the mix and
    // owns the timeline; the artist sends their mic and records lossless takes locally,
    // which are transferred to the host after each take.
    class LinkSession : private juce::Thread
    {
    public:
        enum class Role { None, Host, Artist };

        LinkSession();
        ~LinkSession() override;

        // Message thread
        juce::String hostSession(juce::String displayName);                  // returns the code
        bool joinSession(juce::String code, juce::String displayName);       // false = bad code
        void leave();
        void startWithSignaling(Role role, std::shared_ptr<Signaling> sig, juce::String code, juce::String displayName);   // tests

        Role getRole() const noexcept { return role; }
        juce::String getCode() const { return code; }
        LinkTransport::State getState() const noexcept { return transport ? transport->getState() : LinkTransport::State::Idle; }
        juce::String getPeerName() const;

        void sendChat(const juce::String& text);
        void addMarker(const juce::String& text);
        void setSections(const juce::Array<Section>& s);
        void setTalkback(bool on) noexcept { talkback.store(on); }
        void setRecordEnabled(bool on) noexcept { recordEnabled.store(on); }
        void setMonitorGain(float g) noexcept { monitorGain.store(g); }   // host: artist level / artist: engineer mix level
        void setHearSelf(bool on) noexcept { hearSelf.store(on); }
        void setLossless(bool on) noexcept { lossless.store(on); }
        void setSendClick(bool on) noexcept { sendClick.store(on); }   // host: metronome in the artist's mix
        void setMonoTakes(bool on) noexcept { monoTakes.store(on); }   // host: artist records mono (half the transfer)
        juce::File exportTakeAligned(int takeId) const;                    // WAV padded from 0:00 for drag-and-drop
        // Host: play this take over its part of the song (replacing whichever overlaps it).
        void playTake(int takeId);
        // Host: remove a take from the session (files go to the Trash on both sides).
        void deleteTake(int takeId);

        juce::var getStateJson() const;                                   // for the UI

        // Video frames (JPEG) from the other studio arrive here (network thread).
        std::function<void(const uint8_t*, size_t)> videoSink;
        bool sendVideoFrame(const void* data, size_t size) { return transport && transport->sendVideo(data, size); }

        // Audio thread. `sidechain` may be null (talkback mic on the host's sidechain input).
        void prepare(double sampleRate, int maxBlock);
        void processBlock(juce::AudioBuffer<float>& io, const juce::AudioBuffer<float>* sidechain,
                          const HostPosition& pos, bool hasPosition);

        static juce::File baseFolder();

    private:
        void run() override;                                 // network worker
        void onControl(const std::string& json);
        void onFileChunk(const uint8_t* data, size_t size);
        void sendJson(const juce::var& v);
        void startLocalTake(int id, double startSec, bool mono);        // artist
        void finishLocalTake(int id);                        // artist
        void pumpOutgoingFile();                             // artist
        void hostTakeStarted(double startSec);               // host
        void hostTakeStopped();
        void loadTakeForPlayback(const Take& t);
        void setPeerName(const juce::String& n);

        Role role = Role::None;
        juce::String code, myName;
        std::shared_ptr<LinkTransport> transport;
        std::unique_ptr<StreamEncoder> encoder;
        std::unique_ptr<StreamDecoder> decoder;
        double sampleRate = 48000.0;
        int maxBlockSize = 512;
        juce::AudioBuffer<float> sendBuf, recvBuf;

        std::atomic<bool> talkback { false }, recordEnabled { true }, hearSelf { true }, lossless { true }, sendClick { true }, monoTakes { true };
        double clickPhase = 0.0; int clickLeft = 0; float clickAmp = 0.0f; double lastBeat = -1.0;
        std::atomic<float> monitorGain { 1.0f };
        std::atomic<float> sendPeakL { 0 }, sendPeakR { 0 }, recvPeakL { 0 }, recvPeakR { 0 };

        // host position (host writes on audio thread; artist receives over control)
        std::atomic<double> posSec { 0.0 }, posPpq { 0.0 }, posBpm { 120.0 };
        std::atomic<int> posNum { 4 }, posDen { 4 };
        std::atomic<bool> posPlaying { false }, posRecording { false };

        // Takes
        mutable juce::CriticalSection takeLock;
        juce::Array<Take> takes;
        std::atomic<int> nextTakeId { 1 };
        std::atomic<int> activeTake { 0 };       // host: take being recorded; artist: take being captured
        std::atomic<bool> hostWasRecording { false };
        std::atomic<double> pendingStartSec { -1.0 };
        std::atomic<bool> pendingStop { false };

        // Artist local capture (lossless)
        juce::TimeSliceThread writerThread { "Nova Link take writer" };
        juce::SpinLock writerLock;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> localWriter;
        juce::File localFile;
        std::atomic<double> localStartSec { -1.0 };
        std::atomic<bool> localNeedsStart { false };
        std::atomic<bool> localMono { false };
        juce::AudioBuffer<float> monoBuf;
        // Artist outgoing file transfer
        struct Outgoing { int id; juce::File file; double startSec; int64_t size; int64_t sent; std::unique_ptr<juce::FileInputStream> in; };
        std::unique_ptr<Outgoing> outgoing;
        juce::Array<int> outgoingQueue;
        std::map<int, double> outgoingStarts;
        // Host incoming file transfer
        struct Incoming { int id; int64_t size; int64_t got; double startSec; std::unique_ptr<juce::FileOutputStream> out; juce::File file; };
        std::map<int, Incoming> incoming;
        std::mutex incomingLock;

        // Host: live-stream capture as a fallback take (written at host rate)
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> liveWriter;
        juce::File liveFile;
        std::atomic<double> liveStartSec { -1.0 };

        // Host take playback (latest ready takes, in RAM)
        struct PlaybackTake { int id = 0; double startSec; double fileRate; juce::AudioBuffer<float> audio; };
        juce::SpinLock playbackLock;
        std::vector<std::shared_ptr<PlaybackTake>> playback;

        mutable juce::CriticalSection chatLock;
        juce::Array<ChatLine> chat;
        juce::Array<Section> sections;
        juce::String peerName;
        juce::File sessionFolder;
        std::atomic<juce::uint32> lastPosSend { 0 };
        std::atomic<float> rttMs { 0.0f };
        std::atomic<juce::uint32> pingSentAt { 0 };
    };
}
