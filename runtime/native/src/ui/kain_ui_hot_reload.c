#include "../../include/kain_ui_hot_reload.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

static void kain_ui_hot_reload_copy_string(char* out, size_t out_cap, const char* value) {
    if (!out || out_cap == 0u) {
        return;
    }

    if (!value) {
        out[0] = '\0';
        return;
    }

    snprintf(out, out_cap, "%s", value);
}

static void kain_ui_hot_reload_sanitize_name(
    const char* input,
    char* out,
    size_t out_cap
) {
    size_t read_index = 0u;
    size_t write_index = 0u;

    if (!out || out_cap == 0u) {
        return;
    }

    out[0] = '\0';
    if (!input || !input[0]) {
        kain_ui_hot_reload_copy_string(out, out_cap, "kain-ui");
        return;
    }

    while (input[read_index] && write_index + 1u < out_cap) {
        unsigned char ch = (unsigned char)input[read_index];
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '_' ||
            ch == '.') {
            out[write_index++] = (char)ch;
        } else {
            out[write_index++] = '_';
        }
        read_index += 1u;
    }
    out[write_index] = '\0';
    if (!out[0]) {
        kain_ui_hot_reload_copy_string(out, out_cap, "kain-ui");
    }
}

static void kain_ui_hot_reload_make_default_channel_name(
    const char* app_name,
    char* out,
    size_t out_cap
) {
    char sanitized[KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX];

    kain_ui_hot_reload_sanitize_name(app_name, sanitized, sizeof(sanitized));
    snprintf(out, out_cap, "kain-ui-reload.%s", sanitized);
}

static void kain_ui_hot_reload_make_platform_channel_name(
    const char* logical_name,
    char* out,
    size_t out_cap
) {
#ifdef _WIN32
    snprintf(out, out_cap, "Local\\%s", logical_name ? logical_name : "kain-ui-reload.kain-ui");
#else
    snprintf(out, out_cap, "/%s", logical_name ? logical_name : "kain-ui-reload.kain-ui");
#endif
}

static int32_t kain_ui_hot_reload_atomic_load_i32(volatile int32_t* value) {
#ifdef _WIN32
    return (int32_t)InterlockedCompareExchange((volatile LONG*)value, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
#else
    return value ? *value : 0;
#endif
}

static void kain_ui_hot_reload_atomic_store_i32(volatile int32_t* value, int32_t next_value) {
    if (!value) {
        return;
    }
#ifdef _WIN32
    InterlockedExchange((volatile LONG*)value, (LONG)next_value);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(value, next_value, __ATOMIC_SEQ_CST);
#else
    *value = next_value;
#endif
}

static int64_t kain_ui_hot_reload_atomic_increment_i64(volatile int64_t* value) {
#ifdef _WIN32
    return (int64_t)InterlockedIncrement64((volatile LONG64*)value);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
#else
    *value += 1;
    return *value;
#endif
}

static uint64_t kain_ui_hot_reload_hash_bytes(uint64_t seed, const void* bytes, size_t byte_length) {
    const unsigned char* cursor = (const unsigned char*)bytes;
    size_t index;
    uint64_t hash = seed;

    if (!bytes) {
        return hash;
    }

    for (index = 0u; index < byte_length; ++index) {
        hash ^= (uint64_t)cursor[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static uint64_t kain_ui_hot_reload_make_fingerprint(const char* path, uint64_t file_signature) {
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = kain_ui_hot_reload_hash_bytes(hash, path, path ? strlen(path) : 0u);
    hash = kain_ui_hot_reload_hash_bytes(hash, &file_signature, sizeof(file_signature));
    return hash ? hash : 1u;
}

static uint64_t kain_ui_hot_reload_now_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0u;
    }
    return ((uint64_t)now.tv_sec * UINT64_C(1000)) + (uint64_t)(now.tv_nsec / 1000000u);
#endif
}

static int kain_ui_hot_reload_read_file_signature(const char* path, uint64_t* out_signature) {
    uint64_t hash;

    if (!path || !path[0] || !out_signature) {
        return 0;
    }

#ifdef _WIN32
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes;
        ULARGE_INTEGER size;
        ULARGE_INTEGER last_write;

        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attributes)) {
            return 0;
        }

        size.LowPart = attributes.nFileSizeLow;
        size.HighPart = attributes.nFileSizeHigh;
        last_write.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
        last_write.HighPart = attributes.ftLastWriteTime.dwHighDateTime;

        hash = UINT64_C(1469598103934665603);
        hash = kain_ui_hot_reload_hash_bytes(hash, path, strlen(path));
        hash = kain_ui_hot_reload_hash_bytes(hash, &size.QuadPart, sizeof(size.QuadPart));
        hash = kain_ui_hot_reload_hash_bytes(hash, &last_write.QuadPart, sizeof(last_write.QuadPart));
    }
