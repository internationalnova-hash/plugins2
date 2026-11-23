#include "LoopLabCore.h"

namespace ll
{
    static constexpr double kMaxSampleSeconds = 60.0;

    LoopLabCore::LoopLabCore() : juce::Thread ("Nova Loop Lab worker")
    {
        formats.registerBasicFormats();
        juce::Random r;
        for (auto& sd : spec.seeds) sd = (juce::uint32) r.nextInt (1 << 30) + 1;
        startThread (juce::Thread::Priority::low);
        startTimer (500);
    }

    LoopLabCore::~LoopLabCore()
    {
        stopTimer();
        signalThreadShouldExit();
        wake.signal();
        stopThread (4000);
        audioModel.store (nullptr);
    }

    juce::File LoopLabCore::libraryFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Nova Loop Lab").getChildFile ("Samples");
    }

    juce::File LoopLabCore::exportsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Nova Loop Lab").getChildFile ("Exports");
    }

    Spec LoopLabCore::getSpec() const
    {
        const juce::ScopedLock sl (specLock);
        return spec;
    }

    void LoopLabCore::updateSpec (const std::function<void (Spec&)>& edit)
    {
        {
            const juce::ScopedLock sl (specLock);
            edit (spec);
            ++specVersion;
        }
        wake.signal();
    }

    void LoopLabCore::loadFile (const juce::File& file, bool resetSettings)
    {
        loadResets.store (resetSettings);
        {
            const juce::ScopedLock sl (specLock);
            pendingFile = file;
            loadError = {};
        }
        loadPending.store (true);
        wake.signal();
    }

    juce::String LoopLabCore::getLoadError() const
    {
        const juce::ScopedLock sl (specLock);
        return loadError;
    }

    std::shared_ptr<const Model> LoopLabCore::getModel() const
    {
        const juce::ScopedLock sl (modelLock);
        return model;
    }

    void LoopLabCore::publish (std::shared_ptr<Model> m)
    {
        m->version = modelVersion.load() + 1;
        const juce::ScopedLock sl (modelLock);
        if (model != nullptr)
            retired.push_back ({ model, juce::Time::getMillisecondCounter() });
        model = m;
        audioModel.store (m.get(), std::memory_order_release);
        modelVersion.store (m->version);
    }

    void LoopLabCore::timerCallback()
    {
        const auto now = juce::Time::getMillisecondCounter();
        const juce::ScopedLock sl (modelLock);
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [&] (const Retired& r) { return now - r.at > 1000 && r.m.get() != audioModel.load(); }),
                       retired.end());
    }

    void LoopLabCore::run()
    {
        while (! threadShouldExit())
        {
            wake.wait (250);
            if (threadShouldExit()) break;

            if (loadPending.exchange (false))
            {
                busy.store (true);
                juce::File file;
                { const juce::ScopedLock sl (specLock); file = pendingFile; }

                juce::String error;
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
                if (! file.existsAsFile())
                    error = "Sample not found: " + file.getFileName();
                else if (reader == nullptr)
                    error = "Couldn't read that file. Try a WAV, AIFF, FLAC or MP3.";
                else if (reader->lengthInSamples < (juce::int64) (0.1 * reader->sampleRate))
                    error = "That sample is too short to chop.";

                if (error.isEmpty())
                {
                    auto smp = std::make_shared<Sample>();
                    smp->sampleRate = reader->sampleRate;
                    const int n = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (kMaxSampleSeconds * reader->sampleRate));
                    smp->audio.setSize (2, n);
                    reader->read (&smp->audio, 0, n, 0, true, reader->numChannels > 1);
                    if (reader->numChannels == 1) smp->audio.copyFrom (1, 0, smp->audio, 0, 0, n);

                    auto an = std::make_shared<Analysis> (analyse (*smp));

                    // Keep a copy in the library so the project still opens if
                    // the original file moves.
                    auto keep = file;
                    const auto lib = libraryFolder();
                    if (! file.isAChildOf (lib) && lib.createDirectory())
                    {
                        const auto hash = juce::String::toHexString (file.getFullPathName().hashCode64() ^ file.getSize()).substring (0, 8);
                        keep = lib.getChildFile (file.getFileNameWithoutExtension() + "_" + hash + file.getFileExtension());
                        if (! keep.existsAsFile() && ! file.copyFileTo (keep))
                            keep = file;
                    }

                    const juce::ScopedLock sl (specLock);
                    spec.sample = smp;
                    spec.analysis = an;
                    spec.samplePath = keep.getFullPathName();
                    if (loadResets.load() || spec.sampleName.isEmpty())
                        spec.sampleName = file.getFileName();
                    if (loadResets.load())
                    {
                        spec.tempoMult = 1.0;
                        spec.fx.assign ((size_t) kMaxChops, ChopFx {});
                        spec.locked.fill (false);
                        juce::Random r;
                        for (auto& sd : spec.seeds) sd = (juce::uint32) r.nextInt (1 << 30) + 1;
                    }
                    ++specVersion;
                }
                else
                {
                    const juce::ScopedLock sl (specLock);
                    loadError = error;
                }
            }

            Spec s;
            int v;
            {
                const juce::ScopedLock sl (specLock);
                s = spec;
                v = specVersion;
            }
            if (v == builtVersion)
            {
                busy.store (false);
                continue;
            }

            busy.store (true);
            if (auto m = build (s, getModel()))
                publish (m);
            builtVersion = v;

            { const juce::ScopedLock sl (specLock); if (specVersion != builtVersion) wake.signal(); }
            busy.store (false);
        }
    }

    std::shared_ptr<Model> LoopLabCore::build (const Spec& s, const std::shared_ptr<const Model>& prev)
    {
        auto m = std::make_shared<Model>();
        m->sampleRate = s.sampleRate > 0 ? s.sampleRate : 48000.0;
        m->bpm = juce::jlimit (40.0, 250.0, s.hostBpm > 20.0 ? s.hostBpm : 120.0);
        m->bars = s.gen.bars;
        if (s.sample == nullptr || s.analysis == nullptr)
            return m;

        m->sample = s.sample;
        m->sampleName = s.sampleName;
        m->analysis = *s.analysis;
        m->analysis.bpm *= s.tempoMult;
        m->analysis.beats = s.sample->seconds() * m->analysis.bpm / 60.0;
        m->analysis.bars = std::max (1, (int) std::lround (m->analysis.beats / 4.0));
        m->tempoMultUsed = s.tempoMult;
        m->chopUsed = s.chop;
        if (s.hostBpm <= 20.0)
            m->bpm = juce::jlimit (40.0, 250.0, m->analysis.bpm);

        const bool sameSample = prev != nullptr && prev->sample == s.sample && std::abs (prev->tempoMultUsed - s.tempoMult) < 1.0e-9;
        const bool sameChops = sameSample && prev->chopUsed.mode == s.chop.mode
                               && std::abs (prev->chopUsed.sensitivity - s.chop.sensitivity) < 1.0e-4f
                               && prev->chopUsed.gridDiv == s.chop.gridDiv;
        if (sameChops)
        {
            m->chops = prev->chops;
            m->effectiveGridDiv = prev->effectiveGridDiv;
        }
        else
        {
            auto r = makeChops (*s.sample, m->analysis, s.chop);
            m->chops = std::move (r.chops);
            m->effectiveGridDiv = r.effectiveGridDiv;
        }

        const int numChops = (int) m->chops.size();
        m->fxUsed = s.fx;
        m->fxUsed.resize ((size_t) std::max (numChops, kMaxChops));

        for (int vIdx = 0; vIdx < kNumVariations; ++vIdx)
        {
            const auto& lockedSteps = s.lockedSteps[(size_t) vIdx];
            const bool lockedOk = s.locked[(size_t) vIdx] && ! lockedSteps.empty()
                                  && std::all_of (lockedSteps.begin(), lockedSteps.end(),
                                                  [&] (const Step& st) { return st.chop < numChops && st.t + st.len <= s.gen.bars * 16; });
            if (lockedOk)
                m->steps[(size_t) vIdx] = lockedSteps;
            else
            {
                auto g = s.gen;
                g.seed = s.seeds[(size_t) vIdx];
                m->steps[(size_t) vIdx] = generate (m->chops, m->fxUsed, *s.sample, m->analysis, g);
            }
        }

        m->tempoRatioUsed = m->analysis.bpm / m->bpm;
        const bool canReuseRenders = sameChops && std::abs (prev->tempoRatioUsed - m->tempoRatioUsed) < 1.0e-9
                                     && std::abs (prev->sampleRate - m->sampleRate) < 1.0e-6;
        m->renderedChops.resize ((size_t) numChops);
        for (int i = 0; i < numChops; ++i)
        {
            if (threadShouldExit()) return nullptr;
            const auto& fx = m->fxUsed[(size_t) i];
            if (canReuseRenders && i < (int) prev->renderedChops.size() && i < (int) prev->fxUsed.size() && prev->fxUsed[(size_t) i] == fx)
                m->renderedChops[(size_t) i] = prev->renderedChops[(size_t) i];
            else
                m->renderedChops[(size_t) i] = renderChop (*s.sample, m->chops[(size_t) i], fx, m->sampleRate, m->tempoRatioUsed);
        }

        LoopSpec ls;
        ls.bars = s.gen.bars;
        ls.bpm = m->bpm;
        ls.sampleRate = m->sampleRate;
        ls.swing = s.swing;
        for (int vIdx = 0; vIdx < kNumVariations; ++vIdx)
        {
            if (threadShouldExit()) return nullptr;
            m->loops[(size_t) vIdx] = renderLoop (m->renderedChops, m->steps[(size_t) vIdx], ls);
        }
        return m;
    }
}
