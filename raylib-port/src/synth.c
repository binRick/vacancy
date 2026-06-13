// Procedural placeholder audio (sound_synth.gd). Everything is synthesized at
// load; structured so real assets can replace call sites cleanly later. Each
// builder returns a raylib Wave (16-bit mono @ AUDIO_RATE) the caller owns.
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef TAUF
#define TAUF 6.28318530717958647692f
#endif

// Tiny xorshift for the noise sources (variation only; no determinism needed).
static uint32_t s_rs = 0x1234567u;
static float nrand(void)   // [-1,1]
{
    s_rs ^= s_rs << 13; s_rs ^= s_rs >> 17; s_rs ^= s_rs << 5;
    return ((float)(s_rs >> 8)/(float)(1u << 24))*2.0f - 1.0f;
}

static Wave make_wave(const float *s, int n)
{
    short *data = (short *)malloc((size_t)n*sizeof(short));
    for (int i = 0; i < n; i++) {
        float v = s[i];
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        data[i] = (short)(v*32767.0f);
    }
    Wave w = { (unsigned)n, AUDIO_RATE, 16, 1, data };
    return w;
}

Wave synth_footstep(const char *surface)
{
    float dur = 0.13f, lp_coef = 0.22f, amp = 0.5f, click = 0.0f, decay = 2.6f;
    if (surface && strcmp(surface, "carpet") == 0) {
        dur = 0.10f; lp_coef = 0.07f; amp = 0.34f; decay = 2.0f;
    } else if (surface && strcmp(surface, "tile") == 0) {
        dur = 0.12f; lp_coef = 0.30f; amp = 0.5f; click = 0.28f; decay = 3.5f;
    } else { // concrete
        dur = 0.13f; lp_coef = 0.22f; amp = 0.5f; decay = 2.6f;
    }
    int n = (int)(AUDIO_RATE*dur);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float lp = 0.0f;
    for (int i = 0; i < n; i++) {
        float frac = (float)i/n;
        float env = powf(1.0f - frac, decay);
        lp += (nrand() - lp)*lp_coef;
        float v = lp;
        if (click > 0.0f) v += nrand()*click*powf(1.0f - frac, 8.0f);
        s[i] = v*env*amp;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_fluorescent_hum(void)
{
    int n = (int)(AUDIO_RATE*2.0f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    for (int i = 0; i < n; i++) {
        float t = (float)i/AUDIO_RATE;
        float v = sinf(TAUF*120.0f*t)*0.5f;
        v += sinf(TAUF*240.0f*t)*0.17f;
        v += sinf(TAUF*360.0f*t)*0.08f;
        v += sinf(TAUF*60.0f*t)*0.05f;
        float trem = 0.86f + 0.14f*sinf(TAUF*6.0f*t);
        s[i] = v*trem*0.16f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_hvac_drone(void)
{
    int n = (int)(AUDIO_RATE*3.0f);
    float *noise = (float *)malloc((size_t)n*sizeof(float));
    uint32_t save = s_rs; s_rs = 0x4711u;
    float lp = 0.0f;
    for (int i = 0; i < n; i++) { lp += (nrand() - lp)*0.02f; noise[i] = lp; }
    s_rs = save;
    float *s = (float *)malloc((size_t)n*sizeof(float));
    int half = n/2;
    for (int i = 0; i < n; i++) {
        float w_a = sinf(PI*i/n);
        float w_b = fabsf(cosf(PI*i/n));
        float rumble = (noise[i]*w_a + noise[(i + half) % n]*w_b)*3.0f;
        float t = (float)i/AUDIO_RATE;
        float sub = sinf(TAUF*42.0f*t)*0.10f + sinf(TAUF*55.0f*t)*0.07f;
        float v = rumble*0.5f + sub;
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        s[i] = v*0.5f;
    }
    free(noise);
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_distant_door(void)
{
    int n = (int)(AUDIO_RATE*0.6f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float lp = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i/AUDIO_RATE;
        float env = expf(-t*7.0f);
        lp += (nrand() - lp)*0.04f;
        float knock = sinf(TAUF*68.0f*t)*expf(-t*16.0f)*0.5f;
        s[i] = (lp*1.6f + knock)*env*0.6f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_metal_groan(void)
{
    int n = (int)(AUDIO_RATE*1.1f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float lp = 0.0f, phase = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i/n;
        float env = sinf(PI*fminf(t*1.3f, 1.0f))*0.8f;
        float freq = 70.0f - 18.0f*t + 5.0f*sinf(t*30.0f);
        phase += freq/AUDIO_RATE;
        float saw = 2.0f*fmodf(phase, 1.0f) - 1.0f;
        lp += (saw - lp)*0.05f;
        s[i] = (lp + lp*nrand()*0.2f)*env*0.5f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_sub_swell(void)
{
    int n = (int)(AUDIO_RATE*1.4f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float phase = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i/n;
        float freq = 32.0f + 30.0f*t;
        phase += freq/AUDIO_RATE;
        s[i] = sinf(TAUF*phase)*sinf(PI*t)*0.6f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_door_creak(void)
{
    int n = (int)(AUDIO_RATE*1.2f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float lp = 0.0f, phase = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i/n;
        float env = sinf(PI*fminf(t*1.25f, 1.0f))*0.8f;
        float freq = 95.0f - 25.0f*t + 8.0f*sinf(t*43.0f);
        phase = fmodf(phase + freq/AUDIO_RATE, 1.0f);
        float saw = 2.0f*phase - 1.0f;
        lp += (saw - lp)*0.08f;
        s[i] = (lp + lp*nrand()*0.35f)*env*0.55f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_locked_rattle(void)
{
    int n = (int)(AUDIO_RATE*0.3f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float lp = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i/AUDIO_RATE;
        float env = 0.0f;
        float starts[2] = {0.0f, 0.13f};
        for (int k = 0; k < 2; k++)
            if (t >= starts[k]) { float e = expf(-(t - starts[k])*45.0f); if (e > env) env = e; }
        lp += (nrand() - lp)*0.3f;
        s[i] = lp*env*0.6f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_elevator_rumble(float seconds)
{
    int n = (int)(AUDIO_RATE*seconds);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    float lp = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i/AUDIO_RATE;
        float fade = fminf(t/0.4f, 1.0f)*fminf((seconds - t)/0.5f, 1.0f);
        lp += (nrand() - lp)*0.06f;
        s[i] = (sinf(TAUF*38.0f*t)*0.35f + lp*1.4f)*fade*0.5f;
    }
    Wave w = make_wave(s, n); free(s); return w;
}

Wave synth_elevator_ding(void)
{
    int n = (int)(AUDIO_RATE*0.9f);
    float *s = (float *)malloc((size_t)n*sizeof(float));
    for (int i = 0; i < n; i++) {
        float t = (float)i/AUDIO_RATE;
        s[i] = (sinf(TAUF*740.0f*t)*0.32f + sinf(TAUF*1480.0f*t)*0.12f)*expf(-t*5.0f);
    }
    Wave w = make_wave(s, n); free(s); return w;
}

// ---- in-memory .wav encode (for looping beds via LoadMusicStreamFromMemory) -
static void put_u32(unsigned char *p, uint32_t v)
{ p[0]=v&0xff; p[1]=(v>>8)&0xff; p[2]=(v>>16)&0xff; p[3]=(v>>24)&0xff; }
static void put_u16(unsigned char *p, uint16_t v) { p[0]=v&0xff; p[1]=(v>>8)&0xff; }

unsigned char *synth_encode_wav(Wave w, int *out_size)
{
    uint32_t data_bytes = w.frameCount*w.channels*(w.sampleSize/8);
    uint32_t total = 44 + data_bytes;
    unsigned char *buf = (unsigned char *)malloc(total);
    memcpy(buf, "RIFF", 4);
    put_u32(buf + 4, 36 + data_bytes);
    memcpy(buf + 8, "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    put_u32(buf + 16, 16);
    put_u16(buf + 20, 1);                                  // PCM
    put_u16(buf + 22, (uint16_t)w.channels);
    put_u32(buf + 24, w.sampleRate);
    put_u32(buf + 28, w.sampleRate*w.channels*(w.sampleSize/8));
    put_u16(buf + 32, (uint16_t)(w.channels*(w.sampleSize/8)));
    put_u16(buf + 34, (uint16_t)w.sampleSize);
    memcpy(buf + 36, "data", 4);
    put_u32(buf + 40, data_bytes);
    memcpy(buf + 44, w.data, data_bytes);
    *out_size = (int)total;
    return buf;
}
