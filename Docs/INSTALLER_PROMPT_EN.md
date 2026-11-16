# Reusable prompt — D.A Production Installer (English)

**How to use it:** start a new task with the agent, attach the design picture
(`Installer.png`, 1857 × 847 — the same picture every time), fill in the block below,
and paste everything from "TASK" to the end of the message. Nothing else is needed:
no Google Drive, no installer `.exe` to analyse.

```
PRODUCT NAME:      D.A ______            (what the user sees, e.g. "D.A AURA")
REPO / BRANCH:     github.com/<owner>/<repo>  branch: <branch>
PLUGIN TARGET:     <the JUCE target in that repo that already builds the plug-in>
VERSION:           <x.y.z>
MANUFACTURER CODE: <4 chars, e.g. DApr>
PLUGIN CODE:       <4 chars, e.g. Aura>
FORMATS:           VST3 on Windows;  AU + VST3 on macOS (Universal arm64 + x86_64)
```

---

## TASK

Add to that repository the **D.A Production installer** for `<PRODUCT NAME>`: a small
JUCE desktop application that carries the plug-in inside itself, shows the attached
design as its window, and copies the plug-in into the standard plug-in folders.
Produce the same thing that already exists for **D.A AURA** — same behaviour, same
pixels, same tests, on Windows x64 and macOS Universal. Do not design anything new.

## THE REFERENCE — PORT IT, DO NOT RE-INVENT IT

A complete, CI-verified implementation is public. Clone it and port it file by file:

```sh
git clone --depth 1 -b arena/03e643f9-d-a-dreamyreverb \
    https://github.com/drdrillcontact-create/D.A-DreamyReverb.git /tmp/ref
git -C /tmp/ref fetch --depth 1 origin 1d65f6a63f3797472642f075a8564b300083740a
git -C /tmp/ref checkout 1d65f6a63f3797472642f075a8564b300083740a
```

Files to take over as they are (they contain no product-specific text, only the tokens
in the next section):

| From the reference | What it is |
|---|---|
| `Installer/Design/make-assets.py` | turns the design into the installer's pictures + `picture.sum` |
| `Installer/Assets/*` (except `icon.png`) | `picture.png`, `patchText.png`, `patchNoButtons.png`, `patchFinish.png`, `picture.sum`, the five Red Hat Display TTFs and `RedHatDisplay-OFL.txt` |
| `Installer/InstallerBuildConfig.h.in`, `Installer/payload.rc.in` | build-time header and Windows resource scripts |
| `Installer/Source/*` | the whole installer (`InstallerArt.h`, `InstallerAssets.*`, `InstallerConfig.h`, `InstallerEngine.*`, `InstallerMain.cpp`, `InstallerPicture.*`, `InstallerShell.h`, `InstallerView.*`, `InstallerWindow.h`, `PayloadWindows.cpp`, `SystemMouse.cpp`, `SystemWindow.cpp`) |
| `.github/scripts/*` | `run-installer.sh`, `run-installer-silently.sh`, `look-at-screenshot.py`, `capture-screen.*`, `shrink-picture.ps1`, `annotate*.sh`, `alone.sh`, `run-pluginval.sh` |
| `Tests/*` | the headless Linux harness (`InstallerHarness.cpp`, `local-juce.sh`, `run-installer-harness.sh`) |
| `CMakeLists.txt` | the `juce_add_gui_app` installer section, the payload target, the Windows/macOS packaging |
| `.github/workflows/build.yml` | both CI jobs |

Then rename every token — nothing of the old product may remain:

| Token in the reference | Becomes |
|---|---|
| `daaura` (C++ namespace, folders) | `<ns>` |
| `DAAURA_` (CMake variables, defines, `DAAURA_PRODUCT_NAME` …) | `<NS>_` |
| `"D.A AURA"` (product name, file names, bundle name) | `"<PRODUCT NAME>"` |
| `com.da.aura` / `com.da.aura.installer` | your bundle identifiers |

Keep the C++ namespace and the class names as `daaura` **only if** this repo is a
D.A Production product; otherwise rename as above. Finish with:

```sh
grep -rniE 'daaura|D\.A\.? AURA|dreamy|cloudveil|lune' . --exclude-dir=.git   # must print nothing
```

