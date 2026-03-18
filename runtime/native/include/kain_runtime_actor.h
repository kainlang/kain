#ifndef KAIN_RUNTIME_ACTOR_H
#define KAIN_RUNTIME_ACTOR_H

#include "kain_runtime_base.h"
#include "kain_runtime_diagnostics.h"
#include <stddef.h>

/*
 * KAIN Native Runtime Actor ABI
 *
 * This header defines the canonical actor runtime ABI for the KAIN native
 * runtime. It provides declarations for actor lifecycle, mailbox operations,
 * supervision, monitoring, registry, and scheduler integration.
 *
 * Actor Model Features:
 * - Actor spawn and bootstrap
 * - Mailbox-based message passing with backpressure
 * - Actor identity and typed message metadata
 * - Supervision trees (restart, shutdown, escalation)
 * - Monitors and links for failure propagation
 * - Actor registry for named actors/services
 * - Scheduler integration for fairness and blocking
 */

/* Actor ID Type */
typedef unsigned long long KainActorId;

#define KAIN_ACTOR_ID_INVALID 0

/* Actor State */
typedef enum {
    KAIN_ACTOR_STATE_UNINITIALIZED = 0,
    KAIN_ACTOR_STATE_INITIALIZING,
    KAIN_ACTOR_STATE_RUNNING,
    KAIN_ACTOR_STATE_SUSPENDED,
    KAIN_ACTOR_STATE_SHUTTING_DOWN,
    KAIN_ACTOR_STATE_TERMINATED,
    KAIN_ACTOR_STATE_FAILED,
} KainActorState;

/* Actor Exit Reason */
typedef enum {
    KAIN_ACTOR_EXIT_NORMAL = 0,
    KAIN_ACTOR_EXIT_SHUTDOWN,
    KAIN_ACTOR_EXIT_KILLED,
    KAIN_ACTOR_EXIT_CRASHED,
    KAIN_ACTOR_EXIT_SUPERVISOR_ESCALATION,
} KainActorExitReason;

/* Supervision Strategy */
typedef enum {
    KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE = 0,
    KAIN_SUPERVISION_STRATEGY_ONE_FOR_ALL,
    KAIN_SUPERVISION_STRATEGY_REST_FOR_ONE,
} KainSupervisionStrategy;

/* Restart Policy */
typedef enum {
    KAIN_RESTART_POLICY_PERMANENT = 0,  /* Always restart */
    KAIN_RESTART_POLICY_TEMPORARY,      /* Never restart */
    KAIN_RESTART_POLICY_TRANSIENT,      /* Restart only on abnormal exit */
} KainRestartPolicy;

/* Mailbox Configuration */
#define KAIN_MAILBOX_DEFAULT_CAPACITY 1024
#define KAIN_MAILBOX_UNBOUNDED_CAPACITY 0

/* String Buffer Sizes */
#define KAIN_ACTOR_NAME_MAX 128

/*
 * Actor Message
 *
 * Represents a message in an actor's mailbox. Messages carry type metadata
 * and payload data.
 */
typedef struct {
    unsigned long long type_tag;
    void* data;
    size_t data_size;
    KainActorId sender_id;
} KainActorMessage;

/*
 * Actor Mailbox
 *
 * Message queue for an actor. Supports bounded capacity and backpressure.
 * Opaque structure - implementation details in runtime core.
 */
typedef struct KainActorMailbox KainActorMailbox;

/*
 * Actor Handle
 *
 * Opaque handle to an actor. Used for sending messages, monitoring, linking.
 */
typedef struct KainActorHandle KainActorHandle;

/*
 * Actor Bootstrap Function
 *
 * Entry point for actor execution. Called by the runtime when an actor starts.
 * The actor should process messages from its mailbox and perform its work.
 *
 * Parameters:
 *   actor_id - The ID of this actor
 *   mailbox - The actor's mailbox for receiving messages
 *   user_data - User-provided data passed during spawn
 *
 * Returns:
 *   Exit reason for the actor
 */
typedef KainActorExitReason (*KainActorBootstrapFn)(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
);

/*
 * Actor Spawn Configuration
 *
 * Configuration for spawning a new actor.
 */
typedef struct {
    KainActorBootstrapFn bootstrap_fn;
    void* user_data;
    size_t mailbox_capacity;
    KainRestartPolicy restart_policy;
    KainActorId supervisor_id;
    char name[KAIN_ACTOR_NAME_MAX];
} KainActorSpawnConfig;

