#include "image_fx.h"

#include <stdio.h>
#include <stdlib.h>

struct ImageWorkspace {
    int width;
    int height;
};

static char G_SIGNATURE[120];

int64_t imagefx_checksum(const uint8_t* pixels, size_t len) {
    int64_t checksum = 0;
    size_t index = 0;
    while (index < len) {
        checksum = (checksum * 131ll) + (int64_t)pixels[index] + (int64_t)(index % 17u);
        checksum %= 9000000000000000000ll;
        index += 1;
    }
    return checksum;
}

void imagefx_halo_rgba(uint8_t* pixels, size_t len, int accent) {
    size_t index = 0;
    const uint8_t accent_u8 = (uint8_t)(accent & 0xff);
    while (index + 3 < len) {
        const uint8_t r = pixels[index + 0];
        const uint8_t g = pixels[index + 1];
        const uint8_t b = pixels[index + 2];

        pixels[index + 0] = (uint8_t)(((unsigned int)r / 2u) + ((unsigned int)(255u - b) / 3u) + (accent_u8 / 3u));
        pixels[index + 1] = (uint8_t)(((unsigned int)g / 2u) + ((unsigned int)(255u - r) / 4u) + 18u);
        pixels[index + 2] = (uint8_t)(((unsigned int)b / 2u) + ((unsigned int)(255u - g) / 2u) + (accent_u8 / 5u));
        pixels[index + 3] = 255u;
        index += 4;
    }
}

const char* imagefx_signature(int width, int height, int64_t checksum, int accent) {
    snprintf(
        G_SIGNATURE,
        sizeof(G_SIGNATURE),
        "imagefx:%dx%d:%lld:%d",
        width,
        height,
        (long long)checksum,
        accent
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
