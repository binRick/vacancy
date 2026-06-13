class_name Door
extends Interactable
## Hinged door (SPEC §5). Attached to the Panel body; swings the parent
## Hinge node. Locked doors stay locked unless required_flag is set in
## GameState. is_loop_door is consumed by the loop system in step 7.

@export var locked := false
@export var locked_text := "Locked."
@export var required_flag := ""  ## GameState flag that overrides `locked`
@export var is_loop_door := false  ## step 7: this door re-routes somewhere wrong
@export var open_angle_deg := 105.0
@export var swing_seconds := 1.4

var _open := false
var _busy := false

@onready var _hinge: Node3D = get_parent()
@onready var _audio: AudioStreamPlayer3D = $Audio


func _ready() -> void:
	_audio.bus = "Sfx"


func _door_name() -> String:
	return String(_hinge.get_parent().name)


func is_open() -> bool:
	return _open


## Used by anomalies: put the door in a state silently, no tween.
func set_open_instant(open: bool) -> void:
	_open = open
	prompt_text = "Close" if open else "Open"
	_hinge.rotation.y = deg_to_rad(open_angle_deg) if open else 0.0


func interact(_player: Node3D) -> void:
	if _busy:
		return
	if locked and not (required_flag != "" and GameState.get_flag(required_flag)):
		_audio.stream = SoundSynth.locked_rattle()
		_audio.play()
		GameState.show_caption(locked_text)
		Telemetry.event("door_locked", {"door": _door_name()})
		return
	_busy = true
	_open = not _open
	prompt_text = "Close" if _open else "Open"
	_audio.stream = SoundSynth.door_creak()
	_audio.pitch_scale = randf_range(0.92, 1.08)
	_audio.play()
	Telemetry.event("door", {"door": _door_name(), "open": _open})
	var target := deg_to_rad(open_angle_deg) if _open else 0.0
	var tw := create_tween()
	tw.tween_property(_hinge, "rotation:y", target, swing_seconds) \
			.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	await tw.finished
	_busy = false
