#include "MixAssistantWindow.h"
#include "Theme.h"

using namespace NovaStudio;
using namespace NovaStudio::MixAI;

namespace NovaStudioUI
{
namespace
{
    // Nova look (matches the approved mockup)
    const juce::Colour kBg       = juce::Colour (0xff0e1016);
    const juce::Colour kPanel    = juce::Colour (0xff151821);
    const juce::Colour kCard     = juce::Colour (0xff1b1f2a);
    const juce::Colour kCardHi   = juce::Colour (0xff221f33);
    const juce::Colour kEdge     = juce::Colour (0x18ffffff);
    const juce::Colour kText     = juce::Colour (0xffe9e9f1);
    const juce::Colour kDim      = juce::Colour (0xff8a8fa3);
    const juce::Colour kFaint    = juce::Colour (0xff5d6275);
    const juce::Colour kAccent   = juce::Colour (0xff9650ff);
    const juce::Colour kAccentLo = juce::Colour (0x339650ff);
    const juce::Colour kBlue     = juce::Colour (0xff7fb4ff);
    const juce::Colour kCheck    = juce::Colour (0xffffa53a);

    juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    const juce::String kDot = juce::String::fromUTF8 (" \xc2\xb7 ");     // " · " (source literals are read as Latin-1)

    juce::String dB (float v)
    {
        return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    }

    juce::String panText (float p)
    {
        if (std::abs (p) < 0.03f) return "C";
        return (p < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (p) * 100.f));
    }

    struct NovaLnf : juce::LookAndFeel_V4
    {
        NovaLnf()
        {
            setColour (juce::ComboBox::backgroundColourId, kCard);
            setColour (juce::ComboBox::outlineColourId, kEdge);
            setColour (juce::ComboBox::textColourId, kText);
            setColour (juce::ComboBox::arrowColourId, kDim);
            setColour (juce::PopupMenu::backgroundColourId, kPanel);
            setColour (juce::PopupMenu::textColourId, kText);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccentLo);
            setColour (juce::PopupMenu::highlightedTextColourId, kText);
            setColour (juce::TextButton::buttonColourId, kCard);
            setColour (juce::TextButton::buttonOnColourId, kAccentLo);
            setColour (juce::TextButton::textColourOffId, kDim);
            setColour (juce::TextButton::textColourOnId, kText);
            setColour (juce::ToggleButton::textColourId, kText);
            setColour (juce::ToggleButton::tickColourId, kAccent);
            setColour (juce::ToggleButton::tickDisabledColourId, kFaint);
            setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x30ffffff));
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            auto c = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : bg;
            if (b.getProperties()["primary"])
                c = b.isEnabled() ? kAccent : kAccent.withAlpha (0.35f);
            if (over && b.isEnabled()) c = c.brighter (0.08f);
            if (down) c = c.darker (0.1f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (b.getToggleState() ? kAccent.withAlpha (0.6f) : kEdge);
            g.drawRoundedRectangle (r, 6.f, 1.f);
        }

        juce::Font getTextButtonFont (juce::TextButton& b, int) override
        {
            return font (b.getProperties()["primary"] ? 14.f : 12.5f, (bool) b.getProperties()["primary"] || b.getToggleState());
        }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            g.setColour (findColour (juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle (r, 5.f);
            g.setColour (box.hasKeyboardFocus (false) ? kAccent.withAlpha (0.6f) : kEdge);
            g.drawRoundedRectangle (r, 5.f, 1.f);
            juce::Path p;
            const float cx = (float) w - 11.f, cy = (float) h * 0.5f;
            p.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
            g.setColour (kDim);
            g.fillPath (p);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.f); }
        juce::Font getPopupMenuFont() override { return font (13.f); }
    };

    void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour fill, juce::Colour edge, juce::Colour ink, bool bold = false)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (edge);
        g.drawRoundedRectangle (r, 4.f, 1.f);
        g.setColour (ink);
        g.setFont (font (11.f, bold));
        g.drawText (text, r.reduced (6, 0), juce::Justification::centred, true);
    }

    /// lays chips out left to right, wrapping; returns the y below the last row
    float chipRow (juce::Graphics* g, float x0, float y, float right, const juce::StringArray& items,
                   juce::Colour fill, juce::Colour edge, juce::Colour ink, bool bold = false)
    {
        float x = x0;
        const float h = 20.f, gap = 5.f;
        for (auto& s : items)
        {
            const float w = juce::jmin (right - x0, juce::GlyphArrangement::getStringWidth (font (11.f, bold), s) + 14.f);
            if (x + w > right && x > x0) { x = x0; y += h + gap; }
            if (g != nullptr) chip (*g, { x, y, w, h }, s, fill, edge, ink, bold);
            x += w + gap;
        }
        return items.isEmpty() ? y : y + h + gap;
    }

    juce::String busHeadline (const PlannedBus& b)
    {
        return dB (b.volumeDb);
    }
}

//==============================================================================
// Left: what was heard, one row per track, role changeable
struct MixAssistantPanel::RoleList : juce::Component
{
    explicit RoleList (MixAssistantPanel& o) : owner (o) {}

    static constexpr int kRowH = 56;

