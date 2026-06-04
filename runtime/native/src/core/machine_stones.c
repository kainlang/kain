/*
 * KAIN native machine-stones runtime.
 *
 * This file backs the first native exploitation pass for axiom/pulse/shatter/
 * teleport. The compiler owns syntax and static checks; this runtime owns the
 * tiny ABI kernels that should stay faster than generic package-level glue.
 */

#include "../../include/machine_stones.h"
#include "../../include/cpu.h"
#include "../../include/diagnostics.h"
#include "../../include/fixup.h"
#include "../../include/ownership.h"
#include "../../include/profile.h"

#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

#define KAIN_MACHINE_PULSE_SLOT_COUNT 64u
#define KAIN_MACHINE_PULSE_SLOT_MASK (KAIN_MACHINE_PULSE_SLOT_COUNT - 1u)
#define KAIN_MACHINE_PULSE_SCHEDULER_IDLE_SLEEP_NS UINT64_C(1000000)
#define KAIN_MACHINE_SHATTER_SLOT_BYTES UINT64_C(8)
#define KAIN_MACHINE_TOKEN_SIG(length, first, second, last) \
    ((((uint32_t)(length)) << 24u) ^ (((uint32_t)(uint8_t)(first)) << 16u) ^ \
     (((uint32_t)(uint8_t)(second)) << 8u) ^ ((uint32_t)(uint8_t)(last)))

#if (KAIN_MACHINE_PULSE_SLOT_COUNT & KAIN_MACHINE_PULSE_SLOT_MASK) != 0
#error "KAIN_MACHINE_PULSE_SLOT_COUNT must be a power of two."
#endif

typedef struct KainMachinePulseSlot {
    uint64_t token;
    uint64_t last_ns;
    uint64_t tick;
    uint64_t interval_ns;
    uint64_t jitter_ns;
    uint64_t next_due_ns;
    uint64_t fire_count;
    KainMachinePulseFireFn fire;
    uint8_t occupied;
    uint8_t scheduled;
} KainMachinePulseSlot;

typedef struct KainMachinePulseFireJob {
    KainMachinePulseFireFn fire;
} KainMachinePulseFireJob;

typedef struct KainMachineShatterBuffer {
    uint64_t lane_count;
    uint64_t element_count;
    uint64_t payload_bytes;
    unsigned char data[];
} KainMachineShatterBuffer;

static KainMachinePulseSlot g_machine_pulse_slots[KAIN_MACHINE_PULSE_SLOT_COUNT];
static atomic_flag g_machine_pulse_lock = ATOMIC_FLAG_INIT;
static atomic_int g_machine_pulse_scheduler_running;
static atomic_int g_machine_pulse_stop_requested;
static atomic_int g_machine_pulse_atexit_registered;
static atomic_uint_fast64_t g_machine_pulse_total_fire_count;
static atomic_uint_fast64_t g_machine_teleport_count;
static atomic_uint_fast64_t g_machine_teleport_last_token;
static atomic_uint_fast64_t g_machine_teleport_last_handle;
static atomic_flag g_machine_teleport_untracked_warned = ATOMIC_FLAG_INIT;

#ifdef _WIN32
static HANDLE g_machine_pulse_thread;
#else
static pthread_t g_machine_pulse_thread;
static atomic_int g_machine_pulse_thread_joinable;
#endif

static void kain_machine_lock(atomic_flag* lock) {
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
    }
}

static void kain_machine_unlock(atomic_flag* lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}

static uint64_t kain_machine_mix64(uint64_t value) {
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

static uint64_t kain_machine_hash_text(uint64_t seed, const char* text) {
    uint64_t hash = seed ^ UINT64_C(0xcbf29ce484222325);
    if (text == NULL) {
        return kain_machine_mix64(hash);
    }
    while (*text != '\0') {
        hash ^= (uint8_t)*text++;
        hash *= UINT64_C(0x100000001b3);
    }
    return kain_machine_mix64(hash);
}

static uint64_t kain_machine_add_saturating_u64(uint64_t a, uint64_t b) {
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}

static void kain_machine_sleep_ns(uint64_t ns) {
    uint64_t ms = (ns + UINT64_C(999999)) / UINT64_C(1000000);
    if (ms == 0u) {
        ms = 1u;
    }
#ifdef _WIN32
    Sleep((DWORD)(ms > UINT32_MAX ? UINT32_MAX : ms));
#else
    struct timespec delay;
    delay.tv_sec = (time_t)(ms / UINT64_C(1000));
    delay.tv_nsec = (long)((ms % UINT64_C(1000)) * UINT64_C(1000000));
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
#endif
}

uint64_t kain_machine_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart <= 0) {
        return 0;
    }
    return (uint64_t)((counter.QuadPart * 1000000000ull) / (uint64_t)frequency.QuadPart);
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return ((uint64_t)now.tv_sec * UINT64_C(1000000000)) + (uint64_t)now.tv_nsec;
#endif
}

