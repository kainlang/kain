#ifndef MACHINE_STONES_H
#define MACHINE_STONES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_MACHINE_CAP_ATOMIC_BITMASK UINT64_C(0x0000000000000001)
#define KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER UINT64_C(0x0000000000000002)
#define KAIN_MACHINE_CAP_MEMORY_SHATTER UINT64_C(0x0000000000000004)
#define KAIN_MACHINE_CAP_WORLD_TELEPORT UINT64_C(0x0000000000000008)
#define KAIN_MACHINE_CAP_X86_SSE2 UINT64_C(0x0000000000000100)
#define KAIN_MACHINE_CAP_X86_AVX UINT64_C(0x0000000000000200)
#define KAIN_MACHINE_CAP_X86_AVX2 UINT64_C(0x0000000000000400)
#define KAIN_MACHINE_CAP_X86_AVX512F UINT64_C(0x0000000000000800)

typedef void (*KainMachinePulseFireFn)(void);

uint64_t kain_machine_now_ns(void);
int64_t kain_machine_axiom_accept(const char* target, const char* arch, uint64_t required_caps);
void kain_machine_pulse_snapshot(
    uint64_t pulse_token,
    uint64_t interval_ns,
    uint64_t jitter_ns,
    uint64_t* out_tick,
    int64_t* out_dt_ms,
    uint64_t* out_missed
);
int64_t kain_machine_pulse_start(
    uint64_t pulse_token,
    uint64_t interval_ns,
    uint64_t jitter_ns,
    KainMachinePulseFireFn fire
);
void kain_machine_pulse_stop_all(void);
uint64_t kain_machine_pulse_total_fire_count(void);
void* kain_machine_teleport_ptr(
    void* ptr,
    const char* source_world,
    const char* target_world,
    const char* channel
);
void kain_machine_teleport_note(
    const char* source_world,
    const char* target_world,
    const char* channel
);
void* kain_machine_shatter_alloc(uint64_t lane_count, uint64_t element_count);
void* kain_machine_shatter_lane_ptr(void* handle, uint64_t lane_index, uint64_t element_index);
void* kain_machine_shatter_lane_base(void* handle, uint64_t lane_index);
void kain_machine_shatter_free(void* handle);
uint64_t kain_machine_teleport_count(void);
uint64_t kain_machine_teleport_last_token(void);

#ifdef __cplusplus
}
#endif

#endif
