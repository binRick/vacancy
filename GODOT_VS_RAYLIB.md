# Godot vs. raylib — two builds of the same game

This repository contains *Vacancy*, a small PSX/VHS liminal-horror walker, built
**twice**: once in **Godot 4.6 / GDScript** (`godot-port/`) and once as a
from-scratch **C / raylib 6.0** port (`raylib-port/`). Same game, same layout, same
systems, same ending — two completely different engines underneath.

Both implementations live side by side on the `main` branch — [`godot-port/`](godot-port/)
and [`raylib-port/`](raylib-port/) — so the two builds can be checked out and
diffed directly against each other.

That makes it an unusually clean apples-to-apples comparison. Everything below is
measured from the two implementations in this repo, not from general reputation.

> **How to read this.** Hard numbers (line counts, binary sizes, deps, frame
> rates) are measured. Anything about *effort* or *cost* is an estimate — but a
> grounded one: **both** versions were built with Claude. The Godot original was a
> greenfield build from `SPEC.md` (10 numbered steps, 12 commits, ~2 days); the
> raylib port was built in one session. So the cost comparison below is two real
> Claude builds, not one build and an inference — with one big asymmetry called
> out in §2.

---

## TL;DR

| Dimension | Godot 4 / GDScript | raylib 6 / C |
|---|---|---|
| **Lines you author** | ~2,730 | ~2,720 |
| ...as code | 1,595 GDScript + 128 shader | 2,431 C + 197 shader |
| ...as data/scenes | 1,003 lines of `.tscn` | (geometry is code: `world.c`) |
| **Files** | 23 `.gd`, 9 `.tscn`, 3 shaders | 13 `.c`, 1 `.h`, 4 shaders |
| **What the engine gives you** | physics, audio buses, scene tree, tweens, lighting+shadows, UI, import | a window, GL, input, audio mixer, math |
| **Shipping binary** | 68 MB (engine embedded) | **1.7 MB** (static raylib) |
| **Runtime deps** | none (self-contained) | system frameworks only (OpenGL, Cocoa) |
| **Frame rate (this game)** | 60 fps locked, easily | 60 fps locked; **hundreds uncapped** |
| **Build toolchain** | Godot editor (~100 MB app) | any C compiler (already on every dev box) |
| **How it was built w/ Claude** | greenfield from spec, 10 incremental steps, 12 commits | one-session port of the finished game |

The headline surprise: **the two builds are almost the same size in lines you
write** (~2,720 each). They just distribute that effort completely differently —
Godot pushes ~1,000 lines into declarative scene files and leans on the engine for
systems; raylib makes you write the systems but expresses the world as compact,
looped code.

---

## 1. Lines of code — where the work goes

**Godot (2,726 authored lines):**

```
GDScript   1,595   (23 files; player, floors, anomalies, audio, telemetry…)
.gdshader    128   (3 files; PSX vertex-snap, dither, VHS/CRT)
.tscn      1,003   (9 files; the scene graph: nodes, transforms, materials)
```

**raylib port (2,715 authored lines):**

```
C source   2,128   (13 files)
C header     303   (1 shared header — the whole data model)
GLSL         197   (4 files; same three shaders, split vs/fs)
Make/sh       87   (Makefile + build.sh + run.sh)
```

The interesting comparison is **per subsystem**:

| Subsystem | Godot | raylib | Why |
|---|---|---|---|
| World geometry | `Floor_*.tscn` ≈ 600 lines of declarative boxes | `world.c` = 424 lines | C uses loops/helpers; `.tscn` is flat. **C wins on geometry.** |
| Audio synth | `sound_synth.gd` 213 | `synth.c` 228 | Near-identical (it's just DSP math). |
| Audio routing | `audio_director.gd` 157 (uses engine buses+reverb) | `audio.c` 250 (hand-rolled mixer pool + reverb) | **Godot wins** — buses/reverb are free. |
| Player + collision | `player.gd` 168 (CharacterBody3D does collision) | `player.c` 154 (hand-written cylinder-vs-AABB) | Tie — Godot hides the collision; raylib's is ~40 lines. |
| Render pipeline | SubViewport nodes in `.tscn` + `main.gd` | `render.c` 154 (RT chain, per-room light binding) | **Godot wins** — wiring is point-and-click; raylib binds it by hand. |
| Lighting | engine lights + shadow maps (0 lines) | per-room light arrays in shader + `render.c` | **Godot wins big** — raylib has no built-in lighting at all. |

### Measured with `scc`

Running [`scc`](https://github.com/boyter/scc) over both trees (vendored raylib and
build artifacts excluded) turns the impression into numbers:

| `scc` metric (Code = excl. blanks & comments) | Godot | raylib |
|---|---:|---:|
| **Code lines** | 1,971 | 2,141 |
| — logic (GDScript / C + header) | 1,167 | 2,014 |
| — declarative scene data | 804 | — |
| — shaders | ~115 \* | 106 |
| **Cyclomatic complexity** | **185** | **435** |
| COCOMO est. cost (organic) | $55,082 | $59,463 |
| COCOMO est. schedule | 4.57 mo | 4.71 mo |
| COCOMO est. people | 1.07 | 1.12 |

\* `scc` doesn't recognise Godot's `.gdshader` extension, so the 3 shader files
(~128 lines) fall out of its Godot totals; it also misclassifies the port's GLSL
`.fs/.vs` as "F#". Otherwise the figures are as-reported.

Two things jump out. **By every gross measure the projects are the same size** —
code lines within ~9%, COCOMO cost within ~8%, both scored "≈4.6 months, ~1 person"
by COCOMO's (human-team, LOC-driven) model. (Take the dollar/month figures as a
*relative* signal only — both were actually built by Claude in a session or two,
not five months.)

