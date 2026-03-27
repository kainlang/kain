#include "dcc_suite_ops.h"

#include <stdio.h>

static char G_DCC_SUITE_SIGNATURE[160];

void dcc_suite_apply_sculpt_stamp(uint8_t* pixels, size_t len, int accent) {
    size_t index = 0;
    while (index + 3 < len) {
        pixels[index + 0] = (uint8_t)((pixels[index + 0] + accent) % 255);
        pixels[index + 1] = (uint8_t)((pixels[index + 1] + (accent / 2)) % 255);
        pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]);
        pixels[index + 3] = 255;
        index += 4;
    }
}

const char* dcc_suite_signature(int width, int height, int accent) {
    snprintf(
        G_DCC_SUITE_SIGNATURE,
        sizeof(G_DCC_SUITE_SIGNATURE),
        "dcc-suite:%dx%d:accent=%d",
        width,
        height,
        accent
    );
    return G_DCC_SUITE_SIGNATURE;
}
