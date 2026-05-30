#include "cuda_visual_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_last_gpu_hash = 0;
static uint64_t g_last_cpu_hash = 0;
static int g_last_mismatch_count = 0;

static uint32_t load_u32_le(const unsigned char* bytes) {
    return ((uint32_t)bytes[0]) |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static void store_u32_le(unsigned char* bytes, uint32_t value) {
    bytes[0] = (unsigned char)(value & 255u);
    bytes[1] = (unsigned char)((value >> 8u) & 255u);
    bytes[2] = (unsigned char)((value >> 16u) & 255u);
    bytes[3] = (unsigned char)((value >> 24u) & 255u);
}

static uint64_t fnv1a64(const unsigned char* bytes, size_t length) {
    uint64_t hash = 1469598103934665603ull;
    size_t index;
    for (index = 0; index < length; index += 1u) {
        hash ^= (uint64_t)bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

static uint32_t clamp_index(int value, int limit) {
    if (value < 0) {
        return 0u;
    }
    if (value >= limit) {
        return (uint32_t)(limit - 1);
    }
    return (uint32_t)value;
}

static uint32_t compute_field_value(uint32_t x, uint32_t y, uint32_t seed, uint32_t tone) {
    uint32_t base = (x * 374761393u) + (y * 668265263u) + (seed * 2246822519u);
    uint32_t lane = base ^ (base >> 13u);
    uint32_t ripple = ((x ^ y) + (tone * 17u)) * 2654435761u;
    return (lane ^ ripple) & 255u;
}

static uint32_t pack_rgba(uint32_t red, uint32_t green, uint32_t blue) {
    return (red & 255u) | ((green & 255u) << 8u) | ((blue & 255u) << 16u) | (255u << 24u);
}

static void compute_reference_image(
    int width,
    int height,
    uint32_t seed,
    uint32_t tone,
    uint32_t* out_pixels
) {
    uint32_t* field;
    uint32_t* blur;
    int x;
    int y;
    size_t pixel_count;

    pixel_count = (size_t)width * (size_t)height;
    field = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    blur = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (field == NULL || blur == NULL || out_pixels == NULL) {
        free(field);
        free(blur);
        return;
    }

    for (y = 0; y < height; y += 1) {
        for (x = 0; x < width; x += 1) {
            size_t index = (size_t)y * (size_t)width + (size_t)x;
            field[index] = compute_field_value((uint32_t)x, (uint32_t)y, seed, tone);
        }
    }

    for (y = 0; y < height; y += 1) {
        for (x = 0; x < width; x += 1) {
            uint32_t left_x = clamp_index(x - 1, width);
            uint32_t right_x = clamp_index(x + 1, width);
            uint32_t top_y = clamp_index(y - 1, height);
            uint32_t bottom_y = clamp_index(y + 1, height);
            uint32_t center = field[(size_t)y * (size_t)width + (size_t)x];
            uint32_t left = field[(size_t)y * (size_t)width + (size_t)left_x];
            uint32_t right = field[(size_t)y * (size_t)width + (size_t)right_x];
            uint32_t top = field[(size_t)top_y * (size_t)width + (size_t)x];
            uint32_t bottom = field[(size_t)bottom_y * (size_t)width + (size_t)x];
            blur[(size_t)y * (size_t)width + (size_t)x] =
                (center + left + right + top + bottom) / 5u;
        }
    }

    for (y = 0; y < height; y += 1) {
        for (x = 0; x < width; x += 1) {
            size_t index = (size_t)y * (size_t)width + (size_t)x;
            uint32_t base = field[index];
            uint32_t glow = blur[index];
            uint32_t red = (base + (glow >> 1u) + (tone * 3u)) & 255u;
            uint32_t green = ((base >> 1u) + glow + (tone * 5u)) & 255u;
            uint32_t blue = ((base * 3u) + (glow * 2u) + (tone * 7u)) & 255u;
            out_pixels[index] = pack_rgba(red, green, blue);
        }
    }

    free(field);
    free(blur);
}

static int read_file_bytes(const char* path, unsigned char** out_bytes, size_t* out_length) {
    FILE* file = NULL;
    long length = 0;
    unsigned char* bytes = NULL;

    if (out_bytes == NULL || out_length == NULL || path == NULL || path[0] == '\0') {
        return 0;
    }
    *out_bytes = NULL;
    *out_length = 0u;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    bytes = (unsigned char*)malloc((size_t)length);
    if (bytes == NULL) {
        fclose(file);
        return 0;
    }
    if (fread(bytes, 1u, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return 0;
    }
    fclose(file);
    *out_bytes = bytes;
    *out_length = (size_t)length;
    return 1;
}

static int write_bmp_from_pixels(
    const char* path,
    int width,
    int height,
    const uint32_t* pixels
) {
    unsigned char file_header[14];
    unsigned char info_header[40];
    FILE* file = NULL;
    int stride;
    int image_bytes;
    int y;

    if (path == NULL || path[0] == '\0' || pixels == NULL || width <= 0 || height <= 0) {
        return 0;
    }

    stride = ((width * 3) + 3) & ~3;
    image_bytes = stride * height;

    memset(file_header, 0, sizeof(file_header));
    memset(info_header, 0, sizeof(info_header));
    file_header[0] = 'B';
    file_header[1] = 'M';
    store_u32_le(file_header + 2, (uint32_t)(14 + 40 + image_bytes));
    store_u32_le(file_header + 10, 54u);
    store_u32_le(info_header + 0, 40u);
    store_u32_le(info_header + 4, (uint32_t)width);
    store_u32_le(info_header + 8, (uint32_t)height);
    info_header[12] = 1;
    info_header[14] = 24;
    store_u32_le(info_header + 20, (uint32_t)image_bytes);

    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    fwrite(file_header, 1u, sizeof(file_header), file);
    fwrite(info_header, 1u, sizeof(info_header), file);

    for (y = height - 1; y >= 0; y -= 1) {
        int x;
        for (x = 0; x < width; x += 1) {
            uint32_t pixel = pixels[(size_t)y * (size_t)width + (size_t)x];
            unsigned char bgr[3];
            bgr[0] = (unsigned char)((pixel >> 16u) & 255u);
            bgr[1] = (unsigned char)((pixel >> 8u) & 255u);
            bgr[2] = (unsigned char)(pixel & 255u);
            fwrite(bgr, 1u, sizeof(bgr), file);
        }
        for (x = width * 3; x < stride; x += 1) {
            fputc(0, file);
        }
    }

    fclose(file);
    return 1;
}

static void write_report(
    const char* report_path,
    int width,
    int height,
    int seed,
    int tone,
    int mismatch_count,
    uint64_t gpu_hash,
    uint64_t cpu_hash,
    int exact_match
) {
    FILE* file = fopen(report_path, "wb");
    if (file == NULL) {
        return;
    }
    fprintf(file, "status=%s\n", exact_match ? "exact" : "mismatch");
    fprintf(file, "width=%d\n", width);
    fprintf(file, "height=%d\n", height);
    fprintf(file, "seed=%d\n", seed);
    fprintf(file, "tone=%d\n", tone);
    fprintf(file, "pixels=%d\n", width * height);
    fprintf(file, "mismatch_count=%d\n", mismatch_count);
    fprintf(file, "gpu_hash=%llu\n", (unsigned long long)gpu_hash);
    fprintf(file, "cpu_hash=%llu\n", (unsigned long long)cpu_hash);
    fclose(file);
}

int cuda_visual_native_probe(void) {
    return 1;
}

int cuda_visual_verify_and_render(
    const char* gpu_payload_path,
    const char* report_path,
    const char* gpu_bmp_path,
    const char* cpu_bmp_path,
    const char* diff_bmp_path,
    int width,
    int height,
    int seed,
    int tone
) {
    unsigned char* gpu_bytes = NULL;
    size_t gpu_length = 0u;
    uint32_t* gpu_pixels = NULL;
    uint32_t* cpu_pixels = NULL;
    uint32_t* diff_pixels = NULL;
    size_t pixel_count;
    size_t byte_count;
    size_t index;

    g_last_gpu_hash = 0;
    g_last_cpu_hash = 0;
    g_last_mismatch_count = 0;

    if (width <= 0 || height <= 0 || gpu_payload_path == NULL || report_path == NULL ||
        gpu_bmp_path == NULL || cpu_bmp_path == NULL || diff_bmp_path == NULL) {
        return -1;
    }

    if (!read_file_bytes(gpu_payload_path, &gpu_bytes, &gpu_length)) {
        return -2;
    }

    pixel_count = (size_t)width * (size_t)height;
    byte_count = pixel_count * sizeof(uint32_t);
    if (gpu_length != byte_count) {
        free(gpu_bytes);
        return -3;
    }

    gpu_pixels = (uint32_t*)malloc(byte_count);
    cpu_pixels = (uint32_t*)malloc(byte_count);
    diff_pixels = (uint32_t*)malloc(byte_count);
    if (gpu_pixels == NULL || cpu_pixels == NULL || diff_pixels == NULL) {
        free(gpu_bytes);
        free(gpu_pixels);
        free(cpu_pixels);
        free(diff_pixels);
        return -4;
    }

    for (index = 0u; index < pixel_count; index += 1u) {
        gpu_pixels[index] = load_u32_le(gpu_bytes + (index * 4u));
    }
    compute_reference_image(width, height, (uint32_t)seed, (uint32_t)tone, cpu_pixels);

    for (index = 0u; index < pixel_count; index += 1u) {
        if (gpu_pixels[index] == cpu_pixels[index]) {
            diff_pixels[index] = pack_rgba(0u, 0u, 0u);
        } else {
            uint32_t gpu = gpu_pixels[index];
            uint32_t cpu = cpu_pixels[index];
            uint32_t dr = ((gpu & 255u) > (cpu & 255u))
                ? ((gpu & 255u) - (cpu & 255u))
                : ((cpu & 255u) - (gpu & 255u));
            uint32_t dg = (((gpu >> 8u) & 255u) > ((cpu >> 8u) & 255u))
                ? (((gpu >> 8u) & 255u) - ((cpu >> 8u) & 255u))
                : (((cpu >> 8u) & 255u) - ((gpu >> 8u) & 255u));
            uint32_t db = (((gpu >> 16u) & 255u) > ((cpu >> 16u) & 255u))
                ? (((gpu >> 16u) & 255u) - ((cpu >> 16u) & 255u))
                : (((cpu >> 16u) & 255u) - ((gpu >> 16u) & 255u));
            diff_pixels[index] = pack_rgba(
                dr == 0u ? 32u : dr,
                dg == 0u ? 12u : dg,
                db == 0u ? 160u : db
            );
            g_last_mismatch_count += 1;
        }
    }

    g_last_gpu_hash = fnv1a64((const unsigned char*)gpu_pixels, byte_count);
    g_last_cpu_hash = fnv1a64((const unsigned char*)cpu_pixels, byte_count);

    if (!write_bmp_from_pixels(gpu_bmp_path, width, height, gpu_pixels) ||
        !write_bmp_from_pixels(cpu_bmp_path, width, height, cpu_pixels) ||
        !write_bmp_from_pixels(diff_bmp_path, width, height, diff_pixels)) {
        free(gpu_bytes);
        free(gpu_pixels);
        free(cpu_pixels);
        free(diff_pixels);
        return -5;
    }

    write_report(
        report_path,
        width,
        height,
        seed,
        tone,
        g_last_mismatch_count,
        g_last_gpu_hash,
        g_last_cpu_hash,
        g_last_mismatch_count == 0
    );

    free(gpu_bytes);
    free(gpu_pixels);
    free(cpu_pixels);
    free(diff_pixels);
    return g_last_mismatch_count == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    int status;
    if (argc != 10) {
        fprintf(stderr, "usage: cuda_visual_verify <gpu_payload> <report> <gpu_bmp> <cpu_bmp> <diff_bmp> <width> <height> <seed> <tone>\n");
        return 64;
    }

    status = cuda_visual_verify_and_render(
        argv[1],
        argv[2],
        argv[3],
        argv[4],
        argv[5],
        atoi(argv[6]),
        atoi(argv[7]),
        atoi(argv[8]),
        atoi(argv[9])
    );

    fprintf(stdout, "status=%d mismatch_count=%d gpu_hash=%llu cpu_hash=%llu\n",
        status,
        g_last_mismatch_count,
        (unsigned long long)g_last_gpu_hash,
        (unsigned long long)g_last_cpu_hash);
    return status;
}
