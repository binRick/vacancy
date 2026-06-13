// The loop / anomaly engine (loop_controller.gd + scripts/anomalies/*). A
// data-driven registry of "wrongnesses"; on floor load the controller applies a
// depth-weighted, deterministic-per-(depth,visits,room) selection. Adding a new
// anomaly is one entry in ANOMALIES plus its apply().
#include "game.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// --- shared selection helpers ---
static int lights_unflickered(Floor *f, int *out) {
    int n = 0;
    for (int i=0;i<f->lightCount;i++)
        if (f->lights[i].energy > 0.0f && !f->lights[i].flicker) out[n++] = i;
    return n;
}
static int lights_lit(Floor *f, int *out) {
    int n = 0;
    for (int i=0;i<f->lightCount;i++) if (f->lights[i].energy > 0.0f) out[n++] = i;
    return n;
}

// --- the anomalies ---
static bool an_flicker(Floor *f, int depth, Rng *r) {
    int cand[MAX_FLOOR_LIGHTS]; int n = lights_unflickered(f, cand);
    if (n == 0) return false;
    Light *l = &f->lights[cand[rng_u(r) % n]];
    l->flicker = true;
    l->flickerIntensity = 1.0f + depth*0.25f;
    l->flNextBurst = rng_range(r, 0.5f, 2.0f);
    return true;
}
static bool an_light_out(Floor *f, int depth, Rng *r) {
    (void)depth;
    int cand[MAX_FLOOR_LIGHTS]; int n = lights_unflickered(f, cand);
    if (n == 0) return false;
    Light *l = &f->lights[cand[rng_u(r) % n]];
    l->energy = 0.0f; l->renderEnergy = 0.0f;
    return true;
}
static bool an_door_ajar(Floor *f, int depth, Rng *r) {
    (void)depth;
    int cand[MAX_DOORS]; int n = 0;
    for (int i=0;i<f->doorCount;i++)
        if (!f->doors[i].locked && !f->doors[i].isOpen) cand[n++] = i;
    if (n == 0) return false;
    door_set_open_instant(&f->doors[cand[rng_u(r) % n]], true);
    return true;
}
static bool an_prop_shift(Floor *f, int depth, Rng *r) {
    (void)depth;
    if (f->propCount == 0) return false;
    Prop *p = &f->props[rng_u(r) % f->propCount];
    float ang = rng_range(r, 0.0f, 2.0f*PI);
    float mag = rng_range(r, 0.3f, 0.8f);
    p->pos.x += cosf(ang)*mag;
    p->pos.z += sinf(ang)*mag;
    float twist = rng_range(r, 15.0f, 60.0f)*DEG2RAD;
    p->yaw += (rng_f(r) < 0.5f) ? twist : -twist;
    return true;
}
static bool an_light_hue(Floor *f, int depth, Rng *r) {
    (void)depth;
    int cand[MAX_FLOOR_LIGHTS]; int n = lights_lit(f, cand);
    if (n == 0) return false;
    Light *l = &f->lights[cand[rng_u(r) % n]];
    Vector3 cur = { l->color.r/255.0f, l->color.g/255.0f, l->color.b/255.0f };
    Vector3 tgt = { 0.65f, 1.0f, 0.68f };
    Vector3 mix = Vector3Lerp(cur, tgt, 0.45f);
    l->color = (Color){ (unsigned char)(mix.x*255), (unsigned char)(mix.y*255), (unsigned char)(mix.z*255), 255 };
    return true;
}

typedef struct { const char *id; int minDepth; float weight; bool (*apply)(Floor*,int,Rng*); } AnomalyDef;
static const AnomalyDef ANOMALIES[] = {
    { "light_flicker", 1, 2.0f, an_flicker },
    { "light_out",     2, 1.5f, an_light_out },
    { "door_ajar",     1, 1.5f, an_door_ajar },
    { "prop_shift",    1, 1.2f, an_prop_shift },
    { "light_hue",     3, 1.0f, an_light_hue },
};
#define ANOMALY_COUNT ((int)(sizeof ANOMALIES / sizeof ANOMALIES[0]))

void loop_apply(Floor *f, int depth) {
    if (depth <= 0) return;
    int visits = gs_room_visits(f->name);
    char key[80];
    snprintf(key, sizeof key, "vacancy:%d:%d:%s", depth, visits, f->name);
    Rng r; rng_seed(&r, vac_hash(key));
    rng_f(&r);   // discard: first draw after seeding is biased high (as in GDScript)

    int budget = depth + 1; budget /= 2;
    if (budget < 1) budget = 1; if (budget > 4) budget = 4;

    int pool[ANOMALY_COUNT]; int poolN = 0;
    for (int i=0;i<ANOMALY_COUNT;i++) if (depth >= ANOMALIES[i].minDepth) pool[poolN++] = i;

    char applied[160]; applied[0] = 0; int appliedN = 0;
    while (budget > 0 && poolN > 0) {
        float total = 0;
        for (int i=0;i<poolN;i++) total += ANOMALIES[pool[i]].weight;
        float roll = rng_f(&r)*total;
        int pick = poolN-1;
        for (int i=0;i<poolN;i++) { roll -= ANOMALIES[pool[i]].weight; if (roll <= 0) { pick = i; break; } }
        int def = pool[pick];
        pool[pick] = pool[--poolN];   // erase from pool
        if (ANOMALIES[def].apply(f, depth, &r)) {
            gs_emit_anomaly(ANOMALIES[def].id);
            if (appliedN) strncat(applied, ",", sizeof applied - strlen(applied) - 1);
            strncat(applied, "\"", sizeof applied - strlen(applied) - 1);
            strncat(applied, ANOMALIES[def].id, sizeof applied - strlen(applied) - 1);
            strncat(applied, "\"", sizeof applied - strlen(applied) - 1);
            appliedN++;
            budget--;
        }
    }
    char buf[220];
    snprintf(buf, sizeof buf, "\"depth\":%d,\"applied\":[%s]", depth, applied);
    telemetry_event("anomalies", buf);
}
