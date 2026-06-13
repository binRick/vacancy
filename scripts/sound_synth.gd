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


## Short low-passed noise burst with a sharp decay: a dull footfall.
static func footstep() -> AudioStreamWAV:
	var n := int(RATE * 0.14)
	var s := PackedFloat32Array()
	s.resize(n)
	var rng := RandomNumberGenerator.new()
	var lp := 0.0
	for i in n:
		var env := pow(1.0 - float(i) / n, 2.5)
		lp += (rng.randf_range(-1.0, 1.0) - lp) * 0.18
		s[i] = lp * env * 0.5
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
