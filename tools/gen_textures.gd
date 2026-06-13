extends SceneTree
## One-off generator for placeholder greybox textures (SPEC: no external
## assets — everything procedural). Run from the project root:
##   godot --headless -s tools/gen_textures.gd

const SIZE := 64


func _init() -> void:
	DirAccess.make_dir_recursive_absolute("res://assets")
	_make_checker()
	_make_panel()
	print("textures written to res://assets/")
	quit()


## Floor: classic checker, two close greys with a touch of per-pixel grime.
func _make_checker() -> void:
	var img := Image.create(SIZE, SIZE, false, Image.FORMAT_RGB8)
	var rng := RandomNumberGenerator.new()
	rng.seed = 0x5EED
	var dark := Color(0.42, 0.42, 0.45)
	var light := Color(0.55, 0.55, 0.58)
	for y in SIZE:
		for x in SIZE:
			var even := ((x >> 3) + (y >> 3)) % 2 == 0  # 8px cells
			var c := dark if even else light
			img.set_pixel(x, y, c.darkened(rng.randf_range(0.0, 0.06)))
	img.save_png("res://assets/checker.png")


## Walls: flat fill with darker seam lines, office-panel-ish.
func _make_panel() -> void:
	var img := Image.create(SIZE, SIZE, false, Image.FORMAT_RGB8)
	var rng := RandomNumberGenerator.new()
	rng.seed = 0xCAFE
	var fill := Color(0.52, 0.52, 0.55)
	var seam := Color(0.33, 0.33, 0.37)
	for y in SIZE:
		for x in SIZE:
			var on_seam := x % 32 == 0 or y % 16 == 0
			var c := seam if on_seam else fill
			img.set_pixel(x, y, c.darkened(rng.randf_range(0.0, 0.05)))
	img.save_png("res://assets/panel.png")