#else
    {
        struct stat attributes;
        uint64_t file_size;
        uint64_t seconds;
        uint64_t nanos;

        if (stat(path, &attributes) != 0) {
            return 0;
        }

        file_size = (uint64_t)attributes.st_size;
#if defined(__APPLE__)
        seconds = (uint64_t)attributes.st_mtimespec.tv_sec;
        nanos = (uint64_t)attributes.st_mtimespec.tv_nsec;
#else
        seconds = (uint64_t)attributes.st_mtim.tv_sec;
        nanos = (uint64_t)attributes.st_mtim.tv_nsec;
#endif
        hash = UINT64_C(1469598103934665603);
        hash = kain_ui_hot_reload_hash_bytes(hash, path, strlen(path));
        hash = kain_ui_hot_reload_hash_bytes(hash, &file_size, sizeof(file_size));
        hash = kain_ui_hot_reload_hash_bytes(hash, &seconds, sizeof(seconds));
        hash = kain_ui_hot_reload_hash_bytes(hash, &nanos, sizeof(nanos));
    }
#endif

    *out_signature = hash ? hash : 1u;
    return 1;
}

static void kain_ui_hot_reload_shared_control_init(KainUiHotReloadSharedControl* control) {
    if (!control) {
        return;
    }

    memset(control, 0, sizeof(*control));
    control->magic = KAIN_UI_HOT_RELOAD_SHARED_MAGIC;
    control->version = KAIN_UI_HOT_RELOAD_SHARED_VERSION;
}

static void kain_ui_hot_reload_channel_push_event(
    KainUiHotReloadChannel* channel,
    KainUiHotReloadEventKind kind,
    int32_t generation,
    uint64_t fingerprint,
    const char* text
) {
    int64_t next_sequence;
    KainUiHotReloadEvent* event;
    uint32_t ring_index;

    if (!channel || !channel->control) {
        return;
    }

    next_sequence = kain_ui_hot_reload_atomic_increment_i64(&channel->control->event_sequence) - 1;
    ring_index = (uint32_t)((uint64_t)next_sequence & KAIN_UI_HOT_RELOAD_RING_MASK);
    event = &channel->control->events[ring_index];
    memset(event, 0, sizeof(*event));
    event->sequence = (uint64_t)next_sequence;
    event->fingerprint = fingerprint;
    event->generation = generation >= 0 ? (uint32_t)generation : 0u;
    event->kind = (uint32_t)kind;
    kain_ui_hot_reload_copy_string(event->text, sizeof(event->text), text);
}

void kain_ui_hot_reload_channel_init(KainUiHotReloadChannel* channel) {
    if (!channel) {
        return;
    }

    memset(channel, 0, sizeof(*channel));
#ifdef _WIN32
    channel->platform_handle = (intptr_t)0;
#else
    channel->platform_handle = (intptr_t)-1;
#endif
    channel->platform_view = NULL;
    channel->control = NULL;
}

static int kain_ui_hot_reload_channel_map_shared_control(
    KainUiHotReloadChannel* channel,
    const char* channel_name,
    int create
) {
    char platform_name[KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX + 32];
    size_t mapping_size = sizeof(KainUiHotReloadSharedControl);

    if (!channel || !channel_name || !channel_name[0]) {
        return 0;
    }

    kain_ui_hot_reload_make_platform_channel_name(channel_name, platform_name, sizeof(platform_name));

#ifdef _WIN32
    {
        HANDLE mapping_handle = NULL;
        void* mapping_view = NULL;
        int owner = 0;

        if (create) {
            mapping_handle = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                NULL,
                PAGE_READWRITE,
                0,
                (DWORD)mapping_size,
                platform_name
            );
            if (!mapping_handle) {
                return 0;
            }
            owner = (GetLastError() != ERROR_ALREADY_EXISTS);
        } else {
            mapping_handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, platform_name);
            if (!mapping_handle) {
                return 0;
            }
        }

        mapping_view = MapViewOfFile(mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, mapping_size);
        if (!mapping_view) {
            CloseHandle(mapping_handle);
            return 0;
        }

        channel->initialized = 1;
        channel->owner = owner;
        channel->platform_handle = (intptr_t)mapping_handle;
        channel->platform_view = mapping_view;
        channel->control = (KainUiHotReloadSharedControl*)mapping_view;
    }
