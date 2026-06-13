// Main bootstrap (main.gd + Main.tscn). Owns the window, the frame loop, the
// native-res UI (interact prompt, captions, note overlay, fade, end title), the
// false-exit + ending beats, and the dev flags / self-tests.
#include "game.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ---- dev-flag state -------------------------------------------------------
static bool   opt_selftest, opt_endtest, opt_demo;
static char   opt_screenshot[256];
static char   opt_record[256];    // dir to dump frames into (with --demo)
static int    opt_quit_after;     // quit after N rendered frames (benchmarking)
static int    s_recIndex;
static float  s_runtime;          // seconds since boot
static int    s_frame;

// ---- ending / false-exit sequences ---------------------------------------
enum { END_NONE, END_FADE, END_HOLD, END_TITLE, END_WAIT, END_QUIT };
static int s_end; static float s_endT;
enum { FEX_NONE, FEX_OUT, FEX_HOLD, FEX_IN, FEX_COOLDOWN };
static int s_fex; static float s_fexT;

static Door *find_door(const char *name) {
    for (int i=0;i<F->doorCount;i++) if (strcmp(F->doors[i].name,name)==0) return &F->doors[i];
    return NULL;
}
static Note *find_note(const char *name) {
    for (int i=0;i<F->noteCount;i++) if (strcmp(F->notes[i].name,name)==0) return &F->notes[i];
    return NULL;
}

static void close_note(void) {
    ui.noteOpen = false; g_world_paused = false;
    telemetry_event("note_closed", NULL);
    if (gs.lastNoteFinal && s_end == END_NONE && !gs_get_flag("ended")) {
        gs_set_flag("ended", true);
        telemetry_event("ending", NULL);
        audio_end_fade(3.0f);
        s_end = END_FADE; s_endT = 0.0f;
    }
}

static void open_note_from(Note *n) {
    char buf[96]; snprintf(buf, sizeof buf, "\"note\":\"%s\",\"final\":%s", n->name, n->isFinal?"true":"false");
    telemetry_event("note", buf);
    gs_open_note(n->text, n->isFinal);
}

// ---- UI drawing -----------------------------------------------------------
static void draw_centered(const char *text, int cx, int y, int size, Color col) {
    int w = MeasureText(text, size);
    DrawText(text, cx - w/2, y, size, col);
}

static void draw_note_overlay(void) {
    DrawRectangle(0, 0, WINDOW_W, WINDOW_H, (Color){0,0,0,200});
    // multiline, centred block
    int size = 26;
    int lh = size + 8;
    int lines = 1; for (const char *p = ui.noteText; *p; p++) if (*p=='\n') lines++;
    int total = lines*lh;
    int y = WINDOW_H/2 - total/2;
    char buf[512]; snprintf(buf, sizeof buf, "%s", ui.noteText);
    char *line = buf, *nl;
    while (line) {
        nl = strchr(line, '\n');
        if (nl) *nl = 0;
        draw_centered(line, WINDOW_W/2, y, size, (Color){219,219,204,255});
        y += lh;
        line = nl ? nl+1 : NULL;
    }
    draw_centered("[E] put it back", WINDOW_W/2, WINDOW_H - 70, 20, (Color){140,140,132,255});
}

static void draw_ui(void) {
    if (player.targetType != TARGET_NONE && !ui.noteOpen && !g_world_paused) {
        char buf[64]; snprintf(buf, sizeof buf, "[E] %s", player.targetPrompt);
        draw_centered(buf, WINDOW_W/2, WINDOW_H - 110, 26, (Color){230,230,222,255});
    }
    if (ui.captionTimer > 0.0f)
        draw_centered(ui.caption, WINDOW_W/2, WINDOW_H - 180, 24, (Color){199,199,189,255});
    if (ui.noteOpen) draw_note_overlay();
    if (ui.fadeAlpha > 0.0f)
        DrawRectangle(0, 0, WINDOW_W, WINDOW_H, (Color){0,0,0,(unsigned char)(Clamp(ui.fadeAlpha,0,1)*255)});
    if (ui.endTitleAlpha > 0.0f)
        draw_centered("vacancy", WINDOW_W/2, WINDOW_H/2 - 24, 48, (Color){204,204,199,(unsigned char)(ui.endTitleAlpha*255)});
}

