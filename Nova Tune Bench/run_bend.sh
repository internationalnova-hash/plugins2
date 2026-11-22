#!/bin/bash
# Scores a Nova Bend build: fixed pitch shifts and Flatten.
# usage: ./run_bend.sh <plugin.vst3> <label>
cd "$(dirname "$0")"
B=build/NovaTuneBench_artefacts/Release/NovaTuneBench
P="$1"; L="$2"
IN=(synth "$HOME/Nova/dry vocal.wav" "$HOME/Nova/dry vocal 11.wav")
$B run --plugin "$P" --label "$L shift +5"  --set "Pitch=5"  --shift 5  --out results_bend "${IN[@]}"
$B run --plugin "$P" --label "$L shift -7"  --set "Pitch=-7" --shift -7 --out results_bend "${IN[@]}"
$B run --plugin "$P" --label "$L shift +12" --set "Pitch=12" --shift 12 --out results_bend "${IN[@]}"
$B run --plugin "$P" --label "$L flatten C3" --set "Flatten=1n" --set "Flatten Note=C" --set "Flatten Octave=3" --flatten-midi 48 --out results_bend "${IN[@]}"
