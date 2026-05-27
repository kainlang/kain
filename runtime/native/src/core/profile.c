#include "../../include/profile.h"
#include "../../include/base.h"
#include "../../include/machine_stones.h"

#include <stdatomic.h>

#define KAIN_PROFILE_STACK_DEPTH 64u

typedef struct {
    const char* label;
    const char* file;
    uint32_t line;
    uint64_t start_ns;
    uint64_t token;
} KainProfileFrame;

static _Thread_local KainProfileFrame KAIN_PROFILE_STACK[KAIN_PROFILE_STACK_DEPTH];
static _Thread_local uint32_t KAIN_PROFILE_STACK_TOP = 0u;
static atomic_uint_fast64_t KAIN_PROFILE_ZONE_COUNT;
static atomic_uint_fast64_t KAIN_PROFILE_TOTAL_NS;
static atomic_uint_fast64_t KAIN_PROFILE_LAST_DURATION_NS;
static atomic_uintptr_t KAIN_PROFILE_LAST_LABEL;

void kain_profile_scope_begin(
    KainProfileScope* scope,
    const char* label,
    const char* file,
    uint32_t line
) {
#if KAIN_RUNTIME_PROFILE_TIER == KAIN_RUNTIME_TIER_NOOP
    (void)scope;
    (void)label;
    (void)file;
    (void)line;
#else
    KainProfileFrame* frame;
    if (!scope) {
        return;
    }
    scope->label = label;
    scope->file = file;
    scope->line = line;
    scope->depth = KAIN_PROFILE_STACK_TOP;
    scope->token = 0u;
    scope->start_ns = 0u;
    scope->active = 0u;
    if (!KAIN_RUNTIME_PROFILE_ENABLED() || KAIN_PROFILE_STACK_TOP >= KAIN_PROFILE_STACK_DEPTH) {
        return;
    }
    frame = &KAIN_PROFILE_STACK[KAIN_PROFILE_STACK_TOP];
    frame->label = label;
    frame->file = file;
    frame->line = line;
    frame->start_ns = kain_machine_now_ns();
    frame->token = ((uint64_t)KAIN_PROFILE_STACK_TOP << 32u) ^ frame->start_ns;
    scope->depth = KAIN_PROFILE_STACK_TOP;
    scope->token = frame->token;
    scope->start_ns = frame->start_ns;
    scope->active = 1u;
    KAIN_PROFILE_STACK_TOP += 1u;
#endif
}

void kain_profile_scope_end(KainProfileScope* scope) {
#if KAIN_RUNTIME_PROFILE_TIER == KAIN_RUNTIME_TIER_NOOP
    (void)scope;
#else
    uint64_t end_ns;
    uint64_t duration;
    if (!scope || !scope->active || KAIN_PROFILE_STACK_TOP == 0u) {
        return;
    }
    if (scope->depth + 1u != KAIN_PROFILE_STACK_TOP) {
        return;
    }
    KAIN_PROFILE_STACK_TOP -= 1u;
    end_ns = kain_machine_now_ns();
    duration = end_ns >= scope->start_ns ? end_ns - scope->start_ns : 0u;
    atomic_fetch_add_explicit(&KAIN_PROFILE_ZONE_COUNT, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&KAIN_PROFILE_TOTAL_NS, duration, memory_order_relaxed);
    atomic_store_explicit(&KAIN_PROFILE_LAST_DURATION_NS, duration, memory_order_release);
    atomic_store_explicit(
        &KAIN_PROFILE_LAST_LABEL,
        (uintptr_t)scope->label,
        memory_order_release
    );
    scope->active = 0u;
#endif
}

void kain_profile_reset(void) {
    atomic_store_explicit(&KAIN_PROFILE_ZONE_COUNT, 0u, memory_order_release);
    atomic_store_explicit(&KAIN_PROFILE_TOTAL_NS, 0u, memory_order_release);
    atomic_store_explicit(&KAIN_PROFILE_LAST_DURATION_NS, 0u, memory_order_release);
    atomic_store_explicit(&KAIN_PROFILE_LAST_LABEL, (uintptr_t)0u, memory_order_release);
}

uint64_t kain_profile_zone_count(void) {
    return atomic_load_explicit(&KAIN_PROFILE_ZONE_COUNT, memory_order_acquire);
}

uint64_t kain_profile_total_ns(void) {
    return atomic_load_explicit(&KAIN_PROFILE_TOTAL_NS, memory_order_acquire);
}

uint64_t kain_profile_last_duration_ns(void) {
    return atomic_load_explicit(&KAIN_PROFILE_LAST_DURATION_NS, memory_order_acquire);
}

const char* kain_profile_last_label(void) {
    return (const char*)atomic_load_explicit(&KAIN_PROFILE_LAST_LABEL, memory_order_acquire);
}
