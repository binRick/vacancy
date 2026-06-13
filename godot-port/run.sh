#!/usr/bin/env bash
# Compile (import) and run Vacancy with gamestate telemetry enabled.
#
# Telemetry: while the game runs, run.log (JSON lines) gets a "tick" event
# every second with the full game state, plus immediate discrete events.
# Tail it from another terminal:  tail -f run.log
#
# Override the binary with GODOT=/path/to/godot, the log with LOG_FILE=...
set -euo pipefail
cd "$(dirname "$0")"

find_godot() {
	if [[ -n "${GODOT:-}" ]]; then
		echo "$GODOT"
		return
	fi
	local c
	for c in godot godot4; do
		if command -v "$c" >/dev/null 2>&1; then
			echo "$c"
			return
		fi
	done
	local app
	for app in "/Applications/Godot.app/Contents/MacOS/Godot" "$HOME/Applications/Godot.app/Contents/MacOS/Godot"; do
		if [[ -x "$app" ]]; then
			echo "$app"
			return
		fi
	done
	echo "error: Godot 4 not found; install it or set GODOT=/path/to/godot" >&2
	exit 1
}

GODOT_BIN="$(find_godot)"
LOG_FILE="${LOG_FILE:-$PWD/run.log}"

echo "==> Importing resources (compile step)"
"$GODOT_BIN" --headless --path . --import --quit >/dev/null \
	|| echo "warning: import step exited nonzero (continuing)" >&2

echo "==> Running Vacancy (telemetry -> $LOG_FILE)"
exec "$GODOT_BIN" --path . -- --telemetry-log="$LOG_FILE"
