// Doors, elevator doors, flicker fx (door.gd, elevator.gd, fx_flicker.gd).
// Per-frame animation lives in entities_update_floor; the descent state
// machine that drives the elevator is in floor_manager.c.
#include "game.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define ELEV_DOOR_SLIDE 0.7f
#define ELEV_DOOR_SECONDS 1.1f

static Rng s_fx;   // shared cheap rng for flicker bursts

static float ease_io(float t) {   // sine-ish ease in/out (Godot TRANS_SINE)
    return 0.5f - 0.5f*cosf(t*PI);
}

// ---- doors ----------------------------------------------------------------
static Vector3 door_audio_pos(const Door *d) { return Vector3Add(d->pos, (Vector3){0,1.05f,0}); }

void door_set_open_instant(Door *d, bool open) {
    d->isOpen = open;
    snprintf(d->prompt, sizeof d->prompt, open ? "Close" : "Open");
    d->curAngle = open ? d->openAngleDeg*DEG2RAD : 0.0f;
    d->swinging = false;
    d->busy = false;
}

void door_interact(Door *d) {
    if (d->busy) return;
    if (d->locked) {
        audio_play_positional(SND_LOCKED, door_audio_pos(d), -6.0f, 1.0f);
        gs_show_caption(d->lockedText);
        char buf[64]; snprintf(buf, sizeof buf, "\"door\":\"%s\"", d->name);
        telemetry_event("door_locked", buf);
        return;
    }
    d->busy = true;
    d->isOpen = !d->isOpen;
    snprintf(d->prompt, sizeof d->prompt, d->isOpen ? "Close" : "Open");
    audio_play_positional(SND_DOOR_CREAK, door_audio_pos(d), -2.0f, rng_range(&s_fx, 0.92f, 1.08f));
    char buf[80]; snprintf(buf, sizeof buf, "\"door\":\"%s\",\"open\":%s", d->name, d->isOpen?"true":"false");
    telemetry_event("door", buf);
    d->startAngle = d->curAngle;
    d->targetAngle = d->isOpen ? d->openAngleDeg*DEG2RAD : 0.0f;
    d->swingElapsed = 0.0f;
    d->swinging = true;
}

void door_update(Door *d, float dt) {
    if (!d->swinging) return;
    d->swingElapsed += dt;
    float t = Clamp(d->swingElapsed/d->swingSeconds, 0.0f, 1.0f);
    d->curAngle = Lerp(d->startAngle, d->targetAngle, ease_io(t));
    if (t >= 1.0f) { d->swinging = false; d->busy = false; }
}

// World transform of the panel: pos * RotY(baseYaw) * hinge * RotY(swing) * panel.
// (raylib MatrixMultiply applies the first argument first / innermost.)
Matrix door_panel_matrix(const Door *d) {
    Matrix tPanel = MatrixTranslate(0.45f, 1.05f, 0.0f);
    Matrix rSwing = MatrixRotateY(d->curAngle);
    Matrix tHinge = MatrixTranslate(-0.45f, 0.0f, 0.0f);
    Matrix rBase  = MatrixRotateY(d->baseYaw);
    Matrix tPos   = MatrixTranslate(d->pos.x, d->pos.y, d->pos.z);
    return MatrixMultiply(tPanel, MatrixMultiply(rSwing, MatrixMultiply(tHinge, MatrixMultiply(rBase, tPos))));
}

// Closed door blocks the doorway; open (incl. anomaly-ajar) lets you through.
Box door_closed_box(const Door *d) {
    float hx = 0.45f, hz = 0.06f;   // half panel width / depth (+margin)
    float c = fabsf(cosf(d->baseYaw)), s = fabsf(sinf(d->baseYaw));
    float wx = hx*c + hz*s, wz = hx*s + hz*c;
    Vector3 ctr = Vector3Add(d->pos, (Vector3){0,1.05f,0});
    return (Box){ (Vector3){ctr.x-wx, 0, ctr.z-wz}, (Vector3){ctr.x+wx, 2.1f, ctr.z+wz} };
}

// ---- elevator -------------------------------------------------------------
void elevator_open_doors(Elevator *e) {
    if (e->doorsMoving || e->doorsOpen) return;
    e->doorsMoving = true; e->doorsOpen = true;
    e->doorStart = e->doorOffset; e->doorTarget = ELEV_DOOR_SLIDE; e->doorElapsed = 0.0f;
    telemetry_event("elevator_doors", "\"open\":true");
}

void elevator_close_doors(Elevator *e) {
    e->doorsMoving = true; e->doorsOpen = false;
    e->doorStart = e->doorOffset; e->doorTarget = 0.0f; e->doorElapsed = 0.0f;
    telemetry_event("elevator_doors", "\"open\":false");
}

void elevator_ding(Elevator *e) {
    audio_play_positional(SND_ELEV_DING, Vector3Add(e->pos, (Vector3){0,1.5f,-1}), -3.0f, 1.0f);
}

void elevator_update(Elevator *e, float dt) {
    if (!e->doorsMoving) return;
    e->doorElapsed += dt;
    float t = Clamp(e->doorElapsed/ELEV_DOOR_SECONDS, 0.0f, 1.0f);
    e->doorOffset = Lerp(e->doorStart, e->doorTarget, ease_io(t));
    if (t >= 1.0f) e->doorsMoving = false;
}

// ---- flicker fx (fx_flicker.gd) -------------------------------------------
void flicker_update(Light *l, float dt) {
    if (l->flNextBurst <= 0.0f && l->flClock == 0.0f)
        l->flNextBurst = rng_range(&s_fx, 0.5f, 2.0f);
    l->flClock += dt;
    if (l->flClock >= l->flNextBurst) {
        l->flBurstEnd = l->flClock + rng_range(&s_fx, 0.15f, 0.7f);
        l->flNextBurst = l->flClock + rng_range(&s_fx, 1.5f, 6.0f)/fmaxf(l->flickerIntensity, 0.1f);
    }
    float base = l->energy;
    if (l->flClock < l->flBurstEnd)
        l->renderEnergy = base*(0.1f + 0.9f*rng_f(&s_fx)*rng_f(&s_fx));
    else
        l->renderEnergy = base;
}

void entities_update_floor(Floor *f, float dt) {
    for (int i=0;i<f->doorCount;i++) door_update(&f->doors[i], dt);
    if (f->hasElevator) elevator_update(&f->elevator, dt);
    for (int i=0;i<f->lightCount;i++) {
        Light *l = &f->lights[i];
        if (l->flicker) flicker_update(l, dt);
        else l->renderEnergy = l->energy;
    }
}
