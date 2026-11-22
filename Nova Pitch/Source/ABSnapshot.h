#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// A/B snapshot system - identical pattern to Nova 7even's ABSnapshot
// (the first plugin in this catalog to need one); copied verbatim rather
// than promoted to _shared/ since it's still only used by two plugins.
// Intentionally simple: no undo stack, no more than 2 slots.
//
// Both snapshots are embedded as extra child XML elements alongside the
// live apvts state in getStateInformation's XML, so a DAW session
// restores both A and B, not just whichever is currently active.
class ABSnapshot
{
public:
    enum class Slot { A, B };

    Slot getActiveSlot() const noexcept { return activeSlot; }
    bool hasSnapshot (Slot slot) const noexcept { return getState (slot).isValid(); }

    // Saves the processor's current live parameter state into whichever
    // slot is currently active.
    void captureActive (juce::AudioProcessorValueTreeState& apvts) noexcept
    {
        getState (activeSlot) = apvts.copyState();
    }

    // The classic "A/B" button: switches to the other slot and recalls
    // its stored state into the live processor. If that slot has never
    // been captured, this is a no-op (the live state simply stays as-is)
    // rather than resetting to defaults - matches how most A/B systems
    // treat an empty slot.
    void toggleAndRecall (juce::AudioProcessorValueTreeState& apvts)
    {
        activeSlot = (activeSlot == Slot::A) ? Slot::B : Slot::A;
        recallActive (apvts);
    }

    void recallActive (juce::AudioProcessorValueTreeState& apvts) const
    {
        const auto& state = getState (activeSlot);
        if (state.isValid())
            apvts.replaceState (state.createCopy());
    }

    // Overwrites the OTHER slot with a copy of the active slot's state -
    // "start B from what A currently sounds like."
    void copyActiveToOther() noexcept
    {
        getState (activeSlot == Slot::A ? Slot::B : Slot::A) = getState (activeSlot).createCopy();
    }

    // Embeds both snapshots (if captured) as wrapper child elements under
    // an already-created XML element (typically the same XmlElement the
    // caller is about to pass to copyXmlToBinary for getStateInformation).
    void addToXml (juce::XmlElement& parent) const
    {
        addSlotToXml (parent, "ABSnapshotA", stateA);
        addSlotToXml (parent, "ABSnapshotB", stateB);
    }

    void restoreFromXml (const juce::XmlElement& parent)
    {
        restoreSlotFromXml (parent, "ABSnapshotA", stateA);
        restoreSlotFromXml (parent, "ABSnapshotB", stateB);
    }

private:
    juce::ValueTree& getState (Slot slot) noexcept { return slot == Slot::A ? stateA : stateB; }
    const juce::ValueTree& getState (Slot slot) const noexcept { return slot == Slot::A ? stateA : stateB; }

    static void addSlotToXml (juce::XmlElement& parent, const char* wrapperTag, const juce::ValueTree& state)
    {
        if (! state.isValid())
            return;
        if (auto inner = state.createXml())
        {
            auto wrapper = std::make_unique<juce::XmlElement> (wrapperTag);
            wrapper->addChildElement (inner.release());
            parent.addChildElement (wrapper.release());
        }
    }

    static void restoreSlotFromXml (const juce::XmlElement& parent, const char* wrapperTag, juce::ValueTree& state)
    {
        if (auto* wrapper = parent.getChildByName (wrapperTag))
            if (auto* inner = wrapper->getFirstChildElement())
                state = juce::ValueTree::fromXml (*inner);
    }

    Slot activeSlot = Slot::A;
    juce::ValueTree stateA;
    juce::ValueTree stateB;
};