But the **complexity numbers diverge sharply: 185 vs 435 — about 2.4×.** And almost
all of the port's complexity — **424 of its 435** — lives in the C, exactly where
collision, per-room lighting, the audio mixer, and the descent state machine are
*implemented* instead of *configured*. Godot's 804 lines of scene data score a flat
**zero** complexity: it's declarative description, not branching logic. Same
project size, wildly different distribution of control flow — which is the entire
"engine vs. no-engine" tradeoff captured in a single statistic.

**Takeaway:** Godot's leverage is *systems you don't write* (physics, audio buses,
shadowed lighting, scene tree). raylib's leverage is *expressing content as code*
— the 1,003-line scene graph collapses to a 424-line builder. They roughly cancel
out on *size*, but not on *complexity*: in raylib you own ~2.4× the branching logic
because the engine isn't carrying any of it for you. **For a content-heavy game
(many hand-built levels), Godot's scene editor pulls ahead. For a systems-light,
procedurally-defined game, raylib stays flat — at the price of writing the systems
yourself.**

---

## 2. Cost to build with Claude

Both versions of this game were built by Claude, so this isn't a guess. But read
the one big asymmetry first, because it colours everything else.

### The asymmetry: greenfield-from-spec vs. port-from-reference

These were **not equally hard tasks**, and the difference is about the *task*, not
the engine:

- **The Godot build was greenfield.** It was authored from `SPEC.md` — a design
  brief, not an implementation. Claude had to *invent* the architecture: how the
  anomaly engine is structured, how the descent swap works, how the audio buses
  layer. The git history shows this as a careful **10-step incremental build (12
  commits over ~2 days)**, one subsystem per step (skeleton → player → shader →
  floor → interaction → post → loop → audio → ending → export), each verifiable
  in the editor before the next. That cadence *is* the cost of designing as you go.
- **The raylib build was a port.** It had a complete, working reference (the Godot
  game) to translate. The hard design questions were already answered; the job was
  re-expression — and re-implementing the systems Godot gave for free. That's why
  it landed in **one session** rather than ten steps. One session does **not** mean
  raylib is easier than Godot; it means porting a proven design is easier than
  designing from a spec, in any engine.

So: don't read "12 commits vs. 1 session" as "Godot is 12× harder." Most of that
gap is greenfield-vs-port. The engine-specific costs are below.

### Where each engine helped or fought the agent

**Godot made Claude productive by** offering high-level primitives — tweens,
signals, `move_and_slide()`, audio buses, shadowed lights are one call each — so
GDScript is concise and each of the 10 steps was small. The editor also lets you
*see* a half-built game between steps, which suits incremental construction.

**Godot fought the agent on hand-authored scene data.** The 1,003 lines of `.tscn`
are node paths, `ext_resource` IDs, `SubResource` references, and packed
`Transform3D` matrices written by hand. These are easy to get subtly wrong and the
mistakes surface **at runtime**, not at parse time — and verifying needs the ~100 MB
editor plus an import/`.godot` cache step. Tellingly, even *with* Godot's built-in
physics, door collision still needed a dedicated bug-fix commit
(`Fix: open doors now let the player walk through`) — the exact doorway/collision
problem also has to be solved by hand in the C port.

**raylib made Claude productive by** turning the compiler into a free, instant
oracle. Type errors, bad signatures, and missing includes failed at build time —
the whole port compiled after ~2 trivial fixes. The verify loop is tight and
deterministic: `make` → run with `--screenshot` → read the PNG → read the JSON
telemetry. Every claim ("the descent works", "the ending fires") was checked
against a screenshot or a telemetry event. Real bugs (raylib's default Esc-to-quit;
an unported `dev_jump_to_ending` call in the endtest) were each caught in a single
run. Nothing is hidden in an editor — it's all `.c`/`.h`/`.glsl` text the model
fully sees.

**raylib fought the agent by** having no engine: collision, per-room lighting,
the audio mixer/pan/reverb, and the render-target chain are all hand-written. That
is the ~800 extra lines, and it's where the bugs lived.

### Net

For an autonomous agent specifically, the deciding factor is the **verification
loop**, and there raylib has a real edge: a compiler is a hard gate and a
self-contained binary is trivial to screenshot and to drive headlessly, whereas
`.tscn` correctness is only provable by running the editor. Offsetting that, Godot
needs **far less code per feature**, which means fewer chances to introduce a bug
in the first place. Rule of thumb: **Godot is cheaper to *write* (the engine
already has the feature); raylib is cheaper to *verify and reason about* (nothing
is hidden, the compiler gates every change).** And whichever engine you pick,
**porting an existing game costs a fraction of designing one from a spec.**

