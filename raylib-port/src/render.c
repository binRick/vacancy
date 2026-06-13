// Render pipeline (Main.tscn post chain + the three shaders). The 3D world is
// drawn into a 320x240 render texture with the PSX material (per-room point
// lights, no shadow maps); a dither pass crunches it at internal res; a VHS/CRT
// pass upscales it to the window. UI is drawn crisp on top by main.c.
#include "game.h"
#include <stdlib.h>

static Shader s_psx, s_dither, s_vhs;
static RenderTexture2D s_world, s_dither_rt;
static Material s_mat;

// psx custom uniform locations
static int u_snap, u_affine, u_ambient, u_lcount, u_lpos, u_lcol, u_len, u_lrng;
// post uniform locations
static int ud_res, uv_intensity, uv_scan, uv_barrel, uv_time, uv_res;

static float s_intensity = 0.18f;

void render_init(void) {
    s_psx    = LoadShader("shaders/psx.vs", "shaders/psx.fs");
    s_dither = LoadShader(0, "shaders/dither.fs");
    s_vhs    = LoadShader(0, "shaders/vhs.fs");

    u_snap   = GetShaderLocation(s_psx, "snapResolution");
    u_affine = GetShaderLocation(s_psx, "affine");
    u_ambient= GetShaderLocation(s_psx, "ambientColor");
    u_lcount = GetShaderLocation(s_psx, "lightCount");
    u_lpos   = GetShaderLocation(s_psx, "lightPos");
    u_lcol   = GetShaderLocation(s_psx, "lightColor");
    u_len    = GetShaderLocation(s_psx, "lightEnergy");
    u_lrng   = GetShaderLocation(s_psx, "lightRange");

    Vector2 snap = { INTERNAL_W, INTERNAL_H };
    float affine = 1.0f;
    Vector3 ambient = { 0.6f*0.35f, 0.62f*0.35f, 0.68f*0.35f };
    SetShaderValue(s_psx, u_snap, &snap, SHADER_UNIFORM_VEC2);
    SetShaderValue(s_psx, u_affine, &affine, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s_psx, u_ambient, &ambient, SHADER_UNIFORM_VEC3);

    ud_res = GetShaderLocation(s_dither, "resolution");
    Vector2 res = { INTERNAL_W, INTERNAL_H };
    SetShaderValue(s_dither, ud_res, &res, SHADER_UNIFORM_VEC2);

    uv_intensity = GetShaderLocation(s_vhs, "intensity");
    uv_scan      = GetShaderLocation(s_vhs, "scanlineStrength");
    uv_barrel    = GetShaderLocation(s_vhs, "barrel");
    uv_time      = GetShaderLocation(s_vhs, "time");
    uv_res       = GetShaderLocation(s_vhs, "resolution");
    float scan = 0.3f, barrel = 0.05f;
    SetShaderValue(s_vhs, uv_scan, &scan, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s_vhs, uv_barrel, &barrel, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s_vhs, uv_res, &res, SHADER_UNIFORM_VEC2);

    s_world     = LoadRenderTexture(INTERNAL_W, INTERNAL_H);
    s_dither_rt = LoadRenderTexture(INTERNAL_W, INTERNAL_H);

    s_mat = LoadMaterialDefault();
    s_mat.shader = s_psx;
}

void render_shutdown(void) {
    UnloadRenderTexture(s_world);
    UnloadRenderTexture(s_dither_rt);
    UnloadShader(s_psx); UnloadShader(s_dither); UnloadShader(s_vhs);
    // s_mat uses our shader + shared textures; just free its maps array
    s_mat.shader = (Shader){0};
    UnloadMaterial(s_mat);
}

void render_set_vhs_intensity(float v) { s_intensity = v; }
RenderTexture2D render_world_rt(void) { return s_world; }

