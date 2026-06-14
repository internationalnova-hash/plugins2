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
    auto snapArea  = bounds.removeFromBottom(kSnapH);
    auto toolsArea = bounds.removeFromBottom(kToolsH);
    auto velArea   = bounds.removeFromBottom(kVelHeight);
    auto mainArea  = bounds;

    auto keysArea = mainArea.removeFromLeft(kKeyWidth);
    auto gridArea = mainArea;

    g.fillAll(BeatTheme::bg());
    drawPianoKeys(g, keysArea);
    drawGrid(g, gridArea);
    drawNotes(g, gridArea);
    drawVelocityLane(g, velArea);
    drawToolsRow(g, toolsArea);
    drawSnapControls(g, snapArea);
}

void PianoRollView::resized() {}

int PianoRollView::hitTestNote(int pitch, int step) const
{
    for (int i = notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes.getReference(i);
        if (n.pitch == pitch && step >= n.startStep && step < n.startStep + n.lengthSteps)
            return i;
    }
    return -1;
}

int PianoRollView::snapDivisorSteps() const
{
    // Grid steps are 1/16-note resolution; map snap choice to a step count.
    switch (snapMode)
    {
        case 0: return 4; // 1/4
        case 1: return 2; // 1/8
        case 2: return 1; // 1/16
        case 3: return 1; // 1/32 — finer than grid resolution, falls back to per-step
        default: return 1; // Free
    }
}

void PianoRollView::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds();
    auto snapArea  = bounds.removeFromBottom(kSnapH);
    auto toolsArea = bounds.removeFromBottom(kToolsH);
    bounds.removeFromBottom(kVelHeight);
    auto mainArea = bounds;
    auto keysArea = mainArea.removeFromLeft(kKeyWidth);
    auto gridArea = mainArea;

    // ── On-screen keyboard: click a key to audition its pitch ───────────
    if (keysArea.contains(e.getPosition()))
    {
        const int pitch = pitchAtKeyboardY(e.y - keysArea.getY());
        if (pitch >= 0)
        {
            activeKeyPitch = pitch;
            if (onPianoKeyEvent) onPianoKeyEvent(pitch, true);
            repaint();
        }
        return;
    }

    // ── Snap row clicks ──────────────────────────────────────────────────
    if (snapArea.contains(e.getPosition()))
    {
        const int btnX0 = snapArea.getX() + 92;
        const int idx = (e.x - btnX0) / 38;
        if (idx >= 0 && idx <= 4 && e.x < btnX0 + 5 * 38)
            setSnapMode(idx);
        return;
    }

    // ── Tools row clicks ─────────────────────────────────────────────────
    if (toolsArea.contains(e.getPosition()))
    {
        static const char* toolNames[] = { "Strum", "Flam", "Chop", "Arp", "Scale", "Ghost" };
        const int btnW = 56;
        int btnX0 = toolsArea.getX() + 6;
        int idx = (e.x - btnX0) / btnW;
        if (idx >= 0 && idx < 6 && e.x < btnX0 + 6 * btnW)
        {
            switch (idx)
            {
                case 0: // Strum — ascending by default, shift = descending
                    toolStrum(! e.mods.isShiftDown());
                    break;
                case 1: toolFlam(); break;
                case 2: toolChop(e.mods.isShiftDown() ? 4 : 2); break;
                case 3: // Arpeggiate — cycle pattern with shift, default up
                    toolArpeggiate(e.mods.isShiftDown() ? 1 : 0);
                    break;
                case 4: showScalePickerMenu(); break;
                case 5: setShowGhostNotes(! showGhostNotes); break;
                default: break;
            }
            juce::ignoreUnused(toolNames);
        }
        return;
    }

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
    int hitIdx = hitTestNote(pitch, step);

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        if (hitIdx >= 0)
        {
            if (! selectedNotes.contains(hitIdx))
            {
                if (! e.mods.isShiftDown())
                    selectedNotes.clear();
                selectedNotes.add(hitIdx);
            }
            showNoteContextMenu(hitIdx);
        }
        else
        {
            showGridContextMenu();
        }
        repaint();
        return;
    }

    if (hitIdx >= 0)
    {
        // Select (shift = add/remove from selection)
        if (e.mods.isShiftDown())
        {
            if (selectedNotes.contains(hitIdx))
                selectedNotes.removeFirstMatchingValue(hitIdx);
            else
                selectedNotes.add(hitIdx);
        }
        else if (e.getNumberOfClicks() >= 2)
        {
            // Double-click = delete
            notes.remove(hitIdx);
            selectedNotes.clear();
        }
        else
        {
            selectedNotes.clearQuick();
            selectedNotes.add(hitIdx);
        }
        repaint();
        return;
    }

    // Snap the new note's start position to the active grid resolution
    int divisor = snapDivisorSteps();
    int snappedStep = (divisor > 1) ? (step / divisor) * divisor : step;

    notes.add({ pitch, snappedStep, 1, 0.8f });
    if (! e.mods.isShiftDown())
        selectedNotes.clearQuick();
    selectedNotes.add(notes.size() - 1);
    repaint();
}

void PianoRollView::mouseDrag(const juce::MouseEvent& e)
{
    if (activeKeyPitch < 0)
        return;

    auto bounds = getLocalBounds();
    bounds.removeFromBottom(kSnapH);
    bounds.removeFromBottom(kToolsH);
    bounds.removeFromBottom(kVelHeight);
    auto keysArea = bounds.removeFromLeft(kKeyWidth);

    // Slide across the on-screen keyboard to glide between pitches, FL-style
    const int pitch = keysArea.getX() <= e.x && e.x < keysArea.getRight()
                          ? pitchAtKeyboardY(e.y - keysArea.getY())
                          : -1;

    if (pitch >= 0 && pitch != activeKeyPitch)
    {
        if (onPianoKeyEvent) onPianoKeyEvent(activeKeyPitch, false);
        activeKeyPitch = pitch;
        if (onPianoKeyEvent) onPianoKeyEvent(activeKeyPitch, true);
        repaint();
    }
}

void PianoRollView::mouseUp(const juce::MouseEvent&)
{
    if (activeKeyPitch >= 0)
    {
        if (onPianoKeyEvent) onPianoKeyEvent(activeKeyPitch, false);
        activeKeyPitch = -1;
        repaint();
    }
}

