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
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

int PatternPlaylist::blockAtPoint(int x, int y) const
{
    if (y < kHeaderH) return -1;
    int lane = (y - kHeaderH) / kLaneH;
    if (lane < 0 || lane >= kNumLanes) return -1;
    int barW = juce::jmax(1, (getWidth() - kLabelW) / numBars);
    int bar  = (x - kLabelW) / barW;

    for (int i = 0; i < blocks.size(); ++i)
    {
        const auto& blk = blocks.getReference(i);
        if (blk.lane == lane && bar >= blk.startBar && bar < blk.startBar + blk.lengthBars)
            return i;
    }
    return -1;
}

void PatternPlaylist::laneBarFromPoint(int x, int y, int& lane, int& bar) const
{
    lane = (y - kHeaderH) / kLaneH;
    int barW = juce::jmax(1, (getWidth() - kLabelW) / numBars);
    bar  = (x - kLabelW) / barW;
    lane = juce::jlimit(0, kNumLanes - 1, lane);
    bar  = juce::jlimit(0, numBars - 1, bar);
}

void PatternPlaylist::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::bg());

    int barW = juce::jmax(1, (getWidth() - kLabelW) / numBars);

    // Header row
    g.setColour(BeatTheme::panel());
    g.fillRect(0, 0, getWidth(), kHeaderH);
    g.setFont(juce::FontOptions(10.0f));
    for (int b = 0; b < numBars; ++b)
    {
        int x = kLabelW + b * barW;
        g.setColour((b % 4 == 0) ? BeatTheme::gridBeat() : BeatTheme::grid());
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
        if (b % 4 == 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawText(juce::String(b + 1), x + 2, 2, 24, 16, juce::Justification::left);
        }
    }

    // Lane rows
    const char* laneNames[] = { "Drums", "Bass", "Lead", "FX" };
    for (int ln = 0; ln < kNumLanes; ++ln)
    {
        int y = kHeaderH + ln * kLaneH;
        g.setColour(ln == selectedLane ? BeatTheme::panel().brighter(0.08f) : BeatTheme::panel());
        g.fillRect(0, y, kLabelW, kLaneH);
        g.setColour(BeatTheme::edge());
        g.drawRect(0, y, kLabelW, kLaneH, 1);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(laneNames[ln], 4, y, kLabelW - 4, kLaneH, juce::Justification::centredLeft);

        g.setColour(BeatTheme::stepOff().withAlpha(0.35f));
        g.fillRect(kLabelW, y, getWidth() - kLabelW, kLaneH);

        // Beat grid lines within lane
        for (int b = 0; b < numBars; ++b)
        {
            if (b % 4 == 0)
            {
                g.setColour(BeatTheme::gridBeat());
                g.drawVerticalLine(kLabelW + b * barW, (float)y, (float)(y + kLaneH));
            }
        }
    }

    // Pattern blocks
    for (int i = 0; i < blocks.size(); ++i)
    {
        const auto& blk = blocks.getReference(i);
        int x = kLabelW + blk.startBar * barW;
        int y = kHeaderH + blk.lane * kLaneH;
        int w = blk.lengthBars * barW - 2;
        auto r = juce::Rectangle<int>(x, y + 2, w, kLaneH - 4);
        g.setColour(blk.colour.withAlpha(i == dragBlockIdx ? 0.9f : 0.7f));
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
    if (e.y < kHeaderH) return;

    int lane, bar;
    laneBarFromPoint(e.x, e.y, lane, bar);
    selectedLane = lane;

    int hit = blockAtPoint(e.x, e.y);

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        // Right-click: erase block
        if (hit >= 0)
            blocks.remove(hit);
        repaint();
        return;
    }

    if (hit >= 0)
    {
        // Start dragging existing block
        dragBlockIdx   = hit;
        int barW = juce::jmax(1, (getWidth() - kLabelW) / numBars);
        dragOffsetBars = bar - blocks.getReference(hit).startBar;
    }
    else
    {
        // Stamp a new 4-bar block at click position
        blocks.add({ lane, bar, 4, activeColour, activeName });
        dragBlockIdx   = blocks.size() - 1;
        dragOffsetBars = 0;
    }
    repaint();
}

void PatternPlaylist::mouseDrag(const juce::MouseEvent& e)
{
    if (dragBlockIdx < 0 || dragBlockIdx >= blocks.size()) return;
    int lane, bar;
    laneBarFromPoint(e.x, e.y, lane, bar);
    auto& blk = blocks.getReference(dragBlockIdx);
    blk.startBar = juce::jmax(0, bar - dragOffsetBars);
    blk.lane     = lane;
    repaint();
}

