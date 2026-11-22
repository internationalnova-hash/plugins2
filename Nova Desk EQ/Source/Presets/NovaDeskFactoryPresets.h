#pragma once

#include <vector>

#include "NovaDeskPreset.h"

// The factory preset library (build spec sections 3-29): 26 hand-authored
// starting points across 7 categories. Pure data - see NovaDeskPreset.h's
// comment. Defined once in NovaDeskFactoryPresets.cpp and never mutated;
// NovaDeskPresetManager only reads from it.
const std::vector<NovaDeskPreset>& getNovaDeskFactoryPresets();
