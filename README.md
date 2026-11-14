# D.A Dreamy Reverb

A lush, shimmering, modulated reverb plugin built with JUCE 8.

* **Windows 64-bit:** VST3
* **macOS (Universal: Apple Silicon + Intel):** AU and VST3

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

1. Create a new repository on GitHub.
2. Upload **everything** in this folder, including the hidden `.github` folder
   (`.github/workflows/build.yml`). The easiest way:

   ```
   git init
   git add .
   git commit -m "Dreamy Reverb"
   git branch -M main
   git remote add origin https://github.com/<you>/<repo>.git
   git push -u origin main
   ```
3. Open the **Actions** tab. The "Build D.A Dreamy Reverb" workflow starts on every push
   (you can also start it by hand with "Run workflow").
4. When it finishes, download the artifacts at the bottom of the run page:
   * `DreamyReverb-Windows-x64-VST3`
   * `DreamyReverb-macOS-AU-VST3`

JUCE is downloaded automatically by CMake, nothing else is needed.

## Install

**Windows:** copy `D.A Dreamy Reverb.vst3` to `C:\Program Files\Common Files\VST3\`

**macOS:** unzip `DreamyReverb-macOS.zip`, then copy

* `D.A Dreamy Reverb.component` to `~/Library/Audio/Plug-Ins/Components/`
* `D.A Dreamy Reverb.vst3` to `~/Library/Audio/Plug-Ins/VST3/`

The macOS build is only ad-hoc signed (not notarized), so remove the download quarantine once:

```
xattr -cr ~/Library/Audio/Plug-Ins/Components/"D.A Dreamy Reverb.component"
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"D.A Dreamy Reverb.vst3"
```

Then restart your DAW (Logic may need a rescan or a reboot to pick up the new AU).

## Build locally

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Results are in `build/DreamyReverb_artefacts/Release/`.

## Layout

```
CMakeLists.txt                 build setup (fetches JUCE 8.0.4)
Source/DreamyReverbDSP.h       the reverb algorithm (plain C++)
Source/PluginProcessor.*       parameters, state, audio callback
Source/PluginEditor.*          user interface
Design/aurora-design.png       the approved look of the plugin
.github/workflows/build.yml    CI for Windows and macOS
```

Note: JUCE is dual licensed (AGPLv3 or commercial). Check the JUCE licence before selling the plugin.
