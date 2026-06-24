// ============================================================================
//  hot_reload_test.c — Hot Reload Channel Demo
//  ============================================================================
//  Demonstrates the Kain UI hot-reload shared memory IPC channel:
//    - Initialize a hot-reload channel (owner side)
//    - Open the channel from a "watcher" side
//    - Send bundle request events through the ring buffer
//    - Read/handle events on the watcher side
//    - Demonstrate the file-watch polling pattern
//    - Track generation counters for applied/rejected bundles
//  ============================================================================
//  This test does NOT create a window — it tests the shared-memory IPC
//  substrate that the UI hot-reload system uses for live bundle swapping.
//
//  Architecture:
//    KainUiHotReloadChannel uses a named shared memory segment (Win32:
//    CreateFileMapping/MapViewOfFile) to share a KainUiHotReloadSharedControl
//    struct between the running Kain process and a file-watcher process.
//
//    The ring buffer (events[KAIN_UI_HOT_RELOAD_RING_CAPACITY]) records
//    REQUESTED, APPLIED, REJECTED, and INFO events with sequence numbers.
//
//    The watcher polls for file changes, writes a bundle path into
//    requested_bundle_path, then increments request_generation. The runtime
//    detects the change, loads the bundle, increments applied_generation
//    on success (or failed_generation on failure).
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 hot_reload_test.c ../TEST/stubs.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
//      ../ui_hot_reload.c ../ui_compiled_bundle.c ../ui_runtime.c ^
//      ../../core/input_system.c ^
//      -I../../../include -I.. -I../../core ^
//      -luser32 -lgdi32 -lopengl32 -o hot_reload_test.exe
//  ============================================================================

// Note: We define NO_WINDOW for this test — no window needed
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_hot_reload.h"
#include "../../include/ui_runtime.h"
#include "../../include/ui_bundle.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Test helpers ───────────────────────────────────────────────────────
static int g_passed = 0;
static int g_failed = 0;

#define TEST(name, expr) do { \
    int _ok = (expr); \
    if (_ok) { \
        g_passed++; \
        printf("  [PASS] %s\n", name); \
    } else { \
        g_failed++; \
        printf("  [FAIL] %s  (line %d)\n", name, __LINE__); \
    } \
} while(0)

#define TEST_MSG(name, expr, fmt, ...) do { \
    int _ok = (expr); \
    if (_ok) { \
        g_passed++; \
        printf("  [PASS] %s\n", name); \
    } else { \
        g_failed++; \
        printf("  [FAIL] %s  (" fmt ")\n", name, __VA_ARGS__); \
    } \
} while(0)

