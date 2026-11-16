# D.A Production Installer — reusable prompt (English)

**How to use:** attach the design picture (`Installer.png`, 1857 × 847), fill in the block
below, then paste everything from `TASK` to the end of this file. No Google Drive link and
no installer `.exe` is ever needed — the picture is the master.

```
PRODUCT NAME:      D.A ______
PRODUCT WORD:      ______            (the word after "D.A"; uppercase; used in file names, codes, targets)
CODE PREFIX:       DA______          (normally "DA" + PRODUCT WORD)
BUNDLE ID:         com.da.______
REPO / BRANCH:     github.com/<owner>/<repo>    branch: <branch>
PLUGIN TARGET:     <the JUCE target in that repo that already builds the plug-in>
VERSION:           <x.y.z>
MANUFACTURER CODE: <4 chars>
PLUGIN CODE:       <4 chars>
FORMATS:           VST3 on Windows;  AU + VST3 on macOS (Universal arm64 + x86_64)
```

---

## TASK

Add to that repository the **D.A Production installer** for `<PRODUCT NAME>`: a small JUCE
desktop application that carries the plug-in inside itself, shows the attached design as its
window, and copies the plug-in into the standard plug-in folders — on Windows x64 and macOS
Universal. This installer already exists, finished and verified in CI, for another product of
this house. Port it; do not design anything new.

## THE REFERENCE — PORT IT, DO NOT RE-INVENT IT

```sh
git clone --depth 1 -b arena/03e643f9-d-a-dreamyreverb \
    https://github.com/drdrillcontact-create/D.A-DreamyReverb.git /tmp/ref
git -C /tmp/ref fetch --depth 1 origin 1d65f6a63f3797472642f075a8564b300083740a
git -C /tmp/ref checkout 1d65f6a63f3797472642f075a8564b300083740a
```

That tree is the installer for **a different product**, so it carries that product's name, its
code prefix and its lowercase namespace (`DA…`, `da…`) in CMake variables, defines, the C++
namespace, the headers, the workflow, the scripts and the tests. Read them from the reference
instead of guessing:

```sh
old=$(sed -n 's/^ *set *( *[A-Z0-9]*_PRODUCT_NAME *"\(.*\)" *) *$/\1/p' /tmp/ref/CMakeLists.txt | head -n1)
prefix=$(grep -oE '[A-Z0-9]+_PRODUCT_NAME' /tmp/ref/CMakeLists.txt | head -n1 | sed 's/_PRODUCT_NAME//')
ns=$(printf '%s' "$prefix" | tr 'A-Z' 'a-z')
printf 'old=<%s>  prefix=<%s>  ns=<%s>\n' "$old" "$prefix" "$ns"
```

Take these over as they are (they hold no product-specific words, only those tokens):

`Installer/Design/make-assets.py` · `Installer/Assets/*` except `icon.png` (`picture.png`,
`patchText.png`, `patchNoButtons.png`, `patchFinish.png`, `picture.sum`, the five Red Hat
Display TTFs, `RedHatDisplay-OFL.txt`) · `Installer/InstallerBuildConfig.h.in` ·
`Installer/payload.rc.in` · all of `Installer/Source/` · `.github/scripts/` · `Tests/` · the
installer section of `CMakeLists.txt` · `.github/workflows/build.yml`.

Then, in the new repository, replace every occurrence of the old tokens:

| In the reference | Becomes |
|---|---|
| `"$old"` — product name, file and bundle names | `"<PRODUCT NAME>"` |
| `$prefix` — CMake variables, defines, `…_PRODUCT_NAME`, `…_PAYLOAD`, `…_LOCAL` | `<CODE PREFIX>` |
| `$ns` — C++ namespace, folder names, identifiers | lowercase `<CODE PREFIX>` |
| `com.da.<old word>` — bundle identifiers | `<BUNDLE ID>` and `<BUNDLE ID>.installer` |

Finish with this, which must print nothing:

```sh
grep -rniF -e "$old" -e "$prefix" -e "$ns" . --exclude-dir=.git
```

Also rewrite the comments that name the old product in prose, and the README.

## THE ATTACHED DESIGN IS THE ONLY TRUTH

* 1857 × 847. The installer's window is its crop at **(58, 86)**, size **1764 × 695**. There
  are **1.8 design pixels per logical unit**, so the window is laid out once for
  **980 × 386 units** and the whole layout is scaled when the window is resized
  (0.7× … 2.5×, aspect locked, Lanczos resampling).
* The frame is an octagon: inside the crop its edges are at left **14**, top **14**, right
  **1751**, bottom **679**, each corner cut at 45° 17–18 px along either side. **Outside it
  nothing is drawn and nothing is seen** — the window is transparent there: no title bar, no
  border, no shadow, no rounding by the system (Windows: handle `WM_NCCALCSIZE`,
  `DWMWA_BORDER_COLOR = DWMWA_COLOR_NONE`, `DWMWCP_DONOTROUND`; macOS: `setOpaque(false)`,
  clear background).
* **The first page is the design, pixel for pixel**: inside the frame `0 of 1,154,439` pixels
  may differ. That is a test, not a slogan.
* Nothing is a stock JUCE widget: the button pills, the checkboxes, the progress bar, the
  minimise bar and the close cross are all drawn.
* Places, in the 980 × 386 units: red `Next` pill `{479.44, 306.67, 218.89, 38.89}` r = 7.8
  with a 15-unit red glow; outlined `Cancel` pill `{719.44, 306.67, 202.22, 38.89}` r = 11.7;
  minimise glyph at `(907.78, 29.44)`, close glyph at `(942.22, 29.72)`, reach ±7; type
  column from x = 479.5 to 921.7.
