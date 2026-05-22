"""Benchmark runtime sync pathway candidates for actor init and service catalog population."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import textwrap
from pathlib import Path

from _runtime_scan_common import DATA_DIR, Z3_ROOT


BENCH_C = r"""
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <time.h>
#endif

static volatile uint64_t g_sink = 0u;

static void bench_pause(unsigned int spin_index) {
#ifdef _WIN32
    if ((spin_index & 63u) == 63u) {
        SwitchToThread();
    } else {
        YieldProcessor();
    }
#else
    if ((spin_index & 63u) == 63u) {
        sched_yield();
    } else {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ __volatile__("pause" ::: "memory");
#endif
    }
#endif
}

static uint64_t bench_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000000ULL) / (uint64_t)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
#endif
}

enum {
    ACTOR_INIT_COLD = 0u,
    ACTOR_INIT_BUSY = 1u,
    ACTOR_INIT_READY = 2u,
    SERVICE_CAPACITY = 64,
    SERVICE_KEY_MAX = 48,
    SERVICE_COUNT = 31,
};

typedef struct {
    char key[SERVICE_KEY_MAX];
    uint64_t state;
} BenchServiceDescriptor;

typedef struct {
    int initialized;
    int count;
    BenchServiceDescriptor descriptors[SERVICE_CAPACITY];
} PlainRegistry;

typedef struct {
    atomic_uint initialized;
    atomic_uint mutation_gate;
    atomic_int count;
    BenchServiceDescriptor descriptors[SERVICE_CAPACITY];
} AtomicRegistry;

static const char* g_service_keys[SERVICE_COUNT] = {
    "base.memory",
    "memory.ownership",
    "base.diagnostics",
    "contract",
    "reflection",
    "actor.runtime",
    "actor.registry",
    "async.runtime",
    "async.timers",
    "io.net",
    "io.process",
    "platform.app-host",
    "platform.input",
    "gfx.viewport",
    "gfx.raw-native",
    "gfx.backend.vulkan",
    "gfx.backend.d3d12",
    "gfx.shader.spirv",
    "gfx.compute",
    "scene.runtime",
    "scene.query",
    "scene.mutation",
    "runtime.inspection",
    "device.reflection",
    "ui.bundle",
    "ui.component",
    "asset.gltf",
    "asset.ingestion",
    "asset.realtime",
    "host.bridge",
    "compatibility"
};

