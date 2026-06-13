extends Anomaly
## A light starts stuttering. Flickers harder the deeper you are.


func _init() -> void:
	id = "light_flicker"
	min_depth = 1
	weight = 2.0


func apply(floor_node: Node3D, depth: int, rng: RandomNumberGenerator) -> bool:
	var candidates := []
	for light in lights_of(floor_node):
		if light.light_energy > 0.0 and not light.has_node("Flicker"):
			candidates.append(light)
	if candidates.is_empty():
		return false
	var light: Light3D = candidates[rng.randi() % candidates.size()]
	var fx := FlickerFx.new()
	fx.name = "Flicker"
	fx.intensity = 1.0 + depth * 0.25
	light.add_child(fx)
	return true