int PianoRollView::pitchAtKeyboardY(int relativeY) const
{
    const int totalPitches = kNumOctaves * 12;
    const int startPitch   = 36;

    const int y = relativeY - kHeaderH;
    if (y < 0) return -1;

    const int row = y / kRowHeight;
    if (row < 0 || row >= totalPitches) return -1;

    return startPitch + (totalPitches - 1 - row);
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

        auto baseColour = isBlack ? juce::Colour::fromRGB(30,30,40) : juce::Colour::fromRGB(200,200,210);
        g.setColour(pitch == activeKeyPitch ? BeatTheme::accent() : baseColour);
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

        // Highlight rows that fall within the active scale (FL-style scale guide)
        if (scaleRoot >= 0 && isPitchInScale(pitch))
        {
            g.setColour(BeatTheme::accent().withAlpha(0.10f));
            g.fillRect(area.getX(), y, area.getWidth(), kRowHeight);
        }

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

    if (showGhostNotes)
    {
        for (const auto& ghost : ghostNotes)
        {
            int pitchIdx = (startPitch + totalPitches - 1 - ghost.pitch);
            if (pitchIdx < 0 || pitchIdx >= totalPitches) continue;

            int x = area.getX() + ghost.startStep * stepW;
            int y = area.getY() + kHeaderH + pitchIdx * kRowHeight;
            int w = ghost.lengthSteps * stepW - 2;
            int h = kRowHeight - 1;

            auto gr = juce::Rectangle<int>(x, y, w, h);
            g.setColour(BeatTheme::noteBlock().withAlpha(0.18f));
            g.fillRoundedRectangle(gr.toFloat(), 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(gr.toFloat(), 2.0f, 1.0f);
        }
    }

    for (int i = 0; i < notes.size(); ++i)
    {
        const auto& note = notes.getReference(i);
        int pitchIdx = (startPitch + totalPitches - 1 - note.pitch);
        if (pitchIdx < 0 || pitchIdx >= totalPitches) continue;

        int x = area.getX() + note.startStep * stepW;
        int y = area.getY() + kHeaderH + pitchIdx * kRowHeight;
        int w = note.lengthSteps * stepW - 2;
        int h = kRowHeight - 1;

        const bool isSelected = selectedNotes.contains(i);

        auto nr = juce::Rectangle<int>(x, y, w, h);
        g.setColour((isSelected ? BeatTheme::noteBlock().brighter(0.5f) : BeatTheme::noteBlock()).withAlpha(0.85f));
        g.fillRoundedRectangle(nr.toFloat(), 2.0f);
        g.setColour(isSelected ? juce::Colours::white.withAlpha(0.9f) : BeatTheme::noteBlock().brighter(0.4f));
        g.drawRoundedRectangle(nr.toFloat(), 2.0f, isSelected ? 1.6f : 1.0f);

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

    static const char* labels[] = { "1/4", "1/8", "1/16", "1/32", "Free" };
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Snap: " + juce::String(labels[juce::jlimit(0, 4, snapMode)]),
               area.getX() + 8, area.getY(), 80, area.getHeight(), juce::Justification::centredLeft);

    int btnX = area.getX() + 92;
    for (int i = 0; i < 5; ++i)
    {
        bool active = (i == snapMode);
        g.setColour(active ? BeatTheme::accent() : BeatTheme::stepOff());
        g.fillRoundedRectangle((float)btnX, (float)(area.getY() + 3), 34.0f, (float)(area.getHeight() - 6), 3.0f);
        g.setColour(juce::Colours::white.withAlpha(active ? 1.0f : 0.6f));
        g.drawText(labels[i], btnX, area.getY(), 34, area.getHeight(), juce::Justification::centred);
        btnX += 38;
    }

    g.setColour(BeatTheme::edge());
    g.drawRect(area, 1);
}

void PianoRollView::drawToolsRow(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(BeatTheme::panel().brighter(0.03f));
    g.fillRect(area);

    static const char* names[] = { "Strum", "Flam", "Chop", "Arp", "Scale", "Ghost" };
    const int btnW = 56;
    int btnX = area.getX() + 6;
    for (int i = 0; i < 6; ++i)
    {
        bool isScaleBtn = (i == 4);
        bool isGhostBtn = (i == 5);
        bool active = (isScaleBtn && scaleRoot >= 0) || (isGhostBtn && showGhostNotes);
        auto br = juce::Rectangle<float>((float)btnX, (float)(area.getY() + 3), (float)(btnW - 6), (float)(area.getHeight() - 6));
        g.setColour(active ? BeatTheme::accent().withAlpha(0.8f) : BeatTheme::stepOff());
        g.fillRoundedRectangle(br, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(active ? 1.0f : 0.7f));
        g.setFont(juce::FontOptions(10.0f));
        juce::String label = names[i];
        if (isScaleBtn && scaleRoot >= 0)
        {
            static const char* rootNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
            label = juce::String(rootNames[scaleRoot]) + " " + scaleTypeName(scaleType);
        }
        g.drawText(label, br.toNearestInt(), juce::Justification::centred, true);
        btnX += btnW;
    }

    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(selectedNotes.isEmpty() ? "Select notes, then apply a tool"
                                       : (juce::String(selectedNotes.size()) + " note(s) selected"),
               btnX + 8, area.getY(), area.getWidth() - (btnX + 8 - area.getX()), area.getHeight(),
               juce::Justification::centredLeft, true);

    g.setColour(BeatTheme::edge());
    g.drawRect(area, 1);
}

// ── Snap / scale state ───────────────────────────────────────────────────────

void PianoRollView::setSnapMode(int mode)
{
    snapMode = juce::jlimit(0, 4, mode);
    repaint();
}

void PianoRollView::setGhostNotes(juce::Array<NoteBlock> ghosts)
{
    ghostNotes = std::move(ghosts);
    repaint();
}

void PianoRollView::setShowGhostNotes(bool shouldShow)
{
    showGhostNotes = shouldShow;
    repaint();
}

const char* PianoRollView::scaleTypeName(int type)
{
    static const char* names[] = { "Major", "Minor", "Dorian", "Mixolydian", "Chromatic" };
    return names[juce::jlimit(0, kNumScaleTypes - 1, type)];
}

void PianoRollView::setScale(int rootPitchClass, int newScaleType)
{
    scaleRoot = juce::jlimit(-1, 11, rootPitchClass);
    scaleType = juce::jlimit(0, kNumScaleTypes - 1, newScaleType);
    repaint();
}

bool PianoRollView::isPitchInScale(int pitch) const
{
    if (scaleRoot < 0) return false;
    if (scaleType == 4) return true; // Chromatic — every note belongs

    static const int intervals[kNumScaleTypes - 1][7] = {
        { 0, 2, 4, 5, 7, 9, 11 },   // Major
        { 0, 2, 3, 5, 7, 8, 10 },   // Natural minor
        { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
        { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydian
    };
    int rel = ((pitch - scaleRoot) % 12 + 12) % 12;
    for (int iv : intervals[scaleType])
        if (iv == rel) return true;
    return false;
}

// ── Note tools (FL Studio Piano Roll parity) ─────────────────────────────────

void PianoRollView::toolStrum(bool ascending, int stepsBetweenNotes)
{
    if (selectedNotes.size() < 2) return;

    juce::Array<int> order = selectedNotes;
    std::sort(order.begin(), order.end(), [this, ascending](int a, int b)
    {
        int pa = notes.getReference(a).pitch;
        int pb = notes.getReference(b).pitch;
        return ascending ? (pa < pb) : (pa > pb);
    });

    int anchor = notes.getReference(order.getFirst()).startStep;
    for (int idx : order)
        anchor = juce::jmin(anchor, notes.getReference(idx).startStep);

    for (int i = 0; i < order.size(); ++i)
        notes.getReference(order[i]).startStep = anchor + i * juce::jmax(1, stepsBetweenNotes);

    repaint();
}

void PianoRollView::toolFlam()
{
    if (selectedNotes.isEmpty()) return;

    juce::Array<int> sorted = selectedNotes;
    sorted.sort();

    juce::Array<NoteBlock> graceNotes;
    for (int idx : sorted)
    {
        if (! juce::isPositiveAndBelow(idx, notes.size())) continue;
        const auto& n = notes.getReference(idx);
        if (n.startStep > 0)
            graceNotes.add({ n.pitch, n.startStep - 1, 1, juce::jmax(0.1f, n.velocity * 0.6f) });
    }

    for (auto& gn : graceNotes)
    {
        notes.add(gn);
        selectedNotes.addIfNotAlreadyThere(notes.size() - 1);
    }
    repaint();
}

void PianoRollView::toolChop(int numSlices)
{
    if (selectedNotes.isEmpty() || numSlices < 2) return;

    juce::Array<int> sorted = selectedNotes;
    sorted.sort();

    juce::Array<NoteBlock> originals;
    for (int idx : sorted)
        if (juce::isPositiveAndBelow(idx, notes.size()))
            originals.add(notes.getReference(idx));

    for (int i = sorted.size() - 1; i >= 0; --i)
        notes.remove(sorted[i]);

    selectedNotes.clearQuick();

    for (auto& orig : originals)
    {
        int sliceLen = juce::jmax(1, orig.lengthSteps / numSlices);
        int n = juce::jmax(1, orig.lengthSteps / sliceLen);
        for (int s = 0; s < n; ++s)
        {
            notes.add({ orig.pitch, orig.startStep + s * sliceLen, sliceLen, orig.velocity });
            selectedNotes.add(notes.size() - 1);
        }
    }
    repaint();
}

void PianoRollView::toolArpeggiate(int pattern)
{
    if (selectedNotes.size() < 2) return;

    juce::Array<int> order = selectedNotes;
    std::sort(order.begin(), order.end(), [this](int a, int b)
    {
        return notes.getReference(a).pitch < notes.getReference(b).pitch;
    });

    if (pattern == 1) // down
    {
        std::reverse(order.begin(), order.end());
    }
    else if (pattern == 2) // up-down — replay the descending half as new note copies
    {
        juce::Array<int> seq = order;
        for (int i = (int)order.size() - 2; i > 0; --i)
        {
            auto copy = notes.getReference(order[i]);
            notes.add(copy);
            seq.add(notes.size() - 1);
        }
        order = seq;
    }
    else if (pattern == 3) // down-up — replay the ascending half as new note copies
    {
        std::reverse(order.begin(), order.end());
        juce::Array<int> seq = order;
        for (int i = (int)order.size() - 2; i > 0; --i)
        {
            auto copy = notes.getReference(order[i]);
            notes.add(copy);
            seq.add(notes.size() - 1);
        }
        order = seq;
    }
    else if (pattern == 4) // random
    {
        juce::Random rng;
        for (int i = order.size() - 1; i > 0; --i)
        {
            int j = rng.nextInt(i + 1);
            order.swap(i, j);
        }
    }

    int anchor = notes.getReference(order.getFirst()).startStep;
    for (int idx : selectedNotes)
        anchor = juce::jmin(anchor, notes.getReference(idx).startStep);

    const int spacing = juce::jmax(1, snapDivisorSteps());
    for (int i = 0; i < order.size(); ++i)
    {
        auto& n = notes.getReference(order[i]);
        n.startStep   = anchor + i * spacing;
        n.lengthSteps = juce::jmin(n.lengthSteps, spacing);
    }

    selectedNotes = order;
    repaint();
}

void PianoRollView::toolChordStamp(int chordShape)
{
    if (selectedNotes.isEmpty()) return;

    // Semitone offsets stacked above each selected note's root pitch.
    static const int shapes[][3] = {
        { 0, 4, 7 },   // major
        { 0, 3, 7 },   // minor
        { 0, 3, 6 },   // diminished
        { 0, 4, 8 },   // augmented
        { 0, 4, 7 },   // maj7 (extra handled below)
        { 0, 3, 7 },   // min7
        { 0, 4, 7 },   // dom7
        { 0, 2, 7 },   // sus2
        { 0, 5, 7 },   // sus4
    };
    static const int extraTone[] = { -1, -1, -1, -1, 11, 10, 10, -1, -1 }; // 7th interval, -1 = none

    if (! juce::isPositiveAndBelow(chordShape, (int) (sizeof(shapes) / sizeof(shapes[0]))))
        return;

    juce::Array<int> roots = selectedNotes;
    roots.sort();

    juce::Array<int> newSelection;
    for (int idx : roots)
    {
        if (! juce::isPositiveAndBelow(idx, notes.size())) continue;
        const auto root = notes.getReference(idx);
        newSelection.add(idx);

        for (int t = 1; t < 3; ++t)
        {
            int p = root.pitch + shapes[chordShape][t];
            if (p >= 0 && p <= 127)
            {
                notes.add({ p, root.startStep, root.lengthSteps, root.velocity });
                newSelection.add(notes.size() - 1);
            }
        }

        if (extraTone[chordShape] >= 0)
        {
            int p = root.pitch + extraTone[chordShape];
            if (p >= 0 && p <= 127)
            {
                notes.add({ p, root.startStep, root.lengthSteps, root.velocity });
                newSelection.add(notes.size() - 1);
            }
        }
    }

    selectedNotes = newSelection;
    repaint();
}

void PianoRollView::toolRandomizeVelocity(float amount)
{
    if (selectedNotes.isEmpty()) return;
    juce::Random rng;
    const float jitter = juce::jlimit(0.0f, 1.0f, amount);

    for (int idx : selectedNotes)
        if (juce::isPositiveAndBelow(idx, notes.size()))
        {
            auto& n = notes.getReference(idx);
            const float delta = (rng.nextFloat() * 2.0f - 1.0f) * jitter;
            n.velocity = juce::jlimit(0.05f, 1.0f, n.velocity + delta);
        }
    repaint();
}

void PianoRollView::toolScaleVelocity(float factor)
{
    if (selectedNotes.isEmpty()) return;

    for (int idx : selectedNotes)
        if (juce::isPositiveAndBelow(idx, notes.size()))
        {
            auto& n = notes.getReference(idx);
            n.velocity = juce::jlimit(0.05f, 1.0f, n.velocity * factor);
        }
    repaint();
}

void PianoRollView::toolHumanizeTiming(int maxStepJitter)
{
    if (selectedNotes.isEmpty() || maxStepJitter <= 0) return;
    juce::Random rng;

    for (int idx : selectedNotes)
        if (juce::isPositiveAndBelow(idx, notes.size()))
        {
            auto& n = notes.getReference(idx);
            const int jitter = rng.nextInt(juce::Range<int>(-maxStepJitter, maxStepJitter + 1));
            n.startStep = juce::jmax(0, n.startStep + jitter);
        }
    repaint();
}

void PianoRollView::toolQuantizeSelection()
{
    if (selectedNotes.isEmpty()) return;
    int divisor = snapDivisorSteps();
    if (divisor <= 1) return;

    for (int idx : selectedNotes)
        if (juce::isPositiveAndBelow(idx, notes.size()))
        {
            auto& n = notes.getReference(idx);
            n.startStep = (n.startStep / divisor) * divisor;
        }
    repaint();
}

// ── Context menus ────────────────────────────────────────────────────────────

void PianoRollView::showNoteContextMenu(int noteIndex)
{
    juce::ignoreUnused(noteIndex);

    juce::PopupMenu strumSub;
    strumSub.addItem(1, "Strum up (low to high)");
    strumSub.addItem(2, "Strum down (high to low)");

    juce::PopupMenu chopSub;
    chopSub.addItem(10, "Chop into 2");
    chopSub.addItem(11, "Chop into 3");
    chopSub.addItem(12, "Chop into 4");

    juce::PopupMenu arpSub;
    arpSub.addItem(20, "Arpeggiate up");
    arpSub.addItem(21, "Arpeggiate down");
    arpSub.addItem(22, "Arpeggiate up-down");
    arpSub.addItem(23, "Arpeggiate down-up");
    arpSub.addItem(24, "Arpeggiate random");

    juce::PopupMenu chordSub;
    chordSub.addItem(40, "Major");
    chordSub.addItem(41, "Minor");
    chordSub.addItem(42, "Diminished");
    chordSub.addItem(43, "Augmented");
    chordSub.addItem(44, "Major 7th");
    chordSub.addItem(45, "Minor 7th");
    chordSub.addItem(46, "Dominant 7th");
    chordSub.addItem(47, "Sus2");
    chordSub.addItem(48, "Sus4");

    const bool haveChord = selectedNotes.size() >= 2;
    const bool haveAny   = ! selectedNotes.isEmpty();

    juce::PopupMenu velSub;
    velSub.addItem(60, "Randomize (subtle)");
    velSub.addItem(61, "Randomize (wide)");
    velSub.addItem(62, "Scale up (x1.2)");
    velSub.addItem(63, "Scale down (x0.8)");

    juce::PopupMenu humanizeSub;
    humanizeSub.addItem(70, "Humanize timing (subtle)");
    humanizeSub.addItem(71, "Humanize timing (loose)");

    juce::PopupMenu m;
    m.addSubMenu("Strum", strumSub, haveChord);
    m.addItem(102, "Flam");
    m.addSubMenu("Chop", chopSub);
    m.addSubMenu("Arpeggiate", arpSub, haveChord);
    m.addSubMenu("Chord stamp", chordSub, haveAny);
    m.addItem(30, "Quantize to grid");
    m.addSubMenu("Velocity", velSub, haveAny);
    m.addSubMenu("Humanize", humanizeSub, haveAny);
    m.addSeparator();
    m.addItem(99, "Delete");

    m.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        switch (result)
        {
            case 1:   toolStrum(true);  break;
            case 2:   toolStrum(false); break;
            case 102: toolFlam();       break;
            case 10:  toolChop(2);      break;
            case 11:  toolChop(3);      break;
            case 12:  toolChop(4);      break;
            case 20:  toolArpeggiate(0); break;
            case 21:  toolArpeggiate(1); break;
            case 22:  toolArpeggiate(2); break;
            case 23:  toolArpeggiate(3); break;
            case 24:  toolArpeggiate(4); break;
            case 40:  toolChordStamp(0); break;
            case 41:  toolChordStamp(1); break;
            case 42:  toolChordStamp(2); break;
            case 43:  toolChordStamp(3); break;
            case 44:  toolChordStamp(4); break;
            case 45:  toolChordStamp(5); break;
            case 46:  toolChordStamp(6); break;
            case 47:  toolChordStamp(7); break;
            case 48:  toolChordStamp(8); break;
            case 30:  toolQuantizeSelection(); break;
            case 60:  toolRandomizeVelocity(0.12f); break;
            case 61:  toolRandomizeVelocity(0.35f); break;
            case 62:  toolScaleVelocity(1.2f);  break;
            case 63:  toolScaleVelocity(0.8f);  break;
            case 70:  toolHumanizeTiming(1); break;
            case 71:  toolHumanizeTiming(3); break;
            case 99:
            {
                juce::Array<int> sorted = selectedNotes;
                sorted.sort();
                for (int i = sorted.size() - 1; i >= 0; --i)
                    if (juce::isPositiveAndBelow(sorted[i], notes.size()))
                        notes.remove(sorted[i]);
                selectedNotes.clear();
                repaint();
                break;
            }
            default: break;
        }
    });
}

void PianoRollView::showGridContextMenu()
{
    juce::PopupMenu m;
    m.addItem(1, "Select all");
    m.addItem(2, "Clear selection");
    m.addSeparator();
    m.addItem(3, "Set scale/key...");
    m.addItem(4, "Clear scale highlight", scaleRoot >= 0);

    m.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        switch (result)
        {
            case 1:
                selectedNotes.clearQuick();
                for (int i = 0; i < notes.size(); ++i)
                    selectedNotes.add(i);
                repaint();
                break;
            case 2: selectedNotes.clear(); repaint(); break;
            case 3: showScalePickerMenu(); break;
            case 4: setScale(-1, scaleType); break;
            default: break;
        }
    });
}