#else
    {
        int open_flags = O_RDWR | (create ? O_CREAT : 0);
        int file_descriptor = shm_open(platform_name, open_flags, 0600);
        void* mapping_view = NULL;
        int owner = 0;
        struct stat mapping_stat;

        if (file_descriptor < 0) {
            return 0;
        }

        if (create) {
            if (ftruncate(file_descriptor, (off_t)mapping_size) != 0) {
                close(file_descriptor);
                return 0;
            }
            owner = 1;
        } else {
            if (fstat(file_descriptor, &mapping_stat) != 0 ||
                (size_t)mapping_stat.st_size < mapping_size) {
                close(file_descriptor);
                return 0;
            }
        }

        mapping_view = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
        if (mapping_view == MAP_FAILED) {
            close(file_descriptor);
            return 0;
        }

        channel->initialized = 1;
        channel->owner = owner;
        channel->platform_handle = (intptr_t)file_descriptor;
        channel->platform_view = mapping_view;
        channel->control = (KainUiHotReloadSharedControl*)mapping_view;
    }
#endif

    kain_ui_hot_reload_copy_string(channel->channel_name, sizeof(channel->channel_name), channel_name);
    if (create) {
        if (channel->owner ||
            channel->control->magic != KAIN_UI_HOT_RELOAD_SHARED_MAGIC ||
            channel->control->version != KAIN_UI_HOT_RELOAD_SHARED_VERSION) {
            kain_ui_hot_reload_shared_control_init(channel->control);
        }
    } else if (channel->control->magic != KAIN_UI_HOT_RELOAD_SHARED_MAGIC ||
               channel->control->version != KAIN_UI_HOT_RELOAD_SHARED_VERSION) {
        kain_ui_hot_reload_channel_close(channel);
        return 0;
    }

    return 1;
}

int kain_ui_hot_reload_channel_create(KainUiHotReloadChannel* channel, const char* channel_name) {
    kain_ui_hot_reload_channel_init(channel);
    return kain_ui_hot_reload_channel_map_shared_control(channel, channel_name, 1);
}

int kain_ui_hot_reload_channel_open(KainUiHotReloadChannel* channel, const char* channel_name) {
    kain_ui_hot_reload_channel_init(channel);
    return kain_ui_hot_reload_channel_map_shared_control(channel, channel_name, 0);
}

void kain_ui_hot_reload_channel_close(KainUiHotReloadChannel* channel) {
    char platform_name[KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX + 32];

    if (!channel || !channel->initialized) {
        return;
    }

    kain_ui_hot_reload_make_platform_channel_name(channel->channel_name, platform_name, sizeof(platform_name));
#ifdef _WIN32
    if (channel->platform_view) {
        UnmapViewOfFile(channel->platform_view);
    }
    if (channel->platform_handle) {
        CloseHandle((HANDLE)channel->platform_handle);
    }
#else
    if (channel->platform_view) {
        munmap(channel->platform_view, sizeof(KainUiHotReloadSharedControl));
    }
    if (channel->platform_handle >= 0) {
        close((int)channel->platform_handle);
    }
    if (channel->owner && platform_name[0]) {
        shm_unlink(platform_name);
    }
#endif
    kain_ui_hot_reload_channel_init(channel);
}

int kain_ui_hot_reload_channel_request_bundle(
    KainUiHotReloadChannel* channel,
    const char* bundle_path,
    uint64_t fingerprint,
    int32_t generation_hint
) {
    int32_t request_generation;
    int32_t applied_generation;
    int32_t failed_generation;

    if (!channel || !channel->control || !bundle_path || !bundle_path[0]) {
        return 0;
    }

    request_generation = generation_hint;
    if (request_generation <= 0) {
        request_generation = kain_ui_hot_reload_atomic_load_i32(&channel->control->request_generation);
        applied_generation = kain_ui_hot_reload_atomic_load_i32(&channel->control->applied_generation);
        failed_generation = kain_ui_hot_reload_atomic_load_i32(&channel->control->failed_generation);
        if (applied_generation > request_generation) {
            request_generation = applied_generation;
        }
        if (failed_generation > request_generation) {
            request_generation = failed_generation;
        }
        request_generation += 1;
    }

    kain_ui_hot_reload_copy_string(
        channel->control->requested_bundle_path,
        sizeof(channel->control->requested_bundle_path),
        bundle_path
    );
    channel->control->requested_fingerprint = fingerprint;
    kain_ui_hot_reload_copy_string(
        channel->control->last_status,
        sizeof(channel->control->last_status),
        "bundle reload requested"
    );
    channel->control->last_error[0] = '\0';
    kain_ui_hot_reload_atomic_store_i32(&channel->control->request_generation, request_generation);
    kain_ui_hot_reload_channel_push_event(
        channel,
        KAIN_UI_HOT_RELOAD_EVENT_REQUESTED,
        request_generation,
        fingerprint,
        bundle_path
    );
    return request_generation;
}

