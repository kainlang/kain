/*
 * check_actor.c — CBMC verification harness for the actor mailbox subsystem
 *
 * Verifies the core message queue operations (enqueue/try_receive) with
 * real pointer provenance through static backing buffers. CBMC explores
 * all paths including malloc failure, full mailbox, closed mailbox.
 *
 * The mailbox uses pthread mutex/cond internally. CBMC treats these as
 * nondet external calls (no thread modeling needed for single-TU safety).
 *
 * Run: python test/scripts/run_pipeline.py cbmc --harness check_actor
 */

#include "actor.h"
#include "base.h"

/* ──────────────────────────────────────────────────────────────────────
 * Static backing buffers — CBMC knows these are real allocated objects
 * ────────────────────────────────────────────────────────────────────── */
static KainActorMailbox g_mbox;
static KainActorMessage g_msg;
static unsigned char g_payload[256];
static MessageNode g_node_pool[8];

/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a clean empty mailbox with valid backing
 *
 * We fill the struct with nondet bytes, then override fields to
 * produce a valid initial state. This exercises real code paths
 * while giving CBMC valid pointer provenance.
 * ────────────────────────────────────────────────────────────────────── */
static KainActorMailbox* create_empty_mailbox(size_t capacity) {
    KainActorMailbox* m = &g_mbox;
    __CPROVER_havoc_object(m);

    /* Linked list: empty initially */
    m->head       = NULL;
    m->tail       = NULL;
    m->free_nodes = NULL;
    m->capacity   = capacity;
    m->count      = 0;
    m->free_node_count = 0;
    m->closed     = 0;

    /* No inline message pending */
    m->inline_message_pending = 0;
    m->inline_message_borrowed = 0;
    m->inline_message_data     = NULL;
    m->inline_message_size     = 0;
    m->inline_message_type_tag = 0ULL;
    m->inline_message_sender_id = 0ULL;

    return m;
}


/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a message struct with a valid payload pointer
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


/* ═══════════════════════════════════════════════════════════════════════
 * ACCESSOR TESTS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Simple null-safe accessors — these always succeed for any input */
void check_count_null(void) {
    size_t c = kain_actor_mailbox_count(NULL);
    __CPROVER_assert(c == 0, "mailbox_count(NULL) == 0");
}

void check_capacity_null(void) {
    size_t c = kain_actor_mailbox_capacity(NULL);
    __CPROVER_assert(c == 0, "mailbox_capacity(NULL) == 0");
}

void check_is_full_null(void) {
    int f = kain_actor_mailbox_is_full(NULL);
    __CPROVER_assert(f == 0, "mailbox_is_full(NULL) == 0");
}

void check_count_returns_field(void) {
    KainActorMailbox* m = create_empty_mailbox(1024);
    m->count = 42;
    size_t c = kain_actor_mailbox_count(m);
    __CPROVER_assert(c == 42, "mailbox_count returns count field");
}

void check_capacity_returns_field(void) {
    KainActorMailbox* m = create_empty_mailbox(2048);
    size_t c = kain_actor_mailbox_capacity(m);
    __CPROVER_assert(c == 2048, "mailbox_capacity returns capacity field");
}

void check_is_full_when_at_capacity(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    m->count = 100;
    int f = kain_actor_mailbox_is_full(m);
    __CPROVER_assert(f == 1, "is_full == 1 when count >= capacity");
}

void check_is_full_when_below_capacity(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    m->count = 50;
    int f = kain_actor_mailbox_is_full(m);
    __CPROVER_assert(f == 0, "is_full == 0 when count < capacity");
}