    void rebuild()
    {
        boxes.clear();
        const auto names = allRoleNames();
        for (size_t i = 0; i < owner.analysis.tracks.size(); ++i)
        {
            auto* box = boxes.add (new juce::ComboBox());
            box->addItemList (names, 1);
            box->setSelectedItemIndex ((int) owner.roles[i], juce::dontSendNotification);
            box->onChange = [this, i, box]
            {
                owner.roles[i] = roleFromName (box->getText());
                owner.rebuildPlan();
                repaint();
            };
            addAndMakeVisible (box);
        }
        setSize (getWidth(), juce::jmax (1, (int) owner.analysis.tracks.size() * kRowH + 8));
        resized();
        repaint();
    }

    void resized() override
    {
        for (int i = 0; i < boxes.size(); ++i)
            boxes[i]->setBounds (getWidth() - 136, i * kRowH + 10, 124, 24);
    }

    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < owner.analysis.tracks.size(); ++i)
        {
            const auto& t = owner.analysis.tracks[i];
            const Role r = owner.roles[i];
            auto row = juce::Rectangle<int> (0, (int) i * kRowH, getWidth(), kRowH).reduced (8, 3);
            g.setColour (kCard.withAlpha (0.55f));
            g.fillRoundedRectangle (row.toFloat(), 6.f);
            g.setColour (roleColour (r));
            g.fillRoundedRectangle (row.removeFromLeft (3).toFloat(), 1.5f);
            row.removeFromLeft (8);
            auto text = row.withTrimmedRight (136);
            g.setColour (r == Role::Skip ? kFaint : kText);
            g.setFont (font (13.f, true));
            g.drawText (t.name, text.removeFromTop (row.getHeight() / 2).withTrimmedTop (4), juce::Justification::bottomLeft, true);
            g.setColour (kDim);
            g.setFont (font (11.f));
            g.drawText (t.heard, text.withTrimmedTop (2), juce::Justification::topLeft, true);

            // confidence under the dropdown
            auto bar = juce::Rectangle<float> ((float) getWidth() - 136.f, (float) i * kRowH + 38.f, 124.f, 3.f);
            g.setColour (juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (bar, 1.5f);
            const bool unsure = t.confidence < 0.55f && r == t.role;
            g.setColour (unsure ? kCheck : roleColour (r));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.05f, 1.f, t.confidence)), 1.5f);
            if (unsure)
            {
                g.setFont (font (10.f, true));
                g.drawText ("CHECK", bar.translated (0, 4).withHeight (12), juce::Justification::centredLeft);
            }
        }
    }

    MixAssistantPanel& owner;
    juce::OwnedArray<juce::ComboBox> boxes;
};

//==============================================================================
// Middle: the plan as cards (click one to see it on the right)
struct MixAssistantPanel::PlanView : juce::Component
{
    explicit PlanView (MixAssistantPanel& o) : owner (o) {}

    struct Card { juce::Rectangle<float> r; int bus; };
    std::vector<Card> cards;

    juce::StringArray membersOf (const PlannedBus& b) const
    {
        juce::StringArray out;
        for (int ti : b.members)
            for (const auto& m : owner.plan.tracks)
                if (m.track == ti)
                {
                    const auto& s = owner.engine.getSession();
                    const auto name = juce::isPositiveAndBelow (ti, s.getNumTracks()) ? s.getTrack (ti).name : juce::String (ti + 1);
                    out.add (name + (std::abs (m.pan) > 0.03f ? "  " + panText (m.pan) : juce::String()));
                }
        return out;
    }

    static juce::StringArray chainOf (const std::vector<PluginStep>& chain)
    {
        juce::StringArray out;
        for (auto& s : chain) out.add (s.plugin);
        return out;
    }

    static juce::String sendsText (const std::vector<Send>& sends)
    {
        juce::StringArray parts;
        for (auto& s : sends) parts.add (s.toBus + " " + juce::String (juce::roundToInt (s.levelDb)));
        return parts.isEmpty() ? juce::String() : "sends > " + parts.joinIntoString (kDot);
    }

    float layoutCard (juce::Graphics* g, const PlannedBus& b, juce::Rectangle<float> area, bool sel)
    {
        const float x = area.getX() + 12.f, right = area.getRight() - 12.f;
        float y = area.getY() + 12.f;
        if (g != nullptr)
        {
            g->setColour (sel ? kCardHi : kCard);
            g->fillRoundedRectangle (area, 8.f);
            g->setColour (sel ? kAccent.withAlpha (0.8f) : kEdge);
            g->drawRoundedRectangle (area.reduced (0.5f), 8.f, sel ? 1.5f : 1.f);
            g->setColour (b.colour);
            g->fillRoundedRectangle ({ x, y + 3.f, 9.f, 9.f }, 2.f);
            g->setColour (kText);
            g->setFont (font (13.f, true));
            g->drawText (b.name, juce::Rectangle<float> (x + 15.f, y - 1.f, right - x - 90.f, 18.f), juce::Justification::centredLeft, true);
            g->setColour (kDim);
            g->setFont (font (11.f));
            g->drawText (b.name == "Straight to Main Out" ? juce::String ("Main Out") : busHeadline (b), juce::Rectangle<float> (right - 90.f, y - 1.f, 90.f, 18.f), juce::Justification::centredRight);
        }
        y += 26.f;
        y = chipRow (g, x, y, right, membersOf (b), kBg.withAlpha (0.6f), kEdge, kDim);
        y = chipRow (g, x, y, right, chainOf (b.chain), kAccentLo, kAccent.withAlpha (0.45f), juce::Colour (0xffd9c6ff), true);
        const auto sends = sendsText (b.sends);
        if (sends.isNotEmpty())
        {
            if (g != nullptr) { g->setColour (kBlue); g->setFont (font (11.f)); g->drawText (sends, juce::Rectangle<float> (x, y, right - x, 16.f), juce::Justification::centredLeft, true); }
            y += 18.f;
        }
        return y + 6.f - area.getY();
    }

