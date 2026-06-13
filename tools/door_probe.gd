extends Node3D
## Headless diagnostic for the "door opens but you can't walk through" report.
## Instances Door.tscn, opens it, and probes the physics space to see whether
## the panel's COLLIDER actually swings out of the doorway (vs. only the mesh).
## Run: godot --headless --path . tools/door_probe.tscn

var _panel: Node          # AnimatableBody3D with door.gd
var _frame := 0
var _phase := 0
var _log: Array[String] = []


func _ready() -> void:
	var door: Node3D = load("res://scenes/props/Door.tscn").instantiate()
	add_child(door)
	_panel = door.get_node("Hinge/Panel")
	_log.append("default sync_to_physics = %s" % str(_panel.sync_to_physics))


# Small box right inside where the CLOSED leaf sits (well clear of the hinge at
# x=-0.45). Closed -> inside the panel; open -> should be empty if collider moved.
func _leaf_spot_hits() -> int:
	return _shape_hits(_box(Vector3(0.3, 0.3, 0.3) * 0.2), Vector3(0.3, 1.0, 0.0))


# Player capsule planted on the threshold: can the player body occupy the doorway?
func _capsule_hits() -> int:
	var cap := CapsuleShape3D.new()
	cap.radius = 0.32
	cap.height = 1.8
	return _shape_hits(cap, Vector3(0.0, 0.9, 0.0))


func _box(size: Vector3) -> BoxShape3D:
	var b := BoxShape3D.new()
	b.size = size
	return b


func _shape_hits(shape: Shape3D, pos: Vector3) -> int:
	var p := PhysicsShapeQueryParameters3D.new()
	p.shape = shape
	p.transform = Transform3D(Basis(), pos)
	p.collision_mask = 0xFFFFFFFF
	p.collide_with_bodies = true
	return get_world_3d().direct_space_state.intersect_shape(p, 16).size()


func _physics_process(_dt: float) -> void:
	_frame += 1
	match _phase:
		0:
			if _frame >= 3:
				_log.append("CLOSED (sync=%s): leaf_spot=%d  capsule=%d" % [
						str(_panel.sync_to_physics), _leaf_spot_hits(), _capsule_hits()])
				_panel.set_open_instant(true)
				_advance()
		1:
			if _frame >= 5:
				_log.append("OPEN   (sync=%s): leaf_spot=%d  capsule=%d  <-- want 0/0" % [
						str(_panel.sync_to_physics), _leaf_spot_hits(), _capsule_hits()])
				_panel.set_open_instant(false)
				_panel.sync_to_physics = false
				_advance()
		2:
			if _frame >= 5:
				_panel.set_open_instant(true)
				_advance()
		3:
			if _frame >= 5:
				_log.append("OPEN   (sync=false): leaf_spot=%d  capsule=%d  <-- want 0/0" % [
						_leaf_spot_hits(), _capsule_hits()])
				_advance()
		4:
			for line in _log:
				print("[PROBE] ", line)
			get_tree().quit()


func _advance() -> void:
	_phase += 1
	_frame = 0
