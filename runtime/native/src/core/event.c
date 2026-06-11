/*
 * event.c — Kain Native Runtime Event Bus
 *
 * Topic-based pub/sub subscriber registry for the `emit` keyword.
 * Provides subscribe, unsubscribe, emit, and cleanup operations
 * with thread-safe access via a single mutex.
 *
 * Design:
 * - DJB2 hash over event name → 256-bucket hash table
 * - Chained collision resolution (linked list of KainEventTopic)
 * - Snapshot subscriber list under lock, iterate outside lock
 * - Duplicate subscribe detection
 * - Auto-cleanup on actor termination
 *
 * Verification:
 * - CBMC harness: runtime/native/test/cbmc/check_event.c
 *   (5 properties: subscribe+emit, unsubscribe, multi-subscriber,
 *    cleanup_actor, duplicate subscribe)
 */

#include "../../include/event.h"
#include "../../include/actor.h"
#include "../../include/base.h"
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────
 * Global Event Bus Singleton
 * ────────────────────────────────────────────────────────────────────── */
static KainEventBus g_event_bus;
static int g_event_bus_initialized = 0;

/* ──────────────────────────────────────────────────────────────────────
 * DJB2 Hash
 *
 * Classic DJB2 string hash (xor variant) used for topic bucket lookup.
 * Returns a bucket index in [0, KAIN_EVENT_BUS_BUCKETS).
 * ────────────────────────────────────────────────────────────────────── */
static unsigned int kain_event_hash(const char* name) {
    unsigned long hash = 5381;
    int c;
    if (name == NULL) {
        return 0;
    }
    while ((c = (unsigned char)*name++) != '\0') {
        hash = ((hash << 5) + hash) ^ c;  /* hash * 33 ^ c */
    }
    return (unsigned int)(hash % KAIN_EVENT_BUS_BUCKETS);
}

/* ──────────────────────────────────────────────────────────────────────
 * Lock / Unlock Helpers
 * ────────────────────────────────────────────────────────────────────── */
static void kain_event_bus_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_event_bus.lock);
#else
    pthread_mutex_lock(&g_event_bus.lock);
#endif
}

static void kain_event_bus_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_event_bus.lock);
#else
    pthread_mutex_unlock(&g_event_bus.lock);
#endif
}

/* ──────────────────────────────────────────────────────────────────────
 * Initialize Event Bus
 *
 * Idempotent: only initializes the first time it's called.
 * Must be thread-safe during startup — called from
 * kain_actor_runtime_init().
 * ────────────────────────────────────────────────────────────────────── */
void kain_event_bus_init(void) {
    if (g_event_bus_initialized) {
        return;
    }

#ifdef _WIN32
    InitializeCriticalSection(&g_event_bus.lock);
#else
    pthread_mutex_init(&g_event_bus.lock, NULL);
#endif

    memset(g_event_bus.buckets, 0, sizeof(g_event_bus.buckets));
    g_event_bus.topic_count = 0;
    g_event_bus_initialized = 1;
}

/* ──────────────────────────────────────────────────────────────────────
 * Subscribe an Actor to an Event Topic
 *
 * Finds or creates the topic for event_name, then appends the actor_id
 * to the subscriber list. Checks for duplicate subscriptions — if the
 * actor is already subscribed, returns 0 (no-op).
 *
 * Returns 0 on success, -1 on error (OOM, NULL event_name).
 * ────────────────────────────────────────────────────────────────────── */
int kain_event_subscribe(KainActorId actor_id, const char* event_name) {
    KainEventSubscriber* sub;
    KainEventSubscriber* existing;
    KainEventTopic* topic;
    unsigned int bucket;

    if (event_name == NULL || event_name[0] == '\0') {
        return -1;
    }
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        return -1;
    }

    bucket = kain_event_hash(event_name);
    kain_event_bus_lock();

    /* Find or create the topic */
    topic = g_event_bus.buckets[bucket];
    while (topic != NULL) {
        if (strncmp(topic->name, event_name, KAIN_EVENT_TOPIC_NAME_MAX - 1) == 0) {
            break;
        }
        topic = topic->next;
    }

    if (topic == NULL) {
        /* Create new topic */
        topic = (KainEventTopic*)calloc(1, sizeof(KainEventTopic));
        if (topic == NULL) {
            kain_event_bus_unlock();
            return -1;
        }
        strncpy_s(topic->name, KAIN_EVENT_TOPIC_NAME_MAX, event_name, _TRUNCATE);
        topic->name[KAIN_EVENT_TOPIC_NAME_MAX - 1] = '\0';
        topic->head = NULL;
        topic->count = 0;
        topic->next = g_event_bus.buckets[bucket];
        g_event_bus.buckets[bucket] = topic;
        g_event_bus.topic_count++;
    }

    /* Check for duplicate subscription */
    existing = topic->head;
    while (existing != NULL) {
        if (existing->actor_id == actor_id) {
            /* Already subscribed — no-op */
            kain_event_bus_unlock();
            return 0;
        }
        existing = existing->next;
    }

    /* Create and prepend subscriber node */
    sub = (KainEventSubscriber*)malloc(sizeof(KainEventSubscriber));
    if (sub == NULL) {
        kain_event_bus_unlock();
        return -1;
    }
    sub->actor_id = actor_id;
    sub->next = topic->head;
    topic->head = sub;
    topic->count++;

    kain_event_bus_unlock();
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────
 * Unsubscribe an Actor from an Event Topic
 *
 * Removes the actor from the subscriber list. If the actor is the last
 * subscriber, the topic is freed and removed from the bucket chain.
 *
 * Returns 0 on success, -1 if the actor was not subscribed.
 * ────────────────────────────────────────────────────────────────────── */