## THE ATTACHED DESIGN IS THE ONLY TRUTH

* The design is 1857 × 847. The installer's window is its crop at **(58, 86)**, size
  **1764 × 695**. At the design's scale there are **1.8 design pixels per logical unit**,
  so the window is laid out once for **980 × 386 units** and the whole layout is scaled
  when the window is resized (0.7× … 2.5×, aspect locked, Lanczos resampling).
* The frame is an octagon: inside the crop its edges are at left **14**, top **14**,
  right **1751**, bottom **679**, each corner cut at 45° 17–18 px along either side.
  Inside the frame is the page; **outside it nothing is drawn and nothing is seen** —
  the window is transparent there: no title bar, no border, no shadow, no rounding by
  the system (Windows: `WM_NCCALCSIZE` handled, `DWMWA_BORDER_COLOR = NONE`,
  `DWMWCP_DONOTROUND`; macOS: `setOpaque(false)`, clear background).
* **The first page is the design, pixel for pixel**: inside the frame `0 of 1,154,439`
  pixels may differ. That is a test, not a slogan.
* Nothing is a JUCE widget: the buttons' pills, the checkboxes, the progress bar, the
  minimize bar and close cross are all drawn, not `TextButton`/`ToggleButton` shapes.
* Where things are (measured on the design, in the 980 × 386 units): red `Next` pill
  `{479.44, 306.67, 218.89, 38.89}` r = 7.8 with a 15-unit red glow; outlined `Cancel`
  pill `{719.44, 306.67, 202.22, 38.89}` r = 11.7; minimise glyph at `(907.78, 29.44)`,
  close glyph at `(942.22, 29.72)`, reach ±7; type column from x = 479.5 to 921.7.
* Colours: red `0xffea030f`, headline red `0xffdd0510`, white `0xffdedede`, running
  text `0xffaaa9a9`, panel `0xff040303`.
* Typeface: Red Hat Display (Regular, Medium, SemiBold, Bold, ExtraBold), OFL, built in.

## THE PAGES

| # | Page | What is on it |
|---|---|---|
| 1 | Welcome | exactly the design: `Welcome to the` / `D.A Production Installer` / two grey lines / red `Next →` + `Cancel` |
| 2 | Select the Install Location | headline `Select the` / `Install Location`; one row per format with a red ticked checkbox and its folder: `Audio Unit  ~/Library/Audio/Plug-Ins/Components`, `VST3  ~/Library/Audio/Plug-Ins/VST3`, on Windows one row: `VST3  C:\Program Files\Common Files\VST3`; all ticked to begin with; `Next` is off when nothing is ticked |
| 3 | Installing | `Please wait` / `Installing <PRODUCT NAME>`; red progress bar in the design's place with `NN %`; `Copying the plug-in files…` |
| 4 | Installation complete | `Installation complete` / `<PRODUCT NAME> is Ready`; `<PRODUCT NAME> has been installed on your computer.`; `Click Finish to close the installer.`; one red `Finish` |
| 5 | Installation failed | `Installation failed` / `<PRODUCT NAME> Not Installed`; the reason; one red `Close` |

The design's own words stay as they are (the company does not change); the product
name appears only where the table above says it does.

## BEHAVIOUR

