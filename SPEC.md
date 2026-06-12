# Vacancy — Game Specification

> A short first-person descent through the fluorescent-lit sublevels of a building that closed hours ago, where every empty room you walk into looks a little more like the last one you tried to leave.

A PSX-style liminal-space horror walker built in **Godot 4.x** (GDScript). Small scope, high atmosphere. The dread comes from repetition, wrongness, and restraint — not jump scares.

---

## 1. Core Pillars

1. **Liminal repetition** — the player descends through floors that subtly loop and degrade. Rooms recur with small wrong differences. The player should never be certain they're making progress.
2. **PSX/VHS aesthetic as the mood engine** — low internal resolution, vertex snapping, affine (perspective-incorrect) texture mapping, dithering, and a VHS/CRT post-process layer. The jank IS the art direction.
3. **No combat, no inventory bloat** — this is a walker. The only verbs are move, look, interact (doors/buttons/notes), and crouch. Tension comes from navigation and sound.

---

## 2. Technical Target

- **Engine:** Godot 4.3+ (use `4.x` stable). Forward+ renderer is fine; we deliberately downscale.
- **Language:** GDScript only. No C# dependency.
- **Platform:** Primary export target is **Linux x86_64** (single self-contained binary). Keep export presets for Linux first; Windows secondary.
- **Resolution strategy:** Render the 3D world into a `SubViewport` at **320×240** (or 256×224 for extra crunch), then stretch that texture to the window with nearest-neighbor filtering. UI renders at native res on top.
- **Target framerate:** 60 fps cap, but the look is locked low-res regardless.
- **Repo layout:** assume this becomes a self-hosted project (ximg-style). Keep it a clean single-repo Godot project with no external asset-store dependencies — everything procedural or placeholder-grey until art is dropped in.

---

## 3. Project Structure

```
vacancy/
├── project.godot
├── scenes/
│   ├── Main.tscn              # bootstraps, holds the SubViewport + post stack
│   ├── Player.tscn            # CharacterBody3D + camera rig
│   ├── floors/
│   │   ├── FloorBase.tscn     # reusable floor template (rooms, corridors)
│   │   ├── Floor_Lobby.tscn
│   │   ├── Floor_Sublevel.tscn
│   │   └── ...
│   └── props/
│       ├── Door.tscn
│       ├── Elevator.tscn
│       ├── Note.tscn          # readable text pickup
│       └── Flicker_Light.tscn
├── scripts/
│   ├── player.gd
│   ├── interactable.gd        # base class / interface
│   ├── door.gd
│   ├── elevator.gd
│   ├── floor_manager.gd       # loads/unloads floors, drives the descent
│   ├── loop_controller.gd     # the "rooms recur wrong" logic
│   └── game_state.gd          # autoload singleton: progress, flags, descent depth
├── shaders/
│   ├── psx_vertex_snap.gdshader
│   ├── affine_texture.gdshader   # (combined with vertex snap in one spatial shader)
│   ├── post_dither.gdshader      # screen-space ordered dithering
│   └── post_vhs_crt.gdshader     # scanlines, chroma offset, noise, wobble
├── materials/
├── audio/
│   ├── ambience/              # fluorescent hum loop, HVAC drone, room tone
│   ├── footsteps/
│   └── sfx/
├── fonts/
└── assets/                    # placeholder greybox textures, swap later
```

`game_state.gd` is an autoloaded singleton. It owns: `descent_depth` (int), `seen_rooms` (dictionary), and event flags. `floor_manager.gd` reads it to decide what to spawn next.

---

## 4. Player Controller (`player.gd`)