uint64_t kain_machine_real_time_now_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER li;
    GetSystemTimePreciseAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    // FILETIME is 100-nanosecond intervals since Jan 1 1601.
    // Offset to Unix epoch: 116444736000000000 100ns ticks.
    return (uint64_t)((li.QuadPart - 116444736000000000ULL) / 10000ULL);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

static uint32_t kain_machine_token_signature(const char* key) {
    size_t length;
    if (key == NULL) {
        return 0u;
    }
    length = strlen(key);
    return KAIN_MACHINE_TOKEN_SIG(
        length,
        length > 0u ? key[0] : 0,
        length > 1u ? key[1] : 0,
        length > 0u ? key[length - 1u] : 0
    );
}

static uint64_t kain_machine_capability_mask_for_key(const char* key) {
    /* Proof: runtime/native/src/core/z3/proofs/native-machine-capability-token-signatures-are-collision-free.yaml */
    switch (kain_machine_token_signature(key)) {
        case KAIN_MACHINE_TOKEN_SIG(14u, 'a', 't', 'k'):
            return strcmp(key, "atomic.bitmask") == 0 ? KAIN_MACHINE_CAP_ATOMIC_BITMASK : 0u;
        case KAIN_MACHINE_TOKEN_SIG(19u, 't', 'i', 'r'):
            return strcmp(key, "time.hardware-timer") == 0 ? KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER : 0u;
        case KAIN_MACHINE_TOKEN_SIG(10u, 't', 'i', 'e'):
            return strcmp(key, "time.pulse") == 0 ? KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER : 0u;
        case KAIN_MACHINE_TOKEN_SIG(14u, 'm', 'e', 'r'):
            return strcmp(key, "memory.shatter") == 0 ? KAIN_MACHINE_CAP_MEMORY_SHATTER : 0u;
        case KAIN_MACHINE_TOKEN_SIG(14u, 'w', 'o', 't'):
            return strcmp(key, "world.teleport") == 0 ? KAIN_MACHINE_CAP_WORLD_TELEPORT : 0u;
        case KAIN_MACHINE_TOKEN_SIG(26u, 'i', 'n', 'f'):
            return strcmp(key, "interop.zero-copy-handoff") == 0 ? KAIN_MACHINE_CAP_WORLD_TELEPORT : 0u;
        default:
            return 0u;
    }
}

static uint64_t kain_machine_current_capabilities(void) {
    uint64_t caps = KAIN_MACHINE_CAP_ATOMIC_BITMASK |
                    KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER |
                    KAIN_MACHINE_CAP_MEMORY_SHATTER |
                    KAIN_MACHINE_CAP_WORLD_TELEPORT;
    uint64_t cpu = abi_cpu_feature_mask();
    if ((cpu & KAIN_CPU_FEATURE_X86_SSE2) != 0u) {
        caps |= KAIN_MACHINE_CAP_X86_SSE2;
    }
    if ((cpu & KAIN_CPU_FEATURE_X86_AVX) != 0u) {
        caps |= KAIN_MACHINE_CAP_X86_AVX;
    }
    if ((cpu & KAIN_CPU_FEATURE_X86_AVX2) != 0u) {
        caps |= KAIN_MACHINE_CAP_X86_AVX2;
    }
    if ((cpu & KAIN_CPU_FEATURE_X86_AVX512F) != 0u) {
        caps |= KAIN_MACHINE_CAP_X86_AVX512F;
    }
    return caps;
}

