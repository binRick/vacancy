extends Node
## Autoload: GameState — owns descent progress, room memory, and event flags.
## floor_manager.gd (later) reads this to decide what to spawn next.

signal depth_changed(new_depth: int)
signal flag_set(flag: String, value: Variant)
signal caption_shown(text: String)
signal note_opened(text: String)
signal anomaly_applied(id: String)
signal false_exit_used

var descent_depth: int = 0
var seen_rooms: Dictionary = {}
var flags: Dictionary = {}
var current_floor_name: String = ""
var current_surface: String = "tile"
var last_note_final: bool = false
var false_exit_target: Transform3D = Transform3D.IDENTITY

## Registered by the player controller on _ready (step 2). Telemetry reads it.
var player: Node3D = null


func descend() -> void:
	descent_depth += 1
	depth_changed.emit(descent_depth)
	Telemetry.event("descend", {"depth": descent_depth})


func set_flag(flag: String, value: Variant = true) -> void:
	flags[flag] = value
	flag_set.emit(flag, value)
	Telemetry.event("flag", {"flag": flag, "value": value})


func get_flag(flag: String, default: Variant = false) -> Variant:
	return flags.get(flag, default)


## Brief one-line message at the bottom of the screen ("Locked.").
func show_caption(text: String) -> void:
	caption_shown.emit(text)


## Full-screen readable note overlay; main.gd renders it and pauses the world.
## A final note ends the game when dismissed (SPEC §6).
func open_note(text: String, is_final: bool = false) -> void:
	last_note_final = is_final
	note_opened.emit(text)


func mark_room_seen(room_id: String) -> void:
	seen_rooms[room_id] = seen_rooms.get(room_id, 0) + 1
	Telemetry.event("room_seen", {"room": room_id, "times": seen_rooms[room_id]})


## One-line description of the whole game state, for the telemetry heartbeat.
func snapshot() -> Dictionary:
	var snap := {
		"depth": descent_depth,
		"floor": current_floor_name,
		"rooms_seen": seen_rooms.size(),
		"flags": flags,
		"master_db": snappedf(AudioServer.get_bus_volume_db(0), 0.1),
	}
	if is_instance_valid(player):
		var pos: Vector3 = player.global_position
		snap["player"] = {
			"pos": [snappedf(pos.x, 0.01), snappedf(pos.y, 0.01), snappedf(pos.z, 0.01)],
			"yaw_deg": snappedf(player.global_rotation_degrees.y, 0.1),
		}
		if player.has_method("telemetry_state"):
			snap["player"].merge(player.telemetry_state())
	return snap
