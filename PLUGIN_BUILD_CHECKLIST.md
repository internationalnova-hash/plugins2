# Plugin Build Checklist

Use this file for any new plugin added to this repo. It is based on the patterns already used in working plugins such as Nova Silk and Nova Master, plus the macOS delivery issues discovered during Nova Clean.

## 1. Required Repo Structure

Each plugin should contain:

- `CMakeLists.txt`
- `Source/PluginProcessor.cpp`
- `Source/PluginProcessor.h`
- `Source/PluginEditor.cpp`
- `Source/PluginEditor.h`
- `Source/ui/public/index.html`
- `Source/ui/public/js/index.js` when the UI needs JS
- `Source/ui/public/js/juce/index.js` when the web UI talks to JUCE
- `Source/ui/public/js/juce/check_native_interop.js` when using the JUCE web bridge
- `Source/ui/public/n_logo.png` when the branded Nova header is used

Optional:

- `Source/ParameterIDs.hpp`
- `Documentation/`
- `Design/`
- `status.json`

## 2. CMake Baseline

Follow the established repo pattern:

- Disable VST2 explicitly:
  - `JUCE_BUILD_VST2 OFF`
  - `JUCE_VST2_FORMAT OFF`
  - `JUCE_VST2SDK_PATH ""`
- Use `cmake_minimum_required(VERSION 3.15)`
- Use JUCE via `FetchContent`
- Pin JUCE to `8.0.8` unless the repo is intentionally upgraded
- On macOS set:
  - `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"`
  - `CMAKE_OSX_DEPLOYMENT_TARGET "11.0"`

Platform formats should match current repo behavior:

- Windows: `VST3 Standalone`
- macOS: `VST3 AU Standalone`
- Linux: `VST3 LV2 Standalone`

## 3. Plugin Definition Checklist

Every plugin should define:

- `COMPANY_NAME "International Nova"`
- `COMPANY_WEBSITE "https://noizefield.com"`
- A unique `BUNDLE_ID`
- A unique 4-char `PLUGIN_MANUFACTURER_CODE`
- A unique 4-char `PLUGIN_CODE`
- Correct `PRODUCT_NAME`
- Correct `VST3_CATEGORIES`
- `IS_SYNTH FALSE`
- `NEEDS_MIDI_INPUT FALSE` unless intentionally needed
- `NEEDS_MIDI_OUTPUT FALSE` unless intentionally needed
- `IS_MIDI_EFFECT FALSE`
- `EDITOR_WANTS_KEYBOARD_FOCUS FALSE`

## 4. Web UI Embedding Checklist

If the plugin uses the web UI pattern:

- Add all web assets to `juce_add_binary_data(...)`
- Include `index.html`
- Include JS entry files used by the UI
- Include `n_logo.png` if referenced by the HTML
- Link the binary data target into the plugin target
- Define `JUCE_WEB_BROWSER=1`
- Define `JUCE_VST3_CAN_REPLACE_VST2=0`

Platform compile definitions:

- Windows:
  - `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1`
  - `_USE_MATH_DEFINES=1`
- macOS:
  - `JUCE_USE_CURL=0`
- Linux:
  - `JUCE_USE_CURL=0`
  - `JUCE_JACK=1`

## 5. Source Link Checklist

Make sure `target_sources(...)` includes everything actually used by the plugin:

- Processor cpp/h
- Editor cpp/h
- Shared parameter header if present

Make sure `target_link_libraries(...)` includes:

- `juce_audio_basics`
- `juce_audio_devices`
- `juce_audio_formats`
- `juce_audio_plugin_client`
- `juce_audio_processors`
- `juce_audio_utils`
- `juce_core`
- `juce_data_structures`
- `juce_dsp`
- `juce_events`
- `juce_graphics`
- `juce_gui_basics`
- `juce_gui_extra`

Add `juce_opengl` only when needed.

## 5A. Knob Wiring Checklist (Critical)

This is the most common regression area in new plugin builds.

For every interactive knob/slider:

