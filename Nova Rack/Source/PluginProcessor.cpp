#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String bypassId (int i) { return "slot" + juce::String (i + 1) + "_bypass"; }
juce::String mixId (int i)    { return "slot" + juce::String (i + 1) + "_mix"; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::NormalisableRange<float> gainRange { -24.0f, 24.0f, 0.1f };
    auto dbText = juce::AudioParameterFloatAttributes().withLabel ("dB")
                      .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "in_gain", 1 }, "Input", gainRange, 0.0f, dbText));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out_gain", 1 }, "Output", gainRange, 0.0f, dbText));

    for (int i = 0; i < NovaRackAudioProcessor::kMaxSlots; ++i)
    {
        const auto n = juce::String (i + 1);
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypassId (i), 1 }, "Slot " + n + " Bypass", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { mixId (i), 1 }, "Slot " + n + " Mix", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; })));
    }
    return layout;
}

void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plainValue)
{
    if (auto* p = s.getParameter (id))
    {
        const auto v = p->convertTo0to1 (plainValue);
        if (std::abs (p->getValue() - v) > 1.0e-6f)
            p->setValueNotifyingHost (v);
    }
}

//==============================================================================
/** Full-size window for a hosted plugin's own editor (the rack's "full window" button). */
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioPluginInstance& p, const juce::String& title)
        : DocumentWindow (title, juce::Colour::fromRGB (6, 11, 18), DocumentWindow::closeButton), plugin (p)
    {
        setUsingNativeTitleBar (true);
        attachEditor();
        // Hosts don't own this window, so keep it above the DAW instead of losing it behind the arrange page.
        setAlwaysOnTop (true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~PluginWindow() override { clearContentComponent(); }

    void showAgain()
    {
        if (getContentComponent() == nullptr)
            attachEditor();
        setVisible (true);
        toFront (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
        clearContentComponent(); // frees the plugin's editor (its web view) while hidden
    }

private:
    void attachEditor()
    {
        juce::AudioProcessorEditor* ed = plugin.hasEditor() ? plugin.createEditorIfNeeded() : nullptr;
        if (ed == nullptr)
            ed = new juce::GenericAudioProcessorEditor (plugin);
        setContentOwned (ed, true);
        setResizable (ed->isResizable(), false);
    }

    juce::AudioPluginInstance& plugin;
};
} // namespace

//==============================================================================
NovaRackAudioProcessor::NovaRackAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NovaRack", createLayout())
{
    inGainParam = apvts.getRawParameterValue ("in_gain");
    outGainParam = apvts.getRawParameterValue ("out_gain");
    for (int i = 0; i < kMaxSlots; ++i)
    {
        bypassParams[(size_t) i] = apvts.getRawParameterValue (bypassId (i));
        mixParams[(size_t) i] = apvts.getRawParameterValue (mixId (i));
    }
    startTimerHz (10);
}

NovaRackAudioProcessor::~NovaRackAudioProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    {
        const juce::ScopedLock sl (audioLock);
        audioSlots.clear();
    }
    slots.clear();
}

bool NovaRackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return in == layouts.getMainOutputChannelSet();
}

void NovaRackAudioProcessor::prepareSlot (nr::Slot& s)
{
    s.prepare (sampleRate, blockSize, std::max (1, getTotalNumInputChannels()));
}

void NovaRackAudioProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    blockSize = block;
    inGain.reset (sr, 0.03);
    outGain.reset (sr, 0.03);
    inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inGainParam->load()));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outGainParam->load()));

    match.prepare (sr, getTotalNumInputChannels());

    const juce::ScopedLock sl (audioLock);
    for (auto& s : slots)
        prepareSlot (*s);
    isPrepared = true;
    updateLatency(); // report latency straight away
}

void NovaRackAudioProcessor::releaseResources()
{
    const juce::ScopedLock sl (audioLock);
    for (auto& s : slots)
        s->release();
    isPrepared = false;
}

void NovaRackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int nc = getTotalNumInputChannels();
    const int n = buffer.getNumSamples();
    for (int c = nc; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nc == 0 || n == 0)
        return;

    juce::AudioBuffer<float> io (buffer.getArrayOfWritePointers(), nc, n);

    inGain.setTargetValue (juce::Decibels::decibelsToGain (inGainParam->load()));
    inGain.applyGain (io, n);
    inPeak.store (std::max (inPeak.load(), io.getMagnitude (0, n)));
    match.pushCapture (io);   // Nova Match: "Capture" records the rack's input

    {
        const juce::ScopedLock sl (audioLock);
        for (int i = 0; i < audioSlots.size(); ++i)
        {
            const bool bypassed = bypassParams[(size_t) i]->load() > 0.5f;
            const float wet = bypassed ? 0.0f : mixParams[(size_t) i]->load() * 0.01f;
            audioSlots.getUnchecked (i)->process (io, midi, wet, getPlayHead(), isNonRealtime());
        }
    }

    outGain.setTargetValue (juce::Decibels::decibelsToGain (outGainParam->load()));
    outGain.applyGain (io, n);
    match.renderPreview (io); // Nova Match A/B replaces the output while it's on
    outPeak.store (std::max (outPeak.load(), io.getMagnitude (0, n)));
    midi.clear();
}

//==============================================================================
void NovaRackAudioProcessor::publishChain()
{
    juce::Array<nr::Slot*> next;
    for (auto& s : slots)
        next.add (s.get());
    {
        const juce::ScopedLock sl (audioLock);
        audioSlots.swapWith (next);
    }
    updateLatency();
    rackDirty = true;
    ++rackGeneration;
}

void NovaRackAudioProcessor::resolveMissing()
{
    // A session can load before the plugin scan finishes (first launch, new install):
    // once the library knows about a missing plugin, load it with the settings it kept.
    const bool wasDirty = rackDirty;
    bool changed = false;
    for (int i = 0; i < numSlots(); ++i)
    {
        auto* s = slot (i);
        if (s == nullptr || ! s->missing || ! library->find (s->pluginName).has_value())
            continue;

        nr::SlotData sd;
        sd.plugin = s->pluginName;
        sd.state = s->savedState;
        sd.params = s->pendingParams;
        sd.face = s->face;
        sd.preset = s->presetName;
        sd.bypass = getBypass (i);
        sd.mix = getMix (i);

        removeSlot (i);
        if (addSlot (sd.plugin, i, &sd).isEmpty())
            changed = true;
    }
    if (changed)
    {
        rackDirty = wasDirty;
        ++rackGeneration;
    }
}

void NovaRackAudioProcessor::timerCallback()
{
    if (const int gen = library->getGeneration(); gen != seenLibraryGeneration && ! library->isScanning())
    {
        seenLibraryGeneration = gen;
        resolveMissing();
    }
    updateLatency();
}

void NovaRackAudioProcessor::updateLatency()
{
    int latency = 0;
    double tail = 0.0;
    for (auto& s : slots)
    {
        s->refreshLatency();
        latency += s->getLatency();
        if (s->instance != nullptr)
            tail += std::min (30.0, s->instance->getTailLengthSeconds());
    }
    tailSeconds = tail;
    if (latency != getLatencySamples())
        setLatencySamples (latency);
}

bool NovaRackAudioProcessor::getBypass (int i) const
{
    return juce::isPositiveAndBelow (i, kMaxSlots) && bypassParams[(size_t) i]->load() > 0.5f;
}

void NovaRackAudioProcessor::setBypass (int i, bool b)
{
    if (juce::isPositiveAndBelow (i, kMaxSlots)) { setParam (apvts, bypassId (i), b ? 1.0f : 0.0f); rackDirty = true; }
}

float NovaRackAudioProcessor::getMix (int i) const
{
    return juce::isPositiveAndBelow (i, kMaxSlots) ? mixParams[(size_t) i]->load() * 0.01f : 1.0f;
}

void NovaRackAudioProcessor::setMix (int i, float m)
{
    if (juce::isPositiveAndBelow (i, kMaxSlots)) { setParam (apvts, mixId (i), juce::jlimit (0.0f, 1.0f, m) * 100.0f); rackDirty = true; }
}

float NovaRackAudioProcessor::getGainDb (bool output) const
{
    return (output ? outGainParam : inGainParam)->load();
}

void NovaRackAudioProcessor::setGainDb (bool output, float db)
{
    setParam (apvts, output ? "out_gain" : "in_gain", juce::jlimit (-24.0f, 24.0f, db));
    rackDirty = true;
}

