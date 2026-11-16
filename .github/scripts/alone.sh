#!/usr/bin/env bash
# For trying the installer the way somebody gets it: as one file, with nothing of the build
# anywhere near. (To be read in with `source`.)
#
#   put_out_of_reach <folder>   renames <folder> to <folder>.out-of-reach, so that a program
#                               that looked for anything in it would find nothing
#   bring_back <folder>         undoes that
#   home_of <installer>         the folder the installer lies in (for a macOS application: the
#                               folder that holds the .app)

put_out_of_reach() {
  local folder="$1" attempt
  [ -d "$folder" ] || { echo "alone: there is no folder $folder to put out of reach"; return 1; }

  for attempt in 1 2 3 4; do
    if mv "$folder" "$folder.out-of-reach" 2> /dev/null; then
      echo "alone: the folder \"$folder\" is out of reach while the installer runs"
      return 0
    fi

    # (Windows: build tools that stay around after the build keep the folder open)
    if command -v taskkill > /dev/null 2>&1; then
      taskkill //F //IM MSBuild.exe //IM vctip.exe //IM mspdbsrv.exe > /dev/null 2>&1 || true
    fi
    sleep 2
  done

  echo "alone: the folder \"$folder\" could not be put out of reach"
  return 1
}

bring_back() {
  local folder="$1" attempt
  [ -d "$folder.out-of-reach" ] || return 0

  for attempt in 1 2 3 4 5; do
    mv "$folder.out-of-reach" "$folder" 2> /dev/null && return 0
    sleep 2
  done

  echo "alone: the folder \"$folder\" could not be brought back"
  return 1
}

home_of() {
  case "$1" in
    *.app/Contents/MacOS/*) dirname "${1%%.app/Contents/MacOS/*}.app" ;;
    *)                      dirname "$1" ;;
  esac
}