int kain_event_unsubscribe(KainActorId actor_id, const char* event_name) {
    KainEventTopic* topic;
    KainEventTopic* prev_topic;
    KainEventSubscriber* sub;
    KainEventSubscriber* prev_sub;
    unsigned int bucket;

    if (event_name == NULL || event_name[0] == '\0') {
        return -1;
    }
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        return -1;
    }

    bucket = kain_event_hash(event_name);
    kain_event_bus_lock();

    /* Find the topic in the bucket chain */
    prev_topic = NULL;
    topic = g_event_bus.buckets[bucket];
    while (topic != NULL) {
        if (strncmp(topic->name, event_name, KAIN_EVENT_TOPIC_NAME_MAX - 1) == 0) {
            break;
        }
        prev_topic = topic;
        topic = topic->next;
    }

    if (topic == NULL) {
        kain_event_bus_unlock();
        return -1;
    }

    /* Find and remove the subscriber */
    prev_sub = NULL;
    sub = topic->head;
    while (sub != NULL) {
        if (sub->actor_id == actor_id) {
            if (prev_sub == NULL) {
                topic->head = sub->next;
            } else {
                prev_sub->next = sub->next;
            }
            free(sub);
            topic->count--;

            /* If last subscriber, free the topic */
            if (topic->count == 0) {
                if (prev_topic == NULL) {
                    g_event_bus.buckets[bucket] = topic->next;
                } else {
                    prev_topic->next = topic->next;
                }
                g_event_bus.topic_count--;
                free(topic);
            }

            kain_event_bus_unlock();
            return 0;
        }
        prev_sub = sub;
        sub = sub->next;
    }

    /* Not found */
    kain_event_bus_unlock();
    return -1;
}

/* ──────────────────────────────────────────────────────────────────────
 * Emit an Event to All Subscribers
 *
 * Algorithm:
 * 1. Hash event_name → bucket
 * 2. Lock bus mutex
 * 3. Find the topic in the bucket chain
 * 4. Snapshot subscriber list (pointer copy of head + count)
 * 5. Unlock bus mutex
 * 6. For each subscriber, deliver via kain_actor_send()
 *
 * Individual delivery failures (mailbox closed, actor dead) are
 * silently ignored — the emit fans out to all remaining subscribers.
 *
 * Returns the number of successful deliveries.
 * ────────────────────────────────────────────────────────────────────── */
int kain_event_emit(const char* event_name, const KainActorMessage* message, KainDiagnostic* diag) {
    KainEventTopic* topic;
    KainEventSubscriber* sub;
    KainEventSubscriber* head_snapshot;
    unsigned int count_snapshot;
    unsigned int bucket;
    int delivered = 0;

    if (event_name == NULL || event_name[0] == '\0') {
        return 0;
    }
    if (message == NULL) {
        return 0;
    }

    bucket = kain_event_hash(event_name);
    kain_event_bus_lock();

    /* Find the topic */
    topic = g_event_bus.buckets[bucket];
    while (topic != NULL) {
        if (strncmp(topic->name, event_name, KAIN_EVENT_TOPIC_NAME_MAX - 1) == 0) {
            break;
        }
        topic = topic->next;
    }

    if (topic == NULL || topic->head == NULL) {
        kain_event_bus_unlock();
        return 0;
    }

    /* Snapshot the subscriber list */
    head_snapshot = topic->head;
    count_snapshot = topic->count;

    kain_event_bus_unlock();

    /* Iterate outside lock — subscriber nodes are stable until removed */
    sub = head_snapshot;
    while (sub != NULL && count_snapshot > 0) {
        int rc = kain_actor_send(sub->actor_id, message, diag);
        if (rc == 0) {
            delivered++;
        }
        /* Silently ignore failures: mailbox closed (-2), actor dead (-4), etc. */
        sub = sub->next;
        count_snapshot--;
    }

    return delivered;
}

/* ──────────────────────────────────────────────────────────────────────
 * Cleanup All Subscriptions for a Terminating Actor
 *
 * Walks all buckets and all topics, removing any subscriber nodes
 * that match the given actor_id. If a topic becomes empty, it is
 * freed and removed from the bucket chain.
 *
 * Called automatically from kain_actor_complete_exit_side_effects()
 * during actor teardown.
 * ────────────────────────────────────────────────────────────────────── */
void kain_event_cleanup_actor(KainActorId actor_id) {
    unsigned int b;
    KainEventTopic* topic;
    KainEventTopic* prev_topic;
    KainEventTopic* next_topic;
    KainEventSubscriber* sub;
    KainEventSubscriber* prev_sub;
    KainEventSubscriber* next_sub;

    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        return;
    }

    kain_event_bus_lock();

    for (b = 0; b < KAIN_EVENT_BUS_BUCKETS; b++) {
        prev_topic = NULL;
        topic = g_event_bus.buckets[b];
        while (topic != NULL) {
            next_topic = topic->next;

            /* Walk subscriber list for this topic */
            prev_sub = NULL;
            sub = topic->head;
            while (sub != NULL) {
                next_sub = sub->next;
                if (sub->actor_id == actor_id) {
                    /* Remove this subscriber */
                    if (prev_sub == NULL) {
                        topic->head = next_sub;
                    } else {
                        prev_sub->next = next_sub;
                    }
                    free(sub);
                    topic->count--;
                } else {
                    prev_sub = sub;
                }
                sub = next_sub;
            }

            /* If topic is now empty, free it */
            if (topic->count == 0) {
                if (prev_topic == NULL) {
                    g_event_bus.buckets[b] = next_topic;
                } else {
                    prev_topic->next = next_topic;
                }
                g_event_bus.topic_count--;
                free(topic);
            } else {
                prev_topic = topic;
            }

            topic = next_topic;
        }
    }

    kain_event_bus_unlock();
}
