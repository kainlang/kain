#include "beacon_math.h"

#include <stdio.h>

static char G_BUFFER[64];
static unsigned char G_BYTES[8] = {3, 1, 4, 1, 5, 9, 2, 6};

int beacon_add(int a, int b) {
    return a + b;
}

_Bool beacon_is_even(int value) {
    return (value % 2) == 0;
}

double beacon_scale(int value, double factor) {
    return ((double)value) * factor;
}

const char* beacon_label(int id) {
    snprintf(G_BUFFER, sizeof(G_BUFFER), "beacon-%d", id);
    return G_BUFFER;
}

const unsigned char* beacon_payload_bytes(int id, int* byte_count) {
    if (byte_count != NULL) {
        *byte_count = (id % 8) + 1;
    }
    return G_BYTES;
}

void beacon_fill_buffer(float* values, int count) {
    if (values == NULL || count <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        values[i] = (float)(i + 1) * 1.25f;
    }
}
