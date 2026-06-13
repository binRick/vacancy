class_name Anomaly
extends RefCounted
## Base for one unit of wrongness (SPEC §6). Subclasses set id/min_depth/
## weight in _init and implement apply(), returning false when no valid
## target exists on this floor (the controller then picks something else).
## Each anomaly should be small and individually deniable.

var id := "anomaly"
var min_depth := 1
var weight := 1.0


func apply(_floor_node: Node3D, _depth: int, _rng: RandomNumberGenerator) -> bool:
	return false


static func lights_of(floor_node: Node3D) -> Array:
	var out := []
	var lights := floor_node.get_node_or_null("Lights")
	if lights != null:
		for child in lights.get_children():
			if child is Light3D:
				out.append(child)
	return out


static func doors_of(floor_node: Node3D) -> Array:
	var out := []
	var props := floor_node.get_node_or_null("Props")
	if props != null:
		for child in props.get_children():
			var panel := child.get_node_or_null("Hinge/Panel")
			if panel is Door:
				out.append(panel)
	return out


static func movables_of(floor_node: Node3D) -> Array:
	var out := []
	for node in floor_node.get_tree().get_nodes_in_group("anomaly_movable"):
		if floor_node.is_ancestor_of(node):
			out.append(node)
	return out
