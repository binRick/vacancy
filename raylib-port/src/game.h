// Vacancy — raylib 6 port. Shared data model and module interface.
//
// One header for the whole game keeps the cross-references between the small
// translation units (player, world, audio, anomalies, ...) easy. The structure
// mirrors the Godot original: a GameState singleton, a FloorManager that swaps
// floors mid-descent, a data-driven anomaly engine, and a procedural-audio
// director. See the repo README for the per-file mapping back to the .gd files.
#ifndef VACANCY_GAME_H
#define VACANCY_GAME_H

#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>
#include <stdint.h>

// ---- internal resolution / window (SPEC §7) ------------------------------
#define INTERNAL_W 320
#define INTERNAL_H 240
#define WINDOW_W 1280
#define WINDOW_H 960

// After this many descents the elevator opens onto the wrong final lobby.
#define END_DEPTH 7

#define AUDIO_RATE 22050

// ---- fixed-capacity floor contents ---------------------------------------
#define MAX_SURFACES 96
#define MAX_FLOOR_LIGHTS 16
#define MAX_DOORS 8
#define MAX_PROPS 8
#define MAX_NOTES 4
#define MAX_COLLIDERS 96
#define MAX_ROOMS 8
#define MAX_SHADER_LIGHTS 8   // per-room light budget (matches psx.fs)

// Diffuse texture slots (procedurally generated in world.c).
enum { TEX_CHECKER, TEX_PANEL, TEX_CEILING, TEX_FLAT, TEX_COUNT };

// Interact target kinds (player camera raycast).
enum {
    TARGET_NONE = 0,
    TARGET_DOOR,
    TARGET_NOTE,
    TARGET_ELEV_CALL,
    TARGET_ELEV_DESCEND,
};

// Axis-aligned collision/interact volume in world space.
typedef struct { Vector3 min, max; } Box;

// One static box of world geometry. The size+UV are baked into `mesh`, so it
// draws with a translate-only model matrix.
typedef struct {
    Mesh mesh;
    Vector3 pos;     // world center
    int tex;         // TEX_*
    Color color;     // colDiffuse tint
    int room;        // light grouping
} Surface;

// An omni light. Anomalies mutate energy/color/flicker at runtime.
typedef struct {
    Vector3 pos;
    Color color;          // current (hue anomaly shifts this)
    float baseEnergy;     // configured
    float energy;         // current steady value (light_out zeroes it)
    float range;
    int room;
    bool flicker;         // FlickerFx attached
    float flickerIntensity;
    float flClock, flBurstEnd, flNextBurst;
    float renderEnergy;   // resolved each frame (flicker applied)
} Light;

// Hinged door (door.gd). Swings the panel about a hinge offset from `pos`.
typedef struct {
    char name[24];
    Vector3 pos;          // doorway centre at floor level
    float baseYaw;        // orientation (radians, multiples of PI/2)
    bool locked, isOpen, busy;
    char prompt[24];
    char lockedText[96];
    float openAngleDeg;
    float swingSeconds;
    float curAngle;       // current swing (radians)
    float startAngle, targetAngle, swingElapsed;
    bool swinging;
    int room;
    Color color;
    Mesh mesh;            // panel 0.9 x 2.1 x 0.06
} Door;

// Movable greybox prop (anomaly_movable group; prop_shift target).
typedef struct {
    Vector3 pos;
    float yaw;
    int room;
    Mesh mesh;
    Color color;
} Prop;

// Readable note (note.gd).
typedef struct {
    char name[24];
    Vector3 pos;
    float yaw;
    int room;
    bool isFinal;
    char text[512];
    Mesh mesh;
    Color color;
} Note;

