// Deterministic RNG + string hash. The Godot original seeds a PCG generator
// with hash("vacancy:depth:visits:room") so a recurring room differs from its
// earlier self in a fixed way. We can't bit-match Godot's hash+PCG, but we
// reproduce the *property*: deterministic per (depth, visits, room), and a
// depth-weighted draw. (Exact anomaly picks won't match the Godot build.)
#include "game.h"

// FNV-1a, 64-bit.
uint64_t vac_hash(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

// PCG32 (minimal). Same family Godot uses, different stream.
void rng_seed(Rng *r, uint64_t seed)
{
    r->state = 0;
    r->inc = (seed << 1u) | 1u;
    rng_u(r);
    r->state += seed + 0x9E3779B97F4A7C15ULL;
    rng_u(r);
}

uint32_t rng_u(Rng *r)
{
    uint64_t old = r->state;
    r->state = old*6364136223846793005ULL + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

float rng_f(Rng *r)
{
    return (float)(rng_u(r) >> 8) / (float)(1u << 24);   // [0,1)
}

float rng_range(Rng *r, float lo, float hi)
{
    return lo + (hi - lo)*rng_f(r);
}
