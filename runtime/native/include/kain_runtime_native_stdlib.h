#ifndef KAIN_RUNTIME_NATIVE_STDLIB_H
#define KAIN_RUNTIME_NATIVE_STDLIB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t kain_native_runtime_init(void);
int64_t kain_native_runtime_shutdown(void);

int64_t kain_native_actor_invalid_id(void);
int64_t kain_native_actor_default_mailbox_capacity(void);
int64_t kain_native_actor_unbounded_mailbox_capacity(void);
int64_t kain_native_actor_spawn(const char* actor_name, const char* init_payload);
int64_t kain_native_actor_send(int64_t actor_id, const char* message_name, const char* data_payload);
int kain_native_actor_state_invalid(int64_t actor_id);
int64_t kain_native_actor_get_state(int64_t actor_id);
int64_t kain_native_actor_shutdown(int64_t actor_id);
int64_t kain_native_actor_kill(int64_t actor_id);
int64_t kain_native_actor_registry_lookup(const char* name);
int64_t kain_native_actor_registry_register(const char* name, int64_t actor_id);
int64_t kain_native_actor_registry_unregister(const char* name);
int64_t kain_native_actor_monitor(int64_t monitor_id, int64_t monitored_id);
int64_t kain_native_actor_demonitor(int64_t monitor_id, int64_t monitored_id);
int64_t kain_native_actor_link(int64_t actor_a, int64_t actor_b);
int64_t kain_native_actor_unlink(int64_t actor_a, int64_t actor_b);
int64_t kain_native_actor_supervision_observed_child_exit_count(int64_t actor_id);
int64_t kain_native_actor_supervision_restart_attempt_count(int64_t actor_id);
int64_t kain_native_actor_supervision_escalation_count(int64_t actor_id);
int kain_native_actor_supervision_limit_hit(int64_t actor_id);
int64_t kain_native_actor_scheduler_queue_depth(void);
int64_t kain_native_actor_scheduler_max_queue_depth(void);
int64_t kain_native_actor_scheduler_total_enqueued(void);
int64_t kain_native_actor_scheduler_total_dequeued(void);
int64_t kain_native_actor_scheduler_worker_count(void);
int64_t kain_native_actor_scheduler_active_workers(void);
int64_t kain_native_actor_scheduler_busy_workers(void);
int64_t kain_native_actor_scheduler_overflow_thread_spawns(void);

int64_t kain_native_entangle_reset(void);
int64_t kain_native_entangle_registered_count(void);
int64_t kain_native_entangle_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
);
const char* kain_native_entangle_get_authority(int64_t index);
const char* kain_native_entangle_get_mirror(int64_t index);
const char* kain_native_entangle_get_policy(int64_t index);
const char* kain_native_entangle_get_type_name(int64_t index);

int64_t kain_native_now_millis(void);
int64_t kain_native_sleep_millis(int64_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RUNTIME_NATIVE_STDLIB_H */
