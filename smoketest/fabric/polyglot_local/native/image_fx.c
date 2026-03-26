#include "image_fx.h"

#include <stdio.h>

static char G_SIGNATURE[128];

uint64_t imagefx_checksum(const uint8_t* pixels, size_t len) {
    uint64_t checksum = 1469598103934665603ull;
    size_t index = 0;
    while (index < len) {
        checksum ^= (uint64_t)pixels[index];
        checksum *= 1099511628211ull;
        index += 1;
    }
    return checksum;
}

void imagefx_halo_rgba(uint8_t* pixels, size_t len, int accent) {
    size_t index = 0;
    while (index + 3 < len) {
        pixels[index + 0] = (uint8_t)((pixels[index + 0] + accent) % 255);
        pixels[index + 1] = (uint8_t)((pixels[index + 1] + (accent / 2)) % 255);
        pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]);
        pixels[index + 3] = 255;
        index += 4;
    }
}

const char* imagefx_signature(int width, int height, uint64_t checksum) {
    snprintf(
        G_SIGNATURE,
        sizeof(G_SIGNATURE),
        "imagefx:%dx%d:%llu",
        width,
        height,
        (unsigned long long)checksum
    );
    return G_SIGNATURE;
}
