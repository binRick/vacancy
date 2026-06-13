// World geometry, procedural textures, and the floor builders. Ports
// FloorBase/Floor_Lobby/Floor_Sublevel + the prop scenes. The Godot floors use
// CSG boxes with subtracted doorways; here walls with openings are built as
// segments (two jambs + a lintel) so the gaps are real in both render and
// collision. Geometry is grouped by "room" so lighting can be bound per room
// (no shadow maps, so cross-room light leak is avoided by only lighting a
// room's own surfaces).
#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Room ids (light grouping). The elevator cab is its own room everywhere.
enum { R_A, R_B, R_C, R_D, R_ELEV = 7 };

static Texture2D s_tex[TEX_COUNT];

// ---- procedural greybox textures (gen_textures.gd) ------------------------
static uint32_t trs;
static float tr(void) { trs ^= trs<<13; trs ^= trs>>17; trs ^= trs<<5; return (float)(trs>>8)/(float)(1u<<24); }
static Color darken(Color c, float a) {   // Godot Color.darkened(a) == c*(1-a)
    return (Color){ (unsigned char)(c.r*(1-a)), (unsigned char)(c.g*(1-a)), (unsigned char)(c.b*(1-a)), 255 };
}
static Color c01(float r, float g, float b) {
    return (Color){ (unsigned char)(Clamp(r,0,1)*255), (unsigned char)(Clamp(g,0,1)*255), (unsigned char)(Clamp(b,0,1)*255), 255 };
}

static Texture2D gen_checker(void) {
    Image img = GenImageColor(64, 64, BLACK);
    trs = 0x5EEDu;
    Color dark = c01(0.42f,0.42f,0.45f), light = c01(0.55f,0.55f,0.58f);
    for (int y=0;y<64;y++) for (int x=0;x<64;x++) {
        bool even = (((x>>3)+(y>>3))%2)==0;
        ImageDrawPixel(&img, x, y, darken(even?dark:light, tr()*0.06f));
    }
    Texture2D t = LoadTextureFromImage(img); UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT); SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}
static Texture2D gen_panel(void) {
    Image img = GenImageColor(64, 64, BLACK);
    trs = 0xCAFEu;
    Color fill = c01(0.52f,0.52f,0.55f), seam = c01(0.33f,0.33f,0.37f);
    for (int y=0;y<64;y++) for (int x=0;x<64;x++) {
        bool s = (x%32==0)||(y%16==0);
        ImageDrawPixel(&img, x, y, darken(s?seam:fill, tr()*0.05f));
    }
    Texture2D t = LoadTextureFromImage(img); UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT); SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}
static Texture2D gen_ceiling(void) {
    Image img = GenImageColor(64, 64, BLACK);
    trs = 0xBEEFu;
    Color fill = c01(0.58f,0.58f,0.56f), seam = c01(0.4f,0.4f,0.39f);
    for (int y=0;y<64;y++) for (int x=0;x<64;x++) {
        Color c = (x%32==0||y%32==0)?seam:fill;
        if (tr()<0.08f) c = darken(c,0.12f);
        ImageDrawPixel(&img, x, y, darken(c, tr()*0.04f));
    }
    Texture2D t = LoadTextureFromImage(img); UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT); SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}

void world_init_textures(void) {
    s_tex[TEX_CHECKER] = gen_checker();
    s_tex[TEX_PANEL]   = gen_panel();
    s_tex[TEX_CEILING] = gen_ceiling();
    Image w = GenImageColor(2, 2, WHITE);
    s_tex[TEX_FLAT] = LoadTextureFromImage(w); UnloadImage(w);
    SetTextureFilter(s_tex[TEX_FLAT], TEXTURE_FILTER_POINT);
}
Texture2D world_texture(int tex) { return s_tex[tex]; }

