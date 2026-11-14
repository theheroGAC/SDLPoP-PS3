#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PS3DEV_DEFAULT="/mnt/d/ps3dev-sdk"

if [ -x "$PS3DEV_DEFAULT/ppu/bin/powerpc64-ps3-elf-gcc" ]; then
	export PS3DEV="${PS3DEV:-$PS3DEV_DEFAULT}"
	export PSL1GHT="${PSL1GHT:-$PS3DEV}"
	cd "$ROOT"
	exec make -f "$ROOT/Makefile" "$@"
fi

if ! command -v wsl.exe >/dev/null 2>&1; then
	echo "La toolchain PS3 non e' eseguibile e WSL non e' disponibile." >&2
	exit 1
fi

WROOT="$(wsl.exe -e wslpath -a "$ROOT" | tr -d '\r')"
exec wsl.exe -e bash -lc "cd '$WROOT' && bash ./build.sh $*"