    void paint (juce::Graphics& g) override
    {
        cards.clear();
        const auto& p = owner.plan;
        const float w = (float) getWidth(), pad = 12.f, gap = 10.f;
        float y = 6.f;

        if (owner.state == State::Listening)
        {
            g.setColour (kDim);
            g.setFont (font (14.f));
            g.drawText ("Listening to every track... " + juce::String (juce::roundToInt (owner.progress->load() * 100.f)) + "%",
                        getLocalBounds().withHeight (80), juce::Justification::centred);
            return;
        }

        // group buses, two per row
        std::vector<int> groups, returns;
        for (size_t i = 0; i < p.buses.size(); ++i) (p.buses[i].isReturn ? returns : groups).push_back ((int) i);
        const float colW = (w - pad * 2.f - gap) * 0.5f;
        for (size_t k = 0; k < groups.size(); k += 2)
        {
            float rowH = 0.f;
            for (size_t j = k; j < juce::jmin (groups.size(), k + 2); ++j)
                rowH = juce::jmax (rowH, layoutCard (nullptr, p.buses[(size_t) groups[j]], { 0, 0, colW, 1000.f }, false));
            for (size_t j = k; j < juce::jmin (groups.size(), k + 2); ++j)
            {
                juce::Rectangle<float> r (pad + (float) (j - k) * (colW + gap), y, colW, rowH);
                layoutCard (&g, p.buses[(size_t) groups[j]], r, owner.selected == groups[j]);
                cards.push_back ({ r, groups[j] });
            }
            y += rowH + gap;
        }

        // tracks that go straight to Main Out (e.g. a lone lead vocal with its own chain)
        if (auto direct = owner.directTracks(); ! direct.empty())
        {
            PlannedBus pseudo;
            pseudo.name = "Straight to Main Out";
            pseudo.colour = kDim;
            for (auto* m : direct) { pseudo.members.push_back (m->track); for (auto& st : m->chain) pseudo.chain.push_back (st); }
            juce::Rectangle<float> r (pad, y, w - pad * 2.f, 0.f);
            r.setHeight (layoutCard (nullptr, pseudo, r.withHeight (1000.f), false));
            layoutCard (&g, pseudo, r, owner.selected == -3);
            cards.push_back ({ r, -3 });
            y += r.getHeight() + gap;
        }

        // effect returns on one line
        if (! returns.empty())
        {
            juce::Rectangle<float> r (pad, y, w - pad * 2.f, 40.f);
            const bool sel = std::find (returns.begin(), returns.end(), owner.selected) != returns.end();
            g.setColour (sel ? kCardHi : kCard);
            g.fillRoundedRectangle (r, 8.f);
            g.setColour (sel ? kAccent.withAlpha (0.8f) : kEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
            g.setColour (kBlue);
            g.fillRoundedRectangle ({ r.getX() + 12.f, r.getCentreY() - 4.5f, 9.f, 9.f }, 2.f);
            g.setColour (kText);
            g.setFont (font (13.f, true));
            g.drawText ("Effect Returns", juce::Rectangle<float> (r.getX() + 27.f, r.getY(), 130.f, r.getHeight()), juce::Justification::centredLeft);
            float x = r.getX() + 160.f;
            for (int bi : returns)
            {
                const auto& b = p.buses[(size_t) bi];
                const auto label = b.name + (b.chain.empty() ? juce::String() : kDot + b.chain.front().plugin);
                const float cw = juce::GlyphArrangement::getStringWidth (font (11.f, true), label) + 16.f;
                juce::Rectangle<float> c (x, r.getCentreY() - 10.f, cw, 20.f);
                chip (g, c, label, kAccentLo, kAccent.withAlpha (0.45f), juce::Colour (0xffd9c6ff), true);
                cards.push_back ({ c.expanded (2.f), bi });
                x += cw + 6.f;
            }
            y += 40.f + gap;
        }

        // mix bus
        {
            juce::Rectangle<float> r (pad, y, w - pad * 2.f, 40.f);
            const bool sel = owner.selected == -2;
            g.setColour (sel ? kCardHi : kBg.withAlpha (0.5f));
            g.fillRoundedRectangle (r, 8.f);
            g.setColour (sel ? kAccent.withAlpha (0.8f) : kEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
            g.setColour (kText);
            g.setFont (font (13.f, true));
            g.drawText ("Mix Bus  >", juce::Rectangle<float> (r.getX() + 12.f, r.getY(), 90.f, r.getHeight()), juce::Justification::centredLeft);
            float x = r.getX() + 104.f;
            juce::StringArray steps;
            for (auto& s : p.master) steps.add (s.plugin);
            if (steps.isEmpty()) steps.add ("no mix bus polish");
            for (auto& s : steps)
            {
                const float cw = juce::GlyphArrangement::getStringWidth (font (11.f, true), s) + 16.f;
                chip (g, { x, r.getCentreY() - 10.f, cw, 20.f }, s, kAccentLo, kAccent.withAlpha (0.45f), juce::Colour (0xffd9c6ff), true);
                x += cw + 6.f;
            }
            g.setColour (kDim);
            g.setFont (font (12.f));
            g.drawText ("> Master Out", juce::Rectangle<float> (x + 4.f, r.getY(), 120.f, r.getHeight()), juce::Justification::centredLeft);
            cards.push_back ({ r, -2 });
            y += 40.f + gap;
        }

        // notes
        g.setFont (font (11.5f));
        for (auto& n : p.notes)
        {
            g.setColour (kDim);
            g.drawFittedText (juce::String::fromUTF8 ("\xc2\xb7  ") + n, juce::Rectangle<int> ((int) pad + 4, (int) y, (int) (w - pad * 2.f) - 8, 30), juce::Justification::topLeft, 2);
            y += 30.f;
        }
        neededHeight = (int) y + 10;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& c : cards)
            if (c.r.contains (e.position)) { owner.selected = c.bus; owner.everSelected = true; owner.repaint(); return; }   // repaints the plan and the detail column
    }

    int neededHeight = 400;
    MixAssistantPanel& owner;
};

//==============================================================================
// Right: what it'll do to the selected bus, and why
struct MixAssistantPanel::DetailView : juce::Component
{
    explicit DetailView (MixAssistantPanel& o) : owner (o) {}

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.plan;
        auto area = getLocalBounds().reduced (14, 12);
        if (owner.state == State::Listening) return;

