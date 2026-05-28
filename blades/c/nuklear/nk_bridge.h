// nk_bridge.h — minimal header exposing Nuklear via bridge trampolines.
// Kain uses `include nk_bridge.h as nb` to avoid the nk_clear duplicate bug.
// The real work happens in nk_bridge.c which compiles Nuklear underneath.

#ifndef NK_BRIDGE_H
#define NK_BRIDGE_H

int nk_bridge_strlen(const char *s);
unsigned int nk_bridge_murmur_hash(const void *key, int len, unsigned int seed);
void nk_bridge_recti(int x, int y, int w, int h, int *out);
void nk_bridge_hsv(int h, int s, int v, int *out);
void nk_bridge_rgb(int r, int g, int b, int *out);

#endif