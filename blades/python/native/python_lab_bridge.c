#include "python_lab_bridge.h"

#include <stdint.h>

static uint32_t python_lab_rotl32(uint32_t value, uint32_t bits) {
    bits &= 31u;
    if (bits == 0u) {
        return value;
    }
    return (value << bits) | (value >> (32u - bits));
}

static uint32_t python_lab_avalanche32(uint32_t value) {
    uint32_t x = value;
    x ^= x >> 16u;
    x *= 2246822519u;
    x ^= x >> 13u;
    x *= 3266489917u;
    x ^= x >> 16u;
    return x;
}

int python_lab_bridge_bias(int value) {
    uint32_t seed = (uint32_t)(value + 29);
    return (int)(python_lab_avalanche32(seed) & 1023u) + 17;
}

int python_lab_bridge_mix(int seed, int salt) {
    uint32_t lane = (uint32_t)seed ^ python_lab_rotl32((uint32_t)(salt + 41), 7u);
    lane ^= 0x9E3779B9u;
    lane = python_lab_avalanche32(lane + python_lab_rotl32((uint32_t)seed, 11u));
    return (int)(lane & 0x7fffffffu);
}

int python_lab_bridge_fold4(int a, int b, int c, int d) {
    uint32_t lane = (uint32_t)(a + 97);
    lane = python_lab_avalanche32(lane ^ python_lab_rotl32((uint32_t)b, 3u));
    lane = python_lab_avalanche32(lane + python_lab_rotl32((uint32_t)c, 9u));
    lane ^= python_lab_rotl32((uint32_t)d, 15u);
    return (int)(python_lab_avalanche32(lane) & 0x7fffffffu);
}

int python_lab_bridge_window_route(int width, int height, int frames, int seed) {
    uint32_t span = (uint32_t)(width * 3 + height * 5 + frames * 7 + seed * 11 + 53);
    span = python_lab_avalanche32(span);
    return (int)((span % 8191u) + 31u);
}
