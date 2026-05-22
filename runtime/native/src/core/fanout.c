#include "../../include/fanout.h"

#include <limits.h>
#include <stdatomic.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define KAIN_FANOUT_MAX_THREADS 64

typedef struct KainFanoutJob {
    atomic_llong next_index;
    int64_t end;
    void* ctx;
    KainFanoutIndexFn fn;
} KainFanoutJob;

typedef struct KainFanoutWorkerArgs {
    KainFanoutJob* job;
} KainFanoutWorkerArgs;

static void kain_fanout_drain_job(KainFanoutJob* job) {
    for (;;) {
        int64_t index = (int64_t)atomic_fetch_add_explicit(
            &job->next_index,
            1,
            memory_order_seq_cst
        );
        if (index >= job->end) {
            return;
        }
        job->fn(job->ctx, index);
    }
}

static int kain_fanout_cpu_worker_count(int64_t work_items) {
    if (work_items <= 0) {
        return 0;
    }

    int cpu_count = 1;
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0 && info.dwNumberOfProcessors <= INT_MAX) {
        cpu_count = (int)info.dwNumberOfProcessors;
    }
#else
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online > 0 && online <= INT_MAX) {
        cpu_count = (int)online;
    }
#endif
    if (cpu_count < 1) {
        cpu_count = 1;
    }
    if (cpu_count > KAIN_FANOUT_MAX_THREADS) {
        cpu_count = KAIN_FANOUT_MAX_THREADS;
    }
    if (work_items < cpu_count) {
        return (int)work_items;
    }
    return cpu_count;
}

#ifdef _WIN32
static DWORD WINAPI kain_fanout_worker_entry(LPVOID raw_args) {
    KainFanoutWorkerArgs* args = (KainFanoutWorkerArgs*)raw_args;
    kain_fanout_drain_job(args->job);
    return 0;
}
#else
static void* kain_fanout_worker_entry(void* raw_args) {
    KainFanoutWorkerArgs* args = (KainFanoutWorkerArgs*)raw_args;
    kain_fanout_drain_job(args->job);
    return NULL;
}
#endif

int __kain_fanout_i64(int64_t start, int64_t end, void* ctx, KainFanoutIndexFn fn) {
    if (fn == NULL) {
        return -1;
    }
    if (end <= start) {
        return 0;
    }

    int worker_count = kain_fanout_cpu_worker_count(end - start);
    if (worker_count <= 1) {
        for (int64_t index = start; index < end; ++index) {
            fn(ctx, index);
        }
        return 0;
    }

    KainFanoutJob job;
    atomic_init(&job.next_index, (atomic_llong)start);
    job.end = end;
    job.ctx = ctx;
    job.fn = fn;

#ifdef _WIN32
    HANDLE threads[KAIN_FANOUT_MAX_THREADS - 1];
    KainFanoutWorkerArgs args[KAIN_FANOUT_MAX_THREADS - 1];
    int spawned = 0;
    for (int worker = 0; worker < worker_count - 1; ++worker) {
        args[worker].job = &job;
        HANDLE thread = CreateThread(
            NULL,
            0,
            kain_fanout_worker_entry,
            &args[worker],
            0,
            NULL
        );
        if (thread == NULL) {
            for (int joined = 0; joined < spawned; ++joined) {
                WaitForSingleObject(threads[joined], INFINITE);
                CloseHandle(threads[joined]);
            }
            return -1;
        }
        threads[worker] = thread;
        spawned += 1;
    }

    kain_fanout_drain_job(&job);
    for (int worker = 0; worker < spawned; ++worker) {
        WaitForSingleObject(threads[worker], INFINITE);
        CloseHandle(threads[worker]);
    }
#else
    pthread_t threads[KAIN_FANOUT_MAX_THREADS - 1];
    KainFanoutWorkerArgs args[KAIN_FANOUT_MAX_THREADS - 1];
    int spawned = 0;
    for (int worker = 0; worker < worker_count - 1; ++worker) {
        args[worker].job = &job;
        if (pthread_create(&threads[worker], NULL, kain_fanout_worker_entry, &args[worker]) != 0) {
            for (int joined = 0; joined < spawned; ++joined) {
                pthread_join(threads[joined], NULL);
            }
            return -1;
        }
        spawned += 1;
    }

    kain_fanout_drain_job(&job);
    for (int worker = 0; worker < spawned; ++worker) {
        pthread_join(threads[worker], NULL);
    }
#endif

    return 0;
}

void kain_fanout_runtime_shutdown(void) {
}
