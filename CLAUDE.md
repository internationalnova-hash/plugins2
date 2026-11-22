# Nova Audio Plugin Development — Claude Instructions

This repo contains JUCE audio plugins built by Nova Audio. Read this file every session before writing any code.

---

## Build System

- **JUCE version**: 8.0.8 (fetched via CMake FetchContent from `https://github.com/juce-framework/JUCE.git`)
- **CMake minimum**: 3.15
- **Formats**: VST3 + Standalone on Windows/Linux, VST3 + AU + Standalone on macOS

### Build commands (run from inside the plugin folder, e.g. `Juice Gang/`):

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# VST3 output lands in:
# Windows: build/JuiceGang_artefacts/Release/VST3/Juice Gang.vst3
# macOS:   build/JuiceGang_artefacts/Release/VST3/Juice Gang.vst3
```

### Install VST3 to DAW folder (Windows):
```bash
cp -r "build/JuiceGang_artefacts/Release/VST3/Juice Gang.vst3" "$APPDATA/VST3/"
```

---

## CMakeLists Rules

These rules are **mandatory** — never deviate:

- `COMPANY_NAME` must always be `"Nova Audio"`
- `COMPANY_WEBSITE` must always be `""` (leave blank — audio website not created yet)
- `JUCE_WEB_BROWSER=0` — disable the built-in web browser module
- `JUCE_USE_CURL=0` — disable curl
- `JUCE_VST3_CAN_REPLACE_VST2=0`
- Each plugin needs a unique 4-character `PLUGIN_MANUFACTURER_CODE` (`"NvAu"`) and `PLUGIN_CODE`
- Link against: `juce_audio_basics`, `juce_audio_devices`, `juce_audio_formats`, `juce_audio_plugin_client`, `juce_audio_processors`, `juce_audio_utils`, `juce_core`, `juce_data_structures`, `juce_dsp`, `juce_events`, `juce_graphics`, `juce_gui_basics`, `juce_gui_extra`
- Always include: `juce_recommended_config_flags`, `juce_recommended_lto_flags`, `juce_recommended_warning_flags`
- On Windows add: `target_compile_definitions(... _USE_MATH_DEFINES=1)`

---

## DSP Architecture (Juice Gang Pattern)

All plugins follow the Juice Gang DSP pattern. These rules prevent audio dropout, NaN blowup, and thread stalls.

### Parameter layout
- Use `AudioParameterBool` for stage on/off switches (filter, delay, reverb) — default `false` so stages are OFF at load
- Use `AudioParameterFloat` with `NormalisableRange` for continuous params
- Global bypass is an `AudioParameterBool("bypass", "Bypass", false)`

### processBlock rules

1. **Read all params once at the top of the block** — never call `getRawParameterValue` inside a sample loop
2. **Gate every DSP stage** — only run filter/delay/reverb if their bool param is true
3. **Reset DSP on stage transition only** — track `prevFilterActive`, `prevDelayActive`, `prevReverbActive`; call `.reset()` only when transitioning from active→inactive, NOT every block (resetting a DelayLine every block zeroes ~350KB and stalls the audio thread)
4. **Filter coefficients once per block** — call `setCutoffFrequency` / `setResonance` once, then loop `processSample` per sample
5. **Clamp StateVariableTPTFilter resonance to Q ≤ 4.0** — above Q≈4 the filter self-oscillates and produces NaN
6. **Clamp cutoff to 0.40 × sampleRate** — Nyquist instability kills audio near 20kHz at 44.1kHz SR
7. **Reverb on full block** — `juce::Reverb::processStereo(dataL, dataR, N)` takes the whole block, not one sample at a time
8. **NaN safety at output** — `juce::jlimit` does NOT catch NaN (IEEE 754 comparisons with NaN always return false). Use:
   ```cpp
   dataL[i] = std::isfinite(l) ? juce::jlimit(-1.f, 1.f, l) : 0.f;
   ```
9. **Dry buffer** — save dry signal before processing using:
   ```cpp
   dryBuf.setSize(nCh, N, false, false, true);  // keepExistingContent=true avoids realloc on audio thread
   dryBuf.makeCopyOf(buf, true);
   ```
10. **SmoothedValue** — call `getNextValue()` once per sample inside the sample loop (not once per block). Or use `skip(N)` to advance N steps at once when you only need the final value.
11. **Input/output gain** — use `juce::Decibels::decibelsToGain(param->load())`, apply input gain before saving dry buf, apply output gain after wet/dry blend

### Standard processBlock skeleton

```cpp
void Processor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (*apvts.getRawParameterValue("bypass") > 0.5f) return;

    const int N   = buf.getNumSamples();
    const int nCh = buf.getNumChannels();
    float* dataL  = buf.getWritePointer(0);
    float* dataR  = nCh > 1 ? buf.getWritePointer(1) : dataL;

    // Read params once
    const bool  filterOn  = *apvts.getRawParameterValue("filterOn")  > 0.5f;
    const bool  delayOn   = *apvts.getRawParameterValue("delayOn")   > 0.5f;
    const bool  reverbOn  = *apvts.getRawParameterValue("reverbOn")  > 0.5f;
    const float cutoffHz  = *apvts.getRawParameterValue("cutoff");
    const float resonance = *apvts.getRawParameterValue("resonance");
    const float inGain    = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("masterIn")->load());
    const float outGain   = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("masterOut")->load());
    const float masterMix = *apvts.getRawParameterValue("masterMix") / 100.f;

    // Reset on transition only
    if (!filterOn && prevFilterActive) { filterL.reset(); filterR.reset(); }
    if (!delayOn  && prevDelayActive)  { delayLine.reset(); }
    if (!reverbOn && prevReverbActive) { reverb.reset(); }
    prevFilterActive = filterOn;
    prevDelayActive  = delayOn;
    prevReverbActive = reverbOn;

    // Input gain
    for (int ch = 0; ch < nCh; ++ch)
        juce::FloatVectorOperations::multiply(buf.getWritePointer(ch), inGain, N);

    // Save dry
    dryBuf.setSize(nCh, N, false, false, true);
    dryBuf.makeCopyOf(buf, true);

    // Filter
    if (filterOn) {
        const float safeMax = (float)(currentSR * 0.40f);
        const float safeCut = juce::jmin(cutoffHz, safeMax);
        const float safeRes = juce::jlimit(0.1f, 4.0f, resonance);
        filterL.setCutoffFrequency(safeCut); filterL.setResonance(safeRes);
        filterR.setCutoffFrequency(safeCut); filterR.setResonance(safeRes);
        for (int i = 0; i < N; ++i) {
            dataL[i] = filterL.processSample(0, dataL[i]);
            dataR[i] = filterR.processSample(0, dataR[i]);
        }
    }

    // Delay (per sample)
    if (delayOn) { /* ... popSample / pushSample loop ... */ }

    // Reverb (full block)
    if (reverbOn) {
        // set params, then:
        if (nCh >= 2) reverb.processStereo(dataL, dataR, N);
        else          reverb.processMono(dataL, N);
    }

    // Wet/dry blend
    if (masterMix < 0.999f) {
        const float dry = 1.f - masterMix;
        for (int ch = 0; ch < nCh; ++ch) {
            auto* w = buf.getWritePointer(ch);
            const auto* d = dryBuf.getReadPointer(ch);
            for (int i = 0; i < N; ++i)
                w[i] = w[i] * masterMix + d[i] * dry;
        }
    }

    // Output gain + NaN-safe hard limiter
    for (int i = 0; i < N; ++i) {
        const float l = dataL[i] * outGain;
        const float r = dataR[i] * outGain;
        dataL[i] = std::isfinite(l) ? juce::jlimit(-1.f, 1.f, l) : 0.f;
        dataR[i] = std::isfinite(r) ? juce::jlimit(-1.f, 1.f, r) : 0.f;
    }
}
```

---

## UI Architecture

Juice Gang uses a **native JUCE UI** (no web view). Key patterns:

- `juce::AudioProcessorValueTreeState` (apvts) with `SliderAttachment` / `ButtonAttachment` to bind UI controls to parameters
- `juce::Slider`, `juce::ToggleButton`, `juce::ComboBox` for controls
- Custom `LookAndFeel` for visual styling
- The editor reads `processor.apvts` directly — never cache parameter values in the editor

---

## Plugin Inventory

| Plugin | Folder | PLUGIN_CODE | Description |
|--------|--------|-------------|-------------|
| Juice Gang | `Juice Gang/` | `JcGg` | Filter + delay + reverb multi-FX |
| Space By Nova | `Space By Nova/` | (check CMakeLists) | Reverb/space plugin |
| Nova Motion FX | `Nova Motion FX/` | (check CMakeLists) | Motion macro multi-FX with WebUI |

---

## Known Issues & History

- **StateVariableTPTFilter above Q=4** self-oscillates → NaN → silence. Always clamp resonance.
- **jlimit does not catch NaN** — always guard with `std::isfinite()` before limiting.
- **DelayLine.reset() every block** causes audio thread stalls (~350KB zeroed each call). Reset on transition only.
- **Filter coefficients per sample** causes instability — set once per block only.
- **juce::Reverb** must process full blocks — do not call processStereo with N=1 in a loop.
- **COMPANY_WEBSITE must be blank** — do not set it until the Nova Audio website exists.