- JS control ID exists in `index.html` and is selected in JS.
- JS drag/gesture handlers are attached (`pointerdown`, `pointermove`, `pointerup/cancel`).
- JS sends value updates to JUCE using the exact parameter id:
  - `__juce__slider<paramId>` with `eventType: 'valueChanged'`.
- `PluginEditor.h` has a matching `juce::WebSliderRelay` for that same `<paramId>`.
- `PluginEditor.cpp` has a matching `juce::WebSliderParameterAttachment` to APVTS parameter `<paramId>`.
- `createWebOptions(...)` includes `.withOptionsFrom(...)` for that relay.
- `PluginProcessor` actually defines APVTS parameter `<paramId>` and reads it in DSP/UI logic.

High-value validation pass before shipping:

- Drag each knob in the plugin UI and verify value readout changes.
- Verify parameter moves in host automation/parameter list.
- Reload plugin instance and verify knob states restore correctly.
- Switch presets/modes and ensure knobs stay synced.

Known failure patterns to catch early:

- UI knob renders but has no pointer drag handler.
- JS emits parameter id that does not exist in APVTS.
- Relay exists but missing `.withOptionsFrom(...)` binding.
- Parameter exists in APVTS but UI uses a different id spelling/case.
- Mapping math mismatch (for example, dB knob displayed with wrong range conversion).

## 6. GitHub Actions Checklist

Every plugin should have dedicated workflows:

- `.github/workflows/build-<plugin>-macos.yml`
- `.github/workflows/build-<plugin>-windows.yml`

Minimum workflow behavior:

- Trigger on plugin path changes
- Clean the plugin build directory
- Configure with CMake
- Build Release
- Upload artifacts

If plugin CMake uses `NEEDS_WEBVIEW2 TRUE` on Windows:

- CI must install/download the `Microsoft.Web.WebView2` package before CMake configure
- CI must pass `JUCE_WEBVIEW2_PACKAGE_LOCATION` to CMake configure
- Do not rely on the runner having this package preinstalled

Do not assume GitHub Actions success means the plugin is ready for users. It only proves build success unless extra validation is added.

## 7. macOS Release Checklist

This is the part that was missing for Nova Clean and must be checked before sending builds to users.

- Build succeeds on macOS runner
- Expected macOS artifacts exist:
  - `.vst3`
  - `.component`
  - `.app`
- Plugin bundle is signed with Developer ID Application, not only ad-hoc signed
- Notarization succeeds with `notarytool`
- Stapling is completed where applicable
- Gatekeeper acceptance is verified with `spctl`
- AU is validated with `auval` or tested in Logic/GarageBand
- VST3 is validated with `pluginval` or tested in a VST3 host/DAW
- Final artifact is a clean release package, not the entire raw build tree

Important:

- A downloaded plugin may also need quarantine removed during local testing:
  - `xattr -dr com.apple.quarantine <plugin-or-app>`

## 7A. Temporary Internal Testing Policy

Current repo reality:

- Nova suite plugins are not yet Developer ID signed
- Nova suite plugins are not yet notarized
- For now, internal testing on macOS is allowed with a manual security bypass step

What this means:

- A successful macOS build is acceptable for internal testing even if it is only ad-hoc signed
- The tester may need to remove quarantine or bypass macOS security manually before loading the plugin
- This is acceptable for internal/private testing only, not final public delivery

Current terminal workaround for internal testing:

- `xattr -dr com.apple.quarantine <plugin-or-app>`

If macOS still blocks the file, use Finder or System Settings to approve opening it, or move/install the plugin manually after quarantine removal.

## 8. Definition Of Done For A New Plugin

A new plugin is not done until all of the following are true:

- Repo structure is complete
- CMake builds on the target platforms
- UI assets are embedded correctly
- Plugin formats are correct per platform
- GitHub Actions workflows exist for the plugin
- macOS artifact is signed and validated before user testing
- The downloaded artifact installs and opens in a real DAW

## 9. Compare Every New Plugin Against These Existing Patterns

Use these plugins as repo references:

- `Nova Silk`
- `Nova Master`
- `Nova Clean` for recent DSP + UI integration work

If a new plugin deviates from these patterns, treat that as intentional and verify the reason before merging or distributing builds.