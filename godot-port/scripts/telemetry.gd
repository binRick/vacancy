extends Node
## Autoload: Telemetry — gamestate logging for outside observers.
##
## Enabled when the game is launched with `--telemetry-log=<path>` after the
## `--` separator (run.sh does this), or with VACANCY_TELEMETRY_LOG set.
## Writes JSON lines: one "tick" heartbeat per second with the full GameState
## snapshot, plus immediate discrete events (descend, flag, room_seen, ...).
## Disabled (zero cost) in a normal launch.

const HEARTBEAT_INTERVAL := 1.0

var _file: FileAccess = null
var _accum := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS  # keep heartbeats during pause
	var path := _resolve_log_path()
	if path.is_empty():
		set_process(false)
		return
	_file = FileAccess.open(path, FileAccess.WRITE)
	if _file == null:
		push_error("Telemetry: cannot open log file '%s': %s"
				% [path, error_string(FileAccess.get_open_error())])
		set_process(false)
		return
	event("start", {
		"godot": Engine.get_version_info().string,
		"date": Time.get_datetime_string_from_system(),
	})


func _resolve_log_path() -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--telemetry-log="):
			return arg.trim_prefix("--telemetry-log=")
	return OS.get_environment("VACANCY_TELEMETRY_LOG")


func _process(delta: float) -> void:
	_accum += delta
	if _accum >= HEARTBEAT_INTERVAL:
		_accum = fmod(_accum, HEARTBEAT_INTERVAL)
		_heartbeat()


func _heartbeat() -> void:
	var scene := get_tree().current_scene
	var data := {
		"fps": Engine.get_frames_per_second(),
		"paused": get_tree().paused,
		"scene": String(scene.name) if scene else "<none>",
	}
	data.merge(GameState.snapshot())
	event("tick", data)


## Log a discrete event immediately. Safe to call when telemetry is disabled.
func event(ev_name: String, data: Dictionary = {}) -> void:
	if _file == null:
		return
	var line := {"t": snappedf(Time.get_ticks_msec() / 1000.0, 0.01), "ev": ev_name}
	line.merge(data)
	_file.store_line(JSON.stringify(line))
	_file.flush()


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST or what == NOTIFICATION_EXIT_TREE:
		if _file != null:
			event("end")
			_file.close()
			_file = null