// ---- box mesh (size + per-face UV tiling baked in) ------------------------
static void quad(float *V, float *N, float *T, int *o,
                 Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 n,
                 float du, float dv) {
    Vector3 ps[6] = { a,b,c, a,c,d };
    float uv[12]  = { 0,0, du,0, du,dv, 0,0, du,dv, 0,dv };
    for (int i=0;i<6;i++) {
        V[*o*3+0]=ps[i].x; V[*o*3+1]=ps[i].y; V[*o*3+2]=ps[i].z;
        N[*o*3+0]=n.x; N[*o*3+1]=n.y; N[*o*3+2]=n.z;
        T[*o*2+0]=uv[i*2]; T[*o*2+1]=uv[i*2+1];
        (*o)++;
    }
}

Mesh world_make_box(Vector3 size, float uv) {
    float hx=size.x*0.5f, hy=size.y*0.5f, hz=size.z*0.5f;
    Mesh m = {0};
    m.vertexCount = 36; m.triangleCount = 12;
    float *V = (float*)MemAlloc(36*3*sizeof(float));
    float *N = (float*)MemAlloc(36*3*sizeof(float));
    float *T = (float*)MemAlloc(36*2*sizeof(float));
    int o=0;
    float ux=size.x*uv, uy=size.y*uv, uz=size.z*uv;
    quad(V,N,T,&o, (Vector3){hx,-hy,hz},(Vector3){hx,-hy,-hz},(Vector3){hx,hy,-hz},(Vector3){hx,hy,hz}, (Vector3){1,0,0}, uz, uy);
    quad(V,N,T,&o, (Vector3){-hx,-hy,-hz},(Vector3){-hx,-hy,hz},(Vector3){-hx,hy,hz},(Vector3){-hx,hy,-hz},(Vector3){-1,0,0}, uz, uy);
    quad(V,N,T,&o, (Vector3){-hx,hy,-hz},(Vector3){-hx,hy,hz},(Vector3){hx,hy,hz},(Vector3){hx,hy,-hz}, (Vector3){0,1,0}, ux, uz);
    quad(V,N,T,&o, (Vector3){-hx,-hy,hz},(Vector3){-hx,-hy,-hz},(Vector3){hx,-hy,-hz},(Vector3){hx,-hy,hz},(Vector3){0,-1,0}, ux, uz);
    quad(V,N,T,&o, (Vector3){-hx,-hy,hz},(Vector3){hx,-hy,hz},(Vector3){hx,hy,hz},(Vector3){-hx,hy,hz}, (Vector3){0,0,1}, ux, uy);
    quad(V,N,T,&o, (Vector3){hx,-hy,-hz},(Vector3){-hx,-hy,-hz},(Vector3){-hx,hy,-hz},(Vector3){hx,hy,-hz},(Vector3){0,0,-1}, ux, uy);
    m.vertices=V; m.normals=N; m.texcoords=T;
    UploadMesh(&m, false);
    return m;
}

// ---- floor assembly helpers ----------------------------------------------
int floor_add_collider(Floor *f, Vector3 c, Vector3 s) {
    if (f->colliderCount >= MAX_COLLIDERS) return -1;
    Box b = { (Vector3){c.x-s.x*0.5f, c.y-s.y*0.5f, c.z-s.z*0.5f},
              (Vector3){c.x+s.x*0.5f, c.y+s.y*0.5f, c.z+s.z*0.5f} };
    f->colliders[f->colliderCount] = b;
    return f->colliderCount++;
}

static void add_surf(Floor *f, int room, Vector3 c, Vector3 s, int tex, Color col, float uv) {
    if (f->surfaceCount >= MAX_SURFACES) return;
    Surface *su = &f->surfaces[f->surfaceCount++];
    su->mesh = world_make_box(s, uv);
    su->pos = c; su->tex = tex; su->color = col; su->room = room;
}

// Visual box + optional collider.
static void add_box(Floor *f, int room, Vector3 c, Vector3 s, int tex, Color col, float uv, bool collide) {
    add_surf(f, room, c, s, tex, col, uv);
    if (collide) floor_add_collider(f, c, s);
}

