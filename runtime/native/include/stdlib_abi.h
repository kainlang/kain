#ifndef STDLIB_ABI_H
#define STDLIB_ABI_H

#include <stdint.h>

#include "graphics_system.h"
#include "input_system.h"
#include "net_system.h"
#include "process_system.h"
#include "ui_system.h"
#include "converge.h"
#include "cpu.h"
#include "simd.h"
#include "json.h"
#include "machine_stones.h"
#include "platform_library.h"

#ifdef __cplusplus
extern "C" {
#endif

int64_t abi_runtime_init(void);
int64_t abi_runtime_shutdown(void);
int64_t abi_runtime_heap_validate(void);
int64_t abi_attrition_checkpoint(const char* label, int64_t subject_id);
int64_t abi_attrition_note_progress(int64_t iteration, int64_t checksum);
int64_t abi_attrition_result_set(int64_t checksum, int64_t run_status, const char* run_failure);

void* abi_option_none(void);
void* abi_option_some(const void* payload, int64_t payload_size);
int64_t abi_option_is_some(const void* value);
int64_t abi_option_is_none(const void* value);
int64_t abi_option_payload_copy(const void* value, void* out_payload, int64_t out_payload_size);

void* abi_result_ok(const void* payload, int64_t payload_size);
void* abi_result_err(const void* payload, int64_t payload_size);
int64_t abi_result_is_ok(const void* value);
int64_t abi_result_is_err(const void* value);
void* abi_result_ok_option(const void* value);
int64_t abi_result_payload_copy(const void* value, void* out_payload, int64_t out_payload_size);

int64_t abi_tagged_is_success(const void* value);
int64_t abi_tagged_matches(const void* value, int64_t tag);
int64_t abi_tagged_payload_copy(const void* value, void* out_payload, int64_t out_payload_size);

void* abi_future_ready_from_value(const void* payload, int64_t payload_size);
int64_t abi_future_state(const void* future_value);
int64_t abi_future_await_payload_copy(const void* future_value, void* out_payload, int64_t out_payload_size);
void* abi_async_sleep_future(int64_t milliseconds);

int64_t abi_actor_abi_version(void);
int64_t abi_actor_invalid_id(void);
int64_t abi_actor_default_mailbox_capacity(void);
int64_t abi_actor_unbounded_mailbox_capacity(void);
int64_t abi_actor_default_ask_timeout_ms(void);
int64_t abi_actor_default_shutdown_grace_ms(void);
int64_t abi_actor_supervision_max_restarts(void);
int64_t abi_actor_supervision_restart_window_millis(void);
int64_t abi_actor_spawn(const char* actor_name, const char* init_payload);
int64_t abi_actor_send(int64_t actor_id, const char* message_name, const char* data_payload);
int abi_actor_state_invalid(int64_t actor_id);
int64_t abi_actor_get_state(int64_t actor_id);
int64_t abi_actor_shutdown(int64_t actor_id);
int64_t abi_actor_kill(int64_t actor_id);
int64_t abi_actor_registry_lookup(const char* name);
int64_t abi_actor_registry_register(const char* name, int64_t actor_id);
int64_t abi_actor_registry_unregister(const char* name);
int64_t abi_actor_monitor(int64_t monitor_id, int64_t monitored_id);
int64_t abi_actor_demonitor(int64_t monitor_id, int64_t monitored_id);
int64_t abi_actor_link(int64_t actor_a, int64_t actor_b);
int64_t abi_actor_unlink(int64_t actor_a, int64_t actor_b);
int64_t abi_actor_supervision_observed_child_exit_count(int64_t actor_id);
int64_t abi_actor_supervision_restart_attempt_count(int64_t actor_id);
int64_t abi_actor_supervision_escalation_count(int64_t actor_id);
int abi_actor_supervision_limit_hit(int64_t actor_id);
int64_t abi_actor_scheduler_queue_depth(void);
int64_t abi_actor_scheduler_max_queue_depth(void);
int64_t abi_actor_scheduler_total_enqueued(void);
int64_t abi_actor_scheduler_total_dequeued(void);
int64_t abi_actor_scheduler_worker_count(void);
int64_t abi_actor_scheduler_active_workers(void);
int64_t abi_actor_scheduler_busy_workers(void);
int64_t abi_actor_scheduler_overflow_thread_spawns(void);

int64_t abi_entangle_reset(void);
int64_t abi_entangle_registered_count(void);
int64_t abi_entangle_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
);
const char* abi_entangle_get_authority(int64_t index);
const char* abi_entangle_get_mirror(int64_t index);
const char* abi_entangle_get_policy(int64_t index);
const char* abi_entangle_get_type_name(int64_t index);

