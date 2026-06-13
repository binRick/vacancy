extends Anomaly
## Something has been moved. Not far. Just enough.


func _init() -> void:
	id = "prop_shift"
	min_depth = 1
	weight = 1.2


func apply(floor_node: Node3D, _depth: int, rng: RandomNumberGenerator) -> bool:
	var props := movables_of(floor_node)
	if props.is_empty():
		return false
	var prop: Node3D = props[rng.randi() % props.size()]
	var ang := rng.randf_range(0.0, TAU)
	prop.position += Vector3(cos(ang), 0.0, sin(ang)) * rng.randf_range(0.3, 0.8)
	var twist := deg_to_rad(rng.randf_range(15.0, 60.0))
	prop.rotation.y += twist if rng.randf() < 0.5 else -twist
	return true
