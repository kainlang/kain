#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    KAIN_FRAME_MAX_DEPTH = 8
};

typedef enum KainArenaId {
    KAIN_ARENA_MAIN = 0,
    KAIN_ARENA_SHARED = 1,
    KAIN_ARENA_GPU = 2,
    KAIN_ARENA_SCRATCH = 3,
    KAIN_ARENA_MAX = 4
} KainArenaId;

typedef enum KainMemType {
    KAIN_MEMTYPE_CPU_RO = 0x8,
    KAIN_MEMTYPE_CPU_WB = 0x4,
    KAIN_MEMTYPE_GPU_RO = 0x2,
    KAIN_MEMTYPE_GPU_LOCAL = 0x1,
    KAIN_MEMTYPE_COUNT = 16,
    KAIN_MEMTYPE_DEFAULT = KAIN_MEMTYPE_CPU_WB,
    KAIN_MEMTYPE_DEFAULT_GPU_RO =
        KAIN_MEMTYPE_CPU_RO | KAIN_MEMTYPE_CPU_WB | KAIN_MEMTYPE_GPU_RO | KAIN_MEMTYPE_GPU_LOCAL,
    KAIN_MEMTYPE_DEFAULT_GPU_RW = KAIN_MEMTYPE_GPU_LOCAL,
    KAIN_MEMTYPE_LEGAL_MASK =
        (1u << KAIN_MEMTYPE_DEFAULT) |
        (1u << (KAIN_MEMTYPE_CPU_RO | KAIN_MEMTYPE_CPU_WB)) |
        (1u << (KAIN_MEMTYPE_CPU_WB | KAIN_MEMTYPE_GPU_RO)) |
        (1u << (KAIN_MEMTYPE_CPU_WB | KAIN_MEMTYPE_GPU_LOCAL)) |
        (1u << KAIN_MEMTYPE_DEFAULT_GPU_RO) |
        (1u << KAIN_MEMTYPE_DEFAULT_GPU_RW)
} KainMemType;

typedef struct {
    size_t low_offset;
    size_t high_offset;
} KainFrameMarker;

typedef struct {
    KainFrameMarker markers[KAIN_FRAME_MAX_DEPTH];
    uint8_t depth;
} KainFrameStack;

typedef struct {
    unsigned char* start;
    unsigned char* end;
    unsigned char* low;
    unsigned char* high;
    size_t reserved_bytes;
    uint8_t arena_id;
    uint8_t memtype;
    uint16_t flags;
    KainFrameStack frame;
    atomic_uint lock_word;
} KainArena;

int kain_memtype_is_legal(uint8_t memtype);
size_t kain_align_up_size(size_t value, size_t alignment, int* overflowed);

int kain_arena_init(
    KainArena* arena,
    KainArenaId arena_id,
    void* start,
    size_t size,
    KainMemType memtype
);
void kain_arena_reset(KainArena* arena);
size_t kain_arena_available(const KainArena* arena);
void* kain_arena_alloc_lo(KainArena* arena, size_t size, size_t alignment);
void* kain_arena_alloc_hi(KainArena* arena, size_t size, size_t alignment);
int kain_frame_set_marker(KainArena* arena);
int kain_frame_release_to_last_marker(KainArena* arena);
void kain_frame_release_all(KainArena* arena);

#ifdef __cplusplus
}
#endif

#endif /* ARENA_H */
