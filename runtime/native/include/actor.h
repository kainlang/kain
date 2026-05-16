#ifndef ACTOR_H
#define ACTOR_H

#include "base.h"
#include "diagnostics.h"
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations from base.h */
typedef struct MessageNode MessageNode;

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

#define KAIN_ACTOR_ABI_VERSION 3U
#define KAIN_ACTOR_ID_BITS 64U
#define KAIN_ACTOR_ID_INVALID 0ULL
#define KAIN_ACTOR_REF_GENERATION_BITS 32U

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

/* Actor Execution Class */
typedef enum {
    KAIN_ACTOR_EXECUTION_CLASS_INVALID = 0,
    KAIN_ACTOR_EXECUTION_CLASS_MICROCELL = 1,
    KAIN_ACTOR_EXECUTION_CLASS_WORLDCELL = 2,
    KAIN_ACTOR_EXECUTION_CLASS_NETCELL = 3,
    KAIN_ACTOR_EXECUTION_CLASS_HOSTCELL = 4,
    KAIN_ACTOR_EXECUTION_CLASS_ACCELERATOR_CELL = 5,
    KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT = 6,
} KainActorExecutionClass;

/* Actor Locality Class */
typedef enum {
    KAIN_ACTOR_LOCALITY_INVALID = 0,
    KAIN_ACTOR_LOCALITY_LOCAL = 1,
    KAIN_ACTOR_LOCALITY_WORLD_AFFINE = 2,
    KAIN_ACTOR_LOCALITY_HOST_AFFINE = 3,
    KAIN_ACTOR_LOCALITY_REMOTE = 4,
} KainActorLocalityClass;

/*
 * Actor Reference
 *
 * Generation-tagged actor reference that makes execution class and locality
 * first-class runtime truth instead of assuming every actor handle is a raw
 * local mailbox slot.
 */
typedef struct {
    KainActorId actor_id;
    unsigned int generation;
    unsigned int execution_class;
    unsigned int locality_class;
} KainActorRef;

/* Supervision Configuration */
#define KAIN_SUPERVISION_MAX_RESTARTS 5
#define KAIN_SUPERVISION_RESTART_WINDOW_SECONDS 60
#define KAIN_SUPERVISION_RESTART_WINDOW_MILLIS 60000ULL

/* Mailbox Configuration */
#define KAIN_MAILBOX_DEFAULT_CAPACITY 1024
#define KAIN_MAILBOX_UNBOUNDED_CAPACITY 0
#define KAIN_ACTOR_DEFAULT_ASK_TIMEOUT_MS 30000ULL
#define KAIN_ACTOR_DEFAULT_SHUTDOWN_GRACE_MS 5000ULL
#define KAIN_ACTOR_DEFAULT_MICROCELL_TURN_BUDGET 64U

/* String Buffer Sizes */
#define KAIN_ACTOR_NAME_MAX 128

/* Runtime Capacity Configuration */
#define KAIN_ACTOR_TABLE_CAPACITY 1024
#define KAIN_ACTOR_REGISTRY_CAPACITY 256
#define KAIN_ACTOR_SCHEDULER_WORKER_COUNT 4

/* Monitor Notification Message Tag Base */
#define KAIN_ACTOR_MONITOR_EXIT_TAG_BASE 0xDEAD0000ULL

/*
 * Native Actor ABI Descriptor
 *
 * This is the runtime-owned contract that Rust model crates, LLVM lowering,
 * direct C lowering, stdlib wrappers, and C++ consumers can use to assert they
 * are targeting the same native actor floor.
 */
typedef struct {
    unsigned int abi_version;
    unsigned short actor_id_bits;
    unsigned short actor_ref_generation_bits;
    KainActorId invalid_actor_id;
    unsigned int default_execution_class;
    unsigned int default_locality_class;
    unsigned int synthetic_reply_port_execution_class;
    unsigned int synthetic_reply_port_locality_class;
    size_t default_mailbox_capacity;
    size_t unbounded_mailbox_capacity;
    unsigned long long default_ask_timeout_ms;
    unsigned long long default_shutdown_grace_ms;
    unsigned int supervision_max_restarts;
    unsigned long long supervision_restart_window_millis;
    size_t actor_name_max;
    size_t scheduler_worker_count;
    size_t actor_table_capacity;
    size_t registry_capacity;
    unsigned long long monitor_exit_tag_base;
    unsigned int default_microcell_turn_budget;
} KainActorAbiDescriptor;