//==============================================================================
void NovaRackAudioProcessor::applySlotData (int index, const nr::SlotData& d)
{
    auto* s = slot (index);
    if (s == nullptr)
        return;

    if (d.state.getSize() > 0)
        s->setState (d.state);
    if (s->instance == nullptr)
        s->pendingParams = d.params;
    if (s->instance != nullptr && d.params.size() > 0)
        nr::applyParamTexts (*s->instance, d.params);

    if (s->instance != nullptr)
    {
        juce::Array<int> face;
        for (auto i : d.face)
            if (juce::isPositiveAndBelow (i, s->instance->getParameters().size()))
                face.add (i);
        if (face.isEmpty()) s->chooseDefaultFace();
        else                s->face = face;
    }
    else
        s->face = d.face;

    s->presetName = d.preset;
    setBypass (index, d.bypass);
    setMix (index, d.mix);
    rackDirty = true;
    ++rackGeneration;
}

juce::String NovaRackAudioProcessor::addSlot (const juce::String& pluginName, int index, const nr::SlotData* initial)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const juce::ScopedLock structure (structureLock);
    if (numSlots() >= kMaxSlots)
        return "The rack holds " + juce::String (kMaxSlots) + " plugins.";

    auto s = std::make_unique<nr::Slot>();
    s->pluginName = pluginName;
    juce::String error;

    if (const auto desc = library->find (pluginName))
    {
        s->description = *desc;
        s->instance = library->create (*desc, sampleRate, blockSize, error);
        if (s->instance == nullptr)
            return error.isNotEmpty() ? error : "Couldn't load " + pluginName + ".";
        s->pluginName = desc->name;
    }
    else
    {
        if (initial == nullptr)
            return pluginName + " isn't installed.";
        s->missing = true;          // keep its settings so saving the session doesn't lose them
        error = "missing";
    }

    if (isPrepared)
        prepareSlot (*s);
    if (s->instance != nullptr)
        s->chooseDefaultFace();

    // Slot bypass/mix parameters are positional: shift the later slots' values along with them.
    index = juce::isPositiveAndBelow (index, numSlots() + 1) ? index : numSlots();
    std::vector<std::pair<bool, float>> pos;
    for (int i = 0; i < numSlots(); ++i)
        pos.push_back ({ getBypass (i), getMix (i) });
    pos.insert (pos.begin() + index, { false, 1.0f });

    slots.insert (slots.begin() + index, std::move (s));
    for (int i = 0; i < (int) pos.size(); ++i)
    {
        setBypass (i, pos[(size_t) i].first);
        setMix (i, pos[(size_t) i].second);
    }

    if (initial != nullptr)
        applySlotData (index, *initial);

    publishChain();
    return error;
}

void NovaRackAudioProcessor::removeSlot (int index)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const juce::ScopedLock structure (structureLock);
    if (slot (index) == nullptr)
        return;

    std::vector<std::pair<bool, float>> pos;
    for (int i = 0; i < numSlots(); ++i)
        pos.push_back ({ getBypass (i), getMix (i) });
    pos.erase (pos.begin() + index);
    pos.push_back ({ false, 1.0f });

    auto doomed = std::move (slots[(size_t) index]);
    slots.erase (slots.begin() + index);
    publishChain();          // out of the audio chain before it is destroyed
    doomed.reset();

    for (int i = 0; i < (int) pos.size() && i < kMaxSlots; ++i)
    {
        setBypass (i, pos[(size_t) i].first);
        setMix (i, pos[(size_t) i].second);
    }
}

void NovaRackAudioProcessor::moveSlot (int from, int to)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const juce::ScopedLock structure (structureLock);
    if (slot (from) == nullptr || ! juce::isPositiveAndBelow (to, numSlots()) || from == to)
        return;

    std::vector<std::pair<bool, float>> pos;
    for (int i = 0; i < numSlots(); ++i)
        pos.push_back ({ getBypass (i), getMix (i) });

    auto moving = std::move (slots[(size_t) from]);
    slots.erase (slots.begin() + from);
    slots.insert (slots.begin() + to, std::move (moving));
    const auto p = pos[(size_t) from];
    pos.erase (pos.begin() + from);
    pos.insert (pos.begin() + to, p);

    publishChain();
    for (int i = 0; i < (int) pos.size(); ++i)
    {
        setBypass (i, pos[(size_t) i].first);
        setMix (i, pos[(size_t) i].second);
    }
}

