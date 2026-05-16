#ifndef KAIN_UI_HOT_RELOAD_H
#define KAIN_UI_HOT_RELOAD_H

#include "ui_runtime.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_UI_HOT_RELOAD_CHANNEL_ENV "ABI_UI_HOT_RELOAD_CHANNEL"
#define KAIN_UI_HOT_RELOAD_POLL_INTERVAL_ENV "ABI_UI_HOT_RELOAD_POLL_INTERVAL_MS"
#define KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX 128
#define KAIN_UI_HOT_RELOAD_BUNDLE_PATH_MAX 512
#define KAIN_UI_HOT_RELOAD_EVENT_TEXT_MAX 256
#define KAIN_UI_HOT_RELOAD_RING_CAPACITY 128u
#define KAIN_UI_HOT_RELOAD_RING_MASK (KAIN_UI_HOT_RELOAD_RING_CAPACITY - 1u)
#define KAIN_UI_HOT_RELOAD_POLL_INTERVAL_MS_DEFAULT 125u
#define KAIN_UI_HOT_RELOAD_SHARED_MAGIC 0x4B48524Cu
#define KAIN_UI_HOT_RELOAD_SHARED_VERSION 1u

#if (KAIN_UI_HOT_RELOAD_RING_CAPACITY & (KAIN_UI_HOT_RELOAD_RING_CAPACITY - 1u)) != 0
#error "KAIN_UI_HOT_RELOAD_RING_CAPACITY must be a power of two"
#endif

typedef enum {
    KAIN_UI_HOT_RELOAD_EVENT_NONE = 0,
    KAIN_UI_HOT_RELOAD_EVENT_REQUESTED = 1,
    KAIN_UI_HOT_RELOAD_EVENT_APPLIED = 2,
    KAIN_UI_HOT_RELOAD_EVENT_REJECTED = 3,
    KAIN_UI_HOT_RELOAD_EVENT_INFO = 4,
} KainUiHotReloadEventKind;

typedef struct {
    uint64_t sequence;
    uint64_t fingerprint;
    uint32_t generation;
    uint32_t kind;
    char text[KAIN_UI_HOT_RELOAD_EVENT_TEXT_MAX];
} KainUiHotReloadEvent;

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile int32_t request_generation;
    volatile int32_t applied_generation;
    volatile int32_t failed_generation;
    volatile int32_t reserved0;
    volatile int64_t event_sequence;
    uint64_t requested_fingerprint;
    uint64_t applied_fingerprint;
    uint64_t failed_fingerprint;
    uint64_t watched_file_signature;
    char requested_bundle_path[KAIN_UI_HOT_RELOAD_BUNDLE_PATH_MAX];
    char last_status[KAIN_UI_HOT_RELOAD_EVENT_TEXT_MAX];
    char last_error[KAIN_UI_HOT_RELOAD_EVENT_TEXT_MAX];
    KainUiHotReloadEvent events[KAIN_UI_HOT_RELOAD_RING_CAPACITY];
} KainUiHotReloadSharedControl;

typedef struct {
    int initialized;
    int owner;
    intptr_t platform_handle;
    void* platform_view;
    char channel_name[KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX];
    KainUiHotReloadSharedControl* control;
} KainUiHotReloadChannel;

typedef struct {
    int initialized;
    int file_watch_enabled;
    int channel_enabled;
    uint32_t poll_interval_ms;
    uint32_t reserved0;
    uint64_t last_poll_clock_ms;
    uint64_t last_seen_file_signature;
    uint64_t last_applied_file_signature;
    int32_t last_observed_request_generation;
    int32_t last_applied_generation;
    char app_name[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    char bundle_path[KAIN_UI_HOT_RELOAD_BUNDLE_PATH_MAX];
    char last_status[KAIN_UI_HOT_RELOAD_EVENT_TEXT_MAX];
    char last_error[KAIN_UI_HOT_RELOAD_EVENT_TEXT_MAX];
    KainUiHotReloadChannel channel;
} KainUiHotReloadController;

void kain_ui_hot_reload_channel_init(KainUiHotReloadChannel* channel);
int kain_ui_hot_reload_channel_create(KainUiHotReloadChannel* channel, const char* channel_name);
int kain_ui_hot_reload_channel_open(KainUiHotReloadChannel* channel, const char* channel_name);
void kain_ui_hot_reload_channel_close(KainUiHotReloadChannel* channel);
int kain_ui_hot_reload_channel_request_bundle(
    KainUiHotReloadChannel* channel,
    const char* bundle_path,
    uint64_t fingerprint,
    int32_t generation_hint
);

void kain_ui_hot_reload_controller_init(KainUiHotReloadController* controller);
int kain_ui_hot_reload_controller_boot(
    KainUiHotReloadController* controller,
    const char* app_name,
    const char* bundle_env_name
);
void kain_ui_hot_reload_controller_shutdown(KainUiHotReloadController* controller);
int kain_ui_hot_reload_controller_apply_pending(
    KainUiHotReloadController* controller,
    KainUiRuntimeState* runtime_state,
    KainUiCompiledBundle* compiled_bundle,
    const KainUiRuntimeReloadOptions* options,
    KainUiRuntimeReloadReport* report
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_UI_HOT_RELOAD_H */
