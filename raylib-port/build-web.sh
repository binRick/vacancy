#!/usr/bin/env bash
# Build Vacancy (raylib 6 port) to WebAssembly with Emscripten.
#
# Output: web/vacancy.{js,wasm,data}, loaded by the hand-written web/index.html.
# Self-bootstrapping: clones raylib 6.0 into vendor/raylib-web/ and builds the
# WebGL2 (OpenGL ES 3) static lib on first run, then compiles the game with emcc.
#
# Requires: emscripten (emcc/emmake) and git. Homebrew's emscripten ships a
# wrapper that mis-sets the python var, so we pin EMSDK_PYTHON to a 3.10+ python.
set -euo pipefail
cd "$(dirname "$0")"

# Homebrew emscripten needs EMSDK_PYTHON pointing at python >= 3.10.
if [[ -z "${EMSDK_PYTHON:-}" ]]; then
	for py in python3.14 python3.13 python3.12 python3.11 python3.10; do
		if command -v "$py" >/dev/null 2>&1; then export EMSDK_PYTHON="$(command -v "$py")"; break; fi
	done
fi

RAYLIB_DIR="vendor/raylib-web"
RAYLIB_LIB="$RAYLIB_DIR/src/libraylib.web.a"
RAYLIB_VER="6.0"

if [[ ! -f "$RAYLIB_LIB" ]]; then
	if [[ ! -d "$RAYLIB_DIR" ]]; then
		echo "==> Cloning raylib $RAYLIB_VER (web)"
		git clone --depth 1 --branch "$RAYLIB_VER" https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
	fi
	echo "==> Building raylib web static library (PLATFORM_WEB, OpenGL ES 3 / WebGL2)"
	emmake make -C "$RAYLIB_DIR/src" \
		PLATFORM=PLATFORM_WEB \
		GRAPHICS=GRAPHICS_API_OPENGL_ES3 \
		-j"$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN || echo 4)"
fi

# --- stage shaders for WebGL2 -------------------------------------------------
# The desktop shaders are #version 330 (desktop GL). They are already written in
# the modern in/out/texture()/finalColor style, so the only change WebGL2 needs
# is the version directive plus the required float/int precision qualifiers.
STAGE="build-web/shaders"
echo "==> Staging WebGL2 shaders -> $STAGE"
rm -rf "$STAGE"; mkdir -p "$STAGE"
for f in shaders/*.vs shaders/*.fs; do
	out="$STAGE/$(basename "$f")"
	printf '#version 300 es\nprecision highp float;\nprecision highp int;\n' > "$out"
	# drop the original "#version 330" line, keep the rest verbatim
	tail -n +2 "$f" >> "$out"
done

# --- compile the game ---------------------------------------------------------
mkdir -p web
echo "==> Compiling vacancy -> web/vacancy.js"
emcc \
	src/*.c \
	-o web/vacancy.js \
	-std=c11 -O3 \
	-DPLATFORM_WEB \
	-I "$RAYLIB_DIR/src" \
	"$RAYLIB_LIB" \
	-s USE_GLFW=3 \
	-s MIN_WEBGL_VERSION=2 \
	-s MAX_WEBGL_VERSION=2 \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s STACK_SIZE=4mb \
	-s FORCE_FILESYSTEM=1 \
	-s EXPORTED_RUNTIME_METHODS=ccall \
	--preload-file "$STAGE@shaders"

echo "==> Done:"
ls -lh web/vacancy.js web/vacancy.wasm web/vacancy.data
