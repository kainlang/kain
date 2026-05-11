#include "../../native/include/kain_runtime_native_stdlib.h"

#include <stdio.h>
#include <string.h>

static int expect_int(int condition, int code) {
    return condition ? 0 : code;
}

int main(void) {
    int status = (int)kain_native_runtime_init();
    if (status != 0) {
        return 10;
    }

    status = expect_int(kain_native_actor_invalid_id() == 0, 11);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_actor_registry_lookup("missing-service") == 0, 12);
    if (status != 0) {
        return status;
    }

    int64_t actor_id = kain_native_actor_spawn("probe", "total=0");
    status = expect_int(actor_id != kain_native_actor_invalid_id(), 23);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_actor_send(actor_id, "Add", "value=3") == 0, 24);
    if (status != 0) {
        return status;
    }

    status = expect_int(!kain_native_actor_state_invalid(actor_id), 25);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_actor_shutdown(actor_id) == 0, 26);
    if (status != 0) {
        return status;
    }

    status = (int)kain_native_entangle_reset();
    if (status != 0) {
        return 13;
    }

    status = expect_int(kain_native_entangle_registered_count() == 0, 14);
    if (status != 0) {
        return status;
    }

    status = (int)kain_native_entangle_register(
        "Physics.player_health",
        "UI.health_display",
        "single_writer",
        "Int"
    );
    if (status != 0) {
        return 15;
    }

    status = expect_int(kain_native_entangle_registered_count() == 1, 16);
    if (status != 0) {
        return status;
    }

    status = expect_int(
        strcmp(kain_native_entangle_get_authority(0), "Physics.player_health") == 0,
        17
    );
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(kain_native_entangle_get_mirror(0), "UI.health_display") == 0, 18);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(kain_native_entangle_get_policy(0), "single_writer") == 0, 19);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(kain_native_entangle_get_type_name(0), "Int") == 0, 20);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_actor_scheduler_queue_depth() >= 0, 21);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_sleep_millis(0) == 0, 22);
    if (status != 0) {
        return status;
    }

    const char* fs_dir = kain_native_fs_temp_dir("kain-native-fs");
    status = expect_int(fs_dir != 0 && fs_dir[0] != '\0' && kain_native_fs_is_dir(fs_dir), 30);
    if (status != 0) {
        return status;
    }

    const char* fs_file = kain_native_fs_path_join(fs_dir, "bridge.txt");
    status = expect_int(kain_native_fs_write_text(fs_file, "hello") == 0, 31);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_fs_append_text(fs_file, " fs") == 0, 32);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_fs_exists(fs_file) && kain_native_fs_is_file(fs_file), 33);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(kain_native_fs_read_text(fs_file), "hello fs") == 0, 34);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(
        kain_native_fs_hash_file(fs_file),
        "52c69d2e3b3ec3cf129ec23ff5775dba3f016b4c1b18b1168fdb0ff7f1775a1f"
    ) == 0, 40);
    if (status != 0) {
        return status;
    }

    const char* fs_copy = kain_native_fs_path_join(fs_dir, "copy.txt");
    status = expect_int(kain_native_fs_copy_file(fs_file, fs_copy) == 0, 35);
    if (status != 0) {
        return status;
    }

    const char* fs_moved = kain_native_fs_path_join(fs_dir, "moved.txt");
    status = expect_int(kain_native_fs_move_path(fs_copy, fs_moved) == 0, 36);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_fs_atomic_write_text(fs_moved, "atomic") == 0, 37);
    if (status != 0) {
        return status;
    }

    status = expect_int(strcmp(kain_native_fs_read_text(fs_moved), "atomic") == 0, 38);
    if (status != 0) {
        return status;
    }

    status = expect_int(kain_native_fs_remove_dir_all(fs_dir) == 0, 39);
    if (status != 0) {
        return status;
    }

    return (int)kain_native_runtime_shutdown();
}
