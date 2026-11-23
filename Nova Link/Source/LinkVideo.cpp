#include "LinkVideo.h"

namespace NovaLink
{
    LinkVideo::LinkVideo(LinkSession& s) : session(s)
    {
        session.videoSink = [this] (const uint8_t* d, size_t n)
        {
            const auto now = juce::Time::getMillisecondCounter();
            std::lock_guard<std::mutex> g(lock);
            remote.replaceAll(d, n);
            ++remoteCounter;
            ++fpsFrames;
            if (now - fpsWindowStart >= 1000)
            {
                remoteFps.store((float) fpsFrames * 1000.0f / (float) juce::jmax(1u, now - fpsWindowStart));
                fpsFrames = 0;
                fpsWindowStart = now;
            }
        };
    }

    LinkVideo::~LinkVideo()
    {
        stop();
        session.videoSink = nullptr;
    }

   #if ! JUCE_MAC
    bool LinkVideo::hostAllowsCamera() { return true; }
   #endif

    juce::String LinkVideo::start()
    {
        if (camera != nullptr) return {};
        if (!hostAllowsCamera())
            return "This DAW doesn't allow plugins to use the camera. Audio, chat and takes still work.";
        const auto devices = juce::CameraDevice::getAvailableDevices();
        if (devices.isEmpty()) return "No camera found.";
        camera.reset(juce::CameraDevice::openDevice(0, 320, 180, 1280, 720, false));
        if (camera == nullptr) return "The camera couldn't be opened (check camera permission in System Settings).";
        camera->addListener(this);
        return {};
    }

    void LinkVideo::stop()
    {
        if (camera != nullptr) { camera->removeListener(this); camera.reset(); }
        std::lock_guard<std::mutex> g(lock);
        self.reset();
    }

    void LinkVideo::imageReceived(const juce::Image& image)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastSent < 66) return;   // ~15 fps
        lastSent = now;

        const int w = 640, h = juce::jmax(1, image.getHeight() * w / juce::jmax(1, image.getWidth()));
        const auto scaled = image.rescaled(w, h, juce::Graphics::mediumResamplingQuality);
        juce::JPEGImageFormat jpeg;
        juce::MemoryOutputStream out;
        jpeg.setQuality(quality);
        if (!jpeg.writeImageToStream(scaled, out)) return;

        // Keep frames under one network message; adapt quality to fit.
        if (out.getDataSize() > 56000) quality = juce::jmax(0.25f, quality - 0.05f);
        else if (out.getDataSize() < 30000) quality = juce::jmin(0.75f, quality + 0.02f);
        if (out.getDataSize() > 60000) return;

        { std::lock_guard<std::mutex> g(lock); self.replaceAll(out.getData(), out.getDataSize()); }
        session.sendVideoFrame(out.getData(), out.getDataSize());
    }

    juce::MemoryBlock LinkVideo::getRemoteFrame(int& counter) const
    {
        std::lock_guard<std::mutex> g(lock);
        counter = remoteCounter;
        return remote;
    }

    juce::MemoryBlock LinkVideo::getSelfFrame() const
    {
        std::lock_guard<std::mutex> g(lock);
        return self;
    }
}
