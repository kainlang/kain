#include "../../native/include/actor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(KainActorId) * 8u == KAIN_ACTOR_ID_BITS, "KainActorId width drifted");
_Static_assert(KAIN_ACTOR_ID_INVALID == 0ULL, "invalid actor id must stay zero");
_Static_assert(KAIN_MAILBOX_DEFAULT_CAPACITY == 1024, "default mailbox capacity drifted");
_Static_assert(KAIN_ACTOR_NAME_MAX == 128, "actor name buffer size drifted");
#endif

typedef struct {
    volatile int received_count;
    volatile int errors;
    size_t sizes[2];
    unsigned long long tags[2];
    char payloads[2][32];
} EchoProbe;

typedef struct {
    volatile int notification_count;
    volatile int errors;
    unsigned long long tag;
    KainActorId sender_id;
} MonitorProbe;

static int expect_true(int condition, int code, const char* label) {
    if (!condition) {
        printf("FAIL %d: %s\n", code, label);
        return code;
    }
    return 0;
}

static int wait_until_at_least(const volatile int* value, int target, int timeout_ms) {
    int elapsed = 0;
    while (*value < target && elapsed < timeout_ms) {
        Sleep(10);
        elapsed += 10;
    }
    return *value >= target;
}

static int wait_for_actor_state(KainActorId actor_id, KainActorState state, int timeout_ms) {
    int elapsed = 0;
    while (kain_actor_get_state(actor_id) != state && elapsed < timeout_ms) {
        Sleep(10);
        elapsed += 10;
    }
    return kain_actor_get_state(actor_id) == state;
}

static KainActorExitReason echo_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    EchoProbe* probe = (EchoProbe*)user_data;
    (void)actor_id;

    for (int i = 0; i < 2; i++) {
        KainActorMessage message;
        KainDiagnostic diag;
        kain_diagnostic_init(&diag);

        if (kain_actor_receive(mailbox, &message, &diag) != 0) {
            probe->errors = 10 + i;
            return KAIN_ACTOR_EXIT_CRASHED;
        }

        probe->sizes[i] = message.data_size;
        probe->tags[i] = message.type_tag;
        if (message.data != NULL && message.data_size > 0) {
            size_t copy_size = message.data_size;
            if (copy_size >= sizeof(probe->payloads[i])) {
                copy_size = sizeof(probe->payloads[i]) - 1;
            }
            memcpy(probe->payloads[i], message.data, copy_size);
            probe->payloads[i][copy_size] = '\0';
            free(message.data);
        }
        probe->received_count = i + 1;
    }

    {
        KainActorMessage extra;
        KainDiagnostic diag;
        memset(&extra, 0, sizeof(extra));
        kain_diagnostic_init(&diag);
        if (kain_actor_try_receive(mailbox, &extra, &diag) != 1) {
            probe->errors = 20;
            if (extra.data != NULL) {
                free(extra.data);
            }
            return KAIN_ACTOR_EXIT_CRASHED;
        }
    }

    return KAIN_ACTOR_EXIT_NORMAL;
}

static KainActorExitReason monitor_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    MonitorProbe* probe = (MonitorProbe*)user_data;
    KainActorMessage message;
    KainDiagnostic diag;
    (void)actor_id;

    kain_diagnostic_init(&diag);
    if (kain_actor_receive(mailbox, &message, &diag) != 0) {
        probe->errors = 1;
        return KAIN_ACTOR_EXIT_CRASHED;
    }

    probe->tag = message.type_tag;
    probe->sender_id = message.sender_id;
    probe->notification_count = 1;
    if (message.data != NULL) {
        free(message.data);
    }

    return KAIN_ACTOR_EXIT_NORMAL;
}

static KainActorExitReason blocking_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    KainActorMessage message;
    KainDiagnostic diag;
    (void)actor_id;
    (void)user_data;

    for (;;) {
        kain_diagnostic_init(&diag);
        if (kain_actor_receive(mailbox, &message, &diag) != 0) {
            return KAIN_ACTOR_EXIT_SHUTDOWN;
        }
        if (message.data != NULL) {
            free(message.data);
        }
    }
}