static int kain_machine_target_matches(const char* target) {
    return target == NULL || target[0] == '\0' || strcmp(target, "llvm") == 0 ||
           strcmp(target, "native") == 0;
}

static int kain_machine_arch_matches(const char* arch) {
    if (arch == NULL || arch[0] == '\0') {
        return 1;
    }
#if defined(_M_X64) || defined(__x86_64__)
    return strcmp(arch, "x86_64") == 0 || strcmp(arch, "amd64") == 0;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return strcmp(arch, "aarch64") == 0 || strcmp(arch, "arm64") == 0;
#else
    return 0;
#endif
}

int64_t kain_machine_axiom_accept(const char* target, const char* arch, uint64_t required_caps) {
    uint64_t available;
    (void)kain_machine_capability_mask_for_key;
    if (!kain_machine_target_matches(target) || !kain_machine_arch_matches(arch)) {
        return 0;
    }
    available = kain_machine_current_capabilities();
    return (available & required_caps) == required_caps ? 1 : 0;
}

static uint32_t kain_machine_pulse_slot_start(uint64_t token) {
    return (uint32_t)(kain_machine_mix64(token) & KAIN_MACHINE_PULSE_SLOT_MASK);
}

static KainMachinePulseSlot* kain_machine_pulse_slot(uint64_t token) {
    uint32_t start = kain_machine_pulse_slot_start(token);
    uint32_t probe;
    for (probe = 0u; probe < KAIN_MACHINE_PULSE_SLOT_COUNT; ++probe) {
        uint32_t index = (start + probe) & KAIN_MACHINE_PULSE_SLOT_MASK;
        KainMachinePulseSlot* slot = &g_machine_pulse_slots[index];
        if (!slot->occupied || slot->token == token) {
            if (!slot->occupied) {
                slot->occupied = 1u;
                slot->token = token;
                slot->last_ns = 0u;
                slot->tick = 0u;
                slot->interval_ns = 0u;
                slot->jitter_ns = 0u;
                slot->next_due_ns = 0u;
                slot->fire_count = 0u;
                slot->fire = NULL;
                slot->scheduled = 0u;
            }
            return slot;
        }
    }
    return &g_machine_pulse_slots[start];
}

static void kain_machine_pulse_record_fire(void) {
    atomic_fetch_add_explicit(&g_machine_pulse_total_fire_count, 1u, memory_order_relaxed);
}

static void kain_machine_pulse_register_atexit_once(void) {
    if (atomic_exchange_explicit(&g_machine_pulse_atexit_registered, 1, memory_order_acq_rel) == 0) {
        atexit(kain_machine_pulse_stop_all);
    }
}

#ifdef _WIN32
static DWORD WINAPI kain_machine_pulse_scheduler_proc(LPVOID ignored)
#else
static void* kain_machine_pulse_scheduler_proc(void* ignored)
#endif
{
    (void)ignored;
    while (atomic_load_explicit(&g_machine_pulse_stop_requested, memory_order_acquire) == 0) {
        KainMachinePulseFireJob jobs[KAIN_MACHINE_PULSE_SLOT_COUNT];
        uint32_t job_count = 0u;
        uint64_t now_ns = kain_machine_now_ns();
        uint64_t sleep_ns = KAIN_MACHINE_PULSE_SCHEDULER_IDLE_SLEEP_NS;
        uint32_t i;

        kain_machine_lock(&g_machine_pulse_lock);
        for (i = 0u; i < KAIN_MACHINE_PULSE_SLOT_COUNT; ++i) {
            KainMachinePulseSlot* slot = &g_machine_pulse_slots[i];
            if (!slot->occupied || !slot->scheduled || slot->fire == NULL) {
                continue;
            }
            if (slot->next_due_ns <= now_ns) {
                if (job_count < KAIN_MACHINE_PULSE_SLOT_COUNT) {
                    jobs[job_count].fire = slot->fire;
                    ++job_count;
                }
                slot->next_due_ns = kain_machine_add_saturating_u64(now_ns, slot->interval_ns);
                ++slot->fire_count;
            } else {
                uint64_t until_due = slot->next_due_ns - now_ns;
                if (until_due < sleep_ns) {
                    sleep_ns = until_due;
                }
            }
        }
        kain_machine_unlock(&g_machine_pulse_lock);

        for (i = 0u; i < job_count; ++i) {
            kain_machine_pulse_record_fire();
            jobs[i].fire();
        }
        if (job_count == 0u) {
            kain_machine_sleep_ns(sleep_ns);
        }
    }
    atomic_store_explicit(&g_machine_pulse_scheduler_running, 0, memory_order_release);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int kain_machine_pulse_ensure_scheduler_started(void) {
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(
            &g_machine_pulse_scheduler_running,
            &expected,
            1,
            memory_order_acq_rel,
            memory_order_acquire
        )) {
        return 1;
    }
    atomic_store_explicit(&g_machine_pulse_stop_requested, 0, memory_order_release);
#ifdef _WIN32
    g_machine_pulse_thread =
        CreateThread(NULL, 0, kain_machine_pulse_scheduler_proc, NULL, 0, NULL);
    if (g_machine_pulse_thread == NULL) {
        atomic_store_explicit(&g_machine_pulse_scheduler_running, 0, memory_order_release);
        return 0;
    }
#else
    if (pthread_create(&g_machine_pulse_thread, NULL, kain_machine_pulse_scheduler_proc, NULL) != 0) {
        atomic_store_explicit(&g_machine_pulse_scheduler_running, 0, memory_order_release);
        return 0;
    }
    atomic_store_explicit(&g_machine_pulse_thread_joinable, 1, memory_order_release);
#endif
    return 1;
}

