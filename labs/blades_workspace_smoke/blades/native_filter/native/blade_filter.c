#include "blade_filter.h"

#include <stdio.h>

static char BLADE_FILTER_SIGNATURE[128];

int64_t blade_filter_checksum(const uint8_t* pixels, size_t len) {
    int64_t checksum = 17;
    size_t index = 0;
    while (index < len) {
        checksum = (checksum * 131 + (int64_t)pixels[index] + (int64_t)index) % 1000000007;
        index += 1;
    }
    return checksum;
}

void blade_filter_apply_rgba(uint8_t* pixels, size_t len, int accent) {
    size_t index = 0;
    while (index + 3 < len) {
        pixels[index + 0] = (uint8_t)((pixels[index + 0] + accent) % 255);
        pixels[index + 1] = (uint8_t)((pixels[index + 1] + (accent / 2)) % 255);
        pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]);
        pixels[index + 3] = 255;
        index += 4;
    }
}

const char* blade_filter_signature(int width, int height, int64_t checksum) {
    snprintf(
        BLADE_FILTER_SIGNATURE,
        sizeof(BLADE_FILTER_SIGNATURE),
        "blade-filter:%dx%d:%lld",
        width,
        height,
        (long long)checksum
    );
    return BLADE_FILTER_SIGNATURE;
}