void check_is_full_when_unbounded(void) {
    KainActorMailbox* m = create_empty_mailbox(0); /* unbounded */
    m->count = 999999;
    int f = kain_actor_mailbox_is_full(m);
    __CPROVER_assert(f == 0, "unbounded mailbox never full");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ENQUEUE TESTS  (tests static function kain_actor_mailbox_append_copied_locked)
 *
 * Forward-declare the static enqueue function from actor.c — since the
 * harness and source form one translation unit, we can call it directly.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Forward declaration of static function from actor.c */
static int kain_actor_mailbox_append_copied_locked(
    KainActorMailbox* mailbox,
    const KainActorMessage* message
);

void check_enqueue_empty(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    KainActorMessage *msg = create_simple_message();

    int rc = kain_actor_mailbox_append_copied_locked(m, msg);

    if (rc == 0) {
        __CPROVER_assert(m->count == 1, "enqueue empty: count == 1");
        __CPROVER_assert(m->head != NULL, "enqueue empty: head != NULL");
        __CPROVER_assert(m->tail != NULL, "enqueue empty: tail != NULL");
        __CPROVER_assert(m->head == m->tail, "enqueue empty: head == tail (single node)");
        __CPROVER_assert(m->head->next == NULL, "enqueue empty: head->next == NULL");
        /* Message data copied (not same pointer) */
        __CPROVER_assert(m->head->type_tag == msg->type_tag,
                         "enqueue: type_tag preserved");
        __CPROVER_assert(m->head->sender_id == msg->sender_id,
                         "enqueue: sender_id preserved");
        __CPROVER_assert(m->head->data_size == msg->data_size,
                         "enqueue: data_size preserved");
    } else {
        /* OOM or other error — count unchanged */
        __CPROVER_assert(m->count == 0,
                         "enqueue fail: count unchanged");
    }
}

void check_enqueue_twice(void) {
    KainActorMailbox* m = create_empty_mailbox(100);

    /* Create two copies of the message (distinct payloads) */
    g_payload[0] = 0xAA;
    KainActorMessage msg1;
    __CPROVER_havoc_object(&msg1);
    msg1.type_tag  = 1ULL;
    msg1.sender_id = 10ULL;
    msg1.data      = &g_payload[0];
    msg1.data_size = 1;

    unsigned char data2[32];
    __CPROVER_havoc_object(data2);
    KainActorMessage msg2;
    __CPROVER_havoc_object(&msg2);
    msg2.type_tag  = 2ULL;
    msg2.sender_id = 20ULL;
    msg2.data      = data2;
    msg2.data_size = sizeof(data2);

    int rc1 = kain_actor_mailbox_append_copied_locked(m, &msg1);
    int rc2 = kain_actor_mailbox_append_copied_locked(m, &msg2);

    if (rc1 == 0 && rc2 == 0) {
        __CPROVER_assert(m->count == 2, "enqueue twice: count == 2");
        __CPROVER_assert(m->head != NULL, "enqueue twice: head != NULL");
        __CPROVER_assert(m->tail != NULL, "enqueue twice: tail != NULL");
        __CPROVER_assert(m->head != m->tail, "enqueue twice: head != tail (two nodes)");
        __CPROVER_assert(m->head->next != NULL, "enqueue twice: head->next != NULL");
        __CPROVER_assert(m->head->next->next == NULL,
                         "enqueue twice: tail->next == NULL");
        __CPROVER_assert(m->head->type_tag == 1ULL,
                         "enqueue twice: first preserved (FIFO)");
        __CPROVER_assert(m->tail->type_tag == 2ULL,
                         "enqueue twice: second is tail (FIFO)");
    }
}

void check_enqueue_full(void) {
    KainActorMailbox* m = create_empty_mailbox(3);

    KainActorMessage msgs[3];
    unsigned char payloads[3][16];
    int all_ok = 1;
    for (int i = 0; i < 3; i++) {
        __CPROVER_havoc_object(&msgs[i]);
        __CPROVER_havoc_object(payloads[i]);
        msgs[i].type_tag  = (unsigned long long)(i + 1);
        msgs[i].sender_id = (unsigned long long)i;
        msgs[i].data      = payloads[i];
        msgs[i].data_size = sizeof(payloads[i]);
    }

    /* Fill the mailbox */
    for (int i = 0; i < 3; i++) {
        if (kain_actor_mailbox_append_copied_locked(m, &msgs[i]) != 0) {
            all_ok = 0;
        }
    }

    __CPROVER_assume(all_ok); /* all three fit */
    __CPROVER_assert(m->count == 3, "full: count == 3");

    /* Fourth should be rejected */
    int rc4 = kain_actor_mailbox_append_copied_locked(m, &msgs[0]);
    __CPROVER_assert(rc4 == -3, "full: rejected with -3 (capacity)");
    __CPROVER_assert(m->count == 3, "full: count still 3 after rejection");

    /* Also try via the public accessor */
    int full = kain_actor_mailbox_is_full(m);
    __CPROVER_assert(full == 1, "full: is_full returns 1");
}

void check_enqueue_closed(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    KainActorMessage* msg = create_simple_message();
    m->closed = 1;

    int rc = kain_actor_mailbox_append_copied_locked(m, msg);
    __CPROVER_assert(rc == -2, "closed: returns -2");
    __CPROVER_assert(m->count == 0, "closed: count unchanged");
    __CPROVER_assert(m->head == NULL, "closed: head still NULL");
}

void check_enqueue_null_mailbox(void) {
    int rc = kain_actor_mailbox_append_copied_locked(NULL, &g_msg);
    __CPROVER_assert(rc == -1, "null mailbox: returns -1");
}

void check_enqueue_null_message(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    int rc = kain_actor_mailbox_append_copied_locked(m, NULL);
    __CPROVER_assert(rc == -1, "null message: returns -1");
}

/* ═══════════════════════════════════════════════════════════════════════
 * TRY_RECEIVE TESTS  (calls thread-safe public API, CBMC treats
 * pthread mutex/cond as nondet externals)
 * ═══════════════════════════════════════════════════════════════════════ */

void check_try_receive_empty(void) {
    KainActorMailbox* m = create_empty_mailbox(100);

    KainActorMessage out;
    __CPROVER_havoc_object(&out);

    int rc = kain_actor_try_receive(m, &out, NULL);
    __CPROVER_assert(rc == 1, "try_receive empty: returns 1 (empty)");
}

void check_try_receive_null_mailbox(void) {
    KainActorMessage out;
    int rc = kain_actor_try_receive(NULL, &out, NULL);
    __CPROVER_assert(rc == -1, "try_receive(NULL,...): returns -1");
}

void check_try_receive_null_message(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    int rc = kain_actor_try_receive(m, NULL, NULL);
    __CPROVER_assert(rc == -1, "try_receive(...,NULL): returns -1");
}

void check_try_receive_single(void) {
    KainActorMailbox* m = create_empty_mailbox(100);
    KainActorMessage* msg = create_simple_message();

    /* Manually insert a single node into the mailbox's linked list.
     * This bypasses the OS-dependent init while testing real dequeue. */
    MessageNode* node = &g_node_pool[0];
    node->type_tag  = msg->type_tag;
    node->sender_id = msg->sender_id;
    node->data      = g_payload;    /* reuse same payload buffer */
    node->data_size = msg->data_size;
    node->next      = NULL;

    m->head  = node;
    m->tail  = node;
    m->count = 1;

    KainActorMessage out;
    __CPROVER_havoc_object(&out);

    /* The inline-message check checks inline_message_pending first.
     * Since we set it to 0, it passes through to the real dequeue. */
    int rc = kain_actor_try_receive(m, &out, NULL);

    if (rc == 0) {
        __CPROVER_assert(out.type_tag  == 1ULL,
                         "try_receive: type_tag correct");
        __CPROVER_assert(out.sender_id == 42ULL,
                         "try_receive: sender_id correct");
        __CPROVER_assert(out.data_size == sizeof(g_payload),
                         "try_receive: data_size correct");
    }
}

void check_try_receive_fifo(void) {
    /* Enqueue two, dequeue both — verify FIFO order */
    KainActorMailbox* m = create_empty_mailbox(100);

    /* Set up two messages with manual nodes */
    unsigned char payload_a[8];
    unsigned char payload_b[8];
    __CPROVER_havoc_object(payload_a);
    __CPROVER_havoc_object(payload_b);

    MessageNode* node_a = &g_node_pool[0];
    MessageNode* node_b = &g_node_pool[1];
    node_a->type_tag  = 10ULL;  node_a->sender_id = 1;
    node_a->data      = payload_a;  node_a->data_size = 4;
    node_a->next      = node_b;

    node_b->type_tag  = 20ULL;  node_b->sender_id = 2;
    node_b->data      = payload_b;  node_b->data_size = 8;
    node_b->next      = NULL;

    m->head  = node_a;
    m->tail  = node_b;
    m->count = 2;

    KainActorMessage out1, out2;
    __CPROVER_havoc_object(&out1);
    __CPROVER_havoc_object(&out2);

    int rc1 = kain_actor_try_receive(m, &out1, NULL);
    int rc2 = kain_actor_try_receive(m, &out2, NULL);

    if (rc1 == 0 && rc2 == 0) {
        __CPROVER_assert(out1.type_tag == 10ULL,
                         "FIFO: first dequeued is first enqueued");
        __CPROVER_assert(out2.type_tag == 20ULL,
                         "FIFO: second dequeued is second enqueued");
        __CPROVER_assert(m->count == 0, "FIFO: count == 0 after both dequeued");
        __CPROVER_assert(m->head == NULL, "FIFO: head == NULL after both dequeued");
        __CPROVER_assert(m->tail == NULL, "FIFO: tail == NULL after both dequeued");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * NULL-HANDLING TESTS
 * ═══════════════════════════════════════════════════════════════════════ */

void check_spawn_config_init_null(void) {
    /* Should not crash on NULL */
    kain_actor_spawn_config_init(NULL);
}

void check_spawn_config_init_ok(void) {
    KainActorSpawnConfig cfg;
    __CPROVER_havoc_object(&cfg);

    kain_actor_spawn_config_init(&cfg);

    __CPROVER_assert(cfg.mailbox_capacity == KAIN_MAILBOX_DEFAULT_CAPACITY,
                     "spawn_config: default mailbox capacity");
    __CPROVER_assert(cfg.supervision_strategy
                     == KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE,
                     "spawn_config: default supervision strategy");
    __CPROVER_assert(cfg.restart_policy == KAIN_RESTART_POLICY_TEMPORARY,
                     "spawn_config: default restart policy");
}


/* ═══════════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════════ */

int main(void) {
    /* Accessors */
    check_count_null();
    check_capacity_null();
    check_is_full_null();
    check_count_returns_field();
    check_capacity_returns_field();
    check_is_full_when_at_capacity();
    check_is_full_when_below_capacity();
    check_is_full_when_unbounded();

    /* Enqueue */
    check_enqueue_empty();
    check_enqueue_twice();
    check_enqueue_full();
    check_enqueue_closed();
    check_enqueue_null_mailbox();
    check_enqueue_null_message();

    /* Try receive */
    check_try_receive_empty();
    check_try_receive_null_mailbox();
    check_try_receive_null_message();
    check_try_receive_single();
    check_try_receive_fifo();

    /* Config */
    check_spawn_config_init_null();
    check_spawn_config_init_ok();

    return 0;
}