void kain_machine_pulse_snapshot(
    uint64_t pulse_token,
    uint64_t interval_ns,
    uint64_t jitter_ns,
    uint64_t* out_tick,
    int64_t* out_dt_ms,
    uint64_t* out_missed
) {
    uint64_t now_ns = kain_machine_now_ns();
    uint64_t elapsed_ns;
    uint64_t advanced;
    KainMachinePulseSlot* slot;
    if (interval_ns == 0u) {
        interval_ns = 1u;
    }

    kain_machine_lock(&g_machine_pulse_lock);
    slot = kain_machine_pulse_slot(pulse_token);
    if (slot->last_ns == 0u) {
        slot->last_ns = now_ns;
        slot->tick = 0u;
        elapsed_ns = interval_ns;
        advanced = 1u;
    } else {
        elapsed_ns = now_ns >= slot->last_ns ? now_ns - slot->last_ns : 0u;
        advanced = elapsed_ns / interval_ns;
        if (advanced > 0u) {
            slot->tick += advanced;
            slot->last_ns = now_ns;
        }
    }

    if (out_tick != NULL) {
        *out_tick = slot->tick;
    }
    if (out_dt_ms != NULL) {
        uint64_t dt = elapsed_ns / UINT64_C(1000000);
        *out_dt_ms = dt > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)dt;
    }
    if (out_missed != NULL) {
        uint64_t tolerated = interval_ns + jitter_ns;
        /* Proof: runtime/native/src/core/z3/proofs/native-machine-pulse-missed-beat-stays-bounded.yaml */
        *out_missed = (advanced > 1u && elapsed_ns > tolerated) ? advanced - 1u : 0u;
    }
    kain_machine_unlock(&g_machine_pulse_lock);
}

int64_t kain_machine_pulse_start(
    uint64_t pulse_token,
    uint64_t interval_ns,
    uint64_t jitter_ns,
    KainMachinePulseFireFn fire
) {
    uint64_t now_ns;
    KainMachinePulseSlot* slot;
    if (fire == NULL) {
        errno = EINVAL;
        return 0;
    }
    if (interval_ns == 0u) {
        interval_ns = 1u;
    }

    kain_machine_pulse_register_atexit_once();
    now_ns = kain_machine_now_ns();
    kain_machine_lock(&g_machine_pulse_lock);
    slot = kain_machine_pulse_slot(pulse_token);
    slot->interval_ns = interval_ns;
    slot->jitter_ns = jitter_ns;
    slot->next_due_ns = kain_machine_add_saturating_u64(now_ns, interval_ns);
    slot->fire = fire;
    slot->scheduled = 1u;
    ++slot->fire_count;
    kain_machine_unlock(&g_machine_pulse_lock);

    if (!kain_machine_pulse_ensure_scheduler_started()) {
        kain_machine_lock(&g_machine_pulse_lock);
        slot = kain_machine_pulse_slot(pulse_token);
        slot->scheduled = 0u;
        slot->fire = NULL;
        kain_machine_unlock(&g_machine_pulse_lock);
        return 0;
    }

    kain_machine_pulse_record_fire();
    fire();
    return 1;
}