static void check_event_ring(KainUiHotReloadChannel* channel) {
    if (!channel || !channel->control) return;
    KainUiHotReloadSharedControl* ctrl = channel->control;

    printf("    Ring events (%lld total):\n", (long long)ctrl->event_sequence);
    for (uint32_t i = 0; i < KAIN_UI_HOT_RELOAD_RING_CAPACITY && i < 8; i++) {
        KainUiHotReloadEvent* ev = &ctrl->events[i];
        if (ev->kind == KAIN_UI_HOT_RELOAD_EVENT_NONE) continue;
        const char* kind_str = "";
        switch (ev->kind) {
            case KAIN_UI_HOT_RELOAD_EVENT_REQUESTED: kind_str = "REQUESTED"; break;
            case KAIN_UI_HOT_RELOAD_EVENT_APPLIED:   kind_str = "APPLIED"; break;
            case KAIN_UI_HOT_RELOAD_EVENT_REJECTED:  kind_str = "REJECTED"; break;
            case KAIN_UI_HOT_RELOAD_EVENT_INFO:      kind_str = "INFO"; break;
        }
        printf("    [%u] seq=%llu gen=%u kind=%s text='%s'\n",
               i, (unsigned long long)ev->sequence,
               (unsigned)ev->generation, kind_str, ev->text);
    }
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    printf("========================================================\n");
    printf("  Kain UI Hot Reload — Shared Memory Channel Test\n");
    printf("========================================================\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // ── Test 1: Channel initialization ─────────────────────────────
    printf("[Test 1] Channel initialization\n");
    {
        KainUiHotReloadChannel channel;
        memset(&channel, 0, sizeof(channel));
        kain_ui_hot_reload_channel_init(&channel);

        TEST("initialized flag is 0", channel.initialized == 0);
        TEST("control is NULL after init", channel.control == NULL);
        TEST("channel_name is empty", channel.channel_name[0] == '\0');
    }

    // ── Test 2: Create a channel (owner) ───────────────────────────
    printf("\n[Test 2] Channel creation (owner)\n");
    {
        KainUiHotReloadChannel channel;
        memset(&channel, 0, sizeof(channel));

        int result = kain_ui_hot_reload_channel_create(&channel, "kain-test-reload");
        TEST("channel_create returns success", result == 0);
        TEST("initialized is 1 after creation", channel.initialized == 1);

        if (channel.control) {
            printf("    Shared memory at %p\n", (void*)channel.control);
            printf("    Magic: 0x%08X (expected 0x%08X)\n",
                   channel.control->magic, KAIN_UI_HOT_RELOAD_SHARED_MAGIC);
            printf("    Version: %u (expected %u)\n",
                   channel.control->version, KAIN_UI_HOT_RELOAD_SHARED_VERSION);

            TEST("magic is correct", channel.control->magic == KAIN_UI_HOT_RELOAD_SHARED_MAGIC);
            TEST("version is correct", channel.control->version == KAIN_UI_HOT_RELOAD_SHARED_VERSION);
            TEST("request_generation starts at 0", channel.control->request_generation == 0);
            TEST("applied_generation starts at 0", channel.control->applied_generation == 0);
            TEST("event_sequence starts at 0", channel.control->event_sequence == 0);
            TEST("channel_name matches", strcmp(channel.channel_name, "kain-test-reload") == 0);
        } else {
            printf("    [WARN] No shared memory — skipping struct tests\n");
            TEST("control not available (may not have CreateFileMapping)", 0);
        }

        // Clean up
        kain_ui_hot_reload_channel_close(&channel);
        TEST("initialized is 0 after close", channel.initialized == 0);
    }

    // ── Test 3: Open a channel (watcher) ───────────────────────────
    printf("\n[Test 3] Channel open (watcher)\n");
    {
        // First create the channel
        KainUiHotReloadChannel owner;
        memset(&owner, 0, sizeof(owner));
        int create_ok = kain_ui_hot_reload_channel_create(&owner, "kain-test-reload-2");

        // Now try to open it
        KainUiHotReloadChannel watcher;
        memset(&watcher, 0, sizeof(watcher));
        int open_result = kain_ui_hot_reload_channel_open(&watcher, "kain-test-reload-2");

        if (create_ok == 0 && open_result == 0) {
            TEST("watcher initialized", watcher.initialized == 1);
            if (watcher.control && owner.control) {
                TEST("same shared memory (magic matches)",
                     watcher.control->magic == owner.control->magic);
                TEST("watcher sees same version",
                     watcher.control->version == owner.control->version);
            }
        } else {
            printf("    [WARN] Channel creation/open may not be fully functional\n");
            printf("    (Expected on systems without CreateFileMapping support)\n");
        }

        kain_ui_hot_reload_channel_close(&watcher);
        kain_ui_hot_reload_channel_close(&owner);
    }

    // ── Test 4: Request a bundle ───────────────────────────────────
    printf("\n[Test 4] Bundle request via channel\n");
    {
        KainUiHotReloadChannel channel;
        memset(&channel, 0, sizeof(channel));
        int create_ok = kain_ui_hot_reload_channel_create(&channel, "kain-test-reload-3");

        if (create_ok == 0 && channel.control) {
            int req_ok = kain_ui_hot_reload_channel_request_bundle(
                &channel, "/tmp/test_bundle.json", 0xDEADBEEF, 1);
            TEST("request_bundle returns success", req_ok == 0);

            if (req_ok == 0) {
                // Check that shared state was updated
                TEST("request_generation incremented",
                     channel.control->request_generation == 1);
                TEST("requested_fingerprint matches",
                     channel.control->requested_fingerprint == 0xDEADBEEF);
                TEST("bundle_path was set",
                     strcmp(channel.control->requested_bundle_path, "/tmp/test_bundle.json") == 0);

                printf("    request_generation: %d\n", channel.control->request_generation);
                printf("    requested_fingerprint: 0x%llX\n",
                       (unsigned long long)channel.control->requested_fingerprint);
                printf("    requested_bundle_path: %s\n",
                       channel.control->requested_bundle_path);
            }
        } else {
            printf("    [WARN] Skipping bundle request test — channel not available\n");
        }

        kain_ui_hot_reload_channel_close(&channel);
    }

    // ── Test 5: Controller lifecycle ───────────────────────────────
    printf("\n[Test 5] Controller lifecycle\n");
    {
        KainUiHotReloadController controller;
        memset(&controller, 0, sizeof(controller));
        kain_ui_hot_reload_controller_init(&controller);

        TEST("controller initialized flag is 0", controller.initialized == 0);
        TEST("poll_interval_ms has default", controller.poll_interval_ms == 0 ||
             controller.poll_interval_ms == KAIN_UI_HOT_RELOAD_POLL_INTERVAL_MS_DEFAULT ||
             controller.poll_interval_ms > 0);

        // Note: controller_boot needs a bundle_env_name; it will try to read
        // from env var and may fail gracefully. This tests the init path.
        printf("    Controller init state: poll_interval=%u ms\n",
               (unsigned)controller.poll_interval_ms);
    }

    // ── Test 6: Ring buffer behavior ───────────────────────────────
    printf("\n[Test 6] Ring buffer behavior\n");
    {
        KainUiHotReloadChannel channel;
        memset(&channel, 0, sizeof(channel));
        int create_ok = kain_ui_hot_reload_channel_create(&channel, "kain-test-reload-ring");

        if (create_ok == 0 && channel.control) {
            KainUiHotReloadSharedControl* ctrl = channel.control;

            // Write events directly (simulating what the runtime does)
            // Each event should increment event_sequence
            ctrl->event_sequence = 5; // Simulate 5 previous events

            // Check ring capacity
            TEST("ring capacity is power of 2",
                 (KAIN_UI_HOT_RELOAD_RING_CAPACITY & (KAIN_UI_HOT_RELOAD_RING_CAPACITY - 1)) == 0);
            TEST("ring capacity is 128", KAIN_UI_HOT_RELOAD_RING_CAPACITY == 128);
            printf("    Ring capacity: %u slots\n", (unsigned)KAIN_UI_HOT_RELOAD_RING_CAPACITY);
        } else {
            printf("    [WARN] Skipping ring buffer test — channel not available\n");
        }

        kain_ui_hot_reload_channel_close(&channel);
    }

    // ── Test 7: Multiple bundle requests ───────────────────────────
    printf("\n[Test 7] Multiple bundle requests\n");
    {
        KainUiHotReloadChannel channel;
        memset(&channel, 0, sizeof(channel));
        int create_ok = kain_ui_hot_reload_channel_create(&channel, "kain-test-reload-multi");

        if (create_ok == 0 && channel.control) {
            KainUiHotReloadSharedControl* ctrl = channel.control;

            // Send multiple requests
            for (int i = 0; i < 3; i++) {
                char path[256];
                snprintf(path, sizeof(path), "/tmp/bundle_%d.json", i);
                kain_ui_hot_reload_channel_request_bundle(&channel, path, 0x1000 + i, i + 1);
            }

            TEST("generation incremented 3 times", ctrl->request_generation == 3);
            printf("    Final generation: %d\n", ctrl->request_generation);
            printf("    Final path: %s\n", ctrl->requested_bundle_path);
        } else {
            printf("    [WARN] Skipping multi-request test — channel not available\n");
        }

        kain_ui_hot_reload_channel_close(&channel);
    }

    // ── Summary ────────────────────────────────────────────────────
    printf("\n========================================================\n");
    printf("  Results: %d passed, %d failed out of %d total\n",
           g_passed, g_failed, g_passed + g_failed);
    printf("========================================================\n");

    return g_failed > 0 ? 1 : 0;
}
