#include "AdvancedPanel.h"

AdvancedPanel::AdvancedPanel (juce::AudioProcessorValueTreeState& apvts_,
                              NovaVoxLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_)
{
}

AdvancedPanel::~AdvancedPanel()
{
    clearControls();
}

void AdvancedPanel::showModule (const juce::String& moduleName)
{
    clearControls();
    activeModule = moduleName;

    if      (moduleName == "CONSOLE") { accentColour = juce::Colour (0xFFD4A843); buildConsoleControls(); }
    else if (moduleName == "CURVE")   { accentColour = juce::Colour (0xFF4A90FF); buildCurveControls();   }
    else if (moduleName == "LEVEL")   { accentColour = juce::Colour (0xFF8B5CF6); buildLevelControls();   }
    else if (moduleName == "MOTION")  { accentColour = juce::Colour (0xFF06B6D4); buildMotionControls();  }
    else if (moduleName == "SPACE")   { accentColour = juce::Colour (0xFFEC4899); buildSpaceControls();   }

    resized();
    repaint();
}

void AdvancedPanel::clearControls()
{
    for (auto& k : knobs)
    {
        if (k.slider) { k.slider->setLookAndFeel (nullptr); removeChildComponent (k.slider.get()); }
        if (k.label)  removeChildComponent (k.label.get());
    }
    for (auto& t : toggles)
    {
        if (t.button) removeChildComponent (t.button.get());
        if (t.label)  removeChildComponent (t.label.get());
    }
    knobs.clear();
    toggles.clear();
}

void AdvancedPanel::addKnobControl (const juce::String& paramId, const juce::String& labelText)
{
    LabelledKnob lk;
    lk.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag,
                                                 juce::Slider::NoTextBox);
    lk.slider->setLookAndFeel (&laf);
    if (apvts.getParameter (paramId))
        lk.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, *lk.slider);
    addAndMakeVisible (*lk.slider);

    lk.label = std::make_unique<juce::Label>();
    lk.label->setText (labelText.toUpperCase(), juce::dontSendNotification);
    lk.label->setFont (juce::Font (juce::FontOptions().withHeight (9.f).withStyle ("Bold")));
    lk.label->setColour (juce::Label::textColourId, juce::Colour (0xFF64748B));
    lk.label->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (*lk.label);

    knobs.push_back (std::move (lk));
}

void AdvancedPanel::addToggleControl (const juce::String& paramId, const juce::String& labelText)
{
    LabelledToggle lt;
    lt.button = std::make_unique<juce::ToggleButton>();
    lt.button->setButtonText (labelText.toUpperCase());
    if (apvts.getParameter (paramId))
        lt.attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, paramId, *lt.button);
    addAndMakeVisible (*lt.button);

    lt.label = std::make_unique<juce::Label>();
    lt.label->setText (labelText.toUpperCase(), juce::dontSendNotification);
    lt.label->setFont (juce::Font (juce::FontOptions().withHeight (9.f).withStyle ("Bold")));
    lt.label->setColour (juce::Label::textColourId, juce::Colour (0xFF64748B));
    lt.label->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (*lt.label);

    toggles.push_back (std::move (lt));
}

void AdvancedPanel::buildConsoleControls()
{
    // All console controls exposed via the hero knob and mode selector on the card
}

void AdvancedPanel::buildCurveControls()
{
    addKnobControl ("curve_low_db",  "Low");
    addKnobControl ("curve_mid_db",  "Mid");
    addKnobControl ("curve_high_db", "High");
}

void AdvancedPanel::buildLevelControls()
{
    addKnobControl   ("level_output_db", "Output");
    addToggleControl ("level_magic",      "Magic");
}

void AdvancedPanel::buildMotionControls()
{
    addKnobControl ("motion_rate",  "Rate");
    addKnobControl ("motion_depth", "Depth");
}

void AdvancedPanel::buildSpaceControls()
{
    addKnobControl ("space_decay",    "Decay");
    addKnobControl ("space_damping",  "Damping");
    addKnobControl ("space_predelay", "Pre-Delay");
}

void AdvancedPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xFF0F0F18));
    g.fillRoundedRectangle (bounds, 10.f);

    g.setColour (accentColour.withAlpha (0.5f));
    g.drawLine (bounds.getX() + 10.f, bounds.getY() + 0.5f,
                bounds.getRight() - 10.f, bounds.getY() + 0.5f, 1.5f);

    g.setFont (juce::Font (juce::FontOptions().withHeight (11.f).withStyle ("Bold")));
    g.setColour (accentColour);
    g.drawText (activeModule + " \xe2\x80\x94 ADVANCED",
                (int) bounds.getX() + 14, (int) bounds.getY() + 6,
                300, 18, juce::Justification::centredLeft, false);

    if (knobs.empty() && toggles.empty() && activeModule.isNotEmpty())
    {
        g.setFont (juce::Font (juce::FontOptions().withHeight (10.f)));
        g.setColour (juce::Colour (0xFF64748B));
        g.drawText ("All controls are on the module card.",
                    getLocalBounds().reduced (14, 0),
                    juce::Justification::centred, false);
    }
}

void AdvancedPanel::resized()
{
    const auto bounds = getLocalBounds().reduced (14, 28);
    const int  knobH  = juce::jmin (bounds.getHeight(), 70);
    const int  knobW  = 70;
    const int  labelH = 14;

    int x = bounds.getX();
    for (auto& k : knobs)
    {
        k.slider->setBounds (x, bounds.getY(), knobW, knobH - labelH);
        k.label ->setBounds (x, bounds.getY() + knobH - labelH, knobW, labelH);
        x += knobW + 10;
    }
    for (auto& t : toggles)
    {
        t.button->setBounds (x, bounds.getY() + (knobH - 24) / 2, 80, 24);
        t.label ->setBounds (x, bounds.getY() + knobH - labelH, 80, labelH);
        x += 90;
    }
}