        juce::String title, why;
        std::vector<const PluginStep*> chain;
        juce::StringArray level;      // "label|value"
        if (owner.selected == -2)
        {
            title = "MIX BUS";
            why = p.master.empty() ? "Mix bus polish is off - the mix bus is left as it is."
                                   : "Nova Mirror nudges the tone toward the style (or your reference), then Nova AI Master "
                                     "listens to the finished mix and masters it to the loudness target, true-peak safe.";
            for (auto& s : p.master) chain.push_back (&s);
            level.add ("Buses used|" + juce::String (p.busesUsed) + " of " + juce::String (p.busesAvailable) + " free");
            level.add ("Changes|" + juce::String (p.changeCount()));
        }
        else if (owner.selected == -3)
        {
            title = "STRAIGHT TO MAIN OUT";
            why = "These tracks don't need a group bus: each one gets its own level, pan and chain and plays straight to Main Out.";
            for (auto* m : owner.directTracks())
                if (m->role == Role::Lead)
                {
                    why << " The lead vocal stays here so its own chain (Nova AI Vocal) does the work, with sends to the vocal effects.";
                    break;
                }
            const auto& s = owner.engine.getSession();
            for (auto* m : owner.directTracks())
            {
                if (juce::isPositiveAndBelow (m->track, s.getNumTracks()))
                    level.add (s.getTrack (m->track).name + "|" + (m->clipGainDb > 0.05f ? "clip +" + juce::String (m->clipGainDb, 1) + "  " : juce::String())
                               + dB (m->volumeDb) + "  " + panText (m->pan));
                for (auto& st : m->chain) chain.push_back (&st);
                for (auto& sd : m->sends) level.add ("  " + sd.toBus + " send|" + juce::String (sd.levelDb, 0) + " dB");
            }
        }
        else if (juce::isPositiveAndBelow (owner.selected, (int) p.buses.size()))
        {
            const auto& b = p.buses[(size_t) owner.selected];
            title = b.name.toUpperCase();
            why = b.why;
            for (auto& s : b.chain) chain.push_back (&s);
            level.add ("Fader|" + dB (b.volumeDb));
            if (! b.members.empty()) level.add ("Tracks|" + juce::String ((int) b.members.size()));
            for (auto& s : b.sends) level.add (s.toBus + " send|" + juce::String (s.levelDb, 0) + " dB");
            for (auto& m : p.tracks)
                if (std::find (b.members.begin(), b.members.end(), m.track) != b.members.end() && m.change)
                {
                    const auto& s = owner.engine.getSession();
                    if (juce::isPositiveAndBelow (m.track, s.getNumTracks()))
                        level.add (s.getTrack (m.track).name + "|" + (m.clipGainDb > 0.05f ? "clip +" + juce::String (m.clipGainDb, 1) + "  " : juce::String())
                                   + dB (m.volumeDb) + "  " + panText (m.pan));
                    for (auto& st : m.chain) chain.push_back (&st);
                }
        }

        g.setColour (kText);
        g.setFont (font (12.f, true));
        g.drawText (title, area.removeFromTop (20), juce::Justification::centredLeft, true);
        area.removeFromTop (8);

