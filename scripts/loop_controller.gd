class_name LoopController
## The "rooms recur wrong" logic (SPEC §6): picks depth-weighted anomalies
## from a data-driven registry and applies them to a freshly loaded floor.
## Authoring a new anomaly = one script in scripts/anomalies/ extending
## Anomaly, plus a line here. Core logic never changes.

const ANOMALY_SCRIPTS: Array = [
	preload("res://scripts/anomalies/light_flicker.gd"),
	preload("res://scripts/anomalies/light_out.gd"),
	preload("res://scripts/anomalies/door_ajar.gd"),
	preload("res://scripts/anomalies/prop_shift.gd"),
	preload("res://scripts/anomalies/light_hue.gd"),
]


static func apply(floor_node: Node3D, depth: int) -> void:
	if depth <= 0:
		return
	var rng := RandomNumberGenerator.new()
	# Deterministic per depth AND visit count: a room recurring deliberately
	# differs from its first appearance (seen_rooms is the memory).
	var visits: int = GameState.seen_rooms.get(String(floor_node.name), 0)
	rng.seed = hash("vacancy:%d:%d:%s" % [depth, visits, floor_node.name])
	# ~1-2 anomalies early, up to 4 deep down.
	var budget := clampi((depth + 1) / 2, 1, 4)
	var pool: Array = []
	for script in ANOMALY_SCRIPTS:
		var anomaly: Anomaly = script.new()
		if depth >= anomaly.min_depth:
			pool.append(anomaly)
	var applied: Array = []
	while budget > 0 and not pool.is_empty():
		var total := 0.0
		for a: Anomaly in pool:
			total += a.weight
		var roll := rng.randf() * total
		var pick: Anomaly = pool.back()
		for a: Anomaly in pool:
			roll -= a.weight
			if roll <= 0.0:
				pick = a
				break
		pool.erase(pick)
		if pick.apply(floor_node, depth, rng):
			applied.append(pick.id)
			budget -= 1
	Telemetry.event("anomalies", {"depth": depth, "applied": applied})