void PatternPlaylist::mouseUp(const juce::MouseEvent&)
{
    dragBlockIdx = -1;
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
    // mute indicator
    if (x < 25) return 1;
    // pan indicator
    if (x < 50) return 2;
    // volume indicator
    if (x < 75) return 3;
    // name: rest
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

    for (int row = 0; row < kNumRows; ++row)
        paintChannelStrip(g, channelStripArea(row), row);

    paintStepGrid(g, stepGridArea());

    if (showGraphEditor)
        paintGraphEditor(g, graphArea());

    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void StepSequencerView::resized() {}

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

    int cx = r.getX() + 8 + 3;
    const int ctrlSz = juce::jmin(13, r.getHeight() - 8);
    const int ctrlY  = r.getCentreY() - ctrlSz / 2;

    // Mute indicator — small flat square, not a glowing LED
    auto ledR = juce::Rectangle<int>(cx, ctrlY, ctrlSz, ctrlSz);
    g.setColour(ch.muted ? juce::Colour::fromRGB(50, 52, 64) : BeatTheme::accent().withAlpha(0.85f));
    g.fillRoundedRectangle(ledR.toFloat(), 2.0f);
    g.setColour(BeatTheme::edge());
    g.drawRoundedRectangle(ledR.toFloat(), 2.0f, 1.0f);
    cx += ctrlSz + 6;

    // Pan indicator — flat bar with centred tick (no candy knob)
    auto panR = juce::Rectangle<int>(cx, ctrlY, ctrlSz + 6, ctrlSz);
    g.setColour(BeatTheme::stepOff());
    g.fillRoundedRectangle(panR.toFloat(), 2.0f);
    g.setColour(BeatTheme::edge());
    g.drawRoundedRectangle(panR.toFloat(), 2.0f, 1.0f);
    {
        float tickX = panR.getX() + 2.0f + (panR.getWidth() - 4.0f) * (0.5f + ch.pan * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.fillRect(juce::Rectangle<float>(tickX - 0.75f, (float)panR.getY() + 2.0f, 1.5f, (float)panR.getHeight() - 4.0f));
    }
    cx += ctrlSz + 6 + 6;

    // Volume indicator — flat fill bar
    auto volR = juce::Rectangle<int>(cx, ctrlY, ctrlSz + 6, ctrlSz);
    g.setColour(BeatTheme::stepOff());
    g.fillRoundedRectangle(volR.toFloat(), 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.fillRoundedRectangle(volR.toFloat().withWidth(volR.getWidth() * ch.volume), 2.0f);
    g.setColour(BeatTheme::edge());
    g.drawRoundedRectangle(volR.toFloat(), 2.0f, 1.0f);
    cx += ctrlSz + 6 + 8;

    // Name (flat, no rounded pill background — just a coloured underline when selected)
    auto nameR = juce::Rectangle<int>(cx, r.getY(), r.getRight() - cx - 4, r.getHeight());
    g.setColour(juce::Colours::white.withAlpha(ch.muted ? 0.35f : (isSelected ? 0.95f : 0.8f)));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(ch.name, nameR.reduced(4, 0), juce::Justification::centredLeft, true);
    if (isSelected)
    {
        g.setColour(ch.colour.withAlpha(0.8f));
        g.fillRect(nameR.getX(), r.getBottom() - 2, nameR.getWidth(), 2);
    }

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
                g.fillRoundedRectangle(cellR.expanded(0, 1), 2.0f);
            }

            if (active)
            {
                g.setColour(ch.colour);
                g.fillRoundedRectangle(cellR, 2.0f);
                g.setColour(ch.colour.brighter(0.4f).withAlpha(0.6f));
                g.drawRoundedRectangle(cellR.reduced(0.5f), 2.0f, 1.0f);

                // Inline velocity bar at bottom of cell (FL Studio style)
                float vel = ch.velocities[s];
                const float velH = 4.0f;
                float filledW = cellR.getWidth() * vel;
                g.setColour(juce::Colours::black.withAlpha(0.25f));
                g.fillRect(juce::Rectangle<float>(cellR.getX(), cellR.getBottom() - velH,
                                                  cellR.getWidth(), velH));
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.fillRect(juce::Rectangle<float>(cellR.getX(), cellR.getBottom() - velH,
                                                  filledW, velH));
            }
            else
            {
                g.setColour(BeatTheme::stepOff());
                g.fillRoundedRectangle(cellR, 2.0f);
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

    // ── Channel strip ─────────────────────────────────────────────────────────
    for (int row = 0; row < kNumRows; ++row)
    {
        auto sr = channelStripArea(row);
        if (sr.contains(pos))
        {
            selectedRow = row;
            auto localP = pos - sr.getTopLeft();
            int part = hitTestChannelStrip(localP);

            if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
            {
                if (part == 1) // right-click LED = solo menu
                {
                    juce::PopupMenu m;
                    m.addItem(1, "Solo");
                    m.addItem(2, "Unsolo all");
                    m.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result)
                    {
                        if (result == 1)
                        {
                            for (int r2 = 0; r2 < kNumRows; ++r2)
                            {
                                bool muted = (r2 != row);
                                currentPattern().channels[(size_t)r2].muted = muted;
                                if (onRowMutedChanged) onRowMutedChanged(r2, muted);
                            }
                        }
                        else if (result == 2)
                        {
                            for (int r2 = 0; r2 < kNumRows; ++r2)
                            {
                                currentPattern().channels[(size_t)r2].muted = false;
                                if (onRowMutedChanged) onRowMutedChanged(r2, false);
                            }
                        }
                        repaint();
                    });
                }
                else if (part == 4) // right-click name = sample editor popup
                {
                    auto& ch = currentPattern().channels[(size_t)row];
                    auto* popup = new SampleEditorPopup(ch.name, juce::String(), ch.volume, ch.pan, 0.0f);

                    popup->onVolumeChanged = [this, row](float v) {
                        currentPattern().channels[(size_t)row].volume = v;
                        if (onRowVolumeChanged) onRowVolumeChanged(row, v);
                    };
                    popup->onPanChanged = [this, row](float p) {
                        currentPattern().channels[(size_t)row].pan = p;
                        if (onRowPanChanged) onRowPanChanged(row, p);
                    };
                    popup->onLoadSample = [this, row, popup]() {
                        auto chooser = std::make_shared<juce::FileChooser>(
                            "Load Sample",
                            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                            "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
                        chooser->launchAsync(
                            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [this, row, chooser](const juce::FileChooser& fc)
                            {
                                auto f = fc.getResult();
                                if (f.existsAsFile())
                                {
                                    currentPattern().channels[(size_t)row].name = f.getFileNameWithoutExtension();
                                    if (onSampleAssigned) onSampleAssigned(row, f.getFullPathName());
                                    repaint();
                                }
                            });
                    };

                    auto* callout = &juce::CallOutBox::launchAsynchronously(
                        std::unique_ptr<juce::Component>(popup),
                        sr, nullptr);
                    juce::ignoreUnused(callout);
                }
                else
                {
                    handleChannelRightClickMenu(row);
                }
                return;
            }

            if (part == 1) // mute LED — left-click toggles mute
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
                    // Double-click = rename
                    renameChannel(row);
                }
                else
                {
                    // Single left-click = open file browser to load sample
                    auto chooser = std::make_shared<juce::FileChooser>(
                        "Load Sample for " + currentPattern().channels[(size_t)row].name,
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

            if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
            {
                // Right-click = deactivate step (FL Studio behavior)
                auto& active = currentPattern().channels[(size_t)row].steps[step];
                active = false;
                dragActivating = false;
                isDragging     = true;
                dragStartRow   = row;
                dragStartStep  = step;
                if (onStepChanged) onStepChanged(row, step, false);
                repaint();
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

    // Step grid drag — activate/deactivate range (works for both left and right button)
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
// SampleEditorPopup
// ─────────────────────────────────────────────────────────────────────────────

SampleEditorPopup::SampleEditorPopup(const juce::String& channelName,
                                     const juce::String& filePath,
                                     float volume, float pan, float pitch)
    : name(channelName), path(filePath), vol(volume), panVal(pan), pitchVal(pitch)
{
    setSize(320, 260);

    auto styleSlider = [](juce::Slider& s, double lo, double hi, double val)
    {
        s.setRange(lo, hi);
        s.setValue(val, juce::dontSendNotification);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
        s.setColour(juce::Slider::backgroundColourId,   juce::Colour::fromRGB(20, 23, 34));
        s.setColour(juce::Slider::thumbColourId,        juce::Colour::fromRGB(80, 100, 255));
        s.setColour(juce::Slider::trackColourId,        juce::Colour::fromRGB(50, 60, 120));
        s.setColour(juce::Slider::textBoxTextColourId,  juce::Colours::white.withAlpha(0.8f));
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(15, 18, 28));
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGBA(0,0,0,0));
    };

    styleSlider(volumeSlider, 0.0, 2.0, volume);
    volumeSlider.onValueChange = [this]() { if (onVolumeChanged) onVolumeChanged((float)volumeSlider.getValue()); };
    addAndMakeVisible(volumeSlider);

    styleSlider(panSlider, -1.0, 1.0, pan);
    panSlider.onValueChange = [this]() { if (onPanChanged) onPanChanged((float)panSlider.getValue()); };
    addAndMakeVisible(panSlider);

    styleSlider(pitchSlider, -24.0, 24.0, pitch);
    pitchSlider.onValueChange = [this]() { if (onPitchChanged) onPitchChanged((float)pitchSlider.getValue()); };
    addAndMakeVisible(pitchSlider);

    auto styleLabel = [](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        l.setFont(juce::FontOptions(11.0f));
    };
    styleLabel(volLabel,   "Volume");
    styleLabel(panLabel,   "Pan");
    styleLabel(pitchLabel, "Pitch (st)");
    addAndMakeVisible(volLabel);
    addAndMakeVisible(panLabel);
    addAndMakeVisible(pitchLabel);

    fileLabel.setText(path.isEmpty() ? "No sample loaded" : juce::File(path).getFileName(),
                      juce::dontSendNotification);
    fileLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    fileLabel.setFont(juce::FontOptions(10.0f));
    fileLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(fileLabel);

    loadBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour::fromRGB(28, 32, 50));
    loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    loadBtn.onClick = [this]() { if (onLoadSample) onLoadSample(); };
    addAndMakeVisible(loadBtn);
}

void SampleEditorPopup::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(14, 17, 26));
    g.setColour(juce::Colour::fromRGBA(255,255,255,18));
    g.drawRect(getLocalBounds(), 1);

    // Title bar
    auto titleR = getLocalBounds().removeFromTop(28);
    g.setColour(juce::Colour::fromRGB(20, 24, 38));
    g.fillRect(titleR);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(name, titleR.reduced(8, 0), juce::Justification::centredLeft);

    // Waveform placeholder
    auto waveR = juce::Rectangle<int>(8, 32, getWidth() - 16, 80);
    g.setColour(juce::Colour::fromRGB(10, 12, 20));
    g.fillRoundedRectangle(waveR.toFloat(), 4.0f);
    g.setColour(juce::Colour::fromRGBA(255,255,255,12));
    g.drawRoundedRectangle(waveR.toFloat(), 4.0f, 1.0f);

    if (path.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("No sample loaded — click Load Sample", waveR, juce::Justification::centred);
    }
    else
    {
        // Simple fake waveform squiggle
        juce::Path wave;
        const int n = waveR.getWidth();
        wave.startNewSubPath((float)waveR.getX(), (float)waveR.getCentreY());
        for (int i = 0; i < n; i += 2)
        {
            float t   = (float)i / (float)n;
            float amp = (0.3f + 0.5f * std::sin(t * 6.0f)) * (waveR.getHeight() * 0.4f);
            amp *= (1.0f - 0.5f * t);  // fade out toward end
            float y   = (float)waveR.getCentreY() + (i % 4 == 0 ? amp : -amp);
            wave.lineTo((float)(waveR.getX() + i), y);
        }
        g.setColour(juce::Colour::fromRGB(80, 100, 255).withAlpha(0.8f));
        g.strokePath(wave, juce::PathStrokeType(1.5f));
    }
}

void SampleEditorPopup::resized()
{
    const int labelW = 62;
    const int sliderH = 22;
    const int gap = 6;
    int y = 120;

    auto row = [&](juce::Label& lbl, juce::Slider& sl) {
        lbl.setBounds(8, y, labelW, sliderH);
        sl.setBounds(labelW + 10, y, getWidth() - labelW - 18, sliderH);
        y += sliderH + gap;
    };
    row(volLabel,   volumeSlider);
    row(panLabel,   panSlider);
    row(pitchLabel, pitchSlider);

    y += 4;
    fileLabel.setBounds(8, y, getWidth() - 16, 16);
    y += 20;
    loadBtn.setBounds(8, y, getWidth() - 16, 24);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeatPatternToolbar
// ─────────────────────────────────────────────────────────────────────────────

BeatPatternToolbar::BeatPatternToolbar()
{
    auto styleSm = [this](juce::TextButton& btn) {
        btn.setColour(juce::TextButton::buttonColourId,  juce::Colour::fromRGB(28, 32, 48));
        btn.setColour(juce::TextButton::buttonOnColourId, BeatTheme::accent());
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
        btn.setColour(juce::TextButton::textColourOnId,  juce::Colours::white);
        btn.addListener(this);
        addAndMakeVisible(&btn);
    };

    for (auto* b : { &prevPatBtn, &nextPatBtn, &addPatBtn, &patDropBtn,
                     &steps16Btn, &steps32Btn, &steps64Btn,
                     &swingDnBtn, &swingUpBtn,
                     &addChanBtn, &velGraphBtn, &patSongBtn })
        styleSm(*b);

    // Toggle-style buttons
    steps16Btn.setClickingTogglesState(false);
    steps32Btn.setClickingTogglesState(false);
    steps64Btn.setClickingTogglesState(false);
    velGraphBtn.setClickingTogglesState(false);
    patSongBtn.setClickingTogglesState(false);

    patNameLabel.setText("Pattern 1", juce::dontSendNotification);
    patNameLabel.setColour(juce::Label::textColourId,       juce::Colours::white);
    patNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(20, 24, 36));
    patNameLabel.setFont(juce::FontOptions(13.0f));
    patNameLabel.setJustificationType(juce::Justification::centred);
    patNameLabel.setInterceptsMouseClicks(false, false); // pass clicks to toolbar
    addAndMakeVisible(patNameLabel);

    // Style the dropdown button distinctly
    patDropBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour::fromRGB(40, 50, 80));
    patDropBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));

    swingLabel.setText("SWING 0%", juce::dontSendNotification);
    swingLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    swingLabel.setFont(juce::FontOptions(11.0f));
    swingLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(swingLabel);

    updateStepButtons();
}

