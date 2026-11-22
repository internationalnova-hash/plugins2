# Nova Tune Bench

Scores pitch-correction plugins without needing ears. It hosts any VST3/AU,
renders test vocals through it, then tracks the pitch of input and output
with its own independent YIN tracker (shares no code with any Nova engine).

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DFETCHCONTENT_SOURCE_DIR_JUCE="$HOME/Nova/plugins2/Nova AI Master/build/_deps/juce-src"
cmake --build build -j8
```

Universal binary, so Intel-only plugins can be loaded with `arch -x86_64`.

## Use

- `./run_hard_tune.sh`: scores Neptune Lite, Nova Pitch and Nova Autotune at hard-tune settings. Results go to `results/results.csv`, and the rendered audio goes to `results/<label>/`.
- `NovaTuneBench list <plugin>`: shows the plugin's parameter names.
- `NovaTuneBench synth <dir>`: writes the synthetic test vocals, which are off-key scales, vibrato, slides and a fast run.
- `NovaTuneBench run --plugin <path|none> --set "Name=value" ... --out <dir> synth <file.wav>...`
- `NovaTuneBench score --in dry.wav --out tuned.wav [--lagms N]`: scores a render made in a DAW.
- `NovaTuneBench track --in dry.wav --out tuned.wav [--lagms N]`: prints input and output notes every 50 ms.
- Set `TUNEBENCH_DEBUG=1` to print each snap event and each click time.

## Metrics

| Column | Meaning | Good hard tune |
|---|---|---|
| inTune% | Settled frames within 10 cents of the note | > 90 |
| err c | Median distance from the note, in cents | < 3 |
| warbl | Median wobble (std dev, cents) on held notes | low; natural modes keep vibrato |
| wrong% | Settled frames on a different note than the nearest scale note | < 2 |
| oct% | The part of wrong% that is octave jumps | ~0 |
| snapMs | Time to land within 15 cents after a note change | < 20 |
| lost% | Clearly sung input that comes out no longer clearly pitched (garble) | < 2 |
| rough | Median added aperiodicity; above 0 means rougher or more metallic | ~0 |
| clk/m | New clicks per minute. Consonants moved by ±10 ms are ignored | 0 |
| latency r/m | Latency the plugin reports to the DAW / latency the probe measures | equal |

Latency is measured with an in-tune probe note, cross-correlated across its onset.