static uint64_t service_state(const char* text) {
    uint64_t hash = 1469598103934665603ULL;
    unsigned char byte;
    while ((byte = (unsigned char)*text++) != 0u) {
        if (byte >= 'A' && byte <= 'Z') {
            byte = (unsigned char)(byte + ('a' - 'A'));
        }
        hash ^= (uint64_t)byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void make_descriptor(BenchServiceDescriptor* descriptor, const char* key) {
    memset(descriptor, 0, sizeof(*descriptor));
    strncpy(descriptor->key, key, sizeof(descriptor->key) - 1u);
    descriptor->state = service_state(key);
}

static int actor_fast_plain(const int* ready_flag) {
    return *ready_flag != 0;
}

static int actor_fast_atomic(const atomic_uint* state) {
    return atomic_load_explicit(state, memory_order_acquire) == ACTOR_INIT_READY;
}

#ifdef _WIN32
static INIT_ONCE g_actor_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK bench_actor_once_cb(PINIT_ONCE init_once, PVOID parameter, PVOID* context) {
    (void)init_once;
    (void)parameter;
    (void)context;
    return TRUE;
}
static int actor_fast_os_once(void) {
    InitOnceExecuteOnce(&g_actor_once, bench_actor_once_cb, NULL, NULL);
    return 1;
}
#else
static pthread_once_t g_actor_once = PTHREAD_ONCE_INIT;
static void bench_actor_once_cb(void) {}
static int actor_fast_os_once(void) {
    pthread_once(&g_actor_once, bench_actor_once_cb);
    return 1;
}
#endif

static void plain_registry_init(PlainRegistry* registry) {
    memset(registry, 0, sizeof(*registry));
    registry->initialized = 1;
}

static void atomic_registry_init(AtomicRegistry* registry) {
    memset(registry, 0, sizeof(*registry));
    atomic_init(&registry->initialized, 1u);
    atomic_init(&registry->mutation_gate, 0u);
    atomic_init(&registry->count, 0);
}

static int plain_lookup(const PlainRegistry* registry, const BenchServiceDescriptor* needle) {
    int i;
    for (i = 0; i < registry->count; ++i) {
        if (registry->descriptors[i].state == needle->state &&
            strcmp(registry->descriptors[i].key, needle->key) == 0) {
            return i;
        }
    }
    return -1;
}

static int atomic_lookup(const AtomicRegistry* registry, const BenchServiceDescriptor* needle) {
    int count = atomic_load_explicit(&registry->count, memory_order_acquire);
    int i;
    for (i = 0; i < count; ++i) {
        if (registry->descriptors[i].state == needle->state &&
            strcmp(registry->descriptors[i].key, needle->key) == 0) {
            return i;
        }
    }
    return -1;
}

static void atomic_registry_lock(AtomicRegistry* registry) {
    unsigned int spin_index = 0u;
    for (;;) {
        unsigned int expected = 0u;
        if (atomic_compare_exchange_weak_explicit(
                &registry->mutation_gate,
                &expected,
                1u,
                memory_order_acquire,
                memory_order_relaxed
            )) {
            return;
        }
        bench_pause(spin_index++);
    }
}

static void atomic_registry_unlock(AtomicRegistry* registry) {
    atomic_store_explicit(&registry->mutation_gate, 0u, memory_order_release);
}

static int plain_register(PlainRegistry* registry, const BenchServiceDescriptor* descriptor) {
    if (!registry->initialized) {
        plain_registry_init(registry);
    }
    if (registry->count >= SERVICE_CAPACITY) {
        return -2;
    }
    if (plain_lookup(registry, descriptor) >= 0) {
        return -3;
    }
    registry->descriptors[registry->count] = *descriptor;
    registry->count += 1;
    return 0;
}

static int atomic_register_per_entry(AtomicRegistry* registry, const BenchServiceDescriptor* descriptor) {
    int count;
    if (atomic_load_explicit(&registry->initialized, memory_order_acquire) == 0u) {
        atomic_registry_init(registry);
    }
    if (atomic_lookup(registry, descriptor) >= 0) {
        return -3;
    }
    atomic_registry_lock(registry);
    if (atomic_lookup(registry, descriptor) >= 0) {
        atomic_registry_unlock(registry);
        return -3;
    }
    count = atomic_load_explicit(&registry->count, memory_order_relaxed);
    if (count >= SERVICE_CAPACITY) {
        atomic_registry_unlock(registry);
        return -2;
    }
    registry->descriptors[count] = *descriptor;
    atomic_store_explicit(&registry->count, count + 1, memory_order_release);
    atomic_registry_unlock(registry);
    return 0;
}

static int atomic_register_batch(AtomicRegistry* registry, const BenchServiceDescriptor* descriptors, int descriptor_count) {
    int i;
    if (atomic_load_explicit(&registry->initialized, memory_order_acquire) == 0u) {
        atomic_registry_init(registry);
    }
    atomic_registry_lock(registry);
    for (i = 0; i < descriptor_count; ++i) {
        int count;
        int slot = atomic_lookup(registry, &descriptors[i]);
        if (slot >= 0) {
            continue;
        }
        count = atomic_load_explicit(&registry->count, memory_order_relaxed);
        if (count >= SERVICE_CAPACITY) {
            atomic_registry_unlock(registry);
            return -2;
        }
        registry->descriptors[count] = descriptors[i];
        atomic_store_explicit(&registry->count, count + 1, memory_order_release);
    }
    atomic_registry_unlock(registry);
    return 0;
}

static double bench_actor_plain(uint64_t iterations) {
    int ready_flag = 1;
    uint64_t start = bench_now_ns();
    uint64_t i;
    for (i = 0u; i < iterations; ++i) {
        g_sink += (uint64_t)actor_fast_plain(&ready_flag);
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_actor_atomic(uint64_t iterations) {
    atomic_uint state;
    uint64_t start;
    uint64_t i;
    atomic_init(&state, ACTOR_INIT_READY);
    start = bench_now_ns();
    for (i = 0u; i < iterations; ++i) {
        g_sink += (uint64_t)actor_fast_atomic(&state);
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_actor_os_once(uint64_t iterations) {
    uint64_t start;
    uint64_t i;
    (void)actor_fast_os_once();
    start = bench_now_ns();
    for (i = 0u; i < iterations; ++i) {
        g_sink += (uint64_t)actor_fast_os_once();
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_service_plain(uint64_t iterations, const BenchServiceDescriptor* descriptors) {
    uint64_t start = bench_now_ns();
    uint64_t i;
    for (i = 0u; i < iterations; ++i) {
        PlainRegistry registry;
        int index;
        plain_registry_init(&registry);
        for (index = 0; index < SERVICE_COUNT; ++index) {
            plain_register(&registry, &descriptors[index]);
        }
        g_sink += (uint64_t)registry.count;
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_service_locked_entry(uint64_t iterations, const BenchServiceDescriptor* descriptors) {
    uint64_t start = bench_now_ns();
    uint64_t i;
    for (i = 0u; i < iterations; ++i) {
        AtomicRegistry registry;
        int index;
        atomic_registry_init(&registry);
        for (index = 0; index < SERVICE_COUNT; ++index) {
            atomic_register_per_entry(&registry, &descriptors[index]);
        }
        g_sink += (uint64_t)atomic_load_explicit(&registry.count, memory_order_relaxed);
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_service_locked_batch(uint64_t iterations, const BenchServiceDescriptor* descriptors) {
    uint64_t start = bench_now_ns();
    uint64_t i;
    for (i = 0u; i < iterations; ++i) {
        AtomicRegistry registry;
        atomic_registry_init(&registry);
        atomic_register_batch(&registry, descriptors, SERVICE_COUNT);
        g_sink += (uint64_t)atomic_load_explicit(&registry.count, memory_order_relaxed);
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_lookup_plain(uint64_t iterations, const BenchServiceDescriptor* descriptors) {
    PlainRegistry registry;
    uint64_t start;
    uint64_t i;
    int index;
    plain_registry_init(&registry);
    for (index = 0; index < SERVICE_COUNT; ++index) {
        plain_register(&registry, &descriptors[index]);
    }
    start = bench_now_ns();
    for (i = 0u; i < iterations; ++i) {
        g_sink += (uint64_t)(plain_lookup(&registry, &descriptors[i % SERVICE_COUNT]) + 1);
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

static double bench_lookup_atomic(uint64_t iterations, const BenchServiceDescriptor* descriptors) {
    AtomicRegistry registry;
    uint64_t start;
    uint64_t i;
    int index;
    atomic_registry_init(&registry);
    atomic_register_batch(&registry, descriptors, SERVICE_COUNT);
    start = bench_now_ns();
    for (i = 0u; i < iterations; ++i) {
        g_sink += (uint64_t)(atomic_lookup(&registry, &descriptors[i % SERVICE_COUNT]) + 1);
    }
    return (double)(bench_now_ns() - start) / (double)iterations;
}

int main(int argc, char** argv) {
    uint64_t actor_iterations = 20000000ULL;
    uint64_t service_iterations = 200000ULL;
    uint64_t lookup_iterations = 20000000ULL;
    BenchServiceDescriptor descriptors[SERVICE_COUNT];
    int i;
    double actor_plain;
    double actor_atomic;
    double actor_once;
    double service_plain;
    double service_locked_entry;
    double service_locked_batch;
    double lookup_plain;
    double lookup_atomic;

    if (argc >= 2) {
        actor_iterations = (uint64_t)_strtoui64(argv[1], NULL, 10);
    }
    if (argc >= 3) {
        service_iterations = (uint64_t)_strtoui64(argv[2], NULL, 10);
    }
    if (argc >= 4) {
        lookup_iterations = (uint64_t)_strtoui64(argv[3], NULL, 10);
    }

    for (i = 0; i < SERVICE_COUNT; ++i) {
        make_descriptor(&descriptors[i], g_service_keys[i]);
    }

    actor_plain = bench_actor_plain(actor_iterations);
    actor_atomic = bench_actor_atomic(actor_iterations);
    actor_once = bench_actor_os_once(actor_iterations);
    service_plain = bench_service_plain(service_iterations, descriptors);
    service_locked_entry = bench_service_locked_entry(service_iterations, descriptors);
    service_locked_batch = bench_service_locked_batch(service_iterations, descriptors);
    lookup_plain = bench_lookup_plain(lookup_iterations, descriptors);
    lookup_atomic = bench_lookup_atomic(lookup_iterations, descriptors);

    printf(
        "{"
        "\"actor_ready_plain_ns_per_call\":%.6f,"
        "\"actor_ready_atomic_ns_per_call\":%.6f,"
        "\"actor_ready_os_once_ns_per_call\":%.6f,"
        "\"service_populate_plain_ns_per_catalog\":%.6f,"
        "\"service_populate_locked_entry_ns_per_catalog\":%.6f,"
        "\"service_populate_locked_batch_ns_per_catalog\":%.6f,"
        "\"service_lookup_plain_ns_per_lookup\":%.6f,"
        "\"service_lookup_atomic_ns_per_lookup\":%.6f,"
        "\"sink\":%llu"
        "}\n",
        actor_plain,
        actor_atomic,
        actor_once,
        service_plain,
        service_locked_entry,
        service_locked_batch,
        lookup_plain,
        lookup_atomic,
        (unsigned long long)g_sink
    );
    return 0;
}
"""


def compile_and_run(
    actor_iterations: int,
    service_iterations: int,
    lookup_iterations: int,
) -> dict:
    compiler = shutil.which("clang") or shutil.which("gcc")
    if compiler is None:
        raise RuntimeError("Neither clang nor gcc was found on PATH.")

    generated_dir = Z3_ROOT / "generated" / "sync_pathway_bench"
    generated_dir.mkdir(parents=True, exist_ok=True)
    src_path = generated_dir / "sync_pathway_bench.c"
    exe_suffix = ".exe" if subprocess.os.name == "nt" else ""
    exe_path = generated_dir / f"sync_pathway_bench{exe_suffix}"
    src_path.write_text(BENCH_C, encoding="utf-8")

    compile_cmd = [compiler, "-O3", "-std=c11", str(src_path), "-o", str(exe_path)]
    if subprocess.os.name != "nt":
        compile_cmd.append("-lpthread")
    subprocess.run(compile_cmd, check=True, capture_output=True, text=True)

    run_cmd = [
        str(exe_path),
        str(actor_iterations),
        str(service_iterations),
        str(lookup_iterations),
    ]
    completed = subprocess.run(run_cmd, check=True, capture_output=True, text=True)
    return json.loads(completed.stdout)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actor-iterations", type=int, default=20_000_000)
    parser.add_argument("--service-iterations", type=int, default=200_000)
    parser.add_argument("--lookup-iterations", type=int, default=20_000_000)
    args = parser.parse_args()

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    raw = compile_and_run(
        actor_iterations=args.actor_iterations,
        service_iterations=args.service_iterations,
        lookup_iterations=args.lookup_iterations,
    )
    raw["actor_atomic_vs_os_once_speedup"] = (
        raw["actor_ready_os_once_ns_per_call"] / raw["actor_ready_atomic_ns_per_call"]
    )
    raw["actor_atomic_vs_plain_regression"] = (
        raw["actor_ready_atomic_ns_per_call"] / raw["actor_ready_plain_ns_per_call"]
    )
    raw["service_locked_batch_vs_locked_entry_speedup"] = (
        raw["service_populate_locked_entry_ns_per_catalog"]
        / raw["service_populate_locked_batch_ns_per_catalog"]
    )
    raw["service_lookup_atomic_vs_plain_regression"] = (
        raw["service_lookup_atomic_ns_per_lookup"]
        / raw["service_lookup_plain_ns_per_lookup"]
    )
    raw["bench_config"] = {
        "actor_iterations": args.actor_iterations,
        "service_iterations": args.service_iterations,
        "lookup_iterations": args.lookup_iterations,
    }

    output_path = DATA_DIR / "sync_pathway_bench.json"
    output_path.write_text(json.dumps(raw, indent=2), encoding="utf-8")

    print("Runtime Sync Pathway Benchmark")
    print(f"JSON: {output_path}")
    print(
        "Actor fast path ns/call: "
        f"plain={raw['actor_ready_plain_ns_per_call']:.3f}, "
        f"atomic={raw['actor_ready_atomic_ns_per_call']:.3f}, "
        f"os_once={raw['actor_ready_os_once_ns_per_call']:.3f}"
    )
    print(
        "Service populate ns/catalog: "
        f"plain={raw['service_populate_plain_ns_per_catalog']:.3f}, "
        f"locked_entry={raw['service_populate_locked_entry_ns_per_catalog']:.3f}, "
        f"locked_batch={raw['service_populate_locked_batch_ns_per_catalog']:.3f}"
    )
    print(
        "Service lookup ns/lookup: "
        f"plain={raw['service_lookup_plain_ns_per_lookup']:.3f}, "
        f"atomic={raw['service_lookup_atomic_ns_per_lookup']:.3f}"
    )
    print(
        "Key deltas: "
        f"actor atomic vs os_once={raw['actor_atomic_vs_os_once_speedup']:.2f}x faster, "
        f"service batch vs per-entry lock={raw['service_locked_batch_vs_locked_entry_speedup']:.2f}x faster"
    )


if __name__ == "__main__":
    main()