BeatPatternToolbar::~BeatPatternToolbar()
{
    for (auto* b : { &prevPatBtn, &nextPatBtn, &addPatBtn, &patDropBtn,
                     &steps16Btn, &steps32Btn, &steps64Btn,
                     &swingDnBtn, &swingUpBtn,
                     &addChanBtn, &velGraphBtn, &patSongBtn })
        b->removeListener(this);
}

void BeatPatternToolbar::styleToggleBtn(juce::TextButton& btn)
{
    btn.setColour(juce::TextButton::buttonOnColourId, BeatTheme::accent());
}

void BeatPatternToolbar::updateStepButtons()
{
    steps16Btn.setColour(juce::TextButton::buttonColourId,
        currentSteps == 16  ? BeatTheme::accent() : juce::Colour::fromRGB(28, 32, 48));
    steps32Btn.setColour(juce::TextButton::buttonColourId,
        currentSteps == 32  ? BeatTheme::accent() : juce::Colour::fromRGB(28, 32, 48));
    steps64Btn.setColour(juce::TextButton::buttonColourId,
        currentSteps == 64  ? BeatTheme::accent() : juce::Colour::fromRGB(28, 32, 48));
    repaint();
}

void BeatPatternToolbar::setPatternName(const juce::String& name)
{
    patNameLabel.setText(name, juce::dontSendNotification);
}

