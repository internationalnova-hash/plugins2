#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ─── Preset data ─────────────────────────────────────────────────────────────
struct JuicePreset
{
    const char* name;
    const char* category;   // emoji + name
    // Filter
    bool  filterOn;
    int   filterMode;       // 0=LP 1=HP 2=BP 3=Notch
    float cutoff;
    float resonance;
    float drive;
    float filterMix;
    // Delay
    bool  delayOn;
    float delayTime;
    float delayFb;
    float delayMix;
    // Reverb
    bool  reverbOn;
    float reverbSize;
    float reverbDecay;
    float reverbMix;
    // SIP + master
    float sip;
    float masterMix;
};

// ─────────────────────────────────────────────────────────────────────────────
static const JuicePreset kJuicePresets[] =
{
    // ── 🍊 Citrus (Transitions) ──────────────────────────────────────────────
    { "Lemon Drop",      "🍊 Citrus",  true,  0,  400,  45,  0,  100,  false, 0.25f, 30,  0,   false, 50, 2.0f, 0,   0,   100 },
    { "Orange Peel",     "🍊 Citrus",  true,  0,  800,  55, 10,  100,  false, 0.25f, 30,  0,   false, 50, 2.0f, 0,   0,   100 },
    { "Citrus Splash",   "🍊 Citrus",  true,  0, 1200,  65, 15,  100,  true,  0.25f, 40, 30,   false, 50, 2.0f, 0,   0,   100 },
    { "Sour Sweep",      "🍊 Citrus",  true,  0,  300,  80, 20,  100,  false, 0.25f, 30,  0,   false, 50, 2.0f, 0,   0,   100 },
    { "Fresh Squeeze",   "🍊 Citrus",  true,  0, 2000,  30,  0,  100,  false, 0.5f,  25,  0,   true,  40, 1.5f, 20,  0,   100 },
    { "Vitamin C",       "🍊 Citrus",  true,  1, 3000,  40,  0,  100,  false, 0.25f, 30,  0,   false, 50, 2.0f, 0,   0,   100 },
    { "Citrus Fade",     "🍊 Citrus",  true,  0,  600,  35,  5,  100,  true,  0.5f,  35, 25,   true,  30, 1.2f, 18,  0,   100 },
    { "Peel Back",       "🍊 Citrus",  true,  1, 5000,  50,  0,  100,  false, 0.25f, 30,  0,   false, 50, 2.0f, 0,   0,   100 },
    { "Juiced Out",      "🍊 Citrus",  true,  0,  200,  90, 30,  100,  true,  0.25f, 55, 40,   true,  60, 3.0f, 35,  0,   100 },
    { "Sunset Filter",   "🍊 Citrus",  true,  0, 1500,  20,  0,  100,  false, 0.5f,  20,  0,   true,  50, 2.5f, 25,  0,   100 },

    // ── 🍇 Berry (Vocals) ────────────────────────────────────────────────────
    { "Grape Escape",    "🍇 Berry",   true,  0, 8000,  25,  0,  100,  true,  0.375f,35, 30,   true,  55, 2.5f, 30,  0,   100 },
    { "Berry Smooth",    "🍇 Berry",   true,  0,12000,  15,  0,  100,  true,  0.5f,  30, 25,   true,  40, 1.8f, 22,  0,   100 },
    { "Purple Rain",     "🍇 Berry",   true,  0,16000,  10,  0,  100,  true,  0.25f, 40, 20,   true,  80, 5.0f, 45,  0,   100 },
    { "Blackberry Air",  "🍇 Berry",   true,  1, 2000,  30,  0,  100,  false, 0.25f, 30,  0,   true,  70, 4.0f, 40,  0,   100 },
    { "Cherry Space",    "🍇 Berry",   true,  0,10000,  20,  5,  100,  true,  0.25f, 45, 28,   true,  65, 3.5f, 35,  0,   100 },
    { "Blueberry Mist",  "🍇 Berry",   true,  0, 6000,  18,  0,  100,  false, 0.25f, 30,  0,   true,  75, 4.5f, 50,  0,   100 },
    { "Raspberry Echo",  "🍇 Berry",   false, 0, 2000,   0,  0,  100,  true,  0.375f,50, 35,   true,  50, 2.0f, 28,  0,   100 },
    { "Sweet Harmony",   "🍇 Berry",   true,  0,14000,   8,  0,  100,  true,  0.5f,  28, 20,   true,  45, 2.0f, 25,  0,   100 },
    { "Vocal Drip",      "🍇 Berry",   true,  0, 9000,  22,  3,  100,  true,  0.25f, 40, 30,   true,  55, 2.5f, 30,  0,   100 },
    { "Candy Verse",     "🍇 Berry",   true,  0,11000,  30,  8,  100,  true,  0.125f,35, 22,   true,  40, 1.5f, 20,  0,   100 },

    // ── 🍉 Tropical (Big FX) ─────────────────────────────────────────────────
    { "Mango Madness",   "🍉 Tropical",true,  0,  500,  75, 40,  100,  true,  0.5f,  60, 50,   true,  80, 5.0f, 55,  30,  100 },
    { "Pineapple Wave",  "🍉 Tropical",true,  0,  800,  70, 30,  100,  true,  0.75f, 55, 45,   true,  75, 4.5f, 50,  25,  100 },
    { "Coconut Dream",   "🍉 Tropical",false, 0, 2000,   0,  0,  100,  true,  1.0f,  50, 40,   true,  90, 7.0f, 60,  15,  100 },
    { "Island Drop",     "🍉 Tropical",true,  0,  300,  85, 50,  100,  true,  0.5f,  65, 55,   true,  85, 6.0f, 60,  40,  100 },
    { "Passionfruit",    "🍉 Tropical",true,  2, 2000,  60, 20,  100,  true,  0.375f,45, 35,   true,  70, 4.0f, 45,  20,  100 },
    { "Paradise",        "🍉 Tropical",false, 0, 2000,   0,  0,  100,  true,  2.0f,  40, 35,   true, 100, 8.0f, 70,  10,  100 },
    { "Tropical Storm",  "🍉 Tropical",true,  0,  200,  95, 60,  100,  true,  0.25f, 70, 60,   true,  80, 5.0f, 55,  50,  100 },
    { "Ocean Mist",      "🍉 Tropical",true,  0, 4000,  30,  0,  100,  true,  0.75f, 35, 30,   true,  90, 7.0f, 65,   5,  100 },
    { "Summer Juice",    "🍉 Tropical",true,  0, 1000,  50, 15,  100,  true,  0.5f,  50, 40,   true,  65, 3.5f, 40,  35,  100 },
    { "Beach Fade",      "🍉 Tropical",true,  0, 3000,  20,  0,  100,  true,  1.0f,  30, 25,   true,  80, 5.5f, 55,   8,  100 },

    // ── 🍎 Orchard (Subtle Mix FX) ───────────────────────────────────────────
    { "Apple Polish",    "🍎 Orchard", true,  0,16000,   5,  0,  100,  false, 0.25f, 20,  0,   true,  20, 0.8f, 12,  0,   100 },
    { "Crisp Cut",       "🍎 Orchard", true,  1, 1500,  15,  0,  100,  false, 0.25f, 20,  0,   false, 30, 1.0f,  0,  0,   100 },
    { "Fresh Air",       "🍎 Orchard", true,  1, 3000,   8,  0,  100,  false, 0.25f, 15,  0,   true,  25, 1.0f, 15,  0,   100 },
    { "Pear Pressure",   "🍎 Orchard", true,  0, 8000,  12,  0,  100,  true,  0.5f,  20, 15,   false, 30, 1.2f,  0,  0,   100 },
    { "Honeycrisp",      "🍎 Orchard", true,  0,14000,   8,  0,  100,  true,  0.25f, 18, 12,   true,  20, 0.8f, 10,  0,   100 },
    { "Orchard Glow",    "🍎 Orchard", false, 0, 2000,   0,  0,  100,  false, 0.5f,  20,  0,   true,  35, 1.5f, 18,  0,   100 },
    { "Morning Dew",     "🍎 Orchard", true,  1, 2500,  10,  0,  100,  false, 0.25f, 15,  0,   true,  30, 1.2f, 14,  0,   100 },
    { "Clean Pour",      "🍎 Orchard", false, 0, 2000,   0,  0,  100,  false, 0.25f, 15,  0,   true,  15, 0.6f,  8,  0,   100 },
    { "Sweet Balance",   "🍎 Orchard", true,  0,10000,  10,  0,  100,  true,  0.375f,18, 12,   true,  25, 1.0f, 12,  0,   100 },
    { "Soft Blend",      "🍎 Orchard", true,  0,12000,   8,  0,  100,  false, 0.5f,  20,  0,   true,  20, 0.8f, 10,  0,   100 },

    // ── 🍒 Candy (Creative) ──────────────────────────────────────────────────
    { "Slushie",         "🍒 Candy",   true,  2, 1000,  80, 30,  100,  true,  0.125f,65, 50,   true,  40, 1.5f, 35,  0,   100 },
    { "Sour Patch",      "🍒 Candy",   true,  3, 2000,  90,  0,  100,  true,  0.0625f,55,40,   false, 30, 1.0f,  0,  0,   100 },
    { "Bubblegum",       "🍒 Candy",   true,  0,  800,  70, 25,  100,  true,  0.125f,60, 45,   true,  45, 2.0f, 30,  0,   100 },
    { "Cotton Candy",    "🍒 Candy",   true,  0, 6000,  45, 10,  100,  true,  0.25f, 40, 30,   true,  55, 2.5f, 40,  0,   100 },
    { "Freeze Pop",      "🍒 Candy",   true,  1, 4000,  55,  0,  100,  true,  0.0625f,45,35,   false, 20, 0.8f,  0,  0,   100 },
    { "Neon Juice",      "🍒 Candy",   true,  2, 1500,  85, 20,  100,  true,  0.125f,70, 55,   true,  50, 2.0f, 40,  0,   100 },
    { "Arcade",          "🍒 Candy",   true,  3, 3000,  75,  0,  100,  true,  0.125f,60, 45,   false, 25, 1.0f,  0,  0,   100 },
    { "Sugar Rush",      "🍒 Candy",   true,  0,  500,  80, 40,  100,  true,  0.125f,65, 55,   true,  35, 1.5f, 25,  0,   100 },
    { "Rainbow Splash",  "🍒 Candy",   true,  2, 2000,  75, 15,  100,  true,  0.25f, 55, 45,   true,  60, 3.0f, 45,  0,   100 },
    { "Hyper Berry",     "🍒 Candy",   true,  0,  300,  95, 60,  100,  true,  0.0625f,75,60,   true,  45, 2.0f, 40,  0,   100 },

    // ── 🥤 SIP (Macro) ───────────────────────────────────────────────────────
    { "Tiny Sip",        "🥤 SIP",     true,  0, 2000,  10,  0,  100,  false, 0.25f, 30,  0,   false, 30, 1.0f,  0,  5,   100 },
    { "Big Gulp",        "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.5f,  40, 30,   true,  50, 2.0f, 25,  35,  100 },
    { "Chug",            "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.25f, 55, 45,   true,  70, 4.0f, 45,  60,  100 },
    { "Bottoms Up",      "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.25f, 65, 55,   true,  85, 6.0f, 60,  80,  100 },
    { "Last Drop",       "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.5f,  30, 20,   true,  40, 2.0f, 30,  15,  100 },
    { "Refill",          "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.25f, 35, 25,   true,  45, 2.5f, 30,  20,  100 },
    { "Double Cup",      "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.25f, 50, 40,   true,  60, 3.5f, 40,  45,  100 },
    { "Full Carton",     "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.5f,  60, 50,   true,  75, 5.0f, 55,  70,  100 },
    { "Overfilled",      "🥤 SIP",     true,  0, 2000,  10,  0,  100,  true,  0.25f, 70, 60,   true,  90, 7.0f, 65, 100,  100 },
    { "Empty Carton",    "🥤 SIP",     false, 0, 2000,   0,  0,  100,  false, 0.25f, 30,  0,   false, 30, 1.0f,  0,   0,  100 },

    // ── 🌊 Space (Reverb) ────────────────────────────────────────────────────
    { "Small Cup",       "🌊 Space",   true,  0,16000,   5,  0,  100,  false, 0.25f, 20,  0,   true,  20, 0.5f, 20,  0,   100 },
    { "Glass Bottle",    "🌊 Space",   true,  0,14000,   8,  0,  100,  false, 0.25f, 20,  0,   true,  35, 1.0f, 28,  0,   100 },
    { "Juice Bar",       "🌊 Space",   true,  0,12000,  10,  0,  100,  true,  0.25f, 25, 15,   true,  45, 1.5f, 35,  0,   100 },
    { "Warehouse",       "🌊 Space",   true,  0,10000,  12,  0,  100,  true,  0.5f,  30, 20,   true,  65, 3.0f, 50,  0,   100 },
    { "Stadium Pour",    "🌊 Space",   true,  0, 8000,  15,  0,  100,  true,  0.75f, 35, 25,   true,  80, 5.0f, 60,  0,   100 },
    { "Deep Ocean",      "🌊 Space",   true,  0, 4000,  20,  0,  100,  false, 0.25f, 20,  0,   true,  90, 7.0f, 70,  0,   100 },
    { "Fruit Cave",      "🌊 Space",   true,  0, 6000,  18,  0,  100,  true,  0.5f,  30, 18,   true,  75, 4.5f, 55,  0,   100 },
    { "Canyon Splash",   "🌊 Space",   true,  0, 8000,  22,  5,  100,  true,  1.0f,  40, 28,   true,  85, 6.0f, 65,  0,   100 },
    { "Cloud Juice",     "🌊 Space",   true,  0,16000,   5,  0,  100,  false, 0.25f, 15,  0,   true, 100, 8.0f, 75,  0,   100 },
    { "Infinite Pour",   "🌊 Space",   false, 0, 2000,   0,  0,  100,  false, 0.25f, 90,  0,   true, 100,10.0f, 80,  0,   100 },

    // ── 🌀 Delay ─────────────────────────────────────────────────────────────
    { "Echo Box",        "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  0.5f,  45, 40,   false, 30, 1.0f,  0,  0,   100 },
    { "Sticky Straw",    "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  0.25f, 65, 45,   false, 30, 1.0f,  0,  0,   100 },
    { "Juice Trail",     "🌀 Delay",   true,  0,10000,  10,  0,  100,  true,  0.375f,50, 38,   true,  30, 1.2f, 20,  0,   100 },
    { "Ping Pong Punch", "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  0.5f,  55, 45,   false, 20, 0.8f,  0,  0,   100 },
    { "Slap Carton",     "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  0.125f,30, 30,   false, 15, 0.5f,  0,  0,   100 },
    { "Bounce Back",     "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  0.375f,60, 40,   false, 20, 0.8f,  0,  0,   100 },
    { "Drip Delay",      "🌀 Delay",   true,  0, 8000,  12,  0,  100,  true,  0.5f,  40, 35,   true,  25, 1.0f, 15,  0,   100 },
    { "Splash Echo",     "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  0.75f, 50, 42,   true,  40, 1.5f, 25,  0,   100 },
    { "Long Pour",       "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  1.0f,  55, 38,   true,  50, 2.0f, 30,  0,   100 },
    { "Endless Sip",     "🌀 Delay",   false, 0, 2000,   0,  0,  100,  true,  1.5f,  80, 45,   true,  60, 3.0f, 35,  0,   100 },

    // ── 🎛 Filter FX ─────────────────────────────────────────────────────────
    { "Phone Call",      "🎛 Filter",  true,  2,  800,  60,  0,  100,  false, 0.25f, 30,  0,   false, 20, 0.8f,  0,  0,   100 },
    { "Radio Juice",     "🎛 Filter",  true,  2, 1200,  50,  5,  100,  false, 0.125f,25,  0,   false, 15, 0.5f,  0,  0,   100 },
    { "Vinyl Cup",       "🎛 Filter",  true,  0,  600,  30, 20,  100,  false, 0.25f, 20,  0,   true,  25, 1.0f, 15,  0,   100 },
    { "Lo-Fi Lemon",     "🎛 Filter",  true,  0,  500,  25, 30,  100,  false, 0.25f, 25,  0,   true,  30, 1.2f, 18,  0,   100 },
    { "AM Berry",        "🎛 Filter",  true,  2,  900,  55,  8,  100,  false, 0.25f, 20,  0,   false, 20, 0.8f,  0,  0,   100 },
    { "DJ Sweep",        "🎛 Filter",  true,  0,  300,  70, 15,  100,  false, 0.25f, 35,  0,   false, 20, 0.8f,  0,  0,   100 },
    { "EDM Lift",        "🎛 Filter",  true,  0,  200,  80, 20,  100,  true,  0.25f, 45, 30,   true,  50, 2.0f, 30,  0,   100 },
    { "Hip Hop Filter",  "🎛 Filter",  true,  0,  800,  35, 10,  100,  true,  0.25f, 35, 25,   false, 20, 0.8f,  0,  0,   100 },
    { "House Rise",      "🎛 Filter",  true,  0,  400,  60, 10,  100,  true,  0.25f, 40, 28,   true,  45, 2.0f, 28,  0,   100 },
    { "Tech Fade",       "🎛 Filter",  true,  0, 1500,  45, 18,  100,  true,  0.5f,  35, 22,   true,  35, 1.5f, 20,  0,   100 },

    // ── 🔥 Producer Favorites ─────────────────────────────────────────────────
    { "Nova Vocal Throw","🔥 Favorites",true, 0,12000,  20,  5,  100,  true,  0.25f, 40, 30,   true,  50, 2.0f, 30,  0,   100 },
    { "Nova Beat Drop",  "🔥 Favorites",true, 0,  400,  75, 25,  100,  true,  0.5f,  55, 45,   true,  70, 4.0f, 45,  30,  100 },
    { "Travis Filter",   "🔥 Favorites",true, 0,  600,  65, 15,  100,  true,  0.375f,50, 38,   true,  60, 3.0f, 38,  0,   100 },
    { "Drake Delay",     "🔥 Favorites",true, 0,10000,  15,  3,  100,  true,  0.375f,45, 35,   true,  45, 2.0f, 28,  0,   100 },
    { "Afro Bounce",     "🔥 Favorites",true, 0, 2000,  40, 10,  100,  true,  0.25f, 50, 38,   true,  40, 1.8f, 25,  0,   100 },
    { "Latin Transition","🔥 Favorites",true, 0,  800,  55, 12,  100,  true,  0.5f,  40, 30,   true,  50, 2.5f, 32,  0,   100 },
    { "Club Drop",       "🔥 Favorites",true, 0,  300,  85, 30,  100,  true,  0.25f, 60, 50,   true,  65, 3.5f, 45,  25,  100 },
    { "Pop Lift",        "🔥 Favorites",true, 0,  500,  60, 10,  100,  true,  0.25f, 45, 35,   true,  55, 2.5f, 35,  15,  100 },
    { "Trap Intro",      "🔥 Favorites",true, 0,  200,  70, 20,  100,  true,  0.5f,  50, 40,   true,  45, 2.0f, 30,  10,  100 },
    { "House Build",     "🔥 Favorites",true, 0,  350,  72, 15,  100,  true,  0.25f, 55, 42,   true,  60, 3.0f, 40,  20,  100 },
    { "Festival Sweep",  "🔥 Favorites",true, 0,  200,  90, 35,  100,  true,  0.5f,  65, 55,   true,  80, 5.0f, 55,  40,  100 },
    { "R&B Space",       "🔥 Favorites",true, 0, 8000,  18,  2,  100,  true,  0.375f,35, 25,   true,  65, 3.5f, 42,   0,  100 },
    { "Reggaeton Rise",  "🔥 Favorites",true, 0,  600,  65, 18,  100,  true,  0.25f, 48, 38,   true,  50, 2.5f, 35,  20,  100 },
    { "Drill Fade",      "🔥 Favorites",true, 0, 1500,  50, 12,  100,  true,  0.5f,  40, 30,   true,  45, 2.0f, 28,   0,  100 },
    { "Future Bounce",   "🔥 Favorites",true, 0,  400,  80, 22,  100,  true,  0.375f,60, 48,   true,  70, 4.0f, 48,  30,  100 },
};

static constexpr int kNumPresets = (int)(sizeof (kJuicePresets) / sizeof (kJuicePresets[0]));

// Category display order
static const char* kCategoryOrder[] = {
    "🔥 Favorites",
    "🥤 SIP",
    "🍊 Citrus",
    "🍇 Berry",
    "🍉 Tropical",
    "🍎 Orchard",
    "🍒 Candy",
    "🌊 Space",
    "🌀 Delay",
    "🎛 Filter",
};
static constexpr int kNumCategories = 10;

// ─── Apply a preset to the APVTS ─────────────────────────────────────────────
inline void applyPreset (const JuicePreset& p, juce::AudioProcessorValueTreeState& apvts)
{
    auto set = [&](const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
    };
    auto setBool = [&](const char* id, bool v) { set (id, v ? 1.f : 0.f); };

    setBool ("filterOn",     p.filterOn);
    set     ("filterMode",   (float)p.filterMode);
    set     ("cutoff",       p.cutoff);
    set     ("resonance",    p.resonance);
    set     ("drive",        p.drive);
    set     ("filterMix",    p.filterMix);
    setBool ("delayOn",      p.delayOn);
    set     ("delayTime",    p.delayTime);
    set     ("delayFeedback",p.delayFb);
    set     ("delayMix",     p.delayMix);
    setBool ("reverbOn",     p.reverbOn);
    set     ("reverbSize",   p.reverbSize);
    set     ("reverbDecay",  p.reverbDecay);
    set     ("reverbMix",    p.reverbMix);
    set     ("sip",          p.sip);
    set     ("masterMix",    p.masterMix);
}
