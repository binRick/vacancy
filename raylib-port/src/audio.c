// AudioDirector (audio_director.gd): looping fluorescent-hum + HVAC-drone bed,
// a depth/space-scaled algorithmic reverb standing in for Godot's reverb bus,
// sparse anomaly stingers fired a beat late, and silence used as a tool. All
// source audio is procedural (synth.c).
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define POOL 24
#define SCHED 16

static bool s_ready;

// Beds (held in memory for the life of the stream).
static Music s_hum, s_hvac;
static unsigned char *s_humBuf, *s_hvacBuf;

// One-shot pool.
static Sound s_pool[POOL];
static bool s_active[POOL];

// Scheduled stingers.
static struct { float t; int which; float db; bool positional; Vector3 pos; } s_sched[SCHED];

// Depth/space reverb parameters (room 0..1, wet 0..0.6).
static float s_room = 0.42f, s_wet = 0.18f, s_spaceRoom = 0.42f;

// Master + ducking.
static float s_masterDb = 0.0f;
static int   s_fade;            // 0 none, 1 end-fade
static float s_fadeRate, s_fadeTarget;
// silence: state machine
enum { SIL_NONE, SIL_WAIT, SIL_DOWN, SIL_HOLD, SIL_UP };
static int s_sil; static float s_silT, s_silHold;

static float db_lin(float db) { return powf(10.0f, db/20.0f); }

// ---- lightweight Schroeder reverb (per one-shot; Sfx bus stand-in) --------
static Wave reverb_apply(Wave in, float room, float wet) {
    int n = (int)in.frameCount;
    short *src = (short *)in.data;
    if (wet < 0.01f) {   // dry copy
        short *d = malloc((size_t)n*sizeof(short));
        memcpy(d, src, (size_t)n*sizeof(short));
        Wave w = { (unsigned)n, AUDIO_RATE, 16, 1, d }; return w;
    }
    int tail = (int)(AUDIO_RATE*(0.25f + 0.45f*room));
    int m = n + tail;
    float *x = calloc((size_t)m, sizeof(float));
    for (int i=0;i<n;i++) x[i] = src[i]/32768.0f;

    int cd[4] = {1557,1617,1491,1422};
    float scale = 0.75f + 0.65f*room;
    float fb = 0.5f + 0.42f*room;
    float *acc = calloc((size_t)m, sizeof(float));
    for (int c=0;c<4;c++) {
        int d = (int)(cd[c]*scale); if (d < 1) d = 1; if (d >= m) d = m-1;
        float *buf = calloc((size_t)m, sizeof(float));
        for (int i=0;i<m;i++) { float in_s = x[i] + (i>=d ? buf[i-d]*fb : 0.0f); buf[i] = in_s; acc[i] += in_s*0.25f; }
        free(buf);
    }
    int ad[2] = {225,556}; float ag = 0.5f;
    for (int a=0;a<2;a++) {
        int d = ad[a];
        float *buf = calloc((size_t)m, sizeof(float));
        for (int i=0;i<m;i++) {
            float bufout = (i>=d ? buf[i-d] : 0.0f);
            float in_s = acc[i] + (-ag)*bufout;
            buf[i] = in_s; acc[i] = bufout + ag*in_s;
        }
        free(buf);
    }
    short *out = malloc((size_t)m*sizeof(short));
    for (int i=0;i<m;i++) {
        float v = x[i]*(1.0f-wet) + acc[i]*wet;
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        out[i] = (short)(v*32767.0f);
    }
    free(x); free(acc);
    Wave w = { (unsigned)m, AUDIO_RATE, 16, 1, out };
    return w;
}

static int pool_slot(void) {
    for (int i=0;i<POOL;i++) if (!s_active[i]) return i;
    return -1;   // all busy; drop
}

