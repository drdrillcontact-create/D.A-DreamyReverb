# Short prompt — same task, one page (English + فارسی)

Use this when you want to paste something small. It is the same task as
`INSTALLER_PROMPT_EN.md` / `INSTALLER_PROMPT_FA.md`, compressed.

---

## English

Build the **D.A Production installer** for `<PRODUCT NAME>` in this repo
(`<repo>`, branch `<branch>`), exactly as it already exists for D.A AURA.

Attached is the master design `Installer.png`, 1857 × 847. It is the only truth: crop it at
(58, 86) → 1764 × 695 px, 1.8 px per logical unit → the window is laid out for 980 × 386
units and the whole thing scales when resized (aspect locked). The first page must be that
picture **pixel for pixel** (inside the frame `0 of 1154439` pixels may differ). The frame
is an octagon (inside the crop: 14, 14, 1751, 679, corners cut 45°); outside it the window
is transparent — no title bar, no border, no shadow. Nothing is a stock JUCE widget; pills,
ticks, progress bar and the min/close glyphs are all drawn. Pages: Welcome → Select the
Install Location → Installing → Installation complete / failed. `--silent` installs headless
(exit 0/1); `--self-test <folder>` drives itself and writes screenshots; a re-install
replaces the old copy completely and the result is compared file for file with the build.

**Port, don't invent** — clone the working implementation and rename tokens:

```sh
git clone --depth 1 -b arena/03e643f9-d-a-dreamyreverb \
    https://github.com/drdrillcontact-create/D.A-DreamyReverb.git /tmp/ref
git -C /tmp/ref fetch --depth 1 origin 1d65f6a63f3797472642f075a8564b300083740a
git -C /tmp/ref checkout 1d65f6a63f3797472642f075a8564b300083740a
```

Take `Installer/`, `.github/scripts/`, `Tests/`, and the installer part of
`CMakeLists.txt` + `.github/workflows/build.yml`; rename `daaura` → `<ns>`,
`DAAURA_` → `<NS>_`, `"D.A AURA"` → `"<PRODUCT NAME>"`, bundle ids → yours; then
`grep -rniE 'daaura|D\.A\.? AURA|dreamy|cloudveil|lune' . --exclude-dir=.git` must be empty.

Windows: payload zip as an `RCDATA` resource in one `.exe`, manifest
`requireAdministrator`, static CRT. macOS: payload at `Contents/Resources/payload.zip`,
Universal `arm64;x86_64`, deployment target 11.0, ad-hoc `codesign` on everything.
**Trap:** `juce_add_binary_data(... HEADER_NAME InstallerData.h ...)` — without
`HEADER_NAME` the build fails on `Cannot open include file: 'InstallerData.h'`.

Prove it before saying it is done: `bash Tests/run-installer-harness.sh /tmp/pages` prints
`ALL PASSED` and writes every page as a picture for you to look at; then CI green on both
jobs — build, `lipo`, `codesign --verify`, `pluginval` (strictness 10, seeds 0x3/0x7),
`auval -v aufx <code> <maker>`, the installer run alone on the Desktop with the build folder
out of reach (welcome page matches the design, transparent around the frame, installed bundle
identical file for file), then a `--silent` run that replaces it — and the installer, plug-in
and installer-pictures artifacts uploaded.

Do not redraw or restyle the design, do not swap it for a normal window, do not change the
words baked into the picture, and do not ask for a Google Drive link or an `.exe` to
analyse — the attached picture is the master.

---

## فارسی

**اینستالر D.A Production** را برای «<نام محصول>» در همین ریپو (`<repo>`، شاخهٔ
`<branch>`) بساز، دقیقاً همان‌طور که برای D.A AURA ساخته شده است.

