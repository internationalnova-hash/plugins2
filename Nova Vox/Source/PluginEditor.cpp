#include "PluginEditor.h"

static constexpr int kPluginW       = 1080;
static constexpr int kPluginH       = 680;
static constexpr int kVizH          = 130;
static constexpr int kCardH         = 290;
static constexpr int kAdvH          = 130;
static constexpr int kHeaderH       = 40;
static constexpr int kVuW           = 14;
static constexpr int kCardMarginX   = 10;
static constexpr int kCardGap       = 6;

NovaVoxAudioProcessorEditor::NovaVoxAudioProcessorEditor (NovaVoxAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      visualizer (p.vizData),
      advancedPanel (p.apvts, laf)
{
    setLookAndFeel (&laf);
    setSize (kPluginW, kPluginH);

    // ── Visualizer ──────────────────────────────────────────────────────────
    addAndMakeVisible (visualizer);

    // ── VU Meters ───────────────────────────────────────────────────────────
    inputMeter.setPeakHold (true);
    outputMeter.setPeakHold (true);
    outputMeter.setGainReduction (0.f);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    // ── Module card configs ─────────────────────────────────────────────────
    auto makeCard = [&] (const ModuleCardConfig& cfg) -> std::unique_ptr<ModuleCard>
    {
        auto card = std::make_unique<ModuleCard> (p.apvts, cfg, laf);
        card->onAdvancedToggled = [this] (ModuleCard* c) { onAdvancedToggled (c); };
        addAndMakeVisible (*card);
        return card;
    };

    consoleCard = makeCard ({
        "CONSOLE", "NOVA CONSOLE",
        juce::Colour (0xFFD4A843),
        "console_amount", "console_on", "console_mode",
        { "Clean", "British", "Tube/Tape", "Gold", "Modern" }
    });
    curveCard = makeCard ({
        "CURVE", "NOVA CURVE",
        juce::Colour (0xFF4A90FF),
        "curve_amount", "curve_on", "curve_mode",
        { "Flat", "Bright", "Warm", "Air" }
    });
    levelCard = makeCard ({
        "LEVEL", "NOVA LEVEL",
        juce::Colour (0xFF8B5CF6),
        "level_amount", "level_on", "level_mode",
        { "Smooth", "Punch", "Limit" }
    });
    motionCard = makeCard ({
        "MOTION", "NOVA MOTION",
        juce::Colour (0xFF06B6D4),
        "motion_amount", "motion_on", "motion_mode",
        { "Filter", "Delay", "Reverb", "All" }
    });
    spaceCard = makeCard ({
        "SPACE", "NOVA SPACE",
        juce::Colour (0xFFEC4899),
        "space_amount", "space_on", "space_mode",
        { "Studio", "Arena", "Dream", "Vintage" }
    });

    cards = { consoleCard.get(), curveCard.get(), levelCard.get(),
              motionCard.get(), spaceCard.get() };

    // ── Advanced panel ───────────────────────────────────────────────────────
    advancedPanel.setVisible (false);
    addAndMakeVisible (advancedPanel);

    // ── Input / Output gain sliders (horizontal, above card row) ───────────
    auto setupGain = [&] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);
        s.setLookAndFeel (&laf);
        addAndMakeVisible (s);
    };
    setupGain (inputGainSlider);
    setupGain (outputGainSlider);

    inputAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "global_input_db",  inputGainSlider);
    outputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "global_output_db", outputGainSlider);

    // ── Global bypass ────────────────────────────────────────────────────────
    bypassButton.setClickingTogglesState (true);
    bypassButton.setLookAndFeel (&laf);
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "global_bypass", bypassButton);

    startTimerHz (30);
}

NovaVoxAudioProcessorEditor::~NovaVoxAudioProcessorEditor()
{
    stopTimer();

    // Clear LAF from every child that holds &laf before laf destructs
    for (auto* card : cards)
        if (card) card->setLookAndFeel (nullptr);
    inputGainSlider .setLookAndFeel (nullptr);
    outputGainSlider.setLookAndFeel (nullptr);
    bypassButton    .setLookAndFeel (nullptr);
    advancedPanel   .setLookAndFeel (nullptr);
    setLookAndFeel  (nullptr);
}