// A wall (thin in one axis) with a rectangular doorway gap. `axis` = 'x' for a
// wall that runs along X (gap measured along X), 'z' for a wall along Z.
static void add_wall_gap(Floor *f, int room, Vector3 c, Vector3 s, int tex, Color col, float uv,
                         char axis, float gapCenter, float gapWidth, float gapTop) {
    float y1 = c.y + s.y*0.5f;
    float gs = gapCenter - gapWidth*0.5f, ge = gapCenter + gapWidth*0.5f;
    if (axis == 'x') {
        float ws = c.x - s.x*0.5f, we = c.x + s.x*0.5f;
        if (gs - ws > 0.01f)
            add_box(f, room, (Vector3){(ws+gs)*0.5f, c.y, c.z}, (Vector3){gs-ws, s.y, s.z}, tex, col, uv, true);
        if (we - ge > 0.01f)
            add_box(f, room, (Vector3){(ge+we)*0.5f, c.y, c.z}, (Vector3){we-ge, s.y, s.z}, tex, col, uv, true);
        if (y1 - gapTop > 0.01f)
            add_box(f, room, (Vector3){gapCenter, (gapTop+y1)*0.5f, c.z}, (Vector3){gapWidth, y1-gapTop, s.z}, tex, col, uv, true);
    } else {
        float ws = c.z - s.z*0.5f, we = c.z + s.z*0.5f;
        if (gs - ws > 0.01f)
            add_box(f, room, (Vector3){c.x, c.y, (ws+gs)*0.5f}, (Vector3){s.x, s.y, gs-ws}, tex, col, uv, true);
        if (we - ge > 0.01f)
            add_box(f, room, (Vector3){c.x, c.y, (ge+we)*0.5f}, (Vector3){s.x, s.y, we-ge}, tex, col, uv, true);
        if (y1 - gapTop > 0.01f)
            add_box(f, room, (Vector3){c.x, (gapTop+y1)*0.5f, gapCenter}, (Vector3){s.x, y1-gapTop, gapWidth}, tex, col, uv, true);
    }
}

static void add_light(Floor *f, int room, Vector3 p, Color col, float energy, float range) {
    if (f->lightCount >= MAX_FLOOR_LIGHTS) return;
    Light *l = &f->lights[f->lightCount++];
    l->pos = p; l->color = col; l->baseEnergy = energy; l->energy = energy;
    l->range = range; l->room = room; l->renderEnergy = energy;
}

static Door *add_door(Floor *f, int room, const char *name, Vector3 pos, float yaw) {
    Door *d = &f->doors[f->doorCount++];
    memset(d, 0, sizeof *d);
    snprintf(d->name, sizeof d->name, "%s", name);
    d->pos = pos; d->baseYaw = yaw; d->room = room;
    d->openAngleDeg = 105.0f; d->swingSeconds = 1.4f;
    snprintf(d->prompt, sizeof d->prompt, "Open");
    snprintf(d->lockedText, sizeof d->lockedText, "Locked.");
    d->color = c01(0.5f,0.42f,0.36f);
    d->mesh = world_make_box((Vector3){0.9f,2.1f,0.06f}, 1.0f);
    return d;
}

static void add_prop(Floor *f, int room, Vector3 pos, float yaw) {
    Prop *p = &f->props[f->propCount++];
    p->pos = pos; p->yaw = yaw; p->room = room;
    p->color = c01(0.55f,0.48f,0.4f);
    p->mesh = world_make_box((Vector3){0.55f,0.55f,0.55f}, 1.0f);
}

static void add_note(Floor *f, int room, const char *name, Vector3 pos, float yaw, const char *text) {
    Note *n = &f->notes[f->noteCount++];
    memset(n, 0, sizeof *n);
    snprintf(n->name, sizeof n->name, "%s", name);
    n->pos = pos; n->yaw = yaw; n->room = room; n->isFinal = false;
    snprintf(n->text, sizeof n->text, "%s", text);
    n->color = c01(0.85f,0.84f,0.78f);
    n->mesh = world_make_box((Vector3){0.22f,0.012f,0.3f}, 1.0f);
}

