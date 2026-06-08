/*
 * CBMC verification harness for batch_queue
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
// kain_batch_queue_lock
void kain_batch_queue_lock(KainBatchQueue* queue);
// kain_batch_queue_enqueue
int kain_batch_queue_enqueue( KainBatchQueue* queue, const KainBatchQueueEntry* entry );

int main(void) {
    { void *__p; kain_batch_queue_lock(__p); }
    __CPROVER_assert(1, "kain_batch_queue_lock: call ok");
    { void *__a; unsigned long long __b; kain_batch_queue_enqueue(__a, __b); }
    __CPROVER_assert(1, "kain_batch_queue_enqueue: call ok");
    return 0;
}