        auto section = [&] (const juce::String& head, int h, std::function<void (juce::Rectangle<int>)> body)
        {
            auto box = area.removeFromTop (h);
            g.setColour (kCard.withAlpha (0.7f));
            g.fillRoundedRectangle (box.toFloat(), 7.f);
            g.setColour (kEdge);
            g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 7.f, 1.f);
            auto in = box.reduced (12, 10);
            g.setColour (kFaint);
            g.setFont (font (10.5f, true));
            g.drawText (head, in.removeFromTop (14), juce::Justification::centredLeft);
            in.removeFromTop (6);
            body (in);
            area.removeFromTop (10);
        };

        if (why.isNotEmpty())
            section ("WHY", 104, [&] (juce::Rectangle<int> r)
            {
                g.setColour (kText.withAlpha (0.85f));
                g.setFont (font (12.f));
                g.drawFittedText (why, r, juce::Justification::topLeft, 5, 1.f);
            });

        if (! chain.empty())
            section ("CHAIN", 42 + juce::jmin (6, (int) chain.size()) * 30, [&] (juce::Rectangle<int> r)
            {
                int k = 1;
                for (auto* s : chain)
                {
                    if (k > 6) break;
                    auto row = r.removeFromTop (26);
                    r.removeFromTop (4);
                    g.setColour (kBg.withAlpha (0.6f));
                    g.fillRoundedRectangle (row.toFloat(), 5.f);
                    g.setColour (kFaint);
                    g.setFont (font (11.f));
                    g.drawText (juce::String (k++), row.removeFromLeft (22), juce::Justification::centred);
                    g.setColour (kText);
                    g.setFont (font (12.f, true));
                    g.drawText (s->plugin, row.removeFromLeft (110), juce::Justification::centredLeft, true);
                    g.setColour (kDim);
                    g.setFont (font (10.5f));
                    juce::String note = s->note;
                    if (note.isEmpty())
                        for (auto& st : s->settings) note << (note.isEmpty() ? juce::String() : kDot) << st.name << " " << st.value;
                    g.drawText (note, row.reduced (6, 0), juce::Justification::centredRight, true);
                }
            });

        if (! level.isEmpty())
            section ("LEVEL & SPACE", 42 + juce::jmin (10, level.size()) * 20, [&] (juce::Rectangle<int> r)
            {
                for (int i = 0; i < juce::jmin (10, level.size()); ++i)
                {
                    auto row = r.removeFromTop (20);
                    const auto value = level[i].fromFirstOccurrenceOf ("|", false, false);
                    const int vw = (int) juce::GlyphArrangement::getStringWidth (font (12.f, true), value) + 10;
                    g.setColour (kText);
                    g.setFont (font (12.f, true));
                    g.drawText (value, row, juce::Justification::centredRight, true);
                    g.setColour (kDim);
                    g.setFont (font (12.f));
                    g.drawText (level[i].upToFirstOccurrenceOf ("|", false, false), row.withTrimmedRight (vw), juce::Justification::centredLeft, true);
                }
            });
    }

    MixAssistantPanel& owner;
};

//==============================================================================
MixAssistantPanel::MixAssistantPanel (StudioAudioEngine& e, Callbacks c)
    : engine (e), cb (std::move (c))
{
    lnf = std::make_unique<NovaLnf>();
    setLookAndFeel (lnf.get());

    styleBox.addItemList (styleNames(), 1);
    styleBox.setSelectedItemIndex (0, juce::dontSendNotification);
    styleBox.onChange = [this] { options.style = styleBox.getText(); rebuildPlan(); };
    addAndMakeVisible (styleBox);

    refButton.onClick = [this]
    {
        if (options.reference.existsAsFile())
        {
            options.reference = juce::File();
            refButton.setButtonText ("Drop a song...");
            rebuildPlan();
            return;
        }
        chooser = std::make_unique<juce::FileChooser> ("Pick a reference song", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile()) filesDropped ({ fc.getResult().getFullPathName() }, 0, 0);
                              });
    };
    addAndMakeVisible (refButton);

    for (int i = 0; i < 3; ++i)
    {
        howFar[i].setRadioGroupId (4101);
        howFar[i].setClickingTogglesState (true);
        howFar[i].onClick = [this, i] { if (howFar[i].getToggleState()) { options.intensity = i; rebuildPlan(); } };
        addAndMakeVisible (howFar[i]);
    }
    howFar[1].setToggleState (true, juce::dontSendNotification);

    roleList = std::make_unique<RoleList> (*this);
    roleViewport.setViewedComponent (roleList.get(), false);
    roleViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (roleViewport);
    planView = std::make_unique<PlanView> (*this);
    planViewport.setViewedComponent (planView.get(), false);
    planViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (planViewport);
    detailView = std::make_unique<DetailView> (*this);
    addAndMakeVisible (*detailView);

    struct Opt { juce::ToggleButton* b; bool Options::* field; };
    for (auto o : { Opt { &optRoute, &Options::route }, Opt { &optLevels, &Options::levels }, Opt { &optChains, &Options::chains },
                    Opt { &optRoom, &Options::makeRoom }, Opt { &optTune, &Options::tune }, Opt { &optPolish, &Options::masterPolish } })
    {
        o.b->setToggleState (options.*(o.field), juce::dontSendNotification);
        auto field = o.field;
        auto* btn = o.b;
        o.b->onClick = [this, btn, field] { options.*field = btn->getToggleState(); rebuildPlan(); };
        addAndMakeVisible (*o.b);
    }

    for (auto* b : { &beforeBtn, &afterBtn })
    {
        b->setRadioGroupId (4102);
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    beforeBtn.onClick = [this] { if (beforeBtn.getToggleState() && showingAfter) showVersion (false); };
    afterBtn.onClick  = [this] { if (afterBtn.getToggleState() && ! showingAfter) showVersion (true); };
    afterBtn.setToggleState (true, juce::dontSendNotification);

    applyBtn.getProperties().set ("primary", true);
    applyBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    applyBtn.onClick = [this] { apply(); };
    addAndMakeVisible (applyBtn);

    setState (State::Listening);
    startListening();
    startTimerHz (15);
}

