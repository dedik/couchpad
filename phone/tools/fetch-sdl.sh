#!/bin/sh
# Pobiera źródła SDL2 do third_party/SDL2 (nie trzymamy ich w repozytorium).
set -eu
VERSION=2.30.9
DIR="$(cd "$(dirname "$0")/.." && pwd)/third_party"
if [ -f "$DIR/SDL2/CMakeLists.txt" ]; then
    echo "SDL2 już jest w $DIR/SDL2"
    exit 0
fi
mkdir -p "$DIR"
TMP="$(mktemp -d)"
curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-$VERSION.tar.gz" -o "$TMP/sdl.tar.gz"
tar -xzf "$TMP/sdl.tar.gz" -C "$TMP"
mv "$TMP/SDL2-$VERSION" "$DIR/SDL2"
rm -rf "$TMP"
echo "SDL2 $VERSION w $DIR/SDL2"
