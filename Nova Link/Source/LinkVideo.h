#pragma once
#include <juce_video/juce_video.h>
#include <mutex>
#include "LinkSession.h"

namespace NovaLink
{
    // Webcam video both ways, as JPEG frames over the video channel (~15 fps, 640 px wide).
    // Frames that can't be sent in time are dropped, so video never delays the audio.
    class LinkVideo : private juce::CameraDevice::Listener
    {
    public:
        explicit LinkVideo(LinkSession& s);
        ~LinkVideo() override;

        // Returns an empty string on success, otherwise why the camera can't be used.
        juce::String start();
        void stop();
        bool isRunning() const noexcept { return camera != nullptr; }

        juce::MemoryBlock getRemoteFrame(int& counter) const;
        juce::MemoryBlock getSelfFrame() const;
        float getRemoteFps() const noexcept { return remoteFps.load(); }

        // macOS terminates an app that opens the camera without declaring it (Info.plist
        // NSCameraUsageDescription). Some DAWs don't declare it, so check first.
        static bool hostAllowsCamera();

    private:
        void imageReceived(const juce::Image& image) override;

        LinkSession& session;
        std::unique_ptr<juce::CameraDevice> camera;
        mutable std::mutex lock;
        juce::MemoryBlock remote, self;
        int remoteCounter = 0;
        juce::uint32 lastSent = 0, fpsWindowStart = 0;
        int fpsFrames = 0;
        std::atomic<float> remoteFps { 0.0f };
        float quality = 0.6f;
    };
}
