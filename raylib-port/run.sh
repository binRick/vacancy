#!/usr/bin/env bash
# Build and run Vacancy with gamestate telemetry enabled.
#
# Telemetry: while the game runs, run.log (JSON lines) gets a "tick" event every
# second with the full game state, plus immediate discrete events. Tail it from
# another terminal:  tail -f run.log
#
# Override the log path with LOG_FILE=...; pass extra dev flags as arguments,
# e.g.  ./run.sh --depth=4 --sublevel
set -euo pipefail
cd "$(dirname "$0")"

LOG_FILE="${LOG_FILE:-$PWD/run.log}"

./build.sh >/dev/null

echo "==> Running Vacancy (telemetry -> $LOG_FILE)"
exec ./build/vacancy --telemetry-log="$LOG_FILE" "$@"
