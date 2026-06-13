class_name FlickerFx
extends Node
## Makes the parent light stutter like a dying fluorescent tube: long
## stable stretches, then a short burst of random dips. `intensity`
## shortens the calm between bursts.

var intensity := 1.0

var _light: Light3D
var _base_energy := 0.0
var _clock := 0.0
var _burst_end := 0.0
var _next_burst := 0.0


func _ready() -> void:
	_light = get_parent() as Light3D
	_base_energy = _light.light_energy
	_next_burst = randf_range(0.5, 2.0)


func _process(delta: float) -> void:
	_clock += delta
	if _clock >= _next_burst:
		_burst_end = _clock + randf_range(0.15, 0.7)
		_next_burst = _clock + randf_range(1.5, 6.0) / maxf(intensity, 0.1)
	if _clock < _burst_end:
		_light.light_energy = _base_energy * (0.1 + 0.9 * randf() * randf())
	else:
		_light.light_energy = _base_energy