void NovaRackAudioProcessor::clearRack()
{
    JUCE_ASSERT_MESSAGE_THREAD
    const juce::ScopedLock structure (structureLock);
    auto doomed = std::move (slots);
    slots.clear();
    publishChain();
    doomed.clear();
    for (int i = 0; i < kMaxSlots; ++i)
    {
        setBypass (i, false);
        setMix (i, 1.0f);
    }
}

juce::StringArray NovaRackAudioProcessor::loadRack (const nr::RackData& r)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const juce::ScopedLock structure (structureLock);
    clearRack();
    setGainDb (false, r.inGainDb);
    setGainDb (true, r.outGainDb);

    juce::StringArray missing;
    for (const auto& sd : r.slots)
    {
        if (numSlots() >= kMaxSlots)
            break;
        const auto err = addSlot (sd.plugin, -1, &sd);
        if (err.isNotEmpty())
            missing.addIfNotAlreadyThere (sd.plugin);
    }

    rackName = r.name.isNotEmpty() ? r.name : juce::String ("New Rack");
    rackDirty = false;
    ++rackGeneration;
    return missing;
}

nr::SlotData NovaRackAudioProcessor::captureSlot (int index)
{
    nr::SlotData d;
    if (auto* s = slot (index))
    {
        d.plugin = s->pluginName;
        d.state = s->getState();
        if (s->missing)
            d.params = s->pendingParams;   // keep factory values until the plugin shows up
        d.bypass = getBypass (index);
        d.mix = getMix (index);
        d.face = s->face;
        d.preset = s->presetName;
    }
    return d;
}

nr::RackData NovaRackAudioProcessor::captureRack()
{
    nr::RackData r;
    r.name = rackName;
    r.inGainDb = getGainDb (false);
    r.outGainDb = getGainDb (true);
    for (int i = 0; i < numSlots(); ++i)
        r.slots.add (captureSlot (i));
    return r;
}

//==============================================================================
void NovaRackAudioProcessor::openPluginWindow (int index)
{
    auto* s = slot (index);
    if (s == nullptr || s->instance == nullptr)
        return;

    if (auto* w = dynamic_cast<PluginWindow*> (s->window.get()))
    {
        w->showAgain();
        return;
    }
    s->window = std::make_unique<PluginWindow> (*s->instance, s->pluginName + "  |  Nova Rack");
}

void NovaRackAudioProcessor::closeAllWindows()
{
    for (auto& s : slots)
        s->window.reset();
}

//==============================================================================
juce::String NovaRackAudioProcessor::stateJson()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("rack", captureRack().toVar());
    o->setProperty ("dirty", rackDirty);
    return juce::JSON::toString (juce::var (o), true);
}

void NovaRackAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::String json;
    {
        const juce::ScopedLock pl (pendingLock);
        json = pendingState; // a restore is still queued: hand that back unchanged
    }

    if (json.isEmpty())
    {
        // Hosts may ask from any thread; structureLock keeps the chain still while it's read.
        const juce::ScopedLock sl (structureLock);
        json = stateJson();
    }

    dest.reset();
    juce::MemoryOutputStream out (dest, false);
    out.writeString (json);
}

void NovaRackAudioProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream in (data, (size_t) size, false);
    const auto json = in.readString();

    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        { const juce::ScopedLock pl (pendingLock); pendingState.clear(); }
        restoreFromJson (json);
        return;
    }

    { const juce::ScopedLock pl (pendingLock); pendingState = json; }
    triggerAsyncUpdate();
}

void NovaRackAudioProcessor::handleAsyncUpdate()
{
    juce::String json;
    { const juce::ScopedLock pl (pendingLock); json.swapWith (pendingState); }
    if (json.isNotEmpty())
        restoreFromJson (json);
}

void NovaRackAudioProcessor::restoreFromJson (const juce::String& json)
{
    const auto v = juce::JSON::parse (json);
    if (! v.isObject())
        return;
    loadRack (nr::RackData::fromVar (v.getProperty ("rack", {})));
    rackDirty = (bool) v.getProperty ("dirty", false);
}

juce::AudioProcessorEditor* NovaRackAudioProcessor::createEditor()
{
    return new NovaRackAudioProcessorEditor (*this);
}