void kain_ui_hot_reload_controller_init(KainUiHotReloadController* controller) {
    if (!controller) {
        return;
    }

    memset(controller, 0, sizeof(*controller));
    kain_ui_hot_reload_channel_init(&controller->channel);
}

int kain_ui_hot_reload_controller_boot(
    KainUiHotReloadController* controller,
    const char* app_name,
    const char* bundle_env_name
) {
    char channel_name[KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX];
    char* env_channel_name = NULL;
    char* env_bundle_path = NULL;
    uint64_t bundle_signature = 0u;
    int poll_interval_ms;

    if (!controller) {
        return 0;
    }

    kain_ui_hot_reload_controller_init(controller);
    kain_ui_hot_reload_copy_string(
        controller->app_name,
        sizeof(controller->app_name),
        app_name ? app_name : "kain-ui"
    );

    poll_interval_ms = kain_env_int(
        KAIN_UI_HOT_RELOAD_POLL_INTERVAL_ENV,
        (int)KAIN_UI_HOT_RELOAD_POLL_INTERVAL_MS_DEFAULT
    );
    if (poll_interval_ms < 16) {
        poll_interval_ms = 16;
    }
    controller->poll_interval_ms = (uint32_t)poll_interval_ms;

    env_bundle_path = kain_env_dup(bundle_env_name ? bundle_env_name : KAIN_UI_COMPILED_BUNDLE_ENV);
    if (env_bundle_path && env_bundle_path[0]) {
        kain_ui_hot_reload_copy_string(
            controller->bundle_path,
            sizeof(controller->bundle_path),
            env_bundle_path
        );
        controller->file_watch_enabled = 1;
        if (kain_ui_hot_reload_read_file_signature(controller->bundle_path, &bundle_signature)) {
            controller->last_seen_file_signature = bundle_signature;
            controller->last_applied_file_signature = bundle_signature;
        }
    }
    kain_env_free(env_bundle_path);

    env_channel_name = kain_env_dup(KAIN_UI_HOT_RELOAD_CHANNEL_ENV);
    if (env_channel_name && env_channel_name[0]) {
        kain_ui_hot_reload_copy_string(channel_name, sizeof(channel_name), env_channel_name);
    } else {
        kain_ui_hot_reload_make_default_channel_name(controller->app_name, channel_name, sizeof(channel_name));
    }
    kain_env_free(env_channel_name);

    if (kain_ui_hot_reload_channel_create(&controller->channel, channel_name)) {
        controller->channel_enabled = 1;
        controller->channel.control->watched_file_signature = controller->last_seen_file_signature;
        kain_ui_hot_reload_channel_push_event(
            &controller->channel,
            KAIN_UI_HOT_RELOAD_EVENT_INFO,
            0,
            controller->last_seen_file_signature,
            controller->file_watch_enabled
                ? "hot reload controller booted with bundle watch"
                : "hot reload controller booted without initial bundle path"
        );
    }

    if (controller->file_watch_enabled) {
        snprintf(
            controller->last_status,
            sizeof(controller->last_status),
            "watching %s",
            controller->bundle_path
        );
    } else {
        kain_ui_hot_reload_copy_string(
            controller->last_status,
            sizeof(controller->last_status),
            "waiting for bundle path"
        );
    }

    controller->initialized = 1;
    return 1;
}

void kain_ui_hot_reload_controller_shutdown(KainUiHotReloadController* controller) {
    if (!controller) {
        return;
    }

    kain_ui_hot_reload_channel_close(&controller->channel);
    controller->initialized = 0;
}

