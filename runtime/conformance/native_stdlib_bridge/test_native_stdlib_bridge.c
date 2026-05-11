#include "../../native/include/kain_runtime_native_stdlib.h"

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

    return (int)kain_native_runtime_shutdown();
}
