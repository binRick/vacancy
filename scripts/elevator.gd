class_name Elevator
extends Node3D
## Elevator doors + buttons (SPEC §5). The actual descent (floor swap,
## rumble, depth increment) is orchestrated by floor_manager.gd via
## descent_requested — this cab gets freed along with its floor mid-ride.

signal descent_requested(elevator: Elevator)

const DOOR_SLIDE := 0.7
const DOOR_SECONDS := 1.1

var _doors_open := false
var _busy := false

@onready var _door_l: AnimatableBody3D = $DoorL
@onready var _door_r: AnimatableBody3D = $DoorR
@onready var _audio: AudioStreamPlayer3D = $Audio
@onready var _closed_l_x: float = _door_l.position.x
@onready var _closed_r_x: float = _door_r.position.x


func _ready() -> void:
	_audio.bus = "Sfx"


func open_doors() -> void:
	if _busy or _doors_open:
		return
	_busy = true
	Telemetry.event("elevator_doors", {"open": true})
	await _slide_doors(true)
	_busy = false


## Called by the floor manager once a ride starts; no re-entry guard —
## request_descent already holds _busy until this cab is freed.
func close_doors() -> void:
	Telemetry.event("elevator_doors", {"open": false})
	await _slide_doors(false)


func request_descent() -> void:
	if _busy or not _doors_open:
		return
	_busy = true
	Telemetry.event("elevator_ride", {"from_depth": GameState.descent_depth})
	descent_requested.emit(self)


func ding() -> void:
	_audio.stream = SoundSynth.elevator_ding()
	_audio.play()


func _slide_doors(open: bool) -> void:
	_doors_open = open
	var offset := DOOR_SLIDE if open else 0.0
	var tw := create_tween().set_parallel(true)
	tw.tween_property(_door_l, "position:x", _closed_l_x - offset, DOOR_SECONDS) \
			.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tw.tween_property(_door_r, "position:x", _closed_r_x + offset, DOOR_SECONDS) \
			.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	await tw.finished