/*
 * Actor Message
 *
 * Represents a message in an actor's mailbox. Messages carry type metadata
 * and payload data.
 *
 * Special type tags:
 * - 0xDEAD0000 + exit_reason: Monitor notification (exit reason in lower bits)
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
 *
 * OWNERSHIP:
 * - Owned by the actor's runtime state
 * - Created during actor spawn, destroyed during actor termination
 * - Thread-safe: multiple senders can send to the same mailbox concurrently
 * - Only the owning actor can receive from its mailbox
 *
 * LIFETIME:
 * - Lives from actor spawn until actor termination completes
 * - Messages in the mailbox are freed during mailbox destruction
 * - Senders must handle mailbox-full backpressure or blocking
 */
typedef struct KainActorMailbox {
    /* Message queue storage */
    MessageNode* head;
    MessageNode* tail;
    MessageNode* free_nodes;

    /* Capacity and backpressure */
    size_t capacity;        /* 0 = unbounded, >0 = bounded */
    size_t count;           /* Current message count */
    size_t free_node_count;

    /* Synchronization */
#ifdef _WIN32
    CRITICAL_SECTION lock;
    HANDLE not_empty;       /* Signaled when messages available */
    HANDLE not_full;        /* Signaled when space available */
#else
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif

    /* State flags */
    int closed;             /* 1 if mailbox is closed (actor shutting down) */
} KainActorMailbox;

/*
 * Actor Monitor Reference
 *
 * Represents a monitor relationship between two actors.
 *
 * OWNERSHIP:
 * - Owned by the monitoring actor's state
 * - Created during kain_actor_monitor(), destroyed when monitor is removed
 *   or when either actor terminates
 *
 * LIFETIME:
 * - Lives until explicitly removed or either actor terminates
 * - When monitored actor exits, a notification message is sent to monitor
 */
typedef struct KainActorMonitor {
    KainActorId monitor_id;     /* Actor doing the monitoring */
    KainActorId monitored_id;   /* Actor being monitored */
    struct KainActorMonitor* next;
} KainActorMonitor;

/*
 * Actor Link Reference
 *
 * Represents a bidirectional link between two actors.
 *
 * OWNERSHIP:
 * - Shared between both linked actors' states
 * - Created during kain_actor_link(), destroyed during kain_actor_unlink()
 *   or when either actor terminates
 *
 * LIFETIME:
 * - Lives until explicitly unlinked or either actor terminates
 * - When one actor exits abnormally, the other is terminated
 */
typedef struct KainActorLink {
    KainActorId actor_a;
    KainActorId actor_b;
    struct KainActorLink* next;
} KainActorLink;

/*
 * Actor Supervisor Reference
 *
 * Represents the supervisor relationship for an actor.
 *
 * OWNERSHIP:
 * - Owned by the child actor's state
 * - Set during actor spawn if supervisor_id is provided
 *
 * LIFETIME:
 * - Lives for the lifetime of the child actor
 * - Used to notify supervisor of child exit and apply restart policy
 */
typedef struct {
    KainActorId supervisor_id;
    KainSupervisionStrategy strategy;
    KainRestartPolicy restart_policy;
    int restart_count;
    time_t last_restart_time;
    time_t restart_window_start;
    KainActorExitReason last_child_exit_reason;
    int restart_limit_hit;
} KainActorSupervisor;

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

typedef enum {
    KAIN_ACTOR_ENTRY_KIND_INVALID = 0,
    KAIN_ACTOR_ENTRY_KIND_LEGACY_BOOTSTRAP = 1,
    KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN = 2,
} KainActorEntryKind;