// ---- sequences ------------------------------------------------------------
static void update_ending(float dt) {
    if (s_end == END_NONE) return;
    s_endT += dt;
    switch (s_end) {
    case END_FADE:  ui.fadeAlpha = Clamp(s_endT/3.0f, 0, 1); if (s_endT>=3.0f){s_end=END_HOLD;s_endT=0;} break;
    case END_HOLD:  if (s_endT>=2.5f){s_end=END_TITLE;s_endT=0;} break;
    case END_TITLE: ui.endTitleAlpha = Clamp(s_endT/2.5f,0,1)*0.45f; if (s_endT>=2.5f){s_end=END_WAIT;s_endT=0;} break;
    case END_WAIT:  if (s_endT>=3.5f){s_end=END_QUIT;} break;
    default: break;
    }
}

static void start_false_exit(void) {
    telemetry_event("false_exit", NULL);
    s_fex = FEX_OUT; s_fexT = 0.0f;
}
static void update_false_exit(float dt) {
    if (s_fex == FEX_NONE) return;
    s_fexT += dt;
    switch (s_fex) {
    case FEX_OUT: ui.fadeAlpha = Clamp(s_fexT/0.4f,0,1);
        if (s_fexT>=0.4f) {
            if (gs.falseExitSet) { player.pos = gs.falseExitPos; player.yaw = gs.falseExitYaw; player.velocity=(Vector3){0,0,0}; }
            s_fex = FEX_HOLD; s_fexT = 0;
        } break;
    case FEX_HOLD: if (s_fexT>=0.35f){s_fex=FEX_IN;s_fexT=0;} break;
    case FEX_IN: ui.fadeAlpha = 1.0f - Clamp(s_fexT/0.7f,0,1);
        if (s_fexT>=0.7f){ui.fadeAlpha=0;s_fex=FEX_COOLDOWN;s_fexT=0;} break;
    case FEX_COOLDOWN: if (s_fexT>=2.5f){s_fex=FEX_NONE;} break;
    default: break;
    }
}

static bool player_in_box(Box b) {
    return player.pos.x>=b.min.x && player.pos.x<=b.max.x &&
           player.pos.z>=b.min.z && player.pos.z<=b.max.z;
}

// ---- self-test scripts ----------------------------------------------------
static int s_step;
static void run_selftest(float t) {
    Elevator *e = F->hasElevator ? &F->elevator : NULL;
    if (s_step==0 && t>=0.5f) {
        Door *d; if ((d=find_door("StorageDoor"))) door_interact(d);
        if ((d=find_door("ExitDoorL"))) door_interact(d);
        Note *n = find_note("DeskNote"); if (n) open_note_from(n);
        s_step=1;
    } else if (s_step==1 && t>=1.2f) {
        close_note();
        if (e) elevator_open_doors(e);
        s_step=2;
    } else if (s_step==2 && t>=3.0f) {
        if (e) { player.pos = Vector3Add(e->pos,(Vector3){0,0,-1.0f}); fm_request_descent(); }
        s_step=3;
    } else if (s_step==3 && t>=11.0f) {
        if (F->hasElevator) fm_request_descent();
        s_step=4;
    } else if (s_step==4 && t>=19.0f) {
        s_end = END_QUIT;
    }
}
static void run_endtest(float t) {
    if (s_step==0 && t>=0.5f) { player.pos=(Vector3){0,0,3.9f}; s_step=1; }
    else if (s_step==1 && t>=1.8f) { Note *n=find_note("DeskNote"); if (n) open_note_from(n); s_step=2; }
    else if (s_step==2 && t>=2.5f) { close_note(); s_step=3; }
    else if (s_step==3 && t>=16.0f) { s_end = END_QUIT; }
}

// ---- scripted demo walkthrough (for --record video capture) ---------------
static int   d_step;
static float d_timer, d_lookBase;
static bool  d_called, d_descended;

