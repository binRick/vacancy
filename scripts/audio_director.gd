extends Node
## Autoload: AudioDirector — the atmosphere bed (SPEC §8). Looping
## fluorescent hum + HVAC drone on a dedicated bus, an Sfx bus carrying
## reverb whose character follows the space and deepens with descent, and
## sparse stingers tied to anomaly events. Silence is used as a tool.
##
## Bus assignment is done in code (here + each emitter's _ready) so the
## buses are guaranteed to exist first and real assets drop in cleanly.

const HUM_BUS := "Ambience"
const SFX_BUS := "Sfx"

var _hum: AudioStreamPlayer
var _hvac: AudioStreamPlayer
var _sfx_reverb: AudioEffectReverb
var _space_room := 0.42   # reverb room_size for the current space type
var _silencing := false

@onready var _sfx_idx: int = AudioServer.get_bus_index(SFX_BUS)


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS  # the bed never pauses
	_build_buses()
	_start_bed()
	GameState.depth_changed.connect(_on_depth_changed)
	GameState.anomaly_applied.connect(_on_anomaly_applied)
	Telemetry.event("audio_ready", {"buses": AudioServer.bus_count})


func _build_buses() -> void:
	_ensure_bus(HUM_BUS, "Master")
	var sfx := _ensure_bus(SFX_BUS, "Master")
	_sfx_idx = sfx
	if AudioServer.get_bus_effect_count(sfx) == 0:
		_sfx_reverb = AudioEffectReverb.new()
		_sfx_reverb.dry = 0.92
		_sfx_reverb.wet = 0.18
		_sfx_reverb.damping = 0.6
		_sfx_reverb.spread = 1.0
		_sfx_reverb.predelay_msec = 22.0
		AudioServer.add_bus_effect(sfx, _sfx_reverb)
	else:
		_sfx_reverb = AudioServer.get_bus_effect(sfx, 0) as AudioEffectReverb
	_apply_reverb(0)


func _ensure_bus(bus_name: String, send: String) -> int:
	var idx := AudioServer.get_bus_index(bus_name)
	if idx != -1:
		return idx
	idx = AudioServer.bus_count
	AudioServer.add_bus(idx)
	AudioServer.set_bus_name(idx, bus_name)
	AudioServer.set_bus_send(idx, send)
	return idx


func _start_bed() -> void:
	_hum = _make_bed_player(SoundSynth.fluorescent_hum(), -15.0)
	_hvac = _make_bed_player(SoundSynth.hvac_drone(), -19.0)


func _make_bed_player(stream: AudioStream, db: float) -> AudioStreamPlayer:
	var p := AudioStreamPlayer.new()
	p.stream = stream
	p.bus = HUM_BUS
	p.volume_db = db
	p.autoplay = true
	add_child(p)
	p.play()
	return p


## Space type sets the baseline reverb (a small room vs a long corridor);
## descent layers extra size/wetness on top for a cavernous wrongness.
func set_space(space: String) -> void:
	_space_room = 0.6 if space == "corridor" else 0.42
	_apply_reverb(GameState.descent_depth)


func _apply_reverb(depth: int) -> void:
	if _sfx_reverb == null:
		return
	_sfx_reverb.room_size = clampf(_space_room + 0.04 * depth, 0.0, 0.95)
	_sfx_reverb.wet = clampf(0.18 + 0.025 * depth, 0.0, 0.6)


func _on_depth_changed(depth: int) -> void:
	_apply_reverb(depth)
	if _hvac != null:
		_hvac.volume_db = -19.0 + minf(depth, 6) * 0.7  # the drone presses in
	# Deeper down, the hum sometimes cuts to total silence on arrival.
	if depth >= 3:
		var rng := RandomNumberGenerator.new()
		rng.seed = hash("silence:%d" % depth)
		rng.randf()  # discard: the first draw after seeding is biased high
		var chance := clampf(0.35 + 0.06 * (depth - 3), 0.0, 0.7)
		if rng.randf() < chance:
			_schedule_silence(rng.randf_range(2.0, 5.0), 2.0)


## Total silence for `hold` seconds (SPEC §8 — "a hum that drops out for two
## seconds of total silence"). Ducks Master so even footsteps fall away.
func _schedule_silence(delay: float, hold: float) -> void:
	if _silencing:
		return
	_silencing = true
	await get_tree().create_timer(delay, false).timeout
	Telemetry.event("audio_silence", {"hold": hold})
	var tw := create_tween()
	tw.tween_method(_set_master_db, 0.0, -60.0, 0.12)
	tw.tween_interval(hold)
	tw.tween_method(_set_master_db, -60.0, 0.0, 0.6)
	await tw.finished
	_silencing = false


func _set_master_db(db: float) -> void:
	AudioServer.set_bus_volume_db(0, db)


## The cut to black takes the sound with it.
func end_fade(duration: float) -> void:
	Telemetry.event("audio_end_fade")
	var tw := create_tween()
	tw.tween_method(_set_master_db, AudioServer.get_bus_volume_db(0), -60.0, duration)


func _exit_tree() -> void:
	# Stop the bed so its persistent streams release cleanly on shutdown.
	if is_instance_valid(_hum):
		_hum.stop()
	if is_instance_valid(_hvac):
		_hvac.stop()


func _on_anomaly_applied(id: String) -> void:
	match id:
		"door_ajar":
			_delayed_stinger(SoundSynth.distant_door(), -7.0, randf_range(0.5, 3.5))
		"prop_shift":
			_delayed_stinger(SoundSynth.metal_groan(), -13.0, randf_range(1.0, 4.0))
		"light_out":
			_delayed_stinger(SoundSynth.sub_swell(), -15.0, randf_range(0.0, 1.5))


func _delayed_stinger(stream: AudioStream, db: float, delay: float) -> void:
	await get_tree().create_timer(delay, false).timeout
	var p := AudioStreamPlayer.new()
	p.stream = stream
	p.bus = SFX_BUS
	p.volume_db = db
	add_child(p)
	p.finished.connect(p.queue_free)
	p.play()
	Telemetry.event("stinger", {"db": db})
