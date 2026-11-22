#include <JuceHeader.h>
#include "MainWindow.h"

class NovaBassLabApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new NovaBassLabMainWindow (getApplicationName()));
    }

    void shutdown() override { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<NovaBassLabMainWindow> mainWindow;
};

START_JUCE_APPLICATION (NovaBassLabApplication)
