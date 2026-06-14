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

void PianoRollView::mouseDown(const juce::MouseEvent& /*e*/) {}

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
    steps[0][0] = steps[0][4] = steps[0][8]  = steps[0][12] = true;
    steps[1][2] = steps[1][6] = steps[1][10] = steps[1][14] = true;
    steps[2][0] = steps[2][2] = steps[2][4]  = steps[2][6]  =
    steps[2][8] = steps[2][10]= steps[2][12] = steps[2][14] = true;
    steps[3][3] = steps[3][11] = true;

    startTimerHz(30);
}

StepSequencerView::~StepSequencerView()
{
    stopTimer();
}

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
            int newCursor = ((int)(posSamples / sr * stepsPerSec)) % kNumSteps;
            if (newCursor != cursorStep) { cursorStep = newCursor; repaint(); }
        }
    }
    else if (cursorStep != -1)
    {
        cursorStep = -1;
        repaint();
    }
}

void StepSequencerView::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::bg());

    g.setColour(BeatTheme::panel());
    g.fillRect(0, 0, getWidth(), kTopH);

    int stepW = juce::jmax(1, (getWidth() - kHeaderW) / kNumSteps);
    g.setFont(juce::FontOptions(10.0f));
    for (int s = 0; s < kNumSteps; ++s)
    {
        int x = kHeaderW + s * stepW;
        if (s % 4 == 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawText(juce::String(s / 4 + 1), x + 2, 2, 20, kTopH - 4, juce::Justification::left);
        }
        g.setColour(s % 4 == 0 ? BeatTheme::gridBeat() : BeatTheme::grid());
        g.drawVerticalLine(x, 0.0f, (float)kTopH);
    }

    for (int row = 0; row < kNumRows; ++row)
    {
        int y = kTopH + row * kRowH;
        drawRowHeader(g, { 0, y, kHeaderW, kRowH }, row, muted[row], soloed[row]);
        for (int s = 0; s < kNumSteps; ++s)
        {
            int x = kHeaderW + s * stepW;
            drawStep(g, { x + 1, y + 2, stepW - 2, kRowH - 4 },
                     steps[row][s], s == cursorStep);
        }
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void StepSequencerView::resized() {}

void StepSequencerView::mouseDown(const juce::MouseEvent& e)
{
    int stepW = juce::jmax(1, (getWidth() - kHeaderW) / kNumSteps);
    int gridX = e.x - kHeaderW;
    int gridY = e.y - kTopH;
    if (gridX < 0 || gridY < 0) return;

    int row = gridY / kRowH;
    int col = gridX / stepW;
    if (row < kNumRows && col < kNumSteps)
    {
        steps[row][col] = !steps[row][col];
        repaint();
    }
}

void StepSequencerView::drawRowHeader(juce::Graphics& g, juce::Rectangle<int> r,
                                       int row, bool isMuted, bool isSolo) const
{
    g.setColour(BeatTheme::panel());
    g.fillRect(r);
    g.setColour(kRowColours[row].withAlpha(0.6f));
    g.fillRect(r.getX(), r.getY(), 3, r.getHeight());

    g.setColour(juce::Colours::white.withAlpha(isMuted ? 0.35f : 0.85f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(kRowNames[row], r.getX() + 6, r.getY(), 44, r.getHeight(), juce::Justification::centredLeft);

    auto mr  = juce::Rectangle<int>(r.getRight() - 34, r.getCentreY() - 8, 14, 16);
    auto sr2 = juce::Rectangle<int>(r.getRight() - 18, r.getCentreY() - 8, 14, 16);

    g.setColour(isMuted ? juce::Colour::fromRGB(255,160,40) : BeatTheme::stepOff());
    g.fillRoundedRectangle(mr.toFloat(), 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("M", mr, juce::Justification::centred);

    g.setColour(isSolo ? juce::Colour::fromRGB(160,80,255) : BeatTheme::stepOff());
    g.fillRoundedRectangle(sr2.toFloat(), 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawText("S", sr2, juce::Justification::centred);

    g.setColour(BeatTheme::edge());
    g.drawRect(r, 1);
}

void StepSequencerView::drawStep(juce::Graphics& g, juce::Rectangle<int> r,
                                  bool active, bool isCursor) const
{
    if (isCursor)
    {
        g.setColour(BeatTheme::stepCursor());
        g.fillRoundedRectangle(r.toFloat(), 3.0f);
    }

    if (active)
    {
        g.setColour(BeatTheme::stepOn());
        g.fillRoundedRectangle(r.toFloat().reduced(isCursor ? 0.0f : 1.0f), 3.0f);
        g.setColour(BeatTheme::accentGlow());
        g.drawRoundedRectangle(r.toFloat().reduced(1.0f), 3.0f, 1.5f);
    }
    else
    {
        g.setColour(BeatTheme::stepOff());
        g.fillRoundedRectangle(r.toFloat().reduced(1.0f), 3.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrumRackPanel
// ─────────────────────────────────────────────────────────────────────────────

DrumRackPanel::DrumRackPanel() {}

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
            drawPad(g, r, kPadNames[idx], idx == selectedPad, kPadAccents[row]);
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
        repaint();
    }
}

void DrumRackPanel::drawPad(juce::Graphics& g, juce::Rectangle<int> r,
                              const juce::String& name, bool selected,
                              juce::Colour accent) const
{
    g.setColour(selected ? accent.withAlpha(0.55f) : BeatTheme::padBase());
    g.fillRoundedRectangle(r.toFloat(), 5.0f);

    g.setColour(accent.withAlpha(selected ? 0.9f : 0.25f));
    g.drawRoundedRectangle(r.toFloat().reduced(1.0f), 5.0f, selected ? 1.5f : 1.0f);

    g.setColour(juce::Colours::white.withAlpha(selected ? 1.0f : 0.65f));
    g.setFont(juce::FontOptions(9.5f));
    g.drawText(name, r.reduced(3, 0), juce::Justification::centred, true);

    g.setColour(accent.withAlpha(selected ? 0.9f : 0.4f));
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
        g.drawText("-- empty --", slot.reduced(4, 0), juce::Justification::centredLeft);
    }

    g.setColour(BeatTheme::edge());
    g.drawRoundedRectangle(area.toFloat().reduced(2.0f), 4.0f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeatWindow
// ─────────────────────────────────────────────────────────────────────────────

BeatWindow::BeatWindow(NovaStudio::TransportState& transport)
    : stepSeq(transport)
{
    addAndMakeVisible(browser);
    addAndMakeVisible(playlist);
    addAndMakeVisible(pianoRoll);
    addAndMakeVisible(stepSeq);
    addAndMakeVisible(drumRack);
}

BeatWindow::~BeatWindow() = default;

void BeatWindow::paint(juce::Graphics& g)
{
    g.fillAll(BeatTheme::bg());
}

void BeatWindow::resized()
{
    auto area = getLocalBounds().reduced(4);

    browser .setBounds(area.removeFromLeft(180));
    area.removeFromLeft(3);
    drumRack.setBounds(area.removeFromRight(220));
    area.removeFromRight(3);

    auto centerTop = area.removeFromTop(area.getHeight() / 4);
    playlist.setBounds(centerTop);
    area.removeFromTop(3);

    const int seqH = StepSequencerView::kTopH + StepSequencerView::kNumRows * StepSequencerView::kRowH + 6;
    stepSeq.setBounds(area.removeFromBottom(seqH));
    area.removeFromBottom(3);
    pianoRoll.setBounds(area);
}