static float angle_lerp(float a, float target, float step) {
    float d = target - a;
    while (d > PI) d -= 2*PI; while (d < -PI) d += 2*PI;
    if (d > step) d = step; if (d < -step) d = -step;
    return a + d;
}
static bool walk_to(Vector3 t, float dt) {
    float dx = t.x - player.pos.x, dz = t.z - player.pos.z;
    float dist = sqrtf(dx*dx + dz*dz);
    if (dist < 0.35f) { g_scripted_move = (Vector2){0,0}; return true; }
    player.yaw = angle_lerp(player.yaw, atan2f(dx, -dz), 3.5f*dt);
    g_scripted_move = (Vector2){0,-1};
    return false;
}
static bool face_to(float yaw, float dt) {
    player.yaw = angle_lerp(player.yaw, yaw, 2.5f*dt);
    g_scripted_move = (Vector2){0,0};
    float d = yaw - player.yaw; while (d > PI) d -= 2*PI; while (d < -PI) d += 2*PI;
    return fabsf(d) < 0.04f;
}
#define DADV() do { d_step++; d_timer = 0; } while (0)

// A hands-off first-person tour: look around the lobby, walk to the elevator,
// ride down, and step out into the degraded sublevel.
static void demo_update(float dt) {
    g_scripted_input = true;
    d_timer += dt;
    switch (d_step) {
    case 0:
        g_scripted_move = (Vector2){0,0};
        player.yaw = d_lookBase + 0.5f*sinf(d_timer*1.3f);
        if (d_timer > 3.0f) DADV();
        break;
    case 1: if (walk_to((Vector3){2.6f,0,0.8f},  dt)) DADV(); break;
    case 2: if (walk_to((Vector3){4.0f,0,-3.2f},  dt)) DADV(); break;
    case 3: if (walk_to((Vector3){4.0f,0,-8.0f},  dt)) DADV(); break;
    case 4: if (walk_to((Vector3){4.0f,0,-10.7f}, dt)) DADV(); break;
    case 5:
        face_to(0.0f, dt);
        if (d_timer > 0.5f && !d_called) { elevator_open_doors(&F->elevator); d_called = true; }
        if (d_timer > 2.4f) DADV();
        break;
    case 6: if (walk_to((Vector3){4.0f,0,-13.0f}, dt)) DADV(); break;
    case 7: if (face_to(PI, dt) && d_timer > 0.7f) DADV(); break;
    case 8:
        g_scripted_move = (Vector2){0,0};
        if (d_timer > 0.3f && !d_descended) { fm_request_descent(); d_descended = true; }
        if (d_timer > 5.8f) DADV();
        break;
    case 9:  if (walk_to((Vector3){0,0,-8.0f}, dt)) DADV(); break;
    case 10: if (walk_to((Vector3){0,0,-3.5f}, dt)) { d_lookBase = player.yaw; DADV(); } break;
    case 11:
        g_scripted_move = (Vector2){0,0};
        player.yaw = d_lookBase + 0.55f*sinf(d_timer*1.2f);
        if (d_timer > 3.4f) DADV();
        break;
    default:
        s_end = END_QUIT;
        break;
    }
}

static void record_frame(void) {
    Image img = LoadImageFromScreen();
    ImageResize(&img, 640, 480);
    char path[300];
    snprintf(path, sizeof path, "%s/f%05d.png", opt_record, s_recIndex++);
    ExportImage(img, path);
    UnloadImage(img);
}

// ---- screenshot -----------------------------------------------------------
static void capture_and_quit(void) {
    Image scr = LoadImageFromScreen();
    ExportImage(scr, opt_screenshot);
    UnloadImage(scr);
    char raw[300]; snprintf(raw, sizeof raw, "%s", opt_screenshot);
    char *dot = strstr(raw, ".png"); if (dot) snprintf(dot, 9, "_raw.png"); else strncat(raw, "_raw.png", sizeof raw - strlen(raw) - 1);
    Image rawimg = LoadImageFromTexture(render_world_rt().texture);
    ImageFlipVertical(&rawimg);
    ExportImage(rawimg, raw);
    UnloadImage(rawimg);
    s_end = END_QUIT;
}