- `CharacterBody3D`. WASD movement, mouse look, `Shift` to walk-slow, `Ctrl` to crouch, `E` to interact.
- Headbob is **subtle** — a small sine offset on the camera. Disable it during crouch.
- Movement speed deliberately slow (~3.0 m/s walk). This is not an action game.
- Raycast from camera center each frame to detect `interactable.gd` nodes within ~2.5m; show a minimal "[E]" prompt in the UI when one is in range.
- Footstep audio triggered by distance traveled, not a timer, so it stays in sync at any speed. Pitch-randomize ±5%.
- No jump. (Optional: a tiny step-up so the player doesn't catch on baseboards.)

## 5. Interaction System

- `interactable.gd` defines a single method `interact(player)` and an `prompt_text` export.
- **Doors** (`door.gd`): open/close with a slow creak, some are locked (require a flag), some open into a room the player *just left*. A door can be flagged `is_loop_door` — opening it teleports the player to a near-identical room with one altered detail (see §6).
- **Elevator** (`elevator.gd`): the descent mechanism. Player enters, presses a floor button, doors close, ambience shifts, `game_state.descent_depth += 1`, doors reopen onto the next (more degraded) floor. The elevator panel should sometimes offer floors that shouldn't exist (B7, B12, a blank button).
- **Notes** (`note.gd`): readable text fragments that build the "building that closed hours ago" backstory. Keep them sparse and ambiguous. Display as an overlay; `E` or `Esc` to dismiss.

## 6. The Loop System (`loop_controller.gd`) — the heart of the game

This is what makes it *Vacancy* and not just a walking sim. Goals:

- As `descent_depth` increases, the same room prefabs are reused but with escalating "wrongness" applied procedurally:
  - lights flicker more / go out
  - a chair is moved, a door that was closed is open
  - exit signs point the wrong way
  - the room the player tries to *leave* is occasionally the room they *re-enter*
  - color grading desaturates and the VHS wobble intensifies with depth
- Implement wrongness as a list of toggleable "anomaly" modifiers applied to a floor on load, weighted by depth. Each anomaly is small and individually deniable. Never more than ~2 active per room early on.
- Track `seen_rooms` so a room recurring can deliberately differ from its first appearance ("a little more like the last one you tried to leave").
- **Win/end condition:** after N descents (target: a 10–20 minute playthrough), the elevator opens onto the original lobby — but empty, lit wrong, and the exit door now leads back to the elevator. One final note or visual beat, then a quiet cut to black. No monster reveal. The horror is that there's no exit.

Keep the anomaly system data-driven so I can author new anomalies without touching core logic.

## 7. PSX / VHS Render Pipeline

**Geometry shader** (`psx_vertex_snap.gdshader`, a `spatial` shader applied to world materials):
- Snap vertex positions to a low-resolution grid in clip/screen space (the classic PSX "vertex jitter"). Expose `snap_resolution` as a uniform.
- Affine texture mapping: defeat perspective-correct UVs to get the warbling PSX texture swim. Achieve by scaling UVs by vertex depth and dividing in fragment, or via `noperspective`-style emulation.
- Vertex-lit / low-precision lighting. Flat or per-vertex shading preferred over smooth.

**Post stack** (full-screen, order matters), applied to the SubViewport texture:
1. `post_dither.gdshader` — ordered (Bayer 4×4 or 8×8) dithering with a reduced color palette / bit-depth crunch.
2. `post_vhs_crt.gdshader` — scanlines, slight barrel distortion, chromatic aberration on edges, tracking noise, occasional vertical roll/wobble. Expose an `intensity` uniform that `game_state.descent_depth` drives upward.

Internal render at 320×240 → nearest-neighbor upscale to window. UI/text layer renders crisp on top so notes stay readable.

## 8. Audio

- Looping **fluorescent hum** + **HVAC drone** as the bed. This is 70% of the atmosphere — prioritize it.
- Reverb buses per space type (small room vs long corridor vs stairwell). Use Godot's `AudioServer` bus effects.
- Footsteps with surface variation (carpet/concrete/tile) keyed off a material tag on the floor mesh.
- Sparse, non-musical stingers tied to anomaly events — a distant door, a hum that drops out for two seconds of total silence, etc. Silence is a tool.
- All audio can start as placeholder/synthesized; structure the buses so real assets drop in cleanly.

## 9. Build / Tooling Notes

- Include a `.gitignore` for Godot (`.godot/`, `export.cfg` secrets, builds).
- Provide an `export_presets.cfg` configured for Linux/X11 export, single binary.
- Add a short `README.md`: how to open in Godot 4.x, how to run, how to export the Linux build, and a one-paragraph design statement.
- Optional but appreciated: a `Makefile` or shell script wrapping `godot --headless --export-release "Linux/X11" build/vacancy` so the Linux build is one command. (Assume `godot` is on PATH.)
- No external addons or asset-store packages. Everything in-repo.

## 10. Build Order (do it in this sequence)

1. Project skeleton + autoload `game_state.gd` + `Main.tscn` with the 320×240 SubViewport upscale working (render a greybox cube to confirm the low-res pipeline).
2. Player controller with mouse look, movement, crouch, footsteps.
3. PSX spatial shader (vertex snap + affine) on greybox geometry — confirm the look before building content.
4. One greyboxed floor (`Floor_Lobby`) with doors and an elevator.
5. Interaction system + notes + elevator descent incrementing `descent_depth`.
6. Post-process stack (dither, then VHS/CRT), intensity wired to depth.
7. Loop/anomaly system — data-driven wrongness modifiers.
8. Audio beds + reverb buses + anomaly stingers.
9. End sequence + the no-exit final beat.
10. Linux export preset + README + build script.

## 11. Explicitly Out of Scope (v1)

- No combat, weapons, enemies, or chase sequences.
- No save system (a 10–20 min game doesn't need one).
- No multiplayer, no networking, no leaderboard.
- No store assets — placeholder greybox + procedural until art is authored.

---

**Tone reminder for the implementer:** restraint over spectacle. Empty, quiet, fluorescent, slightly wrong. If a feature makes the game *louder*, it's probably wrong for *Vacancy*.

