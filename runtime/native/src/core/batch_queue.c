#include "../../include/batch_queue.h"
#include <string.h>

static void kain_batch_queue_mutex_init(KainBatchQueue* queue) {
#ifdef _WIN32
    InitializeCriticalSection(&queue->lock);
#else
    pthread_mutex_init(&queue->lock, NULL);
#endif
}

static void kain_batch_queue_mutex_lock(KainBatchQueue* queue) {
#ifdef _WIN32
    EnterCriticalSection(&queue->lock);
#else
    pthread_mutex_lock(&queue->lock);
#endif
}

static void kain_batch_queue_mutex_unlock(KainBatchQueue* queue) {
#ifdef _WIN32
    LeaveCriticalSection(&queue->lock);
#else
    pthread_mutex_unlock(&queue->lock);
#endif
}

void kain_batch_queue_init(
    KainBatchQueue* queue,
    KainBatchQueueEntry* active_entries,
    KainBatchQueueEntry* pending_entries,
    size_t capacity
) {
    if (queue == NULL || active_entries == NULL || pending_entries == NULL || capacity == 0u) {
        return;
    }

    memset(queue, 0, sizeof(*queue));
    kain_batch_queue_mutex_init(queue);
    queue->active_entries = active_entries;
    queue->pending_entries = pending_entries;
    queue->capacity = capacity;
    queue->initialized = 1;
}

void kain_batch_queue_lock(KainBatchQueue* queue) {
    if (queue == NULL || !queue->initialized) {
        return;
    }

    kain_batch_queue_mutex_lock(queue);
    queue->hold_depth += 1u;
    kain_batch_queue_mutex_unlock(queue);
}

int kain_batch_queue_enqueue(
    KainBatchQueue* queue,
    const KainBatchQueueEntry* entry
) {
    KainBatchQueueEntry* target_entries;
    size_t* target_count;

    if (queue == NULL || entry == NULL || !queue->initialized) {
        return -1;
    }

    kain_batch_queue_mutex_lock(queue);
    if (queue->hold_depth != 0u) {
        target_entries = queue->pending_entries;
        target_count = &queue->pending_count;
    } else {
        target_entries = queue->active_entries;
        target_count = &queue->active_count;
    }

    if (*target_count >= queue->capacity) {
        kain_batch_queue_mutex_unlock(queue);
        return -1;
    }

    target_entries[*target_count] = *entry;
    *target_count += 1u;
    kain_batch_queue_mutex_unlock(queue);
    return 0;
}

void kain_batch_queue_unlock_and_drain(
    KainBatchQueue* queue,
    KainBatchQueueDrainFn drain_fn,
    void* user_data
) {
    if (queue == NULL || !queue->initialized) {
        return;
    }

    for (;;) {
        KainBatchQueueEntry entry;
        int have_entry = 0;

        kain_batch_queue_mutex_lock(queue);
        if (queue->hold_depth != 0u) {
            queue->hold_depth -= 1u;
            if (queue->hold_depth != 0u) {
                kain_batch_queue_mutex_unlock(queue);
                return;
            }
        }

        if (queue->active_head >= queue->active_count) {
            if (queue->pending_count != 0u) {
                memcpy(
                    queue->active_entries,
                    queue->pending_entries,
                    queue->pending_count * sizeof(KainBatchQueueEntry)
                );
                queue->active_head = 0u;
                queue->active_count = queue->pending_count;
                queue->pending_count = 0u;
            } else {
                queue->active_head = 0u;
                queue->active_count = 0u;
            }
        }

        if (queue->active_head < queue->active_count) {
            entry = queue->active_entries[queue->active_head];
            queue->active_head += 1u;
            if (queue->active_head >= queue->active_count) {
                queue->active_head = 0u;
                queue->active_count = 0u;
            }
            have_entry = 1;
        }
        kain_batch_queue_mutex_unlock(queue);

        if (!have_entry) {
            return;
        }

        if (drain_fn != NULL) {
            drain_fn(&entry, user_data);
        }
    }
}
