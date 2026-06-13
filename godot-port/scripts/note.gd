class_name Note
extends Interactable
## Readable text fragment (SPEC §5). Sparse and ambiguous backstory; the
## overlay rendering lives in main.gd (native res so it stays readable).

@export_multiline var text := ""
@export var is_final := false  ## the last note: closing it ends the game


func interact(_player: Node3D) -> void:
	Telemetry.event("note", {"note": String(name), "final": is_final})
	GameState.open_note(text, is_final)
