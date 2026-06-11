#ifndef KAIN_EVENT_BUS_H
#define KAIN_EVENT_BUS_H

#include "actor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KAIN Native Runtime Event Bus ABI
 *
 * A topic-based pub/sub subscriber registry that enables first-class
 * event broadcast (`emit EventName(...)`) from the Kain language surface.
 * Actors subscribe to named event topics and receive fan-out deliveries
 * through their existing mailbox infrastructure.
 *
 * Design:
 * - Hash table with 256 buckets, chained collision resolution
 * - Single pthread_mutex_t for thread safety
 * - Snapshot subscriber list before unlock → iterate outside lock
 * - Auto-cleanup on actor termination via kain_event_cleanup_actor
 * - Duplicate subscribe is a no-op
 */

#define KAIN_EVENT_BUS_BUCKETS 256
#define KAIN_EVENT_TOPIC_NAME_MAX 64

/*
 * Event Bus Error Codes (reuse ACTOR_BASE range, 3000-3999)
 * 3013 and 3014 are free in the current diagnostics table.
 */
#define KAIN_DIAG_CODE_EVENT_BUS_SUBSCRIBE_FAILED  (KAIN_DIAG_CODE_ACTOR_BASE + 13)
#define KAIN_DIAG_CODE_EVENT_BUS_EMIT_FAILED       (KAIN_DIAG_CODE_ACTOR_BASE + 14)

/*
 * Event Subscriber Node
 *
 * A linked-list node representing one actor subscribed to a topic.
 *
 * OWNERSHIP:
 * - Owned by the KainEventTopic's subscriber list
 * - Allocated on subscribe, freed on unsubscribe or cleanup
 *
 * LIFETIME:
 * - Lives until the actor unsubscribes or terminates
 */
typedef struct KainEventSubscriber {
    KainActorId actor_id;
    struct KainEventSubscriber* next;
} KainEventSubscriber;

/*
 * Event Topic
 *
 * Represents a named event topic with its subscriber list.
 * Topics are nodes in a hash-table collision chain.
 *
 * OWNERSHIP:
 * - Owned by the KainEventBus hash table
 * - Created on first subscribe, destroyed when last subscriber removed
 */
typedef struct KainEventTopic {
    char name[KAIN_EVENT_TOPIC_NAME_MAX];
    KainEventSubscriber* head;
    unsigned int count;
    struct KainEventTopic* next;   /* collision chain */
} KainEventTopic;

/*
 * Event Bus
 *
 * Global singleton registry mapping event names to subscriber lists.
 * Thread-safe via a single mutex.
 *
 * OWNERSHIP:
 * - Owned by the runtime process (singleton)
 * - Initialized once during kain_event_bus_init()
 */
typedef struct {
    KainEventTopic* buckets[KAIN_EVENT_BUS_BUCKETS];
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    unsigned int topic_count;
} KainEventBus;

/*
 * Initialize the global event bus.
 *
 * Must be called once at runtime startup before any event operations.
 * Idempotent: subsequent calls are no-ops.
 */
void kain_event_bus_init(void);

/*
 * Subscribe an actor to an event topic.
 *
 * If the topic does not exist, it is created. If the actor is already
 * subscribed to the topic, the call is a no-op (returns 0).
 *
 * Returns 0 on success, -1 on error (OOM, invalid parameters).
 */
int kain_event_subscribe(KainActorId actor_id, const char* event_name);

/*
 * Unsubscribe an actor from an event topic.
 *
 * If the actor is the last subscriber, the topic is freed.
 *
 * Returns 0 on success, -1 if the actor was not subscribed to the topic.
 */
int kain_event_unsubscribe(KainActorId actor_id, const char* event_name);

/*
 * Emit an event to all subscribers of a topic.
 *
 * Takes a snapshot of the subscriber list under lock, then iterates
 * outside the lock and delivers the message to each subscriber's mailbox
 * via kain_actor_send(). Individual delivery failures (e.g., mailbox
 * closed, actor dead) are silently ignored — the emit continues to the
 * next subscriber.
 *
 * If the topic does not exist, returns 0 (no subscribers to deliver to).
 *
 * Returns the number of successful deliveries.
 */
int kain_event_emit(const char* event_name, const KainActorMessage* message, KainDiagnostic* diag);

/*
 * Remove all subscriptions for a terminating actor.
 *
 * Walks all buckets and topics to find and remove any subscriber nodes
 * matching the given actor_id. Called automatically during actor teardown
 * from kain_actor_complete_exit_side_effects().
 *
 * Safe to call with an invalid or zero actor_id — does nothing.
 */
void kain_event_cleanup_actor(KainActorId actor_id);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_EVENT_BUS_H */
