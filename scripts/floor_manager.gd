class_name FloorManager
extends Node
## Loads/unloads floors and drives the descent (SPEC §3/§6). The swap
## happens mid-ride behind closed elevator doors: the player keeps their
## position relative to the cab while the world changes around them.

const FLOOR_SUBLEVEL := preload("res://scenes/floors/Floor_Sublevel.tscn")
const RIDE_SECONDS := 2.6

var current_floor: Node3D

var _rumble := AudioStreamPlayer.new()

@onready var _world: Node3D = get_node("../SubViewport/World")


func _ready() -> void:
	_rumble.volume_db = -6.0
	add_child(_rumble)
	current_floor = _world.get_node("Floor_Lobby")
	GameState.current_floor_name = String(current_floor.name)
	GameState.mark_room_seen(String(current_floor.name))
	_hook(current_floor)


func _hook(floor_node: Node3D) -> void:
	var elevator: Elevator = floor_node.get_node_or_null("Props/Elevator")
	if elevator != null:
		elevator.descent_requested.connect(_on_descent_requested)


func _scene_for_depth(_depth: int) -> PackedScene:
	# Step 9 routes the final depth back to a wrong lobby; until then the
	# sublevels just keep coming.
	return FLOOR_SUBLEVEL


func _on_descent_requested(elevator: Elevator) -> void:
	var player: Node3D = GameState.player
	await elevator.close_doors()
	_rumble.stream = SoundSynth.elevator_rumble(RIDE_SECONDS)
	_rumble.play()
	await get_tree().create_timer(RIDE_SECONDS * 0.55, false).timeout
	# Swap the world while the doors are shut.
	var rel: Transform3D = elevator.global_transform.affine_inverse() * player.global_transform
	var next: Node3D = _scene_for_depth(GameState.descent_depth + 1).instantiate()
	_retire(current_floor)
	current_floor = next
	_world.add_child(next)
	var new_elevator: Elevator = next.get_node("Props/Elevator")
	player.global_transform = new_elevator.global_transform * rel
	GameState.descend()
	GameState.current_floor_name = String(next.name)
	GameState.mark_room_seen(String(next.name))
	LoopController.apply(next, GameState.descent_depth)
	_hook(next)
	await get_tree().create_timer(RIDE_SECONDS * 0.45, false).timeout
	_rumble.stop()
	new_elevator.ding()
	await new_elevator.open_doors()


## Dev helper: jump straight onto a sublevel floor (no ride), keeping
## whatever descent_depth is already set (see --depth/--sublevel flags).
## queue_free is deferred, so rename the outgoing floor or the incoming
## instance gets auto-renamed on add_child (breaking name-based tracking).
func _retire(floor_node: Node3D) -> void:
	floor_node.name = String(floor_node.name) + "_outgoing"
	floor_node.queue_free()


func dev_swap_to_sublevel() -> void:
	var next: Node3D = FLOOR_SUBLEVEL.instantiate()
	_retire(current_floor)
	current_floor = next
	_world.add_child(next)
	GameState.current_floor_name = String(next.name)
	GameState.mark_room_seen(String(next.name))
	LoopController.apply(next, maxi(GameState.descent_depth, 1))
	_hook(next)
	GameState.player.global_position = \
			next.get_node("SpawnPoint").global_position
