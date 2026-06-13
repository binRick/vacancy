# Vacancy

A short first-person descent through the fluorescent-lit sublevels of a
building that closed hours ago, where every empty room you walk into looks a
little more like the last one you tried to leave.

*Vacancy* is a PSX/VHS-styled liminal-space horror walker built in Godot 4.x
(GDScript only). There is no combat and no inventory — you move, look, crouch,
and interact. The dread comes from repetition, wrongness, and restraint: the
fluorescent hum under everything, a chair that has moved, a light that has
died, the elevator that only ever goes down. After enough descents the doors
open back onto the lobby where you started — empty, lit wrong, with no way out.
There is no monster. The horror is that there is no exit.

---

## Requirements

- **Godot 4.6.x** (stable). The Forward+ renderer is fine; the game
  deliberately renders the 3D world at 320×240 and upscales with
  nearest-neighbor filtering.
- For building a Linux binary: the matching **export templates** must be
  installed (Godot → *Editor* → *Manage Export Templates*, or drop the
  contents of `Godot_v4.6.x-stable_export_templates.tpz` into the templates
  directory). Not needed just to play from the editor.

## Play

Open the project in Godot and press *Play*, or from a terminal:

```sh
./run.sh
```

`run.sh` imports resources, then launches the game with a per-second
telemetry log written to `run.log` (JSON lines: a heartbeat with depth,
position, and audio state plus discrete events — useful for debugging a
playthrough without watching over your own shoulder).

### Controls

| | |
|---|---|
| **WASD** | move |
| **Mouse** | look |
| **Shift** | walk slowly |
| **Ctrl** | crouch |
| **E** | interact (doors, buttons, notes) |
| **Esc** | release the mouse |

## Build (Linux x86_64)

```sh
./build.sh
```

Produces `build/vacancy.x86_64` — a single self-contained binary (the PCK is
embedded). Equivalent to:

```sh
godot --headless --path . --export-release "Linux" build/vacancy.x86_64
```

## Project layout

```
scenes/      Main, Player, floors/ (Lobby, Sublevel), props/ (Door, Elevator, Note)
scripts/     player, floor_manager, loop_controller, anomalies/, game_state, audio_director, ...
shaders/     psx_vertex_snap (vertex jitter + affine UVs), post_dither, post_vhs_crt
assets/      procedural placeholder textures (swap later)
tools/       gen_textures.gd (regenerates the placeholder textures)
```

`game_state.gd` is an autoloaded singleton owning `descent_depth`,
`seen_rooms`, and event flags; `floor_manager.gd` swaps floors during the
elevator ride; `loop_controller.gd` applies depth-weighted **anomalies**
(authoring a new one is a single script in `scripts/anomalies/` plus one
registry line). The PSX look and the VHS/CRT post stack both intensify with
depth.

## Status

Built to the `SPEC.md` build order, steps 1–10. Everything is procedural or
placeholder greybox; the structure is in place for real art and audio to drop
in (textures in `assets/`, audio synthesized in `scripts/sound_synth.gd` and
routed through named buses in `scripts/audio_director.gd`).
