#include "PluginEditor.h"
#include "ui/Visualizer.h"
#include "ui/ModuleCard.h"
#include "ui/AdvancedPanel.h"
#include "ui/VUMeter.h"

static constexpr int kPluginW       = 1080;
static constexpr int kPluginH       = 680;
static constexpr int kVizH          = 130;
static constexpr int kCardH         = 290;
static constexpr int kAdvH          = 130;
static constexpr int kHeaderH       = 40;
static constexpr int kVuW           = 14;
static constexpr int kCardMarginX   = 10;
static constexpr int kCardGap       = 6;

static const juce::Colour kBackground (0xFF080810);

// ─── Content: holds the whole UI at its natural design size; the editor
//     scales this component as a single unit when the window is resized ───
class NovaVoxAudioProcessorEditor::Content : public juce::Component,
                                              private juce::Timer
{
public:
    Content (NovaVoxAudioProcessor& p, NovaVoxLookAndFeel& lafRef)
        : processor (p), laf (lafRef),
          visualizer (p.vizData),
          advancedPanel (p.apvts, lafRef)
    {
        // ── Visualizer ──────────────────────────────────────────────────────
        addAndMakeVisible (visualizer);

        // ── VU Meters ───────────────────────────────────────────────────────
        inputMeter.setPeakHold (true);
        outputMeter.setPeakHold (true);
        outputMeter.setGainReduction (0.f);
        addAndMakeVisible (inputMeter);
        addAndMakeVisible (outputMeter);

        // ── Module card configs ─────────────────────────────────────────────
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

        // ── Advanced panel ───────────────────────────────────────────────────
        advancedPanel.setVisible (false);
        addAndMakeVisible (advancedPanel);

        // ── Input / Output gain sliders (horizontal, above card row) ────────
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

        // ── Global bypass ─────────────────────────────────────────────────────
        bypassButton.setClickingTogglesState (true);
        bypassButton.setLookAndFeel (&laf);
        addAndMakeVisible (bypassButton);
        bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "global_bypass", bypassButton);

        setSize (kPluginW, kPluginH);

        startTimerHz (30);
    }

    ~Content() override
    {
        stopTimer();

        // Clear LAF from every child that holds &laf before laf destructs
        for (auto* card : cards)
            if (card) card->setLookAndFeel (nullptr);
        inputGainSlider .setLookAndFeel (nullptr);
        outputGainSlider.setLookAndFeel (nullptr);
        bypassButton    .setLookAndFeel (nullptr);
        advancedPanel   .setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // Full background
        g.setColour (kBackground);
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

    void resized() override
    {
        const int W = getWidth();

        // ── Header controls ──────────────────────────────────────────────────
        bypassButton.setBounds      (W - 72, 8, 60, 24);
        inputGainSlider.setBounds   (160, 10, 200, 20);
        outputGainSlider.setBounds  (W - 72 - 212, 10, 200, 20);

        // ── VU Meters ────────────────────────────────────────────────────────
        const int meterTop = kHeaderH + 4;
        const int meterH   = kVizH + kCardH - 8;
        inputMeter .setBounds (kCardMarginX, meterTop, kVuW, meterH);
        outputMeter.setBounds (W - kCardMarginX - kVuW, meterTop, kVuW, meterH);

        // ── Visualizer ───────────────────────────────────────────────────────
        const int vizX = kCardMarginX + kVuW + 4;
        const int vizW = W - 2 * (kCardMarginX + kVuW + 4);
        visualizer.setBounds (vizX, kHeaderH, vizW, kVizH);

        // ── Module cards ─────────────────────────────────────────────────────
        const int cardAreaX = kCardMarginX + kVuW + 4;
        const int cardAreaW = W - 2 * (kCardMarginX + kVuW + 4);
        const int cardY     = kHeaderH + kVizH + 4;
        const int numCards  = (int) cards.size();
        const int totalGap  = kCardGap * (numCards - 1);
        const int cardW     = (cardAreaW - totalGap) / numCards;

        for (int i = 0; i < numCards; ++i)
            if (cards[i] != nullptr)
                cards[i]->setBounds (cardAreaX + i * (cardW + kCardGap), cardY, cardW, kCardH);

        // ── Advanced panel ───────────────────────────────────────────────────
        if (advancedPanel.isVisible())
            advancedPanel.setBounds (kCardMarginX, cardY + kCardH + 6, W - 2 * kCardMarginX, kAdvH);
    }

private:
    void timerCallback() override
    {
        inputMeter.setLevel  (processor.getInputPeak());
        outputMeter.setLevel (processor.getOutputPeak());
        outputMeter.setGainReduction (processor.getLevelGainReduction()
                                     + processor.getConsoleGainReduction());
    }

    void onAdvancedToggled (ModuleCard* card)
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

    NovaVoxAudioProcessor& processor;
    NovaVoxLookAndFeel&    laf;

    // ── Visualizer ─────────────────────────────────────────────────────────
    NovaVoxVisualizer visualizer;

    // ── VU Meters ──────────────────────────────────────────────────────────
    VUMeter inputMeter;
    VUMeter outputMeter;

    // ── Five module cards ──────────────────────────────────────────────────
    std::unique_ptr<ModuleCard> consoleCard;
    std::unique_ptr<ModuleCard> curveCard;
    std::unique_ptr<ModuleCard> levelCard;
    std::unique_ptr<ModuleCard> motionCard;
    std::unique_ptr<ModuleCard> spaceCard;

    std::array<ModuleCard*, 5> cards {};

    // ── Advanced panel (shown below cards) ─────────────────────────────────
    AdvancedPanel  advancedPanel;
    ModuleCard*    activeAdvCard = nullptr;

    // ── Input / output gain labels and sliders ─────────────────────────────
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttach;

    // ── Global bypass ──────────────────────────────────────────────────────
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
};

// ─── Editor ──────────────────────────────────────────────────────────────────
NovaVoxAudioProcessorEditor::NovaVoxAudioProcessorEditor (NovaVoxAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    setLookAndFeel (&laf);

    // content's setSize() synchronously triggers its resized(), which
    // unconditionally iterates all 5 slots of `cards` - constructing it any
    // earlier crashes with a null-pointer deref, since `cards` only gets its
    // real pointers once all five ModuleCards have been constructed.
    content = std::make_unique<Content> (p, laf);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits ((int) (kPluginW * 0.6f), (int) (kPluginH * 0.6f),
                      (int) (kPluginW * 1.5f), (int) (kPluginH * 1.5f));
    setSize (kPluginW, kPluginH);
}

NovaVoxAudioProcessorEditor::~NovaVoxAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NovaVoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
}

void NovaVoxAudioProcessorEditor::resized()
{
    if (content == nullptr)
        return;

    const float scaleX = (float) getWidth()  / (float) kPluginW;
    const float scaleY = (float) getHeight() / (float) kPluginH;
    const float scale  = juce::jmin (scaleX, scaleY);

    content->setBounds (0, 0, kPluginW, kPluginH);

    const float offsetX = ((float) getWidth()  - (float) kPluginW * scale) * 0.5f;
    const float offsetY = ((float) getHeight() - (float) kPluginH * scale) * 0.5f;

    content->setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
}
