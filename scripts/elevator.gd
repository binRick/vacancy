class_name Elevator
extends Node3D
## The descent mechanism (SPEC §5): the call button opens the doors, the
## inside button rides down one level (descent_depth += 1). What the doors
## reopen onto is the floor manager's job (step 7); until then the same
## floor repeats — fittingly.

signal ride_finished(new_depth: int)

const DOOR_SLIDE := 0.7
const DOOR_SECONDS := 1.1

@export var ride_seconds := 2.6

var _doors_open := false
var _busy := false

@onready var _door_l: AnimatableBody3D = $DoorL
@onready var _door_r: AnimatableBody3D = $DoorR
@onready var _audio: AudioStreamPlayer3D = $Audio
@onready var _closed_l_x: float = _door_l.position.x
@onready var _closed_r_x: float = _door_r.position.x


func open_doors() -> void:
	if _busy or _doors_open:
		return
	_busy = true
	Telemetry.event("elevator_doors", {"open": true})
	await _slide_doors(true)
	_busy = false


func request_descent() -> void:
	if _busy or not _doors_open:
		return
	_busy = true
	Telemetry.event("elevator_ride", {"from_depth": GameState.descent_depth})
	await _slide_doors(false)
	_audio.stream = SoundSynth.elevator_rumble(ride_seconds)
	_audio.play()
	await get_tree().create_timer(ride_seconds, false).timeout
	GameState.descend()
	_audio.stream = SoundSynth.elevator_ding()
	_audio.play()
	await _slide_doors(true)
	_busy = false
	ride_finished.emit(GameState.descent_depth)


func _slide_doors(open: bool) -> void:
	_doors_open = open
	var offset := DOOR_SLIDE if open else 0.0
	var tw := create_tween().set_parallel(true)
	tw.tween_property(_door_l, "position:x", _closed_l_x - offset, DOOR_SECONDS) \
			.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tw.tween_property(_door_r, "position:x", _closed_r_x + offset, DOOR_SECONDS) \
			.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	await tw.finished