void BeatPatternToolbar::setStepCount(int steps)
{
    currentSteps = steps;
    updateStepButtons();
}

void BeatPatternToolbar::setSwing(float pct)
{
    currentSwing = pct;
    swingLabel.setText("SWING " + juce::String(juce::roundToInt(pct)) + "%",
                       juce::dontSendNotification);
}

void BeatPatternToolbar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(16, 19, 28));
    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 14));
    g.drawRect(getLocalBounds(), 1);

    // Section dividers
    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 10));
}

void BeatPatternToolbar::resized()
{
    const int h   = getHeight() - 6;
    const int gap = 3;

    // Calculate total content width to center it
    // Pattern nav: 20 + 3 + 100 + 3 + 20 + 3 + 18 + 3 + 20 + (11) = 201
    // Steps:       28+3+28+3+28 + 11 = 101
    // Swing:       18+3+68+3+18 + 11 = 121
    // Tools:       56+3+38+3+68 = 168
    // Total ~ 591
    const int totalW = 20+3+100+3+20+3+18+3+20+11 + 28+3+28+3+28+11 + 18+3+68+3+18+11 + 56+3+38+3+68;
    int startX = juce::jmax(4, (getWidth() - totalW) / 2);
    const int y = 3;

    auto place = [&](juce::Component& c, int w) {
        c.setBounds(startX, y, w, h);
        startX += w + gap;
    };
    auto gap8 = [&]() { startX += 8; };

    // Pattern navigation: < [Name] [v] > +
    place(prevPatBtn,   20);
    place(patNameLabel, 100);
    place(patDropBtn,   18);
    place(nextPatBtn,   20);
    place(addPatBtn,    20);
    gap8();

    // Step count
    place(steps16Btn, 28);
    place(steps32Btn, 28);
    place(steps64Btn, 28);
    gap8();

    // Swing
    place(swingDnBtn,  18);
    place(swingLabel,  68);
    place(swingUpBtn,  18);
    gap8();

    // Tools
    place(patSongBtn,  56);
    place(velGraphBtn, 38);
    place(addChanBtn,  68);
}