MixAssistantPanel::~MixAssistantPanel()
{
    alive->store (false);
    stopTimer();
    setLookAndFeel (nullptr);
}

void MixAssistantPanel::startListening()
{
    setState (State::Listening);
    progress->store (0.f);
    auto copy = std::make_shared<Session> (engine.getSession());     // listen to a copy: the session can change meanwhile
    auto aliveFlag = alive;
    auto prog = progress;
    juce::Component::SafePointer<MixAssistantPanel> safe (this);
    juce::Thread::launch ([copy, aliveFlag, prog, safe]
    {
        auto result = std::make_shared<SessionAnalysis> (analyse (*copy, [aliveFlag, prog] (float f) { prog->store (f); return aliveFlag->load(); }));
        juce::MessageManager::callAsync ([safe, result]
        {
            if (safe == nullptr) return;
            safe->analysis = *result;
            safe->roles.clear();
            for (auto& t : safe->analysis.tracks) safe->roles.push_back (t.role);
            safe->roleList->setSize (safe->roleViewport.getWidth() - 10, 10);
            safe->roleList->rebuild();
            safe->setState (State::Ready);
            safe->rebuildPlan();
        });
    });
}

std::vector<const TrackMove*> MixAssistantPanel::directTracks() const
{
    std::vector<const TrackMove*> out;
    for (auto& m : plan.tracks)
        if (m.change && m.toBus.isEmpty() && m.role != Role::Skip && m.role != Role::Other) out.push_back (&m);
    return out;
}

void MixAssistantPanel::rebuildPlan()
{
    if (state == State::Listening) return;
    plan = makePlan (analysis, roles, engine.getSession(), options);
    const bool valid = selected == -2 || selected == -3 || juce::isPositiveAndBelow (selected, (int) plan.buses.size());
    if (! valid || ! everSelected)
    {
        selected = -2;
        for (size_t i = 0; i < plan.buses.size(); ++i) if (! plan.buses[i].isReturn) { selected = (int) i; break; }
        if (selected == -2 && ! directTracks().empty()) selected = -3;
    }
    // re-planning after Apply means a fresh Apply (it replaces the last one)
    if (state == State::Applied) applyBtn.setButtonText ("Apply again");
    planView->repaint();
    detailView->repaint();
    resized();
    repaint();
}

juce::String MixAssistantPanel::headerStatus() const
{
    switch (state)
    {
        case State::Listening: return "Listening... " + juce::String (juce::roundToInt (progress->load() * 100.f)) + "%";
        case State::Applying:  return "Applying - Nova AI Master is listening to the mix... " + juce::String (juce::roundToInt (progress->load() * 100.f)) + "%";
        case State::Failed:    return "Something went wrong - see the status line";
        case State::Ready:
        case State::Applied:   break;
    }
    const int secs = (int) std::lround (analysis.seconds);
    const double sr = engine.getCurrentSampleRate();
    return "Listened to " + juce::String ((int) analysis.tracks.size()) + " tracks" + kDot + juce::String (secs / 60) + ":"
         + juce::String (secs % 60).paddedLeft ('0', 2) + (sr > 0 ? kDot + juce::String (sr / 1000.0, sr == 44100.0 ? 1 : 0) + " kHz" : juce::String())
         + (state == State::Applied ? kDot + applyNote : juce::String());
}

void MixAssistantPanel::setState (State s)
{
    state = s;
    const bool idle = s == State::Ready || s == State::Applied;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &styleBox, &refButton, &howFar[0], &howFar[1], &howFar[2], &optRoute, &optLevels,
                                                                          &optChains, &optRoom, &optTune, &optPolish, &roleViewport })
        c->setEnabled (idle);
    applyBtn.setEnabled (idle);
    applyBtn.setButtonText (s == State::Applying ? "Applying..." : s == State::Applied ? "Apply again" : "Apply mix");
    beforeBtn.setEnabled (s == State::Applied);
    afterBtn.setEnabled (s == State::Applied);
    if (cb.setBusy) cb.setBusy (s == State::Applying);
    repaint();
}

