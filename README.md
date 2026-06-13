# Vacancy

> A short first-person descent through the fluorescent-lit sublevels of a
> building that closed hours ago, where every empty room you walk into looks a
> little more like the last one you tried to leave.

*Vacancy* is a PSX/VHS-styled **liminal-space horror walker** built in Godot
4.x (GDScript only). It is small, quiet, and deliberately ugly in the way old
hardware was ugly. There is no combat, no inventory, no enemy, and no chase.
You move, look, crouch, and interact — and you take the elevator down. The
dread is built entirely out of repetition, wrongness, and restraint.

A full playthrough runs roughly **10–20 minutes**.

---

## What it is

You are alone in an office building after closing. The lights are on because
nobody turned them off. The air handlers are still running. A desk has a note
on it; a door down the hall is open that probably shouldn't be. The only way
on is the elevator, and the elevator only goes **down**.

Each descent opens onto another sublevel that is *almost* the floor you just
left — the same corridors, the same doors, the same hum — but something is
off. A light that was steady is now stuttering. A chair has moved a few
inches and turned to face the wrong way. A tube that was lit is dead. A door
that was shut stands ajar. None of it is loud. Each individual wrongness is
small enough to talk yourself out of. They accumulate.

The game never tells you how far you've gone or how far is left. The floor
counter on the elevator panel is not to be trusted. You are meant to lose
track of whether you are making progress or going in circles — because, in the
end, you are doing both.

The horror here is **architectural and atmospheric**, not a monster. Nothing
jumps out. The most frightening thing the game does is, occasionally, on
arrival at a new floor, cut every sound to total silence for two seconds.

## The premise (story)

The narrative is sparse and ambiguous, delivered only through environmental
detail and a handful of readable notes you find on desks. Nothing is spelled
out. Pieced together, the fragments suggest a building whose night should have
ended hours ago and didn't:

- A **security checklist** — front entrance locked after the last sign-out,
  leave the sublevel lights on for the cleaners, last sign-out 18:42, *no one
  after.* And a final instruction: *"If the elevator gets called from a
  sublevel after close, do not answer it."*
- A **maintenance ticket** for the sublevel corridor lights, closed and
  reopened over and over — *"CLOSED (no fault found). Reopened."* — a loop
  printed on paper.

You answered the elevator.

## How it ends

> This section spoils the ending — which *is* the point of the game, so it's
> documented rather than hidden. Skip it if you'd rather play first.

After enough descents (seven, by default) the doors open not onto another
sublevel but onto the **original lobby** — the one you started in. Except it's
empty, dimmed, and lit a sickly green; one tube is dead and another flickers.
The desk note has changed. The front doors — locked all night — are now
unlocked.

You walk to them, you push them open, and you are standing back at the
elevator. The exit leads inward. There is no way out. You read the last note,
the screen fades, a title holds for a moment in the dark, and that's it. No
reveal, no creature, no explanation. *The horror is that there's no exit.*

---

## How it plays

It's a walking sim. The entire verb set is move, look, crouch, and a single
context interact. Movement is deliberately slow (~3 m/s); this is a game you
are meant to move through carefully, listening.

| Input | Action |
|---|---|
| **W A S D** | move |
| **Mouse** | look |
| **Shift** (hold) | walk slowly |
| **Ctrl** (hold) | crouch |
| **E** | interact — open/close doors, press elevator buttons, read notes |
| **Esc** | release the mouse cursor |

There is no jump. A subtle camera headbob is keyed to distance travelled (not a
timer) so footsteps stay in sync at any speed, and it's disabled while
crouching. Footstep timbre changes with the floor surface (tile, carpet,
concrete). A minimal `[E] …` prompt appears when you're looking at something
you can use; readable notes open as a full-screen overlay that pauses the world
until you dismiss them.

The **elevator** is the spine of the game. Step in, press a button, the doors
close, the cab rumbles, the world is quietly swapped behind the shut doors
(you keep your position relative to the cab, so it feels like one continuous
ride), and the doors open on somewhere worse. The panel sometimes offers floors
that shouldn't exist.

---

## What makes it work (the systems)

