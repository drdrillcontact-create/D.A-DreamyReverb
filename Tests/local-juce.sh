# Sourced by Tests/run-harness.sh - not meant to be run on its own.
#
# Lets a Linux machine that has only git and g++ compile against JUCE: no CMake, and no X11 or
# FreeType development packages. JUCE and the headers it asks for are cloned, and the JUCE
# modules are compiled once (about two minutes on two cores; after that, seconds).
# None of this is used to build the plug-in itself - that is what CMakeLists.txt is for.
#
# Everything is kept in $DAAURA_LOCAL (default: /tmp/daaura-local).

LOCAL_ROOT="${DAAURA_LOCAL:-/tmp/daaura-local}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# What CMakeLists.txt says the product is called, who makes it, and its version
cmake_value() { sed -n "s/^ *set *( *$1 *\"\(.*\)\" *) *\$/\1/p" "$REPO_ROOT/CMakeLists.txt" | head -n 1; }
PRODUCT_NAME="$(cmake_value DAAURA_PRODUCT_NAME)"
COMPANY_NAME="$(cmake_value DAAURA_COMPANY_NAME)"
PRODUCT_VERSION="$(sed -n 's/^ *project *(.* VERSION *\([0-9.]*\).*/\1/p' "$REPO_ROOT/CMakeLists.txt" | head -n 1)"

# The JUCE version is the one the plug-in is built with
JUCE_TAG="$(sed -n 's/^ *GIT_TAG *\([^ ]*\).*/\1/p' "$REPO_ROOT/CMakeLists.txt" | head -n 1)"
JUCE_DIR="$LOCAL_ROOT/JUCE-$JUCE_TAG"
JUCE_DEPS="$LOCAL_ROOT/deps"

# The modules that get compiled
JUCE_MODULES="juce_core juce_events juce_graphics juce_data_structures juce_audio_basics juce_gui_basics juce_gui_extra juce_audio_processors_headless juce_audio_processors"

# (JUCE_MODAL_LOOPS_PERMITTED lets the harness run the message loop for a while and carry on,
# which is how it lets the editor's timer and button clicks happen. The plug-in does not use it.)
JUCE_DEFS="-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DDEBUG=1 -D_DEBUG=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_MODAL_LOOPS_PERMITTED=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_FONTCONFIG=0 -DJUCE_ALSA=0 -DJUCE_JACK=0 -DJUCE_PLUGINHOST_VST3=0 -DJUCE_PLUGINHOST_LV2=0 -DJUCE_PLUGINHOST_LADSPA=0 -DJUCE_PLUGINHOST_VST=0 -DJUCE_USE_OGGVORBIS=0 -DJUCE_USE_FLAC=0 -DJUCE_USE_MP3AUDIOFORMAT=0 -DJUCE_USE_WINDOWS_MEDIA_FORMAT=0 -DJUCE_DISABLE_JUCE_VERSION_PRINTING=1 -DJUCE_CHECK_MEMORY_LEAKS=1"
for m in $JUCE_MODULES juce_audio_devices juce_audio_formats juce_audio_utils; do JUCE_DEFS="$JUCE_DEFS -DJUCE_MODULE_AVAILABLE_$m=1"; done

JUCE_INCLUDES="-I$JUCE_DIR/modules -I$JUCE_DEPS/freetype/include -I$JUCE_DEPS/x11"
JUCE_CXXFLAGS="-std=c++17 -O1 -g0 -w -fPIC -pthread $JUCE_DEFS $JUCE_INCLUDES"
JUCE_CFLAGS="-O1 -g0 -w -fPIC -pthread $JUCE_DEFS $JUCE_INCLUDES"

# The compiled modules are kept per JUCE version and per set of options, so that changing
# either builds them afresh instead of mixing old objects with new ones
JUCE_OBJ="$LOCAL_ROOT/obj-$JUCE_TAG-$(printf '%s' "$JUCE_DEFS" | cksum | cut -d ' ' -f 1)"

# The FreeType runtime library is on practically every Linux; only its headers are fetched
JUCE_LIBS="$(gcc -print-file-name=libfreetype.so.6) -ldl -lpthread -lrt"

# Clones what is missing and compiles the JUCE modules that have not been compiled yet
local_juce_setup()
{
    if [ -z "$JUCE_TAG" ]; then echo "local-juce.sh: no GIT_TAG found in CMakeLists.txt" >&2; return 1; fi
    if [ -z "$PRODUCT_NAME" ] || [ -z "$COMPANY_NAME" ] || [ -z "$PRODUCT_VERSION" ]; then
        echo "local-juce.sh: could not read the product name, the company name or the version from CMakeLists.txt" >&2; return 1
    fi
    if [ ! -e "$(gcc -print-file-name=libfreetype.so.6)" ]; then echo "local-juce.sh: libfreetype.so.6 is not installed" >&2; return 1; fi

    mkdir -p "$LOCAL_ROOT" "$JUCE_OBJ" "$JUCE_DEPS"
    [ -d "$JUCE_DIR" ]           || git -c advice.detachedHead=false clone -q --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git "$JUCE_DIR" || return 1
    [ -d "$JUCE_DEPS/freetype" ] || git -c advice.detachedHead=false clone -q --depth 1 --branch VER-2-12-1 https://github.com/freetype/freetype.git "$JUCE_DEPS/freetype" || return 1
    [ -d "$JUCE_DEPS/x11" ]      || git clone -q --depth 1 https://github.com/allyourcodebase/x11-headers.git "$JUCE_DEPS/x11" || return 1

    local sources="" missing="" m f
    for m in $JUCE_MODULES; do
        for f in "$JUCE_DIR/modules/$m/$m"*.cpp "$JUCE_DIR/modules/$m/$m"*.c; do
            [ -f "$f" ] || continue
            sources="$sources $f"
            [ -f "$JUCE_OBJ/$(basename "$f").o" ] || missing="$missing $f"
        done
    done

    [ -z "$missing" ] && return 0

    echo "local-juce.sh: compiling $(echo $missing | wc -w) JUCE source files into $JUCE_OBJ (once)"
    export JUCE_OBJ JUCE_CXXFLAGS JUCE_CFLAGS
    printf '%s\n' $missing | xargs -P "$(nproc)" -I{} bash -c '
        f="{}"; o="$JUCE_OBJ/$(basename "$f").o"
        case "$f" in
            *.cpp) g++ $JUCE_CXXFLAGS -c "$f" -o "$o.part" 2> "$o.log" ;;
            *)     gcc $JUCE_CFLAGS   -c "$f" -o "$o.part" 2> "$o.log" ;;
        esac && mv "$o.part" "$o" && rm -f "$o.log"
        exit 0'

    for f in $sources; do
        if [ ! -f "$JUCE_OBJ/$(basename "$f").o" ]; then
            echo "local-juce.sh: could not compile $f" >&2
            grep -m 6 "error" "$JUCE_OBJ/$(basename "$f").o.log" >&2
            return 1
        fi
    done
}
