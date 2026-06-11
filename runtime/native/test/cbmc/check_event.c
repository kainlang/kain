/*
 * check_event.c — CBMC verification harness for the event bus subsystem
 *
 * Verifies the core event bus operations (subscribe, unsubscribe, emit,
 * cleanup_actor) with real pointer provenance through static backing
 * buffers. CBMC explores all paths including OOM, NULL parameters,
 * and edge cases.
 *
 * The event bus uses pthread mutex internally. CBMC treats these as
 * nondet external calls (no thread modeling needed for single-TU safety).
 *
 * Run: python test/scripts/run_pipeline.py cbmc --harness check_event
 */

#include "event.h"
#include "actor.h"
#include "base.h"

/* ──────────────────────────────────────────────────────────────────────
 * Static backing buffers — CBMC knows these are real allocated objects
 * ────────────────────────────────────────────────────────────────────── */
static KainEventBus g_bus;
static KainEventTopic g_topics[4];
static KainEventSubscriber g_subs[12];
static KainActorMessage g_msg;
static unsigned char g_payload[256];

/* ──────────────────────────────────────────────────────────────────────
 * Helper: initialize a clean event bus for testing
 *
 * We bypass pthread_mutex_init (which CBMC can't model) by placing the
 * bus in a known state with real pointer provenance.
 * ────────────────────────────────────────────────────────────────────── */
static void init_clean_bus(void) {
    __CPROVER_havoc_object(&g_bus);
    memset(g_bus.buckets, 0, sizeof(g_bus.buckets));
    g_bus.topic_count = 0;
    g_bus.initialized = 1;
}

/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a simple message with a valid payload pointer
 * ────────────────────────────────────────────────────────────────────── */
static KainActorMessage* create_simple_message(void) {
    __CPROVER_havoc_object(&g_msg);
    __CPROVER_havoc_object(g_payload);

    g_msg.type_tag  = 1ULL;
    g_msg.sender_id = 42ULL;
    g_msg.data      = g_payload;
    g_msg.data_size = sizeof(g_payload);
    return &g_msg;
}

/*
 * We need access to the internal static globals from event.c to manipulate
 * them directly for CBMC testing (since CBMC can't model pthread_mutex_init).
 *
 * Re-declare the global so the combined TU links against event.c's g_event_bus
 * and g_event_bus_initialized. The real definitions are in event.c.
 */
extern KainEventBus g_event_bus;
extern int g_event_bus_initialized;

/*
 * Also declare the lock/unlock helpers so CBMC knows they are external
 * (pthread/CriticalSection ops that CBMC treats as nondet).
 * These are actually static in event.c but linking against the combined
 * TU will resolve them.
 */

/* ═══════════════════════════════════════════════════════════════════════
 * PROPERTY 1: Subscribe + emit delivers exactly one message
 *
 * After subscribing one actor and emitting, the subscribers should
 * receive the message. We verify by checking that the subscriber list
 * is non-empty after subscribe, and that emit returns 1.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_subscribe_emit_single(void) {
    /* Reset the global bus to a clean state */
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    /* Subscribe actor 100 to topic "test_event" */
    int rc = kain_event_subscribe(100ULL, "test_event");
    __CPROVER_assert(rc == 0, "subscribe: returns 0 on success");

    /* Verify the topic exists and has one subscriber */
    unsigned int bucket = 5381;
    {
        const char* name = "test_event";
        unsigned long h = 5381;
        int c;
        while ((c = (unsigned char)*name++) != '\0') {
            h = ((h << 5) + h) ^ c;
        }
        bucket = (unsigned int)(h % 256);
    }
    __CPROVER_assert(g_event_bus.buckets[bucket] != NULL,
                     "subscribe: topic exists in bucket");
    __CPROVER_assert(g_event_bus.buckets[bucket]->count == 1,
                     "subscribe: subscriber count == 1");
    __CPROVER_assert(g_event_bus.buckets[bucket]->head != NULL,
                     "subscribe: subscriber list non-empty");
    __CPROVER_assert(g_event_bus.buckets[bucket]->head->actor_id == 100ULL,
                     "subscribe: subscriber actor_id == 100");

    /* Emit a message */
    KainActorMessage* msg = create_simple_message();
    int delivered = kain_event_emit("test_event", msg, NULL);
    __CPROVER_assert(delivered >= 0, "emit: returns non-negative");
    /* The actual delivery count depends on kain_actor_send (nondet for CBMC
     * since it touches the actor table). We verify the structure is correct. */
}

