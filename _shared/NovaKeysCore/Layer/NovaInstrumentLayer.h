#pragma once

#include "../Sampler/NovaSamplerEngine.h"
#include "../Sampler/NovaSamplePlayback.h"
#include "../Synth/NovaSynthVoice.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

// One of LAYER A/B/C/D (spec section 6): wraps EITHER a multisampled
// instrument OR a synthesized sound behind one shared control surface -
// on/off, volume, pan, tune, octave, filter, ADSR, key+velocity range. The
// layer's own ADSR/filter apply uniformly on top of whichever source type
// is configured - this is what makes a sample zone (which has no envelope
// of its own) actually respond to note-off, and it's what gives a synth
// layer its second, layer-level envelope stage on top of the oscillator's
// own (spec section 7 lists ADSR as part of the synth layer too - both are
// real, standard in hybrid workstations, not a duplicate/bug).
//
// This class represents ONE layer's contribution to ONE simultaneously
// triggered note - the polyphony manager (task #93) is expected to own an
// array of these (voices x 4 layers) and call noteOn/noteOff/renderAdding
// on whichever slot a given note is assigned to.
enum class NovaLayerSourceType { Sample, Synth };

struct NovaLayerParams
{
    bool on = true;
    float volumeDb = 0.f;
    float pan = 0.f;             // -1..1
    float tuneCents = 0.f;
    int octaveShift = 0;         // -3..+3

    int lowKey = 0, highKey = 127;
    int lowVelocity = 1, highVelocity = 127;

    float filterCutoffHz = 20000.f; // wide open by default - no coloration unless set
    float filterResonance = 0.7f;
    bool filterHighPass = false;

    float attackMs = 5.f, decayMs = 200.f, sustain = 1.f, releaseMs = 300.f;

    // Filter envelope: sweeps the cutoff (in octaves, relative to
    // filterCutoffHz above) on its own ADSR, independent of the amp
    // envelope above - the standard technique for a "plucked" attack
    // transient (bright pick/pluck that decays to a duller sustain) that
    // amplitude shaping alone cannot produce. filterEnvAmount defaults to
    // 0 (no modulation), so every preset predating this feature is
    // unaffected until explicitly given a nonzero amount.
    float filterEnvAmount = 0.f; // octaves; positive brightens on attack, negative darkens
    float filterEnvAttackMs = 2.f, filterEnvDecayMs = 150.f, filterEnvSustain = 0.f, filterEnvReleaseMs = 100.f;
};

class NovaInstrumentLayer
{
public:
    NovaInstrumentLayer() = default;

    void prepare (double outputSampleRate, int maxBlockSize);

    // Message-thread setup calls (preset load / layer edit).
    void configureAsSample (NovaSamplerEngine* samplerEngine, const NovaLayerParams& p) noexcept;
    void configureAsSynth (const NovaSynthVoiceParams& synthParams, const NovaLayerParams& p) noexcept;
    void setOn (bool shouldBeOn) noexcept { layerParams.on = shouldBeOn; }

    bool matchesRange (int midiNote, int velocity) const noexcept;

    // No-op (silently) if the layer is off or out of range for this
    // note/velocity - callers can unconditionally call noteOn on every
    // layer for a triggered note and let each layer self-gate, which is
    // exactly what spec section 6's key/velocity-range splits require.
    void noteOn (int midiNote, int velocity) noexcept;
    void noteOff() noexcept;
    void reset() noexcept;

    bool isActive() const noexcept { return layerEnvelope.isActive(); }

    // Renders numSamples, ADDING into outL/outR. Realtime-safe: scratch
    // buffers are pre-sized in prepare(), never resized here.
    void renderAdding (float* outL, float* outR, int numSamples) noexcept;

private:
    NovaLayerSourceType sourceType = NovaLayerSourceType::Sample;
    NovaLayerParams layerParams;

    NovaSamplerEngine* samplerEngine = nullptr; // non-owning, shared across voices of this layer
    NovaSamplePlayback samplePlayback;

    NovaSynthVoiceParams synthParams;
    NovaSynthVoice synthVoice;

    juce::ADSR layerEnvelope;
    juce::ADSR filterEnvelope;
    juce::dsp::StateVariableTPTFilter<float> layerFilter;

    std::vector<float> scratchL, scratchR;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaInstrumentLayer)
};