static void play_wave(Wave dry, float db, float pitch, bool positional, Vector3 pos) {
    if (!s_ready) { free(dry.data); return; }
    int slot = pool_slot();
    Wave wet = reverb_apply(dry, s_room, s_wet);
    free(dry.data);
    if (slot < 0) { free(wet.data); return; }
    s_pool[slot] = LoadSoundFromWave(wet);
    free(wet.data);
    float vol = db_lin(db);
    float pan = 0.5f;
    if (positional) {
        Vector3 eye = { player.pos.x, player.eyeHeight, player.pos.z };
        float dist = Vector3Distance(pos, eye);
        vol *= 1.0f/(1.0f + 0.35f*dist);
        Vector3 right = { cosf(player.yaw), 0, sinf(player.yaw) };
        Vector3 to = Vector3Normalize(Vector3Subtract(pos, eye));
        float rd = Vector3DotProduct(to, right);
        pan = Clamp(0.5f - rd*0.4f, 0.05f, 0.95f);
    }
    SetSoundVolume(s_pool[slot], Clamp(vol, 0.0f, 1.0f));
    SetSoundPitch(s_pool[slot], pitch);
    SetSoundPan(s_pool[slot], pan);
    PlaySound(s_pool[slot]);
    s_active[slot] = true;
}

static Wave wave_for(int which) {
    switch (which) {
    case SND_DOOR_CREAK:  return synth_door_creak();
    case SND_LOCKED:      return synth_locked_rattle();
    case SND_ELEV_DING:   return synth_elevator_ding();
    case SND_DISTANT_DOOR:return synth_distant_door();
    case SND_METAL_GROAN: return synth_metal_groan();
    case SND_SUB_SWELL:   return synth_sub_swell();
    case SND_FOOTSTEP_TILE:    return synth_footstep("tile");
    case SND_FOOTSTEP_CARPET:  return synth_footstep("carpet");
    case SND_FOOTSTEP_CONCRETE:return synth_footstep("concrete");
    default: return synth_door_creak();
    }
}

// ---- public ---------------------------------------------------------------
void audio_init(void) {
    int humSize, hvacSize;
    Wave hw = synth_fluorescent_hum();  s_humBuf = synth_encode_wav(hw, &humSize); free(hw.data);
    Wave dw = synth_hvac_drone();       s_hvacBuf = synth_encode_wav(dw, &hvacSize); free(dw.data);
    s_hum  = LoadMusicStreamFromMemory(".wav", s_humBuf, humSize);
    s_hvac = LoadMusicStreamFromMemory(".wav", s_hvacBuf, hvacSize);
    s_hum.looping = true; s_hvac.looping = true;
    SetMusicVolume(s_hum, db_lin(-15.0f));
    SetMusicVolume(s_hvac, db_lin(-19.0f));
    PlayMusicStream(s_hum); PlayMusicStream(s_hvac);
    s_ready = true;
    char buf[32]; snprintf(buf, sizeof buf, "\"buses\":3"); telemetry_event("audio_ready", buf);
}

void audio_set_space(const char *space) {
    s_spaceRoom = (space && strcmp(space,"corridor")==0) ? 0.6f : 0.42f;
    s_room = Clamp(s_spaceRoom + 0.04f*gs.descentDepth, 0.0f, 0.95f);
    s_wet  = Clamp(0.18f + 0.025f*gs.descentDepth, 0.0f, 0.6f);
}

void audio_on_depth_changed(int depth) {
    s_room = Clamp(s_spaceRoom + 0.04f*depth, 0.0f, 0.95f);
    s_wet  = Clamp(0.18f + 0.025f*depth, 0.0f, 0.6f);
    if (s_ready) SetMusicVolume(s_hvac, db_lin(-19.0f + (depth<6?depth:6)*0.7f));   // drone presses in
    if (depth >= 3) {
        Rng r; char k[24]; snprintf(k, sizeof k, "silence:%d", depth);
        rng_seed(&r, vac_hash(k)); rng_f(&r);
        float chance = Clamp(0.35f + 0.06f*(depth-3), 0.0f, 0.7f);
        if (rng_f(&r) < chance && s_sil == SIL_NONE) {
            s_sil = SIL_WAIT; s_silT = rng_range(&r, 2.0f, 5.0f); s_silHold = 2.0f;
        }
    }
}

void audio_on_anomaly(const char *id) {
    int which = -1; float db = 0, lo = 0, hi = 0;
    if (strcmp(id,"door_ajar")==0)  { which=SND_DISTANT_DOOR; db=-7;  lo=0.5f; hi=3.5f; }
    else if (strcmp(id,"prop_shift")==0) { which=SND_METAL_GROAN; db=-13; lo=1.0f; hi=4.0f; }
    else if (strcmp(id,"light_out")==0)  { which=SND_SUB_SWELL;  db=-15; lo=0.0f; hi=1.5f; }
    if (which < 0) return;
    Rng r; rng_seed(&r, vac_hash(id) ^ (uint64_t)(gs.descentDepth*2654435761u));
    for (int i=0;i<SCHED;i++) if (s_sched[i].which == 0 && s_sched[i].t <= 0) {
        s_sched[i].which = which; s_sched[i].db = db; s_sched[i].positional = false;
        s_sched[i].t = rng_range(&r, lo, hi);
        return;
    }
}

