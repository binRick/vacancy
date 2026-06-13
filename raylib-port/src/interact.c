// Interaction: a camera-centre ray finds the nearest usable thing within 2.5m
// and the player gets an "[E] <prompt>" (player.gd._update_interact_target +
// the interactable raycast). E activates it.
#include "game.h"
#include <string.h>
#include <stdio.h>

#define REACH 2.5f

static BoundingBox box_around(Vector3 c, float hx, float hy, float hz) {
    return (BoundingBox){ (Vector3){c.x-hx,c.y-hy,c.z-hz}, (Vector3){c.x+hx,c.y+hy,c.z+hz} };
}

static Vector3 door_center(const Door *d) {
    Matrix m = door_panel_matrix(d);
    return (Vector3){ m.m12, m.m13, m.m14 };
}

void interact_update(Floor *f, Camera3D cam) {
    Ray ray = { cam.position, Vector3Normalize(Vector3Subtract(cam.target, cam.position)) };
    float best = REACH;
    int type = TARGET_NONE, index = -1;
    const char *prompt = "";

    for (int i=0;i<f->doorCount;i++) {
        Vector3 c = door_center(&f->doors[i]);
        RayCollision rc = GetRayCollisionBox(ray, box_around(c, 0.55f, 1.1f, 0.55f));
        if (rc.hit && rc.distance < best) { best=rc.distance; type=TARGET_DOOR; index=i; prompt=f->doors[i].prompt; }
    }
    for (int i=0;i<f->noteCount;i++) {
        Vector3 c = Vector3Add(f->notes[i].pos, (Vector3){0,0.05f,0});
        RayCollision rc = GetRayCollisionBox(ray, box_around(c, 0.25f, 0.2f, 0.25f));
        if (rc.hit && rc.distance < best) { best=rc.distance; type=TARGET_NOTE; index=i; prompt="Read"; }
    }
    if (f->hasElevator) {
        Elevator *e = &f->elevator;
        RayCollision rc = GetRayCollisionBox(ray, box_around(e->callBtnPos, 0.18f,0.25f,0.18f));
        if (rc.hit && rc.distance < best) { best=rc.distance; type=TARGET_ELEV_CALL; index=0; prompt="Call elevator"; }
        rc = GetRayCollisionBox(ray, box_around(e->insideBtnPos, 0.16f,0.22f,0.18f));
        if (rc.hit && rc.distance < best) { best=rc.distance; type=TARGET_ELEV_DESCEND; index=0; prompt="Descend"; }
    }

    player.targetType = type;
    player.targetIndex = index;
    snprintf(player.targetPrompt, sizeof player.targetPrompt, "%s", prompt);
}

void interact_activate(Floor *f) {
    switch (player.targetType) {
    case TARGET_DOOR:
        door_interact(&f->doors[player.targetIndex]);
        break;
    case TARGET_NOTE: {
        Note *n = &f->notes[player.targetIndex];
        char buf[96]; snprintf(buf, sizeof buf, "\"note\":\"%s\",\"final\":%s", n->name, n->isFinal?"true":"false");
        telemetry_event("note", buf);
        gs_open_note(n->text, n->isFinal);
        break;
    }
    case TARGET_ELEV_CALL:
        elevator_open_doors(&f->elevator);
        break;
    case TARGET_ELEV_DESCEND:
        fm_request_descent();
        break;
    default: break;
    }
}