void BeatPatternToolbar::buttonClicked(juce::Button* b)
{
    if (b == &prevPatBtn && onPrevPattern)  { onPrevPattern(); return; }
    if (b == &nextPatBtn && onNextPattern)  { onNextPattern(); return; }
    if (b == &addPatBtn  && onAddPattern)   { onAddPattern();  return; }
    if (b == &addChanBtn && onAddChannel)   { onAddChannel();  return; }

    if (b == &steps16Btn) { currentSteps = 16; updateStepButtons(); if (onStepCountChanged) onStepCountChanged(16); return; }
    if (b == &steps32Btn) { currentSteps = 32; updateStepButtons(); if (onStepCountChanged) onStepCountChanged(32); return; }
    if (b == &steps64Btn) { currentSteps = 64; updateStepButtons(); if (onStepCountChanged) onStepCountChanged(64); return; }

    if (b == &swingDnBtn)
    {
        currentSwing = juce::jmax(0.0f, currentSwing - 5.0f);
        setSwing(currentSwing);
        if (onSwingChanged) onSwingChanged(currentSwing);
        return;
    }
    if (b == &swingUpBtn)
    {
        currentSwing = juce::jmin(100.0f, currentSwing + 5.0f);
        setSwing(currentSwing);
        if (onSwingChanged) onSwingChanged(currentSwing);
        return;
    }
    if (b == &velGraphBtn)
    {
        showVel = !showVel;
        velGraphBtn.setColour(juce::TextButton::buttonColourId,
            showVel ? BeatTheme::accent() : juce::Colour::fromRGB(28, 32, 48));
        if (onShowVelocityGraph) onShowVelocityGraph(showVel);
        return;
    }
    if (b == &patSongBtn)
    {
        isPat = !isPat;
        patSongBtn.setButtonText(isPat ? "PAT" : "SONG");
        patSongBtn.setColour(juce::TextButton::buttonColourId,
            isPat ? BeatTheme::accent() : juce::Colour::fromRGB(28, 32, 48));
        if (onPatSongToggle) onPatSongToggle(isPat);
        return;
    }
    if (b == &patDropBtn)
    {
        showPatternDropdown();
        return;
    }
}

