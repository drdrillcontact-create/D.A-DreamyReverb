// D.A AURA - the 50 factory presets (plain C++, no JUCE dependency)
#pragma once

namespace dreamy
{

struct Preset
{
    const char* name;
    const char* category;
    float mix;        // 0..1
    float size;       // 0..1
    float decay;      // seconds, 0.3..30
    float preDelay;   // ms, 0..500
    float highCut;    // Hz, 1000..20000
    float lowCut;     // Hz, 20..1000
    float modDepth;   // 0..1
    float modRate;    // Hz, 0.05..3
    float shimmer;    // 0..1
    float width;      // 0..1.5
    int   interval;   // 0 octave, 1 octave + fifth, 2 two octaves, 3 sub + octave
};

constexpr int numShimmerIntervals = 4;

// Names of the shimmer intervals, in the order used by Preset::interval
constexpr const char* shimmerIntervalNames[numShimmerIntervals] =
{
    "Octave", "Octave + 5th", "2 Octaves", "Sub + Octave"
};

// The first preset is identical to the default value of every parameter.
constexpr Preset factoryPresets[] =
{
    //  name                     category           mix    size   decay  pre     high     low    depth  rate   shim   width  interval

    // ---- Signature ------------------------------------------------------------------------------------------------------
    { "Dreamy Default",         "Signature",       0.40f, 0.65f,  7.0f,  25.0f,  6500.0f, 120.0f, 0.45f, 0.45f, 0.30f, 1.00f, 0 },
    { "Angel Halo",             "Signature",       0.45f, 0.75f,  9.5f,  40.0f,  8500.0f, 180.0f, 0.40f, 0.35f, 0.62f, 1.15f, 0 },
    { "Crystal Cathedral",      "Signature",       0.42f, 0.92f, 12.0f,  55.0f,  9500.0f, 150.0f, 0.30f, 0.25f, 0.55f, 1.20f, 2 },
    { "Velvet Sky",             "Signature",       0.38f, 0.70f,  6.0f,  30.0f,  4800.0f, 140.0f, 0.55f, 0.40f, 0.38f, 1.10f, 0 },
    { "Aurora Borealis",        "Signature",       0.50f, 0.85f, 14.0f,  60.0f,  7500.0f, 200.0f, 0.65f, 0.22f, 0.70f, 1.35f, 1 },
    { "Glass Choir",            "Signature",       0.45f, 0.60f,  8.0f,  20.0f, 11000.0f, 250.0f, 0.35f, 0.50f, 0.78f, 1.20f, 2 },
    { "Deep Heaven",            "Signature",       0.45f, 0.95f, 16.0f,  80.0f,  5500.0f,  90.0f, 0.50f, 0.18f, 0.50f, 1.25f, 3 },
    { "Silk Shimmer",           "Signature",       0.30f, 0.55f,  4.5f,  15.0f,  7000.0f, 160.0f, 0.40f, 0.55f, 0.42f, 1.00f, 0 },

    // ---- Vocals ---------------------------------------------------------------------------------------------------------
    { "Vocal Halo",             "Vocals",          0.22f, 0.50f,  3.2f,  45.0f,  7500.0f, 220.0f, 0.30f, 0.50f, 0.30f, 1.05f, 0 },
    { "Lead Vocal Air",         "Vocals",          0.18f, 0.42f,  2.4f,  60.0f, 10500.0f, 280.0f, 0.25f, 0.60f, 0.36f, 1.10f, 0 },
    { "Dream Pop Vocal",        "Vocals",          0.35f, 0.68f,  5.5f,  70.0f,  6200.0f, 240.0f, 0.55f, 0.45f, 0.48f, 1.20f, 0 },
    { "Whisper Cloud",          "Vocals",          0.30f, 0.60f,  4.2f,  35.0f,  5200.0f, 300.0f, 0.45f, 0.35f, 0.40f, 1.15f, 0 },
    { "Choir Lift",             "Vocals",          0.32f, 0.78f,  6.8f,  50.0f,  8200.0f, 200.0f, 0.38f, 0.30f, 0.58f, 1.20f, 1 },
    { "Backing Vox Wash",       "Vocals",          0.40f, 0.72f,  7.5f,  25.0f,  5800.0f, 260.0f, 0.60f, 0.40f, 0.45f, 1.30f, 0 },
    { "Ballad Plate Glow",      "Vocals",          0.24f, 0.38f,  2.8f,  30.0f,  9000.0f, 180.0f, 0.20f, 0.70f, 0.22f, 1.00f, 0 },
    { "Ethereal Ad-Libs",       "Vocals",          0.45f, 0.82f,  9.0f, 110.0f,  7000.0f, 320.0f, 0.50f, 0.28f, 0.66f, 1.35f, 2 },

    // ---- Guitars --------------------------------------------------------------------------------------------------------
    { "Ambient Guitar Swell",   "Guitars",         0.48f, 0.80f, 10.0f,  35.0f,  6800.0f, 170.0f, 0.50f, 0.30f, 0.64f, 1.25f, 0 },
    { "Post-Rock Skyline",      "Guitars",         0.45f, 0.88f, 12.5f,  45.0f,  6000.0f, 150.0f, 0.58f, 0.26f, 0.58f, 1.30f, 1 },
    { "Clean Strat Shine",      "Guitars",         0.28f, 0.52f,  3.8f,  20.0f,  9000.0f, 190.0f, 0.35f, 0.55f, 0.40f, 1.05f, 0 },
    { "Edge Of Heaven",         "Guitars",         0.35f, 0.62f,  5.2f, 120.0f,  7800.0f, 200.0f, 0.42f, 0.48f, 0.52f, 1.15f, 0 },
    { "Acoustic Glow",          "Guitars",         0.22f, 0.45f,  2.9f,  25.0f,  8500.0f, 160.0f, 0.25f, 0.50f, 0.26f, 1.00f, 0 },
    { "Shoegaze Wall",          "Guitars",         0.60f, 0.90f, 11.0f,  15.0f,  5200.0f, 140.0f, 0.85f, 0.65f, 0.55f, 1.40f, 3 },
    { "Slide Mirage",           "Guitars",         0.40f, 0.74f,  8.0f,  90.0f,  6400.0f, 180.0f, 0.62f, 0.35f, 0.60f, 1.25f, 1 },

    // ---- Keys & Synths --------------------------------------------------------------------------------------------------
    { "Felt Piano Dream",       "Keys & Synths",   0.34f, 0.66f,  6.2f,  30.0f,  5000.0f, 130.0f, 0.40f, 0.32f, 0.34f, 1.10f, 0 },
    { "Grand Piano Halo",       "Keys & Synths",   0.26f, 0.58f,  4.5f,  25.0f,  8800.0f, 110.0f, 0.25f, 0.40f, 0.30f, 1.05f, 0 },
    { "Rhodes Stardust",        "Keys & Synths",   0.32f, 0.56f,  4.8f,  20.0f,  6800.0f, 150.0f, 0.55f, 0.60f, 0.44f, 1.15f, 0 },
    { "Pad Expander",           "Keys & Synths",   0.50f, 0.86f, 11.5f,  40.0f,  7200.0f, 120.0f, 0.60f, 0.28f, 0.50f, 1.40f, 0 },
    { "Synth Bloom",            "Keys & Synths",   0.42f, 0.76f,  8.5f,  30.0f,  9500.0f, 160.0f, 0.48f, 0.38f, 0.68f, 1.25f, 2 },
    { "Pluck Sparkle",          "Keys & Synths",   0.30f, 0.48f,  3.4f,  10.0f, 12000.0f, 220.0f, 0.30f, 0.65f, 0.56f, 1.10f, 2 },
    { "Organ Vault",            "Keys & Synths",   0.38f, 0.90f,  9.5f,  50.0f,  6000.0f,  80.0f, 0.32f, 0.22f, 0.46f, 1.20f, 3 },
    { "Music Box Frost",        "Keys & Synths",   0.36f, 0.58f,  5.6f,  15.0f, 13000.0f, 350.0f, 0.28f, 0.45f, 0.72f, 1.15f, 2 },

    // ---- Cinematic ------------------------------------------------------------------------------------------------------
    { "Neon Dawn",              "Cinematic",       0.48f, 0.94f, 15.0f,  70.0f,  5600.0f, 100.0f, 0.55f, 0.20f, 0.56f, 1.35f, 1 },
    { "Event Horizon",          "Cinematic",       0.50f, 1.00f, 22.0f,  90.0f,  6800.0f,  70.0f, 0.40f, 0.15f, 0.50f, 1.30f, 3 },
    { "Cathedral Of Ice",       "Cinematic",       0.44f, 0.96f, 13.0f,  65.0f, 11500.0f, 240.0f, 0.30f, 0.24f, 0.74f, 1.25f, 2 },
    { "Slow Sunrise",           "Cinematic",       0.46f, 0.84f, 17.0f, 150.0f,  6200.0f, 130.0f, 0.52f, 0.16f, 0.62f, 1.30f, 0 },
    { "Underwater Temple",      "Cinematic",       0.52f, 0.88f, 12.0f,  40.0f,  2600.0f,  90.0f, 0.78f, 0.30f, 0.44f, 1.40f, 3 },
    { "Starfield",              "Cinematic",       0.40f, 0.78f, 10.0f,  55.0f, 14000.0f, 400.0f, 0.36f, 0.34f, 0.90f, 1.45f, 2 },
    { "Last Light",             "Cinematic",       0.42f, 0.72f,  8.5f, 100.0f,  4500.0f, 150.0f, 0.48f, 0.28f, 0.50f, 1.20f, 1 },

    // ---- Ambient --------------------------------------------------------------------------------------------------------
    { "Infinite Horizon",       "Ambient",         0.55f, 1.00f, 30.0f,  60.0f,  6000.0f, 110.0f, 0.55f, 0.12f, 0.58f, 1.40f, 0 },
    { "Cloud Bank",             "Ambient",         0.60f, 0.92f, 18.0f,  20.0f,  3800.0f, 160.0f, 0.80f, 0.20f, 0.36f, 1.50f, 0 },
    { "Deep Space Drone",       "Ambient",         0.58f, 1.00f, 26.0f, 120.0f,  4800.0f,  50.0f, 0.60f, 0.10f, 0.54f, 1.35f, 3 },
    { "Tape Heaven",            "Ambient",         0.50f, 0.80f, 12.0f,  30.0f,  3400.0f, 200.0f, 0.95f, 0.85f, 0.46f, 1.25f, 0 },
    { "Ocean Of Glass",         "Ambient",         0.52f, 0.95f, 21.0f,  80.0f,  9000.0f, 180.0f, 0.45f, 0.14f, 0.76f, 1.45f, 1 },
    { "Sleep Cycle",            "Ambient",         0.48f, 0.86f, 16.0f, 200.0f,  3000.0f, 120.0f, 0.65f, 0.08f, 0.40f, 1.30f, 0 },

    // ---- Special --------------------------------------------------------------------------------------------------------
    { "Dark Matter",            "Special",         0.46f, 0.90f, 14.0f,  45.0f,  2200.0f,  60.0f, 0.50f, 0.22f, 0.52f, 1.20f, 3 },
    { "Fifth Dimension",        "Special",         0.44f, 0.76f,  9.0f,  30.0f,  8000.0f, 200.0f, 0.40f, 0.36f, 1.00f, 1.30f, 1 },
    { "Shimmer Slap",           "Special",         0.30f, 0.30f,  1.6f, 180.0f,  9500.0f, 250.0f, 0.20f, 0.60f, 0.60f, 1.10f, 0 },
    { "Small Room Sparkle",     "Special",         0.25f, 0.12f,  0.9f,   5.0f, 11000.0f, 200.0f, 0.15f, 0.70f, 0.50f, 0.90f, 0 },
    { "Pure Hall (No Shimmer)", "Special",         0.30f, 0.78f,  3.6f,  30.0f,  7500.0f, 100.0f, 0.30f, 0.40f, 0.00f, 1.10f, 0 },
    { "Pure Plate (No Shimmer)","Special",         0.28f, 0.34f,  2.2f,  10.0f, 10000.0f, 150.0f, 0.22f, 0.65f, 0.00f, 1.00f, 0 },
};

constexpr int numPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
static_assert (numPresets == 50, "there must be exactly 50 factory presets");

} // namespace dreamy
