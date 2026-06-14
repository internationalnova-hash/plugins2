#include "PluginBrowserPanel.h"

using namespace NovaStudioUI;

static juce::Colour kPanelBg    = juce::Colour::fromRGB(16, 18, 26);
static juce::Colour kBorder     = juce::Colour::fromRGBA(255,255,255,18);
static juce::Colour kAccent     = juce::Colour::fromRGB(80, 110, 255);
static juce::Colour kBtnBg      = juce::Colour::fromRGB(32, 36, 52);
static juce::Colour kRowSel     = juce::Colour::fromRGBA(80, 110, 255, 60);
static juce::Colour kRowAlt     = juce::Colour::fromRGBA(255,255,255, 6);
static juce::Colour kTextDim    = juce::Colour::fromRGBA(255,255,255,120);

// ─────────────────────────────────────────────────────────────────────────────

PluginBrowserPanel::PluginBrowserPanel(NovaStudio::StudioAudioEngine& engineRef)
    : engine(engineRef)
{
    formatManager.addDefaultFormats();

    // Search box
    searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colours::grey);
    searchBox.setColour(juce::TextEditor::backgroundColourId, kBtnBg);
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId, kBorder);
    searchBox.addListener(this);
    addAndMakeVisible(searchBox);

    // Plugin list
    pluginList.setModel(this);
    pluginList.setColour(juce::ListBox::backgroundColourId, kPanelBg);
    pluginList.setColour(juce::ListBox::outlineColourId, kBorder);
    pluginList.setRowHeight(22);
    addAndMakeVisible(pluginList);

    // Buttons
    for (auto* btn : { &scanBtn, &loadBtn, &browseBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, kBtnBg);
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.85f));
        btn->addListener(this);
        addAndMakeVisible(btn);
    }
    loadBtn.setColour(juce::TextButton::buttonColourId, kAccent.withAlpha(0.7f));
    loadBtn.setEnabled(false);

    // Status / target labels
    statusLabel.setColour(juce::Label::textColourId, kTextDim);
    statusLabel.setFont(juce::FontOptions(11.0f));
    statusLabel.setText("Click 'Scan Plugins' to discover installed VST3/LV2 plugins.", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    targetLabel.setColour(juce::Label::textColourId, kAccent.brighter(0.3f));
    targetLabel.setFont(juce::FontOptions(11.0f));
    setTargetTrackAndSlot(0, 0);
    addAndMakeVisible(targetLabel);

    // Progress bar (hidden until scan)
    addAndMakeVisible(progressBar);
    progressBar.setVisible(false);
}

PluginBrowserPanel::~PluginBrowserPanel()
{
    stopTimer();
}

void PluginBrowserPanel::setTargetTrackAndSlot(int trackIndex, int slotIndex)
{
    targetTrack = trackIndex;
    targetSlot  = slotIndex;
    targetLabel.setText("Target: Track " + juce::String(trackIndex + 1) +
                        "  |  Insert Slot " + juce::String(slotIndex + 1),
                        juce::dontSendNotification);
    pluginList.selectRow(-1, true);  // clear prior selection so re-targeting a new slot starts fresh
    loadBtn.setEnabled(false);
}

void PluginBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(kPanelBg);

    // Header
    auto header = getLocalBounds().removeFromTop(36);
    g.setColour(kPanelBg.brighter(0.06f));
    g.fillRect(header);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("Plugin Browser", header.reduced(12, 0), juce::Justification::centredLeft);

    g.setColour(kBorder);
    g.drawRect(getLocalBounds(), 1);
    g.drawHorizontalLine(36, 0.0f, (float)getWidth());
}

void PluginBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(30); // header

    // Target label
    targetLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    // Button row
    auto btnRow = area.removeFromTop(28);
    int btnW = btnRow.getWidth() / 3;
    scanBtn  .setBounds(btnRow.removeFromLeft(btnW).reduced(2, 0));
    browseBtn.setBounds(btnRow.removeFromLeft(btnW).reduced(2, 0));
    loadBtn  .setBounds(btnRow.reduced(2, 0));
    area.removeFromTop(4);

    // Progress bar (same row height, shown/hidden)
    progressBar.setBounds(area.removeFromTop(14));
    area.removeFromTop(4);

    // Search box
    searchBox.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);

    // Status label
    statusLabel.setBounds(area.removeFromBottom(16));
    area.removeFromBottom(2);

    // List fills remaining
    pluginList.setBounds(area);
}