typedef enum {
    KAIN_ACTOR_TURN_IDLE = 0,
    KAIN_ACTOR_TURN_YIELDED = 1,
    KAIN_ACTOR_TURN_STOPPED = 2,
    KAIN_ACTOR_TURN_CRASHED = 3,
} KainActorTurnStatus;

typedef KainActorTurnStatus (*KainActorTurnFn)(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data,
    unsigned int budget
);

/*
 * Actor Spawn Configuration (stored for restart)
 *
 * Stored configuration needed to restart an actor.
 */
typedef struct {
    KainActorBootstrapFn bootstrap_fn;
    /* Retained by the actor runtime for the actor's lifetime. The compiler
     * owns this RC-managed state object and the runtime keeps one reference
     * while the actor is alive. */
    void* user_data;
    size_t mailbox_capacity;
    KainSupervisionStrategy supervision_strategy;
    KainRestartPolicy restart_policy;
    KainActorId supervisor_id;
    /* 1 when user_data is a Kain RC allocation the runtime must retain/release. */
    int retain_user_data;
    char name[KAIN_ACTOR_NAME_MAX];
    KainActorEntryKind entry_kind;
    KainActorTurnFn turn_fn;
    unsigned int microcell_turn_budget;
    unsigned int execution_class;
    unsigned int locality_class;
} KainActorSpawnConfigStored;

/*
 * Actor Scheduler Queue Node
 *
 * Represents an actor in the scheduler's ready queue.
 *
 * OWNERSHIP:
 * - Owned by the scheduler
 * - Created when actor becomes runnable, destroyed when scheduled
 *
 * LIFETIME:
 * - Transient: exists only while actor is in ready queue
 * - Actor may be enqueued/dequeued multiple times during its lifetime
 */
typedef struct KainActorSchedulerNode {
    KainActorId actor_id;
    struct KainActorSchedulerNode* next;
} KainActorSchedulerNode;

/*
 * Actor State Record
 *
 * Complete runtime state for an actor instance.
 *
 * OWNERSHIP:
 * - Owned by the actor runtime system
 * - Created during kain_actor_spawn(), destroyed during actor termination cleanup
 * - Referenced by actor ID in global actor table
 *
 * LIFETIME:
 * - Lives from spawn until termination cleanup completes
 * - State transitions: UNINITIALIZED -> INITIALIZING -> RUNNING ->
 *   (SUSPENDED) -> SHUTTING_DOWN -> TERMINATED/FAILED
 * - All owned resources (mailbox, monitors, links) are cleaned up during termination
 *
 * THREAD SAFETY:
 * - Actor state is protected by the global actor table lock
 * - Mailbox has its own synchronization for concurrent sends
 * - Bootstrap function executes on actor's dedicated thread
 */
typedef struct {
    /* Identity */
    KainActorId actor_id;
    unsigned int ref_generation;
    unsigned int execution_class;
    unsigned int locality_class;
    char name[KAIN_ACTOR_NAME_MAX];
    unsigned long long spawn_sequence;

    /* State and lifecycle */
    KainActorState state;
    KainActorExitReason exit_reason;

    /* Execution context */
    KainActorEntryKind entry_kind;
    KainActorBootstrapFn bootstrap_fn;
    KainActorTurnFn turn_fn;
    unsigned int microcell_turn_budget;
    void* user_data;

#ifdef _WIN32
    HANDLE thread_handle;
    DWORD thread_id;
#else
    pthread_t thread;
#endif
    int direct_thread_started;

    /* Mailbox */
    KainActorMailbox mailbox;

    /* Supervision */
    KainActorSupervisor supervisor;
    KainActorSpawnConfigStored spawn_config;  /* Stored for restart */

    /* Monitors and links */
    KainActorMonitor* monitors;     /* List of actors this actor monitors */
    KainActorLink* links;           /* List of links involving this actor */

    /* Supervision observations */
    size_t observed_child_exit_count;
    KainActorId last_observed_child_id;
    KainActorExitReason last_observed_child_exit_reason;
    int supervision_limit_hits;
    size_t restart_attempt_count;
    size_t strategy_shutdown_count;
    size_t escalation_count;

    /* Scheduler integration */
    int in_scheduler_queue;         /* 1 if currently in ready queue */
    int in_scheduler_turn;          /* 1 if a scheduler-owned microcell turn is in flight */

    /* Diagnostics */
    KainDiagnostic last_error;
} KainActorState_Internal;