### The loop / anomaly system — the heart of the game

The reason a handful of reused rooms reads as an endless, decaying descent is a
small **data-driven anomaly engine** (`scripts/loop_controller.gd` +
`scripts/anomalies/`). When a floor loads, the controller applies a depth-
weighted set of "wrongnesses" to it. Each anomaly is one short script that
extends a common base, declares a `min_depth` and a `weight`, and implements a
single `apply()` method. Adding a new kind of wrongness is one file plus one
line in the registry; the core logic never changes.

The currently authored anomalies:

| Anomaly | From depth | What it does |
|---|---|---|
| **Light flicker** | 1 | a fluorescent tube starts stuttering; flickers harder the deeper you are |
| **Door ajar** | 1 | a door that should be shut stands open — paired with a distant-door sound a beat later |
| **Prop shift** | 1 | something has been moved, not far, and turned 15–60° — paired with a low metal groan |
| **Light out** | 2 | a tube is simply dead; nobody fixed it — paired with a sub-bass swell |
| **Light hue** | 3 | a light has gone slightly green, like it's dying or already has |

How many fire is itself depth-scaled: roughly one or two near the top, up to
four far down. Crucially, the selection is **deterministic per depth *and* per
how many times you've seen that room** — the engine seeds its RNG with
`depth + visit-count + room`. So a room recurring is not random noise; it
deliberately differs from its earlier self, which is what sells *"a little more
like the last one you tried to leave."* The game keeps a `seen_rooms` memory
for exactly this.

### The PSX / VHS render pipeline — the mood engine

The look is not a filter slapped on at the end; the whole 3D world is rendered
the way a 1997 console would have, and the jank is the art direction.

- The world renders into a **320×240 SubViewport** and is upscaled to the
  window (1280×960, 4:3) with **nearest-neighbor** filtering. Everything in 3D
  is locked to that low internal resolution; the UI and note text draw crisp on
  top so they stay readable.
- A **spatial shader** (`shaders/psx_vertex_snap.gdshader`) gives every surface
  the period-correct artifacts: **vertex snapping** (geometry jitters to a
  coarse grid as it moves — the classic PSX wobble) and **affine texture
  mapping** (perspective-incorrect UVs, so textures swim and warp across
  surfaces).
- A two-stage **post stack**: an ordered-dither / bit-crunch pass
  (`post_dither.gdshader`) at the internal resolution, then a **VHS/CRT** pass
  (`post_vhs_crt.gdshader`) — scanlines, slight barrel distortion, chromatic
  aberration, tracking noise, and roll — applied to the upscaled image.

The VHS pass exposes an `intensity` uniform that **rises with descent depth**.
The deeper you go, the more the signal degrades. The picture decaying is the
building decaying.

### Audio — 70% of the atmosphere

All sound is **synthesized procedurally at runtime** (`scripts/sound_synth.gd`)
and routed through named buses (`scripts/audio_director.gd`), so real recorded
assets can drop in later without touching the structure.

- A continuous **bed**: a looping fluorescent hum plus an HVAC drone. This is
  most of the dread. As you descend, the drone presses in and the reverb opens
  up — small-room early, cavernous and wet deep down.
- **Footsteps** vary by floor surface and are pitch-randomised so they never
  sound looped.
- **Sparse, non-musical stingers** are tied to anomaly events — a door
  somewhere you can't see, a groan of moving metal, a swell under a light going
  out — fired a deliberately uneven beat *after* the thing happens, so they
  don't read as feedback.
- **Silence is a tool.** From a few floors down, arriving on a new level
  sometimes ducks *everything* — even your own footsteps — to total silence for
  a couple of seconds. The cut to black at the very end takes the sound with
  it.

---

## Design statement

Restraint over spectacle. Empty, quiet, fluorescent, slightly wrong. The
guiding rule throughout: *if a feature makes the game louder, it's probably
wrong for Vacancy.* The fear is supposed to come from an ordinary place made
subtly uninhabitable, and from the slow certainty that the way out isn't where
you left it.

---

## Requirements