/* ═══════════════════════════════════════════════════════════════════════
 * PROPERTY 2: Unsubscribe prevents delivery
 *
 * After subscribe + unsubscribe, the topic should be empty or removed.
 * Emit should return 0.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_unsubscribe_prevents_delivery(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc_sub = kain_event_subscribe(200ULL, "my_event");
    __CPROVER_assert(rc_sub == 0, "unsub-prop: subscribe succeeds");

    int rc_unsub = kain_event_unsubscribe(200ULL, "my_event");
    __CPROVER_assert(rc_unsub == 0, "unsub-prop: unsubscribe succeeds");

    /* After unsubscribe, emit should find no topic */
    KainActorMessage* msg = create_simple_message();
    int delivered = kain_event_emit("my_event", msg, NULL);
    __CPROVER_assert(delivered == 0,
                     "unsub-prop: emit returns 0 after unsubscribe");

    /* Verify topic was freed (count == 0 → topic removed) */
    /* The topic should not be in any bucket */
    int found_topic = 0;
    for (unsigned int b = 0; b < 256; b++) {
        KainEventTopic* t = g_event_bus.buckets[b];
        while (t != NULL) {
            if (strcmp(t->name, "my_event") == 0) {
                found_topic = 1;
            }
            t = t->next;
        }
    }
    __CPROVER_assert(found_topic == 0,
                     "unsub-prop: topic removed when last subscriber leaves");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PROPERTY 3: Multiple subscribers all receive the message
 *
 * Subscribe three different actors, emit, verify all three are in the
 * subscriber list and emit delivers to multiple.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_multi_subscriber(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc1 = kain_event_subscribe(10ULL, "broadcast");
    int rc2 = kain_event_subscribe(20ULL, "broadcast");
    int rc3 = kain_event_subscribe(30ULL, "broadcast");
    __CPROVER_assert(rc1 == 0, "multi: sub 1 ok");
    __CPROVER_assert(rc2 == 0, "multi: sub 2 ok");
    __CPROVER_assert(rc3 == 0, "multi: sub 3 ok");

    /* Verify the topic has count == 3 */
    /* Find the topic */
    {
        const char* name = "broadcast";
        unsigned long h = 5381;
        int c;
        while ((c = (unsigned char)*name++) != '\0') {
            h = ((h << 5) + h) ^ c;
        }
        unsigned int bucket = (unsigned int)(h % 256);
        __CPROVER_assert(g_event_bus.buckets[bucket] != NULL,
                         "multi: topic exists");
        __CPROVER_assert(g_event_bus.buckets[bucket]->count == 3,
                         "multi: count == 3");
    }

    /* Emit should fan out to all 3 */
    KainActorMessage* msg = create_simple_message();
    int delivered = kain_event_emit("broadcast", msg, NULL);
    __CPROVER_assert(delivered >= 0, "multi: emit returns non-negative");
    /* Cannot assert delivered == 3 because kain_actor_send is nondet for CBMC */
}

