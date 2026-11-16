#!/usr/bin/env bash
# Photographs the screen into a PNG file: all of it, or the given part and a little around it.
#
# Usage: capture-screen.sh <file.png> [<x> <y> <width> <height> [left|right]]
# Says in one line, beginning "screen:", how it went.
# (The part is given in the units the system's own tools count in: pixels on Windows, points
# on macOS. "left" or "right" takes only that half of it.)
here="$(cd "$(dirname "$0")" && pwd)"
out="$1"
x="$2"; y="$3"; w="$4"; h="$5"
half="$6"
margin=24

if [ -n "$h" ]; then
  w=$((w + 2 * margin)); h=$((h + 2 * margin))
  x=$((x - margin)); y=$((y - margin))
  if [ "$x" -lt 0 ]; then w=$((w + x)); x=0; fi
  if [ "$y" -lt 0 ]; then h=$((h + y)); y=0; fi

  case "$half" in
    left)  w=$((w / 2)) ;;
    right) x=$((x + w / 2)); w=$((w - w / 2)) ;;
  esac
fi

rm -f "$out"

case "$(uname -s)" in
  Darwin)
    if [ -n "$h" ]; then
      screencapture -x -R "$x,$y,$w,$h" "$out"
    else
      screencapture -x "$out"
    fi
    [ -s "$out" ] && echo "screen: photographed with screencapture"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$here/capture-screen.ps1")" \
      "$(cygpath -w "$out")" "${x:-0}" "${y:-0}" "${w:-0}" "${h:-0}"
    ;;
  *)
    echo "capture-screen.sh: no way to photograph the screen on $(uname -s)" >&2
    exit 1
    ;;
esac

[ -s "$out" ]
