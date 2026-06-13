#!/usr/bin/env bash
# Export the self-contained Linux x86_64 build to build/vacancy.x86_64.
#
# Requires Godot 4.6.x AND its export templates installed (the editor
# offers this via Editor > Manage Export Templates, or drop the matching
# Godot_v<ver>_export_templates.tpz contents into the templates dir).
#
# Override the binary with GODOT=/path/to/godot.
set -euo pipefail
cd "$(dirname "$0")"

find_godot() {
	if [[ -n "${GODOT:-}" ]]; then echo "$GODOT"; return; fi
	local c
	for c in godot godot4; do
		if command -v "$c" >/dev/null 2>&1; then echo "$c"; return; fi
	done
	local app
	for app in "/Applications/Godot.app/Contents/MacOS/Godot" "$HOME/Applications/Godot.app/Contents/MacOS/Godot"; do
		if [[ -x "$app" ]]; then echo "$app"; return; fi
	done
	echo "error: Godot 4 not found; install it or set GODOT=/path/to/godot" >&2
	exit 1
}

GODOT_BIN="$(find_godot)"
OUT="build/vacancy.x86_64"
mkdir -p build

echo "==> Importing resources"
"$GODOT_BIN" --headless --path . --import --quit >/dev/null 2>&1 || true

echo "==> Exporting Linux/x86_64 release -> $OUT"
"$GODOT_BIN" --headless --path . --export-release "Linux" "$OUT"

chmod +x "$OUT"
echo "==> Done: $OUT ($(du -h "$OUT" | cut -f1)), single self-contained binary"
