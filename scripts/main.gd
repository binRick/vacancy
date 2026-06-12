extends Node
## Main bootstrap: routes the 320x240 SubViewport into the full-window
## TextureRect (nearest-neighbor, aspect-kept) and hosts the native-res UI.

@onready var sub_viewport: SubViewport = $SubViewport
@onready var screen: TextureRect = $Screen
@onready var prompt_label: Label = $UI/InteractPrompt
@onready var player: CharacterBody3D = $SubViewport/World/Player


func _ready() -> void:
	screen.texture = sub_viewport.get_texture()
	player.interact_target_changed.connect(_on_interact_target_changed)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--screenshot="):
			_capture_and_quit(arg.trim_prefix("--screenshot="))


## The 3D world lives inside a SubViewport, which gets no OS input on its own;
## forward everything so the player controller can see it.
func _unhandled_input(event: InputEvent) -> void:
	sub_viewport.push_input(event)


func _on_interact_target_changed(target: Node) -> void:
	if target == null:
		prompt_label.hide()
		return
	var prompt: String = target.prompt_text if "prompt_text" in target else "Interact"
	prompt_label.text = "[E] %s" % prompt
	prompt_label.show()


## Dev helper: dump one frame of the internal 320x240 render to a PNG and exit.
func _capture_and_quit(path: String) -> void:
	await get_tree().create_timer(1.0).timeout
	var err := sub_viewport.get_texture().get_image().save_png(path)
	if err != OK:
		push_error("screenshot failed: %s" % error_string(err))
	get_tree().quit()
