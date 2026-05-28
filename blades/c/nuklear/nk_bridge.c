// nk_bridge.c — thin C trampoline that compiles Nuklear + exposes fusion wrappers.
// Kain links against this instead of the raw nuklear.h include.
// This bypasses the duplicate @extern bug in the include directive.

#define NK_IMPLEMENTATION
#include "nuklear.h"

// ---- trampolines: rename to avoid the nk_clear collision bug ---------------

int nk_bridge_strlen(const char *s) {
    return (int)nk_strlen(s);
}

unsigned int nk_bridge_murmur_hash(const void *key, int len, unsigned int seed) {
    return nk_murmur_hash(key, len, seed);
}

// nk_bridge_recti: returns raw ints instead of struct nk_rect
// struct nk_rect { float x, y, w, h } packed as 4 ints via scale factor
void nk_bridge_recti(int x, int y, int w, int h, int *out) {
    struct nk_rect r = nk_recti(x, y, w, h);
    out[0] = (int)r.x;
    out[1] = (int)r.y;
    out[2] = (int)r.w;
    out[3] = (int)r.h;
}

// nk_bridge_hsv: returns raw ints instead of struct nk_color
// struct nk_color { nk_byte r, g, b, a }
void nk_bridge_hsv(int h, int s, int v, int *out) {
    struct nk_color c = nk_hsv(h, s, v);
    out[0] = (int)c.r;
    out[1] = (int)c.g;
    out[2] = (int)c.b;
    out[3] = (int)c.a;
}

void nk_bridge_rgb(int r, int g, int b, int *out) {
    struct nk_color c = nk_rgb(r, g, b);
    out[0] = (int)c.r;
    out[1] = (int)c.g;
    out[2] = (int)c.b;
    out[3] = (int)c.a;
}