void PianoRollView::showScalePickerMenu()
{
    static const char* rootNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    juce::PopupMenu rootSub;
    for (int i = 0; i < 12; ++i)
        rootSub.addItem(100 + i, rootNames[i], true, scaleRoot == i);

    juce::PopupMenu typeSub;
    for (int t = 0; t < kNumScaleTypes; ++t)
        typeSub.addItem(200 + t, scaleTypeName(t), true, scaleType == t);

    juce::PopupMenu m;
    m.addSubMenu("Root note", rootSub);
    m.addSubMenu("Scale type", typeSub);
    m.addSeparator();
    m.addItem(1, "Off (chromatic guide)");

    m.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        if (result == 1)
            setScale(-1, scaleType);
        else if (result >= 100 && result < 112)
            setScale(result - 100, scaleType);
        else if (result >= 200 && result < 200 + kNumScaleTypes)
            setScale(scaleRoot < 0 ? 0 : scaleRoot, result - 200);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// FloatingSubPanel
// ─────────────────────────────────────────────────────────────────────────────

FloatingSubPanel::FloatingSubPanel(const juce::String& title)
    : titleText(title)
{
    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
    closeBtn.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn);

    resizer.reset(new juce::ResizableCornerComponent(this, &constrainer));
    constrainer.setMinimumSize(220, 160);
    addAndMakeVisible(*resizer);

    setOpaque(false);
}

FloatingSubPanel::~FloatingSubPanel() = default;

void FloatingSubPanel::setContent(juce::Component* c)
{
    content = c;
    if (content != nullptr)
        addAndMakeVisible(*content);
    resized();
}

void FloatingSubPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Drop shadow / border so the floating panel reads as separate from the canvas behind it
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(area.translated(3, 3));

    g.setColour(BeatTheme::panel());
    g.fillRect(area);

    auto titleBar = area.removeFromTop(kTitleBarH);
    g.setColour(BeatTheme::panel().brighter(0.12f));
    g.fillRect(titleBar);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.drawText(titleText, titleBar.reduced(8, 0), juce::Justification::centredLeft);

    g.setColour(BeatTheme::edge());
    g.drawRect(getLocalBounds(), 1);
}

void FloatingSubPanel::resized()
{
    auto area = getLocalBounds();
    auto titleBar = area.removeFromTop(kTitleBarH);
    closeBtn.setBounds(titleBar.removeFromRight(kTitleBarH).reduced(4));

    if (content != nullptr)
        content->setBounds(area);

    resizer->setBounds(getWidth() - 14, getHeight() - 14, 14, 14);
}

void FloatingSubPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y < kTitleBarH)
    {
        dragger.startDraggingComponent(this, e);
        toFront(true);
    }
}

void FloatingSubPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getMouseDownPosition().y < kTitleBarH)
        dragger.dragComponent(this, e, &constrainer);
}

// ─────────────────────────────────────────────────────────────────────────────
// CompactMixerView — full-featured in-window mixer matching the main Mix window
// ─────────────────────────────────────────────────────────────────────────────

CompactMixerView::CompactMixerView() {}
CompactMixerView::~CompactMixerView() = default;

float CompactMixerView::posToDb(float pos) noexcept
{
    if (pos < 0.35f) return juce::jmap(pos, 0.0f, 0.35f, -96.0f, 0.0f);
    return juce::jmap(pos, 0.35f, 1.0f, 0.0f, 6.0f);
}

float CompactMixerView::dbToPos(float db) noexcept
{
    if (db <= -90.0f) return 0.0f;
    if (db <=   0.0f) return juce::jmap(db, -96.0f, 0.0f, 0.0f, 0.35f);
    return juce::jmap(db, 0.0f, 6.0f, 0.35f, 1.0f);
}

void CompactMixerView::setNumTracks(int numTracks)
{
    strips.clear();
    for (int i = 0; i < numTracks; ++i)
    {
        auto* s = strips.add(new Strip());
        s->name = "Trk " + juce::String(i + 1);
    }
    repaint();
}

void CompactMixerView::setTrackInfo(int index, const juce::String& name, juce::Colour colour,
                                    float volumeDb, float pan, bool muted)
{
    if (! juce::isPositiveAndBelow(index, strips.size()))
        return;
    auto* s     = strips.getUnchecked(index);
    s->name     = name;
    s->colour   = colour;
    s->faderPos = dbToPos(volumeDb);
    s->panPos   = juce::jlimit(0.0f, 1.0f, (pan + 1.0f) * 0.5f);
    s->muted    = muted;
    repaint();
}