int main(int argc, char **argv) {
    const char *telem = NULL;
    int depthArg = 0; bool subFlag = false, endFlag = false;
    bool poseSet = false; Vector3 pose = {0}; float poseYaw = 0;

    for (int i=1;i<argc;i++) {
        if (strncmp(argv[i],"--telemetry-log=",16)==0) telem = argv[i]+16;
        else if (strncmp(argv[i],"--depth=",8)==0) depthArg = atoi(argv[i]+8);
        else if (strcmp(argv[i],"--sublevel")==0) subFlag = true;
        else if (strcmp(argv[i],"--ending")==0) endFlag = true;
        else if (strcmp(argv[i],"--selftest")==0) opt_selftest = true;
        else if (strcmp(argv[i],"--endtest")==0) opt_endtest = true;
        else if (strcmp(argv[i],"--demo")==0) opt_demo = true;
        else if (strncmp(argv[i],"--record=",9)==0) snprintf(opt_record,sizeof opt_record,"%s",argv[i]+9);
        else if (strncmp(argv[i],"--quit-after=",13)==0) opt_quit_after = atoi(argv[i]+13);
        else if (strncmp(argv[i],"--screenshot=",13)==0) snprintf(opt_screenshot,sizeof opt_screenshot,"%s",argv[i]+13);
        else if (strncmp(argv[i],"--pose=",7)==0) {
            float x,z,y; if (sscanf(argv[i]+7,"%f,%f,%f",&x,&z,&y)==3){ pose=(Vector3){x,0,z}; poseYaw=y*DEG2RAD; poseSet=true; }
        }
    }
    if (!telem) telem = getenv("VACANCY_TELEMETRY_LOG");

    SetConfigFlags(FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(WINDOW_W, WINDOW_H, "Vacancy");
    SetExitKey(KEY_NULL);   // we own Esc (release cursor / close note), not window-close
    SetTargetFPS(60);
    InitAudioDevice();

    telemetry_init(telem);
    world_init_textures();
    render_init();
    audio_init();
    fm_init();

    // dev flags (order mirrors main.gd; --pose applied last so it can frame
    // whichever floor a --sublevel/--ending swap produced)
    if (depthArg > 0) {
        gs.descentDepth = depthArg;
        render_set_vhs_intensity(Clamp(0.18f + 0.08f*depthArg, 0.0f, 1.0f));
        audio_on_depth_changed(depthArg);
    }
    if (subFlag) fm_dev_swap_to_sublevel();
    if (endFlag || opt_endtest) fm_dev_jump_to_ending();   // endtest exercises the wrong final lobby
    if (poseSet) { player.pos = pose; player.yaw = poseYaw; }

    DisableCursor();

    while (!WindowShouldClose() && s_end != END_QUIT) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        if (opt_record[0]) dt = 1.0f/60.0f;   // fixed timestep so capture speed is exact
        s_runtime += dt; s_frame++;

        bool scripted = opt_selftest || opt_endtest || opt_demo;

        // -- input (skip while scripted) --
        if (!scripted) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (ui.noteOpen) close_note();
                else if (IsCursorHidden()) EnableCursor();
            }
            if (IsKeyPressed(KEY_E)) {
                if (ui.noteOpen) close_note();
                else if (!g_world_paused && s_end==END_NONE) interact_activate(F);
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !IsCursorHidden() && !ui.noteOpen)
                DisableCursor();
        }

        bool allow = !scripted && !g_world_paused && IsCursorHidden() && s_end==END_NONE && s_frame > 3;

        if (!g_world_paused) {
            if (opt_demo) demo_update(dt);
            player_update(F, dt, allow);
            entities_update_floor(F, dt);
            fm_update(dt);
            // false-exit trigger (final lobby only)
            if (F->hasFalseExit && s_fex==FEX_NONE && s_end==END_NONE && player_in_box(F->falseExitArea))
                start_false_exit();
        }

        Camera3D cam = player_camera();
        if (!g_world_paused) interact_update(F, cam);

        update_false_exit(dt);
        update_ending(dt);
        if (ui.captionTimer > 0) ui.captionTimer -= dt;

        if (opt_selftest) run_selftest(s_runtime);
        if (opt_endtest) run_endtest(s_runtime);

        audio_update(dt);
        telemetry_update(dt, g_world_paused, F->name);

        // -- render --
        render_world(F, cam);
        BeginDrawing();
        ClearBackground(BLACK);
        render_post_to_screen();
        draw_ui();
        EndDrawing();

        if (opt_screenshot[0] && s_frame == 40) capture_and_quit();
        if (opt_record[0] && (s_frame % 2 == 0)) record_frame();
        if (opt_quit_after > 0 && s_frame >= opt_quit_after) s_end = END_QUIT;
    }

    telemetry_shutdown();
    audio_shutdown();
    render_shutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
