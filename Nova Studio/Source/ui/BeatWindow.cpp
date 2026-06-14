#include "BeatWindow.h"

using namespace NovaStudioUI;

// ─────────────────────────────────────────────────────────────────────────────
// BeatBrowser
// ─────────────────────────────────────────────────────────────────────────────

BeatBrowser::BeatBrowser()
{
    searchField.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
    searchField.setColour(juce::TextEditor::backgroundColourId, BeatTheme::panel());
    searchField.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchField.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGBA(255,255,255,20));
    addAndMakeVisible(searchField);

    for (auto* btn : { &kitsBtn, &loopsBtn, &oneshotBtn, &pluginsBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, BeatTheme::stepOff());
        btn->setColour(juce::TextButton::buttonOnColourId, BeatTheme::accent());
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn->addListener(this);
        addAndMakeVisible(btn);
    }
    kitsBtn.setToggleState(true, juce::dontSendNotification);

    contentList.setModel(&contentModel);
    contentList.setColour(juce::ListBox::backgroundColourId, BeatTheme::panel());
    contentList.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGBA(255,255,255,12));
    addAndMakeVisible(contentList);

    favHeader.setText("Favorites", juce::dontSendNotification);
    favHeader.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    favHeader.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(favHeader);

    favList.setModel(&favModel);
    favList.setColour(juce::ListBox::backgroundColourId, BeatTheme::panel());
    addAndMakeVisible(favList);

    rebuildList();
}

BeatBrowser::~BeatBrowser() = default;

void BeatBrowser::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::panel());
    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void BeatBrowser::resized()
{
    auto area = getLocalBounds().reduced(6);
    searchField.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);

    auto catRow = area.removeFromTop(24);
    int btnW = catRow.getWidth() / 4;
    kitsBtn   .setBounds(catRow.removeFromLeft(btnW));
    loopsBtn  .setBounds(catRow.removeFromLeft(btnW));
    oneshotBtn.setBounds(catRow.removeFromLeft(btnW));
    pluginsBtn.setBounds(catRow);
    area.removeFromTop(4);

    auto favArea = area.removeFromBottom(90);
    favHeader.setBounds(favArea.removeFromTop(18));
    favList.setBounds(favArea);

    contentList.setBounds(area);
}

void BeatBrowser::buttonClicked(juce::Button* b)
{
    int newCat = 0;
    if (b == &loopsBtn)   newCat = 1;
    if (b == &oneshotBtn) newCat = 2;
    if (b == &pluginsBtn) newCat = 3;
    selectedCategory = newCat;

    kitsBtn   .setToggleState(newCat == 0, juce::dontSendNotification);
    loopsBtn  .setToggleState(newCat == 1, juce::dontSendNotification);
    oneshotBtn.setToggleState(newCat == 2, juce::dontSendNotification);
    pluginsBtn.setToggleState(newCat == 3, juce::dontSendNotification);

    rebuildList();
}

void BeatBrowser::rebuildList()
{
    contentModel.items.clear();
    switch (selectedCategory)
    {
        case 0: contentModel.items = { "808 Classic", "Acoustic Kit", "Trap Kit", "House Kit", "Jazz Kit" }; break;
        case 1: contentModel.items = { "120BPM Loop A", "Lo-Fi Drums", "Funk Break" }; break;
        case 2: contentModel.items = { "Clap 1", "Snare Dry", "Kick Sub", "Hi-Hat Tight" }; break;
        case 3: contentModel.items = { "DrumSynth", "BeatBox Pro" }; break;
        default: break;
    }
    favModel.items = { "808 Classic", "Kick Sub" };
    contentList.updateContent();
    favList.updateContent();
}

void BeatBrowser::ContentModel::paintListBoxItem(int row, juce::Graphics& g,
                                                  int w, int h, bool selected)
{
    if (selected)
        g.fillAll(BeatTheme::accent().withAlpha(0.3f));
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(items[row], 8, 0, w - 8, h, juce::Justification::centredLeft);
}

// ─────────────────────────────────────────────────────────────────────────────
// PatternPlaylist
// ─────────────────────────────────────────────────────────────────────────────

PatternPlaylist::PatternPlaylist()
{
    blocks.add({ 0, 0, 4, juce::Colour::fromRGB(80,100,255), "Pattern A" });
    blocks.add({ 0, 5, 2, juce::Colour::fromRGB(90,60,200),  "Pattern B" });
    blocks.add({ 1, 0, 8, juce::Colour::fromRGB(60,160,255), "Loop 1"    });
    blocks.add({ 2, 2, 4, juce::Colour::fromRGB(255,100,60), "Perc"      });
}

