#ifndef KAIN_BATCH_QUEUE_H
#define KAIN_BATCH_QUEUE_H

#include "base.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t kind;
    uint64_t arg0;
    uint64_t arg1;
    void* ptr0;
} KainBatchQueueEntry;

typedef void (*KainBatchQueueDrainFn)(
    const KainBatchQueueEntry* entry,
    void* user_data
);

typedef struct {
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    KainBatchQueueEntry* active_entries;
    KainBatchQueueEntry* pending_entries;
    size_t capacity;
    size_t active_head;
    size_t active_count;
    size_t pending_count;
    unsigned int hold_depth;
    int initialized;
} KainBatchQueue;

void kain_batch_queue_init(
    KainBatchQueue* queue,
    KainBatchQueueEntry* active_entries,
    KainBatchQueueEntry* pending_entries,
    size_t capacity
);

void kain_batch_queue_lock(KainBatchQueue* queue);

int kain_batch_queue_enqueue(
    KainBatchQueue* queue,
    const KainBatchQueueEntry* entry
);

void kain_batch_queue_unlock_and_drain(
    KainBatchQueue* queue,
    KainBatchQueueDrainFn drain_fn,
    void* user_data
);

#endif /* KAIN_BATCH_QUEUE_H */
