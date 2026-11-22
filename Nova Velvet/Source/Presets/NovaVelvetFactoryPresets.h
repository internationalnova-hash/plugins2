#pragma once

#include <vector>
#include "NovaVelvetPreset.h"

// Small, deliberately curated factory bank (spec section 39: "create a
// SMALL useful initial preset bank... do not create 50 filler presets") -
// the exact 8 examples the spec itself names, demonstrating the compressor
// across its real use cases rather than padding out a list.
const std::vector<NovaVelvetPreset>& getNovaVelvetFactoryPresets();