// Elevator (elevator.gd + elevator_button.gd). Cab walls live as Surfaces; the
// two sliding panels and the two buttons live here.
typedef struct {
    Vector3 pos;
    float doorOffset;     // current slide: 0 closed .. 0.7 open
    float doorStart, doorTarget, doorElapsed;
    bool doorsMoving;
    bool doorsOpen;
    int room;
    Mesh doorMesh;        // 0.7 x 2.3 x 0.1
    Color doorColor;
    Vector3 callBtnPos, insideBtnPos;
    Vector3 callBtnSize, insideBtnSize;
    Mesh callBtnMesh, insideBtnMesh;
    Color btnColor;
} Elevator;

typedef struct {
    char name[32];
    char surface[16];     // footstep timbre
    char space[16];       // reverb character
    Vector3 spawn;

    int surfaceCount; Surface surfaces[MAX_SURFACES];
    int lightCount;   Light lights[MAX_FLOOR_LIGHTS];
    int doorCount;    Door doors[MAX_DOORS];
    int propCount;    Prop props[MAX_PROPS];
    int noteCount;    Note notes[MAX_NOTES];
    int colliderCount; Box colliders[MAX_COLLIDERS];

    bool hasElevator; Elevator elevator;

    bool hasFalseExit; Box falseExitArea; bool falseExitArmed;
} Floor;

// ---- globals --------------------------------------------------------------
typedef struct {
    int descentDepth;
    struct { char name[32]; int count; } seen[32]; int seenCount;
    struct { char name[32]; bool val; } flags[32]; int flagCount;
    char currentFloorName[32];
    char currentSurface[16];
    bool lastNoteFinal;
    Vector3 falseExitPos; float falseExitYaw; bool falseExitSet;
} GameState;

typedef struct {
    Vector3 pos;          // feet, y == 0 (flat game, no jump)
    float yaw, pitch;     // radians
    Vector3 velocity;     // horizontal only
    bool crouching;
    float eyeHeight;      // animated (crouch)
    float bodyHeight;
    float bobPhase, stepAccum, camBobY;
    Vector3 lastPos;
    float radius;
    int targetType, targetIndex;
    char targetPrompt[32];
} Player;

// Native-resolution UI state (drawn crisp over the post stack by main.c).
typedef struct {
    char caption[160]; float captionTimer; int captionSeq;
    bool noteOpen; char noteText[512];
    float fadeAlpha;          // 0..1 black overlay
    float endTitleAlpha;      // 0..1 "vacancy" title
} UIState;

extern GameState gs;
extern Player player;
extern Floor *F;          // current floor
extern UIState ui;
extern bool g_world_paused;   // note overlay freezes the world

// Scripted movement (demo/record mode): when set, player_update takes its move
// vector from here instead of the keyboard (yaw is driven directly by main.c).
extern bool g_scripted_input;
extern Vector2 g_scripted_move;   // local (x=right, y=forward-negative), as keyboard

// ---- game_state.c ---------------------------------------------------------
void gs_descend(void);
void gs_set_flag(const char *flag, bool value);
bool gs_get_flag(const char *flag);
void gs_show_caption(const char *text);
void gs_open_note(const char *text, bool is_final);
void gs_mark_room_seen(const char *room);
int  gs_room_visits(const char *room);
void gs_emit_anomaly(const char *id);   // -> audio stinger hook

// ---- rng.c ----------------------------------------------------------------
typedef struct { uint64_t state, inc; } Rng;
uint64_t vac_hash(const char *s);        // string -> 64-bit seed
void  rng_seed(Rng *r, uint64_t seed);
float rng_f(Rng *r);                     // [0,1)
float rng_range(Rng *r, float lo, float hi);
uint32_t rng_u(Rng *r);

// ---- world.c (geometry, textures, floor builders) -------------------------
void world_init_textures(void);
Texture2D world_texture(int tex);
Mesh world_make_box(Vector3 size, float uvScale);
void build_lobby(Floor *f, bool ending);
void build_sublevel(Floor *f);
void floor_unload(Floor *f);
int  floor_add_collider(Floor *f, Vector3 center, Vector3 size);

