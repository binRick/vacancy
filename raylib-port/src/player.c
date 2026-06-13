// First-person walker (player.gd). WASD + mouse look, Shift slow, Ctrl crouch,
// no jump. The game is flat, so movement is purely horizontal with cylinder-
// vs-AABB sliding collision; headbob and footsteps are driven by distance
// travelled so they stay in sync at any speed.
#include "game.h"
#include <math.h>
#include <string.h>

#define WALK_SPEED 3.0f
#define SLOW_SPEED 1.6f
#define CROUCH_SPEED 1.4f
#define ACCEL 12.0f
#define MOUSE_SENS 0.0022f
#define PITCH_LIMIT (85.0f*DEG2RAD)
#define STAND_EYE 1.6f
#define CROUCH_EYE 0.95f
#define STEP_DISTANCE 1.9f
#define BOB_AMP 0.035f
#define BODY_TOP 1.8f

bool g_scripted_input;
Vector2 g_scripted_move;

void player_init(Vector3 pos, float yaw) {
    memset(&player, 0, sizeof player);
    player.pos = pos; player.yaw = yaw; player.pitch = 0.0f;
    player.eyeHeight = STAND_EYE; player.radius = 0.32f;
    player.lastPos = pos;
}

static Vector3 forward_vec(void) {
    float cp = cosf(player.pitch);
    return (Vector3){ sinf(player.yaw)*cp, sinf(player.pitch), -cosf(player.yaw)*cp };
}

Camera3D player_camera(void) {
    Vector3 eye = { player.pos.x, player.eyeHeight + player.camBobY, player.pos.z };
    Camera3D cam = {0};
    cam.position = eye;
    cam.target = Vector3Add(eye, forward_vec());
    cam.up = (Vector3){0,1,0};
    cam.fovy = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

// Push a circle (centre p, radius r) out of an axis-aligned rect in XZ.
static void resolve_box(Vector3 *p, float r, Box b) {
    if (!(b.min.y < BODY_TOP && b.max.y > 0.0f)) return;   // no vertical overlap
    float cx = Clamp(p->x, b.min.x, b.max.x);
    float cz = Clamp(p->z, b.min.z, b.max.z);
    float dx = p->x - cx, dz = p->z - cz;
    float d2 = dx*dx + dz*dz;
    if (dx == 0.0f && dz == 0.0f) {
        // Centre inside the rect: eject along the shallowest edge.
        float left = p->x - b.min.x, right = b.max.x - p->x;
        float back = p->z - b.min.z, front = b.max.z - p->z;
        float m = left; int axis = 0;
        if (right < m) { m = right; axis = 1; }
        if (back  < m) { m = back;  axis = 2; }
        if (front < m) { m = front; axis = 3; }
        if (axis==0) p->x = b.min.x - r;
        else if (axis==1) p->x = b.max.x + r;
        else if (axis==2) p->z = b.min.z - r;
        else p->z = b.max.z + r;
    } else if (d2 < r*r) {
        float d = sqrtf(d2);
        float push = r - d;
        p->x += dx/d*push; p->z += dz/d*push;
    }
}

static void collide(Floor *f, Vector3 *p) {
    float r = player.radius;
    for (int pass=0; pass<3; pass++) {
        for (int i=0;i<f->colliderCount;i++) resolve_box(p, r, f->colliders[i]);
        for (int i=0;i<f->doorCount;i++)
            if (!f->doors[i].isOpen) resolve_box(p, r, door_closed_box(&f->doors[i]));
        for (int i=0;i<f->propCount;i++) {
            Vector3 c = f->props[i].pos;
            Box b = { (Vector3){c.x-0.3f, 0, c.z-0.3f}, (Vector3){c.x+0.3f, 0.55f, c.z+0.3f} };
            resolve_box(p, r, b);
        }
        if (f->hasElevator && f->elevator.doorOffset < 0.35f) {
            Vector3 c = Vector3Add(f->elevator.pos, (Vector3){0,1.15f,0.05f});
            Box b = { (Vector3){c.x-0.75f, 0, c.z-0.1f}, (Vector3){c.x+0.75f, 2.3f, c.z+0.1f} };
            resolve_box(p, r, b);
        }
    }
}

void player_update(Floor *f, float dt, bool allow_input) {
    // -- look --
    if (allow_input && IsCursorHidden()) {
        Vector2 md = GetMouseDelta();
        player.yaw += md.x*MOUSE_SENS;
        player.pitch -= md.y*MOUSE_SENS;
        player.pitch = Clamp(player.pitch, -PITCH_LIMIT, PITCH_LIMIT);
    }

    // -- movement --
    Vector2 in = {0,0};
    if (g_scripted_input) {
        in = g_scripted_move;            // demo/record drives movement
    } else if (allow_input) {
        if (IsKeyDown(KEY_W)) in.y -= 1;
        if (IsKeyDown(KEY_S)) in.y += 1;
        if (IsKeyDown(KEY_A)) in.x -= 1;
        if (IsKeyDown(KEY_D)) in.x += 1;
    }
    if (in.x != 0 || in.y != 0) in = Vector2Normalize(in);
    // local (x=right, -z=forward) -> world by yaw
    Vector3 right = { cosf(player.yaw), 0, sinf(player.yaw) };
    Vector3 fwd   = { sinf(player.yaw), 0, -cosf(player.yaw) };
    Vector3 wish = Vector3Add(Vector3Scale(right, in.x), Vector3Scale(fwd, -in.y));

    bool ctrl = allow_input && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    bool slow = allow_input && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    player.crouching = ctrl;
    float speed = WALK_SPEED;
    if (player.crouching) speed = CROUCH_SPEED;
    else if (slow) speed = SLOW_SPEED;

    float blend = 1.0f - expf(-ACCEL*dt);
    Vector3 target = Vector3Scale(wish, speed);
    player.velocity.x = Lerp(player.velocity.x, target.x, blend);
    player.velocity.z = Lerp(player.velocity.z, target.z, blend);

    Vector3 np = player.pos;
    np.x += player.velocity.x*dt;
    np.z += player.velocity.z*dt;
    collide(f, &np);
    player.pos = np;

    // -- crouch eye height --
    float eyeTarget = player.crouching ? CROUCH_EYE : STAND_EYE;
    player.eyeHeight = Lerp(player.eyeHeight, eyeTarget, 1.0f - expf(-9.0f*dt));

    // -- bob + footsteps (distance based) --
    Vector3 moved = Vector3Subtract(player.pos, player.lastPos);
    player.lastPos = player.pos;
    moved.y = 0;
    float dist = Vector2Length((Vector2){moved.x, moved.z});
    bool walking = dist > 0.0001f;
    if (walking) {
        player.bobPhase = fmodf(player.bobPhase + dist/STEP_DISTANCE, 1.0f);
        player.stepAccum += dist;
        if (player.stepAccum >= STEP_DISTANCE) {
            player.stepAccum -= STEP_DISTANCE;
            player_play_footstep();
        }
    }
    float bob = (walking && !player.crouching) ? sinf(player.bobPhase*2.0f*PI)*BOB_AMP : 0.0f;
    player.camBobY = Lerp(player.camBobY, bob, 1.0f - expf(-10.0f*dt));
}

void player_play_footstep(void) {
    audio_footstep(gs.currentSurface[0] ? gs.currentSurface : "tile");
}