void kain_machine_pulse_stop_all(void) {
    uint32_t i;
    atomic_store_explicit(&g_machine_pulse_stop_requested, 1, memory_order_release);
#ifdef _WIN32
    if (g_machine_pulse_thread != NULL) {
        WaitForSingleObject(g_machine_pulse_thread, INFINITE);
        CloseHandle(g_machine_pulse_thread);
        g_machine_pulse_thread = NULL;
    }
#else
    if (atomic_exchange_explicit(&g_machine_pulse_thread_joinable, 0, memory_order_acq_rel) != 0) {
        pthread_join(g_machine_pulse_thread, NULL);
    }
#endif
    atomic_store_explicit(&g_machine_pulse_scheduler_running, 0, memory_order_release);
    kain_machine_lock(&g_machine_pulse_lock);
    for (i = 0u; i < KAIN_MACHINE_PULSE_SLOT_COUNT; ++i) {
        g_machine_pulse_slots[i].scheduled = 0u;
        g_machine_pulse_slots[i].fire = NULL;
    }
    kain_machine_unlock(&g_machine_pulse_lock);
}

uint64_t kain_machine_pulse_total_fire_count(void) {
    return atomic_load_explicit(&g_machine_pulse_total_fire_count, memory_order_acquire);
}

void* kain_machine_teleport_ptr(
    void* ptr,
    const char* source_world,
    const char* target_world,
    const char* channel
) {
    KainProfileScope profile_scope;
    KainRuntimeHandle handle = KAIN_RUNTIME_HANDLE_INVALID;
    void* result = ptr;
    /* Proof: runtime/native/src/core/z3/proofs/native-machine-teleport-token-handoff-is-exclusive.yaml */
    uint64_t token = kain_machine_hash_text((uintptr_t)ptr, source_world);
    token ^= kain_machine_hash_text(token, target_world);
    token ^= kain_machine_hash_text(token, channel);
    kain_profile_scope_begin(&profile_scope, "machine.teleport", __FILE__, (uint32_t)__LINE__);
    __kain_ownership_flush_deferred_decay();
    if (ptr != NULL) {
        handle = kain_fixup_handle_for_pointer(ptr);
        if (handle != KAIN_RUNTIME_HANDLE_INVALID) {
            KainFixupTrackedView view;
            if (kain_fixup_view(handle, &view) == 0 && view.base != NULL && view.size != 0u) {
                uintptr_t base_addr = (uintptr_t)view.base;
                uintptr_t ptr_addr = (uintptr_t)ptr;
                uintptr_t offset = ptr_addr >= base_addr ? ptr_addr - base_addr : 0u;
                (void)kain_fixup_relocate_allocation(handle, view.base, view.base, view.size);
                result = offset < view.size ? (void*)(base_addr + offset) : view.base;
            }
        } else if (!atomic_flag_test_and_set_explicit(
                       &g_machine_teleport_untracked_warned,
                       memory_order_relaxed
                   ) &&
                   KAIN_DIAG_EMIT_IF(KAIN_DIAG_SUBSYSTEM_MACHINE, KAIN_DIAG_SEVERITY_WARNING)) {
            KainDiagnostic diag;
            kain_diagnostic_create(
                &diag,
                KAIN_DIAG_SUBSYSTEM_MACHINE,
                KAIN_DIAG_SEVERITY_WARNING,
                KAIN_DIAG_CODE_GENERIC_ERROR,
                "teleport fell back to token-only handoff",
                "No relocation handle or known-fixup range was registered for this pointer.",
                "runtime/native/src/core/machine_stones.c"
            );
            kain_diagnostic_print(&diag);
        }
    }
    atomic_fetch_add_explicit(&g_machine_teleport_count, 1u, memory_order_relaxed);
    atomic_store_explicit(&g_machine_teleport_last_token, token, memory_order_release);
    atomic_store_explicit(&g_machine_teleport_last_handle, handle, memory_order_release);
    kain_profile_scope_end(&profile_scope);
    return result;
}