int64_t abi_patch_begin(const char* patch_name);
int64_t abi_patch_record_i64(const char* patch_name, const char* path, int64_t old_value, int64_t new_value);
int64_t abi_patch_commit(const char* patch_name);
int64_t abi_patch_undo_last(void);
int64_t abi_patch_journal_count(void);
const char* abi_patch_last_path(void);

int64_t abi_entangle_record_i64(const char* authority, const char* mirror, int64_t value);
int64_t abi_entangle_propagation_count(void);
const char* abi_entangle_last_authority(void);
const char* abi_entangle_last_mirror(void);

int64_t abi_converge_record_i64(const char* converge_name, const char* lane_name, int64_t spec_value, int64_t fast_value);
int64_t abi_converge_record_bool(const char* converge_name, const char* lane_name, int fast_matches);
int64_t abi_converge_mismatch_count(void);

int64_t abi_orchestrate_stage_begin(const char* runtime_name, const char* function_name);
int64_t abi_orchestrate_stage_end_i64(const char* runtime_name, const char* function_name, int64_t status);
int64_t abi_orchestrate_stage_count(void);

int64_t abi_now_millis(void);
int64_t abi_sleep_millis(int64_t milliseconds);

const char* abi_fs_read_text(const char* path);
const char* abi_fs_read_text_range(const char* path, int64_t offset, int64_t length);
int64_t abi_fs_write_text(const char* path, const char* content);
int64_t abi_fs_write_text_len(const char* path, const char* content, int64_t content_length);
int64_t abi_fs_append_text(const char* path, const char* content);
int64_t abi_fs_append_text_len(const char* path, const char* content, int64_t content_length);
int64_t abi_fs_atomic_write_text(const char* path, const char* content);
int64_t abi_fs_atomic_write_text_len(const char* path, const char* content, int64_t content_length);
const char* abi_fs_read_bytes_hex(const char* path);
const char* abi_fs_read_byte_range_hex(const char* path, int64_t offset, int64_t length);
int64_t abi_fs_write_bytes_hex(const char* path, const char* hex);
int abi_fs_exists(const char* path);
int abi_fs_is_file(const char* path);
int abi_fs_is_dir(const char* path);
const char* abi_fs_metadata_text(const char* path);
const char* abi_fs_read_dir_paths_text(const char* path);
const char* abi_fs_walk_paths_text(const char* path);
int64_t abi_fs_create_dir_all(const char* path);
int64_t abi_fs_copy_file(const char* src, const char* dest);
int64_t abi_fs_copy_file_streaming(const char* src, const char* dest, int64_t chunk_size);
int64_t abi_fs_move_path(const char* src, const char* dest);
int64_t abi_fs_remove_file(const char* path);
int64_t abi_fs_remove_dir_all(const char* path);
const char* abi_fs_temp_file(const char* prefix);
const char* abi_fs_temp_dir(const char* prefix);
const char* abi_fs_hash_file(const char* path);
const char* abi_fs_path_join(const char* base, const char* child);
int64_t abi_fs_last_status(void);
const char* abi_fs_last_error_kind(void);
const char* abi_fs_last_error_message(void);

const char* abi_crypto_random_bytes_hex(int64_t length);
const char* abi_crypto_sha256_text(const char* text, int64_t text_length);
const char* abi_crypto_hmac_sha256_text(const char* key, int64_t key_length, const char* message, int64_t message_length);
const char* abi_crypto_blake3_text(const char* text, int64_t text_length);
int64_t abi_map_release(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif /* STDLIB_ABI_H */
