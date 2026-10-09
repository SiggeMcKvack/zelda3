#!/bin/bash
# bundle-macos.sh <path/to/Zelda3.app>
# Post-processes the Tauri-built app: copies the Homebrew dylibs that the launcher
# (yaml, opus, samplerate) and the game sidecar (SDL2, opus, vulkan, ...) link against
# into Contents/Frameworks, patches their load paths, and re-signs ad-hoc.
# Requires: dylibbundler (brew install dylibbundler)
set -euo pipefail

APP="${1:?Usage: $0 <path/to/Zelda3.app>}"
BINS=("$APP/Contents/MacOS/zelda3-launcher" "$APP/Contents/MacOS/zelda3-game")

FIX_ARGS=()
for bin in "${BINS[@]}"; do FIX_ARGS+=(-x "$bin"); done
dylibbundler -b -of -cd "${FIX_ARGS[@]}" -d "$APP/Contents/Frameworks/" \
    -p @executable_path/../Frameworks/ -s "$(brew --prefix)/lib"

# Homebrew's SDL2 is sdl2-compat, which dlopen()s libSDL3.dylib from its own dir;
# dylibbundler can't see a dlopen, so ship SDL3 alongside it explicitly.
SDL3="$(brew --prefix)/lib/libSDL3.dylib"
if [ -f "$APP/Contents/Frameworks/libSDL2-2.0.0.dylib" ] && [ -f "$SDL3" ]; then
    cp -L "$SDL3" "$APP/Contents/Frameworks/libSDL3.dylib"
    chmod u+w "$APP/Contents/Frameworks/libSDL3.dylib"
    install_name_tool -id @executable_path/../Frameworks/libSDL3.dylib "$APP/Contents/Frameworks/libSDL3.dylib"
fi

# dylibbundler leaves Homebrew rpaths behind; keep exactly one pointing at Frameworks.
for bin in "${BINS[@]}"; do
    while IFS= read -r rpath; do
        install_name_tool -delete_rpath "$rpath" "$bin" 2>/dev/null || true
    done < <(otool -l "$bin" | awk '/cmd LC_RPATH/{found=1} found && /path /{print $2; found=0}')
    install_name_tool -add_rpath "@executable_path/../Frameworks/" "$bin"
done

# install_name_tool invalidates signatures; re-sign the whole bundle ad-hoc.
codesign --force --deep --sign - "$APP"

# Fail loudly if anything still points outside the bundle.
if otool -L "${BINS[@]}" "$APP/Contents/Frameworks/"*.dylib | grep -E "$(brew --prefix)|/usr/local/(opt|Cellar)"; then
    echo "ERROR: unbundled Homebrew dylib references remain" >&2
    exit 1
fi
echo "Bundled dylibs into: $APP"
