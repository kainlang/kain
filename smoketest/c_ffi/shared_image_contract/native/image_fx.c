#include "image_fx.h"

#include <stdio.h>
#include <stdlib.h>

struct ImageWorkspace {
    int width;
    int height;
};

static char G_SIGNATURE[96];

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

void imagefx_invert_rgba(uint8_t* pixels, size_t len) {
    size_t index = 0;
    while (index + 3 < len) {
        pixels[index] = (uint8_t)(255 - pixels[index]);
        pixels[index + 1] = (uint8_t)(255 - pixels[index + 1]);
        pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]);
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

ImageWorkspace* imagefx_workspace_create(int width, int height) {
    ImageWorkspace* workspace = (ImageWorkspace*)malloc(sizeof(ImageWorkspace));
    if (!workspace) {
        return NULL;
    }
    workspace->width = width;
    workspace->height = height;
    return workspace;
}

int imagefx_workspace_area(ImageWorkspace* workspace) {
    if (!workspace) {
        return 0;
    }
    return workspace->width * workspace->height;
}

void imagefx_workspace_destroy(ImageWorkspace* workspace) {
    if (workspace) {
        free(workspace);
    }
}
