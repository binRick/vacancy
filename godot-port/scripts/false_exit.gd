extends Area3D
## The exit that isn't (SPEC §6 end beat). Walking out the front doors of
## the final lobby drops the player back at the elevator — there is no exit.
## Added to the lobby at runtime by FloorManager._apply_ending.

var _cooldown := false


func _ready() -> void:
	body_entered.connect(_on_body_entered)


func _on_body_entered(body: Node3D) -> void:
	if _cooldown or body != GameState.player:
		return
	_cooldown = true
	Telemetry.event("false_exit")
	GameState.false_exit_used.emit()
	await get_tree().create_timer(2.5).timeout
	_cooldown = false