void NovaVoxAudioProcessorEditor::timerCallback()
{
    inputMeter.setLevel  (processor.getInputPeak());
    outputMeter.setLevel (processor.getOutputPeak());
    outputMeter.setGainReduction (processor.getLevelGainReduction()
                                 + processor.getConsoleGainReduction());
}

void NovaVoxAudioProcessorEditor::onAdvancedToggled (ModuleCard* card)
{
    if (card == activeAdvCard)
    {
        // Same card clicked again — close panel
        advancedPanel.setVisible (false);
        advancedPanel.showModule ("");
        card->setAdvancedOpen (false);
        activeAdvCard = nullptr;
    }
    else
    {
        // New card opened
        if (activeAdvCard)
            activeAdvCard->setAdvancedOpen (false);

        activeAdvCard = card;
        advancedPanel.showModule (card->getName());
        advancedPanel.setVisible (true);
    }
    resized();
    repaint();
}

void NovaVoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Full background
    g.setColour (juce::Colour (0xFF080810));
    g.fillRect (bounds);

    // Header bar
    {
        juce::ColourGradient hdr (
            juce::Colour (0xFF10101E), 0.f, 0.f,
            juce::Colour (0xFF0C0C16), 0.f, (float) kHeaderH, false);
        g.setGradientFill (hdr);
        g.fillRect (0, 0, getWidth(), kHeaderH);

        // Bottom hairline
        g.setColour (juce::Colour (0xFF2A2A40));
        g.drawHorizontalLine (kHeaderH - 1, 0.f, (float) getWidth());
    }

    // "NOVA VOX" wordmark
    g.setFont (juce::Font (juce::FontOptions().withHeight (18.f).withStyle ("Bold")));
    g.setColour (juce::Colours::white.withAlpha (0.90f));
    g.drawText ("NOVA VOX", 18, 10, 130, 22, juce::Justification::centredLeft, false);

    // Version tag
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.f)));
    g.setColour (juce::Colour (0xFF3A3A58));
    g.drawText ("v1.0", 18, 28, 60, 10, juce::Justification::centredLeft, false);

    // Divider above advanced panel
    if (advancedPanel.isVisible())
    {
        const int divY = kHeaderH + kVizH + kCardH + 4;
        g.setColour (juce::Colour (0xFF1E1E2C));
        g.fillRect (0, divY, getWidth(), 2);
    }
}

void NovaVoxAudioProcessorEditor::resized()
{
    const int W = getWidth();

    // ── Header controls ──────────────────────────────────────────────────────
    bypassButton.setBounds      (W - 72, 8, 60, 24);
    inputGainSlider.setBounds   (160, 10, 200, 20);
    outputGainSlider.setBounds  (W - 72 - 212, 10, 200, 20);

    // ── VU Meters ────────────────────────────────────────────────────────────
    const int meterTop = kHeaderH + 4;
    const int meterH   = kVizH + kCardH - 8;
    inputMeter .setBounds (kCardMarginX, meterTop, kVuW, meterH);
    outputMeter.setBounds (W - kCardMarginX - kVuW, meterTop, kVuW, meterH);

    // ── Visualizer ───────────────────────────────────────────────────────────
    const int vizX = kCardMarginX + kVuW + 4;
    const int vizW = W - 2 * (kCardMarginX + kVuW + 4);
    visualizer.setBounds (vizX, kHeaderH, vizW, kVizH);

    // ── Module cards ─────────────────────────────────────────────────────────
    const int cardAreaX = kCardMarginX + kVuW + 4;
    const int cardAreaW = W - 2 * (kCardMarginX + kVuW + 4);
    const int cardY     = kHeaderH + kVizH + 4;
    const int numCards  = (int) cards.size();
    const int totalGap  = kCardGap * (numCards - 1);
    const int cardW     = (cardAreaW - totalGap) / numCards;

    for (int i = 0; i < numCards; ++i)
        cards[i]->setBounds (cardAreaX + i * (cardW + kCardGap), cardY, cardW, kCardH);

    // ── Advanced panel ───────────────────────────────────────────────────────
    if (advancedPanel.isVisible())
        advancedPanel.setBounds (kCardMarginX, cardY + kCardH + 6, W - 2 * kCardMarginX, kAdvH);
}