// Elevator cab (Elevator.tscn) placed at world `pos`, facing +Z.
static void add_elevator(Floor *f, Vector3 pos) {
    f->hasElevator = true;
    Elevator *e = &f->elevator;
    memset(e, 0, sizeof *e);
    e->pos = pos; e->room = R_ELEV;
    e->doorColor = c01(0.55f,0.57f,0.6f);
    e->btnColor = c01(0.3f,0.32f,0.35f);
    e->doorMesh = world_make_box((Vector3){0.7f,2.3f,0.1f}, 0.5f);
    e->callBtnSize = (Vector3){0.12f,0.3f,0.1f};
    e->insideBtnSize = (Vector3){0.06f,0.25f,0.12f};
    e->callBtnMesh = world_make_box(e->callBtnSize, 2.0f);
    e->insideBtnMesh = world_make_box(e->insideBtnSize, 2.0f);
    e->callBtnPos   = Vector3Add(pos, (Vector3){0.85f,1.2f,0.21f});
    e->insideBtnPos = Vector3Add(pos, (Vector3){0.84f,1.2f,-1.5f});
    Color cab = c01(0.42f,0.44f,0.48f);
    // Cab shell (local offsets from pos).
    add_surf(f, R_ELEV, Vector3Add(pos,(Vector3){0,-0.15f,-1}), (Vector3){2,0.3f,2.2f}, TEX_PANEL, cab, 0.5f);
    add_surf(f, R_ELEV, Vector3Add(pos,(Vector3){0,2.45f,-1}),  (Vector3){2,0.3f,2.2f}, TEX_PANEL, cab, 0.5f);
    add_box (f, R_ELEV, Vector3Add(pos,(Vector3){0,1.15f,-2}),  (Vector3){2,2.6f,0.3f}, TEX_PANEL, cab, 0.5f, true);
    add_box (f, R_ELEV, Vector3Add(pos,(Vector3){-1,1.15f,-1}), (Vector3){0.3f,2.6f,2.2f}, TEX_PANEL, cab, 0.5f, true);
    add_box (f, R_ELEV, Vector3Add(pos,(Vector3){1,1.15f,-1}),  (Vector3){0.3f,2.6f,2.2f}, TEX_PANEL, cab, 0.5f, true);
    add_light(f, R_ELEV, Vector3Add(pos,(Vector3){0,2.2f,-0.9f}), c01(1,0.96f,0.88f), 1.0f, 4.0f);
}

void floor_unload(Floor *f) {
    for (int i=0;i<f->surfaceCount;i++) UnloadMesh(f->surfaces[i].mesh);
    for (int i=0;i<f->doorCount;i++) UnloadMesh(f->doors[i].mesh);
    for (int i=0;i<f->propCount;i++) UnloadMesh(f->props[i].mesh);
    for (int i=0;i<f->noteCount;i++) UnloadMesh(f->notes[i].mesh);
    if (f->hasElevator) {
        UnloadMesh(f->elevator.doorMesh);
        UnloadMesh(f->elevator.callBtnMesh);
        UnloadMesh(f->elevator.insideBtnMesh);
    }
    memset(f, 0, sizeof *f);
}

// ===========================================================================
//  LOBBY  (start floor; also the wrong final lobby when ending == true)
// ===========================================================================
static const char *LOBBY_NOTE =
    "SECURITY - EVENING CHECKLIST\n\n"
    "- Front entrance locked after last sign-out.  DONE\n"
    "- Leave sublevel lights ON for the cleaners.\n"
    "- Last sign-out 18:42. No one after.\n\n"
    "If the elevator gets called from a sublevel\n"
    "after close, do not answer it.";

static const char *FINAL_NOTE =
    "You signed out at 18:42.\n"
    "You're sure you signed out.\n\n"
    "The lobby is the same. The doors are\n"
    "the same. The light is almost the same.\n\n"
    "You'll try the doors again in a moment.\n"
    "You always do.";

