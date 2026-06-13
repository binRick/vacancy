extends Anomaly
## A light is simply dead. Nobody fixed it.


func _init() -> void:
	id = "light_out"
	min_depth = 2
	weight = 1.5


func apply(floor_node: Node3D, _depth: int, rng: RandomNumberGenerator) -> bool:
	var candidates := []
	for light in lights_of(floor_node):
		if light.light_energy > 0.0 and not light.has_node("Flicker"):
			candidates.append(light)
	if candidates.is_empty():
		return false
	var light: Light3D = candidates[rng.randi() % candidates.size()]
	light.light_energy = 0.0
	return true
