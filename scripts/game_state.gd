extends Node
## Autoload: GameState — owns descent progress, room memory, and event flags.
## floor_manager.gd (later) reads this to decide what to spawn next.

signal depth_changed(new_depth: int)
signal flag_set(flag: String, value: Variant)

var descent_depth: int = 0
var seen_rooms: Dictionary = {}
var flags: Dictionary = {}

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


func mark_room_seen(room_id: String) -> void:
	seen_rooms[room_id] = seen_rooms.get(room_id, 0) + 1
	Telemetry.event("room_seen", {"room": room_id, "times": seen_rooms[room_id]})


## One-line description of the whole game state, for the telemetry heartbeat.
func snapshot() -> Dictionary:
	var snap := {
		"depth": descent_depth,
		"rooms_seen": seen_rooms.size(),
		"flags": flags,
	}
	if is_instance_valid(player):
		var pos: Vector3 = player.global_position
		snap["player"] = {
			"pos": [snappedf(pos.x, 0.01), snappedf(pos.y, 0.01), snappedf(pos.z, 0.01)],
			"yaw_deg": snappedf(player.global_rotation_degrees.y, 0.1),
		}
	return snap
