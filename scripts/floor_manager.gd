class_name FloorManager
extends Node
## Loads/unloads floors and drives the descent (SPEC §3/§6). The swap
## happens mid-ride behind closed elevator doors: the player keeps their
## position relative to the cab while the world changes around them.

const FLOOR_SUBLEVEL := preload("res://scenes/floors/Floor_Sublevel.tscn")
const FLOOR_FINAL := preload("res://scenes/floors/Floor_Lobby.tscn")  # the lobby, made wrong
const RIDE_SECONDS := 2.6

## After this many descents the elevator opens onto the original lobby —
## empty, lit wrong, and with no way out (SPEC §6). ~10-20 min playthrough.
const END_DEPTH := 7

const FINAL_NOTE_TEXT := "You signed out at 18:42.
You're sure you signed out.

The lobby is the same. The doors are
the same. The light is almost the same.

You'll try the doors again in a moment.
You always do."

var current_floor: Node3D

var _rumble := AudioStreamPlayer.new()

@onready var _world: Node3D = get_node("../SubViewport/World")


func _ready() -> void:
	_rumble.volume_db = -6.0
	_rumble.bus = "Sfx"
	add_child(_rumble)
	current_floor = _world.get_node("Floor_Lobby")
	GameState.current_floor_name = String(current_floor.name)
	GameState.mark_room_seen(String(current_floor.name))
	_apply_floor_meta(current_floor)
	_hook(current_floor)


## Surface tag drives footstep timbre; space tag drives reverb character.
func _apply_floor_meta(floor_node: Node3D) -> void:
	GameState.current_surface = floor_node.get_meta("surface", "tile")
	AudioDirector.set_space(floor_node.get_meta("space", "room"))


func _hook(floor_node: Node3D) -> void:
	var elevator: Elevator = floor_node.get_node_or_null("Props/Elevator")
	if elevator != null:
		elevator.descent_requested.connect(_on_descent_requested)


func _scene_for_depth(depth: int) -> PackedScene:
	# At (and past) the end depth the descent opens back onto the lobby.
	return FLOOR_FINAL if depth >= END_DEPTH else FLOOR_SUBLEVEL


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
	_apply_floor_meta(next)
	if GameState.descent_depth >= END_DEPTH:
		_apply_ending(next)
	else:
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


## Turn the lobby instance into the wrong, no-exit final lobby (SPEC §6):
## lit wrong (dim/sickly, half the tubes dead), the exit unlocked but
## looping back to the elevator, and the desk note replaced with the last.
func _apply_ending(lobby: Node3D) -> void:
	Telemetry.event("reached_final_lobby", {"depth": GameState.descent_depth})
	# Lit wrong: dimmed and sickly green, one tube dead, one flickering —
	# unsettling but still navigable under the heavy end-depth VHS.
	var lights: Array = lobby.get_node("Lights").get_children()
	for i in lights.size():
		var light: Light3D = lights[i]
		if not (light is Light3D):
			continue
		light.light_color = Color(0.72, 0.92, 0.74)
		light.light_energy *= 0.85
		if i == 1 and not light.has_node("Flicker"):
			var fx := FlickerFx.new()
			fx.name = "Flicker"
			fx.intensity = 1.3
			light.add_child(fx)
		elif i == lights.size() - 1:
			light.light_energy *= 0.25  # one corner nearly dark
	for door_name in ["ExitDoorL", "ExitDoorR"]:
		var panel: Door = lobby.get_node_or_null("Props/%s/Hinge/Panel" % door_name)
		if panel != null:
			panel.locked = false
	var note: Note = lobby.get_node_or_null("Props/DeskNote")
	if note != null:
		note.text = FINAL_NOTE_TEXT
		note.is_final = true
	# The front doors lead back to the elevator, facing into the lobby.
	GameState.false_exit_target = Transform3D(Basis(Vector3.UP, PI), Vector3(4, 0.1, -11.0))
	var trigger := Area3D.new()
	trigger.name = "FalseExit"
	trigger.set_script(load("res://scripts/false_exit.gd"))
	var shape := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(2.0, 2.5, 1.4)
	shape.shape = box
	trigger.add_child(shape)
	trigger.position = Vector3(0, 1.0, 3.9)  # the front doorway
	lobby.add_child(trigger)


func dev_swap_to_sublevel() -> void:
	var next: Node3D = FLOOR_SUBLEVEL.instantiate()
	_retire(current_floor)
	current_floor = next
	_world.add_child(next)
	GameState.current_floor_name = String(next.name)
	GameState.mark_room_seen(String(next.name))
	_apply_floor_meta(next)
	LoopController.apply(next, maxi(GameState.descent_depth, 1))
	_hook(next)
	GameState.player.global_position = \
			next.get_node("SpawnPoint").global_position


## Dev: jump straight to the wrong final lobby (no ride).
func dev_jump_to_ending() -> void:
	GameState.descent_depth = END_DEPTH
	var next: Node3D = FLOOR_FINAL.instantiate()
	_retire(current_floor)
	current_floor = next
	_world.add_child(next)
	GameState.current_floor_name = String(next.name)
	GameState.mark_room_seen(String(next.name))
	_apply_floor_meta(next)
	_apply_ending(next)
	_hook(next)
	# Stand in the lobby looking north (toward the desk/corridor), clear of
	# the front-doorway trigger near the south wall.
	GameState.player.global_transform = Transform3D(Basis.IDENTITY, Vector3(0, 0.1, 2.0))
	GameState.depth_changed.emit(END_DEPTH)