/*
 * Initialize Actor Spawn Configuration
 *
 * Sets default values for spawn configuration.
 */
void kain_actor_spawn_config_init(KainActorSpawnConfig* config);

/*
 * Spawn Actor
 *
 * Creates and starts a new actor. Returns the actor ID on success,
 * KAIN_ACTOR_ID_INVALID on failure. Populates diagnostic on error.
 */
KainActorId kain_actor_spawn(
    const KainActorSpawnConfig* config,
    KainDiagnostic* diag
);

/*
 * Send Message to Actor
 *
 * Sends a message to an actor's mailbox. Returns 0 on success, non-zero on
 * failure (e.g., mailbox full, actor not found). Populates diagnostic on error.
 */
int kain_actor_send(
    KainActorId target_id,
    const KainActorMessage* message,
    KainDiagnostic* diag
);

/*
 * Receive Message from Mailbox
 *
 * Receives the next message from the actor's mailbox. Blocks if no messages
 * are available. Returns 0 on success, non-zero on error.
 */
int kain_actor_receive(
    KainActorMailbox* mailbox,
    KainActorMessage* message,
    KainDiagnostic* diag
);

/*
 * Try Receive Message (Non-blocking)
 *
 * Attempts to receive a message without blocking. Returns 0 if a message was
 * received, 1 if mailbox is empty, negative on error.
 */
int kain_actor_try_receive(
    KainActorMailbox* mailbox,
    KainActorMessage* message,
    KainDiagnostic* diag
);

/*
 * Shutdown Actor
 *
 * Requests graceful shutdown of an actor. Returns 0 on success, non-zero on error.
 */
int kain_actor_shutdown(
    KainActorId actor_id,
    KainDiagnostic* diag
);

/*
 * Kill Actor
 *
 * Forcefully terminates an actor. Returns 0 on success, non-zero on error.
 */
int kain_actor_kill(
    KainActorId actor_id,
    KainDiagnostic* diag
);

/*
 * Get Actor State
 *
 * Returns the current state of an actor.
 */
KainActorState kain_actor_get_state(KainActorId actor_id);

/*
 * Monitor Actor
 *
 * Registers a monitor relationship. The monitoring actor will receive a
 * notification when the monitored actor exits. Returns 0 on success.
 */
int kain_actor_monitor(
    KainActorId monitor_id,
    KainActorId monitored_id,
    KainDiagnostic* diag
);

/*
 * Link Actors
 *
 * Creates a bidirectional link between two actors. If either actor exits
 * abnormally, the other will be terminated. Returns 0 on success.
 */
int kain_actor_link(
    KainActorId actor_a,
    KainActorId actor_b,
    KainDiagnostic* diag
);

/*
 * Unlink Actors
 *
 * Removes a link between two actors. Returns 0 on success.
 */
int kain_actor_unlink(
    KainActorId actor_a,
    KainActorId actor_b,
    KainDiagnostic* diag
);

/*
 * Register Named Actor
 *
 * Registers an actor with a name in the actor registry. Returns 0 on success,
 * non-zero if the name is already registered or on error.
 */
int kain_actor_registry_register(
    const char* name,
    KainActorId actor_id,
    KainDiagnostic* diag
);

/*
 * Lookup Named Actor
 *
 * Looks up an actor by name in the registry. Returns the actor ID on success,
 * KAIN_ACTOR_ID_INVALID if not found.
 */
KainActorId kain_actor_registry_lookup(const char* name);

/*
 * Unregister Named Actor
 *
 * Removes an actor from the registry. Returns 0 on success.
 */
int kain_actor_registry_unregister(
    const char* name,
    KainDiagnostic* diag
);

/*
 * Get Mailbox Message Count
 *
 * Returns the number of messages currently in the mailbox.
 */
size_t kain_actor_mailbox_count(const KainActorMailbox* mailbox);

/*
 * Get Mailbox Capacity
 *
 * Returns the maximum capacity of the mailbox.
 */
size_t kain_actor_mailbox_capacity(const KainActorMailbox* mailbox);

/*
 * Check if Mailbox is Full
 *
 * Returns 1 if the mailbox is at capacity, 0 otherwise.
 */
int kain_actor_mailbox_is_full(const KainActorMailbox* mailbox);

#endif /* KAIN_RUNTIME_ACTOR_H */