static KainActorId spawn_named_actor(
    const char* name,
    KainActorBootstrapFn bootstrap_fn,
    void* user_data,
    KainActorId supervisor_id,
    KainRestartPolicy restart_policy
) {
    KainActorSpawnConfig config;
    KainDiagnostic diag;

    kain_diagnostic_init(&diag);
    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = bootstrap_fn;
    config.user_data = user_data;
    config.mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    config.supervisor_id = supervisor_id;
    config.restart_policy = restart_policy;
    config.supervision_strategy = KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE;
    snprintf(config.name, sizeof(config.name), "%s", name);

    return kain_actor_spawn(&config, &diag);
}

int main(void) {
    KainDiagnostic diag;
    int status;

    kain_diagnostic_init(&diag);
    kain_actor_runtime_init();

    {
        KainActorAbiDescriptor descriptor = kain_actor_abi_descriptor();
        KainActorAbiDescriptor mutated = descriptor;

        status = expect_true(descriptor.abi_version == KAIN_ACTOR_ABI_VERSION, 10, "ABI version");
        if (status != 0) return status;
        status = expect_true(descriptor.actor_id_bits == KAIN_ACTOR_ID_BITS, 11, "actor id bits");
        if (status != 0) return status;
        status = expect_true(descriptor.invalid_actor_id == KAIN_ACTOR_ID_INVALID, 12, "invalid id");
        if (status != 0) return status;
        status = expect_true(
            descriptor.default_mailbox_capacity == KAIN_MAILBOX_DEFAULT_CAPACITY,
            13,
            "default mailbox capacity"
        );
        if (status != 0) return status;
        status = expect_true(
            descriptor.default_ask_timeout_ms == KAIN_ACTOR_DEFAULT_ASK_TIMEOUT_MS,
            14,
            "ask timeout"
        );
        if (status != 0) return status;
        status = expect_true(
            descriptor.default_shutdown_grace_ms == KAIN_ACTOR_DEFAULT_SHUTDOWN_GRACE_MS,
            15,
            "shutdown grace"
        );
        if (status != 0) return status;
        status = expect_true(
            descriptor.supervision_max_restarts == KAIN_SUPERVISION_MAX_RESTARTS,
            16,
            "supervision restart limit"
        );
        if (status != 0) return status;
        status = expect_true(kain_actor_abi_descriptor_is_compatible(&descriptor), 17, "ABI compatible");
        if (status != 0) return status;

        mutated.abi_version = 999U;
        status = expect_true(!kain_actor_abi_descriptor_is_compatible(&mutated), 18, "ABI mismatch");
        if (status != 0) return status;
    }

    {
        KainActorSpawnConfig config;
        kain_actor_spawn_config_init(&config);
        status = expect_true(config.mailbox_capacity == KAIN_MAILBOX_DEFAULT_CAPACITY, 20, "spawn mailbox default");
        if (status != 0) return status;
        status = expect_true(config.restart_policy == KAIN_RESTART_POLICY_TEMPORARY, 21, "spawn restart default");
        if (status != 0) return status;
        status = expect_true(
            config.supervision_strategy == KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE,
            22,
            "spawn strategy default"
        );
        if (status != 0) return status;
        status = expect_true(config.supervisor_id == KAIN_ACTOR_ID_INVALID, 23, "spawn supervisor default");
        if (status != 0) return status;
        status = expect_true(config.retain_user_data == 0, 24, "spawn user_data ownership default");
        if (status != 0) return status;
        status = expect_true(kain_actor_spawn(&config, &diag) == KAIN_ACTOR_ID_INVALID, 25, "invalid spawn fails");
        if (status != 0) return status;
    }

    {
        KainActorId actor_id = spawn_named_actor(
            "ref_contract",
            blocking_actor_bootstrap,
            NULL,
            KAIN_ACTOR_ID_INVALID,
            KAIN_RESTART_POLICY_TEMPORARY
        );
        KainActorRef actor_ref;

        memset(&actor_ref, 0, sizeof(actor_ref));
        status = expect_true(actor_id != KAIN_ACTOR_ID_INVALID, 26, "ref actor spawn");
        if (status != 0) return status;
        kain_actor_ref_from_id(actor_id, &actor_ref);
        status = expect_true(actor_ref.actor_id == actor_id, 27, "actor ref id mirrors actor id");
        if (status != 0) return status;
        status = expect_true(actor_ref.generation != 0u, 28, "actor ref generation is minted");
        if (status != 0) return status;
        status = expect_true(
            actor_ref.execution_class == KAIN_ACTOR_EXECUTION_CLASS_MICROCELL,
            29,
            "actor ref execution class"
        );
        if (status != 0) return status;
        status = expect_true(
            actor_ref.locality_class == KAIN_ACTOR_LOCALITY_LOCAL,
            30,
            "actor ref locality class"
        );
        if (status != 0) return status;
        status = expect_true(kain_actor_ref_is_live(&actor_ref), 31, "actor ref live");
        if (status != 0) return status;
        status = expect_true(kain_actor_shutdown(actor_id, &diag) == 0, 32, "ref actor shutdown");
        if (status != 0) return status;
    }

    {
        void* reply_port = kain_actor_reply_port_new();
        KainActorRef first_reply_ref;
        KainActorRef stale_reply_ref;
        KainActorRef rebound_reply_ref;
        long long first_value = 41;
        long long second_value = 99;
        long long received_value = 0;

        memset(&first_reply_ref, 0, sizeof(first_reply_ref));
        memset(&stale_reply_ref, 0, sizeof(stale_reply_ref));
        memset(&rebound_reply_ref, 0, sizeof(rebound_reply_ref));

        status = expect_true(reply_port != NULL, 33, "reply port allocates");
        if (status != 0) return status;
        kain_actor_reply_port_actor_ref(reply_port, &first_reply_ref);
        stale_reply_ref = first_reply_ref;

        status = expect_true(first_reply_ref.actor_id != KAIN_ACTOR_ID_INVALID, 34, "reply ref actor id");
        if (status != 0) return status;
        status = expect_true(
            first_reply_ref.execution_class == KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT,
            35,
            "reply ref execution class"
        );
        if (status != 0) return status;
        status = expect_true(
            first_reply_ref.locality_class == KAIN_ACTOR_LOCALITY_LOCAL,
            36,
            "reply ref locality class"
        );
        if (status != 0) return status;
        status = expect_true(kain_actor_ref_is_live(&first_reply_ref), 37, "reply ref live");
        if (status != 0) return status;
        status = expect_true(
            kain_actor_reply_port_send_ref(&first_reply_ref, &first_value, sizeof(first_value)) == 0,
            38,
            "reply ref accepts direct send"
        );
        if (status != 0) return status;
        received_value = kain_actor_reply_port_wait_i64(reply_port, 1000);
        status = expect_true(received_value == first_value, 39, "reply wait returns first payload");
        if (status != 0) return status;

        reply_port = kain_actor_reply_port_new();
        status = expect_true(reply_port != NULL, 40, "reply port rebind allocates");
        if (status != 0) return status;
        kain_actor_reply_port_actor_ref(reply_port, &rebound_reply_ref);
        status = expect_true(!kain_actor_ref_is_live(&stale_reply_ref), 41, "stale reply ref is dead after rebind");
        if (status != 0) return status;
        status = expect_true(
            kain_actor_reply_port_send_ref(&stale_reply_ref, &second_value, sizeof(second_value)) != 0,
            42,
            "stale reply ref send rejected"
        );
        if (status != 0) return status;
        status = expect_true(
            kain_actor_reply_port_send_ref(&rebound_reply_ref, &second_value, sizeof(second_value)) == 0,
            43,
            "rebound reply ref accepts direct send"
        );
        if (status != 0) return status;
        received_value = kain_actor_reply_port_wait_i64(reply_port, 1000);
        status = expect_true(received_value == second_value, 44, "reply wait returns rebound payload");
        if (status != 0) return status;
        kain_actor_reply_port_destroy(reply_port);
        status = expect_true(!kain_actor_ref_is_live(&rebound_reply_ref), 45, "reply ref dies after destroy");
        if (status != 0) return status;
    }

    {
        EchoProbe probe;
        KainActorId actor_id;
        KainActorMessage first;
        KainActorMessage second;
        const char first_payload[] = "alpha";
        const char second_payload[] = "beta";

        memset(&probe, 0, sizeof(probe));
        actor_id = spawn_named_actor("echo_contract", echo_actor_bootstrap, &probe, KAIN_ACTOR_ID_INVALID, KAIN_RESTART_POLICY_TEMPORARY);
        status = expect_true(actor_id != KAIN_ACTOR_ID_INVALID, 30, "echo actor spawn");
        if (status != 0) return status;

        status = expect_true(kain_actor_registry_register("echo_contract", actor_id, &diag) == 0, 31, "registry register");
        if (status != 0) return status;
        status = expect_true(kain_actor_registry_lookup("echo_contract") == actor_id, 32, "registry lookup");
        if (status != 0) return status;
        status = expect_true(kain_actor_registry_register("echo_contract", actor_id, &diag) != 0, 33, "duplicate registry rejected");
        if (status != 0) return status;

        first.type_tag = 1001ULL;
        first.data = (void*)first_payload;
        first.data_size = sizeof(first_payload);
        first.sender_id = KAIN_ACTOR_ID_INVALID;
        second.type_tag = 1002ULL;
        second.data = (void*)second_payload;
        second.data_size = sizeof(second_payload);
        second.sender_id = KAIN_ACTOR_ID_INVALID;

        status = expect_true(kain_actor_send(actor_id, &first, &diag) == 0, 34, "first send");
        if (status != 0) return status;
        status = expect_true(kain_actor_send(actor_id, &second, &diag) == 0, 35, "second send");
        if (status != 0) return status;
        status = expect_true(wait_until_at_least(&probe.received_count, 2, 2000), 36, "echo actor received messages");
        if (status != 0) return status;
        status = expect_true(probe.errors == 0, 37, "echo actor errors");
        if (status != 0) return status;
        status = expect_true(probe.sizes[0] == sizeof(first_payload), 38, "first data_size retained");
        if (status != 0) return status;
        status = expect_true(probe.sizes[1] == sizeof(second_payload), 39, "second data_size retained");
        if (status != 0) return status;
        status = expect_true(strcmp(probe.payloads[0], first_payload) == 0, 40, "first payload retained");
        if (status != 0) return status;
        status = expect_true(strcmp(probe.payloads[1], second_payload) == 0, 41, "second payload retained");
        if (status != 0) return status;
        status = expect_true(kain_actor_registry_unregister("echo_contract", &diag) == 0, 42, "registry unregister");
        if (status != 0) return status;
    }

    {
        MonitorProbe probe;
        KainActorId monitor_id;
        KainActorId child_id;

        memset(&probe, 0, sizeof(probe));
        monitor_id = spawn_named_actor("monitor_contract", monitor_actor_bootstrap, &probe, KAIN_ACTOR_ID_INVALID, KAIN_RESTART_POLICY_TEMPORARY);
        child_id = spawn_named_actor("monitored_contract", blocking_actor_bootstrap, NULL, KAIN_ACTOR_ID_INVALID, KAIN_RESTART_POLICY_TEMPORARY);
        status = expect_true(monitor_id != KAIN_ACTOR_ID_INVALID && child_id != KAIN_ACTOR_ID_INVALID, 50, "monitor actors spawn");
        if (status != 0) return status;
        status = expect_true(wait_for_actor_state(monitor_id, KAIN_ACTOR_STATE_RUNNING, 2000), 501, "monitor actor running");
        if (status != 0) return status;
        status = expect_true(wait_for_actor_state(child_id, KAIN_ACTOR_STATE_RUNNING, 2000), 502, "monitored actor running");
        if (status != 0) return status;
        status = expect_true(kain_actor_monitor(monitor_id, child_id, &diag) == 0, 51, "monitor register");
        if (status != 0) return status;
        status = expect_true(kain_actor_monitor(monitor_id, child_id, &diag) == 0, 52, "monitor duplicate idempotent");
        if (status != 0) return status;
        status = expect_true(kain_actor_shutdown(child_id, &diag) == 0, 53, "monitored shutdown");
        if (status != 0) return status;
        status = expect_true(wait_until_at_least(&probe.notification_count, 1, 2000), 54, "monitor notification");
        if (status != 0) return status;
        status = expect_true(probe.errors == 0, 55, "monitor actor errors");
        if (status != 0) return status;
        status = expect_true(probe.sender_id == child_id, 56, "monitor sender");
        if (status != 0) return status;
        status = expect_true(
            probe.tag == (KAIN_ACTOR_MONITOR_EXIT_TAG_BASE | (unsigned long long)KAIN_ACTOR_EXIT_SHUTDOWN),
            57,
            "monitor exit tag"
        );
        if (status != 0) return status;
    }

    {
        KainActorId link_a = spawn_named_actor("link_a", blocking_actor_bootstrap, NULL, KAIN_ACTOR_ID_INVALID, KAIN_RESTART_POLICY_TEMPORARY);
        KainActorId link_b = spawn_named_actor("link_b", blocking_actor_bootstrap, NULL, KAIN_ACTOR_ID_INVALID, KAIN_RESTART_POLICY_TEMPORARY);
        status = expect_true(link_a != KAIN_ACTOR_ID_INVALID && link_b != KAIN_ACTOR_ID_INVALID, 60, "link actors spawn");
        if (status != 0) return status;
        status = expect_true(kain_actor_link(link_a, link_b, &diag) == 0, 61, "link register");
        if (status != 0) return status;
        status = expect_true(kain_actor_link(link_a, link_b, &diag) == 0, 62, "link duplicate idempotent");
        if (status != 0) return status;
        status = expect_true(kain_actor_unlink(link_a, link_b, &diag) == 0, 63, "unlink");
        if (status != 0) return status;
        status = expect_true(kain_actor_unlink(link_a, link_b, &diag) != 0, 64, "second unlink rejected");
        if (status != 0) return status;
        kain_actor_shutdown(link_a, &diag);
        kain_actor_shutdown(link_b, &diag);
    }

    {
        KainActorId supervisor_id = spawn_named_actor("supervisor_contract", blocking_actor_bootstrap, NULL, KAIN_ACTOR_ID_INVALID, KAIN_RESTART_POLICY_TEMPORARY);
        KainActorId child_id = spawn_named_actor("supervised_contract", blocking_actor_bootstrap, NULL, supervisor_id, KAIN_RESTART_POLICY_TEMPORARY);
        KainActorSupervisionSnapshot child_snapshot;
        KainActorSupervisionSnapshot supervisor_snapshot;

        memset(&child_snapshot, 0, sizeof(child_snapshot));
        memset(&supervisor_snapshot, 0, sizeof(supervisor_snapshot));
        status = expect_true(supervisor_id != KAIN_ACTOR_ID_INVALID && child_id != KAIN_ACTOR_ID_INVALID, 70, "supervision actors spawn");
        if (status != 0) return status;
        status = expect_true(wait_for_actor_state(supervisor_id, KAIN_ACTOR_STATE_RUNNING, 2000), 701, "supervisor running");
        if (status != 0) return status;
        status = expect_true(wait_for_actor_state(child_id, KAIN_ACTOR_STATE_RUNNING, 2000), 702, "supervised child running");
        if (status != 0) return status;
        status = expect_true(kain_actor_get_supervision_snapshot(child_id, &child_snapshot, &diag) == 0, 71, "child supervision snapshot");
        if (status != 0) return status;
        status = expect_true(child_snapshot.supervisor_id == supervisor_id, 72, "child supervisor id");
        if (status != 0) return status;
        status = expect_true(child_snapshot.restart_policy == KAIN_RESTART_POLICY_TEMPORARY, 73, "child restart policy");
        if (status != 0) return status;
        status = expect_true(kain_actor_shutdown(child_id, &diag) == 0, 74, "supervised child shutdown");
        if (status != 0) return status;

        for (int i = 0; i < 200; i++) {
            if (kain_actor_get_supervision_snapshot(supervisor_id, &supervisor_snapshot, &diag) == 0 &&
                supervisor_snapshot.observed_child_exit_count > 0) {
                break;
            }
            Sleep(10);
        }
        status = expect_true(
            supervisor_snapshot.observed_child_exit_count > 0,
            75,
            "supervisor observed child exit"
        );
        if (status != 0) return status;
        status = expect_true(supervisor_snapshot.last_observed_child_id == child_id, 76, "supervisor child id");
        if (status != 0) return status;
        kain_actor_shutdown(supervisor_id, &diag);
    }

    {
        KainActorSchedulerSnapshot snapshot;
        memset(&snapshot, 0, sizeof(snapshot));
        kain_actor_scheduler_snapshot(&snapshot);
        status = expect_true(snapshot.worker_count == KAIN_ACTOR_SCHEDULER_WORKER_COUNT, 80, "scheduler worker count");
        if (status != 0) return status;
        status = expect_true(snapshot.total_enqueued >= snapshot.total_dequeued, 81, "scheduler counters");
        if (status != 0) return status;
    }

    kain_actor_runtime_shutdown();
    printf("PASS: actor ABI contract conformance\n");
    return 0;
}