// ─────────────────────────────────────────────────────────────────────────────
// ListBoxModel
// ─────────────────────────────────────────────────────────────────────────────

int PluginBrowserPanel::getNumRows()
{
    return filteredPlugins.size();
}

void PluginBrowserPanel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (!isPositiveAndBelow(row, filteredPlugins.size())) return;

    if (selected)
        g.fillAll(kRowSel);
    else if (row % 2 == 0)
        g.fillAll(kRowAlt);

    auto& desc = filteredPlugins.getReference(row);

    // Plugin name (bold-ish)
    g.setColour(juce::Colours::white.withAlpha(selected ? 1.0f : 0.85f));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(desc.name, 8, 0, w - 120, h, juce::Justification::centredLeft, true);

    // Format badge
    juce::String fmt = desc.pluginFormatName;
    g.setColour(kAccent.withAlpha(0.7f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(fmt, w - 50, 0, 44, h, juce::Justification::centredRight);

    // Manufacturer (dim)
    g.setColour(kTextDim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(desc.manufacturerName, 8, 0, w - 60, h, juce::Justification::centredRight);
}

void PluginBrowserPanel::listBoxItemDoubleClicked(int /*row*/, const juce::MouseEvent&)
{
    loadSelectedPlugin();
}

void PluginBrowserPanel::selectedRowsChanged(int lastRowSelected)
{
    loadBtn.setEnabled(isPositiveAndBelow(lastRowSelected, filteredPlugins.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Scan
// ─────────────────────────────────────────────────────────────────────────────

void PluginBrowserPanel::startScan()
{
    if (scanning.load()) return;
    scanning = true;
    scanBtn.setEnabled(false);
    progressBar.setVisible(true);
    scanProgress = -1.0;

    allPlugins.clear();
    filteredPlugins.clear();
    pluginList.updateContent();
    statusLabel.setText("Scanning...", juce::dontSendNotification);
    repaint();

    // Set up one PluginDirectoryScanner per format (runs on message thread — safe for VST3/X11)
    scanFmtLists.clear();
    scanners.clear();
    scanResults.clear();
    currentScannerIdx = 0;

    for (int fi = 0; fi < formatManager.getNumFormats(); ++fi)
    {
        auto* fmt = formatManager.getFormat(fi);
        juce::FileSearchPath fmtPaths;
        fmtPaths.addPath(fmt->getDefaultLocationsToSearch());

#if JUCE_LINUX
        if (fmt->getName().containsIgnoreCase("VST3"))
        {
            fmtPaths.add(juce::File("/usr/lib/vst3"));
            fmtPaths.add(juce::File("/usr/local/lib/vst3"));
            fmtPaths.add(juce::File(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName() + "/.vst3"));
        }
        if (fmt->getName().containsIgnoreCase("LV2"))
        {
            fmtPaths.add(juce::File("/usr/lib/lv2"));
            fmtPaths.add(juce::File("/usr/local/lib/lv2"));
            fmtPaths.add(juce::File(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName() + "/.lv2"));
        }
#elif JUCE_MAC
        fmtPaths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
        fmtPaths.add(juce::File(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName() + "/Library/Audio/Plug-Ins/VST3"));
        if (fmt->getName().containsIgnoreCase("AU") || fmt->getName().containsIgnoreCase("AudioUnit"))
            fmtPaths.add(juce::File("/Library/Audio/Plug-Ins/Components"));
#elif JUCE_WINDOWS
        fmtPaths.add(juce::File("C:/Program Files/Common Files/VST3"));
        fmtPaths.add(juce::File("C:/Program Files (x86)/Common Files/VST3"));
#endif

        auto* fmtList = scanFmtLists.add(new juce::KnownPluginList());
        scanners.add(new juce::PluginDirectoryScanner(*fmtList, *fmt, fmtPaths, true, juce::File(), false));
    }

    startTimerHz(30);
}

void PluginBrowserPanel::timerCallback()
{
    if (currentScannerIdx >= scanners.size())
    {
        stopTimer();
        finishScan(std::move(scanResults));
        return;
    }

    // Scan up to 3 files per tick to balance speed vs responsiveness
    for (int i = 0; i < 3; ++i)
    {
        juce::String next;
        bool more = scanners[currentScannerIdx]->scanNextFile(true, next);
        // Update progress label
        if (!next.isEmpty())
            statusLabel.setText("Scanning: " + juce::File(next).getFileName(), juce::dontSendNotification);
        if (!more)
        {
            // Format done — collect results
            for (auto& d : scanFmtLists[currentScannerIdx]->getTypes())
                scanResults.add(d);
            ++currentScannerIdx;
            break;
        }
    }
}

void PluginBrowserPanel::finishScan(juce::Array<juce::PluginDescription> results)
{
    scanning = false;
    scanBtn.setEnabled(true);
    progressBar.setVisible(false);

    knownPlugins.clear();
    allPlugins = std::move(results);
    for (auto& d : allPlugins)
        knownPlugins.addType(d);

    rebuildFilteredList();
    statusLabel.setText("Found " + juce::String(allPlugins.size()) + " plugin(s).",
                        juce::dontSendNotification);
}

void PluginBrowserPanel::rebuildFilteredList()
{
    juce::String query = searchBox.getText().trim().toLowerCase();
    filteredPlugins.clear();

    for (auto& desc : allPlugins)
    {
        if (query.isEmpty()
            || desc.name.toLowerCase().contains(query)
            || desc.manufacturerName.toLowerCase().contains(query)
            || desc.pluginFormatName.toLowerCase().contains(query))
        {
            filteredPlugins.add(desc);
        }
    }

    pluginList.updateContent();
    loadBtn.setEnabled(pluginList.getSelectedRow() >= 0 && !filteredPlugins.isEmpty());
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// Load
// ─────────────────────────────────────────────────────────────────────────────

void PluginBrowserPanel::loadSelectedPlugin()
{
    int row = pluginList.getSelectedRow();
    if (!isPositiveAndBelow(row, filteredPlugins.size()))
    {
        statusLabel.setText("Select a plugin first.", juce::dontSendNotification);
        return;
    }

    auto& desc = filteredPlugins.getReference(row);
    juce::String errorMsg;
    auto instance = formatManager.createPluginInstance(desc, 44100.0, 512, errorMsg);

    if (!instance)
    {
        statusLabel.setText("Failed to load: " + errorMsg, juce::dontSendNotification);
        return;
    }

    juce::String name = instance->getName();
    instance.reset(); // release the test instance; engine will create its own

    // Try file-based load first (most reliable for VST3 bundles)
    juce::File plugFile(desc.fileOrIdentifier);
    bool ok = plugFile.exists() && engine.loadPluginOnTrack(targetTrack, plugFile);

    // Fallback: load directly via PluginDescription
    if (!ok)
        ok = engine.loadPluginByDescription(desc, targetTrack);

    if (ok)
    {
        statusLabel.setText("Loaded: " + name + "  →  Track " + juce::String(targetTrack + 1)
                            + "  Slot " + juce::String(targetSlot + 1),
                            juce::dontSendNotification);
        if (onPluginLoaded) onPluginLoaded(targetTrack, targetSlot, name);
    }
    else
    {
        statusLabel.setText("Load failed for: " + name, juce::dontSendNotification);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Button / TextEditor callbacks
// ─────────────────────────────────────────────────────────────────────────────

void PluginBrowserPanel::buttonClicked(juce::Button* b)
{
    if (b == &scanBtn)
    {
        startScan();
    }
    else if (b == &loadBtn)
    {
        loadSelectedPlugin();
    }
    else if (b == &browseBtn)
    {
        // File picker for a specific .vst3/.so file
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select a plugin file",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.vst3;*.so;*.dll;*.component;*.lv2");

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::canSelectMultipleItems,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (results.isEmpty()) return;

                juce::StringArray paths;
                for (auto& f : results)
                    paths.add(f.getFullPathName());

                juce::OwnedArray<juce::PluginDescription> found;
                knownPlugins.scanAndAddDragAndDroppedFiles(formatManager, paths, found);

                for (auto* d : found)
                {
                    allPlugins.add(*d);
                    knownPlugins.addType(*d);
                }

                rebuildFilteredList();
                statusLabel.setText("Added " + juce::String(found.size()) + " plugin(s) from file.",
                                    juce::dontSendNotification);
            });
    }
}

void PluginBrowserPanel::textEditorTextChanged(juce::TextEditor&)
{
    rebuildFilteredList();
    loadBtn.setEnabled(pluginList.getSelectedRow() >= 0 && !filteredPlugins.isEmpty());
}
