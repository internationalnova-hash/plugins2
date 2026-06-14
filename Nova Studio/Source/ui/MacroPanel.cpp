#include "MacroPanel.h"

namespace NovaStudioUI
{
    MacroPanel::MacroPanel(NovaStudio::StudioAudioEngine& engineRef)
        : engine(engineRef)
    {
        addBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(40, 36, 60));
        addBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(190, 170, 255));
        addBtn.addListener(this);
        addAndMakeVisible(addBtn);

        rebuildSlots();
    }

    MacroPanel::~MacroPanel()
    {
        addBtn.removeListener(this);
        for (auto* slot : slots)
        {
            slot->knob->removeListener(this);
            slot->knob->onRightClick = nullptr;
        }
    }

    void MacroPanel::rebuildSlots()
    {
        for (auto* slot : slots)
            slot->knob->removeListener(this);
        slots.clear();

        auto& session = engine.getSession();
        const int numMacros = session.getNumMacros();

        for (int i = 0; i < numMacros; ++i)
        {
            auto* slot = new MacroSlot();
            slot->knob = std::make_unique<MacroKnob>();
            slot->knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slot->knob->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            slot->knob->setRange(0.0, 1.0, 0.001);
            slot->knob->setValue(session.getMacro(i).value, juce::dontSendNotification);
            slot->knob->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(170, 130, 255));
            slot->knob->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(50, 52, 68));
            slot->knob->setColour(juce::Slider::thumbColourId, juce::Colours::white);
            slot->knob->addListener(this);

            const int idx = i;
            slot->knob->onRightClick = [this, idx]() { showMacroContextMenu(idx); };

            slot->label.setText(session.getMacro(i).name, juce::dontSendNotification);
            slot->label.setJustificationType(juce::Justification::centred);
            slot->label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.75f));
            slot->label.setFont(juce::Font(juce::FontOptions(10.0f)));

            addAndMakeVisible(*slot->knob);
            addAndMakeVisible(slot->label);
            slots.add(slot);
        }

        resized();
        repaint();
    }

    void MacroPanel::refresh()
    {
        auto& session = engine.getSession();
        if (session.getNumMacros() != slots.size())
        {
            rebuildSlots();
            return;
        }

        for (int i = 0; i < slots.size(); ++i)
        {
            slots[i]->knob->setValue(session.getMacro(i).value, juce::dontSendNotification);
            slots[i]->label.setText(session.getMacro(i).name, juce::dontSendNotification);
        }
    }

    void MacroPanel::sliderValueChanged(juce::Slider* slider)
    {
        for (int i = 0; i < slots.size(); ++i)
        {
            if (slots[i]->knob.get() == slider)
            {
                engine.setMacroValue(i, (float) slider->getValue());
                break;
            }
        }
    }

    void MacroPanel::buttonClicked(juce::Button* button)
    {
        if (button == &addBtn)
        {
            if (engine.getSession().getNumMacros() >= kMaxMacros)
                return;
            engine.getSession().addMacro("Macro " + juce::String(engine.getSession().getNumMacros() + 1));
            rebuildSlots();
        }
    }

    void MacroPanel::renameMacro(int macroIndex)
    {
        auto& session = engine.getSession();
        if (!juce::isPositiveAndBelow(macroIndex, session.getNumMacros())) return;

        auto* aw = new juce::AlertWindow("Rename Macro", "Enter a new name:", juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", session.getMacro(macroIndex).name);
        aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, macroIndex](int result)
        {
            std::unique_ptr<juce::AlertWindow> owned(aw);
            if (result != 1) return;
            auto newName = aw->getTextEditorContents("name").trim();
            if (newName.isEmpty()) return;

            auto& sess = engine.getSession();
            if (juce::isPositiveAndBelow(macroIndex, sess.getNumMacros()))
            {
                sess.getMacro(macroIndex).name = newName;
                refresh();
            }
        }), false);
    }

    void MacroPanel::addMappingTarget(int macroIndex, int trackIndex, const juce::String& parameterId)
    {
        auto& session = engine.getSession();
        if (!juce::isPositiveAndBelow(macroIndex, session.getNumMacros())) return;
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks())) return;

        NovaStudio::MacroTarget target;
        target.trackIndex  = trackIndex;
        target.parameterId = parameterId;

        if (parameterId == "volume") { target.minValue = -60.0f; target.maxValue = 6.0f; }
        else if (parameterId == "pan") { target.minValue = -1.0f; target.maxValue = 1.0f; }
        else if (parameterId.startsWith("send")) { target.minValue = -100.0f; target.maxValue = 6.0f; }
        else { target.minValue = 0.0f; target.maxValue = 1.0f; } // plugin params are normalised

        session.getMacro(macroIndex).targets.add(target);
        engine.setMacroValue(macroIndex, session.getMacro(macroIndex).value); // re-apply immediately
    }

    void MacroPanel::showMacroContextMenu(int macroIndex)
    {
        auto& session = engine.getSession();
        if (!juce::isPositiveAndBelow(macroIndex, session.getNumMacros())) return;

        juce::PopupMenu mapSub;
        const int numTracks = session.getNumTracks();
        static constexpr int kBase = 1000;
        for (int t = 0; t < numTracks; ++t)
        {
            juce::PopupMenu trackSub;
            trackSub.addItem(kBase + t * 10 + 1, "Volume");
            trackSub.addItem(kBase + t * 10 + 2, "Pan");
            for (int s = 0; s < 6; ++s)
                trackSub.addItem(kBase + t * 10 + 3 + s, "Send " + juce::String(s + 1));
            mapSub.addSubMenu(session.getTrack(t).name, trackSub);
        }

        juce::PopupMenu m;
        m.addItem(1, "Rename...");
        m.addSubMenu("Map to track parameter", mapSub, numTracks > 0);
        m.addItem(2, "Clear all mappings", !session.getMacro(macroIndex).targets.isEmpty());
        m.addSeparator();
        m.addItem(3, "Remove macro");

        m.showMenuAsync(juce::PopupMenu::Options(), [this, macroIndex](int result)
        {
            auto& sess = engine.getSession();
            if (!juce::isPositiveAndBelow(macroIndex, sess.getNumMacros())) return;

            if (result == 1)
            {
                renameMacro(macroIndex);
            }
            else if (result == 2)
            {
                sess.getMacro(macroIndex).targets.clear();
            }
            else if (result == 3)
            {
                sess.removeMacro(macroIndex);
                rebuildSlots();
            }
            else if (result >= 1000)
            {
                const int rel   = result - 1000;
                const int track = rel / 10;
                const int sub   = rel % 10;

                juce::String paramId;
                if (sub == 1) paramId = "volume";
                else if (sub == 2) paramId = "pan";
                else if (sub >= 3 && sub <= 8) paramId = "send" + juce::String(sub - 2);

                if (paramId.isNotEmpty())
                    addMappingTarget(macroIndex, track, paramId);
            }
        });
    }

    void MacroPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(16, 17, 24));

        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.setFont(juce::Font(juce::FontOptions(11.5f).withStyle("Bold")));
        g.drawText("MACRO CONTROLS", 12, 8, getWidth() - 24, 18, juce::Justification::centredLeft);

        if (slots.isEmpty())
        {
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText("No macros yet — click \"+ Macro\" to add one, then right-click a knob to map it.",
                       12, 36, getWidth() - 24, 40, juce::Justification::topLeft, true);
        }
    }

    void MacroPanel::resized()
    {
        const int W = getWidth();
        addBtn.setBounds(W - 96, 6, 84, 22);

        const int top = 36;
        const int cellW = 90;
        const int cellH = 100;
        const int cols = juce::jmax(1, W / cellW);

        for (int i = 0; i < slots.size(); ++i)
        {
            const int col = i % cols;
            const int row = i / cols;
            const int x = col * cellW;
            const int y = top + row * cellH;

            slots[i]->knob->setBounds(x + (cellW - 64) / 2, y, 64, 64);
            slots[i]->label.setBounds(x, y + 66, cellW, 18);
        }
    }
}