void CompactMixerView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(11, 12, 18));

    // Header bar
    g.setColour(juce::Colour::fromRGB(17, 19, 28));
    g.fillRect(0, 0, getWidth(), kHeaderH);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    g.drawText("MIXER", 0, 0, getWidth(), kHeaderH, juce::Justification::centred);

    if (strips.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("No tracks — add a track to see it here.",
                   getLocalBounds().withTrimmedTop(kHeaderH), juce::Justification::centred);
        return;
    }

    const int top     = kHeaderH;
    const int H       = getHeight() - kHeaderH;
    const int fTop    = top + 44;
    const int fBot    = top + H - 34;
    const int fTravel = juce::jmax(1, fBot - fTop);

    static const char* const dbMarks[] = { "+6", "0", "-6", "-12", "-24", "-inf" };

    for (int i = 0; i < strips.size(); ++i)
    {
        const auto* s  = strips.getUnchecked(i);
        const int sx   = i * kStripW;
        const int sw   = kStripW - 2;
        const juce::Colour col = s->colour;

        // Strip background
        g.setColour(juce::Colour::fromRGB(11, 12, 18));
        g.fillRect(sx, top, sw, H);

        // Colour band at top
        g.setColour(col);
        g.fillRect(sx, top, sw, 4);

        // Track name
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
        g.drawText(s->name, sx + 2, top + 6, sw - 4, 12, juce::Justification::centred);

        // ── Pan knob ──────────────────────────────────────────────────────────
        const float knobCX  = (float)sx + sw * 0.5f;
        const float knobCY  = (float)top + 28.0f;
        const float kr      = 9.0f;
        const float panVal  = s->panPos;

        g.setColour(juce::Colour::fromRGB(8, 9, 14));
        g.fillEllipse(knobCX - kr, knobCY - kr, kr * 2.0f, kr * 2.0f);
        g.setColour(juce::Colour::fromRGB(38, 42, 60));
        g.fillEllipse(knobCX - kr + 1.5f, knobCY - kr + 1.5f, (kr - 1.5f) * 2.0f, (kr - 1.5f) * 2.0f);

        const float offCentre = std::abs(panVal - 0.5f) * 2.0f;
        g.setColour(col.withAlpha(0.35f + offCentre * 0.5f));
        g.drawEllipse(knobCX - kr, knobCY - kr, kr * 2.0f, kr * 2.0f, 1.5f);

        {
            const float sa = -juce::MathConstants<float>::pi * 0.75f;
            const float ea =  juce::MathConstants<float>::pi * 0.75f;
            const float pa = sa + panVal * (ea - sa);
            juce::Path arc;
            arc.addArc(knobCX - kr + 1.0f, knobCY - kr + 1.0f,
                       (kr - 1.0f) * 2.0f, (kr - 1.0f) * 2.0f,
                       juce::jmin(0.0f, pa), juce::jmax(0.0f, pa), true);
            g.setColour(col.withAlpha(panVal != 0.5f ? 0.8f : 0.3f));
            g.strokePath(arc, juce::PathStrokeType(2.0f));
        }
        {
            const float sa = -juce::MathConstants<float>::pi * 0.75f;
            const float ea =  juce::MathConstants<float>::pi * 0.75f;
            const float pa = sa + panVal * (ea - sa);
            const float px = knobCX + (kr - 3.0f) * std::sin(pa);
            const float py = knobCY - (kr - 3.0f) * std::cos(pa);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawLine(knobCX, knobCY, px, py, 1.8f);
        }

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        juce::String panStr;
        if (std::abs(panVal - 0.5f) < 0.01f) panStr = "C";
        else if (panVal < 0.5f) panStr = "L" + juce::String((int)((0.5f - panVal) * 200.0f));
        else                    panStr = "R" + juce::String((int)((panVal - 0.5f) * 200.0f));
        g.drawText(panStr, sx + 2, (int)(knobCY + kr + 1.0f), sw - 4, 9, juce::Justification::centred);

        // ── Fader + level meter ───────────────────────────────────────────────
        const int meterColW = 9;
        const int meterColX = sx + sw - meterColW - 1;
        const int fCX       = sx + (meterColX - sx) / 2;

        // Fader groove
        g.setColour(juce::Colour::fromRGB(6, 7, 11));
        g.fillRoundedRectangle((float)(fCX - 3), (float)fTop, 6.0f, (float)fTravel, 3.0f);
        g.setColour(juce::Colour::fromRGB(30, 34, 48));
        g.drawRoundedRectangle((float)(fCX - 3), (float)fTop, 6.0f, (float)fTravel, 3.0f, 0.8f);

        // Unity (0 dB) tick mark at 35% position
        const int unityY = fTop + (int)(0.35f * fTravel);
        g.setColour(col.withAlpha(0.45f));
        g.fillRect(fCX - 8, unityY - 1, 16, 2);

        // dB scale ticks on the left
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.setFont(juce::Font(juce::FontOptions(6.0f)));
        for (int m = 0; m < 6; ++m)
        {
            const int tickY = fTop + m * fTravel / 5;
            g.drawLine((float)(sx + 4), (float)tickY, (float)(fCX - 5), (float)tickY, 0.7f);
            g.drawText(dbMarks[m], sx + 2, tickY - 4, 18, 8, juce::Justification::right);
        }

        // Segmented level meter
        g.setColour(juce::Colour::fromRGB(8, 10, 16));
        g.fillRoundedRectangle((float)meterColX, (float)fTop, (float)meterColW, (float)fTravel, 2.0f);
        if (s->peakLevel > 0.001f)
        {
            const int fillH = (int)(fTravel * juce::jlimit(0.0f, 1.0f, s->peakLevel));
            const int segH = 3, segGap = 1;
            for (int fy = fBot - segH; fy >= fBot - fillH; fy -= (segH + segGap))
            {
                const float t = 1.0f - (float)(fy - fTop) / (float)fTravel;
                juce::Colour segCol;
                if (t < 0.70f)      segCol = juce::Colour::fromRGB(50, 200,  80);
                else if (t < 0.88f) segCol = juce::Colour::fromRGB(220, 200,  30);
                else                segCol = juce::Colour::fromRGB(220,  50,  50);
                g.setColour(segCol);
                g.fillRect(meterColX + 1, fy, meterColW - 2, segH);
            }
        }

        // ── Fader thumb ───────────────────────────────────────────────────────
        const int handleY = fTop + (int)(s->faderPos * fTravel);
        const int thumbW  = meterColX - sx - 10;
        const int thumbX  = sx + 6;

        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle((float)thumbX + 1.0f, (float)(handleY - kThumbH/2) + 2.0f,
                               (float)thumbW, (float)kThumbH, 4.0f);

        juce::ColourGradient thumbGrad(juce::Colour::fromRGB(72, 78, 110),
                                       (float)thumbX, (float)(handleY - kThumbH/2),
                                       juce::Colour::fromRGB(44, 48, 70),
                                       (float)thumbX, (float)(handleY + kThumbH/2), false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle((float)thumbX, (float)(handleY - kThumbH/2),
                               (float)thumbW, (float)kThumbH, 4.0f);

        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.fillRect(thumbX + 4, handleY - 1, thumbW - 8, 2);

        g.setColour(col.withAlpha(0.35f));
        g.drawRoundedRectangle((float)thumbX, (float)(handleY - kThumbH/2),
                               (float)thumbW, (float)kThumbH, 4.0f, 1.0f);

        // dB readout below fader
        const juce::String dbStr = (s->faderPos <= 0.0f)
                                   ? "-inf"
                                   : (juce::String(posToDb(s->faderPos), 1) + " dB");
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.drawText(dbStr, sx + 2, fBot + 2, sw - 4, 12, juce::Justification::centred);

        // ── M / S / R buttons ─────────────────────────────────────────────────
        const int btnY  = top + H - 18;
        const int btnH2 = 14;
        const int btnW2 = (sw - 10) / 3;

        auto drawBtn = [&](const char* lbl, int bx, juce::Colour bc)
        {
            g.setColour(bc);
            g.fillRoundedRectangle((float)bx, (float)btnY, (float)btnW2, (float)btnH2, 2.5f);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.drawText(lbl, bx, btnY, btnW2, btnH2, juce::Justification::centred);
        };

        drawBtn("M", sx + 3,
                s->muted  ? juce::Colour::fromRGB(180, 50, 50) : juce::Colour::fromRGB(100, 30, 30));
        drawBtn("S", sx + 3 + btnW2 + 2,
                s->soloed ? juce::Colour::fromRGB(30, 170, 30) : juce::Colour::fromRGB(30, 90, 30));
        drawBtn("R", sx + 3 + (btnW2 + 2) * 2,
                juce::Colour::fromRGB(80, 30, 80));

        // Strip separator
        g.setColour(juce::Colour::fromRGB(6, 7, 11));
        g.fillRect(sx + sw, top, 2, H);
    }
}

void CompactMixerView::resized() {}

void CompactMixerView::mouseDown(const juce::MouseEvent& e)
{
    const int top     = kHeaderH;
    const int H       = getHeight() - kHeaderH;
    const int fTop    = top + 44;
    const int fBot    = top + H - 34;
    const int fTravel = juce::jmax(1, fBot - fTop);
    const int btnY    = top + H - 18;
    const int btnH2   = 14;

    const int si = e.x / kStripW;
    if (! juce::isPositiveAndBelow(si, strips.size())) return;

    auto* s        = strips.getUnchecked(si);
    const int sx   = si * kStripW;
    const int sw   = kStripW - 2;
    const int btnW2 = (sw - 10) / 3;

    // Pan knob hit-test
    const float knobCX = (float)sx + sw * 0.5f;
    const float knobCY = (float)top + 28.0f;
    const float kr     = 9.0f;
    const float dx     = (float)e.x - knobCX;
    const float dy     = (float)e.y - knobCY;
    if (dx * dx + dy * dy <= kr * kr * 2.25f)
    {
        dragKind     = DragKind::Pan;
        dragIdx      = si;
        dragStartX   = e.x;
        dragStartVal = s->panPos;
        return;
    }

    // Fader thumb hit-test
    const int meterColX = sx + sw - 10;
    const int thumbW    = meterColX - sx - 10;
    const int thumbX    = sx + 6;
    const int handleY   = fTop + (int)(s->faderPos * fTravel);
    juce::Rectangle<int> thumbRect(thumbX, handleY - kThumbH / 2, thumbW, kThumbH);
    if (thumbRect.contains(e.getPosition()))
    {
        dragKind     = DragKind::Fader;
        dragIdx      = si;
        dragStartY   = e.y;
        dragStartVal = s->faderPos;
        return;
    }

    // Fader groove click — jump to position
    juce::Rectangle<int> grooveRect(sx + 2, fTop, sw - 4, fTravel);
    if (grooveRect.contains(e.getPosition()))
    {
        s->faderPos = juce::jlimit(0.0f, 1.0f, (float)(e.y - fTop) / (float)fTravel);
        if (onVolumeChanged) onVolumeChanged(si, posToDb(s->faderPos));
        dragKind     = DragKind::Fader;
        dragIdx      = si;
        dragStartY   = e.y;
        dragStartVal = s->faderPos;
        repaint();
        return;
    }

    // M button
    juce::Rectangle<int> muteRect(sx + 3, btnY, btnW2, btnH2);
    if (muteRect.contains(e.getPosition()))
    {
        s->muted = !s->muted;
        if (onMuteToggled) onMuteToggled(si, s->muted);
        repaint();
        return;
    }

    // S button
    juce::Rectangle<int> soloRect(sx + 3 + btnW2 + 2, btnY, btnW2, btnH2);
    if (soloRect.contains(e.getPosition()))
    {
        s->soloed = !s->soloed;
        repaint();
    }
}

void CompactMixerView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragKind == DragKind::None || ! juce::isPositiveAndBelow(dragIdx, strips.size()))
        return;

    auto* s = strips.getUnchecked(dragIdx);

    if (dragKind == DragKind::Fader)
    {
        const int H       = getHeight() - kHeaderH;
        const int fTop    = kHeaderH + 44;
        const int fBot    = kHeaderH + H - 34;
        const int fTravel = juce::jmax(1, fBot - fTop);
        const float delta = (float)(e.y - dragStartY) / (float)fTravel;
        s->faderPos = juce::jlimit(0.0f, 1.0f, dragStartVal + delta);
        if (onVolumeChanged) onVolumeChanged(dragIdx, posToDb(s->faderPos));
        repaint();
    }
    else if (dragKind == DragKind::Pan)
    {
        const float delta = (float)(e.x - dragStartX) / 120.0f;
        s->panPos = juce::jlimit(0.0f, 1.0f, dragStartVal + delta);
        if (onPanChanged) onPanChanged(dragIdx, (s->panPos - 0.5f) * 2.0f);
        repaint();
    }
}