* Colours: red `0xffea030f`, headline red `0xffdd0510`, white `0xffdedede`, running text
  `0xffaaa9a9`, panel `0xff040303`.
* Typeface: Red Hat Display (Regular, Medium, SemiBold, Bold, ExtraBold), OFL, built in.

## THE PAGES

| # | Page | What it holds |
|---|---|---|
| 1 | Welcome | exactly the design: `Welcome to the` / `D.A Production Installer` / two grey lines / red `Next →` + `Cancel` |
| 2 | Select the Install Location | `Select the` / `Install Location`; one row per format with a red ticked box and its folder — `Audio Unit  ~/Library/Audio/Plug-Ins/Components`, `VST3  ~/Library/Audio/Plug-Ins/VST3`; on Windows one row: `VST3  C:\Program Files\Common Files\VST3`; all ticked to begin with; `Next` off when nothing is ticked |
| 3 | Installing | `Please wait` / `Installing <PRODUCT NAME>`; the red progress bar in the design's place with `NN %`; `Copying the plug-in files…` |
| 4 | Installation complete | `Installation complete` / `<PRODUCT NAME> is Ready`; `<PRODUCT NAME> has been installed on your computer.`; `Click Finish to close the installer.`; one red `Finish` |
| 5 | Installation failed | `Installation failed` / `<PRODUCT NAME> Not Installed`; the reason; one red `Close` |

The words baked into the design never change (the company does not change); the product name
appears only where this table says.

## BEHAVIOUR

* Dragged anywhere on its face the window moves; taken within ~6 px of the frame it resizes
  and keeps its proportions, like a window of the system's.
* `--silent`: no window, installs into the standard folders, exits `0` on success and `1` on
  failure.
* `--self-test <folder>`: the installer drives itself (clicks Next, Next, Finish, drags its
  frame, moves, minimises, restores), and writes a report and a picture of every page.
* A re-install replaces the old copy **completely** (remove, then copy); what lands on disk
  is compared **file for file** with what was built.
* Errors (a folder that cannot be written, a payload that is not there) land on page 5 with
  the reason — never a crash, never a silent no-op.

## WHAT THE INSTALLER CARRIES

* A build-time target, depending on the plug-in targets, zips them as
  `VST3/<PRODUCT NAME>.vst3/…` and (macOS) `AU/<PRODUCT NAME>.component/…`, **after** code
  signing and after the VST3 `moduleinfo.json` is written.
* **Windows:** the zip becomes an `RCDATA` resource (`payload.rc`) inside one `.exe`; the
  manifest says `requestedExecutionLevel level="requireAdministrator"` (`/MANIFESTUAC`),
  because the VST3 folder belongs to administrators; the MSVC runtime is linked statically.
* **macOS:** the zip goes to `Contents/Resources/payload.zip` inside the `.app`, and the outer
  bundle is re-signed ad hoc (`codesign --force --sign -`) after the copy; Universal
  `arm64;x86_64`, deployment target 11.0; the plug-in bundles are ad-hoc signed too.
* `juce_add_binary_data(… HEADER_NAME InstallerData.h NAMESPACE InstallerData …)` — the
  sources `#include <InstallerData.h>`. **Without `HEADER_NAME` the build dies with
  `Cannot open include file: 'InstallerData.h'`.** This is the one trap here.
* Pictures come from `Installer/Design/make-assets.py` (Pillow + numpy). With the design
  unchanged, `picture.sum` must still read `d2fa580a`. Change the design and every constant
  (`WINDOW`, `TEXT`, `BUTTONS`, the frame, the numbers in `InstallerView.cpp`) must be
  re-measured and everything regenerated.

## PROOF — BEFORE YOU SAY IT IS DONE

1. `bash Tests/run-installer-harness.sh /tmp/pages` prints `[PASS]` for every page and
   `ALL PASSED`, and writes a picture of every page (welcome, location, location-Windows,
   installing, finished, failed, smallest, largest, 100 %). Look at them.
2. `Tests/run-installer-harness.sh --program /tmp/payload.zip --silent --vst3-dir /tmp/out`
   with a zip of dummy bundles: they must arrive intact.
3. CI green on **both** jobs: Release build; macOS `lipo -verify_arch arm64 x86_64` on all
   three binaries and `codesign --verify --deep --strict` on the AU, the VST3 and the `.app`;
   `pluginval` strictness 10 with seeds `0x3` and `0x7`; `auval -v aufx <PLUGIN CODE>
   <MANUFACTURER CODE>` on the **installed** AU; the installer taken out of its uploaded zip,
   put alone on the Desktop with the build folder out of reach, and run there — the welcome
   page matches the design (`0 of 1154439`), nothing of the window is seen around its frame,
   the installed bundle compares file for file; then a `--silent` run that replaces it and
   compares again; and the artifacts uploaded (Windows installer, macOS installer, plain
   VST3 / AU+VST3 zips, installer-pictures zips).
4. The token grep above prints nothing, and the README names the new files.

## NEVER

* Never redraw, re-layout or "improve" the design, and never replace it with a normal system
  window — the frame, the type, the colours and the pixel test stay.
* Never change the words baked into the picture.
* Never ask for a Google Drive link or an `.exe` to analyse: **the attached picture is the
  master**. If something is genuinely missing, ask one short question instead of guessing.
* Never leave one platform failing, and never call it finished while CI is red.
* Never commit certificates, signing identities or secrets — ad-hoc signing only.

## DONE MEANS

A branch where the build workflow is green on both jobs with those six artifacts; the
installer opens as the design, installs into the standard folders on a clean machine,
replaces an older copy cleanly, and no trace of the reference product's name is left in the
tree.
