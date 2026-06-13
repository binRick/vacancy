extends Node
## Main bootstrap: routes the 320x240 SubViewport into the full-window
## TextureRect (nearest-neighbor, aspect-kept) and hosts the native-res UI
## (interact prompt, captions, note overlay).

@onready var sub_viewport: SubViewport = $SubViewport
@onready var dither_viewport: SubViewport = $DitherViewport
@onready var dither_rect: TextureRect = $DitherViewport/DitherRect
@onready var screen: TextureRect = $Screen
@onready var prompt_label: Label = $UI/InteractPrompt
@onready var caption_label: Label = $UI/Caption
@onready var note_overlay: Control = $UI/NoteOverlay
@onready var note_label: Label = $UI/NoteOverlay/Text
@onready var player: CharacterBody3D = $SubViewport/World/Player

var _caption_seq := 0


func _ready() -> void:
	# Main stays live while the tree pauses (note overlay needs input);
	# the SubViewport subtree is explicitly PAUSABLE in the scene.
	process_mode = Node.PROCESS_MODE_ALWAYS
	# Post chain (SPEC §7): 3D world (320x240) -> dither pass at internal
	# res -> VHS/CRT shader on the upscaled image. UI draws crisp above.
	dither_rect.texture = sub_viewport.get_texture()
	screen.texture = dither_viewport.get_texture()
	player.interact_target_changed.connect(_on_interact_target_changed)
	GameState.caption_shown.connect(_on_caption_shown)
	GameState.note_opened.connect(_on_note_opened)
	GameState.depth_changed.connect(_on_depth_changed)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--screenshot="):
			_capture_and_quit(arg.trim_prefix("--screenshot="))
		elif arg.begins_with("--pose="):  # dev: x,z,yaw_deg — place player for screenshots
			var p := arg.trim_prefix("--pose=").split(",")
			if p.size() == 3:
				player.global_position = Vector3(p[0].to_float(), 0.1, p[1].to_float())
				player.rotation_degrees.y = p[2].to_float()
		elif arg.begins_with("--depth="):  # dev: pretend we already descended N times
			GameState.descent_depth = arg.trim_prefix("--depth=").to_int()
			GameState.depth_changed.emit(GameState.descent_depth)
		elif arg == "--selftest":
			_run_selftest()


## The 3D world lives inside a SubViewport, which gets no OS input on its own;
## forward everything so the player controller can see it.
func _unhandled_input(event: InputEvent) -> void:
	if note_overlay.visible:
		if event.is_action_pressed("interact") or event.is_action_pressed("ui_cancel"):
			_close_note()
		return
	sub_viewport.push_input(event)


func _on_interact_target_changed(target: Node) -> void:
	if target == null:
		prompt_label.hide()
		return
	var prompt: String = target.prompt_text if "prompt_text" in target else "Interact"
	prompt_label.text = "[E] %s" % prompt
	prompt_label.show()


## The VHS degrades as the building does (SPEC §7).
func _on_depth_changed(depth: int) -> void:
	var mat: ShaderMaterial = screen.material
	mat.set_shader_parameter("intensity", clampf(0.18 + 0.08 * depth, 0.0, 1.0))


func _on_caption_shown(text: String) -> void:
	caption_label.text = text
	caption_label.show()
	_caption_seq += 1
	var seq := _caption_seq
	await get_tree().create_timer(2.4).timeout
	if seq == _caption_seq:
		caption_label.hide()


func _on_note_opened(text: String) -> void:
	note_label.text = text
	note_overlay.show()
	get_tree().paused = true


func _close_note() -> void:
	note_overlay.hide()
	get_tree().paused = false
	Telemetry.event("note_closed")


## Dev helper: dump the final post-processed window to <path> and the raw
## internal 320x240 render to <path>_raw.png, then exit.
func _capture_and_quit(path: String) -> void:
	await get_tree().create_timer(1.0).timeout
	var err := get_viewport().get_texture().get_image().save_png(path)
	if err != OK:
		push_error("screenshot failed: %s" % error_string(err))
	sub_viewport.get_texture().get_image().save_png(path.replace(".png", "_raw.png"))
	get_tree().quit()


## Dev helper: exercise doors, the note, and the elevator without input;
## verify via the telemetry log. Run: godot --path . -- --selftest
func _run_selftest() -> void:
	var lobby := $SubViewport/World/Floor_Lobby
	await get_tree().create_timer(0.5).timeout
	lobby.get_node("Props/StorageDoor/Hinge/Panel").interact(player)
	lobby.get_node("Props/ExitDoorL/Hinge/Panel").interact(player)
	lobby.get_node("Props/DeskNote").interact(player)
	await get_tree().create_timer(0.6).timeout
	_close_note()
	var elevator := lobby.get_node("Props/Elevator")
	elevator.open_doors()
	await get_tree().create_timer(1.6).timeout
	elevator.get_node("InsideButton").interact(player)
	await get_tree().create_timer(7.0).timeout
	get_tree().quit()