* Dragged anywhere on its face, the window moves; taken within ~6 px of the frame, it
  resizes and keeps its proportions (the same feel as a window of the system's).
* `--silent`: installs into the standard folders with no window, logs what it does,
  exits `0` on success, `1` on failure — used by CI to verify the upgrade path.
* `--self-test <folder>`: the installer drives itself (clicks Next, Next, Finish, drags
  its frame, moves, minimises, restores), writes a report and page pictures; used by CI.
* A re-install replaces the old copy **completely** (remove, then copy), and what lands
  on disk is compared **file for file** with what was built.
* Errors (a folder that cannot be written, a payload that is not there) land on page 5
  with the reason, never a crash or a silent no-op.

## WHAT THE INSTALLER CARRIES

* The plug-in bundles are zipped at build time by a custom target that depends on the
  plug-in targets, in this layout: `VST3/<PRODUCT NAME>.vst3/…` and (macOS)
  `AU/<PRODUCT NAME>.component/…`. The zip must contain the bundles **after** code
  signing and after the VST3 `moduleinfo.json` was written.
* **Windows:** the zip becomes an `RCDATA` resource (`payload.rc`, resource id
  `DAAURA_PAYLOAD`) inside one `.exe`; the manifest says
  `requestedExecutionLevel level="requireAdministrator"` (`/MANIFESTUAC`), because the
  VST3 folder belongs to administrators. MSVC runtime linked statically.
* **macOS:** the zip goes to `Contents/Resources/payload.zip` in the `.app`, and the
  outer bundle is re-signed ad hoc (`codesign --force --sign -`) after the copy; the
  plug-in bundles are signed ad hoc too. Universal: `arm64;x86_64`,
  deployment target 11.0.
* `juce_add_binary_data(... HEADER_NAME InstallerData.h NAMESPACE InstallerData ...)`
  — the sources `#include <InstallerData.h>`. **Without `HEADER_NAME` the build fails
  with `Cannot open include file: 'InstallerData.h'`.** This is the one trap here.
* The pictures come from `Installer/Design/make-assets.py` (needs Pillow + numpy). If
  the design picture is unchanged, `picture.sum` must still read `d2fa580a`. If you ever
  change the design, re-measure `WINDOW`, `TEXT`, `BUTTONS`, the frame and the constants
  in `InstallerView.cpp` / `InstallerAssets.cpp`, and regenerate everything.

## PROOF — BEFORE YOU SAY IT IS DONE

1. **Locally, headless (Linux):** `bash Tests/run-installer-harness.sh /tmp/pages`
   prints per-page `[PASS]` lines and `ALL PASSED`, and writes a picture of every page
   (welcome, location, location-Windows, installing, finished, failed, smallest,
   largest, 100 %) plus `1-welcome-in-type.png` and the pointer states. Look at them.
2. **Locally, with the real payload:** build a zip of dummy bundles and run
   `Tests/run-installer-harness.sh --program /tmp/payload.zip --silent --vst3-dir /tmp/out`;
   the bundles must arrive intact.
3. **In CI, both platforms green** (`Build <PRODUCT NAME>` workflow):
   * Release build of the VST3 (Windows) and of AU + VST3 + installer (macOS).
   * macOS: `lipo -verify_arch arm64 x86_64` on all three binaries and
     `codesign --verify --deep --strict` on the AU, the VST3 and the `.app`.
   * `pluginval` strictness 10, seeds `0x3` and `0x7`, on the VST3.
   * `auval -v aufx <PLUGIN CODE> <MANUFACTURER CODE>` against the **installed** AU.
   * The installer is taken out of its uploaded zip, put alone on the Desktop with the
     build folder moved out of reach, and run there: the welcome page matches the design
     (`0 of 1154439` differ), nothing of the window is seen around its frame
     (all sampled pixels are the desktop), the installed bundle compares file for file.
   * `--silent` run afterwards replaces the same folder and compares again.
   * Artifacts uploaded: `<PRODUCT>-Windows-Installer.zip`,
     `<PRODUCT>-macOS-Installer.zip`, the plain VST3 / AU+VST3 zips, and the
     installer-pictures zips.
4. `grep` for the old product's names prints nothing; `README.md` lists the new files.

## NEVER

* Never redraw, re-layout or "improve" the design, and never replace it with a normal
  system window — the frame, the type, the colours and the pixel test stay.
* Never ask for a Google Drive link or an `.exe` to analyse: **the design picture is
  attached to this conversation and is the master**. If something is genuinely missing,
  ask one short question instead of guessing.
* Never change the words baked into the design (`Welcome to the`, `D.A Production
  Installer`, `Next`, `Cancel`).
* Never leave one platform failing, and never call it finished while CI is red.
* Never put secrets, signing certificates or developer IDs in the repo; ad-hoc signing
  only.

## DONE MEANS

A public branch where `Build <PRODUCT NAME>` is green on **both** jobs, with the six
artifacts above; the installer opens as the design, installs the plug-in into the
standard folders on a clean machine, replaces an older copy cleanly, and leaves nothing
of the reference product's name anywhere in the tree.
