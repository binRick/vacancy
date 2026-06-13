#!/usr/bin/env bash
# Build Vacancy (raylib 6 port) into build/vacancy.
#
# Self-bootstrapping: clones raylib 6.0 into vendor/ and builds the static lib
# on first run, then compiles the game. Requires a C compiler, make, and git.
set -euo pipefail
cd "$(dirname "$0")"

RAYLIB_DIR="vendor/raylib"
RAYLIB_VER="6.0"

if [[ ! -f "$RAYLIB_DIR/src/libraylib.a" ]]; then
	if [[ ! -d "$RAYLIB_DIR" ]]; then
		echo "==> Cloning raylib $RAYLIB_VER"
		git clone --depth 1 --branch "$RAYLIB_VER" https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
	fi
	echo "==> Building raylib static library"
	make -C "$RAYLIB_DIR/src" PLATFORM=PLATFORM_DESKTOP -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

echo "==> Building vacancy"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
echo "==> Done: build/vacancy"