void CompactMixerView::mouseUp(const juce::MouseEvent&)
{
    dragKind = DragKind::None;
    dragIdx  = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// PatternLaunchGrid
// ─────────────────────────────────────────────────────────────────────────────
PatternLaunchGrid::PatternLaunchGrid()  { setInterceptsMouseClicks(true, false); }
PatternLaunchGrid::~PatternLaunchGrid() = default;

void PatternLaunchGrid::setPatterns(const juce::StringArray& names, const juce::Array<juce::Colour>& colours)
{
    pads.clear();
    for (int i = 0; i < names.size(); ++i)
    {
        Pad pad;
        pad.name   = names[i];
        pad.colour = colours[i];
        pads.add(pad);
    }
    resized();
    repaint();
}

void PatternLaunchGrid::setQueuedIndex(int index)
{
    if (queuedIndex != index) { queuedIndex = index; repaint(); }
}

void PatternLaunchGrid::setPlayingIndex(int index)
{
    if (playingIndex != index) { playingIndex = index; repaint(); }
}

void PatternLaunchGrid::resized()
{
    auto area = getLocalBounds().reduced(10);
    if (pads.isEmpty())
        return;

    const int cols = juce::jmax(1, (int) std::ceil(std::sqrt((double) pads.size())));
    const int rows = (pads.size() + cols - 1) / cols;
    const int gap  = 8;
    const int padW = (area.getWidth()  - gap * (cols - 1)) / cols;
    const int padH = (area.getHeight() - gap * (rows - 1)) / juce::jmax(1, rows);

    for (int i = 0; i < pads.size(); ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        pads.getReference(i).bounds = juce::Rectangle<int>(area.getX() + col * (padW + gap),
                                                            area.getY() + row * (padH + gap),
                                                            padW, padH);
    }
}

void PatternLaunchGrid::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(28, 28, 34));

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(13.0f);
    g.drawFittedText("Click a pad to launch its pattern at the next loop boundary",
                     getLocalBounds().removeFromTop(10).reduced(10, 0),
                     juce::Justification::centredLeft, 1);

    for (int i = 0; i < pads.size(); ++i)
    {
        const auto& pad = pads.getReference(i);
        const bool playing = (i == playingIndex);
        const bool queued  = (i == queuedIndex);

        auto fill = pad.colour.withAlpha(playing ? 0.9f : 0.35f);
        g.setColour(fill);
        g.fillRoundedRectangle(pad.bounds.toFloat(), 6.0f);

        g.setColour(queued ? juce::Colours::white : juce::Colours::black.withAlpha(0.4f));
        g.drawRoundedRectangle(pad.bounds.toFloat().reduced(1.0f), 6.0f, queued ? 2.5f : 1.0f);

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        auto padArea = pad.bounds;
        auto labelArea = padArea.removeFromBottom(16);
        g.drawFittedText(pad.name, padArea.reduced(6), juce::Justification::centred, 2);

        if (playing)
        {
            g.setFont(11.0f);
            g.drawFittedText("PLAYING", labelArea.reduced(4, 0),
                             juce::Justification::centredRight, 1);
        }
        else if (queued)
        {
            g.setFont(11.0f);
            g.drawFittedText("QUEUED", labelArea.reduced(4, 0),
                             juce::Justification::centredRight, 1);
        }
    }
}

