#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ELF="$ROOT/build/sdlpop.elf"
PKG_ROOT="$ROOT/pkgbuild"
GAME_DIR="$PKG_ROOT/PS3_GAME"
USR_DIR="$GAME_DIR/USRDIR"
OUT_DIR="$ROOT/dist"
TITLE_ID="SDLP30001"
CONTENT_ID="UP0001-${TITLE_ID}_00-0000000000000000"
PKG_NAME="SDLPoP_PS3.pkg"
PKG_NAME_SPACED="SDLPoP PS3.pkg"

if [ ! -f "$ELF" ]; then
	echo "ELF mancante: $ELF. Eseguire prima ./build.sh" >&2
	exit 1
fi

if [ ! -x /mnt/d/ps3dev-sdk/ppu/bin/powerpc64-ps3-elf-gcc ]; then
	if ! command -v wsl.exe >/dev/null 2>&1; then
		echo "Toolchain PS3 non eseguibile e WSL non disponibile." >&2
		exit 1
	fi
	WROOT="$(wsl.exe -e wslpath -a "$ROOT" | tr -d '\r')"
	exec wsl.exe -e bash -lc "cd '$WROOT' && bash ./package.sh $*"
fi

export PS3DEV="${PS3DEV:-/mnt/d/ps3dev-sdk}"
export PSL1GHT="${PSL1GHT:-$PS3DEV}"
export PATH="$PS3DEV/bin:$PS3DEV/ppu/bin:$PATH"
PYTHON="${PYTHON:-python3}"

if [ ! -f "$ROOT/assets/ICON0.PNG" ] || [ ! -f "$ROOT/assets/PIC1.PNG" ]; then
	"$PYTHON" "$ROOT/assets/make_assets.py"
fi

rm -rf "$GAME_DIR"
mkdir -p "$USR_DIR" "$OUT_DIR"

cp -r "$ROOT/src/data" "$USR_DIR/data"
rm -rf "$USR_DIR/data/font"
mkdir -p "$USR_DIR/screenshots" "$USR_DIR/replays"
cp "$ROOT/SDLPoP.ini" "$USR_DIR/SDLPoP.ini"
cp "$ROOT/assets/ICON0.PNG" "$GAME_DIR/ICON0.PNG"
cp "$ROOT/assets/PIC1.PNG" "$GAME_DIR/PIC1.PNG"

cp "$ELF" "$PKG_ROOT/sdlpop.elf"
powerpc64-ps3-elf-strip "$PKG_ROOT/sdlpop.elf"
sprxlinker "$PKG_ROOT/sdlpop.elf"
(cd "$USR_DIR" && make_self_npdrm "$PKG_ROOT/sdlpop.elf" EBOOT.BIN "$CONTENT_ID")
rm -f "$PKG_ROOT/sdlpop.elf"

if command -v sfo >/dev/null 2>&1; then
	sfo -f "$ROOT/sfo.xml" "$GAME_DIR/PARAM.SFO" --title="SDLPoP PS3" --appid="$TITLE_ID"
elif [ -x "$PS3DEV/bin/sfo" ]; then
	"$PS3DEV/bin/sfo" -f "$ROOT/sfo.xml" "$GAME_DIR/PARAM.SFO" --title="SDLPoP PS3" --appid="$TITLE_ID"
else
	"$PYTHON" "$ROOT/tools/sfo.py" -f "$ROOT/sfo.xml" "$GAME_DIR/PARAM.SFO"
fi

if command -v pkg >/dev/null 2>&1; then
	pkg --contentid="$CONTENT_ID" "$GAME_DIR/" "$OUT_DIR/$PKG_NAME"
elif [ -x "$PS3DEV/bin/pkg" ]; then
	"$PS3DEV/bin/pkg" --contentid="$CONTENT_ID" "$GAME_DIR/" "$OUT_DIR/$PKG_NAME"
else
	"$PYTHON" "$ROOT/tools/pkg_ps3.py" --contentid="$CONTENT_ID" "$GAME_DIR" "$OUT_DIR/$PKG_NAME"
fi

cp -f "$OUT_DIR/$PKG_NAME" "$OUT_DIR/$PKG_NAME_SPACED"

GN_PKG="$OUT_DIR/${PKG_NAME%.pkg}.gnpdrm.pkg"
GN_PKG_SPACED="$OUT_DIR/${PKG_NAME_SPACED%.pkg}.gnpdrm.pkg"
cp -f "$OUT_DIR/$PKG_NAME" "$GN_PKG"
if command -v package_finalize >/dev/null 2>&1; then
	package_finalize "$GN_PKG"
elif [ -x "$PS3DEV/bin/package_finalize" ]; then
	"$PS3DEV/bin/package_finalize" "$GN_PKG"
fi
cp -f "$GN_PKG" "$GN_PKG_SPACED"

echo "Packages ready in $OUT_DIR:"
ls -lh "$OUT_DIR"/*.pkg