void BeatPatternToolbar::mouseDown(const juce::MouseEvent& e)
{
    // Click or right-click on the pattern name label = show dropdown
    if (patNameLabel.getBounds().contains(e.getPosition()))
        showPatternDropdown();
    else if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
        showPatternDropdown();
}

void BeatPatternToolbar::showPatternDropdown()
{
    juce::PopupMenu m;
    m.addItem(1,  "Find first empty");
    m.addItem(2,  "Find next empty");
    m.addSeparator();
    m.addItem(3,  "Rename and color...");
    m.addItem(4,  "Change color...");
    m.addItem(5,  "Random color");
    m.addSeparator();
    m.addItem(6,  "Insert one");
    m.addItem(7,  "Clone");
    m.addItem(8,  "Delete");
    m.addSeparator();
    m.addItem(9,  "Move up");
    m.addItem(10, "Move down");

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&patDropBtn),
        [this](int result)
        {
            if (!onPatternMenuAction) return;
            switch (result)
            {
                case 1:  onPatternMenuAction("findfirst");   break;
                case 2:  onPatternMenuAction("findnext");    break;
                case 3:  onPatternMenuAction("rename");      break;
                case 4:  onPatternMenuAction("color");       break;
                case 5:  onPatternMenuAction("randomcolor"); break;
                case 6:  onPatternMenuAction("insert");      break;
                case 7:  onPatternMenuAction("clone");       break;
                case 8:  onPatternMenuAction("delete");      break;
                case 9:  onPatternMenuAction("moveup");      break;
                case 10: onPatternMenuAction("movedown");    break;
                default: break;
            }
        });
}

// ─────────────────────────────────────────────────────────────────────────────
// BeatWindow
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// BeatTransportBar
// ─────────────────────────────────────────────────────────────────────────────

