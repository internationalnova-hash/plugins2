// Nova Studio engine: clip warping (time stretch with pitch preserved). Message thread only.
#include "StudioAudioEngine.h"
#include "TimeStretcher.h"

namespace NovaStudio
{
    namespace
    {
        juce::File warpFolder()
        {
            return StudioAudioEngine::getDefaultSessionsFolder().getChildFile("Warp");
        }
    }

    bool StudioAudioEngine::renderWarp(Clip& clip, double ratio, int mode, juce::String* error)
    {
        if (clip.isMidi) { if (error) *error = "MIDI clips follow the tempo by themselves."; return false; }
        ratio = juce::jlimit(0.25, 4.0, ratio);

        if (!clip.warped)
        {
            clip.warpSourceFile = clip.file;
            clip.warpSourceOffset = clip.fileOffsetSamples + clip.alignmentOffsetSamples;
            clip.warpSourceLength = clip.lengthSamples;
        }
        const auto& srcFile = clip.warpSourceFile;
        std::unique_ptr<juce::AudioFormatReader> reader(clipFormats.createReaderFor(srcFile));
        if (reader == nullptr) { if (error) *error = "Can't read " + srcFile.getFileName(); return false; }

        const double fileRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        const double toFile = fileRate / juce::jmax(1.0, currentSampleRate);
        const int64_t startF = juce::jmax((int64_t) 0, (int64_t) std::llround((double) clip.warpSourceOffset * toFile));
        const int64_t lenF = juce::jmin(reader->lengthInSamples - startF, (int64_t) std::llround((double) clip.warpSourceLength * toFile));
        if (lenF < 64) { if (error) *error = "The clip is too short to stretch."; return false; }
        if (lenF > (int64_t) (fileRate * 60.0 * 20.0)) { if (error) *error = "Clips longer than 20 minutes can't be warped."; return false; }

        // Rendered files are cached by source region, ratio and mode.
        const juce::String key = srcFile.getFullPathName() + "|" + juce::String(startF) + "|" + juce::String(lenF)
                               + "|" + juce::String(ratio, 5) + "|" + juce::String(mode);
        const auto outFile = warpFolder().getChildFile(juce::File::createLegalFileName(srcFile.getFileNameWithoutExtension())
                                                       + "_" + juce::String::toHexString(key.hashCode64()) + ".wav");
        if (!outFile.existsAsFile())
        {
            juce::AudioBuffer<float> src((int) juce::jlimit(1u, 2u, reader->numChannels), (int) lenF);
            reader->read(&src, 0, (int) lenF, startF, true, src.getNumChannels() > 1);
            auto out = TimeStretcher::stretch(src, fileRate, ratio, mode == 1 ? TimeStretcher::Mode::Beats : TimeStretcher::Mode::Mix);
            if (out.getNumSamples() < 2) { if (error) *error = "Stretching failed."; return false; }

            warpFolder().createDirectory();
            const auto tmp = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + ".part");
            tmp.deleteFile();
            {
                std::unique_ptr<juce::FileOutputStream> stream(tmp.createOutputStream());
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> w(stream ? wav.createWriterFor(stream.get(), fileRate, (unsigned) out.getNumChannels(), 32, {}, 0) : nullptr);
                if (w == nullptr) { if (error) *error = "Can't write the warped audio."; return false; }
                stream.release();
                w->writeFromAudioSampleBuffer(out, 0, out.getNumSamples());
            }
            tmp.moveFileTo(outFile);
        }

