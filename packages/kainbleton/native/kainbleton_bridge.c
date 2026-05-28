#include "kainbleton_bridge.h"

static unsigned int kb_rotl32(unsigned int x, int r) {
    return (x << r) | (x >> (32 - r));
}

int kb_signature(int frames, int tracks, int clips, int salt) {
    unsigned int x = 0x9e3779b9u ^ (unsigned int)frames;
    x ^= kb_rotl32((unsigned int)tracks * 0x85ebca6bu, 7);
    x ^= kb_rotl32((unsigned int)clips * 0xc2b2ae35u, 13);
    x ^= kb_rotl32((unsigned int)salt * 0x27d4eb2du, 19);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    return (int)(x & 0x7fffffffu);
}

int kb_meter_color(int track, int frame, int seed) {
    unsigned int x = (unsigned int)(track * 131 + frame * 17 + seed * 257);
    unsigned int r = 80u + (x * 29u % 176u);
    unsigned int g = 60u + (x * 47u % 170u);
    unsigned int b = 40u + (x * 67u % 188u);
    return (int)((r << 16) | (g << 8) | b);
}

const char *kb_label(void) {
    return "kainbleton-native-bridge";
}
