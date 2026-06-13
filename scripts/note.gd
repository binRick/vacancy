class_name Note
extends Interactable
## Readable text fragment (SPEC §5). Sparse and ambiguous backstory; the
## overlay rendering lives in main.gd (native res so it stays readable).

@export_multiline var text := ""


func interact(_player: Node3D) -> void:
	Telemetry.event("note", {"note": String(name)})
	GameState.open_note(text)
