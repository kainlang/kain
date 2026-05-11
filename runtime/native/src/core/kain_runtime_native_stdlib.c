#include "../../include/kain_runtime_native_stdlib.h"

#include "../../include/kain_runtime_actor.h"
#include "../../include/kain_runtime_base.h"
#include "../../include/kain_runtime_diagnostics.h"
#include "../../include/kain_runtime_entangle.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static KainDiagnostic kain_native_diag(void) {
    KainDiagnostic diag;
    kain_diagnostic_init(&diag);
    return diag;
}

int64_t kain_native_runtime_init(void) {
    kain_actor_runtime_init();
    return 0;
}

int64_t kain_native_runtime_shutdown(void) {
    kain_actor_runtime_shutdown();
    return 0;
}

int64_t kain_native_actor_invalid_id(void) {
    return (int64_t)KAIN_ACTOR_ID_INVALID;
}

int64_t kain_native_actor_default_mailbox_capacity(void) {
    return (int64_t)KAIN_MAILBOX_DEFAULT_CAPACITY;
}

int64_t kain_native_actor_unbounded_mailbox_capacity(void) {
    return (int64_t)KAIN_MAILBOX_UNBOUNDED_CAPACITY;
}

static unsigned long long kain_native_hash_message_name(const char* value) {
    unsigned long long hash = 1469598103934665603ULL;
    if (value == 0) {
        return hash;
    }
    while (*value != '\0') {
        hash ^= (unsigned char)(*value);
        hash *= 1099511628211ULL;
        value++;
    }
    return hash;
}

static void kain_native_copy_actor_name(char* destination, size_t destination_size, const char* source) {
    size_t index = 0;
    if (destination == 0 || destination_size == 0) {
        return;
    }
    if (source != 0) {
        while (source[index] != '\0' && index + 1 < destination_size) {
            destination[index] = source[index];
            index++;
        }
    }
    destination[index] = '\0';
}

static KainActorExitReason kain_native_actor_default_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)user_data;

    for (;;) {
        KainActorMessage message;
        KainDiagnostic diag = kain_native_diag();
        if (kain_actor_receive(mailbox, &message, &diag) != 0) {
            return KAIN_ACTOR_EXIT_NORMAL;
        }
        if (message.data != 0) {
            free(message.data);
        }
    }
}

int64_t kain_native_actor_spawn(const char* actor_name, const char* init_payload) {
    KainDiagnostic diag = kain_native_diag();
    KainActorSpawnConfig config;
    (void)init_payload;

    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = kain_native_actor_default_bootstrap;
    config.user_data = 0;
    config.mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    kain_native_copy_actor_name(config.name, sizeof(config.name), actor_name);

    return (int64_t)kain_actor_spawn(&config, &diag);
}

int64_t kain_native_actor_send(int64_t actor_id, const char* message_name, const char* data_payload) {
    KainDiagnostic diag = kain_native_diag();
    KainActorMessage message;
    message.type_tag = kain_native_hash_message_name(message_name);
    message.data = (void*)data_payload;
    message.data_size = data_payload == 0 ? 0 : strlen(data_payload) + 1;
    message.sender_id = KAIN_ACTOR_ID_INVALID;
    return (int64_t)kain_actor_send((KainActorId)actor_id, &message, &diag);
}

int kain_native_actor_state_invalid(int64_t actor_id) {
    return actor_id <= 0 || kain_actor_get_state((KainActorId)actor_id) == KAIN_ACTOR_STATE_UNINITIALIZED;
}

int64_t kain_native_actor_get_state(int64_t actor_id) {
    return (int64_t)kain_actor_get_state((KainActorId)actor_id);
}

int64_t kain_native_actor_shutdown(int64_t actor_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_shutdown((KainActorId)actor_id, &diag);
}

int64_t kain_native_actor_kill(int64_t actor_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_kill((KainActorId)actor_id, &diag);
}

int64_t kain_native_actor_registry_lookup(const char* name) {
    if (name == 0 || name[0] == '\0') {
        return (int64_t)KAIN_ACTOR_ID_INVALID;
    }
    return (int64_t)kain_actor_registry_lookup(name);
}