void PatternLaunchGrid::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < pads.size(); ++i)
    {
        if (pads.getReference(i).bounds.contains(e.getPosition()))
        {
            if (onPadClicked)
                onPadClicked(i);
            return;
        }
    }
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
    const bool dimmedByFilter = groupFilter.isNotEmpty() && ch.group != groupFilter;

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
    const float nameAlpha = (dimmedByFilter ? 0.3f : 1.0f) * (ch.muted ? 0.35f : (isSelected ? 0.95f : 0.8f));
    g.setColour(juce::Colours::white.withAlpha(nameAlpha));
    g.setFont(juce::FontOptions(11.0f));

    // If this channel belongs to a group, show a small group tag chip after the name
    if (ch.group.isNotEmpty())
    {
        auto tagR = nameR.removeFromRight(juce::jmin(54, nameR.getWidth() / 2)).reduced(2, 4);
        g.setColour(ch.colour.withAlpha(dimmedByFilter ? 0.18f : 0.35f));
        g.fillRoundedRectangle(tagR.toFloat(), 3.0f);
        g.setColour(juce::Colours::white.withAlpha(dimmedByFilter ? 0.25f : 0.65f));
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(ch.group, tagR, juce::Justification::centred, true);
        g.setFont(juce::FontOptions(11.0f));
    }

    g.drawText(ch.name, nameR.reduced(4, 0), juce::Justification::centredLeft, true);
    if (isSelected)
    {
        g.setColour(ch.colour.withAlpha(0.8f));
        g.fillRect(nameR.getX(), r.getBottom() - 2, nameR.getWidth(), 2);
    }

    if (dimmedByFilter)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(r);
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

        // Dim rows that don't match the active group filter (FL-style channel filtering)
        if (groupFilter.isNotEmpty() && ch.group != groupFilter)
        {
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(r.getX(), ry, r.getWidth(), kRowH);
        }
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
                    auto* popup = new SampleEditorPopup(ch.name, ch.samplePath, ch.volume, ch.pan, ch.pitch,
                                                         ch.mixerTrack, numMixerInserts);

                    popup->onVolumeChanged = [this, row](float v) {
                        currentPattern().channels[(size_t)row].volume = v;
                        if (onRowVolumeChanged) onRowVolumeChanged(row, v);
                    };
                    popup->onPanChanged = [this, row](float p) {
                        currentPattern().channels[(size_t)row].pan = p;
                        if (onRowPanChanged) onRowPanChanged(row, p);
                    };
                    popup->onPitchChanged = [this, row](float p) {
                        currentPattern().channels[(size_t)row].pitch = p;
                        if (onRowPitchChanged) onRowPitchChanged(row, p);
                    };
                    popup->onMixerTrackChanged = [this, row](int trackIdx) {
                        currentPattern().channels[(size_t)row].mixerTrack = trackIdx;
                        if (onRowMixerTrackChanged) onRowMixerTrackChanged(row, trackIdx);
                    };
                    popup->onEnvelopeChanged = [this, row](bool en, float a, float h, float d, float s, float r) {
                        auto& c = currentPattern().channels[(size_t)row];
                        c.envEnabled = en; c.envAttack = a; c.envHold = h; c.envDecay = d; c.envSustain = s; c.envRelease = r;
                        if (onRowEnvelopeChanged) onRowEnvelopeChanged(row, en, a, h, d, s, r);
                    };
                    popup->onFilterChanged = [this, row](bool en, int type, float cutoff, float reso) {
                        auto& c = currentPattern().channels[(size_t)row];
                        c.filterEnabled = en; c.filterType = type; c.filterCutoff = cutoff; c.filterResonance = reso;
                        if (onRowFilterChanged) onRowFilterChanged(row, en, type, cutoff, reso);
                    };
                    popup->onLFOChanged = [this, row](bool en, int target, float rate, float depth) {
                        auto& c = currentPattern().channels[(size_t)row];
                        c.lfoEnabled = en; c.lfoTarget = target; c.lfoRate = rate; c.lfoDepth = depth;
                        if (onRowLFOChanged) onRowLFOChanged(row, en, target, rate, depth);
                    };
                    popup->onSampleRegionChanged = [this, row](float startFrac, float endFrac, bool loop) {
                        auto& c = currentPattern().channels[(size_t)row];
                        c.sampleStart = startFrac; c.sampleEnd = endFrac; c.loopEnabled = loop;
                        if (onRowSampleRegionChanged) onRowSampleRegionChanged(row, startFrac, endFrac, loop);
                    };
                    popup->initEngineState(ch.envEnabled, ch.envAttack, ch.envHold, ch.envDecay, ch.envSustain, ch.envRelease,
                                           ch.filterEnabled, ch.filterType, ch.filterCutoff, ch.filterResonance,
                                           ch.lfoEnabled, ch.lfoTarget, ch.lfoRate, ch.lfoDepth,
                                           ch.sampleStart, ch.sampleEnd, ch.loopEnabled);
                    popup->onPreviewSample = [this, row]() {
                        const auto& chan = currentPattern().channels[(size_t)row];
                        if (chan.samplePath.isNotEmpty() && onPreviewSample)
                            onPreviewSample(chan.samplePath);
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
                                    currentPattern().channels[(size_t)row].samplePath = f.getFullPathName();
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
                if (e.mods.isCtrlDown() || e.mods.isCommandDown())
                {
                    // Ctrl/Cmd-click = toggle group filter for this channel's group
                    auto grp = currentPattern().channels[(size_t)row].group;
                    if (grp.isNotEmpty())
                    {
                        groupFilter = (groupFilter == grp) ? juce::String() : grp;
                        repaint();
                    }
                }
                else if (e.getNumberOfClicks() >= 2)
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
                                    currentPattern().channels[(size_t)row].samplePath = result2.getFullPathName();
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

    // Build "Set Group" submenu from groups already used in this pattern
    auto& chForMenu = currentPattern().channels[(size_t)row];
    juce::StringArray existingGroups;
    for (auto& c : currentPattern().channels)
        if (c.group.isNotEmpty() && ! existingGroups.contains(c.group))
            existingGroups.add(c.group);
    static constexpr int kGroupBaseId = 100;
    juce::PopupMenu groupSub;
    for (int i = 0; i < existingGroups.size(); ++i)
        groupSub.addItem(kGroupBaseId + i, existingGroups[i], true, chForMenu.group == existingGroups[i]);
    if (existingGroups.size() > 0)
        groupSub.addSeparator();
    groupSub.addItem(198, "No group (clear)");
    groupSub.addItem(199, "New group...");

    juce::PopupMenu m;
    m.addItem(1,  "Load sample...");
    m.addItem(44, "Replace sample...");
    m.addSeparator();
    m.addItem(2,  "Rename");
    m.addItem(3,  "Change color");
    m.addItem(4,  "Random color");
    m.addSubMenu("Set Group", groupSub);
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
    m.addItem(43, "Clone channel (full)");
    m.addItem(42, "Delete row");

    // Capture steps for copy/paste
    static bool stepClipboard[kMaxSteps] {};
    static int  stepClipboardCount = 0;

    m.showMenuAsync(juce::PopupMenu::Options(), [this, row, existingGroups](int result)
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
                                    currentPattern().channels[(size_t)row].samplePath = result2.getFullPathName();
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
            case 43: // Clone channel (full duplicate incl. sample/settings) into next row
                if (row + 1 < kNumRows)
                {
                    auto& dst = pat.channels[(size_t)(row + 1)];
                    dst = ch;
                    dst.name = ch.name + " Copy";
                    if (onSampleAssigned)     onSampleAssigned(row + 1, dst.samplePath);
                    if (onRowVolumeChanged)   onRowVolumeChanged(row + 1, dst.volume);
                    if (onRowPanChanged)      onRowPanChanged(row + 1, dst.pan);
                    if (onRowPitchChanged)    onRowPitchChanged(row + 1, dst.pitch);
                    if (onRowMutedChanged)    onRowMutedChanged(row + 1, dst.muted);
                    if (onRowMixerTrackChanged) onRowMixerTrackChanged(row + 1, dst.mixerTrack);
                    for (int s = 0; s < sc; ++s) if (onStepChanged) onStepChanged(row + 1, s, dst.steps[s]);
                    repaint();
                }
                break;
            case 44: // Replace sample — swap audio file, keep volume/pan/pitch/group/steps
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Replace Sample for " + ch.name,
                    juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                    "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
                chooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, row, chooser](const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (f.existsAsFile())
                        {
                            currentPattern().channels[(size_t)row].samplePath = f.getFullPathName();
                            if (onSampleAssigned) onSampleAssigned(row, f.getFullPathName());
                            repaint();
                        }
                    });
                break;
            }
            case 198: // Clear group
                ch.group.clear();
                repaint(); break;
            case 199: // New group...
            {
                auto* aw = new juce::AlertWindow("New Group", "Enter a group name for \"" + ch.name + "\":",
                                                  juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor("groupName", "");
                aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                aw->enterModalState(true, juce::ModalCallbackFunction::create([this, row, aw](int okPressed)
                {
                    if (okPressed == 1)
                    {
                        auto name = aw->getTextEditorContents("groupName").trim();
                        if (name.isNotEmpty())
                        {
                            currentPattern().channels[(size_t)row].group = name;
                            repaint();
                        }
                    }
                    delete aw;
                }), false);
                break;
            }
            default:
                if (result >= kGroupBaseId && result < kGroupBaseId + existingGroups.size())
                {
                    ch.group = existingGroups[result - kGroupBaseId];
                    repaint();
                }
                break;
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
        currentPattern().channels[(size_t)row].samplePath = path;
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
                                     float volume, float pan, float pitch,
                                     int mixerTrack, int numMixerTracks)
    : name(channelName), path(filePath), vol(volume), panVal(pan), pitchVal(pitch),
      trackRouting(mixerTrack), numTracks(numMixerTracks)
{
    setSize(340, 360);

    // ── Tab bar (FL Studio style: Sample / Envelope / Functions / Engine) ────
    sampleTabBtn.toggled = true;
    for (auto* b : { &sampleTabBtn, &envelopeTabBtn, &functionsTabBtn, &engineTabBtn })
        addAndMakeVisible(b);
    sampleTabBtn.onClick    = [this]() { showTab(kTabSample); };
    envelopeTabBtn.onClick  = [this]() { showTab(kTabEnvelope); };
    functionsTabBtn.onClick = [this]() { showTab(kTabFunctions); };
    engineTabBtn.onClick    = [this]() { showTab(kTabEngine); };

    // ── Preview / audition button ─────────────────────────────────────────────
    addAndMakeVisible(previewBtn);
    previewBtn.onClick = [this]() { if (onPreviewSample) onPreviewSample(); };

    // ── Tab 0: Sample ─────────────────────────────────────────────────────────
    fileLabel.setText(path.isEmpty() ? "No sample loaded" : juce::File(path).getFileName(),
                      juce::dontSendNotification);
    fileLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    fileLabel.setFont(juce::FontOptions(10.0f));
    fileLabel.setJustificationType(juce::Justification::centred);
    addChildComponent(fileLabel);

    loadBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour::fromRGB(28, 32, 50));
    loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    loadBtn.onClick = [this]() { if (onLoadSample) onLoadSample(); };
    addChildComponent(loadBtn);

    // ── Tab 1: Envelope — rotary knobs ────────────────────────────────────────
    styleKnob(volumeKnob, 0.0, 2.0, volume);
    volumeKnob.onValueChange = [this]() { if (onVolumeChanged) onVolumeChanged((float)volumeKnob.getValue()); };
    addChildComponent(volumeKnob);

    styleKnob(panKnob, -1.0, 1.0, pan);
    panKnob.onValueChange = [this]() { if (onPanChanged) onPanChanged((float)panKnob.getValue()); };
    addChildComponent(panKnob);

    styleKnob(pitchKnob, -24.0, 24.0, pitch);
    pitchKnob.onValueChange = [this]() { if (onPitchChanged) onPitchChanged((float)pitchKnob.getValue()); };
    addChildComponent(pitchKnob);

    auto styleLabel = [](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        l.setFont(juce::FontOptions(11.0f));
        l.setJustificationType(juce::Justification::centred);
    };
    styleLabel(volLabel,   "VOLUME");
    styleLabel(panLabel,   "PAN");
    styleLabel(pitchLabel, "PITCH (st)");
    addChildComponent(volLabel);
    addChildComponent(panLabel);
    addChildComponent(pitchLabel);

    // ── Tab 2: Functions — mixer routing ──────────────────────────────────────
    routingLabel.setText("Mixer routing", juce::dontSendNotification);
    routingLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    routingLabel.setFont(juce::FontOptions(11.0f));
    addChildComponent(routingLabel);

    routingBox.addItem("Master", 1);
    for (int t = 1; t <= numTracks; ++t)
        routingBox.addItem("Insert " + juce::String(t), t + 1);
    routingBox.setSelectedId(trackRouting + 1, juce::dontSendNotification);
    routingBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(20, 23, 34));
    routingBox.setColour(juce::ComboBox::textColourId,       juce::Colours::white.withAlpha(0.85f));
    routingBox.setColour(juce::ComboBox::outlineColourId,    juce::Colour::fromRGBA(255,255,255,24));
    routingBox.onChange = [this]() {
        trackRouting = routingBox.getSelectedId() - 1;
        if (onMixerTrackChanged) onMixerTrackChanged(trackRouting);
    };
    addChildComponent(routingBox);

    // ── Tab 3: Engine — AHDSR / Filter / LFO / sample region ──────────────────
    auto styleToggle = [](juce::ToggleButton& t)
    {
        t.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.75f));
        t.setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(80, 100, 255));
    };
    auto styleSmallLabel = [](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
        l.setFont(juce::FontOptions(9.0f));
        l.setJustificationType(juce::Justification::centred);
    };

    // Envelope row
    styleToggle(envEnableToggle);
    envEnableToggle.onClick = [this]() { fireEnvelopeChanged(); };
    addChildComponent(envEnableToggle);
    styleSmallKnob(attackKnob,  0.0, 4.0, 0.001);
    styleSmallKnob(holdKnob,    0.0, 2.0, 0.0);
    styleSmallKnob(decayKnob,   0.0, 4.0, 0.1);
    styleSmallKnob(sustainKnob, 0.0, 1.0, 1.0);
    styleSmallKnob(releaseKnob, 0.0, 4.0, 0.05);
    for (auto* k : { &attackKnob, &holdKnob, &decayKnob, &sustainKnob, &releaseKnob })
    {
        k->onValueChange = [this]() { fireEnvelopeChanged(); };
        addChildComponent(k);
    }
    styleSmallLabel(attackLabel,  "ATK");
    styleSmallLabel(holdLabel,    "HOLD");
    styleSmallLabel(decayLabel,   "DEC");
    styleSmallLabel(sustainLabel, "SUS");
    styleSmallLabel(releaseLabel, "REL");
    for (auto* l : { &attackLabel, &holdLabel, &decayLabel, &sustainLabel, &releaseLabel })
        addChildComponent(l);

    // Filter row
    styleToggle(filterEnableToggle);
    filterEnableToggle.onClick = [this]() { fireFilterChanged(); };
    addChildComponent(filterEnableToggle);
    filterTypeBox.addItem("Lowpass", 1);
    filterTypeBox.addItem("Highpass", 2);
    filterTypeBox.setSelectedId(1, juce::dontSendNotification);
    filterTypeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(20, 23, 34));
    filterTypeBox.setColour(juce::ComboBox::textColourId,       juce::Colours::white.withAlpha(0.85f));
    filterTypeBox.setColour(juce::ComboBox::outlineColourId,    juce::Colour::fromRGBA(255,255,255,24));
    filterTypeBox.onChange = [this]() { fireFilterChanged(); };
    addChildComponent(filterTypeBox);
    styleSmallKnob(cutoffKnob,     20.0, 20000.0, 8000.0);
    cutoffKnob.setSkewFactorFromMidPoint(1000.0);
    styleSmallKnob(resonanceKnob,  0.1, 10.0, 0.707);
    for (auto* k : { &cutoffKnob, &resonanceKnob })
    {
        k->onValueChange = [this]() { fireFilterChanged(); };
        addChildComponent(k);
    }
    styleSmallLabel(cutoffLabel,    "CUTOFF");
    styleSmallLabel(resonanceLabel, "RESO");
    addChildComponent(cutoffLabel);
    addChildComponent(resonanceLabel);

    // LFO row
    styleToggle(lfoEnableToggle);
    lfoEnableToggle.onClick = [this]() { fireLFOChanged(); };
    addChildComponent(lfoEnableToggle);
    lfoTargetBox.addItem("Pitch",  1);
    lfoTargetBox.addItem("Volume", 2);
    lfoTargetBox.addItem("Cutoff", 3);
    lfoTargetBox.setSelectedId(1, juce::dontSendNotification);
    lfoTargetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(20, 23, 34));
    lfoTargetBox.setColour(juce::ComboBox::textColourId,       juce::Colours::white.withAlpha(0.85f));
    lfoTargetBox.setColour(juce::ComboBox::outlineColourId,    juce::Colour::fromRGBA(255,255,255,24));
    lfoTargetBox.onChange = [this]() { fireLFOChanged(); };
    addChildComponent(lfoTargetBox);
    styleSmallKnob(lfoRateKnob,  0.01, 30.0, 4.0);
    styleSmallKnob(lfoDepthKnob, 0.0, 1.0, 0.0);
    for (auto* k : { &lfoRateKnob, &lfoDepthKnob })
    {
        k->onValueChange = [this]() { fireLFOChanged(); };
        addChildComponent(k);
    }
    styleSmallLabel(lfoRateLabel,  "RATE");
    styleSmallLabel(lfoDepthLabel, "DEPTH");
    addChildComponent(lfoRateLabel);
    addChildComponent(lfoDepthLabel);

    // Sample region row
    styleToggle(loopEnableToggle);
    loopEnableToggle.onClick = [this]() { fireSampleRegionChanged(); };
    addChildComponent(loopEnableToggle);
    styleSmallKnob(sampleStartKnob, 0.0, 1.0, 0.0);
    styleSmallKnob(sampleEndKnob,   0.0, 1.0, 1.0);
    for (auto* k : { &sampleStartKnob, &sampleEndKnob })
    {
        k->onValueChange = [this]() { fireSampleRegionChanged(); };
        addChildComponent(k);
    }
    styleSmallLabel(sampleStartLabel, "START");
    styleSmallLabel(sampleEndLabel,   "END");
    addChildComponent(sampleStartLabel);
    addChildComponent(sampleEndLabel);

    showTab(kTabSample);
}

void SampleEditorPopup::initEngineState(bool envEnabledIn, float attack, float hold, float decay, float sustain, float release,
                                         bool filterEnabledIn, int filterTypeIn, float cutoffHz, float resonance,
                                         bool lfoEnabledIn, int lfoTargetIn, float lfoRateIn, float lfoDepthIn,
                                         float sampleStartIn, float sampleEndIn, bool loopEnabledIn)
{
    envEnableToggle.setToggleState(envEnabledIn, juce::dontSendNotification);
    attackKnob.setValue(attack,   juce::dontSendNotification);
    holdKnob.setValue(hold,       juce::dontSendNotification);
    decayKnob.setValue(decay,     juce::dontSendNotification);
    sustainKnob.setValue(sustain, juce::dontSendNotification);
    releaseKnob.setValue(release, juce::dontSendNotification);

    filterEnableToggle.setToggleState(filterEnabledIn, juce::dontSendNotification);
    filterTypeBox.setSelectedId(filterTypeIn + 1, juce::dontSendNotification);
    cutoffKnob.setValue(cutoffHz,    juce::dontSendNotification);
    resonanceKnob.setValue(resonance, juce::dontSendNotification);

    lfoEnableToggle.setToggleState(lfoEnabledIn, juce::dontSendNotification);
    lfoTargetBox.setSelectedId(lfoTargetIn + 1, juce::dontSendNotification);
    lfoRateKnob.setValue(lfoRateIn,   juce::dontSendNotification);
    lfoDepthKnob.setValue(lfoDepthIn, juce::dontSendNotification);

    loopEnableToggle.setToggleState(loopEnabledIn, juce::dontSendNotification);
    sampleStartKnob.setValue(sampleStartIn, juce::dontSendNotification);
    sampleEndKnob.setValue(sampleEndIn,     juce::dontSendNotification);
}