/*
 * Actor Handle
 *
 * Opaque handle to an actor. Used for sending messages, monitoring, linking.
 *
 * OWNERSHIP:
 * - Lightweight reference to an actor by ID
 * - Does not own the actor state
 * - Multiple handles can reference the same actor
 *
 * LIFETIME:
 * - Handle is valid as long as the actor exists
 * - Operations on invalid handles return KAIN_ACTOR_NOT_FOUND errors
 */
typedef struct KainActorHandle KainActorHandle;

typedef struct {
    KainActorId supervisor_id;
    KainSupervisionStrategy strategy;
    KainRestartPolicy restart_policy;
    int restart_count;
    time_t last_restart_time;
    time_t restart_window_start;
    KainActorExitReason last_child_exit_reason;
    int restart_limit_hit;
    size_t observed_child_exit_count;
    KainActorId last_observed_child_id;
    KainActorExitReason last_observed_child_exit_reason;
    int supervision_limit_hits;
    size_t restart_attempt_count;
    size_t strategy_shutdown_count;
    size_t escalation_count;
} KainActorSupervisionSnapshot;

typedef struct {
    size_t queue_depth;
    size_t max_queue_depth;
    size_t total_enqueued;
    size_t total_dequeued;
    int worker_count;
    int active_workers;
    size_t busy_workers;
    size_t max_busy_workers;
    size_t overflow_thread_spawns;
    int shutdown;
} KainActorSchedulerSnapshot;

/*
 * Actor Spawn Configuration
 *
 * Configuration for spawning a new actor.
 */
typedef struct {
    KainActorBootstrapFn bootstrap_fn;
    void* user_data;
    size_t mailbox_capacity;
    KainSupervisionStrategy supervision_strategy;
    KainRestartPolicy restart_policy;
    KainActorId supervisor_id;
    /* 1 when user_data is a Kain RC allocation the runtime must retain/release. */
    int retain_user_data;
    char name[KAIN_ACTOR_NAME_MAX];
    KainActorEntryKind entry_kind;
    KainActorTurnFn turn_fn;
    unsigned int microcell_turn_budget;
    unsigned int execution_class;
    unsigned int locality_class;
} KainActorSpawnConfig;

/*
 * Initialize Actor Runtime
 *
 * Must be called before any actor operations.
 */
void kain_actor_runtime_init(void);

/*
 * Return the native actor ABI descriptor.
 */
KainActorAbiDescriptor kain_actor_abi_descriptor(void);

/*
 * Check whether an expected descriptor exactly matches this runtime.
 * Returns 1 for compatible, 0 for incompatible.
 */
int kain_actor_abi_descriptor_is_compatible(const KainActorAbiDescriptor* expected);

/*
 * Fill a generation-tagged actor reference from a raw actor ID.
 *
 * If the actor is not live, out_ref is zeroed to an invalid reference.
 */
void kain_actor_ref_from_id(KainActorId actor_id, KainActorRef* out_ref);

/*
 * Validate that an actor reference still names the same live actor generation,
 * execution class, and locality.
 */
int kain_actor_ref_is_live(const KainActorRef* actor_ref);

/*
 * Shutdown Actor Runtime
 *
 * Terminates all actors and cleans up resources.
 */
void kain_actor_runtime_shutdown(void);

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
 * Ask Send by Exact Actor Ref
 *
 * Compiler-lowered local asks use the full generation-tagged actor ref so the
 * runtime can reject stale handles and opportunistically run the first local
 * microcell turn inline when the target mailbox is empty and not already owned
 * by the scheduler. If the fast path is not legal, this falls back to normal
 * mailbox enqueue semantics.
 */
