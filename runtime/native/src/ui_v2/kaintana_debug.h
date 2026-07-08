// ============================================================================
//  kaintana_debug.h — Debug and diagnostic utilities for Kaintana
//
//  Provides reusable helpers for framebuffer analysis, structured stats
//  logging, and conditional debug tracing. Works with any backend that
//  exposes a framebuffer pointer — the caller passes the pointer + dims.
//
//  Include this file AFTER including the backend's .c file (e.g.
//  `#include "backends/win32/host_win32.c"` before this header) if you
//  want to use the convenience macros that reference g_pBits directly.
//
//  Usage:
//    #include "kaintana_debug.h"
//    ...
//    kt_debug_dump_fb("frame_001.bin", g_pBits, fb_w, fb_h, stride);
//    kt_debug_print_stats(frame_num, n_visible, cmd_count);
//    KT_DEBUG_LOG("render_frame: %d voxels visible", n_visible);
// ============================================================================
#ifndef KAINTANA_DEBUG_H
#define KAINTANA_DEBUG_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//  FRAMEBUFFER DUMP — write raw pixel data to a .bin file
//
//  The .bin file contains raw BGRA pixel data (4 bytes per pixel) that
//  can be analyzed with a hex editor, ImageMagick (as .rgba), or loaded
//  into Python/PIL for visual inspection.
//
//  Resolution: width x height
//  Format:     raw BGRA (32-bit, bottom-up or top-down as stored)
//  File size:  width * height * 4 bytes
//
//  Returns 0 on success, nonzero on failure.
// ============================================================================
static inline int kt_debug_dump_fb(const char* path,
                                    const void* fb, int w, int h, int stride)
{
    if (!fb || !path || w <= 0 || h <= 0) return -1;
    if (!path[0]) return -1;

    FILE* f = fopen(path, "wb");
    if (!f) return -2;

    // Write raw pixel data — stride-aware in case pitch != width*4
    const uint8_t* src = (const uint8_t*)fb;
    int bpp = 4;
    size_t row_bytes = (size_t)w * (size_t)bpp;
    int real_stride = (stride > 0) ? stride : (w * 4);

    for (int y = 0; y < h; y++) {
        size_t written = fwrite(src + (size_t)y * (size_t)real_stride,
                                1, row_bytes, f);
        if (written != row_bytes) {
            fclose(f);
            return -3;
        }
    }

    fclose(f);
    return 0;
}

// ============================================================================
//  STATS PRINTER — structured timestamped console output
//
//  Prints a single-line frame summary to stdout in the format:
//    [DEBUG frame N] voxels=M cmds=C pixels=WxH fps=F
// ============================================================================
static inline void kt_debug_print_stats(int frame, int voxels, int cmds,
                                         int fb_w, int fb_h, double fps)
{
    printf("[DEBUG frame %d] voxels=%d cmds=%d pixels=%dx%d fps=%.1f\n",
           frame, voxels, cmds, fb_w, fb_h, fps);
}

// ============================================================================
//  CONDITIONAL DEBUG TRACE — only active when DEBUG is defined
//
//  Usage: KT_DEBUG_LOG("some value: %d", value);
//  Output: [DEBUG timestamp] filename:line: some value: 42
// ============================================================================
#ifdef DEBUG
    #define KT_DEBUG_LOG(fmt, ...) do {                                         \
        time_t _kt_now = time(NULL);                                            \
        char _kt_buf[32] = {0};                                                 \
        struct tm* _kt_tm = localtime(&_kt_now);                                \
        if (_kt_tm) strftime(_kt_buf, sizeof(_kt_buf), "%H:%M:%S", _kt_tm);    \
        fprintf(stderr, "[DEBUG %s %s:%d] " fmt "\n",                           \
                _kt_buf, __FILE__, __LINE__, ##__VA_ARGS__);                    \
        fflush(stderr);                                                         \
    } while (0)
#else
    #define KT_DEBUG_LOG(fmt, ...) ((void)0)
#endif

// ============================================================================
//  PIXEL CHECKSUM — fast hash of framebuffer for golden-file comparison
//
//  Returns a simple XOR-checksum of all pixel values (fast, deterministic).
//  Useful for verifying that the framebuffer contents match expectations.
//
//  More sophisticated: count non-background pixels (pixels where color != bg)
// ============================================================================
static inline uint32_t kt_debug_fb_checksum(const void* fb,
                                             int w, int h, int stride)
{
    if (!fb || w <= 0 || h <= 0) return 0;
    const uint8_t* src = (const uint8_t*)fb;
    int real_stride = (stride > 0) ? stride : (w * 4);
    uint32_t sum = 0;

    for (int y = 0; y < h; y++) {
        const uint32_t* row = (const uint32_t*)(src + (size_t)y * (size_t)real_stride);
        for (int x = 0; x < w; x++) {
            sum ^= row[x];
            sum = (sum << 3) | (sum >> 29);  // rotate
        }
    }
    return sum;
}

// ============================================================================
//  NON-BACKGROUND PIXEL COUNT — count pixels that differ from a background color
//
//  Useful for verifying that rendering produced foreground content.
//  bg_color is the expected uint32_t value (same format as framebuffer).
// ============================================================================
static inline int kt_debug_foreground_pixel_count(const void* fb,
                                                   int w, int h, int stride,
                                                   uint32_t bg_color)
{
    if (!fb || w <= 0 || h <= 0) return 0;
    const uint8_t* src = (const uint8_t*)fb;
    int real_stride = (stride > 0) ? stride : (w * 4);
    int count = 0;

    for (int y = 0; y < h; y++) {
        const uint32_t* row = (const uint32_t*)(src + (size_t)y * (size_t)real_stride);
        for (int x = 0; x < w; x++) {
            if (row[x] != bg_color) count++;
        }
    }
    return count;
}

#ifdef __cplusplus
}
#endif

#endif // KAINTANA_DEBUG_H