int64_t kain_native_actor_registry_register(const char* name, int64_t actor_id) {
    KainDiagnostic diag = kain_native_diag();
    if (name == 0 || name[0] == '\0' || actor_id <= 0) {
        return -1;
    }
    return (int64_t)kain_actor_registry_register(name, (KainActorId)actor_id, &diag);
}

int64_t kain_native_actor_registry_unregister(const char* name) {
    KainDiagnostic diag = kain_native_diag();
    if (name == 0 || name[0] == '\0') {
        return -1;
    }
    return (int64_t)kain_actor_registry_unregister(name, &diag);
}

int64_t kain_native_actor_monitor(int64_t monitor_id, int64_t monitored_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_monitor((KainActorId)monitor_id, (KainActorId)monitored_id, &diag);
}

int64_t kain_native_actor_demonitor(int64_t monitor_id, int64_t monitored_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_demonitor((KainActorId)monitor_id, (KainActorId)monitored_id, &diag);
}

int64_t kain_native_actor_link(int64_t actor_a, int64_t actor_b) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_link((KainActorId)actor_a, (KainActorId)actor_b, &diag);
}

int64_t kain_native_actor_unlink(int64_t actor_a, int64_t actor_b) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_unlink((KainActorId)actor_a, (KainActorId)actor_b, &diag);
}

static int kain_native_actor_supervision_snapshot(
    int64_t actor_id,
    KainActorSupervisionSnapshot* snapshot
) {
    KainDiagnostic diag = kain_native_diag();
    if (snapshot == 0) {
        return -1;
    }
    return kain_actor_get_supervision_snapshot((KainActorId)actor_id, snapshot, &diag);
}

int64_t kain_native_actor_supervision_observed_child_exit_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.observed_child_exit_count;
}

int64_t kain_native_actor_supervision_restart_attempt_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.restart_attempt_count;
}

int64_t kain_native_actor_supervision_escalation_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.escalation_count;
}

int kain_native_actor_supervision_limit_hit(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return 0;
    }
    return snapshot.restart_limit_hit != 0 || snapshot.supervision_limit_hits != 0;
}

static KainActorSchedulerSnapshot kain_native_actor_scheduler_snapshot(void) {
    KainActorSchedulerSnapshot snapshot;
    kain_actor_scheduler_snapshot(&snapshot);
    return snapshot;
}

int64_t kain_native_actor_scheduler_queue_depth(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().queue_depth;
}

int64_t kain_native_actor_scheduler_max_queue_depth(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().max_queue_depth;
}

int64_t kain_native_actor_scheduler_total_enqueued(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().total_enqueued;
}

int64_t kain_native_actor_scheduler_total_dequeued(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().total_dequeued;
}

int64_t kain_native_actor_scheduler_worker_count(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().worker_count;
}

int64_t kain_native_actor_scheduler_active_workers(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().active_workers;
}

int64_t kain_native_actor_scheduler_busy_workers(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().busy_workers;
}

int64_t kain_native_actor_scheduler_overflow_thread_spawns(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().overflow_thread_spawns;
}

int64_t kain_native_entangle_reset(void) {
    kain_runtime_entangle_registry_reset();
    return 0;
}

int64_t kain_native_entangle_registered_count(void) {
    return (int64_t)kain_runtime_entangle_registered_count();
}

int64_t kain_native_entangle_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
) {
    return (int64_t)kain_runtime_entangle_register(authority, mirror, policy, type_name);
}

static const KainRuntimeEntangleBinding* kain_native_entangle_binding_at(int64_t index) {
    static KainRuntimeEntangleBinding binding;
    if (index < 0) {
        return 0;
    }
    if (kain_runtime_entangle_get((size_t)index, &binding) != 0) {
        return 0;
    }
    return &binding;
}

const char* kain_native_entangle_get_authority(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->authority : "";
}

const char* kain_native_entangle_get_mirror(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->mirror : "";
}

const char* kain_native_entangle_get_policy(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->policy : "";
}

const char* kain_native_entangle_get_type_name(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->type_name : "";
}

int64_t kain_native_now_millis(void) {
    return (int64_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

int64_t kain_native_sleep_millis(int64_t milliseconds) {
    if (milliseconds < 0) {
        return -1;
    }
    Sleep((unsigned int)milliseconds);
    return 0;
}
