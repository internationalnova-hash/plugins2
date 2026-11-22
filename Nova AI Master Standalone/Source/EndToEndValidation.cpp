// Headless end-to-end validation: drives the real NovaAIMasterAudioEngine
// (the exact class the standalone app's UI calls) against a real WAV file -
// import -> analyze+master -> export - and checks the result. This is the
// plan's stated standalone verification step: "run a real end-to-end
// import -> master -> export on at least one real mix."
//
// Runs a real JUCE message loop (MessageManager::runDispatchLoopUntil) so
// AudioEngine's background-thread completion callbacks (which fire via
// MessageManager::callAsync) actually get delivered, same as they would in
// the running app.

#include <juce_events/juce_events.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "AudioEngine.h"
#include <iostream>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    bool pumpUntil (std::function<bool()> predicate, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! predicate())
        {
            if (juce::Time::getMillisecondCounter() > deadline) return false;
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
        return true;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::cout << "Usage: NovaAIMasterEndToEndValidation <path-to-test.wav>\n";
        return 1;
    }
    const juce::File inputFile (argv[1]);

    std::cout << "\n[EndToEnd] Importing " << inputFile.getFullPathName() << "\n";
    NovaAIMasterAudioEngine engine;

    bool importDone = false, importOk = false;
    engine.onImportFinished = [&] (bool ok, juce::String err)
    {
        importOk = ok;
        importDone = true;
        if (! ok) std::cout << "    import error: " << err << "\n";
    };

    engine.importFile (inputFile);
    check (pumpUntil ([&] { return importDone; }, 10000), "import completed within 10s");
    check (importOk, "import succeeded");
    check (engine.hasImportedAudio(), "engine reports imported audio present");
    check (engine.getImportedLengthSeconds() > 1.0, "imported length is plausible (>1s)");
    std::cout << "    file: " << engine.getImportedFileName()
              << " " << engine.getImportedSampleRate() << "Hz "
              << engine.getImportedBitDepth() << "-bit "
              << (engine.getImportedIsStereo() ? "stereo" : "mono")
              << " " << engine.getImportedLengthSeconds() << "s\n";

    std::cout << "\n[EndToEnd] Analyzing + mastering (Style=Clean, Intensity=0.6, Loudness=Balanced)\n";
    bool masterDone = false, masterOk = false;
    NovaAudioProfile resultProfile;
    NovaMasteringPlan resultPlan;
    engine.onMasterFinished = [&] (const NovaAudioProfile& p, const NovaMasteringPlan& plan)
    {
        resultProfile = p; resultPlan = plan; masterOk = true; masterDone = true;
    };
    engine.onMasterFailed = [&] (const juce::String& err)
    {
        std::cout << "    master error: " << err << "\n";
        masterDone = true;
    };

    NovaMasterUserPrefs prefs;
    prefs.style = NovaMasterStyle::Clean;
    prefs.intensity = 0.6f;
    prefs.loudness = NovaLoudnessMode::Balanced;
    engine.requestMaster (prefs);

    check (pumpUntil ([&] { return masterDone; }, 30000), "mastering completed within 30s");
    check (masterOk, "mastering succeeded");
    check (engine.hasMasteredAudio(), "engine reports mastered audio present");

    std::cout << "    measured: integratedLufs=" << resultProfile.integratedLufs
              << " truePeakDb=" << resultProfile.truePeakDb
              << " crestFactorDb=" << resultProfile.crestFactorDb
              << " harshness=" << resultProfile.harshness
              << " resonanceScore=" << resultProfile.resonanceScore << "\n";
    std::cout << "    plan: outputGainDb=" << resultPlan.output.gainDb
              << " compRatio=" << resultPlan.compression.ratio
              << " limiterCeiling=" << resultPlan.limiting.truePeakCeilDb << "\n";

    check (resultProfile.integratedLufs > -60.f, "measured loudness is plausible, not silence-floor");
    check (resultPlan.limiting.truePeakCeilDb < 0.f, "plan's true peak ceiling is a real ceiling, not 0/positive");

    std::cout << "\n[EndToEnd] Exporting mastered WAV\n";
    const juce::File outputFile = inputFile.getSiblingFile ("test_mix_MASTERED.wav");
    outputFile.deleteFile();

    juce::String exportError;
    const bool exportOk = engine.exportMastered (outputFile, NovaAIMasterAudioEngine::ExportFormat::Wav24, exportError);
    check (exportOk, "export reported success");
    if (! exportOk) std::cout << "    export error: " << exportError << "\n";

    check (outputFile.existsAsFile(), "exported file exists on disk");
    check (outputFile.getSize() > 1000, "exported file has real content (not empty/truncated)");

    // Re-read the exported file to confirm it's a valid, playable-length WAV.
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (outputFile));
        check (reader != nullptr, "exported file re-opens as a valid audio file");
        if (reader != nullptr)
        {
            const double exportedLength = (double) reader->lengthInSamples / reader->sampleRate;
            std::cout << "    exported: " << reader->sampleRate << "Hz "
                      << reader->bitsPerSample << "-bit " << reader->numChannels
                      << "ch " << exportedLength << "s\n";
            check (std::abs (exportedLength - engine.getImportedLengthSeconds()) < 0.1,
                  "exported length matches imported length (within 0.1s)");
            check (reader->bitsPerSample == 24, "exported at requested 24-bit depth");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL END-TO-END TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