// ---- entities.c (doors, elevator, notes, flicker, false exit) -------------
void door_interact(Door *d);
void door_set_open_instant(Door *d, bool open);
void door_update(Door *d, float dt);
Box  door_closed_box(const Door *d);
Matrix door_panel_matrix(const Door *d);   // world transform of the swung panel
void elevator_open_doors(Elevator *e);
void elevator_close_doors(Elevator *e);   // immediate request (no guard)
void elevator_update(Elevator *e, float dt);
void elevator_ding(Elevator *e);
void flicker_update(Light *l, float dt);
void entities_update_floor(Floor *f, float dt);

// ---- interact.c -----------------------------------------------------------
void interact_update(Floor *f, Camera3D cam);   // sets player target + prompt
void interact_activate(Floor *f);               // E pressed

// ---- player.c -------------------------------------------------------------
void player_init(Vector3 pos, float yaw);
void player_update(Floor *f, float dt, bool allow_input);
Camera3D player_camera(void);
void player_play_footstep(void);

// ---- floor_manager.c ------------------------------------------------------
void fm_init(void);                 // build starting lobby
void fm_update(float dt);           // drive descent state machine
void fm_request_descent(void);      // inside button
void fm_dev_swap_to_sublevel(void);
void fm_dev_jump_to_ending(void);
bool fm_is_riding(void);
void fm_trigger_false_exit(void);

// ---- anomalies.c ----------------------------------------------------------
void loop_apply(Floor *f, int depth);

// ---- audio.c (audio_director.gd) ------------------------------------------
void audio_init(void);
void audio_update(float dt);
void audio_shutdown(void);
void audio_set_space(const char *space);
void audio_on_depth_changed(int depth);
void audio_on_anomaly(const char *id);
void audio_play_positional(int which, Vector3 worldPos, float db, float pitch);
void audio_footstep(const char *surface);   // player's own step (non-positional)
void audio_elevator_rumble(float seconds);   // the ride bed
void audio_master_db(float db);
void audio_end_fade(float seconds);
float audio_master_volume(void);

// sound ids for audio_play_positional / one-shots
enum {
    SND_DOOR_CREAK, SND_LOCKED, SND_ELEV_DING, SND_FOOTSTEP_TILE,
    SND_FOOTSTEP_CARPET, SND_FOOTSTEP_CONCRETE, SND_DISTANT_DOOR,
    SND_METAL_GROAN, SND_SUB_SWELL,
};

// ---- synth.c (sound_synth.gd) ---------------------------------------------
// Each returns a freshly-generated raylib Wave (16-bit mono @ AUDIO_RATE).
Wave synth_footstep(const char *surface);
Wave synth_fluorescent_hum(void);
Wave synth_hvac_drone(void);
Wave synth_distant_door(void);
Wave synth_metal_groan(void);
Wave synth_sub_swell(void);
Wave synth_door_creak(void);
Wave synth_locked_rattle(void);
Wave synth_elevator_rumble(float seconds);
Wave synth_elevator_ding(void);
// Encode a Wave to an in-memory .wav byte buffer (caller frees). For looping
// beds via LoadMusicStreamFromMemory.
unsigned char *synth_encode_wav(Wave w, int *out_size);

// ---- render.c -------------------------------------------------------------
void render_init(void);
void render_shutdown(void);
void render_set_vhs_intensity(float v);
void render_world(Floor *f, Camera3D cam);   // -> internal world render texture
void render_post_to_screen(void);            // dither + vhs upscale to window
RenderTexture2D render_world_rt(void);       // for screenshots

// ---- telemetry.c ----------------------------------------------------------
void telemetry_init(const char *path);
void telemetry_update(float dt, bool paused, const char *scene);
void telemetry_event(const char *name, const char *json_fields); // fields: "\"k\":v,..."
void telemetry_shutdown(void);
bool telemetry_enabled(void);

#endif // VACANCY_GAME_H
