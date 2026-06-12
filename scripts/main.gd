extends Node
## Main bootstrap: routes the 320x240 SubViewport into the full-window
## TextureRect (nearest-neighbor, aspect-kept) and spins the greybox cube
## so the low-res pipeline is visibly working.

const CUBE_SPIN_SPEED := 0.6  # rad/s

@onready var sub_viewport: SubViewport = $SubViewport
@onready var screen: TextureRect = $Screen
@onready var cube: MeshInstance3D = $SubViewport/World/Cube


func _ready() -> void:
	screen.texture = sub_viewport.get_texture()
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--screenshot="):
			_capture_and_quit(arg.trim_prefix("--screenshot="))


## Dev helper: dump one frame of the internal 320x240 render to a PNG and exit.
func _capture_and_quit(path: String) -> void:
	await get_tree().create_timer(1.0).timeout
	var err := sub_viewport.get_texture().get_image().save_png(path)
	if err != OK:
		push_error("screenshot failed: %s" % error_string(err))
	get_tree().quit()


func _process(delta: float) -> void:
	cube.rotate_y(CUBE_SPIN_SPEED * delta)