void audio_play_positional(int which, Vector3 worldPos, float db, float pitch) {
    play_wave(wave_for(which), db, pitch, true, worldPos);
}

void audio_footstep(const char *surface) {
    int which = SND_FOOTSTEP_CONCRETE;
    if (strcmp(surface,"tile")==0) which = SND_FOOTSTEP_TILE;
    else if (strcmp(surface,"carpet")==0) which = SND_FOOTSTEP_CARPET;
    float db = -14.0f + (float)(GetRandomValue(0,300))/100.0f;
    float pitch = 0.95f + (float)(GetRandomValue(0,100))/1000.0f;
    play_wave(wave_for(which), db, pitch, false, (Vector3){0,0,0});
}

void audio_elevator_rumble(float seconds) {
    Wave w = synth_elevator_rumble(seconds);
    play_wave(w, -6.0f, 1.0f, false, (Vector3){0,0,0});
}

void audio_master_db(float db) { s_masterDb = db; }
float audio_master_volume(void) { return s_masterDb; }

void audio_end_fade(float seconds) {
    telemetry_event("audio_end_fade", NULL);
    s_fade = 1; s_fadeTarget = -60.0f;
    s_fadeRate = (s_masterDb - s_fadeTarget)/fmaxf(seconds, 0.01f);
}

void audio_update(float dt) {
    if (!s_ready) return;
    UpdateMusicStream(s_hum);
    UpdateMusicStream(s_hvac);

    for (int i=0;i<POOL;i++)
        if (s_active[i] && !IsSoundPlaying(s_pool[i])) { UnloadSound(s_pool[i]); s_active[i] = false; }

    for (int i=0;i<SCHED;i++) if (s_sched[i].which != 0) {
        s_sched[i].t -= dt;
        if (s_sched[i].t <= 0) {
            char buf[32]; snprintf(buf, sizeof buf, "\"db\":%.0f", s_sched[i].db);
            telemetry_event("stinger", buf);
            play_wave(wave_for(s_sched[i].which), s_sched[i].db, 1.0f, false, (Vector3){0,0,0});
            s_sched[i].which = 0; s_sched[i].t = 0;
        }
    }

    // silence-as-a-tool ducking
    switch (s_sil) {
    case SIL_WAIT: s_silT -= dt; if (s_silT <= 0) { telemetry_event("audio_silence","\"hold\":2.0"); s_sil = SIL_DOWN; s_silT = 0.12f; } break;
    case SIL_DOWN: s_silT -= dt; s_masterDb = Lerp(-60.0f, 0.0f, Clamp(s_silT/0.12f,0,1)); if (s_silT <= 0) { s_sil = SIL_HOLD; s_silT = s_silHold; s_masterDb = -60.0f; } break;
    case SIL_HOLD: s_silT -= dt; if (s_silT <= 0) { s_sil = SIL_UP; s_silT = 0.6f; } break;
    case SIL_UP:   s_silT -= dt; s_masterDb = Lerp(0.0f, -60.0f, Clamp(s_silT/0.6f,0,1)); if (s_silT <= 0) { s_sil = SIL_NONE; s_masterDb = 0.0f; } break;
    default: break;
    }

    if (s_fade == 1) {
        s_masterDb -= s_fadeRate*dt;
        if (s_masterDb <= s_fadeTarget) { s_masterDb = s_fadeTarget; s_fade = 0; }
    }

    SetMasterVolume(Clamp(db_lin(s_masterDb), 0.0f, 1.0f));
}

void audio_shutdown(void) {
    if (!s_ready) return;
    for (int i=0;i<POOL;i++) if (s_active[i]) { UnloadSound(s_pool[i]); s_active[i] = false; }
    StopMusicStream(s_hum); StopMusicStream(s_hvac);
    UnloadMusicStream(s_hum); UnloadMusicStream(s_hvac);
    free(s_humBuf); free(s_hvacBuf);
    s_ready = false;
}
