// The fifty factory presets, in five spaces.
//
// Rules the table below keeps (Tests/PresetChecks.cpp checks every one of them):
//
//   * fifty presets, all names different, in five spaces of ten;
//   * every value inside the range the plug-in's parameter has
//     (mix 0..1, size 0..1, decay 0.3..30 s, pre-delay 0..500 ms, high cut 1000..20000 Hz,
//      low cut 20..1000 Hz, mod depth 0..1, mod rate 0.05..3 Hz, shimmer 0..1, width 0..1.5);
//   * no two presets the same, and within a space from the short/tight ones to the long/large ones;
//   * freeze off everywhere except in the two presets that are meant to hold a chord
//     ("Frozen Glass" and "Infinite").
#include "Presets.h"

namespace aura
{

const Preset presets[numPresets] =
{
    //-------------------------------------------------------------- ROOMS: everyday rooms
    { "Small Room",      "Rooms", { 0.22f, 0.18f,  0.6f,  4.0f,  7500.0f,  180.0f, 0.10f, 0.35f, 0.00f, 0.85f, false } },
    { "Studio Room",     "Rooms", { 0.28f, 0.30f,  1.1f,  8.0f,  9000.0f,  140.0f, 0.18f, 0.30f, 0.00f, 1.00f, false } },
    { "Drum Room",       "Rooms", { 0.30f, 0.35f,  1.4f,  6.0f,  8000.0f,  110.0f, 0.22f, 0.45f, 0.00f, 1.05f, false } },
    { "Live Room",       "Rooms", { 0.32f, 0.42f,  1.9f, 12.0f,  8200.0f,  100.0f, 0.25f, 0.38f, 0.00f, 1.00f, false } },
    { "Garage",          "Rooms", { 0.35f, 0.48f,  2.2f, 10.0f,  6000.0f,  130.0f, 0.30f, 0.55f, 0.00f, 1.10f, false } },
    { "Chapel",          "Rooms", { 0.38f, 0.55f,  3.4f, 22.0f,  7000.0f,  120.0f, 0.28f, 0.30f, 0.05f, 1.15f, false } },
    { "Large Hall",      "Rooms", { 0.35f, 0.68f,  4.6f, 28.0f,  6500.0f,  110.0f, 0.35f, 0.28f, 0.05f, 1.20f, false } },
    { "Concert Hall",    "Rooms", { 0.36f, 0.76f,  5.8f, 32.0f,  6800.0f,  100.0f, 0.38f, 0.26f, 0.08f, 1.20f, false } },
    { "Amphitheatre",    "Rooms", { 0.38f, 0.84f,  7.0f, 40.0f,  6000.0f,  120.0f, 0.40f, 0.22f, 0.10f, 1.30f, false } },
    { "Empty Arena",     "Rooms", { 0.40f, 0.92f,  9.0f, 55.0f,  5200.0f,  140.0f, 0.45f, 0.20f, 0.12f, 1.35f, false } },

    //-------------------------------------------------------------- PLATES: bright and tight
    { "Drum Plate",      "Plates", { 0.26f, 0.22f,  1.6f,  0.0f, 11000.0f,  120.0f, 0.05f, 0.30f, 0.00f, 1.05f, false } },
    { "Thin Plate",      "Plates", { 0.34f, 0.30f,  1.8f,  0.0f, 12000.0f,  300.0f, 0.04f, 0.16f, 0.00f, 1.25f, false } },
    { "Pop Plate",       "Plates", { 0.28f, 0.26f,  2.2f, 12.0f, 10500.0f,  250.0f, 0.06f, 0.25f, 0.00f, 1.20f, false } },
    { "Bright Plate",    "Plates", { 0.30f, 0.25f,  2.4f,  0.0f, 15000.0f,  160.0f, 0.05f, 0.20f, 0.00f, 1.20f, false } },
    { "Dark Plate",      "Plates", { 0.32f, 0.28f,  2.8f,  0.0f,  4200.0f,  100.0f, 0.06f, 0.18f, 0.00f, 1.15f, false } },
    { "Vocal Plate",     "Plates", { 0.30f, 0.30f,  3.0f, 18.0f,  9000.0f,  200.0f, 0.08f, 0.22f, 0.00f, 1.10f, false } },
    { "Vintage Plate",   "Plates", { 0.32f, 0.34f,  3.2f,  2.0f,  3800.0f,   80.0f, 0.12f, 0.12f, 0.00f, 1.00f, false } },
    { "Wide Plate",      "Plates", { 0.34f, 0.32f,  3.4f,  6.0f,  9500.0f,  150.0f, 0.10f, 0.20f, 0.00f, 1.50f, false } },
    { "Metallic Plate",  "Plates", { 0.30f, 0.44f,  3.6f,  0.0f,  8000.0f,  600.0f, 0.35f, 0.70f, 0.00f, 1.10f, false } },
    { "Thick Plate",     "Plates", { 0.36f, 0.38f,  4.0f,  4.0f,  6000.0f,   90.0f, 0.10f, 0.14f, 0.00f, 1.15f, false } },

    //-------------------------------------------------------------- SPACES: architecture
    { "Underpass",       "Spaces", { 0.38f, 0.55f,  4.2f, 18.0f,  5200.0f,  180.0f, 0.25f, 0.35f, 0.00f, 1.30f, false } },
    { "Parking Garage",  "Spaces", { 0.36f, 0.60f,  4.8f, 20.0f,  4000.0f,  220.0f, 0.20f, 0.30f, 0.00f, 1.25f, false } },
    { "Tunnel",          "Spaces", { 0.40f, 0.62f,  5.0f, 24.0f,  4500.0f,  200.0f, 0.30f, 0.40f, 0.00f, 1.40f, false } },
    { "Stairwell",       "Spaces", { 0.34f, 0.66f,  5.5f, 16.0f,  6200.0f,  150.0f, 0.22f, 0.45f, 0.00f, 1.10f, false } },
    { "Cave",            "Spaces", { 0.42f, 0.70f,  6.0f, 30.0f,  3000.0f,  160.0f, 0.45f, 0.25f, 0.00f, 1.20f, false } },
    { "Indoor Pool",     "Spaces", { 0.40f, 0.72f,  6.5f, 26.0f,  7000.0f,  140.0f, 0.40f, 0.28f, 0.05f, 1.35f, false } },
    { "Church",          "Spaces", { 0.36f, 0.80f,  7.5f, 35.0f,  5600.0f,  100.0f, 0.28f, 0.18f, 0.08f, 1.25f, false } },
    { "Cathedral",       "Spaces", { 0.38f, 0.88f,  9.5f, 45.0f,  6000.0f,   90.0f, 0.30f, 0.15f, 0.10f, 1.30f, false } },
    { "Canyon",          "Spaces", { 0.45f, 0.90f, 11.0f, 80.0f,  5000.0f,  120.0f, 0.35f, 0.12f, 0.15f, 1.45f, false } },
    { "Cistern",         "Spaces", { 0.44f, 0.86f, 12.0f, 60.0f,  3500.0f,  100.0f, 0.18f, 0.10f, 0.20f, 1.40f, false } },

    //-------------------------------------------------------------- DREAMS: shimmer and motion
    { "Slow Motion",     "Dreams", { 0.44f, 0.74f,  7.0f, 45.0f,  6800.0f,  190.0f, 0.65f, 0.05f, 0.30f, 1.30f, false } },
    { "Dreamy Cloud",    "Dreams", { 0.42f, 0.75f,  8.0f, 40.0f,  7000.0f,  140.0f, 0.50f, 0.30f, 0.35f, 1.30f, false } },
    { "Shimmer Veil",    "Dreams", { 0.45f, 0.80f,  9.0f, 50.0f,  8000.0f,  160.0f, 0.45f, 0.22f, 0.65f, 1.40f, false } },
    { "Halo Pad",        "Dreams", { 0.48f, 0.78f, 10.0f, 60.0f,  7500.0f,  180.0f, 0.55f, 0.18f, 0.55f, 1.35f, false } },
    { "Night Sky",       "Dreams", { 0.46f, 0.82f, 11.0f, 75.0f,  5500.0f,  120.0f, 0.35f, 0.09f, 0.60f, 1.40f, false } },
    { "Angel Choir",     "Dreams", { 0.50f, 0.85f, 12.0f, 70.0f,  9000.0f,  200.0f, 0.40f, 0.12f, 0.80f, 1.45f, false } },
    { "Lucid Dream",     "Dreams", { 0.48f, 0.84f, 12.0f, 55.0f,  7200.0f,  130.0f, 0.60f, 0.35f, 0.60f, 1.40f, false } },
    { "Aurora Borealis", "Dreams", { 0.50f, 0.86f, 13.0f, 65.0f,  8200.0f,  170.0f, 0.70f, 0.15f, 0.70f, 1.50f, false } },
    { "Ethereal Wash",   "Dreams", { 0.52f, 0.88f, 14.0f, 90.0f,  6500.0f,  150.0f, 0.60f, 0.10f, 0.45f, 1.45f, false } },
    { "Weightless",      "Dreams", { 0.55f, 0.90f, 16.0f,100.0f,  6000.0f,  110.0f, 0.50f, 0.08f, 0.50f, 1.45f, false } },

    //-------------------------------------------------------------- EXTREMES: long, frozen, strange
    { "Broken Radio",    "Extremes", { 0.42f, 0.64f,  8.5f, 30.0f,  1800.0f,  350.0f, 0.85f, 1.80f, 0.10f, 0.70f, false } },
    { "Bloom",           "Extremes", { 0.40f, 0.68f,  9.5f, 65.0f, 14000.0f,  130.0f, 0.35f, 0.25f, 0.45f, 1.40f, false } },
    { "Submerged",       "Extremes", { 0.54f, 0.80f, 11.0f, 85.0f,  2000.0f,   50.0f, 0.60f, 0.11f, 0.15f, 1.10f, false } },
    { "Ghost Ship",      "Extremes", { 0.46f, 0.76f, 15.0f, 95.0f,  2600.0f,   80.0f, 0.75f, 0.16f, 0.40f, 1.35f, false } },
    { "Time Warp",       "Extremes", { 0.48f, 0.70f, 18.0f,  0.0f,  4800.0f,  110.0f, 0.90f, 0.90f, 0.20f, 1.30f, false } },
    { "Frozen Glass",    "Extremes", { 0.45f, 0.60f, 20.0f,  0.0f, 12000.0f,  400.0f, 0.15f, 0.20f, 0.25f, 1.30f, true  } },
    { "Deep Space",      "Extremes", { 0.52f, 0.94f, 22.0f, 150.0f,  3200.0f,   70.0f, 0.45f, 0.05f, 0.75f, 1.50f, false } },
    { "Endless Rise",    "Extremes", { 0.58f, 0.92f, 25.0f,140.0f, 11000.0f,  150.0f, 0.50f, 0.10f, 1.00f, 1.50f, false } },
    { "Black Hole",      "Extremes", { 0.55f, 1.00f, 26.0f, 200.0f,  2200.0f,   60.0f, 0.55f, 0.07f, 0.35f, 1.20f, false } },
    { "Infinite",        "Extremes", { 0.50f, 0.95f, 30.0f, 120.0f,  5500.0f,  100.0f, 0.40f, 0.06f, 0.60f, 1.45f, true  } },
};

const Space spaces[numSpaces] =
{
    { "Rooms",    0, 10 },
    { "Plates",  10, 10 },
    { "Spaces",  20, 10 },
    { "Dreams",  30, 10 },
    { "Extremes",40, 10 },
};

} // namespace aura