BeatTransportBar::BeatTransportBar()
{
    for (auto* b : { &rtzBtn, &playBtn, &stopBtn, &recBtn,
                     &stepGridViewBtn, &pianoRollViewBtn, &mixerViewBtn })
        addAndMakeVisible(b);

    rtzBtn.onClick  = [this]() { if (onReturnToZero) onReturnToZero(); };
    playBtn.onClick = [this]() { if (onPlay)         onPlay(); };
    stopBtn.onClick = [this]() { if (onStop)         onStop(); };
    recBtn.onClick  = [this]() { if (onRecord)       onRecord(); };

    stepGridViewBtn.toggled = true;
    stepGridViewBtn.onClick  = [this]() { setActiveView(0); if (onShowStepSequencer) onShowStepSequencer(); };
    pianoRollViewBtn.onClick = [this]() { setActiveView(1); if (onShowPianoRoll)     onShowPianoRoll(); };
    mixerViewBtn.onClick     = [this]() { if (onShowMixer) onShowMixer(); };

    timecodeLabel.setText("00:00:00.000", juce::dontSendNotification);
    timecodeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timecodeLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(10, 12, 18));
    timecodeLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    timecodeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timecodeLabel);

    bpmLabel.setBPM(120);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(220, 170, 60));
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setTooltip("Drag up/down to change BPM, or double-click to type");
    bpmLabel.onTempoChanged = [this](int bpm) { if (onTempoChanged) onTempoChanged(bpm); };
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
    const int h     = getHeight() - 6;
    const int btnSz = h;
    const int gap   = 4;

    // Total width of transport content: 4 buttons + gaps + timecode + gap + bpm
    const int totalW = (btnSz * 4) + (gap * 3) + 10 + 115 + 8 + 90;
    int x = juce::jmax(6, (getWidth() - totalW) / 2);
    const int y = 3;

    auto place = [&](juce::Component& c, int w) {
        c.setBounds(x, y, w, h); x += w + gap;
    };

    place(rtzBtn,  btnSz);
    place(playBtn, btnSz);
    place(stopBtn, btnSz);
    place(recBtn,  btnSz);
    x += 10;
    place(timecodeLabel, 115);
    x += 4;
    place(bpmLabel, 90);

    // View-switcher icons (FL Studio-style) — right-aligned
    int vx = getWidth() - 6 - btnSz * 3 - gap * 2;
    auto placeAt = [&](juce::Component& c) { c.setBounds(vx, y, btnSz, h); vx += btnSz + gap; };
    placeAt(stepGridViewBtn);
    placeAt(pianoRollViewBtn);
    placeAt(mixerViewBtn);
}

