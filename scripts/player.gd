extends CharacterBody3D
## First-person walker controller (SPEC §4): WASD + mouse look, Shift to
## walk-slow, Ctrl to crouch, E to interact. No jump. Headbob and footsteps
## are driven by distance traveled so they stay in sync at any speed.

signal interact_target_changed(target: Node)

const WALK_SPEED := 3.0
const SLOW_SPEED := 1.6
const CROUCH_SPEED := 1.4
const ACCEL := 12.0
const MOUSE_SENS := 0.0022
const PITCH_LIMIT_DEG := 85.0

const STAND_EYE_HEIGHT := 1.6
const CROUCH_EYE_HEIGHT := 0.95
const STAND_BODY_HEIGHT := 1.8
const CROUCH_BODY_HEIGHT := 1.2
const CROUCH_LERP_SPEED := 9.0

const STEP_DISTANCE := 1.9  # meters between footsteps; bob does 1 cycle per step
const BOB_AMP := 0.035

var _pitch := 0.0
var _crouching := false
var _bob_phase := 0.0  # in cycles
var _step_accum := 0.0
var _last_pos := Vector3.ZERO
var _current_target: Node = null
var _current_prompt := ""
var _mouse_captured_at_msec := -10_000

@onready var head: Node3D = $Head
@onready var camera: Camera3D = $Head/Camera3D
@onready var interact_ray: RayCast3D = $Head/Camera3D/InteractRay
@onready var footsteps: AudioStreamPlayer = $Footsteps
@onready var collider: CollisionShape3D = $CollisionShape3D


func _ready() -> void:
	GameState.player = self
	footsteps.stream = SoundSynth.footstep()
	interact_ray.add_exception(self)
	_last_pos = global_position
	_capture_mouse()


## Capturing warps the OS cursor, which arrives as bogus large motion events
## (possibly spread over several frames on macOS); ignore motion briefly so
## the view doesn't jerk on capture.
func _capture_mouse() -> void:
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	_mouse_captured_at_msec = Time.get_ticks_msec()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		if Time.get_ticks_msec() - _mouse_captured_at_msec < 50:
			return
		rotate_y(-event.relative.x * MOUSE_SENS)
		_pitch = clampf(_pitch - event.relative.y * MOUSE_SENS,
				-deg_to_rad(PITCH_LIMIT_DEG), deg_to_rad(PITCH_LIMIT_DEG))
		head.rotation.x = _pitch
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event is InputEventMouseButton and event.pressed \
			and Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		_capture_mouse()
	elif event.is_action_pressed("interact") and _current_target != null:
		_current_target.interact(self)


func _physics_process(delta: float) -> void:
	_update_crouch(delta)
	_update_movement(delta)
	_update_bob_and_footsteps(delta)
	_update_interact_target()


func _update_movement(delta: float) -> void:
	var input_dir := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var wish := transform.basis * Vector3(input_dir.x, 0.0, input_dir.y)
	var speed := WALK_SPEED
	if _crouching:
		speed = CROUCH_SPEED
	elif Input.is_action_pressed("walk_slow"):
		speed = SLOW_SPEED
	var blend := 1.0 - exp(-ACCEL * delta)
	var horiz := Vector3(velocity.x, 0.0, velocity.z).lerp(wish * speed, blend)
	velocity.x = horiz.x
	velocity.z = horiz.z
	if not is_on_floor():
		velocity += get_gravity() * delta
	move_and_slide()


func _update_crouch(delta: float) -> void:
	if Input.is_action_pressed("crouch"):
		_crouching = true
	elif _crouching and _can_stand():
		_crouching = false
	var eye_target := CROUCH_EYE_HEIGHT if _crouching else STAND_EYE_HEIGHT
	var body_target := CROUCH_BODY_HEIGHT if _crouching else STAND_BODY_HEIGHT
	var blend := 1.0 - exp(-CROUCH_LERP_SPEED * delta)
	head.position.y = lerpf(head.position.y, eye_target, blend)
	var capsule: CapsuleShape3D = collider.shape
	capsule.height = lerpf(capsule.height, body_target, blend)
	collider.position.y = capsule.height * 0.5


func _can_stand() -> bool:
	var query := PhysicsRayQueryParameters3D.create(
			global_position + Vector3.UP * (CROUCH_BODY_HEIGHT - 0.1),
			global_position + Vector3.UP * (STAND_BODY_HEIGHT + 0.05))
	query.exclude = [get_rid()]
	return get_world_3d().direct_space_state.intersect_ray(query).is_empty()


func _update_bob_and_footsteps(delta: float) -> void:
	var moved := global_position - _last_pos
	_last_pos = global_position
	moved.y = 0.0
	var dist := moved.length()
	var walking := is_on_floor() and dist > 0.0001
	if walking:
		_bob_phase = fmod(_bob_phase + dist / STEP_DISTANCE, 1.0)
		_step_accum += dist
		if _step_accum >= STEP_DISTANCE:
			_step_accum -= STEP_DISTANCE
			_play_footstep()
	var bob_y := 0.0
	if walking and not _crouching:
		bob_y = sin(_bob_phase * TAU) * BOB_AMP
	camera.position.y = lerpf(camera.position.y, bob_y, 1.0 - exp(-10.0 * delta))


func _play_footstep() -> void:
	footsteps.pitch_scale = randf_range(0.95, 1.05)
	footsteps.volume_db = randf_range(-14.0, -11.0)
	footsteps.play()


func _update_interact_target() -> void:
	var target: Node = null
	if interact_ray.is_colliding():
		var hit: Object = interact_ray.get_collider()
		if hit is Node and hit.has_method("interact"):
			target = hit
	var prompt := ""
	if target != null:
		prompt = target.prompt_text if "prompt_text" in target else "Interact"
	# Prompt text can change in place (a door flips Open <-> Close).
	if target != _current_target or prompt != _current_prompt:
		_current_target = target
		_current_prompt = prompt
		interact_target_changed.emit(target)


## Merged into the player block of the Telemetry heartbeat by GameState.snapshot().
func telemetry_state() -> Dictionary:
	return {
		"crouch": _crouching,
		"speed": snappedf(Vector3(velocity.x, 0.0, velocity.z).length(), 0.01),
		"target": String(_current_target.name) if _current_target != null else "",
	}
