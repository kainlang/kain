#include "modeler_ops.h"

#include <stdio.h>

static char G_MODELER_SIGNATURE[128];

void modeler_stamp_highlight(uint8_t* pixels, size_t len, int accent) {
    size_t index = 0;
    while (index + 3 < len) {
        pixels[index + 0] = (uint8_t)((pixels[index + 0] + accent) % 255);
        pixels[index + 1] = (uint8_t)((pixels[index + 1] + (accent / 2)) % 255);
        pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]);
        pixels[index + 3] = 255;
        index += 4;
    }
}

const char* modeler_signature(int width, int height, int accent) {
    snprintf(
        G_MODELER_SIGNATURE,
        sizeof(G_MODELER_SIGNATURE),
        "modeler:%dx%d:accent=%d",
        width,
        height,
        accent
    );
    return G_MODELER_SIGNATURE;
}
