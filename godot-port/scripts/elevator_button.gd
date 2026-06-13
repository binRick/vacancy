class_name ElevatorButton
extends Interactable
## A button wired to the parent Elevator: CALL opens the doors from the
## landing, DESCEND starts the ride from inside the cab.

enum Mode { CALL, DESCEND }

@export var mode := Mode.CALL

@onready var _elevator: Elevator = get_parent() as Elevator


func interact(_player: Node3D) -> void:
	match mode:
		Mode.CALL:
			_elevator.open_doors()
		Mode.DESCEND:
			_elevator.request_descent()
