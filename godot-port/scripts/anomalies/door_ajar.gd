extends Anomaly
## A door that should be closed stands open.


func _init() -> void:
	id = "door_ajar"
	min_depth = 1
	weight = 1.5


func apply(floor_node: Node3D, _depth: int, rng: RandomNumberGenerator) -> bool:
	var candidates := []
	for door in doors_of(floor_node):
		if not door.locked and not door.is_open():
			candidates.append(door)
	if candidates.is_empty():
		return false
	var door: Door = candidates[rng.randi() % candidates.size()]
	door.set_open_instant(true)
	return true
