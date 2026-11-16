# D.A AURA

A lush, shimmering, modulated reverb plugin built with JUCE 8.

* **Windows 64-bit:** VST3 (`D.A AURA.vst3`)
* **macOS (Universal: Apple Silicon + Intel):** AU (`D.A AURA.component`) and VST3 (`D.A AURA.vst3`)

## Interface

"Aurora" design (see `Design/aurora-design.png`). The window is resizable: drag the bottom-right
corner (480 px to 1800 px wide, fixed aspect ratio). The size is saved with the project.
Double-click a knob to reset it to its default value.

![design](Design/aurora-design.png)

## Controls

| Knob | What it does |
|---|---|
| Mix | Dry / wet balance (equal power) |
| Size | Room size (0.35x to 2x delay lengths) |
| Decay | Reverb time, 0.3 to 30 s |
| Pre-Delay | Gap before the reverb starts, 0 to 500 ms |
| Shimmer | Octave-up feedback for the angelic halo |
| High Cut | Damping of highs inside the tail |
| Low Cut | Removes mud inside the tail |
| Mod Depth / Mod Rate | Chorus-like movement of the tail |
| Width | Stereo width of the wet signal (0 to 150 %) |
| Freeze | Holds the current tail forever and mutes new input |

## Build with GitHub Actions

1. Push to GitHub; the **Build D.A AURA** workflow starts automatically (or run it manually via **Actions -> Build D.A AURA -> Run workflow**).
2. Download the build artifacts at the bottom of the workflow run:
   * `DA-AURA-Windows-x64-Installer` (`D.A AURA Installer.exe`)
   * `DA-AURA-macOS-Installer` (`D.A AURA Installer.dmg` -> `D.A AURA Installer.app`)

## Install

**Windows:** extract and run `D.A AURA Installer.exe`. The self-contained borderless installer
uses the exact `1737 x 664` D.A Production installer UI with octagonal corners and embeds the
complete `D.A AURA.vst3` bundle inside the `.exe`. Click **Next** to install to
`C:\Program Files\Common Files\VST3\D.A AURA.vst3`, then restart or rescan your DAW.

**macOS:** open `D.A AURA Installer.dmg` and run `D.A AURA Installer.app`. The self-contained
borderless installer uses the exact `1737 x 664` D.A Production installer UI with octagonal
corners and embeds both Universal `D.A AURA.vst3` and `D.A AURA.component` bundles inside the
`.app`. Click **Next** to install both formats to `~/Library/Audio/Plug-Ins/`, then restart or
rescan your DAW.

## Build locally

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Results are in `build/DAAURA_artefacts/Release/`.

## Layout

```
CMakeLists.txt                 build setup (fetches JUCE 8.0.4)
Source/AuraDSP.h               the reverb algorithm (plain C++)
Source/PluginProcessor.*       parameters, state, audio callback
Source/PluginEditor.*          user interface
Design/aurora-design.png       the approved look of the plugin
Installer/                     self-contained Windows & macOS installers + artwork
.github/workflows/build.yml    CI for Windows and macOS
```

Note: JUCE is dual licensed (AGPLv3 or commercial). Check the JUCE licence before selling the plugin.
