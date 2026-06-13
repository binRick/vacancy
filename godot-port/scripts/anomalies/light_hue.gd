extends Anomaly
## A light has gone slightly green, like it's about to die — or already has.


func _init() -> void:
	id = "light_hue"
	min_depth = 3
	weight = 1.0


func apply(floor_node: Node3D, _depth: int, rng: RandomNumberGenerator) -> bool:
	var candidates := []
	for light in lights_of(floor_node):
		if light.light_energy > 0.0:
			candidates.append(light)
	if candidates.is_empty():
		return false
	var light: Light3D = candidates[rng.randi() % candidates.size()]
	light.light_color = light.light_color.lerp(Color(0.65, 1.0, 0.68), 0.45)
	return true
