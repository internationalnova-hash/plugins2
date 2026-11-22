# Nova Rack

One plugin that hosts a chain of Nova plugins. It's a 500-series-style rack, in the spirit of Slate VMR and TVB Rack. Each module wears a faceplate built from that plugin's own artwork. **FULL SCREEN** opens the real plugin at full size. Load it on a vocal, a bus or the master.

## What it does

- **Chain up to 8 Nova plugins.** Click **+** between modules to insert one there. Drag a module by its header to reorder.
- **Per-module controls.** Each module has a power LED (bypass), a mix knob and a preset picker. Its main controls sit on the faceplate. Click a control's name to swap which parameter it shows.
- **Rack presets.** These save the whole chain, with every plugin's full state. Start from the factory racks: vocal chains by genre, analog-only chains, and mix bus and master chains.
- **Module presets.** These save one plugin's settings. Saved presets for every Nova plugin collect in one browser, and clicking one adds that plugin to the rack already set up.
- **Latency.** Each module's dry path is delayed to match its plugin's latency, so mix and bypass stay in time. The rack reports the total latency to the host.
- **Automation.** Rack input/output gain and each slot's bypass and mix are automatable.
- **Missing plugins.** If a session uses a plugin that isn't installed, its settings stay in the rack. If the plugin is installed later, it loads with those settings.

Presets are stored in `~/Library/Application Support/Nova Audio/Nova Rack/` on macOS and `%APPDATA%\Nova Audio\Nova Rack\` on Windows, in the `Racks/` and `Modules/<plugin>/` folders. The plugin scan cache is kept there too.

## How it hosts

Nova plugins are hosted as **VST3 on every platform**, so rack presets move between macOS and Windows unchanged. Only files named like Nova plugins are opened. Instruments and Nova Rack itself are skipped. A plugin that crashes during a scan is skipped on the next scan.

## Factory racks

Factory racks live in `Source/FactoryRacks.cpp`. Values are written as each plugin's own text (`"-3"`, `"1/8D"`, `"Vintage"`) and matched by parameter name. After changing them, check them against the installed plugins:

```bash
cmake --build "Nova Rack/build" --target NovaRackParamDump
"Nova Rack/build/NovaRackParamDump_artefacts/Release/NovaRackParamDump" --verify-factory
```

Running `NovaRackParamDump [name]` with no flag prints every installed Nova plugin's parameters. Use that output when writing new racks.

## Faceplates

Module skins are in `Source/ui/public/js/index.js` (`SKINS`, `PLUGIN_SKINS`). The artwork in `Source/ui/public/skins/` is resized from each plugin's own `Source/ui/public` images, and everything in that folder is embedded automatically. Plugins without a dedicated skin get the modern Nova faceplate.

## Build

```bash
cmake -S "Nova Rack" -B "Nova Rack/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "Nova Rack/build" --config Release
"Nova Rack/build/NovaRackTests_artefacts/Release/NovaRackTests"
```

- **License gate.** `-DNOVA_RACK_REQUIRE_LICENSE=ON` shows the Nova unlock screen (app id `nova-rack`). It's off for local builds; the GitHub workflows build with it on.
- **UI preview.** Serve `Source/ui/public` over HTTP to preview the UI in a browser with mock data. The mock files aren't shipped in the plugin.