void PatternPlaylist::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::bg());

    const int headerH = 20;
    const int laneH   = 28;
    const int numLanes = 4;
    int barW = juce::jmax(1, (getWidth() - 50) / numBars);

    g.setColour(BeatTheme::panel());
    g.fillRect(0, 0, getWidth(), headerH);

    g.setFont(juce::FontOptions(10.0f));
    for (int b = 0; b < numBars; ++b)
    {
        int x = 50 + b * barW;
        g.setColour((b % 4 == 0) ? BeatTheme::gridBeat() : BeatTheme::grid());
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
        if (b % 4 == 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawText(juce::String(b + 1), x + 2, 2, 24, 16, juce::Justification::left);
        }
    }

    const char* laneNames[] = { "Drums", "Bass", "Lead", "FX" };
    for (int ln = 0; ln < numLanes; ++ln)
    {
        int y = headerH + ln * laneH;
        g.setColour(ln == selectedLane ? BeatTheme::panel().brighter(0.05f) : BeatTheme::panel());
        g.fillRect(0, y, 50, laneH);
        g.setColour(BeatTheme::edge());
        g.drawRect(0, y, 50, laneH, 1);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(laneNames[ln], 4, y, 44, laneH, juce::Justification::centredLeft);

        g.setColour(BeatTheme::stepOff().withAlpha(0.4f));
        g.fillRect(50, y, getWidth() - 50, laneH);
    }

    for (auto& blk : blocks)
    {
        int x = 50 + blk.startBar * barW;
        int y = headerH + blk.lane * laneH;
        int w = blk.lengthBars * barW - 2;
        auto r = juce::Rectangle<int>(x, y + 2, w, laneH - 4);
        g.setColour(blk.colour.withAlpha(0.7f));
        g.fillRoundedRectangle(r.toFloat(), 3.0f);
        g.setColour(blk.colour.brighter(0.3f));
        g.drawRoundedRectangle(r.toFloat(), 3.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(blk.name, r.reduced(4, 0), juce::Justification::centredLeft);
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void PatternPlaylist::resized() {}

void PatternPlaylist::mouseDown(const juce::MouseEvent& e)
{
    const int headerH = 20;
    const int laneH   = 28;
    int lane = (e.y - headerH) / laneH;
    if (lane >= 0 && lane < 4)
        selectedLane = lane;
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// PianoRollView
// ─────────────────────────────────────────────────────────────────────────────

PianoRollView::PianoRollView()
{
    notes.add({ 60, 0, 4, 0.8f });
    notes.add({ 62, 6, 2, 0.6f });
    notes.add({ 64, 10, 3, 0.9f });
    notes.add({ 67, 2, 6, 0.7f });
}

void PianoRollView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto snapArea = bounds.removeFromBottom(kSnapH);
    auto velArea  = bounds.removeFromBottom(kVelHeight);
    auto mainArea = bounds;

    auto keysArea = mainArea.removeFromLeft(kKeyWidth);
    auto gridArea = mainArea;

    g.fillAll(BeatTheme::bg());
    drawPianoKeys(g, keysArea);
    drawGrid(g, gridArea);
    drawNotes(g, gridArea);
    drawVelocityLane(g, velArea);
    drawSnapControls(g, snapArea);
}

void PianoRollView::resized() {}

void PianoRollView::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(kSnapH);
    bounds.removeFromBottom(kVelHeight);
    auto mainArea = bounds;
    mainArea.removeFromLeft(kKeyWidth);
    auto gridArea = mainArea;

    if (!gridArea.contains(e.getPosition()))
        return;

    const int totalPitches = kNumOctaves * 12;
    const int startPitch   = 36;
    const int numSteps     = 32;
    int stepW = juce::jmax(1, gridArea.getWidth() / numSteps);

    int relX = e.x - gridArea.getX();
    int relY = e.y - gridArea.getY() - kHeaderH;
    if (relY < 0) return;

    int pitchRow = relY / kRowHeight;
    int step     = relX / stepW;

    if (pitchRow < 0 || pitchRow >= totalPitches) return;
    if (step < 0 || step >= numSteps) return;

    int pitch = startPitch + (totalPitches - 1 - pitchRow);

    for (int i = notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes.getReference(i);
        if (n.pitch == pitch && step >= n.startStep && step < n.startStep + n.lengthSteps)
        {
            notes.remove(i);
            repaint();
            return;
        }
    }

    notes.add({ pitch, step, 1, 0.8f });
    repaint();
}

void PianoRollView::drawPianoKeys(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int totalPitches = kNumOctaves * 12;
    const int startPitch   = 36;

    g.setColour(BeatTheme::panel());
    g.fillRect(area);

    for (int i = 0; i < totalPitches; ++i)
    {
        int pitch = startPitch + (totalPitches - 1 - i);
        int y = area.getY() + kHeaderH + i * kRowHeight;
        int note = pitch % 12;
        bool isBlack = (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);

        g.setColour(isBlack ? juce::Colour::fromRGB(30,30,40) : juce::Colour::fromRGB(200,200,210));
        g.fillRect(area.getX(), y, isBlack ? (int)(area.getWidth() * 0.6f) : area.getWidth(), kRowHeight - 1);

        if (note == 0)
        {
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.setFont(juce::FontOptions(8.0f));
            g.drawText("C" + juce::String(pitch / 12 - 1),
                       area.getX() + 2, y, area.getWidth() - 4, kRowHeight,
                       juce::Justification::centredLeft);
        }
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(area, 1);

    g.setColour(BeatTheme::panel().brighter(0.1f));
    g.fillRect(area.getX(), area.getY(), area.getWidth(), kHeaderH);
    g.setColour(BeatTheme::accent());
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("KEYS", area.getX(), area.getY(), area.getWidth(), kHeaderH, juce::Justification::centred);
}

void PianoRollView::drawGrid(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int totalPitches = kNumOctaves * 12;
    const int startPitch   = 36;
    const int numSteps     = 32;
    int stepW = juce::jmax(1, area.getWidth() / numSteps);

    g.setColour(BeatTheme::stepOff());
    g.fillRect(area);

    g.setColour(BeatTheme::panel().brighter(0.1f));
    g.fillRect(area.getX(), area.getY(), area.getWidth(), kHeaderH);

    for (int s = 0; s < numSteps; ++s)
    {
        int x = area.getX() + s * stepW;
        if (s % 4 == 0)
        {
            g.setColour(BeatTheme::gridBeat());
            g.drawVerticalLine(x, (float)area.getY(), (float)(area.getY() + kHeaderH));
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(juce::String(s / 4 + 1), x + 2, area.getY(), 20, kHeaderH, juce::Justification::left);
        }
    }

    for (int i = 0; i < totalPitches; ++i)
    {
        int pitch = startPitch + (totalPitches - 1 - i);
        int y = area.getY() + kHeaderH + i * kRowHeight;
        int note = pitch % 12;
        bool isBlack = (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);

        g.setColour(isBlack ? BeatTheme::stepOff().darker(0.2f) : BeatTheme::stepOff());
        g.fillRect(area.getX(), y, area.getWidth(), kRowHeight);

        g.setColour(note == 0 ? BeatTheme::gridBeat() : BeatTheme::grid());
        g.drawHorizontalLine(y, (float)area.getX(), (float)area.getRight());
    }

    for (int s = 0; s < numSteps; ++s)
    {
        int x = area.getX() + s * stepW;
        g.setColour(s % 4 == 0 ? BeatTheme::gridBeat() : BeatTheme::grid());
        g.drawVerticalLine(x, (float)(area.getY() + kHeaderH), (float)area.getBottom());
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(area, 1);
}

void PianoRollView::drawNotes(juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int totalPitches = kNumOctaves * 12;
    const int startPitch   = 36;
    const int numSteps     = 32;
    int stepW = juce::jmax(1, area.getWidth() / numSteps);

    for (auto& note : notes)
    {
        int pitchIdx = (startPitch + totalPitches - 1 - note.pitch);
        if (pitchIdx < 0 || pitchIdx >= totalPitches) continue;

        int x = area.getX() + note.startStep * stepW;
        int y = area.getY() + kHeaderH + pitchIdx * kRowHeight;
        int w = note.lengthSteps * stepW - 2;
        int h = kRowHeight - 1;

        auto nr = juce::Rectangle<int>(x, y, w, h);
        g.setColour(BeatTheme::noteBlock().withAlpha(0.85f));
        g.fillRoundedRectangle(nr.toFloat(), 2.0f);
        g.setColour(BeatTheme::noteBlock().brighter(0.4f));
        g.drawRoundedRectangle(nr.toFloat(), 2.0f, 1.0f);

        g.setColour(juce::Colour::fromRGBA(200, 220, 255, (uint8_t)(note.velocity * 180)));
        g.fillRect(x, y, 3, h);
    }
}

void PianoRollView::drawVelocityLane(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(BeatTheme::panel());
    g.fillRect(area);
    g.setColour(BeatTheme::accent().withAlpha(0.4f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("VELOCITY", area.getX() + 4, area.getY(), 60, area.getHeight(), juce::Justification::centredLeft);

    const int numSteps = 32;
    int stepW = juce::jmax(1, (area.getWidth() - kKeyWidth) / numSteps);
    float barMaxH = (float)(area.getHeight() - 6);

    for (auto& note : notes)
    {
        int x = area.getX() + kKeyWidth + note.startStep * stepW + 1;
        int barH = (int)(note.velocity * barMaxH);
        int y = area.getBottom() - 3 - barH;
        g.setColour(BeatTheme::accent().withAlpha(0.7f));
        g.fillRect(x, y, stepW - 2, barH);
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(area, 1);
}

void PianoRollView::drawSnapControls(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(BeatTheme::panel().brighter(0.05f));
    g.fillRect(area);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Snap: 1/16", area.getX() + 8, area.getY(), 80, area.getHeight(), juce::Justification::centredLeft);

    const char* labels[] = { "1/4", "1/8", "1/16", "1/32", "Free" };
    int btnX = area.getX() + 92;
    for (auto* lbl : labels)
    {
        bool active = juce::String(lbl) == "1/16";
        g.setColour(active ? BeatTheme::accent() : BeatTheme::stepOff());
        g.fillRoundedRectangle((float)btnX, (float)(area.getY() + 3), 34.0f, (float)(area.getHeight() - 6), 3.0f);
        g.setColour(juce::Colours::white.withAlpha(active ? 1.0f : 0.6f));
        g.drawText(lbl, btnX, area.getY(), 34, area.getHeight(), juce::Justification::centred);
        btnX += 38;
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(area, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// StepSequencerView
// ─────────────────────────────────────────────────────────────────────────────

StepSequencerView::StepSequencerView(NovaStudio::TransportState& transport)
    : transportState(transport)
{
    patterns.add(Pattern());
    startTimerHz(30);
}

StepSequencerView::~StepSequencerView()
{
    stopTimer();
}

// ── Layout helpers ────────────────────────────────────────────────────────────

juce::Rectangle<int> StepSequencerView::toolbarArea() const
{
    return { 0, 0, getWidth(), kToolbarH };
}

juce::Rectangle<int> StepSequencerView::allRowsArea() const
{
    return { 0, kToolbarH, getWidth(), kNumRows * kRowH };
}

juce::Rectangle<int> StepSequencerView::channelStripArea(int row) const
{
    return { 0, kToolbarH + row * kRowH, kChannelW, kRowH };
}

juce::Rectangle<int> StepSequencerView::stepGridArea() const
{
    return { kChannelW, kToolbarH, getWidth() - kChannelW, kNumRows * kRowH };
}

juce::Rectangle<int> StepSequencerView::graphArea() const
{
    if (!showGraphEditor) return {};
    return { 0, kToolbarH + kNumRows * kRowH, getWidth(), kGraphH };
}

float StepSequencerView::stepCellWidth() const
{
    const int sc = currentPattern().stepCount;
    const int gridW = getWidth() - kChannelW;
    // Each group of 4 gets an extra 2px gap after it
    const int numGroups = sc / 4;
    const int totalGapPx = numGroups * 2;
    return (float)(gridW - totalGapPx) / (float)sc;
}

bool StepSequencerView::hitTestGrid(juce::Point<int> p, int& outRow, int& outStep) const
{
    auto grid = stepGridArea();
    if (!grid.contains(p)) return false;

    int relY = p.y - grid.getY();
    outRow = relY / kRowH;
    if (outRow < 0 || outRow >= kNumRows) return false;

    int relX = p.x - grid.getX();
    const int sc = currentPattern().stepCount;
    const float cellW = stepCellWidth();
    const int numGroups = sc / 4;

    // Step groups: each group of 4 is followed by 2px gap
    // We need to find which step the x falls in
    float x = 0.0f;
    for (int s = 0; s < sc; ++s)
    {
        float cellEnd = x + cellW;
        if ((float)relX >= x && (float)relX < cellEnd)
        {
            outStep = s;
            return true;
        }
        x = cellEnd;
        if ((s + 1) % 4 == 0 && s < sc - 1)
            x += 2.0f; // group gap
    }
    return false;
}

int StepSequencerView::hitTestGraph(juce::Point<int> p) const
{
    auto ga = graphArea();
    if (!showGraphEditor || !ga.contains(p)) return -1;

    int relX = p.x - ga.getX() - kChannelW;
    if (relX < 0) return -1;

    const int sc = currentPattern().stepCount;
    const float cellW = stepCellWidth();
    const int numGroups = sc / 4;

    float x = 0.0f;
    for (int s = 0; s < sc; ++s)
    {
        float cellEnd = x + cellW;
        if ((float)relX >= x && (float)relX < cellEnd)
            return s;
        x = cellEnd;
        if ((s + 1) % 4 == 0 && s < sc - 1)
            x += 2.0f;
    }
    return -1;
}

int StepSequencerView::hitTestChannelStrip(juce::Point<int> localInStrip) const
{
    // 0=colorchip(skip), 1=mute LED, 2=pan, 3=vol, 4=name
    const int x = localInStrip.x;
    // color chip: 0-7
    if (x < 8) return 0;
    // mute LED: 8-27
    if (x < 28) return 1;
    // pan: 28-47
    if (x < 48) return 2;
    // vol: 48-67
    if (x < 68) return 3;
    // name button: rest
    return 4;
}

float StepSequencerView::graphYToVelocity(int y) const
{
    auto ga = graphArea();
    const int innerH = ga.getHeight() - 8;
    int relY = ga.getBottom() - 4 - y;
    float vel = (float)relY / (float)innerH;
    return juce::jlimit(0.0f, 1.0f, vel);
}

int StepSequencerView::rowAtY(int y) const
{
    int gridY = y - kToolbarH;
    if (gridY < 0) return -1;
    int row = gridY / kRowH;
    return (row >= 0 && row < kNumRows) ? row : -1;
}

bool StepSequencerView::isAudioFile(const juce::String& path)
{
    auto ext = juce::File(path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".mp3"
        || ext == ".ogg" || ext == ".flac" || ext == ".m4a";
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void StepSequencerView::timerCallback()
{
    if (transportState.isPlaying())
    {
        double posSamples = (double)transportState.getPositionSamples();
        double sr         = transportState.getSampleRate();
        double bpm        = transportState.getTempo();
        if (sr > 0.0 && bpm > 0.0)
        {
            double stepsPerSec = (bpm / 60.0) * 4.0;
            int sc = currentPattern().stepCount;
            int newCursor = ((int)(posSamples / sr * stepsPerSec)) % sc;
            if (newCursor != cursorStep) { cursorStep = newCursor; repaint(); }
        }
    }
    else if (cursorStep != -1)
    {
        cursorStep = -1;
        repaint();
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void StepSequencerView::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::bg());

    paintToolbar(g, toolbarArea());

    for (int row = 0; row < kNumRows; ++row)
        paintChannelStrip(g, channelStripArea(row), row);

    paintStepGrid(g, stepGridArea());

    if (showGraphEditor)
        paintGraphEditor(g, graphArea());

    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void StepSequencerView::resized() {}

// ── Toolbar ───────────────────────────────────────────────────────────────────

void StepSequencerView::paintToolbar(juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour(BeatTheme::panel());
    g.fillRect(r);
    g.setColour(BeatTheme::edge());
    g.drawRect(r, 1);

    auto& pat = currentPattern();
    int x = r.getX() + 4;
    const int y = r.getY();
    const int h = r.getHeight();

    auto drawBtn = [&](const juce::String& label, int w, bool active = false) -> juce::Rectangle<int>
    {
        auto br = juce::Rectangle<int>(x, y + 4, w, h - 8);
        g.setColour(active ? BeatTheme::accent() : BeatTheme::stepOff().brighter(0.15f));
        g.fillRoundedRectangle(br.toFloat(), 3.0f);
        g.setColour(active ? juce::Colours::white : juce::Colours::white.withAlpha(0.75f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(label, br, juce::Justification::centred);
        x += w + 3;
        return br;
    };

    // Add pattern button
    drawBtn("+", 22);

    // Pattern prev/next + name
    drawBtn("\xe2\x97\x80", 18); // ◀
    auto nameR = juce::Rectangle<int>(x, y + 4, 110, h - 8);
    g.setColour(BeatTheme::stepOff());
    g.fillRoundedRectangle(nameR.toFloat(), 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(pat.name.isEmpty() ? "Pattern " + juce::String(currentPatternIdx + 1) : pat.name,
               nameR, juce::Justification::centred, true);
    x += 113;
    drawBtn("\xe2\x96\xb6", 18); // ▶

    x += 8; // gap

    // Step count buttons
    for (int sc : { 16, 32, 64 })
        drawBtn(juce::String(sc), 28, pat.stepCount == sc);

    x += 8; // gap

    // Swing label + scrub value
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("SWING", x, y, 36, h, juce::Justification::centred);
    x += 38;
    auto swingR = juce::Rectangle<int>(x, y + 4, 44, h - 8);
    g.setColour(BeatTheme::stepOff());
    g.fillRoundedRectangle(swingR.toFloat(), 3.0f);
    g.setColour(BeatTheme::accent().brighter(0.2f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(juce::String((int)(pat.swing * 100)) + "%", swingR, juce::Justification::centred);
    x += 47;

    x += 8; // gap

    // Graph editor toggle
    drawBtn("VEL", 28, showGraphEditor);
}

// ── Channel strip ─────────────────────────────────────────────────────────────

void StepSequencerView::paintChannelStrip(juce::Graphics& g, juce::Rectangle<int> r, int row)
{
    const auto& ch = currentPattern().channels[(size_t)row];
    const bool isSelected = (row == selectedRow);

    // Background
    g.setColour(isSelected ? BeatTheme::panel().brighter(0.08f) : BeatTheme::panel());
    g.fillRect(r);

    // Colour chip (8px left edge)
    g.setColour(ch.colour);
    g.fillRect(r.getX(), r.getY(), 8, r.getHeight());

    int cx = r.getX() + 8 + 2;

    // Mute LED (circle, 18px)
    auto ledR = juce::Rectangle<int>(cx, r.getCentreY() - 9, 18, 18);
    g.setColour(ch.muted ? juce::Colour::fromRGB(80,80,100) : BeatTheme::accent());
    g.fillEllipse(ledR.toFloat());
    g.setColour(juce::Colours::white.withAlpha(ch.muted ? 0.3f : 0.9f));
    g.setFont(juce::FontOptions(7.0f));
    g.drawText("M", ledR, juce::Justification::centred);
    cx += 20;

    // Pan knob (18px circle)
    auto panR = juce::Rectangle<int>(cx, r.getCentreY() - 9, 18, 18);
    g.setColour(BeatTheme::stepOff().brighter(0.2f));
    g.fillEllipse(panR.toFloat());
    g.setColour(BeatTheme::edge());
    g.drawEllipse(panR.toFloat(), 1.0f);
    {
        // Pan indicator line
        float angle = juce::MathConstants<float>::pi * 0.75f
                    + ch.pan * juce::MathConstants<float>::pi * 0.75f;
        float cx2 = panR.getCentreX(), cy2 = panR.getCentreY();
        float len = 6.0f;
        g.setColour(BeatTheme::accent());
        g.drawLine(cx2, cy2,
                   cx2 + std::sin(angle) * len,
                   cy2 - std::cos(angle) * len, 1.5f);
    }
    // "P" label below
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(juce::FontOptions(7.0f));
    g.drawText("P", panR.getX(), panR.getBottom(), 18, 8, juce::Justification::centred);
    cx += 22;

    // Volume knob (18px circle)
    auto volR = juce::Rectangle<int>(cx, r.getCentreY() - 9, 18, 18);
    g.setColour(BeatTheme::stepOff().brighter(0.2f));
    g.fillEllipse(volR.toFloat());
    g.setColour(BeatTheme::edge());
    g.drawEllipse(volR.toFloat(), 1.0f);
    {
        // Vol indicator line: 0=bottom-left, 1=bottom-right arc
        float angle = juce::MathConstants<float>::pi * 0.75f
                    + ch.volume * juce::MathConstants<float>::pi * 1.5f;
        float cx2 = volR.getCentreX(), cy2 = volR.getCentreY();
        float len = 6.0f;
        g.setColour(ch.colour.withAlpha(0.9f));
        g.drawLine(cx2, cy2,
                   cx2 + std::sin(angle) * len,
                   cy2 - std::cos(angle) * len, 1.5f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(juce::FontOptions(7.0f));
    g.drawText("V", volR.getX(), volR.getBottom(), 18, 8, juce::Justification::centred);
    cx += 22;

    // Name button (remainder)
    auto nameR = juce::Rectangle<int>(cx, r.getY() + 3, r.getRight() - cx - 3, r.getHeight() - 6);
    g.setColour(ch.colour.withAlpha(isSelected ? 0.45f : 0.25f));
    g.fillRoundedRectangle(nameR.toFloat(), 3.0f);
    if (isSelected)
    {
        g.setColour(ch.colour.withAlpha(0.7f));
        g.drawRoundedRectangle(nameR.toFloat(), 3.0f, 1.0f);
    }
    g.setColour(juce::Colours::white.withAlpha(ch.muted ? 0.4f : 0.9f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(ch.name, nameR.reduced(4, 0), juce::Justification::centredLeft, true);

    // Row separator
    g.setColour(BeatTheme::edge());
    g.drawHorizontalLine(r.getBottom() - 1, (float)r.getX(), (float)r.getRight());
}

// ── Step grid ─────────────────────────────────────────────────────────────────

void StepSequencerView::paintStepGrid(juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour(BeatTheme::bg());
    g.fillRect(r);

    const auto& pat = currentPattern();
    const int sc     = pat.stepCount;
    const float cellW = stepCellWidth();

    for (int row = 0; row < kNumRows; ++row)
    {
        const auto& ch = pat.channels[(size_t)row];
        const int ry   = r.getY() + row * kRowH;

        float x = (float)r.getX();
        for (int s = 0; s < sc; ++s)
        {
            bool active   = ch.steps[s];
            bool isCursor = (s == cursorStep);

            auto cellR = juce::Rectangle<float>(x + 1, (float)(ry + 3), cellW - 2, (float)(kRowH - 6));

            // Cursor highlight sweeps all rows
            if (isCursor)
            {
                g.setColour(BeatTheme::stepCursor());
                g.fillRoundedRectangle(cellR.expanded(0, 1), 3.0f);
            }

            if (active)
            {
                g.setColour(ch.colour);
                g.fillRoundedRectangle(cellR, 3.0f);
                g.setColour(ch.colour.brighter(0.4f).withAlpha(0.6f));
                g.drawRoundedRectangle(cellR.reduced(0.5f), 3.0f, 1.0f);
            }
            else
            {
                g.setColour(BeatTheme::stepOff());
                g.fillRoundedRectangle(cellR, 3.0f);
            }

            x += cellW;
            // Group gap every 4 steps
            if ((s + 1) % 4 == 0 && s < sc - 1)
                x += 2.0f;
        }

        // Row separator
        g.setColour(BeatTheme::edge());
        g.drawHorizontalLine(ry + kRowH - 1, (float)r.getX(), (float)r.getRight());
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(r, 1);
}

// ── Graph editor ──────────────────────────────────────────────────────────────

void StepSequencerView::paintGraphEditor(juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour(BeatTheme::bg().darker(0.2f));
    g.fillRect(r);

    const auto& pat = currentPattern();
    const auto& ch  = pat.channels[(size_t)selectedRow];
    const int sc    = pat.stepCount;
    const float cellW = stepCellWidth();
    const int innerH = r.getHeight() - 8;

    // "VELOCITY" label
    g.setColour(ch.colour.withAlpha(0.6f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("VELOCITY", r.getX() + 4, r.getY(), kChannelW - 8, r.getHeight(), juce::Justification::centredLeft);

    // Velocity bars
    float x = (float)(r.getX() + kChannelW);
    for (int s = 0; s < sc; ++s)
    {
        float vel = ch.velocities[s];
        float barH = vel * (float)innerH;
        float barY = (float)(r.getBottom() - 4) - barH;

        // Background slot
        g.setColour(BeatTheme::stepOff());
        g.fillRect(x + 1, (float)(r.getY() + 4), cellW - 2, (float)innerH);

        // Bar
        if (ch.steps[s])
        {
            g.setColour(ch.colour.withAlpha(0.85f));
            g.fillRect(x + 1, barY, cellW - 2, barH);
        }
        else
        {
            g.setColour(ch.colour.withAlpha(0.35f));
            g.fillRect(x + 1, barY, cellW - 2, barH);
        }

        x += cellW;
        if ((s + 1) % 4 == 0 && s < sc - 1)
            x += 2.0f;
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(r, 1);
}

// ── Interaction ───────────────────────────────────────────────────────────────

void StepSequencerView::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // ── Toolbar ──────────────────────────────────────────────────────────────
    if (toolbarArea().contains(pos))
    {
        auto& pat = currentPattern();
        auto r = toolbarArea();
        int x = r.getX() + 4;
        const int h = r.getHeight();

        // Helper: check if click is within next button region and advance x
        auto inBtn = [&](int w) -> bool {
            bool hit = (pos.x >= x && pos.x < x + w && pos.y >= r.getY() + 4 && pos.y < r.getBottom() - 4);
            x += w + 3;
            return hit;
        };

        // "+" add pattern
        if (inBtn(22))
        {
            if (patterns.size() < 16)
            {
                patterns.add(Pattern());
                currentPatternIdx = patterns.size() - 1;
                repaint();
            }
            return;
        }
        // Prev pattern ◀
        if (inBtn(18))
        {
            if (currentPatternIdx > 0) --currentPatternIdx;
            repaint(); return;
        }
        // Pattern name label (110px)
        if (pos.x >= x && pos.x < x + 110)
        {
            auto nameR = juce::Rectangle<int>(x, r.getY() + 4, 110, r.getHeight() - 8);
            juce::String current = pat.name.isEmpty() ? "Pattern " + juce::String(currentPatternIdx + 1) : pat.name;
            showInlineRename(nameR, current, true);
            return;
        }
        x += 113;
        // Next pattern ▶
        if (inBtn(18))
        {
            if (currentPatternIdx < patterns.size() - 1) ++currentPatternIdx;
            repaint(); return;
        }

        x += 8;
        // Step count 16/32/64
        for (int sc : { 16, 32, 64 })
        {
            if (inBtn(28))
            {
                pat.stepCount = sc;
                if (onStepCountChanged) onStepCountChanged(sc);
                repaint(); return;
            }
        }

        x += 8;
        // Swing label (36px) + swing scrub (44px)
        x += 38; // skip "SWING" label
        if (pos.x >= x && pos.x < x + 44)
        {
            isSwingDragging  = true;
            swingDragStartX  = e.x;
            swingDragStartVal = pat.swing;
            return;
        }
        x += 47;

        x += 8;
        // VEL toggle
        if (inBtn(28))
        {
            showGraphEditor = !showGraphEditor;
            repaint(); return;
        }
        return;
    }

    // ── Channel strip ─────────────────────────────────────────────────────────
    for (int row = 0; row < kNumRows; ++row)
    {
        auto sr = channelStripArea(row);
        if (sr.contains(pos))
        {
            selectedRow = row;
            auto localP = pos - sr.getTopLeft();
            int part = hitTestChannelStrip(localP);

            if (e.mods.isRightButtonDown())
            {
                handleChannelRightClickMenu(row);
                return;
            }

            if (part == 1) // mute LED
            {
                auto& ch = currentPattern().channels[(size_t)row];
                ch.muted = !ch.muted;
                if (onRowMutedChanged) onRowMutedChanged(row, ch.muted);
                repaint();
                return;
            }
            if (part == 2) // pan knob
            {
                knobDragType    = KnobDragType::Pan;
                knobDragRow     = row;
                knobDragStartY  = e.y;
                knobDragStartVal = currentPattern().channels[(size_t)row].pan;
                return;
            }
            if (part == 3) // vol knob
            {
                knobDragType    = KnobDragType::Volume;
                knobDragRow     = row;
                knobDragStartY  = e.y;
                knobDragStartVal = currentPattern().channels[(size_t)row].volume;
                return;
            }
            if (part == 4) // name button
            {
                if (e.getNumberOfClicks() >= 2)
                {
                    renameChannel(row);
                }
                else
                {
                    // Single click: open file browser to load sample
                    auto chooser = std::make_shared<juce::FileChooser>(
                        "Load Sample for " + currentPattern().channels[(size_t)row].name,
                        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                        "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
                    chooser->launchAsync(
                        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this, row, chooser](const juce::FileChooser& fc)
                        {
                            auto result = fc.getResult();
                            if (result.existsAsFile())
                            {
                                currentPattern().channels[(size_t)row].name = result.getFileNameWithoutExtension();
                                if (onSampleAssigned) onSampleAssigned(row, result.getFullPathName());
                                repaint();
                            }
                        });
                }
                repaint();
                return;
            }
            repaint();
            return;
        }
    }

    // ── Graph editor ──────────────────────────────────────────────────────────
    if (showGraphEditor && graphArea().contains(pos))
    {
        int step = hitTestGraph(pos);
        if (step >= 0 && e.mods.isLeftButtonDown())
        {
            isDraggingGraph = true;
            float vel = graphYToVelocity(pos.y);
            currentPattern().channels[(size_t)selectedRow].velocities[step] = vel;
            if (onVelocityChanged) onVelocityChanged(selectedRow, step, vel);
            repaint();
        }
        return;
    }

    // ── Step grid ─────────────────────────────────────────────────────────────
    {
        int row = -1, step = -1;
        if (hitTestGrid(pos, row, step))
        {
            selectedRow = row;

            if (e.mods.isRightButtonDown())
            {
                handleStepRightClickMenu(row, step);
                return;
            }

            // Left click: toggle and start drag
            auto& active = currentPattern().channels[(size_t)row].steps[step];
            dragActivating = !active;
            active = dragActivating;
            isDragging     = true;
            dragStartRow   = row;
            dragStartStep  = step;
            if (onStepChanged) onStepChanged(row, step, active);
            repaint();
        }
    }
}

void StepSequencerView::mouseDrag(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // Swing scrub drag
    if (isSwingDragging)
    {
        float delta = (float)(e.x - swingDragStartX) / 200.0f;
        float newSwing = juce::jlimit(0.0f, 1.0f, swingDragStartVal + delta);
        currentPattern().swing = newSwing;
        if (onSwingChanged) onSwingChanged(newSwing);
        repaint();
        return;
    }

    // Knob drag
    if (knobDragType != KnobDragType::None && knobDragRow >= 0)
    {
        float delta = (float)(knobDragStartY - e.y) / 80.0f;
        auto& ch = currentPattern().channels[(size_t)knobDragRow];
        if (knobDragType == KnobDragType::Pan)
        {
            ch.pan = juce::jlimit(-1.0f, 1.0f, knobDragStartVal + delta);
            if (onRowPanChanged) onRowPanChanged(knobDragRow, ch.pan);
        }
        else
        {
            ch.volume = juce::jlimit(0.0f, 1.0f, knobDragStartVal + delta);
            if (onRowVolumeChanged) onRowVolumeChanged(knobDragRow, ch.volume);
        }
        repaint();
        return;
    }

    // Graph drag
    if (isDraggingGraph && showGraphEditor)
    {
        int step = hitTestGraph(pos);
        if (step >= 0)
        {
            float vel = graphYToVelocity(pos.y);
            currentPattern().channels[(size_t)selectedRow].velocities[step] = vel;
            if (onVelocityChanged) onVelocityChanged(selectedRow, step, vel);
            repaint();
        }
        return;
    }

    // Step grid drag — activate/deactivate range
    if (isDragging)
    {
        int row = -1, step = -1;
        if (hitTestGrid(pos, row, step))
        {
            auto& active = currentPattern().channels[(size_t)row].steps[step];
            if (active != dragActivating)
            {
                active = dragActivating;
                if (onStepChanged) onStepChanged(row, step, active);
                repaint();
            }
        }
    }
}

void StepSequencerView::mouseUp(const juce::MouseEvent&)
{
    isDragging      = false;
    isDraggingGraph = false;
    isSwingDragging = false;
    knobDragType    = KnobDragType::None;
    knobDragRow     = -1;
}

// ── Context menus ─────────────────────────────────────────────────────────────

void StepSequencerView::handleStepRightClickMenu(int row, int /*step*/)
{
    auto& pat = currentPattern();
    auto& ch  = pat.channels[(size_t)row];
    const int sc = pat.stepCount;

    juce::PopupMenu m;
    m.addItem(1, "Fill every 1");
    m.addItem(2, "Fill every 2");
    m.addItem(3, "Fill every 4");
    m.addSeparator();
    m.addItem(4, "Clear row");
    m.addItem(5, "Reverse");
    m.addItem(6, "Shift left");
    m.addItem(7, "Shift right");
    m.addItem(8, "Randomize");

    m.showMenuAsync(juce::PopupMenu::Options(), [this, row, sc](int result)
    {
        auto& ch2 = currentPattern().channels[(size_t)row];
        switch (result)
        {
            case 1: // Fill every 1
                for (int s = 0; s < sc; ++s) { ch2.steps[s] = true; if (onStepChanged) onStepChanged(row, s, true); }
                break;
            case 2: // Fill every 2
                for (int s = 0; s < sc; ++s) { ch2.steps[s] = (s % 2 == 0); if (onStepChanged) onStepChanged(row, s, ch2.steps[s]); }
                break;
            case 3: // Fill every 4
                for (int s = 0; s < sc; ++s) { ch2.steps[s] = (s % 4 == 0); if (onStepChanged) onStepChanged(row, s, ch2.steps[s]); }
                break;
            case 4: // Clear
                for (int s = 0; s < sc; ++s) { ch2.steps[s] = false; if (onStepChanged) onStepChanged(row, s, false); }
                break;
            case 5: // Reverse
            {
                bool tmp[kMaxSteps];
                std::copy(ch2.steps, ch2.steps + sc, tmp);
                for (int s = 0; s < sc; ++s) { ch2.steps[s] = tmp[sc - 1 - s]; if (onStepChanged) onStepChanged(row, s, ch2.steps[s]); }
                break;
            }
            case 6: // Shift left
            {
                bool first = ch2.steps[0];
                for (int s = 0; s < sc - 1; ++s) ch2.steps[s] = ch2.steps[s + 1];
                ch2.steps[sc - 1] = first;
                for (int s = 0; s < sc; ++s) if (onStepChanged) onStepChanged(row, s, ch2.steps[s]);
                break;
            }
            case 7: // Shift right
            {
                bool last = ch2.steps[sc - 1];
                for (int s = sc - 1; s > 0; --s) ch2.steps[s] = ch2.steps[s - 1];
                ch2.steps[0] = last;
                for (int s = 0; s < sc; ++s) if (onStepChanged) onStepChanged(row, s, ch2.steps[s]);
                break;
            }
            case 8: // Randomize
                for (int s = 0; s < sc; ++s) { ch2.steps[s] = (juce::Random::getSystemRandom().nextFloat() > 0.6f); if (onStepChanged) onStepChanged(row, s, ch2.steps[s]); }
                break;
            default: break;
        }
        repaint();
    });
}

void StepSequencerView::handleChannelRightClickMenu(int row)
{
    juce::PopupMenu fillSub;
    fillSub.addItem(20, "Fill every step");
    fillSub.addItem(21, "Fill every 2 steps");
    fillSub.addItem(22, "Fill every 4 steps");
    fillSub.addItem(23, "Fill every 8 steps");

    juce::PopupMenu m;
    m.addItem(1,  "Load sample...");
    m.addSeparator();
    m.addItem(2,  "Rename");
    m.addItem(3,  "Change color");
    m.addItem(4,  "Random color");
    m.addSeparator();
    m.addItem(5,  "Cut");
    m.addItem(6,  "Copy");
    m.addItem(7,  "Paste");
    m.addSeparator();
    m.addSubMenu("Fill", fillSub);
    m.addItem(30, "Rotate left");
    m.addItem(31, "Rotate right");
    m.addSeparator();
    m.addItem(40, "Clear steps");
    m.addItem(41, "Clone to next row");
    m.addItem(42, "Delete row");

    // Capture steps for copy/paste
    static bool stepClipboard[kMaxSteps] {};
    static int  stepClipboardCount = 0;

    m.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result)
    {
        auto& pat = currentPattern();
        auto& ch  = pat.channels[(size_t)row];
        const int sc = pat.stepCount;

        static bool stepClipboard[kMaxSteps] {};
        static int  stepClipboardCount = 0;

        switch (result)
        {
            case 1: // Load sample
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Load Sample for " + ch.name,
                    juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                    "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
                chooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, row, chooser](const juce::FileChooser& fc)
                    {
                        auto result2 = fc.getResult();
                        if (result2.existsAsFile())
                        {
                            currentPattern().channels[(size_t)row].name = result2.getFileNameWithoutExtension();
                            if (onSampleAssigned) onSampleAssigned(row, result2.getFullPathName());
                            repaint();
                        }
                    });
                break;
            }
            case 2: renameChannel(row); break;
            case 3:
            {
                static const juce::Colour presets[] = {
                    juce::Colour::fromRGB(255,100,60),  juce::Colour::fromRGB(60,180,255),
                    juce::Colour::fromRGB(220,200,60),  juce::Colour::fromRGB(160,80,255),
                    juce::Colour::fromRGB(80,220,120),  juce::Colour::fromRGB(255,160,40),
                    juce::Colour::fromRGB(200,80,200),  juce::Colour::fromRGB(60,200,200)
                };
                int idx = 0;
                for (int i = 0; i < 8; ++i)
                    if (ch.colour == presets[i]) { idx = (i + 1) % 8; break; }
                ch.colour = presets[idx];
                repaint();
                break;
            }
            case 4: // Random color
                ch.colour = juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.7f, 0.9f, 1.0f);
                repaint();
                break;
            case 5: // Cut
                std::copy(ch.steps, ch.steps + sc, stepClipboard);
                stepClipboardCount = sc;
                for (int s = 0; s < sc; ++s) { ch.steps[s] = false; if (onStepChanged) onStepChanged(row, s, false); }
                repaint();
                break;
            case 6: // Copy
                std::copy(ch.steps, ch.steps + sc, stepClipboard);
                stepClipboardCount = sc;
                break;
            case 7: // Paste
                if (stepClipboardCount > 0)
                {
                    int n = juce::jmin(sc, stepClipboardCount);
                    for (int s = 0; s < n; ++s) { ch.steps[s] = stepClipboard[s]; if (onStepChanged) onStepChanged(row, s, ch.steps[s]); }
                    repaint();
                }
                break;
            case 20: // Fill every step
                for (int s = 0; s < sc; ++s) { ch.steps[s] = true;      if (onStepChanged) onStepChanged(row, s, true); }
                repaint(); break;
            case 21: // Fill every 2
                for (int s = 0; s < sc; ++s) { ch.steps[s] = (s % 2 == 0); if (onStepChanged) onStepChanged(row, s, ch.steps[s]); }
                repaint(); break;
            case 22: // Fill every 4
                for (int s = 0; s < sc; ++s) { ch.steps[s] = (s % 4 == 0); if (onStepChanged) onStepChanged(row, s, ch.steps[s]); }
                repaint(); break;
            case 23: // Fill every 8
                for (int s = 0; s < sc; ++s) { ch.steps[s] = (s % 8 == 0); if (onStepChanged) onStepChanged(row, s, ch.steps[s]); }
                repaint(); break;
            case 30: // Rotate left
            {
                bool tmp = ch.steps[0];
                for (int s = 0; s < sc - 1; ++s) ch.steps[s] = ch.steps[s + 1];
                ch.steps[sc - 1] = tmp;
                for (int s = 0; s < sc; ++s) if (onStepChanged) onStepChanged(row, s, ch.steps[s]);
                repaint(); break;
            }
            case 31: // Rotate right
            {
                bool tmp = ch.steps[sc - 1];
                for (int s = sc - 1; s > 0; --s) ch.steps[s] = ch.steps[s - 1];
                ch.steps[0] = tmp;
                for (int s = 0; s < sc; ++s) if (onStepChanged) onStepChanged(row, s, ch.steps[s]);
                repaint(); break;
            }
            case 40: // Clear steps
                for (int s = 0; s < sc; ++s) { ch.steps[s] = false; if (onStepChanged) onStepChanged(row, s, false); }
                repaint(); break;
            case 41: // Clone to next row
                if (row + 1 < kNumRows)
                {
                    auto& dst = pat.channels[(size_t)(row + 1)];
                    std::copy(ch.steps, ch.steps + sc, dst.steps);
                    std::copy(ch.velocities, ch.velocities + sc, dst.velocities);
                    for (int s = 0; s < sc; ++s) if (onStepChanged) onStepChanged(row + 1, s, dst.steps[s]);
                    repaint();
                }
                break;
            case 42: // Delete (clear + rename to empty)
                for (int s = 0; s < sc; ++s) { ch.steps[s] = false; if (onStepChanged) onStepChanged(row, s, false); }
                ch.name = "Empty";
                repaint(); break;
            default: break;
        }
    });
}

void StepSequencerView::renameChannel(int row)
{
    // Show inline text editor over the channel name button
    auto sr = channelStripArea(row);
    // Compute name button area (same logic as paintChannelStrip)
    int cx = sr.getX() + 8 + 2 + 20 + 22 + 22; // color chip + mute + pan + vol
    auto nameR = juce::Rectangle<int>(cx, sr.getY() + 3, sr.getRight() - cx - 3, sr.getHeight() - 6);
    showInlineRename(nameR, currentPattern().channels[(size_t)row].name, false);
    inlineRenameRow = row;
}

void StepSequencerView::showInlineRename(juce::Rectangle<int> bounds, const juce::String& initial, bool isPattern)
{
    // Dismiss any existing editor first
    if (inlineEditor)
    {
        finishInlineRename(false);
    }

    inlineIsPattern = isPattern;
    inlineRenameRow = isPattern ? -1 : inlineRenameRow; // row already set by caller for channel

    inlineEditor = std::make_unique<juce::TextEditor>();
    inlineEditor->setText(initial, false);
    inlineEditor->selectAll();
    inlineEditor->setColour(juce::TextEditor::backgroundColourId, BeatTheme::bg());
    inlineEditor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    inlineEditor->setColour(juce::TextEditor::outlineColourId, BeatTheme::accent());
    inlineEditor->setFont(juce::FontOptions(10.0f));
    inlineEditor->setBounds(bounds);

    // Commit on return, dismiss on escape
    inlineEditor->onReturnKey  = [this] { finishInlineRename(true);  };
    inlineEditor->onEscapeKey  = [this] { finishInlineRename(false); };
    inlineEditor->onFocusLost  = [this] { finishInlineRename(true);  };

    addAndMakeVisible(*inlineEditor);
    inlineEditor->grabKeyboardFocus();
}

void StepSequencerView::finishInlineRename(bool accepted)
{
    if (!inlineEditor) return;

    if (accepted)
    {
        juce::String result = inlineEditor->getText().trim();
        if (result.isNotEmpty())
        {
            if (inlineIsPattern)
            {
                currentPattern().name = result;
            }
            else if (inlineRenameRow >= 0)
            {
                currentPattern().channels[(size_t)inlineRenameRow].name = result;
            }
        }
    }

    inlineEditor.reset();
    inlineRenameRow = -1;
    repaint();
}

// ── Drag-and-drop (sample files from BeatBrowser) ────────────────────────────

bool StepSequencerView::isInterestedInDragSource(const SourceDetails& details)
{
    return isAudioFile(details.description.toString());
}

void StepSequencerView::itemDragEnter(const SourceDetails& details)
{
    dragHighlightRow = rowAtY(details.localPosition.y);
    repaint();
}

void StepSequencerView::itemDragMove(const SourceDetails& details)
{
    int newRow = rowAtY(details.localPosition.y);
    if (newRow != dragHighlightRow) { dragHighlightRow = newRow; repaint(); }
}

void StepSequencerView::itemDragExit(const SourceDetails&)
{
    dragHighlightRow = -1;
    repaint();
}

void StepSequencerView::itemDropped(const SourceDetails& details)
{
    int row = rowAtY(details.localPosition.y);
    if (row >= 0)
    {
        juce::String path = details.description.toString();
        currentPattern().channels[(size_t)row].name = juce::File(path).getFileNameWithoutExtension();
        if (onSampleAssigned) onSampleAssigned(row, path);
    }
    dragHighlightRow = -1;
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumRackPanel
// ─────────────────────────────────────────────────────────────────────────────

DrumRackPanel::DrumRackPanel() {}

DrumRackPanel::~DrumRackPanel()
{
    stopTimer();
}

void DrumRackPanel::timerCallback()
{
    pressedPad = -1;
    stopTimer();
    repaint();
}

void DrumRackPanel::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::panel());

    auto area = getLocalBounds().reduced(4);

    auto header = area.removeFromTop(22);
    g.setColour(BeatTheme::panel().brighter(0.1f));
    g.fillRect(header);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("Drum Rack", header, juce::Justification::centred);

    auto padArea = area.removeFromTop(kPadRows * 52);
    int padW = padArea.getWidth() / kPadCols;
    int padH = 52;

    for (int row = 0; row < kPadRows; ++row)
    {
        for (int col = 0; col < kPadCols; ++col)
        {
            int idx = row * kPadCols + col;
            auto r = juce::Rectangle<int>(padArea.getX() + col * padW,
                                           padArea.getY() + row * padH,
                                           padW - 2, padH - 2);
            drawPad(g, r, kPadNames[idx], padSampleFiles[idx],
                    idx == selectedPad, idx == pressedPad, idx == dragHighlightPad, kPadAccents[row]);
        }
    }

    area.removeFromTop(6);
    auto soundArea = area.removeFromTop(100);
    drawSoundControls(g, soundArea);
    area.removeFromTop(4);
    drawPluginChain(g, area);

    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void DrumRackPanel::resized() {}

void DrumRackPanel::mouseDown(const juce::MouseEvent& e)
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(22);

    int padW = area.getWidth() / kPadCols;
    int padH = 52;
    int col = (e.x - area.getX()) / padW;
    int row = (e.y - area.getY()) / padH;
    if (row >= 0 && row < kPadRows && col >= 0 && col < kPadCols)
    {
        selectedPad = row * kPadCols + col;
        pressedPad  = selectedPad;
        repaint();
        startTimer(150);
    }
}

void DrumRackPanel::drawPad(juce::Graphics& g, juce::Rectangle<int> r,
                              const juce::String& name, const juce::String& sampleFile,
                              bool selected, bool pressed, bool dragOver,
                              juce::Colour accent) const
{
    juce::Colour bg = pressed  ? BeatTheme::padHit()
                    : dragOver ? BeatTheme::accent().withAlpha(0.4f)
                    : selected ? accent.withAlpha(0.55f)
                               : BeatTheme::padBase();
    g.setColour(bg);
    g.fillRoundedRectangle(r.toFloat(), 5.0f);

    float borderAlpha = dragOver ? 1.0f : (pressed ? 1.0f : (selected ? 0.9f : 0.25f));
    g.setColour(dragOver ? BeatTheme::accent() : accent.withAlpha(borderAlpha));
    g.drawRoundedRectangle(r.toFloat().reduced(1.0f), 5.0f, (selected || pressed || dragOver) ? 1.5f : 1.0f);

    g.setColour(juce::Colours::white.withAlpha((selected || pressed) ? 1.0f : 0.65f));
    if (sampleFile.isNotEmpty())
    {
        auto topR = r.reduced(3, 0).removeFromTop(r.getHeight() / 2);
        auto botR = r.reduced(3, 0).removeFromBottom(r.getHeight() / 2);
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(name, topR, juce::Justification::centredBottom, true);
        g.setColour(BeatTheme::accent().brighter(0.3f));
        g.setFont(juce::FontOptions(8.0f));
        g.drawText(juce::File(sampleFile).getFileNameWithoutExtension(),
                   botR, juce::Justification::centredTop, true);
    }
    else
    {
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(name, r.reduced(3, 0), juce::Justification::centred, true);
    }

    g.setColour(accent.withAlpha((selected || pressed) ? 0.9f : 0.4f));
    g.fillEllipse((float)(r.getCentreX() - 3), (float)(r.getBottom() - 8), 6.0f, 6.0f);
}

void DrumRackPanel::drawSoundControls(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(BeatTheme::bg());
    g.fillRoundedRectangle(area.toFloat().reduced(2.0f), 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Sound: " + juce::String(kPadNames[selectedPad]),
               area.getX() + 6, area.getY() + 4, area.getWidth() - 12, 16,
               juce::Justification::left);

    const char* params[] = { "Tune", "Decay", "Pan", "Vol" };
    int kW = (area.getWidth() - 12) / 4;
    for (int i = 0; i < 4; ++i)
    {
        int x = area.getX() + 6 + i * kW;
        int y = area.getY() + 24;
        g.setColour(BeatTheme::stepOff().brighter(0.3f));
        g.fillEllipse((float)(x + kW/2 - 16), (float)y, 32.0f, 32.0f);
        g.setColour(BeatTheme::accent());
        g.drawEllipse((float)(x + kW/2 - 16), (float)y, 32.0f, 32.0f, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(params[i], x, y + 34, kW, 14, juce::Justification::centred);
    }

    g.setColour(BeatTheme::edge());
    g.drawRoundedRectangle(area.toFloat().reduced(2.0f), 4.0f, 1.0f);
}

void DrumRackPanel::drawPluginChain(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(BeatTheme::bg());
    g.fillRoundedRectangle(area.toFloat().reduced(2.0f), 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("FX Chain", area.getX() + 6, area.getY() + 2, 60, 14, juce::Justification::left);

    for (int i = 0; i < 3; ++i)
    {
        auto slot = juce::Rectangle<int>(area.getX() + 4,
                                          area.getY() + 18 + i * 22,
                                          area.getWidth() - 8, 18);
        g.setColour(BeatTheme::stepOff());
        g.fillRoundedRectangle(slot.toFloat(), 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawText("Empty Slot", slot.reduced(4, 0), juce::Justification::centredLeft);
    }

    g.setColour(BeatTheme::edge());
    g.drawRoundedRectangle(area.toFloat().reduced(2.0f), 4.0f, 1.0f);
}

int DrumRackPanel::padAtPoint(juce::Point<int> pos) const
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(22);

    int padW = area.getWidth() / kPadCols;
    int padH = 52;

    int col = (pos.x - area.getX()) / padW;
    int row = (pos.y - area.getY()) / padH;
    if (row >= 0 && row < kPadRows && col >= 0 && col < kPadCols)
        return row * kPadCols + col;
    return -1;
}

bool DrumRackPanel::isAudioFile(const juce::String& path)
{
    auto ext = juce::File(path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".mp3"
        || ext == ".ogg" || ext == ".flac" || ext == ".m4a";
}

bool DrumRackPanel::isInterestedInDragSource(const SourceDetails& details)
{
    return isAudioFile(details.description.toString());
}

void DrumRackPanel::itemDragEnter(const SourceDetails& details)
{
    dragHighlightPad = padAtPoint(details.localPosition);
    repaint();
}

void DrumRackPanel::itemDragMove(const SourceDetails& details)
{
    int newPad = padAtPoint(details.localPosition);
    if (newPad != dragHighlightPad) { dragHighlightPad = newPad; repaint(); }
}

void DrumRackPanel::itemDragExit(const SourceDetails&)
{
    dragHighlightPad = -1;
    repaint();
}

void DrumRackPanel::itemDropped(const SourceDetails& details)
{
    int pad = padAtPoint(details.localPosition);
    if (pad >= 0)
    {
        padSampleFiles[pad] = details.description.toString();
        selectedPad = pad;
    }
    dragHighlightPad = -1;
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// BeatWindow
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// BeatTransportBar
// ─────────────────────────────────────────────────────────────────────────────

BeatTransportBar::BeatTransportBar()
{
    for (auto* b : { &rtzBtn, &playBtn, &stopBtn, &recBtn })
        addAndMakeVisible(b);

    rtzBtn.onClick  = [this]() { if (onReturnToZero) onReturnToZero(); };
    playBtn.onClick = [this]() { if (onPlay)         onPlay(); };
    stopBtn.onClick = [this]() { if (onStop)         onStop(); };
    recBtn.onClick  = [this]() { if (onRecord)       onRecord(); };

    timecodeLabel.setText("00:00:00.000", juce::dontSendNotification);
    timecodeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timecodeLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(10, 12, 18));
    timecodeLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    timecodeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timecodeLabel);

    bpmLabel.setText("120.0 BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.75f));
    bpmLabel.setFont(juce::FontOptions(12.0f));
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bpmLabel);
}

void BeatTransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(14, 16, 24));
    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 18));
    g.drawRect(getLocalBounds(), 1);
}

void BeatTransportBar::resized()
{
    auto area = getLocalBounds().reduced(4, 3);
    const int btnSz = area.getHeight();
    const int gap   = 3;

    rtzBtn .setBounds(area.removeFromLeft(btnSz)); area.removeFromLeft(gap);
    playBtn.setBounds(area.removeFromLeft(btnSz)); area.removeFromLeft(gap);
    stopBtn.setBounds(area.removeFromLeft(btnSz)); area.removeFromLeft(gap);
    recBtn .setBounds(area.removeFromLeft(btnSz)); area.removeFromLeft(gap + 6);

    timecodeLabel.setBounds(area.removeFromLeft(115)); area.removeFromLeft(8);
    bpmLabel.setBounds(area.removeFromLeft(90));
}

void BeatTransportBar::setPlayState(bool playing, bool recording)
{
    playBtn.toggled = playing;
    recBtn.toggled  = recording;
    playBtn.repaint();
    recBtn.repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// BeatWindow
// ─────────────────────────────────────────────────────────────────────────────

BeatWindow::BeatWindow(NovaStudio::TransportState& transport)
    : stepSeq(transport)
{
    addAndMakeVisible(transportBar);
    addAndMakeVisible(browser);
    addAndMakeVisible(playlist);
    addAndMakeVisible(stepSeq);

    transportBar.onPlay          = [this]() { if (onPlay)          onPlay(); };
    transportBar.onStop          = [this]() { if (onStop)          onStop(); };
    transportBar.onRecord        = [this]() { if (onRecord)        onRecord(); };
    transportBar.onReturnToZero  = [this]() { if (onReturnToZero)  onReturnToZero(); };
}

BeatWindow::~BeatWindow() = default;

void BeatWindow::setPlayState(bool playing, bool recording)
{
    transportBar.setPlayState(playing, recording);
}

void BeatWindow::setTimecode(const juce::String& tc)
{
    transportBar.setTimecode(tc);
}

void BeatWindow::setBpm(double bpm)
{
    transportBar.setBpm(bpm);
}

void BeatWindow::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::bg());
}

void BeatWindow::resized()
{
    auto area = getLocalBounds();

    // Transport bar spans full width at top
    transportBar.setBounds(area.removeFromTop(40));
    area.removeFromTop(2);

    // Browser on left
    auto browserArea = area.removeFromLeft(180);
    area.removeFromLeft(2);

    // Pattern arranger takes top ~40% of remaining center
    const int playlistH = juce::roundToInt(area.getHeight() * 0.40f);
    playlist.setBounds(area.removeFromTop(playlistH));
    area.removeFromTop(2);

    // Step sequencer fills the rest
    stepSeq.setBounds(area);

    browser.setBounds(browserArea);
}
