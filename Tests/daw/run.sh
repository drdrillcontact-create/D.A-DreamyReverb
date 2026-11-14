#!/bin/bash
# Temporary debugging aid: installs REAPER and the plugin like a user would and runs test.lua.
# usage: run.sh <folder with the downloaded plugin artifact> [folder with the old 1.0 artifact]
set -u
PLUGIN_DIR="$1"; OLD_DIR="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$PWD/dawtest"; mkdir -p "$WORK"
VER=786

shot() {  # screenshot of the whole desktop
  if [ "$RUNNER_OS" = "Windows" ]; then
    powershell -NoProfile -Command "Add-Type -AssemblyName System.Windows.Forms,System.Drawing; \$b=[System.Windows.Forms.SystemInformation]::VirtualScreen; \$bmp=New-Object System.Drawing.Bitmap \$b.Width,\$b.Height; \$g=[System.Drawing.Graphics]::FromImage(\$bmp); \$g.CopyFromScreen(\$b.Left,\$b.Top,0,0,\$bmp.Size); \$bmp.Save('$(cygpath -w "$WORK")\\$1.png')" || echo "screenshot failed"
  else
    screencapture -x "$WORK/$1.png" || echo "screenshot failed"
  fi
}

if [ "$RUNNER_OS" = "Windows" ]; then
  curl -sSL -o reaper-install.exe "https://www.reaper.fm/files/7.x/reaper${VER}_x64-install.exe"
  ls -la reaper-install.exe
  powershell -NoProfile -Command "Start-Process -Wait -FilePath .\\reaper-install.exe -ArgumentList '/S','/PORTABLE','/D=C:\\reaper'"
  ls /c/reaper | head -30
  REAPER=/c/reaper/reaper.exe
  RES=/c/reaper
  VST3="/c/Program Files/Common Files/VST3"
  mkdir -p "$VST3"
  if [ -n "$OLD_DIR" ]; then cp -R "$OLD_DIR"/*.vst3 "$VST3/"; fi
  cp -R "$PLUGIN_DIR"/*.vst3 "$VST3/"
  ls -la "$VST3"
  find "$VST3" -type f | head -40
  export DAWTEST_OUT="$(cygpath -m "$WORK/result.txt")"
  export DAWTEST_DIR="$(cygpath -m "$WORK")"
  SCRIPT="$(cygpath -w "$HERE/test.lua")"
else
  curl -sSL -o reaper.dmg "https://www.reaper.fm/files/7.x/reaper${VER}_universal.dmg"
  ls -la reaper.dmg
  yes | PAGER=cat hdiutil attach reaper.dmg -nobrowse -mountpoint /tmp/reaperdmg > /dev/null
  ls /tmp/reaperdmg
  mkdir -p "$HOME/reaper"
  cp -R /tmp/reaperdmg/REAPER.app "$HOME/reaper/"
  touch "$HOME/reaper/reaper.ini"          # portable mode
  REAPER="$HOME/reaper/REAPER.app/Contents/MacOS/REAPER"
  RES="$HOME/reaper"
  mkdir -p staging ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
  if [ -n "$OLD_DIR" ]; then
    ditto -x -k "$OLD_DIR"/*.zip oldstaging
    cp -R oldstaging/*.component ~/Library/Audio/Plug-Ins/Components/
    cp -R oldstaging/*.vst3 ~/Library/Audio/Plug-Ins/VST3/
  fi
  ditto -x -k "$PLUGIN_DIR"/*.zip staging
  cp -R staging/*.component ~/Library/Audio/Plug-Ins/Components/
  cp -R staging/*.vst3 ~/Library/Audio/Plug-Ins/VST3/
  ls -la ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  export DAWTEST_OUT="$WORK/result.txt"
  export DAWTEST_DIR="$WORK"
  SCRIPT="$HERE/test.lua"
fi

export DAWTEST_FX="${DAWTEST_FX:-VST3:D.A Dreamy Shimmer Reverb}"

set_ini() {   # writes the VST search path into reaper.ini so REAPER does not ask on first start
  local ini="$RES/reaper.ini"
  echo "--- reaper.ini before:"; cat "$ini" 2>/dev/null | head -20
  local keys="vstpath=$VSTPATH\nvstpath64=$VSTPATH\nvstpath_arm64=$VSTPATH\nvstpath_x64=$VSTPATH"
  if grep -qi "^\[reaper\]" "$ini" 2>/dev/null; then
    awk -v k="$keys" 'BEGIN{gsub(/\\n/,"\n",k)} {print} tolower($0) ~ /^\[reaper\]/ {print k}' "$ini" > "$ini.new" && mv "$ini.new" "$ini"
  else
    printf "[REAPER]\n$keys\n" > "$ini"
  fi
  echo "--- reaper.ini after:"; cat "$ini" | head -20
}

dismiss() {   # answers "No" to the first-run question about the audio device
  echo "dismissing first-run dialog"
  if [ "$RUNNER_OS" = "Windows" ]; then
    powershell -NoProfile -Command "\$w=New-Object -ComObject WScript.Shell; \$null=\$w.AppActivate('REAPER'); Start-Sleep -Milliseconds 500; \$w.SendKeys('n')" || true
  else
    osascript -e 'tell application "System Events" to tell process "REAPER" to click button "No" of window 1' 2>&1 || \
    osascript -e 'tell application "System Events" to key code 53' 2>&1 || true
  fi
}

attempt() {   # attempt <label>
  rm -f "$WORK/result.txt"
  echo "launching REAPER ($1), fx = $DAWTEST_FX"
  "$REAPER" -nosplash -splashlog "$DAWTEST_DIR/splash-$1.log" "$SCRIPT" > "$WORK/reaper-stdout.log" 2>&1 &
  PID=$!
  for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
    sleep 5
    if [ "$i" = 3 ] || [ "$i" = 6 ]; then shot "$1-screen-$i"; fi
    if grep -q "^END" "$WORK/result.txt" 2>/dev/null; then break; fi
    if [ ! -s "$WORK/result.txt" ] && [ "$i" -ge 2 ] && [ "$i" -le 5 ]; then dismiss; fi
    if [ "$i" = 8 ] && [ ! -s "$WORK/result.txt" ]; then echo "no result after 40 s"; break; fi
    if ! kill -0 $PID 2>/dev/null; then echo "REAPER exited early"; break; fi
  done
  shot "$1-screen-final"
  if kill -0 $PID 2>/dev/null; then echo "REAPER still running: ok"; else echo "REAPER IS NOT RUNNING (crashed or quit)"; fi
  if [ "$RUNNER_OS" = "Windows" ]; then taskkill //F //IM reaper.exe > /dev/null 2>&1; else kill -9 $PID 2>/dev/null; fi
  sleep 2
  [ -s "$WORK/result.txt" ]
}

if [ "$RUNNER_OS" = "Windows" ]; then VSTPATH='C:\\Program Files\\Common Files\\VST3'; else VSTPATH="$HOME/Library/Audio/Plug-Ins/VST3"; fi
set_ini
if ! attempt portable; then
  echo "portable attempt gave no result, retrying as a normal install"
  if [ "$RUNNER_OS" = "Windows" ]; then
    powershell -NoProfile -Command "Start-Process -Wait -FilePath .\\reaper-install.exe -ArgumentList '/S'"
    REAPER="/c/Program Files/REAPER (x64)/reaper.exe"; RES="$APPDATA/REAPER"
  else
    rm -f "$HOME/reaper/reaper.ini"; RES="$HOME/Library/Application Support/REAPER"
  fi
  attempt normal
fi

echo "=========== result"
cat "$WORK/result.txt" 2>/dev/null || echo "(no result file)"
echo "=========== plugin scan files"
ls "$RES" 2>/dev/null | tr '\n' ' '; echo
for ini in "$RES"/reaper-vstplugins*.ini "$RES"/reaper-auplugins*.ini; do
  [ -f "$ini" ] || continue
  echo "--- $ini"; grep -i "dreamy" "$ini" || echo "(no entry)"
done
echo "=========== splash log"
cat "$WORK"/splash-*.log 2>/dev/null | grep -i -B1 -A1 "dreamy" | head -40
echo "=========== stdout"
tail -n 30 "$WORK/reaper-stdout.log" 2>/dev/null
exit 0
