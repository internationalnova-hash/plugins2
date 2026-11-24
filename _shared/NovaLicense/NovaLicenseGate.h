#pragma once

// Drop-in license gate for every Nova plugin.
//
//   Processor (header):   NovaLicenseGate licenseGate { *this, "nova-tape", "Nova Tape" };
//   processBlock (first): if (licenseGate.blocks (buffer)) return;
//   Editor (constructor end):   NovaLicenseOverlay::attachTo (*this);
//
// Until the plugin is activated, effects pass audio through untouched (instruments
// are silent) and the editor shows the Nova unlock screen instead of the plugin UI.
// One-time keys unlock this plugin; a Nova All Access key unlocks every Nova plugin.

#include <atomic>
#include <map>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "NovaLicense.h"

#ifndef NOVA_STORE_URL
 #define NOVA_STORE_URL "https://www.getnovaaudio.com"
#endif

class NovaLicenseGate : private juce::Timer
{
public:
    NovaLicenseGate (juce::AudioProcessor& p, juce::String storeId, juce::String productNameIn)
        : processor (p), license (std::move (storeId)), productName (std::move (productNameIn))
    {
        registry()[&processor] = this;
        refresh();
        license.refreshIfDue ([this] { refresh(); });
        startTimer (3000);
    }

    ~NovaLicenseGate() override
    {
        stopTimer();
        registry().erase (&processor);
    }

    /** Audio thread: true while locked (the caller returns from processBlock). */
    template <typename Sample>
    bool blocks (juce::AudioBuffer<Sample>& buffer) noexcept
    {
        if (unlocked.load (std::memory_order_relaxed)) return false;
       #if JucePlugin_IsSynth
        buffer.clear();               // an instrument stays silent
       #else
        // an effect passes its input through untouched (extra output channels silent)
        for (int ch = processor.getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());
       #endif
        return true;
    }

    bool isUnlocked() const noexcept { return unlocked.load(); }
   #if NOVA_LICENSE_DISABLED
    void refresh() { unlocked.store (true); }   // test programs: always unlocked
   #else
    void refresh() { unlocked.store (license.isUnlocked()); }
   #endif

    NovaLicense license;
    const juce::String productName;

    /** The gate belonging to a processor (used by the editor overlay). */
    static NovaLicenseGate* forProcessor (const juce::AudioProcessor* p)
    {
        auto it = registry().find (p);
        return it != registry().end() ? it->second : nullptr;
    }

private:
    void timerCallback() override
    {
        refresh();                                                // another Nova plugin may have activated All Access
        if (++ticks % 200 == 0) license.refreshIfDue ([this] { refresh(); });   // ~every 10 minutes
    }

    static std::map<const juce::AudioProcessor*, NovaLicenseGate*>& registry()
    {
        static std::map<const juce::AudioProcessor*, NovaLicenseGate*> r;
        return r;
    }

    juce::AudioProcessor& processor;
    std::atomic<bool> unlocked { false };
    int ticks = 0;
};