void kain_machine_teleport_note(
    const char* source_world,
    const char* target_world,
    const char* channel
) {
    (void)kain_machine_teleport_ptr(NULL, source_world, target_world, channel);
}

uint64_t kain_machine_teleport_count(void) {
    return atomic_load_explicit(&g_machine_teleport_count, memory_order_acquire);
}

uint64_t kain_machine_teleport_last_token(void) {
    return atomic_load_explicit(&g_machine_teleport_last_token, memory_order_acquire);
}

uint64_t kain_machine_teleport_last_handle(void) {
    return atomic_load_explicit(&g_machine_teleport_last_handle, memory_order_acquire);
}

static int kain_machine_mul_overflow_u64(uint64_t a, uint64_t b, uint64_t* out) {
    if (out == NULL) {
        return 1;
    }
    if (a != 0u && b > UINT64_MAX / a) {
        return 1;
    }
    *out = a * b;
    return 0;
}

static int kain_machine_add_overflow_u64(uint64_t a, uint64_t b, uint64_t* out) {
    if (out == NULL || b > UINT64_MAX - a) {
        return 1;
    }
    *out = a + b;
    return 0;
}

void* kain_machine_shatter_alloc(uint64_t lane_count, uint64_t element_count) {
    uint64_t slots;
    uint64_t payload_bytes;
    uint64_t total_bytes;
    KainMachineShatterBuffer* buffer;
    if (lane_count == 0u || element_count == 0u) {
        errno = EINVAL;
        return NULL;
    }
    if (kain_machine_mul_overflow_u64(lane_count, element_count, &slots) ||
        kain_machine_mul_overflow_u64(slots, KAIN_MACHINE_SHATTER_SLOT_BYTES, &payload_bytes) ||
        kain_machine_add_overflow_u64(sizeof(KainMachineShatterBuffer), payload_bytes, &total_bytes) ||
        total_bytes > (uint64_t)SIZE_MAX) {
        errno = ERANGE;
        return NULL;
    }
    buffer = (KainMachineShatterBuffer*)calloc(1u, (size_t)total_bytes);
    if (buffer == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    buffer->lane_count = lane_count;
    buffer->element_count = element_count;
    buffer->payload_bytes = payload_bytes;
    return buffer;
}

void* kain_machine_shatter_lane_ptr(void* handle, uint64_t lane_index, uint64_t element_index) {
    KainMachineShatterBuffer* buffer = (KainMachineShatterBuffer*)handle;
    uint64_t linear_index;
    uint64_t byte_offset;
    if (buffer == NULL || lane_index >= buffer->lane_count ||
        element_index >= buffer->element_count) {
        errno = ERANGE;
        return NULL;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-machine-shatter-lane-offset-stays-in-payload.yaml */
    if (kain_machine_mul_overflow_u64(lane_index, buffer->element_count, &linear_index) ||
        kain_machine_add_overflow_u64(linear_index, element_index, &linear_index) ||
        kain_machine_mul_overflow_u64(linear_index, KAIN_MACHINE_SHATTER_SLOT_BYTES, &byte_offset) ||
        byte_offset >= buffer->payload_bytes) {
        errno = ERANGE;
        return NULL;
    }
    return (void*)(buffer->data + byte_offset);
}

void* kain_machine_shatter_lane_base(void* handle, uint64_t lane_index) {
    KainMachineShatterBuffer* buffer = (KainMachineShatterBuffer*)handle;
    uint64_t lane_slots;
    uint64_t byte_offset;
    if (buffer == NULL || lane_index >= buffer->lane_count) {
        errno = ERANGE;
        return NULL;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml */
    if (kain_machine_mul_overflow_u64(lane_index, buffer->element_count, &lane_slots) ||
        kain_machine_mul_overflow_u64(lane_slots, KAIN_MACHINE_SHATTER_SLOT_BYTES, &byte_offset) ||
        byte_offset >= buffer->payload_bytes) {
        errno = ERANGE;
        return NULL;
    }
    return (void*)(buffer->data + byte_offset);
}

void kain_machine_shatter_free(void* handle) {
    free(handle);
}
