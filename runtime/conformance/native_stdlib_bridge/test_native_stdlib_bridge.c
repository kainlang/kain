#include "../../native/include/stdlib_abi.h"

#include <stdio.h>
#include <string.h>

static int expect_int(int condition, int code) {
    return condition ? 0 : code;
}

int main(void) {
    int status = (int)abi_runtime_init();
    if (status != 0) {
        return 10;
    }

    status = expect_int(abi_actor_invalid_id() == 0, 11);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_abi_version() == 3, 41);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_default_mailbox_capacity() == 1024, 42);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_default_ask_timeout_ms() == 30000, 43);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_default_shutdown_grace_ms() == 5000, 44);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_supervision_max_restarts() == 5, 45);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_supervision_restart_window_millis() == 60000, 46);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_registry_lookup("missing-service") == 0, 12);
    if (status != 0) {
        return status;
    }

    int64_t actor_id = abi_actor_spawn("probe", "total=0");
    status = expect_int(actor_id != abi_actor_invalid_id(), 23);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_send(actor_id, "Add", "value=3") == 0, 24);
    if (status != 0) {
        return status;
    }

    status = expect_int(!abi_actor_state_invalid(actor_id), 25);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_shutdown(actor_id) == 0, 26);
    if (status != 0) {
        return status;
    }

    status = (int)abi_entangle_reset();
    if (status != 0) {
        return 13;
    }

    status = expect_int(abi_entangle_registered_count() == 0, 14);
    if (status != 0) {
        return status;
    }

    status = (int)abi_entangle_register(
        "Physics.player_health",
        "UI.health_display",
        "single_writer",
        "Int"
    );
    if (status != 0) {
        return 15;
    }

    status = expect_int(abi_entangle_registered_count() == 1, 16);
    if (status != 0) {
        return status;
    }

    status = expect_int(
        strcmp(abi_entangle_get_authority(0), "Physics.player_health") == 0,
        17
    );
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_entangle_get_mirror(0), "UI.health_display") == 0, 18);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_entangle_get_policy(0), "single_writer") == 0, 19);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_entangle_get_type_name(0), "Int") == 0, 20);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_actor_scheduler_queue_depth() >= 0, 21);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_sleep_millis(0) == 0, 22);
    if (status != 0) {
        return status;
    }

    const char* fs_dir = abi_fs_temp_dir("kain-native-fs");
    status = expect_int(fs_dir != 0 && fs_dir[0] != '\0' && abi_fs_is_dir(fs_dir), 30);
    if (status != 0) {
        return status;
    }

    const char* fs_file = abi_fs_path_join(fs_dir, "bridge.txt");
    status = expect_int(abi_fs_write_text(fs_file, "hello") == 0, 31);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_fs_append_text(fs_file, " fs") == 0, 32);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_fs_exists(fs_file) && abi_fs_is_file(fs_file), 33);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_fs_read_text(fs_file), "hello fs") == 0, 34);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_fs_read_text_range(fs_file, 1, 4), "ello") == 0, 41);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_fs_read_byte_range_hex(fs_file, 0, 5), "68656c6c6f") == 0, 42);
    if (status != 0) {
        return status;
    }

    const char* fs_bytes = abi_fs_path_join(fs_dir, "bytes.bin");
    status = expect_int(abi_fs_write_bytes_hex(fs_bytes, "000102ff") == 0, 43);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_fs_read_bytes_hex(fs_bytes), "000102ff") == 0, 44);
    if (status != 0) {
        return status;
    }

    status = expect_int(strstr(abi_fs_metadata_text(fs_file), "file_type=file") != 0, 45);
    if (status != 0) {
        return status;
    }

    status = expect_int(strstr(abi_fs_read_dir_paths_text(fs_dir), "bridge.txt") != 0, 46);
    if (status != 0) {
        return status;
    }

    status = expect_int(strstr(abi_fs_walk_paths_text(fs_dir), "bytes.bin") != 0, 47);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(
        abi_fs_hash_file(fs_file),
        "52c69d2e3b3ec3cf129ec23ff5775dba3f016b4c1b18b1168fdb0ff7f1775a1f"
    ) == 0, 40);
    if (status != 0) {
        return status;
    }

    const char* fs_copy = abi_fs_path_join(fs_dir, "copy.txt");
    status = expect_int(abi_fs_copy_file_streaming(fs_file, fs_copy, 2) == 8, 35);
    if (status != 0) {
        return status;
    }

    const char* fs_moved = abi_fs_path_join(fs_dir, "moved.txt");
    status = expect_int(abi_fs_move_path(fs_copy, fs_moved) == 0, 36);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_fs_atomic_write_text(fs_moved, "atomic") == 0, 37);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(abi_fs_read_text(fs_moved), "atomic") == 0, 38);
    if (status != 0) {
        return status;
    }

    status = expect_int(abi_fs_remove_dir_all(fs_dir) == 0, 39);
    if (status != 0) {
        return status;
    }

    return (int)abi_runtime_shutdown();
}
