// FloorManager (floor_manager.gd). Builds the starting lobby and drives the
// descent: the world is swapped mid-ride behind shut elevator doors, with the
// player keeping their position relative to the cab so the ride feels
// continuous. After END_DEPTH descents the elevator opens onto the wrong final
// lobby (built by build_lobby(..., true)).
#include "game.h"
#include <string.h>
#include <stdio.h>

#define RIDE_SECONDS 2.6f

static Floor s_floors[2];
static int s_cur;

enum { FM_IDLE, FM_CLOSING, FM_RIDING, FM_OPENING };
static int s_state;
static float s_rideT;
static bool s_swapped;

static void set_current(Floor *f) {
    F = f;
    snprintf(gs.currentFloorName, sizeof gs.currentFloorName, "%s", f->name);
    snprintf(gs.currentSurface, sizeof gs.currentSurface, "%s", f->surface);
    audio_set_space(f->space);
}

void fm_init(void) {
    s_cur = 0;
    build_lobby(&s_floors[0], false);
    set_current(&s_floors[0]);
    gs_mark_room_seen(F->name);
    player_init(F->spawn, 0.0f);
    s_state = FM_IDLE;
}

bool fm_is_riding(void) { return s_state != FM_IDLE; }

void fm_request_descent(void) {
    if (!F->hasElevator) return;
    Elevator *e = &F->elevator;
    if (s_state != FM_IDLE || e->doorsMoving || !e->doorsOpen) return;
    char buf[48]; snprintf(buf, sizeof buf, "\"from_depth\":%d", gs.descentDepth);
    telemetry_event("elevator_ride", buf);
    elevator_close_doors(e);
    s_state = FM_CLOSING;
}

static void do_swap(void) {
    int nextDepth = gs.descentDepth + 1;
    Floor *nf = &s_floors[1 - s_cur];
    if (nextDepth >= END_DEPTH) build_lobby(nf, true);
    else build_sublevel(nf);

    Vector3 rel = Vector3Subtract(player.pos, F->elevator.pos);
    floor_unload(F);
    s_cur = 1 - s_cur;
    set_current(nf);
    player.pos = Vector3Add(nf->elevator.pos, rel);

    gs_descend();                       // depth++ -> audio bed + VHS intensity
    gs_mark_room_seen(nf->name);

    if (gs.descentDepth >= END_DEPTH) {
        char buf[48]; snprintf(buf, sizeof buf, "\"depth\":%d", gs.descentDepth);
        telemetry_event("reached_final_lobby", buf);   // ending tweaks already built
    } else {
        loop_apply(nf, gs.descentDepth);
    }
}

void fm_update(float dt) {
    switch (s_state) {
    case FM_CLOSING:
        if (!F->elevator.doorsMoving) {
            audio_elevator_rumble(RIDE_SECONDS);
            s_rideT = 0; s_swapped = false;
            s_state = FM_RIDING;
        }
        break;
    case FM_RIDING:
        s_rideT += dt;
        if (!s_swapped && s_rideT >= RIDE_SECONDS*0.55f) { do_swap(); s_swapped = true; }
        if (s_rideT >= RIDE_SECONDS) {
            elevator_ding(&F->elevator);
            elevator_open_doors(&F->elevator);
            s_state = FM_OPENING;
        }
        break;
    case FM_OPENING:
        if (!F->elevator.doorsMoving) s_state = FM_IDLE;
        break;
    default: break;
    }
}

// ---- dev helpers (--sublevel / --ending / --depth) ------------------------
void fm_dev_swap_to_sublevel(void) {
    Floor *nf = &s_floors[1 - s_cur];
    build_sublevel(nf);
    floor_unload(F);
    s_cur = 1 - s_cur;
    set_current(nf);
    gs_mark_room_seen(nf->name);
    int d = gs.descentDepth > 1 ? gs.descentDepth : 1;
    loop_apply(nf, d);
    player.pos = nf->spawn;
    s_state = FM_IDLE;
}

void fm_dev_jump_to_ending(void) {
    gs.descentDepth = END_DEPTH;
    Floor *nf = &s_floors[1 - s_cur];
    build_lobby(nf, true);
    floor_unload(F);
    s_cur = 1 - s_cur;
    set_current(nf);
    gs_mark_room_seen(nf->name);
    // Stand in the lobby looking north (toward the desk/corridor), clear of the
    // front-doorway trigger near the south wall.
    player.pos = (Vector3){0, 0, 2.0f};
    player.yaw = 0.0f;
    render_set_vhs_intensity(Clamp(0.18f + 0.08f*END_DEPTH, 0.0f, 1.0f));
    audio_on_depth_changed(END_DEPTH);
    s_state = FM_IDLE;
}
