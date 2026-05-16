#include "../../include/converge.h"

#include "../../include/cpu.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

typedef struct KainConvergeTuneCacheSlot {
    uint64_t key;
    uint64_t eligible_mask;
    uint64_t lane_index;
    uint64_t generation;
} KainConvergeTuneCacheSlot;

static KainConvergeTelemetrySample g_kain_converge_telemetry[KAIN_CONVERGE_TELEMETRY_CAP];
static KainConvergeTuneCacheSlot g_kain_converge_cache[KAIN_CONVERGE_TUNE_CACHE_CAP];
static volatile uint64_t g_kain_converge_telemetry_cursor = 0;
static volatile uint64_t g_kain_converge_cache_probes = 0;
static volatile uint64_t g_kain_converge_cache_hits = 0;

static uint64_t kain_converge_atomic_fetch_add_u64(volatile uint64_t* target, uint64_t increment) {
#if defined(_MSC_VER)
    return (uint64_t)_InterlockedExchangeAdd64((volatile long long*)target, (long long)increment);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_add(target, increment, __ATOMIC_RELAXED);
#else
    uint64_t old = *target;
    *target = old + increment;
    return old;
#endif
}

static uint64_t kain_converge_mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ull;
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31;
    return value;
}

static uint64_t kain_converge_cache_key(uint64_t converge_key, uint64_t shape_key) {
    uint64_t cpu = abi_cpu_feature_fingerprint();
    return kain_converge_mix64(converge_key ^ (shape_key + 0x9e3779b97f4a7c15ull) ^ cpu) | 1ull;
}

static uint64_t kain_converge_lowbit_lane(uint64_t eligible_mask, uint64_t fallback_lane) {
    if (eligible_mask == 0) {
        return fallback_lane;
    }
#if defined(_MSC_VER) && defined(_M_X64)
    {
        unsigned long index = 0;
        _BitScanForward64(&index, eligible_mask);
        return (uint64_t)index;
    }
#elif defined(__GNUC__) || defined(__clang__)
    return (uint64_t)__builtin_ctzll(eligible_mask);
#else
    {
        uint64_t index = 0;
        while (((eligible_mask >> index) & 1ull) == 0ull) {
            index += 1;
        }
        return index;
    }
#endif
}

int64_t abi_converge_select_lane_for_key(
    uint64_t converge_key,
    uint64_t shape_key,
    uint64_t eligible_mask,
    uint64_t fallback_lane
) {
    uint64_t key;
    uint64_t base;
    uint64_t stride;
    uint64_t attempt;

    if (eligible_mask == 0) {
        return (int64_t)fallback_lane;
    }

    key = kain_converge_cache_key(converge_key, shape_key);
    base = key & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);
    stride = ((key >> 6) | 1ull) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);

    for (attempt = 0; attempt < KAIN_CONVERGE_TUNE_CACHE_CAP; attempt += 1) {
        uint64_t slot_index = (base + attempt * stride) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);
        KainConvergeTuneCacheSlot* slot = &g_kain_converge_cache[slot_index];
        uint64_t lane = slot->lane_index;
        kain_converge_atomic_fetch_add_u64(&g_kain_converge_cache_probes, 1);
        if (slot->key == key && lane < 64u && ((eligible_mask >> lane) & 1ull) != 0ull) {
            kain_converge_atomic_fetch_add_u64(&g_kain_converge_cache_hits, 1);
            return (int64_t)lane;
        }
        if (slot->key == 0) {
            break;
        }
    }

    return (int64_t)kain_converge_lowbit_lane(eligible_mask, fallback_lane);
}

int64_t abi_converge_commit_winner(
    uint64_t converge_key,
    uint64_t shape_key,
    uint64_t lane_index,
    uint64_t eligible_mask
) {
    uint64_t key;
    uint64_t base;
    uint64_t stride;
    uint64_t attempt;

    if (lane_index >= 64u || ((eligible_mask >> lane_index) & 1ull) == 0ull) {
        return -1;
    }

    key = kain_converge_cache_key(converge_key, shape_key);
    base = key & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);
    stride = ((key >> 6) | 1ull) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);

    for (attempt = 0; attempt < KAIN_CONVERGE_TUNE_CACHE_CAP; attempt += 1) {
        uint64_t slot_index = (base + attempt * stride) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);
        KainConvergeTuneCacheSlot* slot = &g_kain_converge_cache[slot_index];
        if (slot->key == 0 || slot->key == key) {
            slot->key = key;
            slot->eligible_mask = eligible_mask;
            slot->lane_index = lane_index;
            slot->generation += 1;
            return 0;
        }
    }

    {
        KainConvergeTuneCacheSlot* slot = &g_kain_converge_cache[base];
        slot->key = key;
        slot->eligible_mask = eligible_mask;
        slot->lane_index = lane_index;
        slot->generation += 1;
    }
    return 0;
}

int64_t abi_converge_record_telemetry(
    uint64_t converge_key,
    uint64_t shape_key,
    uint64_t lane_index,
    uint64_t elapsed_ticks,
    int64_t status
) {
    uint64_t cursor = kain_converge_atomic_fetch_add_u64(&g_kain_converge_telemetry_cursor, 1);
    uint64_t slot_index = cursor & (KAIN_CONVERGE_TELEMETRY_CAP - 1u);
    KainConvergeTelemetrySample* sample = &g_kain_converge_telemetry[slot_index];
    sample->converge_key = converge_key;
    sample->shape_key = shape_key;
    sample->lane_index = lane_index;
    sample->elapsed_ticks = elapsed_ticks;
    sample->status = status;
    return 0;
}

uint64_t abi_converge_telemetry_count(void) {
    return g_kain_converge_telemetry_cursor;
}

uint64_t abi_converge_cache_probe_count(void) {
    return g_kain_converge_cache_probes;
}

uint64_t abi_converge_cache_hit_count(void) {
    return g_kain_converge_cache_hits;
}