---

## 3. Expected performance

This game deliberately renders its 3D world at **320×240** and upscales, so it is
trivially cheap for either engine — neither breaks a sweat.

- **raylib port (measured):** with the 60 fps cap removed, the telemetry heartbeat
  reported instantaneous frame rates in the **200–600+ fps** range on this Mac.
  The whole scene is a few dozen un-instanced boxes and ≤8 lights per room; the
  bottleneck is nowhere near the CPU/GPU.
- **Godot:** locks to 60 fps just as easily. Its Forward+ renderer carries more
  baseline per-frame and per-process overhead (a full scene tree, servers,
  physics step, more threads), but at this scale it's irrelevant — both sit at the
  cap with enormous headroom.

**Where it would matter:** raylib's floor is lower — smaller resident memory, near-
instant startup, less per-frame engine tax — so on constrained hardware (handhelds,
old laptops, embedded) or for thousands of dynamic objects, the C build has more
runway before you must optimize. Godot's ceiling is higher *with less work* —
once you want real shadows, GI, large streamed worlds, or a physics-heavy scene,
you'd reimplement a lot of engine in C to keep up.

For *Vacancy specifically*: **performance is a wash; both lock 60.**

---

## 4. Binary size & distribution

| | Godot | raylib |
|---|---|---|
| Shipped binary | **68 MB** (engine + game + PCK, self-contained) | **1.7 MB** |
| Runtime dependencies | none | OS frameworks only (OpenGL, Cocoa/X11) |
| Build inputs not shipped | `.godot/` import cache (1.6 MB) | vendored raylib source (150 MB, transient) + 2.4 MB static lib |

raylib produces a **~40× smaller** binary because you link only the slice of engine
you use. Godot ships the whole runtime in every export, which is why a 2,700-line
game weighs 68 MB. Both are genuinely single-file/self-contained from the player's
side; raylib just carries far less.

---

## 5. Other dimensions worth weighing

- **Determinism / replay.** raylib's explicit, fixed-step-friendly code makes
  deterministic behavior easy (the port's anomaly engine is seeded and
  reproducible). Godot's physics and frame-timed tweens are harder to make
  bit-deterministic.
- **Iteration for a *human*.** Godot wins hard: live scene editing, hot-reload,
  an inspector, visual debugging, profiler. The C port is edit→`make`→run.
- **Asset & content pipeline.** Godot has importers, a material editor, animation
  tools, a UI system. raylib gives you `LoadTexture`/`LoadModel` and you build the
  rest. This game sidesteps it (everything procedural), which flatters raylib.
- **Lighting & shadows.** The single biggest "free lunch" gap. Godot lights cast
  shadows out of the box; raylib has *no* lighting — the port hand-writes point
  lights in a shader and avoids cross-wall leak by binding lights **per room**
  (no shadow maps). Faithful enough under the VHS murk, but it's real work the
  engine would have done.
- **Audio.** Godot's named buses + `AudioEffectReverb` are free and flexible; the
  port hand-rolls a voice pool, positional pan/attenuation, and a compact
  Schroeder reverb (~60 lines) to stand in for the bus.
- **Team & ecosystem.** GDScript + the editor are approachable to non-programmers
  and designers; a huge asset/tutorial ecosystem exists. The C build is
  programmer-only but has zero framework lock-in and trivial interop with C/C++
  libraries.
- **Version & supply-chain risk.** The C port pins raylib 6.0 and builds it from
  source — fully reproducible, no editor-version drift. Godot projects are
  coupled to an editor version and its export templates; `.tscn`/`.godot` formats
  evolve across versions.
- **Source control.** C diffs are clean and reviewable. `.tscn` files diff and
  merge poorly (regenerated IDs, reordered nodes), which bites teams.

---

## 6. Which would you actually choose?

**Reach for Godot when** the game is content-heavy (many hand-authored levels,
lots of art, animation, UI), you want shadows/GI/physics without writing them, a
designer needs to touch the project, or you value iteration speed over runtime
footprint. The 68 MB and the engine coupling buy you a lot of free systems.

**Reach for raylib/C when** you want a tiny, dependency-light, fully-understood
binary; the game is systems-light or procedural; you need determinism, unusual
render control (this PSX pipeline is a good example), or to embed in constrained
hardware; or you simply want no engine between you and the metal.

**For *Vacancy*** — small, procedural, render-driven, no physics or shadows really
needed — **raylib is arguably the better fit**: a 1.7 MB binary, total control of
the PSX/VHS look, and roughly the same amount of code. Godot's advantages
(scene editor, free shadows, audio buses) are least valuable precisely for a game
this minimal and this procedurally defined. The Godot original is the easier place
to *keep adding content* with a designer; the raylib port is the leaner thing to
*ship*.

---

*All figures measured from this repository on macOS (Apple Silicon), raylib 6.0
built from source, Godot 4.6.*
