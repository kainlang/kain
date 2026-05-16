/*
 * Actor Runtime Smoke Test: Mailbox Backpressure
 *
 * Tests:
 * - Bounded mailbox capacity
 * - Mailbox full detection
 * - Send failure when mailbox is full
 */

#include "../../native/include/actor.h"
#include "../../native/include/diagnostics.h"
#include <stdio.h>
#include <stdlib.h>

/* Actor that doesn't consume messages */
KainActorExitReason slow_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    /* Sleep to let messages accumulate */
#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif
    return KAIN_ACTOR_EXIT_NORMAL;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Mailbox Backpressure ===\n\n");

    /* Initialize actor runtime */
    kain_actor_runtime_init();

    /* Spawn actor with small mailbox */
    KainActorSpawnConfig config;
    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = slow_actor_bootstrap;
    config.mailbox_capacity = 3; /* Small capacity */

    KainDiagnostic diag;
    KainActorId actor_id = kain_actor_spawn(&config, &diag);

    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Actor spawn failed: %s\n", diag.message);
        return 1;
    }

    printf("Actor spawned with mailbox capacity: 3\n");

    /* Send messages until mailbox is full */
    int sent_count = 0;
    for (int i = 0; i < 5; i++) {
        KainActorMessage msg;
        msg.type_tag = i;
        msg.data = NULL;
        msg.data_size = 0;
        msg.sender_id = KAIN_ACTOR_ID_INVALID;

        int result = kain_actor_send(actor_id, &msg, &diag);
        if (result == 0) {
            sent_count++;
            printf("Message %d sent successfully\n", i);
        } else {
            printf("Message %d failed: %s (code: %d)\n", i, diag.message, diag.code);

            /* Verify it's a mailbox full error */
            if (diag.code != KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL) {
                printf("FAIL: Expected MAILBOX_FULL error, got code %d\n", diag.code);
                return 1;
            }
        }
    }

    printf("\nSent %d messages (capacity was 3)\n", sent_count);

    if (sent_count != 3) {
        printf("FAIL: Expected to send exactly 3 messages, sent %d\n", sent_count);
        return 1;
    }

    /* Shutdown */
    kain_actor_runtime_shutdown();

    printf("\nPASS: Mailbox backpressure test completed successfully\n");
    return 0;
}
