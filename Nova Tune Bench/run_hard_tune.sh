#!/bin/bash
# Scores every tuner at hard-tune settings (fastest retune, chromatic) on the
# synthetic test vocals plus real dry vocals. Results: results/results.csv,
# rendered audio: results/<label>/.
cd "$(dirname "$0")"
B=build/NovaTuneBench_artefacts/Release/NovaTuneBench
limit() { perl -e 'alarm(400); exec(@ARGV)' "$@"; }

rm -rf results

limit $B run --plugin none --out results synth "$HOME/Nova/dry vocal.wav" "$HOME/Nova/dry vocal 11.wav"

limit $B run --plugin "/Library/Audio/Plug-Ins/VST3/Cymatics Neptune Lite.vst3" --label "Neptune Lite (hard)" \
    --set "Retune Speed=0" --set "Key=C" --set "Scale=Chromatic" \
    --out results synth "$HOME/Nova/dry vocal.wav" "$HOME/Nova/dry vocal 11.wav"

limit $B run --plugin "$HOME/Library/Audio/Plug-Ins/VST3/Nova Pitch.vst3" --label "Nova Pitch (hard)" \
    --set "Retune Speed=0" --set "Humanize=0" --set "Key=0" --set "Scale=0" \
    --out results synth "$HOME/Nova/dry vocal.wav" "$HOME/Nova/dry vocal 11.wav"

limit $B run --plugin "$HOME/Library/Audio/Plug-Ins/VST3/Nova Autotune.vst3" --label "Nova Autotune (hard)" \
    --set "Retune Speed=0" --set "Tolerance=0" --set "Amount=100" --set "Key=0" --set "Scale=0" \
    --out results synth "$HOME/Nova/dry vocal.wav" "$HOME/Nova/dry vocal 11.wav"

limit $B run --plugin "$HOME/Library/Audio/Plug-Ins/VST3/Nova AI Vocal.vst3" --label "Nova AI Vocal tuner (hard)" \
    --set "Tune=1" --set "Key=Chromatic" --set "Clean=0" --set "De-Ess=0" --set "Punch=0" --set "Air=0" \
    --set "Warmth=0" --set "Space=0" --set "Mix=1" \
    --out results synth "$HOME/Nova/dry vocal.wav" "$HOME/Nova/dry vocal 11.wav"
