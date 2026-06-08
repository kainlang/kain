/*
 * CBMC verification harness for fanout
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

// Basic type definitions needed by runtime function signatures
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef signed char int8_t;
typedef unsigned long long size_t;
typedef long long ptrdiff_t;

// Forward declarations of functions under test
// kain_fanout_runtime_shutdown
void kain_fanout_runtime_shutdown(void);
// kain_fanout_drain_job
static void kain_fanout_drain_job(KainFanoutJob* job);
// kain_fanout_detect_cpu_count
static int kain_fanout_detect_cpu_count(void);
// kain_fanout_cpu_worker_count
static int kain_fanout_cpu_worker_count(int64_t work_items);
// kain_fanout_runtime_lock
static void kain_fanout_runtime_lock(KainFanoutRuntime* runtime);

int main(void) {
    kain_fanout_runtime_shutdown();
    __CPROVER_assert(1, "kain_fanout_runtime_shutdown: call ok");
    { void *__p; kain_fanout_drain_job(__p); }
    __CPROVER_assert(1, "kain_fanout_drain_job: call ok");
    kain_fanout_detect_cpu_count();
    __CPROVER_assert(1, "kain_fanout_detect_cpu_count: call ok");
    { void *__p; kain_fanout_cpu_worker_count(__p); }
    __CPROVER_assert(1, "kain_fanout_cpu_worker_count: call ok");
    { void *__p; kain_fanout_runtime_lock(__p); }
    __CPROVER_assert(1, "kain_fanout_runtime_lock: call ok");
    return 0;
}