//==============================================================================
class NovaLicenseOverlay : public juce::Component,
                           private juce::ComponentListener,
                           private juce::Timer
{
public:
    /** Adds the unlock screen to an editor (it owns and deletes itself with the editor). */
    static void attachTo (juce::AudioProcessorEditor& editor)
    {
        if (auto* gate = NovaLicenseGate::forProcessor (editor.getAudioProcessor()))
            new NovaLicenseOverlay (editor, *gate);
    }

    ~NovaLicenseOverlay() override
    {
        stopTimer();
        if (host != nullptr) host->removeComponentListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        // Backdrop
        g.fillAll (juce::Colour (0xff09080d));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x2e7c6cff), getWidth() * 0.5f, getHeight() * 0.3f,
                                                 juce::Colour (0x007c6cff), getWidth() * 0.5f, getHeight() * 0.3f + getHeight() * 0.6f, true));
        g.fillAll();

        // Card
        const auto c = card.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (c.translated (0, 10).expanded (6), 22.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17161f), c.getX(), c.getY(), juce::Colour (0xff111017), c.getX(), c.getBottom(), false));
        g.fillRoundedRectangle (c, 18.f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (c.reduced (0.5f), 18.f, 1.f);

        auto r = card.reduced (32, 0);
        r.removeFromTop (28);
        g.setFont (juce::FontOptions (11.f, juce::Font::bold));
        juce::AttributedString brand;
        brand.append ("N O V A   ", juce::FontOptions (11.f, juce::Font::bold), juce::Colour (0xff9a97ab));
        brand.append ("A U D I O", juce::FontOptions (11.f, juce::Font::bold), juce::Colour (0xffb9adff));
        brand.setJustification (juce::Justification::centred);
        brand.draw (g, r.removeFromTop (16).toFloat());

        g.setColour (juce::Colour (0xfff2f0f7));
        g.setFont (juce::FontOptions (24.f, juce::Font::bold));
        g.drawFittedText ("Activate " + gate.productName, r.removeFromTop (40), juce::Justification::centred, 1);
        g.setColour (juce::Colour (0xff9a97ab));
        g.setFont (juce::FontOptions (13.f));
        g.drawFittedText ("Enter the license key from your purchase email.", r.removeFromTop (22), juce::Justification::centred, 1);

        // Divider + All Access card
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (card.getX() + 32, allAccess.getY() - 14, card.getWidth() - 64, 1);
        const auto a = allAccess.toFloat();
        g.setColour (juce::Colour (0x147c6cff));
        g.fillRoundedRectangle (a, 12.f);
        g.setColour (juce::Colour (0x407c6cff));
        g.drawRoundedRectangle (a.reduced (0.5f), 12.f, 1.f);
        auto t = allAccess.reduced (14, 10);
        auto go = t.removeFromRight (64);
        g.setColour (juce::Colour (0xfff2f0f7));
        g.setFont (juce::FontOptions (13.f, juce::Font::bold));
        g.drawText ("Nova All Access", t.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff9a97ab));
        g.setFont (juce::FontOptions (11.f));
        g.drawFittedText ("Every Nova plugin, one key. $9.99/mo or $6.99/mo yearly.\nYour All Access key works here too.", t, juce::Justification::topLeft, 2);
        g.setColour (juce::Colour (0xffb9adff));
        g.setFont (juce::FontOptions (11.f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("GET IT \xe2\x80\xba"), go, juce::Justification::centredRight);
    }

    void resized() override
    {
        const int w = juce::jmin (440, getWidth() - 40), h = 392;
        card = juce::Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre());
        auto r = card.reduced (32, 0);
        r.removeFromTop (28 + 16 + 40 + 22 + 14);
        keyField.setBounds (r.removeFromTop (44));
        r.removeFromTop (12);
        activateButton.setBounds (r.removeFromTop (44));
        r.removeFromTop (6);
        status.setBounds (r.removeFromTop (22));
        r.removeFromTop (22);
        allAccess = r.removeFromTop (62);
        r.removeFromTop (12);
        auto f = r.removeFromTop (20);
        buyLink.setBounds (f.removeFromLeft (f.getWidth() / 2));
        lostLink.setBounds (f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (allAccess.contains (e.getPosition()))
            juce::URL (juce::String (NOVA_STORE_URL) + "/all-access").launchInDefaultBrowser();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (allAccess.contains (e.getPosition()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

private:
    struct Look : juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            const auto r = b.getLocalBounds().toFloat();
            const float k = down ? 0.85f : (over ? 1.08f : 1.f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8f80ff).withMultipliedBrightness (k), 0, 0,
                                                     juce::Colour (0xff6c5cf0).withMultipliedBrightness (k), 0, r.getHeight(), false));
            g.fillRoundedRectangle (r, 10.f);
        }
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (13.f, juce::Font::bold); }
        void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&) override
        {
            g.setColour (juce::Colour (0xff0b0a10));
            g.fillRoundedRectangle (0, 0, (float) w, (float) h, 10.f);
        }
        void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& t) override
        {
            g.setColour (t.hasKeyboardFocus (true) ? juce::Colour (0xff7c6cff) : juce::Colours::white.withAlpha (0.16f));
            g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.f, (float) h - 1.f, 10.f, 1.f);
        }
    };

    NovaLicenseOverlay (juce::AudioProcessorEditor& editor, NovaLicenseGate& g)
        : gate (g), host (&editor)
    {
        setLookAndFeel (&look);
        setWantsKeyboardFocus (true);

        keyField.setJustification (juce::Justification::centred);
        keyField.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.f, juce::Font::bold));
        keyField.setTextToShowWhenEmpty ("NOVA-XXXX-XXXX-XXXX", juce::Colour (0xff4d4a5c));
        keyField.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        keyField.setInputRestrictions (24, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
        keyField.onReturnKey = [this] { activate(); };
        addAndMakeVisible (keyField);

        activateButton.setButtonText ("ACTIVATE");
        activateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        activateButton.onClick = [this] { activate(); };
        addAndMakeVisible (activateButton);

        status.setJustificationType (juce::Justification::centred);
        status.setFont (juce::FontOptions (12.f));
        addAndMakeVisible (status);

        auto styleLink = [] (juce::HyperlinkButton& l, const juce::String& text, const juce::URL& url, juce::Justification j)
        {
            l.setButtonText (text);
            l.setURL (url);
            l.setFont (juce::FontOptions (11.f), false, j);
            l.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff9a97ab));
        };
        styleLink (buyLink, "Buy " + gate.productName, juce::URL (juce::String (NOVA_STORE_URL) + "/plugin/" + storeId()), juce::Justification::centredLeft);
        styleLink (lostLink, "Lost your key?", juce::URL (juce::String (NOVA_STORE_URL) + "/support"), juce::Justification::centredRight);
        addAndMakeVisible (buyLink);
        addAndMakeVisible (lostLink);

        host->addComponentListener (this);
        host->addChildComponent (this);
        setBounds (host->getLocalBounds());
        update();
        startTimer (500);
    }

    juce::String storeId() const
    {
        auto id = gate.license.getAppId();
        return id == "nova-2-track" ? juce::String ("nova-2track") : id;
    }

    void activate()
    {
        const auto key = keyField.getText().trim().toUpperCase();
        if (key.isEmpty()) { showStatus ("Enter your license key.", true); return; }
        activateButton.setEnabled (false);
        showStatus ("Activating...", false);
        juce::Component::SafePointer<NovaLicenseOverlay> safe (this);
        gate.license.activate (key, [safe] (NovaLicense::ActivationResult r)
        {
            if (safe == nullptr) return;
            safe->activateButton.setEnabled (true);
            using R = NovaLicense::ActivationResult;
            switch (r)
            {
                case R::Success:          safe->gate.refresh(); safe->showStatus ("Activated. Enjoy!", false); safe->update(); break;
                case R::InvalidKey:       safe->showStatus ("That key wasn't found. Check it against your purchase email.", true); break;
                case R::WrongPlugin:      safe->showStatus ("That key is for a different Nova plugin.", true); break;
                case R::Expired:          safe->showStatus ("That All Access subscription has ended. Renew it on getnovaaudio.com.", true); break;
                case R::Revoked:          safe->showStatus ("That key has been revoked. Contact support.", true); break;
                case R::SeatLimitReached: safe->showStatus ("That key is already active on its maximum number of computers.", true); break;
                case R::NetworkError:     safe->showStatus ("Couldn't reach the license server. Check your internet connection.", true); break;
                default:                  safe->showStatus ("Activation failed. Please try again.", true); break;
            }
        });
    }

    void showStatus (const juce::String& text, bool error)
    {
        status.setText (text, juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, error ? juce::Colour (0xffff8a7a) : juce::Colour (0xff6ee7a8));
    }

    // Show the unlock screen (and hide the plugin's own UI) only while locked
    void update()
    {
        const bool locked = ! gate.isUnlocked();
        if (locked)
        {
            // Keep the plugin's own UI hidden, even if it tries to show itself later
            for (auto* c : host->getChildren())
                if (c != this && c->isVisible())
                {
                    hidden.addIfNotAlreadyThere (c);
                    c->setVisible (false);
                }
            if (! isVisible())
            {
                setVisible (true);
                toFront (true);
                keyField.grabKeyboardFocus();
            }
        }
        else if (isVisible())
        {
            for (auto& c : hidden) if (c != nullptr) c->setVisible (true);
            hidden.clear();
            setVisible (false);
        }
    }

    void timerCallback() override { update(); }
    void componentMovedOrResized (juce::Component&, bool, bool) override { setBounds (host->getLocalBounds()); }
    void componentBeingDeleted (juce::Component&) override
    {
        host->removeComponentListener (this);
        host = nullptr;
        setLookAndFeel (nullptr);
        delete this;
    }

    NovaLicenseGate& gate;
    juce::Component* host = nullptr;
    Look look;
    juce::TextEditor keyField;
    juce::TextButton activateButton;
    juce::Label status;
    juce::HyperlinkButton buyLink, lostLink;
    juce::Rectangle<int> card, allAccess;
    juce::Array<juce::Component::SafePointer<juce::Component>> hidden;
};
