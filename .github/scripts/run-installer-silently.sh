#!/usr/bin/env bash
# Runs the installer once more, without its window (--silent), over the copy that is already
# installed, and looks at what is there afterwards: the plug-in as it was built, and nothing
# left over from replacing the old copy.
#
# Usage: run-installer-silently.sh <installer program> <label> <built>=<installed> [...]
# (OUT_OF_REACH as for run-installer.sh.)
set -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
source "$here/alone.sh"
installer="$(cd "$(dirname "$1")" 2> /dev/null && pwd)/$(basename "$1")"
label="$2"
shift 2

log="$(pwd)/installer-silent.log"
: > "$log"

if [ ! -f "$installer" ]; then
  echo "::error title=There is no installer to run ($label)::There is no file $installer"
  exit 1
fi

home="$(home_of "$installer")"
status=0

if [ -n "$OUT_OF_REACH" ]; then
  if put_out_of_reach "$OUT_OF_REACH" >> "$log"; then
    trap 'bring_back "$OUT_OF_REACH"' EXIT
  else
    echo "::warning title=The build could not be moved out of the installer's reach ($label)::The folder \"$OUT_OF_REACH\" is in use. The installer was still run on its own, from $home."
    OUT_OF_REACH=""
  fi
fi

(cd "$home" && exec "$installer" --silent --log "$log") || status=$?
echo "exit code $status" >> "$log"

if [ -n "$OUT_OF_REACH" ]; then
  bring_back "$OUT_OF_REACH" >> "$log" || status=1
  trap - EXIT
fi

for pair in "$@"; do
  built="${pair%%=*}"
  installed="${pair#*=}"

  if ! diff -r "$built" "$installed" >> "$log" 2>&1; then
    echo "compare: [FAIL] what it installed is not what was built: $installed" >> "$log"
    status=1
  elif [ "$(uname -s)" = "Darwin" ] && ! codesign --verify --deep --strict "$installed" >> "$log" 2>&1; then
    # (a plug-in whose signature did not survive would not load on Apple Silicon)
    echo "compare: [FAIL] what it installed is what was built, but its signature no longer holds: $installed" >> "$log"
    status=1
  else
    echo "compare: [PASS] the copy it replaced is again what was built, file for file: $installed" >> "$log"
  fi

  left="$(ls "$(dirname "$installed")" | grep -c -E '\.(installing|previous)' || true)"
  echo "left-overs of the replacement in $(dirname "$installed"): $left" >> "$log"
  [ "$left" = "0" ] || status=1
done

cat "$log"

if [ "$status" -ne 0 ]; then
  bash "$here/annotate.sh" "The installer failed when run without its window ($label)" "$log"
  exit 1
fi

bash "$here/annotate.sh" "The installer replaced the installed copy ($label)" "$log" . notice
