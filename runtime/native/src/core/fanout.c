#include "../../include/fanout.h"

#include <limits.h>
#include <stdatomic.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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

typedef struct KainFanoutRuntime {
    int initialized;
    int shutting_down;
    int thread_count;
    unsigned long long generation;
    int active_workers;
    int completed_workers;
    KainFanoutJob* current_job;
#ifdef _WIN32
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE work_ready;
    CONDITION_VARIABLE work_done;
    HANDLE threads[KAIN_FANOUT_MAX_THREADS];
#else
    pthread_mutex_t lock;
    pthread_cond_t work_ready;
    pthread_cond_t work_done;
    pthread_t threads[KAIN_FANOUT_MAX_THREADS];
#endif
} KainFanoutRuntime;

static KainFanoutRuntime g_kain_fanout_runtime = {0};

static void kain_fanout_drain_job(KainFanoutJob* job) {
    for (;;) {
        int64_t index = (int64_t)atomic_fetch_add_explicit(
            &job->next_index,
            1,
            memory_order_seq_cst
        );
        /* Proof: runtime/native/src/core/z3/proofs/native-fanout-range-drain-stays-within-end.yaml */
        if (index >= job->end) {
            return;
        }
        job->fn(job->ctx, index);
    }
}

static int kain_fanout_detect_cpu_count(void) {
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
    return cpu_count;
}

static int kain_fanout_cpu_worker_count(int64_t work_items) {
    if (work_items <= 0) {
        return 0;
    }

    int cpu_count = kain_fanout_detect_cpu_count();
    if (work_items < cpu_count) {
        return (int)work_items;
    }
    return cpu_count;
}

#ifdef _WIN32
static void kain_fanout_runtime_lock(KainFanoutRuntime* runtime) {
    EnterCriticalSection(&runtime->lock);
}

static void kain_fanout_runtime_unlock(KainFanoutRuntime* runtime) {
    LeaveCriticalSection(&runtime->lock);
}

static void kain_fanout_runtime_wait(CONDITION_VARIABLE* cv, KainFanoutRuntime* runtime) {
    SleepConditionVariableCS(cv, &runtime->lock, INFINITE);
}

static void kain_fanout_runtime_signal(CONDITION_VARIABLE* cv) {
    WakeConditionVariable(cv);
}

static void kain_fanout_runtime_broadcast(CONDITION_VARIABLE* cv) {
    WakeAllConditionVariable(cv);
}

static DWORD WINAPI kain_fanout_worker_entry(LPVOID raw_runtime) {
    KainFanoutRuntime* runtime = (KainFanoutRuntime*)raw_runtime;
    unsigned long long seen_generation = 0;
    for (;;) {
        kain_fanout_runtime_lock(runtime);
        while (!runtime->shutting_down
            && (runtime->current_job == NULL || runtime->generation == seen_generation)) {
            kain_fanout_runtime_wait(&runtime->work_ready, runtime);
        }
        if (runtime->shutting_down) {
            kain_fanout_runtime_unlock(runtime);
            return 0;
        }
        seen_generation = runtime->generation;
        KainFanoutJob* job = runtime->current_job;
        kain_fanout_runtime_unlock(runtime);

        kain_fanout_drain_job(job);

        kain_fanout_runtime_lock(runtime);
        if (runtime->current_job == job && runtime->generation == seen_generation) {
            runtime->completed_workers += 1;
            if (runtime->completed_workers >= runtime->active_workers) {
                kain_fanout_runtime_signal(&runtime->work_done);
            }
        }
        kain_fanout_runtime_unlock(runtime);
    }
    return 0;
}
#else
static void kain_fanout_runtime_lock(KainFanoutRuntime* runtime) {
    pthread_mutex_lock(&runtime->lock);
}

static void kain_fanout_runtime_unlock(KainFanoutRuntime* runtime) {
    pthread_mutex_unlock(&runtime->lock);
}

static void kain_fanout_runtime_wait(pthread_cond_t* cv, KainFanoutRuntime* runtime) {
    pthread_cond_wait(cv, &runtime->lock);
}

static void kain_fanout_runtime_signal(pthread_cond_t* cv) {
    pthread_cond_signal(cv);
}

static void kain_fanout_runtime_broadcast(pthread_cond_t* cv) {
    pthread_cond_broadcast(cv);
}

static void* kain_fanout_worker_entry(void* raw_runtime) {
    KainFanoutRuntime* runtime = (KainFanoutRuntime*)raw_runtime;
    unsigned long long seen_generation = 0;
    for (;;) {
        kain_fanout_runtime_lock(runtime);
        while (!runtime->shutting_down
            && (runtime->current_job == NULL || runtime->generation == seen_generation)) {
            kain_fanout_runtime_wait(&runtime->work_ready, runtime);
        }
        if (runtime->shutting_down) {
            kain_fanout_runtime_unlock(runtime);
            return NULL;
        }
        seen_generation = runtime->generation;
        KainFanoutJob* job = runtime->current_job;
        kain_fanout_runtime_unlock(runtime);

        kain_fanout_drain_job(job);

        kain_fanout_runtime_lock(runtime);
        if (runtime->current_job == job && runtime->generation == seen_generation) {
            runtime->completed_workers += 1;
            if (runtime->completed_workers >= runtime->active_workers) {
                kain_fanout_runtime_signal(&runtime->work_done);
            }
        }
        kain_fanout_runtime_unlock(runtime);
    }
}
#endif