void build_lobby(Floor *f, bool ending) {
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "Floor_Lobby");
    snprintf(f->surface, sizeof f->surface, "tile");
    snprintf(f->space, sizeof f->space, "room");
    f->spawn = (Vector3){0, 0, 3};

    Color fl = c01(1,1,1), wl = c01(1,1,1), cl = c01(1,1,1), desk = c01(0.55f,0.5f,0.45f);
    Color L = c01(0.92f,1.0f,0.95f);

    // -- Lobby room (R_A) --
    add_surf(f, R_A, (Vector3){0,-0.15f,0},  (Vector3){14.6f,0.3f,8.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_A, (Vector3){0,3.15f,0},   (Vector3){14.6f,0.3f,8.6f}, TEX_CEILING, cl, 0.5f);
    add_wall_gap(f, R_A, (Vector3){0,1.5f,-4.15f}, (Vector3){14.6f,3,0.3f}, TEX_PANEL, wl, 0.5f, 'x', 4.0f, 2.0f, 2.41f);  // CutCorridor
    add_wall_gap(f, R_A, (Vector3){0,1.5f,4.15f},  (Vector3){14.6f,3,0.3f}, TEX_PANEL, wl, 0.5f, 'x', 0.0f, 2.0f, 2.21f);  // CutEntrance
    add_box(f, R_A, (Vector3){7.15f,1.5f,0},  (Vector3){0.3f,3,8.6f}, TEX_PANEL, wl, 0.5f, true);                          // WallE solid
    add_wall_gap(f, R_A, (Vector3){-7.15f,1.5f,0}, (Vector3){0.3f,3,8.6f}, TEX_PANEL, wl, 0.5f, 'z', 0.0f, 1.0f, 2.21f);   // CutStorage
    add_box(f, R_A, (Vector3){-3.5f,1.5f,0},  (Vector3){0.5f,3,0.5f}, TEX_PANEL, wl, 0.5f, true);   // Pillar1
    add_box(f, R_A, (Vector3){3.5f,1.5f,0},   (Vector3){0.5f,3,0.5f}, TEX_PANEL, wl, 0.5f, true);   // Pillar2
    add_box(f, R_A, (Vector3){0,0.5f,-2},     (Vector3){2.4f,1,0.8f}, TEX_PANEL, desk, 0.5f, true); // DeskBase
    add_surf(f, R_A, (Vector3){0,1.05f,-2},   (Vector3){2.6f,0.1f,1}, TEX_PANEL, desk, 0.5f);       // DeskTop

    // -- Corridor (R_B) --
    add_surf(f, R_B, (Vector3){4,-0.15f,-8.3f}, (Vector3){2.6f,0.3f,8}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_B, (Vector3){4,3.15f,-8.3f},  (Vector3){2.6f,0.3f,8}, TEX_CEILING, cl, 0.5f);
    add_wall_gap(f, R_B, (Vector3){2.85f,1.5f,-8.3f}, (Vector3){0.3f,3,8}, TEX_PANEL, wl, 0.5f, 'z', -8.0f, 1.0f, 2.21f);  // CutOffice
    add_box(f, R_B, (Vector3){5.15f,1.5f,-8.3f}, (Vector3){0.3f,3,8}, TEX_PANEL, wl, 0.5f, true);                          // WallE
    add_wall_gap(f, R_B, (Vector3){4,1.5f,-12.45f}, (Vector3){3.2f,3,0.3f}, TEX_PANEL, wl, 0.5f, 'x', 4.0f, 1.4f, 2.41f);  // WallEnd / CutElevator

    // -- Office (R_C) --
    add_surf(f, R_C, (Vector3){0.7f,-0.15f,-8}, (Vector3){4,0.3f,3.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_C, (Vector3){0.7f,3.15f,-8},  (Vector3){4,0.3f,3.6f}, TEX_CEILING, cl, 0.5f);
    add_box(f, R_C, (Vector3){0.7f,1.5f,-9.65f}, (Vector3){4,3,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_C, (Vector3){0.7f,1.5f,-6.35f}, (Vector3){4,3,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_C, (Vector3){-1.15f,1.5f,-8},   (Vector3){0.3f,3,3.6f}, TEX_PANEL, wl, 0.5f, true);

    // -- Storage (R_D) --
    add_surf(f, R_D, (Vector3){-9.05f,-0.15f,0}, (Vector3){3.5f,0.3f,3.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_D, (Vector3){-9.05f,3.15f,0},  (Vector3){3.5f,0.3f,3.6f}, TEX_CEILING, cl, 0.5f);
    add_box(f, R_D, (Vector3){-9.05f,1.5f,-1.65f}, (Vector3){3.5f,3,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_D, (Vector3){-9.05f,1.5f,1.65f},  (Vector3){3.5f,3,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_D, (Vector3){-10.65f,1.5f,0},     (Vector3){0.3f,3,3.6f}, TEX_PANEL, wl, 0.5f, true);

    // -- Doors --
    Door *exL = add_door(f, R_A, "ExitDoorL", (Vector3){-0.5f,0,4.15f}, 0.0f);
    Door *exR = add_door(f, R_A, "ExitDoorR", (Vector3){0.5f,0,4.15f}, 0.0f);
    for (Door *d = exL; ; d = exR) {
        d->locked = true;
        snprintf(d->prompt, sizeof d->prompt, "Exit");
        snprintf(d->lockedText, sizeof d->lockedText, "It's locked. The building closed hours ago.");
        if (d == exR) break;
    }
    add_door(f, R_D, "StorageDoor", (Vector3){-7.15f,0,0}, PI*0.5f);
    add_door(f, R_B, "OfficeDoor",  (Vector3){2.85f,0,-8}, PI*0.5f);

    add_elevator(f, (Vector3){4,0,-12.45f});
    add_note(f, R_A, "DeskNote", (Vector3){0.4f,1.11f,-1.85f}, 15.0f*DEG2RAD, LOBBY_NOTE);

    // -- Lights (order matters for the ending tweaks) --
    add_light(f, R_A, (Vector3){-3,2.8f,1},   L, 1.5f, 8.0f);  // [0] LobbyLight1
    add_light(f, R_A, (Vector3){3,2.8f,-1},   L, 1.5f, 8.0f);  // [1] LobbyLight2
    add_light(f, R_B, (Vector3){4,2.8f,-6.5f}, L, 1.2f, 5.0f); // [2] CorridorLight1
    add_light(f, R_B, (Vector3){4,2.8f,-10.5f},L, 1.2f, 5.0f); // [3] CorridorLight2
    add_light(f, R_C, (Vector3){0.7f,2.8f,-8}, L, 1.0f, 6.0f); // [4] OfficeLight
    add_light(f, R_D, (Vector3){-9,2.8f,0}, c01(1,0.95f,0.85f), 0.7f, 5.0f); // [5] StorageLight

    if (ending) {
        // The wrong final lobby: lit sickly green/dim, one tube flickering, one
        // nearly dark; exit unlocked but it loops back; the note is the last.
        for (int i=0;i<f->lightCount;i++) {
            Light *l = &f->lights[i];
            l->color = c01(0.72f,0.92f,0.74f);
            l->baseEnergy *= 0.85f; l->energy = l->baseEnergy; l->renderEnergy = l->baseEnergy;
            if (i == 1) { l->flicker = true; l->flickerIntensity = 1.3f; l->flNextBurst = 1.0f; }
            else if (i == f->lightCount-1) { l->baseEnergy *= 0.25f; l->energy = l->baseEnergy; l->renderEnergy = l->baseEnergy; }
        }
        for (int i=0;i<f->doorCount;i++)
            if (strncmp(f->doors[i].name,"ExitDoor",8)==0) f->doors[i].locked = false;
        for (int i=0;i<f->noteCount;i++)
            if (strcmp(f->notes[i].name,"DeskNote")==0) {
                snprintf(f->notes[i].text, sizeof f->notes[i].text, "%s", FINAL_NOTE);
                f->notes[i].isFinal = true;
            }
        // Front doorway trigger volume; loops the player back to the elevator.
        f->hasFalseExit = true;
        Vector3 fc = {0,1.0f,3.9f}, fs = {2.0f,2.5f,1.4f};
        f->falseExitArea = (Box){ Vector3Subtract(fc, Vector3Scale(fs,0.5f)), Vector3Add(fc, Vector3Scale(fs,0.5f)) };
        gs.falseExitPos = (Vector3){4, 0, -11.0f};
        gs.falseExitYaw = PI;          // facing into the lobby (south, +Z)
        gs.falseExitSet = true;
    }
}

// ===========================================================================
//  SUBLEVEL  (the recurring descent floor)
// ===========================================================================
static const char *MAINT_NOTE =
    "MAINTENANCE\n\n"
    "Ticket #4471 - corridor lights, sublevel\n"
    "Status: CLOSED (no fault found)\n"
    "Reopened.\n"
    "Status: CLOSED (no fault found)\n"
    "Reopened.\n"
    "Status: CLOSED (no fault found)\n"
    "Reopened.";

void build_sublevel(Floor *f) {
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "Floor_Sublevel");
    snprintf(f->surface, sizeof f->surface, "concrete");
    snprintf(f->space, sizeof f->space, "corridor");
    f->spawn = (Vector3){0, 0, -9};

    Color fl = c01(0.95f,0.95f,0.97f), wl = c01(0.96f,0.96f,0.98f), cl = c01(1,1,1);
    Color L = c01(0.92f,1.0f,0.95f);

    // -- Corridor (R_A) --
    add_surf(f, R_A, (Vector3){0,-0.15f,-3}, (Vector3){2.6f,0.3f,14.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_A, (Vector3){0,2.85f,-3},  (Vector3){2.6f,0.3f,14.6f}, TEX_CEILING, cl, 0.5f);
    add_wall_gap(f, R_A, (Vector3){-1.15f,1.35f,-3}, (Vector3){0.3f,2.7f,14.6f}, TEX_PANEL, wl, 0.5f, 'z', -3.5f, 1.0f, 2.21f); // CutBreak
    // East wall has two openings (OfficeA, OfficeB): build as three solid runs.
    add_box(f, R_A, (Vector3){1.15f,1.35f,-9.05f}, (Vector3){0.3f,2.7f,2.5f}, TEX_PANEL, wl, 0.5f, true); // z -10.3..-6.5  (top to OfficeA gap)
    add_box(f, R_A, (Vector3){1.15f,1.35f,-3.5f},  (Vector3){0.3f,2.7f,4.0f}, TEX_PANEL, wl, 0.5f, true); // z -5.5..-1.5  (between gaps)
    add_box(f, R_A, (Vector3){1.15f,1.35f,1.9f},   (Vector3){0.3f,2.7f,4.8f}, TEX_PANEL, wl, 0.5f, true); // z -0.5..4.3   (OfficeB gap to S end)
    add_box(f, R_A, (Vector3){1.15f,2.46f,-6},     (Vector3){0.3f,0.48f,1.0f}, TEX_PANEL, wl, 0.5f, true);  // OfficeA lintel
    add_box(f, R_A, (Vector3){1.15f,2.46f,-1},     (Vector3){0.3f,0.48f,1.0f}, TEX_PANEL, wl, 0.5f, true);  // OfficeB lintel
    add_wall_gap(f, R_A, (Vector3){0,1.35f,-10.15f}, (Vector3){2.6f,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, 'x', 0.0f, 1.4f, 2.41f);   // CutElevator
    add_box(f, R_A, (Vector3){0,1.35f,4.15f}, (Vector3){2.6f,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true); // WallS dead end

    // -- OfficeA (R_B) --
    add_surf(f, R_B, (Vector3){3.3f,-0.15f,-6}, (Vector3){4,0.3f,4.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_B, (Vector3){3.3f,2.85f,-6},  (Vector3){4,0.3f,4.6f}, TEX_CEILING, cl, 0.5f);
    add_box(f, R_B, (Vector3){3.3f,1.35f,-8.15f}, (Vector3){4,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_B, (Vector3){3.3f,1.35f,-3.85f}, (Vector3){4,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_B, (Vector3){5.15f,1.35f,-6},    (Vector3){0.3f,2.7f,4.6f}, TEX_PANEL, wl, 0.5f, true);

    // -- OfficeB (R_C) --
    add_surf(f, R_C, (Vector3){3.3f,-0.15f,-1}, (Vector3){4,0.3f,4.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_C, (Vector3){3.3f,2.85f,-1},  (Vector3){4,0.3f,4.6f}, TEX_CEILING, cl, 0.5f);
    add_box(f, R_C, (Vector3){3.3f,1.35f,-3.15f}, (Vector3){4,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_C, (Vector3){3.3f,1.35f,1.15f},  (Vector3){4,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_C, (Vector3){5.15f,1.35f,-1},    (Vector3){0.3f,2.7f,4.6f}, TEX_PANEL, wl, 0.5f, true);

    // -- BreakRoom (R_D) --
    add_surf(f, R_D, (Vector3){-3.3f,-0.15f,-3.5f}, (Vector3){4,0.3f,5.6f}, TEX_CHECKER, fl, 0.5f);
    add_surf(f, R_D, (Vector3){-3.3f,2.85f,-3.5f},  (Vector3){4,0.3f,5.6f}, TEX_CEILING, cl, 0.5f);
    add_box(f, R_D, (Vector3){-3.3f,1.35f,-6.15f}, (Vector3){4,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_D, (Vector3){-3.3f,1.35f,-0.85f}, (Vector3){4,2.7f,0.3f}, TEX_PANEL, wl, 0.5f, true);
    add_box(f, R_D, (Vector3){-5.15f,1.35f,-3.5f}, (Vector3){0.3f,2.7f,5.6f}, TEX_PANEL, wl, 0.5f, true);

    // -- Doors --
    add_door(f, R_A, "OfficeADoor", (Vector3){1.15f,0,-6}, -PI*0.5f);
    add_door(f, R_A, "OfficeBDoor", (Vector3){1.15f,0,-1}, -PI*0.5f);
    add_door(f, R_A, "BreakDoor",   (Vector3){-1.15f,0,-3.5f}, PI*0.5f);

    add_elevator(f, (Vector3){0,0,-10.15f});

    // -- Movable greybox props (anomaly_movable) --
    add_prop(f, R_A, (Vector3){0,0.275f,3.5f}, 0.0f);
    add_prop(f, R_D, (Vector3){-4.3f,0.275f,-5.3f}, 0.0f);
    add_prop(f, R_D, (Vector3){-3.7f,0.275f,-5.5f}, 20.0f*DEG2RAD);
    add_prop(f, R_B, (Vector3){4.5f,0.275f,-7.5f}, 0.0f);

    add_note(f, R_A, "MaintNote", (Vector3){0.05f,0.56f,3.45f}, -20.0f*DEG2RAD, MAINT_NOTE);

    // -- Lights --
    add_light(f, R_A, (Vector3){0,2.5f,-8.5f}, L, 1.1f, 5.0f);
    add_light(f, R_A, (Vector3){0,2.5f,-3},    L, 1.1f, 5.0f);
    add_light(f, R_A, (Vector3){0,2.5f,2},     L, 1.1f, 5.0f);
    add_light(f, R_B, (Vector3){3.3f,2.5f,-6}, L, 1.0f, 5.0f);
    add_light(f, R_C, (Vector3){3.3f,2.5f,-1}, L, 1.0f, 5.0f);
    add_light(f, R_D, (Vector3){-3.3f,2.5f,-3.5f}, c01(1,0.97f,0.88f), 0.9f, 5.0f);
}