int kain_ui_hot_reload_controller_apply_pending(
    KainUiHotReloadController* controller,
    KainUiRuntimeState* runtime_state,
    KainUiCompiledBundle* compiled_bundle,
    const KainUiRuntimeReloadOptions* options,
    KainUiRuntimeReloadReport* report
) {
    KainUiRuntimeReloadReport local_report;
    KainUiRuntimeReloadReport* active_report = report ? report : &local_report;
    uint64_t now_ms;
    uint64_t watched_signature = 0u;
    uint64_t request_fingerprint = 0u;
    uint64_t request_file_signature = 0u;
    char request_path[KAIN_UI_HOT_RELOAD_BUNDLE_PATH_MAX];
    int32_t request_generation = 0;
    int request_from_channel = 0;

    if (!controller || !controller->initialized || !runtime_state || !compiled_bundle) {
        return 0;
    }

    now_ms = kain_ui_hot_reload_now_ms();
    if (controller->last_poll_clock_ms != 0u &&
        now_ms != 0u &&
        (now_ms - controller->last_poll_clock_ms) < controller->poll_interval_ms) {
        return 0;
    }
    controller->last_poll_clock_ms = now_ms;

    request_path[0] = '\0';
    if (controller->channel.control) {
        int32_t observed_generation =
            kain_ui_hot_reload_atomic_load_i32(&controller->channel.control->request_generation);
        if (observed_generation > controller->last_observed_request_generation &&
            controller->channel.control->requested_bundle_path[0]) {
            request_generation = observed_generation;
            request_fingerprint = controller->channel.control->requested_fingerprint;
            kain_ui_hot_reload_copy_string(
                request_path,
                sizeof(request_path),
                controller->channel.control->requested_bundle_path
            );
            if (kain_ui_hot_reload_read_file_signature(request_path, &request_file_signature)) {
                controller->channel.control->watched_file_signature = request_file_signature;
                if (request_fingerprint == 0u) {
                    request_fingerprint = kain_ui_hot_reload_make_fingerprint(
                        request_path,
                        request_file_signature
                    );
                }
            }
            request_from_channel = 1;
        }
    }

    if (!request_path[0] &&
        controller->file_watch_enabled &&
        controller->bundle_path[0] &&
        kain_ui_hot_reload_read_file_signature(controller->bundle_path, &watched_signature) &&
        watched_signature != 0u &&
        watched_signature != controller->last_seen_file_signature &&
        watched_signature != controller->last_applied_file_signature) {
        request_generation = controller->last_applied_generation + 1;
        request_file_signature = watched_signature;
        request_fingerprint = kain_ui_hot_reload_make_fingerprint(controller->bundle_path, watched_signature);
        kain_ui_hot_reload_copy_string(request_path, sizeof(request_path), controller->bundle_path);
    }

    if (!request_path[0]) {
        return 0;
    }

    kain_ui_runtime_reload_report_init(active_report);
    if (!kain_ui_runtime_reload_from_path(runtime_state, request_path, options, active_report)) {
        if (!request_from_channel) {
            controller->last_seen_file_signature = request_file_signature;
        } else {
            controller->last_observed_request_generation = request_generation;
        }
        kain_ui_hot_reload_copy_string(controller->last_error, sizeof(controller->last_error), active_report->summary);
        if (controller->channel.control) {
            controller->channel.control->failed_fingerprint = request_fingerprint;
            kain_ui_hot_reload_atomic_store_i32(
                &controller->channel.control->failed_generation,
                request_generation
            );
            kain_ui_hot_reload_copy_string(
                controller->channel.control->last_error,
                sizeof(controller->channel.control->last_error),
                active_report->summary
            );
            kain_ui_hot_reload_channel_push_event(
                &controller->channel,
                KAIN_UI_HOT_RELOAD_EVENT_REJECTED,
                request_generation,
                request_fingerprint,
                active_report->summary
            );
        }
        return 0;
    }

    *compiled_bundle = runtime_state->bundle;
    controller->last_applied_generation = request_generation;
    controller->last_observed_request_generation = request_generation;
    if (request_file_signature != 0u) {
        controller->last_applied_file_signature = request_file_signature;
        controller->last_seen_file_signature = request_file_signature;
    }
    controller->file_watch_enabled = 1;
    kain_ui_hot_reload_copy_string(controller->bundle_path, sizeof(controller->bundle_path), request_path);
    kain_ui_hot_reload_copy_string(controller->last_status, sizeof(controller->last_status), active_report->summary);
    controller->last_error[0] = '\0';

    if (controller->channel.control) {
        controller->channel.control->applied_fingerprint = request_fingerprint;
        controller->channel.control->watched_file_signature = request_file_signature;
        kain_ui_hot_reload_atomic_store_i32(
            &controller->channel.control->applied_generation,
            request_generation
        );
        kain_ui_hot_reload_copy_string(
            controller->channel.control->last_status,
            sizeof(controller->channel.control->last_status),
            active_report->summary
        );
        controller->channel.control->last_error[0] = '\0';
        kain_ui_hot_reload_channel_push_event(
            &controller->channel,
            KAIN_UI_HOT_RELOAD_EVENT_APPLIED,
            request_generation,
            request_fingerprint,
            active_report->summary
        );
    }

    return 1;
}