        clip.file = outFile;
        clip.fileOffsetSamples = 0;
        clip.alignmentOffsetSamples = 0;
        clip.lengthSamples = juce::jmax((int64_t) 1, (int64_t) std::llround((double) clip.warpSourceLength * ratio));
        clip.warped = true;
        clip.warpRatio = ratio;
        clip.warpMode = mode;
        return true;
    }

    bool StudioAudioEngine::warpClipToTempo(int t, int c, double sourceBpm, bool followTempo, int mode, juce::String* error)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return false;
        auto& clips = session.getTrack(t).clips;
        if (!juce::isPositiveAndBelow(c, clips.size())) return false;
        if (sourceBpm < 20.0 || sourceBpm > 400.0) { if (error) *error = "Enter the clip's original tempo (BPM)."; return false; }
        auto& clip = clips.getReference(c);
        if (!renderWarp(clip, sourceBpm / session.getTempo(), mode, error)) return false;
        clip.warpSourceBpm = sourceBpm;
        clip.warpFollowTempo = followTempo;
        publishSnapshot(false);
        return true;
    }

    bool StudioAudioEngine::stretchClip(int t, int c, double ratio, int mode, juce::String* error)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return false;
        auto& clips = session.getTrack(t).clips;
        if (!juce::isPositiveAndBelow(c, clips.size())) return false;
        auto& clip = clips.getReference(c);
        if (!renderWarp(clip, ratio, mode, error)) return false;
        clip.warpSourceBpm = 0.0;
        clip.warpFollowTempo = false;
        publishSnapshot(false);
        return true;
    }

    void StudioAudioEngine::unwarpClip(int t, int c)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return;
        auto& clips = session.getTrack(t).clips;
        if (!juce::isPositiveAndBelow(c, clips.size())) return;
        auto& clip = clips.getReference(c);
        if (!clip.warped) return;
        clip.file = clip.warpSourceFile;
        clip.fileOffsetSamples = clip.warpSourceOffset;
        clip.lengthSamples = clip.warpSourceLength;
        clip.warped = false;
        clip.warpRatio = 1.0;
        clip.warpSourceBpm = 0.0;
        clip.warpFollowTempo = false;
        publishSnapshot(false);
    }

    double StudioAudioEngine::suggestClipTempo(int t, int c) const
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return 0.0;
        const auto& clips = session.getTrack(t).clips;
        if (!juce::isPositiveAndBelow(c, clips.size())) return 0.0;
        const auto& clip = clips.getReference(c);
        if (clip.isMidi) return 0.0;
        if (clip.warped && clip.warpSourceBpm > 0.0) return clip.warpSourceBpm;

        const auto file = clip.warped ? clip.warpSourceFile : clip.file;
        if (const double named = TimeStretcher::tempoFromFileName(file.getFileName()); named > 0.0)
            return named;

        std::unique_ptr<juce::AudioFormatReader> reader(const_cast<juce::AudioFormatManager&>(clipFormats).createReaderFor(file));
        if (reader == nullptr) return 0.0;
        const double toFile = reader->sampleRate / juce::jmax(1.0, currentSampleRate);
        const int64_t start = (int64_t) ((clip.warped ? clip.warpSourceOffset : clip.fileOffsetSamples) * toFile);
        const int64_t len = juce::jmin((int64_t) (reader->sampleRate * 60.0),
                                       (int64_t) ((clip.warped ? clip.warpSourceLength : clip.lengthSamples) * toFile));
        if (len < reader->sampleRate * 2.0) return 0.0;
        juce::AudioBuffer<float> buf((int) juce::jlimit(1u, 2u, reader->numChannels), (int) len);
        reader->read(&buf, 0, (int) len, start, true, buf.getNumChannels() > 1);
        return TimeStretcher::detectTempo(buf, reader->sampleRate);
    }

    void StudioAudioEngine::rewarpFollowingClips(double oldTempo, double newTempo)
    {
        if (oldTempo <= 0.0 || newTempo <= 0.0 || std::abs(oldTempo - newTempo) < 1.0e-6) return;
        int count = 0;
        for (int t = 0; t < session.getNumTracks(); ++t)
            for (auto& clip : session.getTrack(t).clips)
                if (clip.warped && clip.warpFollowTempo && clip.warpSourceBpm > 0.0)
                {
                    // Musical clips keep their bar position and length in beats.
                    clip.startSample = (int64_t) std::llround((double) clip.startSample * oldTempo / newTempo);
                    clip.fadeInSamples = (int64_t) std::llround((double) clip.fadeInSamples * oldTempo / newTempo);
                    clip.fadeOutSamples = (int64_t) std::llround((double) clip.fadeOutSamples * oldTempo / newTempo);
                    renderWarp(clip, clip.warpSourceBpm / newTempo, clip.warpMode, nullptr);
                    ++count;
                }
        if (count > 0)
            juce::Logger::writeToLog("Re-warped " + juce::String(count) + " clip(s) to " + juce::String(newTempo, 2) + " BPM");
    }
}