static void bind_room_lights(Floor *f, int room) {
    float pos[MAX_SHADER_LIGHTS*3], col[MAX_SHADER_LIGHTS*3], en[MAX_SHADER_LIGHTS], rng[MAX_SHADER_LIGHTS];
    int n = 0;
    for (int i=0; i<f->lightCount && n<MAX_SHADER_LIGHTS; i++) {
        Light *l = &f->lights[i];
        if (l->room != room || l->renderEnergy <= 0.001f) continue;
        pos[n*3+0]=l->pos.x; pos[n*3+1]=l->pos.y; pos[n*3+2]=l->pos.z;
        col[n*3+0]=l->color.r/255.0f; col[n*3+1]=l->color.g/255.0f; col[n*3+2]=l->color.b/255.0f;
        en[n]=l->renderEnergy; rng[n]=l->range;
        n++;
    }
    SetShaderValue(s_psx, u_lcount, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(s_psx, u_lpos, pos, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(s_psx, u_lcol, col, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(s_psx, u_len, en, SHADER_UNIFORM_FLOAT, n);
        SetShaderValueV(s_psx, u_lrng, rng, SHADER_UNIFORM_FLOAT, n);
    }
}

static void draw_mesh(Mesh m, int tex, Color color, Matrix xform) {
    s_mat.maps[MATERIAL_MAP_DIFFUSE].texture = world_texture(tex);
    s_mat.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(m, s_mat, xform);
}

void render_world(Floor *f, Camera3D cam) {
    // -- 3D world into the internal 320x240 target --
    BeginTextureMode(s_world);
    ClearBackground((Color){5, 5, 6, 255});
    BeginMode3D(cam);
    for (int r = 0; r < MAX_ROOMS; r++) {
        bind_room_lights(f, r);
        for (int i=0;i<f->surfaceCount;i++)
            if (f->surfaces[i].room == r)
                draw_mesh(f->surfaces[i].mesh, f->surfaces[i].tex, f->surfaces[i].color,
                          MatrixTranslate(f->surfaces[i].pos.x, f->surfaces[i].pos.y, f->surfaces[i].pos.z));
        for (int i=0;i<f->doorCount;i++)
            if (f->doors[i].room == r)
                draw_mesh(f->doors[i].mesh, TEX_PANEL, f->doors[i].color, door_panel_matrix(&f->doors[i]));
        for (int i=0;i<f->propCount;i++)
            if (f->props[i].room == r)
                draw_mesh(f->props[i].mesh, TEX_PANEL, f->props[i].color,
                          MatrixMultiply(MatrixRotateY(f->props[i].yaw),
                                         MatrixTranslate(f->props[i].pos.x, f->props[i].pos.y, f->props[i].pos.z)));
        for (int i=0;i<f->noteCount;i++)
            if (f->notes[i].room == r)
                draw_mesh(f->notes[i].mesh, TEX_FLAT, f->notes[i].color,
                          MatrixMultiply(MatrixRotateY(f->notes[i].yaw),
                                         MatrixTranslate(f->notes[i].pos.x, f->notes[i].pos.y, f->notes[i].pos.z)));
        if (r == 7 && f->hasElevator) {
            Elevator *e = &f->elevator;
            Vector3 dl = Vector3Add(e->pos, (Vector3){-0.35f - e->doorOffset, 1.15f, 0.05f});
            Vector3 dr = Vector3Add(e->pos, (Vector3){ 0.35f + e->doorOffset, 1.15f, 0.05f});
            draw_mesh(e->doorMesh, TEX_PANEL, e->doorColor, MatrixTranslate(dl.x, dl.y, dl.z));
            draw_mesh(e->doorMesh, TEX_PANEL, e->doorColor, MatrixTranslate(dr.x, dr.y, dr.z));
            draw_mesh(e->callBtnMesh, TEX_PANEL, e->btnColor, MatrixTranslate(e->callBtnPos.x, e->callBtnPos.y, e->callBtnPos.z));
            draw_mesh(e->insideBtnMesh, TEX_PANEL, e->btnColor, MatrixTranslate(e->insideBtnPos.x, e->insideBtnPos.y, e->insideBtnPos.z));
        }
    }
    EndMode3D();
    EndTextureMode();

    // -- dither crunch at internal res --
    BeginTextureMode(s_dither_rt);
    BeginShaderMode(s_dither);
    DrawTextureRec(s_world.texture, (Rectangle){0, 0, INTERNAL_W, -INTERNAL_H}, (Vector2){0, 0}, WHITE);
    EndShaderMode();
    EndTextureMode();
}

void render_post_to_screen(void) {
    float t = (float)GetTime();
    SetShaderValue(s_vhs, uv_time, &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s_vhs, uv_intensity, &s_intensity, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(s_vhs);
    DrawTexturePro(s_dither_rt.texture,
                   (Rectangle){0, 0, INTERNAL_W, -INTERNAL_H},
                   (Rectangle){0, 0, WINDOW_W, WINDOW_H},
                   (Vector2){0, 0}, 0.0f, WHITE);
    EndShaderMode();
}
