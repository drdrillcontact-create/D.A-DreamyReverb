#!/usr/bin/env bash
# Puts a small copy of a picture on the workflow run page - as text: the base64 of a JPEG, cut
# into pieces that each fit into a note. That way a picture taken during a run can be looked at
# by somebody who can read the run's notes but cannot download its artifacts:
#
#   (the pieces, in order, joined together) | base64 -d > picture.jpg
#
# Usage: annotate-picture.sh <title> <picture>
# A step may leave ten notes, so this is for one picture per step, and a job fifty, so for a few
# pictures per job. About 27 000 bytes fit: the picture is made smaller and plainer until it
# does. Never fails the step.
here="$(cd "$(dirname "$0")" && pwd)"
title="$1"
picture="$2"
piece=4000                              # characters per note (GitHub cuts a note off after 4096)
limit=$(( 9 * piece * 3 / 4 - 16 ))     # the bytes that fit into nine notes

if [ ! -s "$picture" ]; then
  echo "::notice title=${title}::there is no such picture (${picture})"
  exit 0
fi

small="${picture%.*}-small.jpg"
size=0

# (width in pixels and JPEG quality: the best of these that fits)
for attempt in "1100 85" "1000 78" "900 72" "800 66" "700 58" "640 50" "560 45" "480 40" "400 35" "320 30"; do
  # shellcheck disable=SC2086
  set -- $attempt
  rm -f "$small"

  case "$(uname -s)" in
    Darwin)
      # (never enlarged)
      have="$(sips -g pixelWidth "$picture" 2> /dev/null | awk '/pixelWidth/ { print $2 }')"
      width="$1"
      if [ -n "$have" ] && [ "$have" -lt "$width" ]; then width="$have"; fi
      sips -s format jpeg -s formatOptions "$2" --resampleWidth "$width" "$picture" --out "$small" > /dev/null 2>&1
      ;;
    MINGW*|MSYS*|CYGWIN*)
      powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$here/shrink-picture.ps1")" \
        "$(cygpath -w "$picture")" "$(cygpath -w "$small")" "$1" "$2"
      ;;
    *)
      # (elsewhere: ImageMagick, where it is installed)
      convert "$picture" -resize "$1x>" -quality "$2" "$small" 2> /dev/null
      ;;
  esac

  if [ ! -s "$small" ]; then
    echo "::notice title=${title}::a small copy of ${picture} could not be made"
    exit 0
  fi

  size="$(wc -c < "$small" | tr -d ' ')"
  [ "$size" -le "$limit" ] && break
done

if [ "$size" -gt "$limit" ]; then
  echo "::notice title=${title}::even the smallest copy of ${picture} is too large for the notes (${size} bytes)"
  exit 0
fi

encoded="$(base64 < "$small" | tr -d '\r\n')"
sum="$(cksum < "$small" | cut -d ' ' -f 1)"
count=$(( (${#encoded} + piece - 1) / piece ))
i=0

while [ "$i" -lt "$count" ]; do
  echo "::notice title=${title} - part $((i + 1)) of ${count} - ${size} bytes - cksum ${sum}::${encoded:$((i * piece)):$piece}"
  i=$((i + 1))
done

exit 0