void BeatTransportBar::setActiveView(int viewIndex)
{
    stepGridViewBtn.toggled  = (viewIndex == 0);
    pianoRollViewBtn.toggled = (viewIndex == 1);
    stepGridViewBtn.repaint();
    pianoRollViewBtn.repaint();
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
    addAndMakeVisible(patternToolbar);
    addAndMakeVisible(browser);
    addAndMakeVisible(playlist);
    addAndMakeVisible(stepSeq);
    addChildComponent(pianoRoll);   // hidden until the user switches views

    transportBar.onPlay          = [this]() { if (onPlay)          onPlay(); };
    transportBar.onStop          = [this]() { if (onStop)          onStop(); };
    transportBar.onRecord        = [this]() { if (onRecord)        onRecord(); };
    transportBar.onReturnToZero  = [this]() { if (onReturnToZero)  onReturnToZero(); };
    transportBar.onTempoChanged  = [this](int bpm) { if (onTempoChanged) onTempoChanged(bpm); };

    // FL Studio-style view switcher: Step sequencer / Piano roll / Mixer
    transportBar.onShowStepSequencer = [this]() {
        showingPianoRoll = false;
        stepSeq.setVisible(true);
        pianoRoll.setVisible(false);
    };
    transportBar.onShowPianoRoll = [this]() {
        showingPianoRoll = true;
        stepSeq.setVisible(false);
        pianoRoll.setVisible(true);
    };
    transportBar.onShowMixer = [this]() { if (onShowMixer) onShowMixer(); };

    // Wire pattern toolbar → step sequencer
    patternToolbar.onStepCountChanged = [this](int steps) {
        stepSeq.currentPattern().stepCount = steps;
        if (stepSeq.onStepCountChanged) stepSeq.onStepCountChanged(steps);
        stepSeq.repaint();
    };
    patternToolbar.onSwingChanged = [this](float pct) {
        stepSeq.currentPattern().swing = pct / 100.0f;
        if (stepSeq.onSwingChanged) stepSeq.onSwingChanged(pct / 100.0f);
    };
    auto syncPatternUI = [this]() {
        const auto& p = stepSeq.currentPattern();
        patternToolbar.setPatternName(p.name);
        patternToolbar.setStepCount(p.stepCount);
        // Use the first channel's colour as the pattern colour for the playlist stamp
        playlist.setActivePattern(p.name, p.channels[0].colour);
        stepSeq.repaint();
    };

    patternToolbar.onNextPattern = [this, syncPatternUI]() {
        stepSeq.currentPatternIdx = juce::jmin(stepSeq.currentPatternIdx + 1,
                                               stepSeq.patterns.size() - 1);
        syncPatternUI();
    };
    patternToolbar.onPrevPattern = [this, syncPatternUI]() {
        stepSeq.currentPatternIdx = juce::jmax(0, stepSeq.currentPatternIdx - 1);
        syncPatternUI();
    };
    patternToolbar.onAddPattern = [this, syncPatternUI]() {
        StepSequencerView::Pattern p;
        p.name = "Pattern " + juce::String(stepSeq.patterns.size() + 1);
        stepSeq.patterns.add(p);
        stepSeq.currentPatternIdx = stepSeq.patterns.size() - 1;
        syncPatternUI();
    };
    patternToolbar.onShowVelocityGraph = [this](bool show) {
        stepSeq.showGraphEditor = show;
        stepSeq.repaint();
    };
    patternToolbar.onPatSongToggle = [this](bool isPat) {
        patMode = isPat;
    };

    patternToolbar.onPatternMenuAction = [this, syncPatternUI](const juce::String& action)
    {
        if (action == "findfirst")
        {
            // Jump to first pattern with all steps off
            for (int i = 0; i < stepSeq.patterns.size(); ++i)
            {
                const auto& p = stepSeq.patterns.getReference(i);
                bool empty = true;
                for (int r = 0; r < StepSequencerView::kNumRows && empty; ++r)
                    for (int s = 0; s < p.stepCount && empty; ++s)
                        if (p.channels[r].steps[s]) empty = false;
                if (empty) { stepSeq.currentPatternIdx = i; syncPatternUI(); return; }
            }
        }
        else if (action == "findnext")
        {
            for (int i = stepSeq.currentPatternIdx + 1; i < stepSeq.patterns.size(); ++i)
            {
                const auto& p = stepSeq.patterns.getReference(i);
                bool empty = true;
                for (int r = 0; r < StepSequencerView::kNumRows && empty; ++r)
                    for (int s = 0; s < p.stepCount && empty; ++s)
                        if (p.channels[r].steps[s]) empty = false;
                if (empty) { stepSeq.currentPatternIdx = i; syncPatternUI(); return; }
            }
        }
        else if (action == "rename")
        {
            stepSeq.showInlineRename({4, 0, 140, 22},
                                     stepSeq.currentPattern().name, true);
        }
        else if (action == "color")
        {
            static const juce::Colour cols[] = {
                juce::Colour::fromRGB(80,100,255), juce::Colour::fromRGB(255,100,60),
                juce::Colour::fromRGB(60,200,120), juce::Colour::fromRGB(220,180,40),
                juce::Colour::fromRGB(160,60,220), juce::Colour::fromRGB(60,180,255)
            };
            static int ci = 0;
            for (int r = 0; r < StepSequencerView::kNumRows; ++r)
                stepSeq.currentPattern().channels[r].colour = cols[ci % 6];
            ci++;
            syncPatternUI();
        }
        else if (action == "randomcolor")
        {
            auto c = juce::Colour::fromHSV(juce::Random::getSystemRandom().nextFloat(), 0.7f, 0.9f, 1.0f);
            for (int r = 0; r < StepSequencerView::kNumRows; ++r)
                stepSeq.currentPattern().channels[r].colour = c;
            syncPatternUI();
        }
        else if (action == "insert")
        {
            StepSequencerView::Pattern p;
            p.name = "Pattern " + juce::String(stepSeq.patterns.size() + 1);
            stepSeq.patterns.insert(stepSeq.currentPatternIdx, p);
            syncPatternUI();
        }
        else if (action == "clone")
        {
            auto copy = stepSeq.currentPattern();
            copy.name += " (copy)";
            stepSeq.patterns.insert(stepSeq.currentPatternIdx + 1, copy);
            stepSeq.currentPatternIdx++;
            syncPatternUI();
        }
        else if (action == "delete")
        {
            if (stepSeq.patterns.size() > 1)
            {
                stepSeq.patterns.remove(stepSeq.currentPatternIdx);
                stepSeq.currentPatternIdx = juce::jmax(0, stepSeq.currentPatternIdx - 1);
                syncPatternUI();
            }
        }
        else if (action == "moveup" && stepSeq.currentPatternIdx > 0)
        {
            stepSeq.patterns.swap(stepSeq.currentPatternIdx, stepSeq.currentPatternIdx - 1);
            stepSeq.currentPatternIdx--;
            syncPatternUI();
        }
        else if (action == "movedown" && stepSeq.currentPatternIdx < stepSeq.patterns.size() - 1)
        {
            stepSeq.patterns.swap(stepSeq.currentPatternIdx, stepSeq.currentPatternIdx + 1);
            stepSeq.currentPatternIdx++;
            syncPatternUI();
        }
    };
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

    // Row 1: transport bar (full width)
    transportBar.setBounds(area.removeFromTop(40));
    area.removeFromTop(1);

    // Row 2: pattern toolbar (full width)
    patternToolbar.setBounds(area.removeFromTop(28));
    area.removeFromTop(2);

    // Browser on left
    auto browserArea = area.removeFromLeft(180);
    area.removeFromLeft(2);

    // Pattern arranger takes top ~38% of remaining center
    const int playlistH = juce::roundToInt(area.getHeight() * 0.38f);
    playlist.setBounds(area.removeFromTop(playlistH));
    area.removeFromTop(2);

    // Step sequencer / piano roll fill the rest (mutually exclusive views)
    stepSeq.setBounds(area);
    pianoRoll.setBounds(area);

    browser.setBounds(browserArea);
}