SampleEditorPopup::~SampleEditorPopup() = default;

void SampleEditorPopup::styleKnob(juce::Slider& s, double lo, double hi, double val)
{
    s.setRange(lo, hi);
    s.setValue(val, juce::dontSendNotification);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                          juce::MathConstants<float>::pi * 2.8f, true);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId,   juce::Colour::fromRGB(80, 100, 255));
    s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(40, 44, 60));
    s.setColour(juce::Slider::thumbColourId,              juce::Colours::white);
    s.setColour(juce::Slider::textBoxTextColourId,        juce::Colours::white.withAlpha(0.8f));
    s.setColour(juce::Slider::textBoxBackgroundColourId,  juce::Colour::fromRGBA(0,0,0,0));
    s.setColour(juce::Slider::textBoxOutlineColourId,     juce::Colour::fromRGBA(0,0,0,0));
}

void SampleEditorPopup::styleSmallKnob(juce::Slider& s, double lo, double hi, double val)
{
    styleKnob(s, lo, hi, val);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 14);
}

void SampleEditorPopup::fireEnvelopeChanged()
{
    if (onEnvelopeChanged)
        onEnvelopeChanged(envEnableToggle.getToggleState(),
                          (float) attackKnob.getValue(),  (float) holdKnob.getValue(),
                          (float) decayKnob.getValue(),   (float) sustainKnob.getValue(),
                          (float) releaseKnob.getValue());
}

void SampleEditorPopup::fireFilterChanged()
{
    if (onFilterChanged)
        onFilterChanged(filterEnableToggle.getToggleState(),
                        filterTypeBox.getSelectedId() - 1,
                        (float) cutoffKnob.getValue(), (float) resonanceKnob.getValue());
}

void SampleEditorPopup::fireLFOChanged()
{
    if (onLFOChanged)
        onLFOChanged(lfoEnableToggle.getToggleState(),
                     lfoTargetBox.getSelectedId() - 1,
                     (float) lfoRateKnob.getValue(), (float) lfoDepthKnob.getValue());
}

void SampleEditorPopup::fireSampleRegionChanged()
{
    if (onSampleRegionChanged)
        onSampleRegionChanged((float) sampleStartKnob.getValue(),
                              (float) sampleEndKnob.getValue(),
                              loopEnableToggle.getToggleState());
}

void SampleEditorPopup::buttonClicked(juce::Button*) {}

void SampleEditorPopup::showTab(int index)
{
    activeTab = index;
    sampleTabBtn.toggled    = (index == kTabSample);
    envelopeTabBtn.toggled  = (index == kTabEnvelope);
    functionsTabBtn.toggled = (index == kTabFunctions);
    engineTabBtn.toggled    = (index == kTabEngine);
    sampleTabBtn.repaint();
    envelopeTabBtn.repaint();
    functionsTabBtn.repaint();
    engineTabBtn.repaint();

    fileLabel.setVisible(index == kTabSample);
    loadBtn.setVisible(index == kTabSample);

    for (auto* c : { (juce::Component*)&volumeKnob, (juce::Component*)&panKnob, (juce::Component*)&pitchKnob,
                     (juce::Component*)&volLabel,   (juce::Component*)&panLabel, (juce::Component*)&pitchLabel })
        c->setVisible(index == kTabEnvelope);

    routingLabel.setVisible(index == kTabFunctions);
    routingBox.setVisible(index == kTabFunctions);

    const bool onEngine = (index == kTabEngine);
    for (auto* c : { (juce::Component*)&envEnableToggle,
                     (juce::Component*)&attackKnob, (juce::Component*)&holdKnob, (juce::Component*)&decayKnob,
                     (juce::Component*)&sustainKnob, (juce::Component*)&releaseKnob,
                     (juce::Component*)&attackLabel, (juce::Component*)&holdLabel, (juce::Component*)&decayLabel,
                     (juce::Component*)&sustainLabel, (juce::Component*)&releaseLabel,
                     (juce::Component*)&filterEnableToggle, (juce::Component*)&filterTypeBox,
                     (juce::Component*)&cutoffKnob, (juce::Component*)&resonanceKnob,
                     (juce::Component*)&cutoffLabel, (juce::Component*)&resonanceLabel,
                     (juce::Component*)&lfoEnableToggle, (juce::Component*)&lfoTargetBox,
                     (juce::Component*)&lfoRateKnob, (juce::Component*)&lfoDepthKnob,
                     (juce::Component*)&lfoRateLabel, (juce::Component*)&lfoDepthLabel,
                     (juce::Component*)&loopEnableToggle,
                     (juce::Component*)&sampleStartKnob, (juce::Component*)&sampleEndKnob,
                     (juce::Component*)&sampleStartLabel, (juce::Component*)&sampleEndLabel })
        c->setVisible(onEngine);

    repaint();
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
    g.drawText(name, titleR.reduced(40, 0), juce::Justification::centredLeft);

    if (activeTab == kTabSample)
    {
        // Waveform display
        auto waveR = juce::Rectangle<int>(8, 76, getWidth() - 16, 80);
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

        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Click the play icon above to audition this sample",
                   8, waveR.getBottom() + 6, getWidth() - 16, 16, juce::Justification::centred);
    }
    else if (activeTab == kTabFunctions)
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Choose which mixer insert this channel's audio is routed to",
                   8, 64, getWidth() - 16, 32, juce::Justification::centredTop);
    }
    else if (activeTab == kTabEngine)
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::FontOptions(9.0f));
        const int rowH = 78;
        int y = 40;
        for (auto* sectionTitle : { "ENVELOPE (AHDSR)", "FILTER", "LFO", "SAMPLE REGION" })
        {
            g.drawText(sectionTitle, 70, y, getWidth() - 80, 14, juce::Justification::centredLeft);
            y += rowH;
        }
    }
}

void SampleEditorPopup::resized()
{
    const int tabSz = 24;
    sampleTabBtn.setBounds(6, 2, tabSz, tabSz);
    envelopeTabBtn.setBounds(6 + tabSz + 4, 2, tabSz, tabSz);
    functionsTabBtn.setBounds(6 + (tabSz + 4) * 2, 2, tabSz, tabSz);
    engineTabBtn.setBounds(6 + (tabSz + 4) * 3, 2, tabSz, tabSz);
    previewBtn.setBounds(getWidth() - tabSz - 6, 2, tabSz, tabSz);

    if (activeTab == kTabSample)
    {
        auto waveR = juce::Rectangle<int>(8, 76, getWidth() - 16, 80);
        fileLabel.setBounds(8, waveR.getBottom() + 26, getWidth() - 16, 16);
        loadBtn.setBounds(8, waveR.getBottom() + 46, getWidth() - 16, 24);
    }
    else if (activeTab == kTabEnvelope)
    {
        const int knobSz = 64;
        const int gap    = (getWidth() - knobSz * 3) / 4;
        int x = gap;
        int y = 90;
        for (auto pair : { std::pair<juce::Slider*, juce::Label*>{ &volumeKnob, &volLabel },
                           std::pair<juce::Slider*, juce::Label*>{ &panKnob,    &panLabel },
                           std::pair<juce::Slider*, juce::Label*>{ &pitchKnob,  &pitchLabel } })
        {
            pair.first->setBounds(x, y, knobSz, knobSz);
            pair.second->setBounds(x - 10, y + knobSz + 2, knobSz + 20, 16);
            x += knobSz + gap;
        }
    }
    else if (activeTab == kTabFunctions)
    {
        routingLabel.setBounds(8, 100, getWidth() - 16, 18);
        routingBox.setBounds(8, 122, getWidth() - 16, 26);
    }
    else if (activeTab == kTabEngine)
    {
        const int knobSz  = 40;
        const int rowH    = 78;
        const int toggleW = 56;
        int y = 38;

        auto layoutKnobRow = [&](juce::ToggleButton& toggle,
                                 std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs,
                                 juce::ComboBox* combo)
        {
            toggle.setBounds(6, y + 18, toggleW, 22);

            int x = 70;
            if (combo != nullptr)
            {
                combo->setBounds(x, y + 16, 84, 22);
                x += 84 + 14;
            }
            for (auto& kp : knobs)
            {
                kp.first->setBounds(x, y + 14, knobSz, knobSz);
                kp.second->setBounds(x - 6, y + 14 + knobSz, knobSz + 12, 12);
                x += knobSz + 16;
            }
            y += rowH;
        };

        layoutKnobRow(envEnableToggle,
                      { { &attackKnob, &attackLabel }, { &holdKnob, &holdLabel }, { &decayKnob, &decayLabel },
                        { &sustainKnob, &sustainLabel }, { &releaseKnob, &releaseLabel } },
                      nullptr);

        layoutKnobRow(filterEnableToggle,
                      { { &cutoffKnob, &cutoffLabel }, { &resonanceKnob, &resonanceLabel } },
                      &filterTypeBox);

        layoutKnobRow(lfoEnableToggle,
                      { { &lfoRateKnob, &lfoRateLabel }, { &lfoDepthKnob, &lfoDepthLabel } },
                      &lfoTargetBox);

        layoutKnobRow(loopEnableToggle,
                      { { &sampleStartKnob, &sampleStartLabel }, { &sampleEndKnob, &sampleEndLabel } },
                      nullptr);
    }
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
                     &swingDnBtn, &swingUpBtn, &grooveBtn, &performBtn,
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

    grooveBtn.setClickingTogglesState(false);
    grooveBtn.setTooltip("Groove/feel template — applies an accent pattern on top of swing");

    performBtn.setClickingTogglesState(false);
    performBtn.setTooltip("Performance mode — launch patterns live, quantized to the loop boundary");

    updateStepButtons();
}