int kain_actor_ask_send_ref(
    const KainActorRef* target_ref,
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
 * Native LLVM reply-port bridge.
 *
 * These helpers back compiler-lowered `ask` / `ask_timeout` roundtrips. A reply
 * port is a synthetic generation-tagged actor ref that accepts the first reply
 * payload and lets the compiler wait or cancel without inventing a second
 * mailbox ABI or a useless waiting OS thread. The native runtime keeps a
 * per-thread reply port hot: successful roundtrips rearm the same synthetic
 * actor by bumping its generation, while destroy/timeout invalidates the live
 * ref and returns the state to the TLS cache for the next ask.
 */
void* kain_actor_reply_port_new(void);
KainActorId kain_actor_reply_port_actor_id(void* reply_port_handle);
void kain_actor_reply_port_actor_ref(void* reply_port_handle, KainActorRef* out_ref);
void kain_actor_reply_port_destroy(void* reply_port_handle);
int kain_actor_reply_port_send(
    KainActorId reply_port_actor_id,
    const void* reply_data,
    size_t reply_size
);
int kain_actor_reply_port_send_ref(
    const KainActorRef* reply_port_ref,
    const void* reply_data,
    size_t reply_size
);
int kain_actor_reply_port_wait(
    void* reply_port_handle,
    long long timeout_ms,
    void* out_reply_data,
    size_t out_reply_capacity,
    size_t* out_reply_size
);
long long kain_actor_reply_port_wait_i64(void* reply_port_handle, long long timeout_ms);

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
 * Get Supervision Snapshot
 *
 * Returns the current supervision state for an actor.
 */
int kain_actor_get_supervision_snapshot(
    KainActorId actor_id,
    KainActorSupervisionSnapshot* snapshot,
    KainDiagnostic* diag
);

/*
 * Monitor Actor
 *
 * Registers a monitor relationship. The monitoring actor will receive a
 * notification message when the monitored actor exits. The notification
 * message has type_tag = 0xDEAD0000 | exit_reason, where exit_reason
 * is the KainActorExitReason value.
 *
 * Monitor semantics:
 * - Monitors are unidirectional (A monitors B, but B doesn't monitor A)
 * - Multiple actors can monitor the same actor
 * - An actor can monitor multiple actors
 * - Duplicate monitor registrations are idempotent (no-op)
 * - Monitors are automatically cleaned up when either actor terminates
 * - Monitor notifications are sent for ALL exit reasons (normal and abnormal)
 *
 * Returns 0 on success, non-zero on error.
 */
int kain_actor_monitor(
    KainActorId monitor_id,
    KainActorId monitored_id,
    KainDiagnostic* diag
);

/*
 * Demonitor Actor
 *
 * Removes a monitor relationship. After demonitor, the monitoring actor
 * will no longer receive exit notifications from the monitored actor.
 *
 * Returns 0 on success, non-zero if the monitor relationship doesn't exist.
 */
int kain_actor_demonitor(
    KainActorId monitor_id,
    KainActorId monitored_id,
    KainDiagnostic* diag
);

/*
 * Link Actors
 *
 * Creates a bidirectional link between two actors. Links provide crash
 * containment: if either actor exits abnormally, the other will be
 * terminated with KAIN_ACTOR_EXIT_KILLED.
 *
 * Link semantics:
 * - Links are bidirectional (if A links to B, then B is linked to A)
 * - Links propagate ONLY on abnormal exit (not KAIN_ACTOR_EXIT_NORMAL)
 * - Abnormal exits include: KILLED, CRASHED, SUPERVISOR_ESCALATION
 * - Duplicate link registrations are idempotent (no-op)
 * - Links are automatically cleaned up when either actor terminates
 * - Link propagation is immediate and synchronous
 *
 * Returns 0 on success, non-zero on error.
 */
int kain_actor_link(
    KainActorId actor_a,
    KainActorId actor_b,
    KainDiagnostic* diag
);

/*
 * Unlink Actors
 *
 * Removes a bidirectional link between two actors. After unlink, the
 * actors will no longer terminate each other on abnormal exit.
 *
 * Returns 0 on success, non-zero if the link doesn't exist.
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

/*
 * Get Scheduler Snapshot
 *
 * Returns the current scheduler queue and worker statistics.
 */
void kain_actor_scheduler_snapshot(KainActorSchedulerSnapshot* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* ACTOR_H */