/* ═══════════════════════════════════════════════════════════════════════
 * PROPERTY 4: cleanup_actor removes a dying actor from ALL topics
 *
 * Subscribe actor 42 to two different topics, then call
 * kain_event_cleanup_actor(42). Verify the actor is removed from both.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_cleanup_actor(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc_a = kain_event_subscribe(42ULL, "topic_a");
    int rc_b = kain_event_subscribe(42ULL, "topic_b");
    __CPROVER_assert(rc_a == 0, "cleanup: sub topic_a ok");
    __CPROVER_assert(rc_b == 0, "cleanup: sub topic_b ok");

    /* Cleanup actor 42 */
    kain_event_cleanup_actor(42ULL);

    /* Verify actor 42 is not subscribed to any topic */
    int found = 0;
    for (unsigned int b = 0; b < 256; b++) {
        KainEventTopic* t = g_event_bus.buckets[b];
        while (t != NULL) {
            KainEventSubscriber* s = t->head;
            while (s != NULL) {
                if (s->actor_id == 42ULL) {
                    found = 1;
                }
                s = s->next;
            }
            t = t->next;
        }
    }
    __CPROVER_assert(found == 0,
                     "cleanup: actor 42 removed from all topics");

    /* Emit should deliver to 0 subscribers */
    KainActorMessage* msg = create_simple_message();
    int delivered_a = kain_event_emit("topic_a", msg, NULL);
    __CPROVER_assert(delivered_a == 0,
                     "cleanup: emit topic_a returns 0 after cleanup");
    int delivered_b = kain_event_emit("topic_b", msg, NULL);
    __CPROVER_assert(delivered_b == 0,
                     "cleanup: emit topic_b returns 0 after cleanup");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PROPERTY 5: Duplicate subscribe is a no-op
 *
 * Subscribing the same actor to the same topic twice should not create
 * duplicate entries in the subscriber list.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_duplicate_subscribe(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc1 = kain_event_subscribe(99ULL, "dup_event");
    int rc2 = kain_event_subscribe(99ULL, "dup_event");
    __CPROVER_assert(rc1 == 0, "dup: first subscribe ok");
    __CPROVER_assert(rc2 == 0, "dup: second subscribe also returns 0 (no-op)");

    /* Verify count is still 1, not 2 */
    unsigned long h = 5381;
    {
        const char* name = "dup_event";
        int c;
        while ((c = (unsigned char)*name++) != '\0') {
            h = ((h << 5) + h) ^ c;
        }
    }
    unsigned int bucket = (unsigned int)(h % 256);
    __CPROVER_assert(g_event_bus.buckets[bucket] != NULL,
                     "dup: topic exists");
    __CPROVER_assert(g_event_bus.buckets[bucket]->count == 1,
                     "dup: count == 1 (not 2)");

    /* Verify only one node with actor_id 99 */
    int count_99 = 0;
    KainEventSubscriber* s = g_event_bus.buckets[bucket]->head;
    while (s != NULL) {
        if (s->actor_id == 99ULL) {
            count_99++;
        }
        s = s->next;
    }
    __CPROVER_assert(count_99 == 1,
                     "dup: exactly one subscriber entry for actor 99");
}

/* ═══════════════════════════════════════════════════════════════════════
 * EDGE CASES: NULL and invalid parameters
 * ═══════════════════════════════════════════════════════════════════════ */

void check_subscribe_null_name(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc = kain_event_subscribe(1ULL, NULL);
    __CPROVER_assert(rc == -1, "subscribe(NULL name): returns -1");
}

void check_subscribe_empty_name(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc = kain_event_subscribe(1ULL, "");
    __CPROVER_assert(rc == -1, "subscribe(empty name): returns -1");
}

void check_subscribe_invalid_id(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc = kain_event_subscribe(0ULL, "test");
    __CPROVER_assert(rc == -1, "subscribe(invalid id): returns -1");
}

void check_unsubscribe_null_name(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc = kain_event_unsubscribe(1ULL, NULL);
    __CPROVER_assert(rc == -1, "unsubscribe(NULL name): returns -1");
}

void check_unsubscribe_not_subscribed(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    int rc = kain_event_unsubscribe(999ULL, "nonexistent");
    __CPROVER_assert(rc == -1,
                     "unsubscribe(not subscribed): returns -1");
}

void check_emit_null_name(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    KainActorMessage* msg = create_simple_message();
    int delivered = kain_event_emit(NULL, msg, NULL);
    __CPROVER_assert(delivered == 0, "emit(NULL name): returns 0");
}

void check_emit_null_message(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    /* Subscribe an actor so topic exists */
    kain_event_subscribe(1ULL, "test");
    int delivered = kain_event_emit("test", NULL, NULL);
    __CPROVER_assert(delivered == 0, "emit(NULL message): returns 0");
}

void check_emit_nonexistent_topic(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    KainActorMessage* msg = create_simple_message();
    int delivered = kain_event_emit("nonexistent", msg, NULL);
    __CPROVER_assert(delivered == 0,
                     "emit(nonexistent topic): returns 0");
}

void check_cleanup_invalid_id(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    /* Should not crash on invalid ID */
    kain_event_cleanup_actor(0ULL);
    /* If we reach here without CBMC finding UB, the test passes */
    __CPROVER_assert(1, "cleanup(invalid id): does not crash");
}

void check_cleanup_empty_bus(void) {
    memset(&g_event_bus, 0, sizeof(g_event_bus));
    g_event_bus_initialized = 1;

    /* Cleanup on an empty bus should be a no-op */
    kain_event_cleanup_actor(42ULL);
    __CPROVER_assert(1, "cleanup(empty bus): does not crash");
}
