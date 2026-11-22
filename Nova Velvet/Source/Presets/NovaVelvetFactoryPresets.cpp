#include "NovaVelvetFactoryPresets.h"

const std::vector<NovaVelvetPreset>& getNovaVelvetFactoryPresets()
{
    // timeSelectIndex: 0=Fixed, 1=Fix-Man, 2=Manual. analogModeIndex: 0=Clean, 1=Tube, 2=Vintage.
    static const std::vector<NovaVelvetPreset> presets = {
        { "vocal_smooth_control", "Vocal - Smooth Control", "Vocal",
          -24.0f, 3.0f, 15.0f, 400.0f, 6.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 20.0f, false },

        { "vocal_forward_lead", "Vocal - Forward Lead", "Vocal",
          -20.0f, 5.0f, 8.0f, 250.0f, 10.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 35.0f, false },

        { "vocal_heavy_velvet", "Vocal - Heavy Velvet", "Vocal",
          -28.0f, 6.0f, 5.0f, 180.0f, 14.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 45.0f, false },

        { "rap_front_and_center", "Rap - Front & Center", "Vocal",
          -22.0f, 5.0f, 1.0f, 50.0f, 10.0f, 0, 30.0f, 100.0f, 0.0f, true, 1, 40.0f, false },

        { "bass_smooth_weight", "Bass - Smooth Weight", "Instrument",
          -18.0f, 4.0f, 25.0f, 300.0f, 6.0f, 2, 40.0f, 100.0f, 0.0f, true, 1, 20.0f, false },

        { "acoustic_gentle_control", "Acoustic - Gentle Control", "Instrument",
          -16.0f, 2.5f, 20.0f, 350.0f, 3.0f, 1, 20.0f, 100.0f, 0.0f, true, 0, 10.0f, false },

        { "drums_optical_punch", "Drums - Optical Punch", "Instrument",
          -14.0f, 4.0f, 1.0f, 50.0f, 8.0f, 0, 100.0f, 100.0f, 0.0f, true, 1, 30.0f, false },

        { "mix_gentle_velvet", "Mix - Gentle Velvet", "Mix Bus",
          -10.0f, 2.0f, 30.0f, 500.0f, 2.0f, 1, 20.0f, 100.0f, 0.0f, true, 1, 8.0f, false },
    };
    return presets;
}