BeatPatternToolbar::~BeatPatternToolbar()
{
    for (auto* b : { &prevPatBtn, &nextPatBtn, &addPatBtn, &patDropBtn,
                     &steps16Btn, &steps32Btn, &steps64Btn,
                     &swingDnBtn, &swingUpBtn, &grooveBtn, &performBtn,
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

void BeatPatternToolbar::setGroove(int templateIndex)
{
    static const char* names[] = { "Straight", "Backbeat", "Pop", "Push", "Hip-Hop" };
    currentGroove = juce::jlimit(0, 4, templateIndex);
    grooveBtn.setButtonText(juce::String("GROOVE: ") + names[currentGroove]);
}

void BeatPatternToolbar::showGrooveMenu()
{
    static const char* names[] = { "Straight", "Backbeat", "Pop", "Push", "Hip-Hop" };

    juce::PopupMenu m;
    for (int i = 0; i < 5; ++i)
        m.addItem(i + 1, names[i], true, currentGroove == i);

    m.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        if (result < 1 || result > 5) return;
        const int idx = result - 1;
        setGroove(idx);
        if (onGrooveChanged) onGrooveChanged(idx);
    });
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
    // Pattern nav: 28 + 3 + 100 + 3 + 24 + 3 + 28 + 3 + 28 + (11) = 231
    // Steps:       28+3+28+3+28 + 11 = 101
    // Swing:       18+3+68+3+18 + 11 = 121
    // Tools:       56+3+38+3+68 = 168
    // Total ~ 621
    const int totalW = 28+3+100+3+24+3+28+3+28+11 + 28+3+28+3+28+11 + 18+3+68+3+18+11 + 56+3+38+3+68;
    int startX = juce::jmax(4, (getWidth() - totalW) / 2);
    const int y = 3;

    auto place = [&](juce::Component& c, int w) {
        c.setBounds(startX, y, w, h);
        startX += w + gap;
    };
    auto gap8 = [&]() { startX += 8; };

    // Pattern navigation: < [Name] [v] > +
    place(prevPatBtn,   28);
    place(patNameLabel, 100);
    place(patDropBtn,   24);
    place(nextPatBtn,   28);
    place(addPatBtn,    28);
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

    // Groove/feel template
    place(grooveBtn, 110);
    gap8();

    // Performance mode
    place(performBtn, 84);
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
    if (b == &grooveBtn) { showGrooveMenu(); return; }
    if (b == &performBtn) { if (onPerformModeToggled) onPerformModeToggled(); return; }
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
    stepGridViewBtn.onClick  = [this]() { if (onShowStepSequencer) onShowStepSequencer(); };
    pianoRollViewBtn.onClick = [this]() { if (onShowPianoRoll)     onShowPianoRoll(); };
    mixerViewBtn.onClick     = [this]() { if (onShowMixer)         onShowMixer(); };

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
    // viewIndex == -1: no view highlighted (e.g. a floating panel was closed)
    stepGridViewBtn.toggled  = (viewIndex == 0);
    pianoRollViewBtn.toggled = (viewIndex == 1);
    mixerViewBtn.toggled     = (viewIndex == 2);
    stepGridViewBtn.repaint();
    pianoRollViewBtn.repaint();
    mixerViewBtn.repaint();
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
    addChildComponent(pianoRoll);     // hidden until the user switches views

    // Step sequencer ("Channel Rack") and Mixer are floating panels that hover
    // over the main canvas, FL Studio-style — draggable, resizable, closable —
    // rather than permanently docked, so the workspace looks neater.
    stepSeqPanel.setContent(&stepSeq);
    stepSeqPanel.onClose = [this]() { stepSeqPanelOpen = false; stepSeqPanel.setVisible(false);
                                      transportBar.setActiveView(-1); };
    addChildComponent(stepSeqPanel);
    stepSeqPanel.setVisible(stepSeqPanelOpen);   // Channel Rack floats open by default

    mixerPanel.setContent(&compactMixer);
    mixerPanel.onClose = [this]() { mixerPanelOpen = false; mixerPanel.setVisible(false);
                                    transportBar.setActiveView(-1); };
    addChildComponent(mixerPanel);
    mixerPanel.setVisible(mixerPanelOpen);

    // Performance Mode — launch pad grid floats over the canvas like the
    // step sequencer/mixer panels. Clicking a pad queues its pattern to take
    // over the engine's active step grid at the next loop boundary.
    performPanel.setContent(&launchGrid);
    performPanel.onClose = [this]() { performPanelOpen = false; performPanel.setVisible(false);
                                      queuedPatternIndex = -1; refreshLaunchGrid();
                                      transportBar.setActiveView(-1); };
    addChildComponent(performPanel);
    performPanel.setVisible(performPanelOpen);

    launchGrid.onPadClicked = [this](int patternIndex)
    {
        if (patternIndex == stepSeq.currentPatternIdx && queuedPatternIndex < 0)
            return; // already playing this pattern and nothing queued — nothing to do
        queuedPatternIndex = patternIndex;
        refreshLaunchGrid();
    };

    transportBar.onPlay          = [this]() { if (onPlay)          onPlay(); };
    transportBar.onStop          = [this]() { if (onStop)          onStop(); };
    transportBar.onRecord        = [this]() { if (onRecord)        onRecord(); };
    transportBar.onReturnToZero  = [this]() { if (onReturnToZero)  onReturnToZero(); };
    transportBar.onTempoChanged  = [this](int bpm) { if (onTempoChanged) onTempoChanged(bpm); };

    // FL Studio-style view switcher: Step sequencer (Channel Rack) / Piano roll / Mixer.
    // Step sequencer and Mixer toggle floating panels (can be open together);
    // Piano roll is the docked canvas view.
    auto panelBoundsFraction = [this](float wFrac, float hFrac, int dx, int dy)
    {
        const int w = juce::jmax(360, juce::roundToInt(canvasArea.getWidth()  * wFrac));
        const int h = juce::jmax(260, juce::roundToInt(canvasArea.getHeight() * hFrac));
        return canvasArea.withSizeKeepingCentre(w, h).translated(dx, dy);
    };

    transportBar.onShowStepSequencer = [this, panelBoundsFraction]() {
        showingPianoRoll = false;
        pianoRoll.setVisible(false);
        // The Channel Rack needs real working room — size it close to the full canvas
        toggleFloatingPanel(stepSeqPanel, stepSeqPanelOpen, panelBoundsFraction(0.94f, 0.92f, -16, -10));
        transportBar.setActiveView(stepSeqPanelOpen ? 0 : -1);
    };
    transportBar.onShowPianoRoll = [this]() {
        showingPianoRoll = true;
        pianoRoll.setVisible(true);
        transportBar.setActiveView(1);
    };
    transportBar.onShowMixer = [this, panelBoundsFraction]() {
        toggleFloatingPanel(mixerPanel, mixerPanelOpen, panelBoundsFraction(0.6f, 0.78f, 30, 24));
        transportBar.setActiveView(mixerPanelOpen ? 2 : -1);
    };

    patternToolbar.onPerformModeToggled = [this, panelBoundsFraction]() {
        toggleFloatingPanel(performPanel, performPanelOpen, panelBoundsFraction(0.5f, 0.6f, -20, 20));
        if (performPanelOpen)
        {
            refreshLaunchGrid();
            startTimerHz(30);
        }
        else
        {
            queuedPatternIndex = -1;
            stopTimer();
        }
    };

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
    patternToolbar.onGrooveChanged = [this](int idx) {
        stepSeq.currentPattern().grooveTemplate = idx;
        if (stepSeq.onGrooveChanged) stepSeq.onGrooveChanged(idx);
    };
    auto syncPatternUI = [this]() {
        const auto& p = stepSeq.currentPattern();
        patternToolbar.setPatternName(p.name);
        patternToolbar.setStepCount(p.stepCount);
        patternToolbar.setGroove(p.grooveTemplate);
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

void BeatWindow::showPianoRollView()
{
    if (transportBar.onShowPianoRoll)
        transportBar.onShowPianoRoll();
}

void BeatWindow::showStepSequencerView()
{
    if (transportBar.onShowStepSequencer)
        transportBar.onShowStepSequencer();
}

void BeatWindow::toggleFloatingPanel(FloatingSubPanel& panel, bool& flag,
                                     juce::Rectangle<int> defaultBounds)
{
    flag = ! flag;
    if (flag)
    {
        if (panel.getBounds().isEmpty() && ! defaultBounds.isEmpty())
            panel.setBounds(defaultBounds);
        panel.setVisible(true);
        panel.toFront(true);
    }
    else
    {
        panel.setVisible(false);
    }
}

void BeatWindow::setMenuBarModel(juce::MenuBarModel* model)
{
    if (model == nullptr)
    {
        menuBar.reset();
    }
    else
    {
        menuBar = std::make_unique<juce::MenuBarComponent>(model);
        addAndMakeVisible(*menuBar);
    }
    resized();
}

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

    // Top menu bar (File/Edit/...) — same as the edit window, for consistency
    // whether the beat window is docked or popped out as a floating window.
    if (menuBar != nullptr)
        menuBar->setBounds(area.removeFromTop(kMenuBarH));

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

    // Remaining canvas area: piano roll docks here when active; the step
    // sequencer ("Channel Rack") and Mixer hover above it as floating panels.
    canvasArea = area;
    pianoRoll.setBounds(area);

    auto defaultPanelBounds = [&](float wFrac, float hFrac, int dx, int dy)
    {
        const int w = juce::jmax(360, juce::roundToInt(area.getWidth()  * wFrac));
        const int h = juce::jmax(260, juce::roundToInt(area.getHeight() * hFrac));
        return area.withSizeKeepingCentre(w, h).translated(dx, dy);
    };

    // Position the floating panels on first layout once the canvas has a
    // sensible size (avoids them landing tiny/top-left during early/zero-size
    // layout passes that happen before the window reaches its real size).
    if (! stepSeqPanelPositioned && area.getWidth() > 200 && area.getHeight() > 200)
    {
        stepSeqPanel.setBounds(defaultPanelBounds(0.94f, 0.92f, -16, -10));
        stepSeqPanelPositioned = true;
    }
    if (! mixerPanelPositioned && area.getWidth() > 200 && area.getHeight() > 200)
    {
        mixerPanel.setBounds(defaultPanelBounds(0.6f, 0.78f, 30, 24));
        mixerPanelPositioned = true;
    }
    if (! performPanelPositioned && area.getWidth() > 200 && area.getHeight() > 200)
    {
        performPanel.setBounds(defaultPanelBounds(0.5f, 0.6f, -20, 20));
        performPanelPositioned = true;
    }

    browser.setBounds(browserArea);
}

// ── Performance Mode ─────────────────────────────────────────────────────
void BeatWindow::timerCallback()
{
    checkPerformanceLaunch();
}

void BeatWindow::checkPerformanceLaunch()
{
    if (queuedPatternIndex < 0 || ! getCurrentStep)
        return;

    const int step = getCurrentStep();
    if (step < 0)
        return;

    // Detect the loop wrapping back to the start (a fall from a high step
    // index down to 0/near-0) and fire the queued pattern at that boundary.
    if (lastSeenStep > step)
        firePatternLaunch(queuedPatternIndex);

    lastSeenStep = step;
}

void BeatWindow::firePatternLaunch(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= stepSeq.patterns.size())
        return;

    queuedPatternIndex = -1;

    auto& pattern = stepSeq.patterns.getReference(patternIndex);
    stepSeq.currentPatternIdx = patternIndex;

    // Push the launched pattern's full step/velocity grid into the engine
    // through the existing per-step callbacks (already wired to the engine
    // by MainComponent), so the new pattern actually drives playback.
    for (int row = 0; row < StepSequencerView::kNumRows; ++row)
    {
        const auto& channel = pattern.channels[row];
        for (int step = 0; step < pattern.stepCount; ++step)
        {
            if (stepSeq.onStepChanged)
                stepSeq.onStepChanged(row, step, channel.steps[step]);
            if (stepSeq.onVelocityChanged)
                stepSeq.onVelocityChanged(row, step, channel.velocities[step]);
        }
    }
    if (stepSeq.onStepCountChanged)
        stepSeq.onStepCountChanged(pattern.stepCount);

    stepSeq.repaint();
    refreshLaunchGrid();
}

void BeatWindow::refreshLaunchGrid()
{
    juce::StringArray names;
    juce::Array<juce::Colour> colours;
    for (auto& p : stepSeq.patterns)
    {
        names.add(p.name);
        colours.add(p.channels[0].colour);
    }
    launchGrid.setPatterns(names, colours);
    launchGrid.setPlayingIndex(stepSeq.currentPatternIdx);
    launchGrid.setQueuedIndex(queuedPatternIndex);
}