عکس ضمیمه، طراحی اصلی `Installer.png` با اندازهٔ ۱۸۵۷ × ۸۴۷ است و تنها مرجع است: برش در
(58, 86) با اندازهٔ 1764 × 695 پیکسل، هر واحد منطقی ۱.۸ پیکسل، پس چیدمان برای ۹۸۰ × ۳۸۶
واحد است و با تغییر اندازهٔ پنجره کل چیدمان مقیاس می‌شود (نسبت ثابت). صفحهٔ اول باید
**پیکسل‌به‌پیکسل** همان عکس باشد (داخل قاب `0 of 1154439`). قاب هشت‌ضلعی است (داخل برش:
۱۴، ۱۴، ۱۷۵۱، ۶۷۹، گوشه‌ها با ۴۵ درجه بریده)؛ بیرون آن پنجره شفاف است — بدون نوار عنوان،
بدون حاشیه، بدون سایه. هیچ‌چیز ویجت آمادهٔ JUCE نیست؛ همه کشیده می‌شود. صفحه‌ها:
Welcome → Select the Install Location → Installing → Installation complete / failed.
`--silent` بدون پنجره نصب می‌کند (خروجی ۰/۱)؛ `--self-test <پوشه>` خودش را اجرا می‌کند و
اسکرین‌شات می‌نویسد؛ نصب مجدد نسخهٔ قبلی را کامل جایگزین می‌کند و نتیجه فایل‌به‌فایل با
بیلد مقایسه می‌شود.

**پورت کن، از صفر نساز** — پیاده‌سازی کارکرده را کلون کن و جای‌نگهدارها را عوض کن:

```sh
git clone --depth 1 -b arena/03e643f9-d-a-dreamyreverb \
    https://github.com/drdrillcontact-create/D.A-DreamyReverb.git /tmp/ref
git -C /tmp/ref fetch --depth 1 origin 1d65f6a63f3797472642f075a8564b300083740a
git -C /tmp/ref checkout 1d65f6a63f3797472642f075a8564b300083740a
```

`Installer/`، `.github/scripts/`، `Tests/` و بخش اینستالر از `CMakeLists.txt` و
`.github/workflows/build.yml` را بردار؛ `daaura` → `<ns>`، `DAAURA_` → `<NS>_`،
`"D.A AURA"` → `"<نام محصول>"` و شناسهٔ باندل‌ها را عوض کن؛ بعد
`grep -rniE 'daaura|D\.A\.? AURA|dreamy|cloudveil|lune' . --exclude-dir=.git` باید خالی
باشد.

ویندوز: زیپ payload به‌صورت `RCDATA` داخل یک `.exe`، مانیفست `requireAdministrator`،
CRT استاتیک. مک: payload در `Contents/Resources/payload.zip`، Universal
`arm64;x86_64`، deployment target ۱۱.۰ و `codesign` ad hoc برای همه.
**تله:** `juce_add_binary_data(... HEADER_NAME InstallerData.h ...)` — بدون `HEADER_NAME`
بیلد با `Cannot open include file: 'InstallerData.h'` می‌خورد.

قبل از گفتن «تمام شد» اثبات کن: `bash Tests/run-installer-harness.sh /tmp/pages` باید
`ALL PASSED` بدهد و عکس همهٔ صفحه‌ها را بنویسد تا خودت ببینی؛ بعد CI روی هر دو job سبز
شود — بیلد، `lipo`، `codesign --verify`، `pluginval` (strictness ۱۰، seedهای 0x3/0x7)،
`auval -v aufx <کد> <سازنده>`، اجرای اینستالر به‌تنهایی روی Desktop با پوشهٔ بیلد خارج از
دسترس (صفحهٔ اول مطابق طراحی، بیرون قاب شفاف، باندل نصب‌شده فایل‌به‌فایل یکسان)، بعد
اجرای `--silent` که جایگزینش می‌کند — و آپلود آرتیفکت‌های اینستالر، پلاگین و عکس‌ها.

طراحی را دوباره نکش و استایلش را عوض نکن، جایش پنجرهٔ معمولی سیستم نگذار، واژه‌های داخل
عکس را تغییر نده، و لینک گوگل‌درایو یا فایل exe برای تحلیل نخواه — عکس ضمیمه مرجع است.
