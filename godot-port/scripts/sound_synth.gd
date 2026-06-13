class_name SoundSynth
## Procedural placeholder audio (SPEC §8): everything synthesized at load,
## structured so real assets can replace call sites cleanly later.

const RATE := 22050


static func _wav(samples: PackedFloat32Array) -> AudioStreamWAV:
	var data := PackedByteArray()
	data.resize(samples.size() * 2)
	for i in samples.size():
		data.encode_s16(i * 2, int(clampf(samples[i], -1.0, 1.0) * 32767.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = RATE
	wav.data = data
	return wav


## As _wav, but flagged to loop forever (for the ambient beds). Seamlessness
## is the caller's job — the bed builders below align phase across the wrap.
static func _loop_wav(samples: PackedFloat32Array) -> AudioStreamWAV:
	var wav := _wav(samples)
	wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
	wav.loop_begin = 0
	wav.loop_end = samples.size()
	return wav


## Short low-passed noise burst with a sharp decay: a dull footfall.
## Surface tag tweaks the timbre (SPEC §8): carpet duller, tile a bright tick.
static func footstep(surface := "tile") -> AudioStreamWAV:
	var dur := 0.13
	var lp_coef := 0.22
	var amp := 0.5
	var click := 0.0
	var decay := 2.5
	match surface:
		"carpet":
			dur = 0.10; lp_coef = 0.07; amp = 0.34; decay = 2.0
		"tile":
			dur = 0.12; lp_coef = 0.30; amp = 0.5; click = 0.28; decay = 3.5
		"concrete", _:
			dur = 0.13; lp_coef = 0.22; amp = 0.5; decay = 2.6
	var n := int(RATE * dur)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	for i in n:
		var frac := float(i) / n
		var env := pow(1.0 - frac, decay)
		lp += (rng.randf_range(-1.0, 1.0) - lp) * lp_coef
		var v := lp
		if click > 0.0:
			v += rng.randf_range(-1.0, 1.0) * click * pow(1.0 - frac, 8.0)
		s[i] = v * env * amp
	return _wav(s)


## Looping fluorescent ballast buzz: 120Hz fundamental + harmonics. Integer
## frequencies over a 2s buffer => phase realigns exactly at the loop point.
static func fluorescent_hum() -> AudioStreamWAV:
	var n := int(RATE * 2.0)
	var s := PackedFloat32Array()
	s.resize(n)
	for i in n:
		var t := float(i) / RATE
		var v := sin(TAU * 120.0 * t) * 0.5
		v += sin(TAU * 240.0 * t) * 0.17
		v += sin(TAU * 360.0 * t) * 0.08
		v += sin(TAU * 60.0 * t) * 0.05
		var trem := 0.86 + 0.14 * sin(TAU * 6.0 * t)  # integer Hz => seamless
		s[i] = v * trem * 0.16
	return _loop_wav(s)


## Looping HVAC drone: heavily low-passed noise made seamless by an
## equal-power half-buffer crossfade (kills the wrap discontinuity), under
## a bed of low beating sines.
static func hvac_drone() -> AudioStreamWAV:
	var n := int(RATE * 3.0)
	var rng := RandomNumberGenerator.new()
	rng.seed = 0x4711
	var noise := PackedFloat32Array()
	noise.resize(n)
	var lp := 0.0
	for i in n:
		lp += (rng.randf_range(-1.0, 1.0) - lp) * 0.02  # deep rumble
		noise[i] = lp
	var s := PackedFloat32Array()
	s.resize(n)
	var half := n / 2
	for i in n:
		var w_a := sin(PI * i / n)        # 0 at the wrap (i=0,n)
		var w_b := absf(cos(PI * i / n))  # 0 at i=n/2 (where copy B wraps)
		var rumble := (noise[i] * w_a + noise[(i + half) % n] * w_b) * 3.0
		var t := float(i) / RATE
		var sub := sin(TAU * 42.0 * t) * 0.10 + sin(TAU * 55.0 * t) * 0.07
		s[i] = clampf(rumble * 0.5 + sub, -1.0, 1.0) * 0.5
	return _loop_wav(s)


## Stinger: a muffled thud, like a door closing somewhere else in the building.
static func distant_door() -> AudioStreamWAV:
	var n := int(RATE * 0.6)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	for i in n:
		var t := float(i) / RATE
		var env := exp(-t * 7.0)
		lp += (rng.randf_range(-1.0, 1.0) - lp) * 0.04  # very dull = far away
		var knock := sin(TAU * 68.0 * t) * exp(-t * 16.0) * 0.5
		s[i] = (lp * 1.6 + knock) * env * 0.6
	return _wav(s)


## Stinger: a low metallic groan (structure settling, or something moving).
static func metal_groan() -> AudioStreamWAV:
	var n := int(RATE * 1.1)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	var phase := 0.0
	for i in n:
		var t := float(i) / n
		var env := sin(PI * minf(t * 1.3, 1.0)) * 0.8
		var freq := 70.0 - 18.0 * t + 5.0 * sin(t * 30.0)
		phase += freq / RATE
		var saw := 2.0 * fmod(phase, 1.0) - 1.0
		lp += (saw - lp) * 0.05
		s[i] = (lp + lp * rng.randf_range(-0.2, 0.2)) * env * 0.5
	return _wav(s)


## Stinger: a slow sub-bass swell, felt more than heard.
static func sub_swell() -> AudioStreamWAV:
	var n := int(RATE * 1.4)
	var s := PackedFloat32Array()
	s.resize(n)
	var phase := 0.0
	for i in n:
		var t := float(i) / n
		var freq := 32.0 + 30.0 * t
		phase += freq / RATE
		s[i] = sin(TAU * phase) * sin(PI * t) * 0.6
	return _wav(s)


## Falling, wobbling saw through a low-pass with noise grit: a slow creak.
static func door_creak() -> AudioStreamWAV:
	var n := int(RATE * 1.2)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	var phase := 0.0
	for i in n:
		var t := float(i) / n
		var env := sin(PI * minf(t * 1.25, 1.0)) * 0.8
		var freq := 95.0 - 25.0 * t + 8.0 * sin(t * 43.0)
		phase = fmod(phase + freq / RATE, 1.0)
		var saw := 2.0 * phase - 1.0
		lp += (saw - lp) * 0.08
		s[i] = (lp + lp * rng.randf_range(-0.35, 0.35)) * env * 0.55
	return _wav(s)


## Two dead thunks: a handle that won't turn.
static func locked_rattle() -> AudioStreamWAV:
	var n := int(RATE * 0.3)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	for i in n:
		var t := float(i) / RATE
		var env := 0.0
		for start in [0.0, 0.13]:
			if t >= start:
				env = maxf(env, exp(-(t - start) * 45.0))
		lp += (rng.randf_range(-1.0, 1.0) - lp) * 0.3
		s[i] = lp * env * 0.6
	return _wav(s)


## Low sine + rumbling noise bed for the ride between floors.
static func elevator_rumble(seconds: float) -> AudioStreamWAV:
	var n := int(RATE * seconds)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	for i in n:
		var t := float(i) / RATE
		var fade := minf(t / 0.4, 1.0) * minf((seconds - t) / 0.5, 1.0)
		lp += (rng.randf_range(-1.0, 1.0) - lp) * 0.06
		s[i] = (sin(TAU * 38.0 * t) * 0.35 + lp * 1.4) * fade * 0.5
	return _wav(s)


## A tired arrival chime.
static func elevator_ding() -> AudioStreamWAV:
	var n := int(RATE * 0.9)
	var s := PackedFloat32Array()
	s.resize(n)
	for i in n:
		var t := float(i) / RATE
		s[i] = (sin(TAU * 740.0 * t) * 0.32 + sin(TAU * 1480.0 * t) * 0.12) * exp(-t * 5.0)
	return _wav(s)
