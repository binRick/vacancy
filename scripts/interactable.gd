class_name Interactable
extends CollisionObject3D
## Base interface for anything the player can use (SPEC §5). The player's
## camera raycast looks for this and shows "[E] <prompt_text>".

@export var prompt_text := "Interact"


func interact(_player: Node3D) -> void:
	pass
