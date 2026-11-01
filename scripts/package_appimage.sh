#!/usr/bin/env bash
# Package a locally built DoDonPachi binary as a Steam Deck x86_64 AppImage.
#
# The binary must have been built by the player from their own ROM (see
# scripts/build-local.sh). This script never adds ROM data or generated C to the AppDir.
#
# Usage: scripts/package_appimage.sh [output.AppImage]
#   DDP_BINARY   the built binary (default: build/cmake/bin/DoDonPachi)
#   APPIMAGETOOL the appimagetool executable (default: appimagetool on PATH)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${DDP_BINARY:-$ROOT/build/cmake/bin/DoDonPachi}"
OUT="${1:-$ROOT/DoDonPachi-SteamDeck-x86_64.AppImage}"
TOOL="${APPIMAGETOOL:-appimagetool}"
APPDIR="$ROOT/build/AppDir"

if [[ ! -x "$BIN" ]]; then
    echo "package_appimage: $BIN is not executable; build it first" >&2
    exit 1
fi

rm -rf "$APPDIR"
install -Dm755 "$BIN" "$APPDIR/usr/bin/DoDonPachi"
install -Dm644 "$ROOT/packaging/dodonpachi.desktop" "$APPDIR/dodonpachi.desktop"
install -Dm644 "$ROOT/packaging/dodonpachi.svg" "$APPDIR/dodonpachi.svg"
install -Dm644 "$ROOT/packaging/dodonpachi.png" "$APPDIR/dodonpachi.png"
install -Dm644 "$ROOT/LICENSE" "$APPDIR/usr/share/doc/DoDonPachi/LICENSE"
mkdir -p "$APPDIR/usr/share/doc/DoDonPachi/LICENSES"
cp "$ROOT"/LICENSES/* "$APPDIR/usr/share/doc/DoDonPachi/LICENSES/"

# Bundle the shared libraries the binary needs (SDL2, libc are resolved by the host;
# SDL2 is bundled because SteamOS versions vary).
if command -v ldd >/dev/null 2>&1; then
    mkdir -p "$APPDIR/usr/lib"
    while read -r line; do
        soname="$(awk '{print $1}' <<<"$line")"
        path="$(awk '{print $3}' <<<"$line")"
        case "$soname" in
            libSDL2*.so*|libSDL2-2*.so*)
                [[ -f "$path" ]] && cp -L "$path" "$APPDIR/usr/lib/$soname"
                ;;
        esac
    done < <(ldd "$BIN")
    cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/DoDonPachi" "$@"
APPRUN
    chmod +x "$APPDIR/AppRun"
fi

if ! command -v "$TOOL" >/dev/null 2>&1 && [[ ! -x "$TOOL" ]]; then
    echo "package_appimage: appimagetool not found (set APPIMAGETOOL)" >&2
    exit 1
fi

ARCH=x86_64 "$TOOL" --no-appstream "$APPDIR" "$OUT"
echo "wrote $OUT"