void MixAssistantPanel::apply()
{
    if (state != State::Ready && state != State::Applied) return;

    // Apply again: start from the session as it was before the last Apply
    if (state == State::Applied && beforeSnap.existsAsFile())
    {
        restoreSnapshot (engine, beforeSnap);
        if (cb.sessionChanged) cb.sessionChanged();
        plan = makePlan (analysis, roles, engine.getSession(), options);
    }

    beforeSnap = takeSnapshot (engine, "before");
    if (! beforeSnap.existsAsFile())
    {
        if (cb.status) cb.status ("Mix Assistant: couldn't save a copy of the session to undo to - nothing was changed");
        return;
    }
    setState (State::Applying);
    progress->store (0.f);
    applyResult = applyPlanExceptMaster (engine, plan);
    if (cb.sessionChanged) cb.sessionChanged();

    const PluginStep* masterStep = nullptr;
    for (auto& s : plan.master) if (s.analyseMix) masterStep = &s;
    if (masterStep == nullptr) { finishApply ({}); return; }

    // the slow part: render the mix and let Nova AI Master decide (the rest of the app is blocked meanwhile)
    auto step = std::make_shared<PluginStep> (*masterStep);
    auto aliveFlag = alive;
    auto prog = progress;
    auto* eng = &engine;
    juce::Component::SafePointer<MixAssistantPanel> safe (this);
    juce::Thread::launch ([step, aliveFlag, prog, eng, safe]
    {
        ApplyResult local;
        auto xml = analyseMixForMaster (*eng, *step, local, [prog] (float f) { prog->store (f); return true; });
        juce::MessageManager::callAsync ([safe, xml, local]
        {
            if (safe == nullptr) return;
            for (auto& l : local.log) safe->applyResult.log.add (l);
            safe->applyResult.masterLufsBefore = local.masterLufsBefore;
            safe->finishApply (xml);
        });
    });
}

void MixAssistantPanel::finishApply (const juce::String& masterPlanXml)
{
    for (auto& s : plan.master)
        if (s.analyseMix) loadAnalysedMaster (engine, s, masterPlanXml, applyResult);

    afterSnap = takeSnapshot (engine, "after");
    showingAfter = true;
    afterBtn.setToggleState (true, juce::dontSendNotification);
    applyNote = "applied " + juce::String (applyResult.busesMade) + " buses, " + juce::String (applyResult.tracksChanged)
              + " tracks, " + juce::String (applyResult.pluginsLoaded) + " plugins";
    juce::String msg = "Mix Assistant: " + applyNote + " - one Undo puts it all back";
    if (! applyResult.missingPlugins.isEmpty())
        msg << " (not installed: " << applyResult.missingPlugins.joinIntoString (", ") << ")";
    if (cb.status) cb.status (msg);
    if (cb.sessionChanged) cb.sessionChanged();
    if (cb.applied && afterSnap.existsAsFile()) cb.applied (beforeSnap, afterSnap);
    setState (State::Applied);
}

void MixAssistantPanel::showVersion (bool after)
{
    const auto f = after ? afterSnap : beforeSnap;
    if (! f.existsAsFile()) return;
    restoreSnapshot (engine, f);
    showingAfter = after;
    if (cb.sessionChanged) cb.sessionChanged();
    if (cb.status) cb.status (after ? "Mix Assistant: After (the new mix)" : "Mix Assistant: Before (your mix as it was)");
}

void MixAssistantPanel::sessionSwappedElsewhere()
{
    // the session was swapped by Undo/Redo: the A/B buttons no longer describe it
    if (state == State::Applied)
    {
        beforeSnap = afterSnap = juce::File();
        setState (State::Ready);
    }
}

bool MixAssistantPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    return state != State::Applying && files.size() == 1
        && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;mp3;flac;m4a");
}

void MixAssistantPanel::filesDropped (const juce::StringArray& files, int, int)
{
    const juce::File f (files[0]);
    if (! f.existsAsFile()) return;
    options.reference = f;
    refButton.setButtonText (f.getFileNameWithoutExtension() + "  x");
    rebuildPlan();
}

void MixAssistantPanel::timerCallback()
{
    if (state == State::Listening || state == State::Applying)
    {
        planView->repaint();
        repaint (0, 0, getWidth(), 56);
    }
}

void MixAssistantPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto r = getLocalBounds();

    // header
    auto head = r.removeFromTop (56).reduced (18, 0);
    g.setColour (kText);
    g.setFont (font (14.f, true));
    g.drawText ("MIX", head.removeFromLeft (34), juce::Justification::centredLeft);
    g.setColour (kAccent);
    g.drawText ("ASSISTANT", head.removeFromLeft (96), juce::Justification::centredLeft);
    const bool busy = state == State::Listening || state == State::Applying;
    g.setColour (busy ? kCheck : juce::Colour (0xff3ddc84));
    g.fillEllipse (juce::Rectangle<float> ((float) head.getX(), (float) head.getCentreY() - 3.f, 6.f, 6.f));
    g.setColour (kDim);
    g.setFont (font (12.f));
    g.drawText (headerStatus(), head.withTrimmedLeft (12).withWidth (juce::jmax (100, styleBox.getX() - head.getX() - 24)), juce::Justification::centredLeft, true);

    g.setColour (kFaint);
    g.setFont (font (10.f, true));
    g.drawText ("SOUND LIKE", styleBox.getBounds().translated (0, -15).withHeight (12), juce::Justification::centredLeft);
    g.drawText ("OR MATCH A REFERENCE", refButton.getBounds().translated (0, -15).withHeight (12), juce::Justification::centredLeft);
    g.drawText ("HOW FAR", howFar[0].getBounds().translated (0, -15).withHeight (12).withWidth (100), juce::Justification::centredLeft);

    // column panels + titles
    auto paintColumn = [&] (juce::Rectangle<int> col, const juce::String& title, const juce::String& hint)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (col.toFloat(), 10.f);
        g.setColour (kEdge);
        g.drawRoundedRectangle (col.toFloat().reduced (0.5f), 10.f, 1.f);
        auto t = col.reduced (14, 0).removeFromTop (34);
        g.setColour (kText.withAlpha (0.8f));
        g.setFont (font (10.5f, true));
        g.drawText (title, t, juce::Justification::centredLeft);
        g.setColour (kFaint);
        g.setFont (font (10.5f));
        g.drawText (hint, t, juce::Justification::centredRight);
    };
    paintColumn (leftCol, "WHAT I HEARD", "click a role to change it");
    int groups = 0, returns = 0;
    for (auto& b : plan.buses) (b.isReturn ? returns : groups)++;
    paintColumn (midCol, "THE PLAN", state == State::Listening ? juce::String()
                 : juce::String (groups) + (groups == 1 ? " bus" : " buses") + kDot + juce::String (returns) + (returns == 1 ? " effect return" : " effect returns")
                   + kDot + juce::String (plan.changeCount()) + " changes");
    paintColumn (rightCol, "DETAIL", "what it'll do");

    // footer
    g.setColour (kPanel);
    g.fillRect (footer);
    g.setColour (kEdge);
    g.drawHorizontalLine (footer.getY(), 0.f, (float) getWidth());
    g.setColour (kFaint);
    g.setFont (font (10.5f));
    g.drawText ("One Undo puts it all back", applyBtn.getBounds().translated (0, applyBtn.getHeight() + 2).withHeight (14).expanded (30, 0),
                juce::Justification::centred);
}

void MixAssistantPanel::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (56).reduced (18, 0);
    auto hf = head.removeFromRight (230).withTrimmedTop (22).withHeight (26);
    for (int i = 0; i < 3; ++i) howFar[i].setBounds (hf.removeFromLeft (i == 2 ? 74 : 76).reduced (1, 0));
    head.removeFromRight (14);
    refButton.setBounds (head.removeFromRight (170).withTrimmedTop (22).withHeight (26));
    head.removeFromRight (12);
    styleBox.setBounds (head.removeFromRight (190).withTrimmedTop (22).withHeight (26));

    footer = r.removeFromBottom (66);
    auto f = footer.reduced (18, 0);
    auto applyArea = f.removeFromRight (150);
    applyBtn.setBounds (applyArea.withSizeKeepingCentre (140, 34).translated (0, -6));
    f.removeFromRight (16);
    auto ab = f.removeFromRight (140).withSizeKeepingCentre (136, 28);
    beforeBtn.setBounds (ab.removeFromLeft (68).reduced (1, 0));
    afterBtn.setBounds (ab.reduced (1, 0));
    int x = f.getX();
    for (auto* t : { &optRoute, &optLevels, &optChains, &optRoom, &optTune, &optPolish })
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (font (12.5f), t->getButtonText()) + 34;
        t->setBounds (x, f.getCentreY() - 12, w, 24);
        x += w + 6;
    }

    r = r.reduced (12, 10);
    const int side = juce::jlimit (260, 330, r.getWidth() / 4);
    leftCol = r.removeFromLeft (side);
    r.removeFromLeft (10);
    rightCol = r.removeFromRight (side);
    r.removeFromRight (10);
    midCol = r;

    roleViewport.setBounds (leftCol.withTrimmedTop (34).reduced (2, 4));
    roleList->setSize (roleViewport.getWidth() - (roleViewport.isVerticalScrollBarShown() ? 10 : 0),
                       juce::jmax (10, (int) analysis.tracks.size() * RoleList::kRowH + 8));
    planViewport.setBounds (midCol.withTrimmedTop (34).reduced (2, 4));
    planView->setSize (planViewport.getWidth() - 10, juce::jmax (planViewport.getHeight(), planView->neededHeight));
    detailView->setBounds (rightCol.withTrimmedTop (30));
}

//==============================================================================
MixAssistantWindow::MixAssistantWindow (StudioAudioEngine& e, MixAssistantPanel::Callbacks callbacks, std::function<void()> closed)
    : juce::DocumentWindow ("Mix Assistant", kBg, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
      onClose (std::move (closed))
{
    setUsingNativeTitleBar (true);
    panel = new MixAssistantPanel (e, std::move (callbacks));
    setContentOwned (panel, false);
    setResizable (true, false);
    setResizeLimits (1040, 640, 2400, 1600);
    centreWithSize (1200, 780);
    setVisible (true);
}

bool MixAssistantWindow::isBusy() const
{
    return panel != nullptr && panel->isApplying();
}

void MixAssistantWindow::closeButtonPressed()
{
    if (isBusy()) return;   // finishing an Apply - closing now would leave it half done
    if (onClose) onClose();
}
}