- **Godot 4.6.x** (stable). The Forward+ renderer is fine — the game
  deliberately renders at 320×240 and upscales. GDScript only; no C#, no
  add-ons, no asset-store packages. Everything is procedural or placeholder
  greybox.
- To export a Linux binary you also need the matching **export templates**
  (Godot → *Editor → Manage Export Templates*). Not needed to play from the
  editor.

## Play

Open the project in Godot and press **Play**, or from a terminal:

```sh
./run.sh
```

`run.sh` imports resources (the compile step), then launches the game with a
per-second telemetry log written to `run.log`.

## Build (Linux x86_64)

```sh
./build.sh
```

Produces `build/vacancy.x86_64` — a single self-contained binary with the PCK
embedded. Equivalent to:

```sh
godot --headless --path . --export-release "Linux" build/vacancy.x86_64
```

---

## Telemetry & dev tools

While the game runs it writes **JSON-lines telemetry** to `run.log`: a
once-per-second heartbeat (descent depth, player position/yaw, speed, current
floor, audio state) plus discrete events (doors, notes, descents, which
anomalies were applied, stingers, silences, the ending). It lets you debug a
playthrough — or confirm one ran correctly headless — without watching over
your own shoulder. Tail it live with `tail -f run.log`.

Command-line flags (passed after `--`, e.g. `godot --path . -- --ending`):

| Flag | Effect |
|---|---|
| `--depth=N` | pretend you've already descended N times (scales look/audio/anomalies) |
| `--sublevel` | boot straight onto a sublevel floor, skipping the ride |
| `--ending` | boot straight onto the wrong final lobby |
| `--selftest` | exercise doors, a note, and two full descents with no input, then quit |
| `--endtest` | exercise the whole end beat (false exit + final note + cut to black), then quit |
| `--pose=x,z,yaw` | place the player (for screenshots) |
| `--screenshot=PATH` | save the post-processed window and the raw 320×240 render, then quit |

There's also a headless physics probe, `tools/door_probe.tscn`, used to verify
that door colliders actually swing clear of their doorways.

---

## Project layout

```
scenes/
  Main.tscn              bootstrap: SubViewport pipeline + post stack + native-res UI
  Player.tscn            CharacterBody3D first-person rig
  floors/                FloorBase, Floor_Lobby (start + reused as the wrong ending),
                         Floor_Sublevel (the recurring descent floor)
  props/                 Door, Elevator, Note
scripts/
  game_state.gd          autoload: descent depth, seen-room memory, flags, signals
  telemetry.gd           autoload: the run.log JSON-lines writer
  audio_director.gd      autoload: ambient bed, reverb buses, stingers, silence
  floor_manager.gd       loads/unloads floors, drives the descent and the ending
  loop_controller.gd     applies depth-weighted anomalies on floor load
  anomalies/             one script per kind of wrongness (+ a base class)
  player.gd  door.gd  elevator.gd  note.gd  false_exit.gd  sound_synth.gd  ...
shaders/
  psx_vertex_snap.gdshader   vertex jitter + affine UVs (world surfaces)
  post_dither.gdshader       ordered dithering / bit-crunch at internal res
  post_vhs_crt.gdshader      scanlines, chroma, noise, wobble (depth-scaled)
assets/                  procedural placeholder textures (swap later)
tools/                   gen_textures.gd (regenerates placeholders), door_probe
build.sh  run.sh  export_presets.cfg  SPEC.md
```

`game_state.gd`, `telemetry.gd`, and `audio_director.gd` are autoloaded
singletons. `SPEC.md` is the original design/build specification this was built
against.

## Status

Built end-to-end against `SPEC.md` (steps 1–10): the full descent loop, the
anomaly system, the PSX/VHS pipeline, the procedural audio, and the no-exit
ending all work. Everything visual and audible is **procedural or placeholder
greybox** — the structure is deliberately set up so real textures (drop into
`assets/`) and real audio (replace the synth in `scripts/sound_synth.gd`, buses
already named in `scripts/audio_director.gd`) slot in without reworking
anything.

## Out of scope (v1)

No combat, weapons, enemies, or chase sequences. No save system (a 15-minute
game doesn't need one). No multiplayer or networking. No store assets.