static int kain_fanout_runtime_init(void) {
    KainFanoutRuntime* runtime = &g_kain_fanout_runtime;
    if (runtime->initialized) {
        return 0;
    }

    runtime->thread_count = kain_fanout_detect_cpu_count() - 1;
    if (runtime->thread_count < 0) {
        runtime->thread_count = 0;
    }
    runtime->generation = 0;
    runtime->active_workers = 0;
    runtime->completed_workers = 0;
    runtime->current_job = NULL;
    runtime->shutting_down = 0;

#ifdef _WIN32
    InitializeCriticalSection(&runtime->lock);
    InitializeConditionVariable(&runtime->work_ready);
    InitializeConditionVariable(&runtime->work_done);
    for (int index = 0; index < runtime->thread_count; ++index) {
        HANDLE thread = CreateThread(NULL, 0, kain_fanout_worker_entry, runtime, 0, NULL);
        if (thread == NULL) {
            runtime->thread_count = index;
            runtime->shutting_down = 1;
            kain_fanout_runtime_broadcast(&runtime->work_ready);
            for (int join_index = 0; join_index < runtime->thread_count; ++join_index) {
                WaitForSingleObject(runtime->threads[join_index], INFINITE);
                CloseHandle(runtime->threads[join_index]);
                runtime->threads[join_index] = NULL;
            }
            DeleteCriticalSection(&runtime->lock);
            memset(runtime, 0, sizeof(*runtime));
            return -1;
        }
        runtime->threads[index] = thread;
    }
#else
    if (pthread_mutex_init(&runtime->lock, NULL) != 0) {
        memset(runtime, 0, sizeof(*runtime));
        return -1;
    }
    if (pthread_cond_init(&runtime->work_ready, NULL) != 0) {
        pthread_mutex_destroy(&runtime->lock);
        memset(runtime, 0, sizeof(*runtime));
        return -1;
    }
    if (pthread_cond_init(&runtime->work_done, NULL) != 0) {
        pthread_cond_destroy(&runtime->work_ready);
        pthread_mutex_destroy(&runtime->lock);
        memset(runtime, 0, sizeof(*runtime));
        return -1;
    }
    for (int index = 0; index < runtime->thread_count; ++index) {
        if (pthread_create(&runtime->threads[index], NULL, kain_fanout_worker_entry, runtime) != 0) {
            runtime->thread_count = index;
            runtime->shutting_down = 1;
            kain_fanout_runtime_broadcast(&runtime->work_ready);
            for (int join_index = 0; join_index < runtime->thread_count; ++join_index) {
                pthread_join(runtime->threads[join_index], NULL);
            }
            pthread_cond_destroy(&runtime->work_done);
            pthread_cond_destroy(&runtime->work_ready);
            pthread_mutex_destroy(&runtime->lock);
            memset(runtime, 0, sizeof(*runtime));
            return -1;
        }
    }
#endif

    runtime->initialized = 1;
    return 0;
}

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
    atomic_init(&job.next_index, (long long)start);
    job.end = end;
    job.ctx = ctx;
    job.fn = fn;

    if (kain_fanout_runtime_init() != 0) {
        return -1;
    }

    KainFanoutRuntime* runtime = &g_kain_fanout_runtime;
    int helper_count = worker_count - 1;
    if (helper_count > runtime->thread_count) {
        helper_count = runtime->thread_count;
    }

    kain_fanout_runtime_lock(runtime);
    while (runtime->current_job != NULL && !runtime->shutting_down) {
        kain_fanout_runtime_wait(&runtime->work_done, runtime);
    }
    if (runtime->shutting_down) {
        kain_fanout_runtime_unlock(runtime);
        return -1;
    }
    runtime->current_job = &job;
    runtime->active_workers = helper_count;
    runtime->completed_workers = 0;
    runtime->generation += 1;
    if (helper_count > 0) {
        kain_fanout_runtime_broadcast(&runtime->work_ready);
    }
    kain_fanout_runtime_unlock(runtime);

    kain_fanout_drain_job(&job);

    kain_fanout_runtime_lock(runtime);
    while (runtime->completed_workers < runtime->active_workers) {
        kain_fanout_runtime_wait(&runtime->work_done, runtime);
    }
    runtime->current_job = NULL;
    runtime->active_workers = 0;
    runtime->completed_workers = 0;
    kain_fanout_runtime_broadcast(&runtime->work_done);
    kain_fanout_runtime_unlock(runtime);

    return 0;
}

void kain_fanout_runtime_shutdown(void) {
    KainFanoutRuntime* runtime = &g_kain_fanout_runtime;
    if (!runtime->initialized) {
        return;
    }

    kain_fanout_runtime_lock(runtime);
    runtime->shutting_down = 1;
    kain_fanout_runtime_broadcast(&runtime->work_ready);
    kain_fanout_runtime_broadcast(&runtime->work_done);
    kain_fanout_runtime_unlock(runtime);

#ifdef _WIN32
    for (int index = 0; index < runtime->thread_count; ++index) {
        WaitForSingleObject(runtime->threads[index], INFINITE);
        CloseHandle(runtime->threads[index]);
        runtime->threads[index] = NULL;
    }
    DeleteCriticalSection(&runtime->lock);
#else
    for (int index = 0; index < runtime->thread_count; ++index) {
        pthread_join(runtime->threads[index], NULL);
    }
    pthread_cond_destroy(&runtime->work_done);
    pthread_cond_destroy(&runtime->work_ready);
    pthread_mutex_destroy(&runtime->lock);
#endif

    memset(runtime, 0, sizeof(*runtime));
}
