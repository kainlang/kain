#ifndef KAIN_RUNTIME_CONVERGE_H
#define KAIN_RUNTIME_CONVERGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_CONVERGE_LANE_MAX 8u
#define KAIN_CONVERGE_TELEMETRY_CAP 64u
#define KAIN_CONVERGE_TUNE_CACHE_CAP 64u
#define KAIN_CONVERGE_NO_WINNER UINT64_MAX

typedef struct KainConvergeTelemetrySample {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t lane_index;
    uint64_t elapsed_ticks;
    int64_t status;
} KainConvergeTelemetrySample;

int64_t kain_native_converge_select_lane_for_key(
    uint64_t converge_key,
    uint64_t shape_key,
    uint64_t eligible_mask,
    uint64_t fallback_lane
);
int64_t kain_native_converge_commit_winner(
    uint64_t converge_key,
    uint64_t shape_key,
    uint64_t lane_index,
    uint64_t eligible_mask
);
int64_t kain_native_converge_record_telemetry(
    uint64_t converge_key,
    uint64_t shape_key,
    uint64_t lane_index,
    uint64_t elapsed_ticks,
    int64_t status
);
uint64_t kain_native_converge_telemetry_count(void);
uint64_t kain_native_converge_cache_probe_count(void);
uint64_t kain_native_converge_cache_hit_count(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RUNTIME_CONVERGE_H */
