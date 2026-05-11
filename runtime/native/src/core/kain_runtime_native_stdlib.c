#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/kain_runtime_native_stdlib.h"

#include "../../include/kain_runtime_actor.h"
#include "../../include/kain_runtime_base.h"
#include "../../include/kain_runtime_diagnostics.h"
#include "../../include/kain_runtime_entangle.h"

#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

void* kain_alloc_rc(size_t size, long long type_tag);

static KainDiagnostic kain_native_diag(void) {
    KainDiagnostic diag;
    kain_diagnostic_init(&diag);
    return diag;
}

int64_t kain_native_runtime_init(void) {
    kain_actor_runtime_init();
    return 0;
}

int64_t kain_native_runtime_shutdown(void) {
    kain_actor_runtime_shutdown();
    return 0;
}

int64_t kain_native_actor_invalid_id(void) {
    return (int64_t)KAIN_ACTOR_ID_INVALID;
}

int64_t kain_native_actor_default_mailbox_capacity(void) {
    return (int64_t)KAIN_MAILBOX_DEFAULT_CAPACITY;
}

int64_t kain_native_actor_unbounded_mailbox_capacity(void) {
    return (int64_t)KAIN_MAILBOX_UNBOUNDED_CAPACITY;
}

static unsigned long long kain_native_hash_message_name(const char* value) {
    unsigned long long hash = 1469598103934665603ULL;
    if (value == 0) {
        return hash;
    }
    while (*value != '\0') {
        hash ^= (unsigned char)(*value);
        hash *= 1099511628211ULL;
        value++;
    }
    return hash;
}

static void kain_native_copy_actor_name(char* destination, size_t destination_size, const char* source) {
    size_t index = 0;
    if (destination == 0 || destination_size == 0) {
        return;
    }
    if (source != 0) {
        while (source[index] != '\0' && index + 1 < destination_size) {
            destination[index] = source[index];
            index++;
        }
    }
    destination[index] = '\0';
}

static KainActorExitReason kain_native_actor_default_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)user_data;

    for (;;) {
        KainActorMessage message;
        KainDiagnostic diag = kain_native_diag();
        if (kain_actor_receive(mailbox, &message, &diag) != 0) {
            return KAIN_ACTOR_EXIT_NORMAL;
        }
        if (message.data != 0) {
            free(message.data);
        }
    }
}

int64_t kain_native_actor_spawn(const char* actor_name, const char* init_payload) {
    KainDiagnostic diag = kain_native_diag();
    KainActorSpawnConfig config;
    (void)init_payload;

    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = kain_native_actor_default_bootstrap;
    config.user_data = 0;
    config.mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    kain_native_copy_actor_name(config.name, sizeof(config.name), actor_name);

    return (int64_t)kain_actor_spawn(&config, &diag);
}

int64_t kain_native_actor_send(int64_t actor_id, const char* message_name, const char* data_payload) {
    KainDiagnostic diag = kain_native_diag();
    KainActorMessage message;
    message.type_tag = kain_native_hash_message_name(message_name);
    message.data = (void*)data_payload;
    message.data_size = data_payload == 0 ? 0 : strlen(data_payload) + 1;
    message.sender_id = KAIN_ACTOR_ID_INVALID;
    return (int64_t)kain_actor_send((KainActorId)actor_id, &message, &diag);
}

int kain_native_actor_state_invalid(int64_t actor_id) {
    return actor_id <= 0 || kain_actor_get_state((KainActorId)actor_id) == KAIN_ACTOR_STATE_UNINITIALIZED;
}

int64_t kain_native_actor_get_state(int64_t actor_id) {
    return (int64_t)kain_actor_get_state((KainActorId)actor_id);
}

int64_t kain_native_actor_shutdown(int64_t actor_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_shutdown((KainActorId)actor_id, &diag);
}

int64_t kain_native_actor_kill(int64_t actor_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_kill((KainActorId)actor_id, &diag);
}

int64_t kain_native_actor_registry_lookup(const char* name) {
    if (name == 0 || name[0] == '\0') {
        return (int64_t)KAIN_ACTOR_ID_INVALID;
    }
    return (int64_t)kain_actor_registry_lookup(name);
}

int64_t kain_native_actor_registry_register(const char* name, int64_t actor_id) {
    KainDiagnostic diag = kain_native_diag();
    if (name == 0 || name[0] == '\0' || actor_id <= 0) {
        return -1;
    }
    return (int64_t)kain_actor_registry_register(name, (KainActorId)actor_id, &diag);
}

int64_t kain_native_actor_registry_unregister(const char* name) {
    KainDiagnostic diag = kain_native_diag();
    if (name == 0 || name[0] == '\0') {
        return -1;
    }
    return (int64_t)kain_actor_registry_unregister(name, &diag);
}

int64_t kain_native_actor_monitor(int64_t monitor_id, int64_t monitored_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_monitor((KainActorId)monitor_id, (KainActorId)monitored_id, &diag);
}

int64_t kain_native_actor_demonitor(int64_t monitor_id, int64_t monitored_id) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_demonitor((KainActorId)monitor_id, (KainActorId)monitored_id, &diag);
}

int64_t kain_native_actor_link(int64_t actor_a, int64_t actor_b) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_link((KainActorId)actor_a, (KainActorId)actor_b, &diag);
}

int64_t kain_native_actor_unlink(int64_t actor_a, int64_t actor_b) {
    KainDiagnostic diag = kain_native_diag();
    return (int64_t)kain_actor_unlink((KainActorId)actor_a, (KainActorId)actor_b, &diag);
}

static int kain_native_actor_supervision_snapshot(
    int64_t actor_id,
    KainActorSupervisionSnapshot* snapshot
) {
    KainDiagnostic diag = kain_native_diag();
    if (snapshot == 0) {
        return -1;
    }
    return kain_actor_get_supervision_snapshot((KainActorId)actor_id, snapshot, &diag);
}

int64_t kain_native_actor_supervision_observed_child_exit_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.observed_child_exit_count;
}

int64_t kain_native_actor_supervision_restart_attempt_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.restart_attempt_count;
}

int64_t kain_native_actor_supervision_escalation_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.escalation_count;
}

int kain_native_actor_supervision_limit_hit(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (kain_native_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return 0;
    }
    return snapshot.restart_limit_hit != 0 || snapshot.supervision_limit_hits != 0;
}

static KainActorSchedulerSnapshot kain_native_actor_scheduler_snapshot(void) {
    KainActorSchedulerSnapshot snapshot;
    kain_actor_scheduler_snapshot(&snapshot);
    return snapshot;
}

int64_t kain_native_actor_scheduler_queue_depth(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().queue_depth;
}

int64_t kain_native_actor_scheduler_max_queue_depth(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().max_queue_depth;
}

int64_t kain_native_actor_scheduler_total_enqueued(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().total_enqueued;
}

int64_t kain_native_actor_scheduler_total_dequeued(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().total_dequeued;
}

int64_t kain_native_actor_scheduler_worker_count(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().worker_count;
}

int64_t kain_native_actor_scheduler_active_workers(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().active_workers;
}

int64_t kain_native_actor_scheduler_busy_workers(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().busy_workers;
}

int64_t kain_native_actor_scheduler_overflow_thread_spawns(void) {
    return (int64_t)kain_native_actor_scheduler_snapshot().overflow_thread_spawns;
}

int64_t kain_native_entangle_reset(void) {
    kain_runtime_entangle_registry_reset();
    return 0;
}

int64_t kain_native_entangle_registered_count(void) {
    return (int64_t)kain_runtime_entangle_registered_count();
}

int64_t kain_native_entangle_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
) {
    return (int64_t)kain_runtime_entangle_register(authority, mirror, policy, type_name);
}

static const KainRuntimeEntangleBinding* kain_native_entangle_binding_at(int64_t index) {
    static KainRuntimeEntangleBinding binding;
    if (index < 0) {
        return 0;
    }
    if (kain_runtime_entangle_get((size_t)index, &binding) != 0) {
        return 0;
    }
    return &binding;
}

const char* kain_native_entangle_get_authority(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->authority : "";
}

const char* kain_native_entangle_get_mirror(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->mirror : "";
}

const char* kain_native_entangle_get_policy(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->policy : "";
}

const char* kain_native_entangle_get_type_name(int64_t index) {
    const KainRuntimeEntangleBinding* binding = kain_native_entangle_binding_at(index);
    return binding ? binding->type_name : "";
}

int64_t kain_native_now_millis(void) {
    return (int64_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

int64_t kain_native_sleep_millis(int64_t milliseconds) {
    if (milliseconds < 0) {
        return -1;
    }
    Sleep((unsigned int)milliseconds);
    return 0;
}

static int64_t g_kain_native_fs_last_status = 0;
static char g_kain_native_fs_last_error_kind[64] = "ok";
static char g_kain_native_fs_last_error_message[512] = "";

static char* kain_native_fs_string_with_len(const char* source, size_t length) {
    char* result = (char*)kain_alloc_rc(length + 1, 1);
    if (result == 0) {
        return 0;
    }
    if (source != 0 && length > 0) {
        memcpy(result, source, length);
    }
    result[length] = '\0';
    return result;
}

static void kain_native_fs_copy_message(char* destination, size_t destination_size, const char* source) {
    if (destination == 0 || destination_size == 0) {
        return;
    }
    if (source == 0) {
        source = "";
    }
#ifdef _WIN32
    strncpy_s(destination, destination_size, source, _TRUNCATE);
#else
    strncpy(destination, source, destination_size - 1);
    destination[destination_size - 1] = '\0';
#endif
}

static const char* kain_native_fs_errno_kind(int error_code) {
    switch (error_code) {
        case 0:
            return "ok";
        case ENOENT:
            return "not_found";
        case EACCES:
            return "access_denied";
        case EEXIST:
            return "already_exists";
        case EINVAL:
            return "invalid_input";
#ifdef ENOTDIR
        case ENOTDIR:
            return "not_a_directory";
#endif
#ifdef EISDIR
        case EISDIR:
            return "is_directory";
#endif
#ifdef ENOTEMPTY
        case ENOTEMPTY:
            return "directory_not_empty";
#endif
#ifdef EXDEV
        case EXDEV:
            return "cross_device";
#endif
        default:
            return "other";
    }
}

static int64_t kain_native_fs_fail(const char* operation, const char* path) {
    int error_code = errno;
    char message[512];
    const char* kind = kain_native_fs_errno_kind(error_code);
    g_kain_native_fs_last_status = error_code == 0 ? -1 : -(int64_t)error_code;
    kain_native_fs_copy_message(g_kain_native_fs_last_error_kind, sizeof(g_kain_native_fs_last_error_kind), kind);
#ifdef _WIN32
    strerror_s(message, sizeof(message), error_code);
#else
    kain_native_fs_copy_message(message, sizeof(message), strerror(error_code));
#endif
    if (operation == 0) {
        operation = "fs";
    }
    if (path == 0) {
        path = "";
    }
    {
        char detail[512];
#ifdef _WIN32
        _snprintf_s(detail, sizeof(detail), _TRUNCATE, "%s failed for '%s': %s", operation, path, message);
#else
        snprintf(detail, sizeof(detail), "%s failed for '%s': %s", operation, path, message);
#endif
        kain_native_fs_copy_message(g_kain_native_fs_last_error_message, sizeof(g_kain_native_fs_last_error_message), detail);
    }
    return g_kain_native_fs_last_status;
}

static int64_t kain_native_fs_ok(void) {
    g_kain_native_fs_last_status = 0;
    kain_native_fs_copy_message(g_kain_native_fs_last_error_kind, sizeof(g_kain_native_fs_last_error_kind), "ok");
    kain_native_fs_copy_message(g_kain_native_fs_last_error_message, sizeof(g_kain_native_fs_last_error_message), "");
    return 0;
}

static int kain_native_fs_path_is_absolute(const char* path) {
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    return strlen(path) > 2 && path[1] == ':';
}

static int64_t kain_native_fs_create_one_dir(const char* path) {
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }
    errno = EACCES;
    return -1;
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
#endif
}

static int64_t kain_native_fs_create_parent_dirs(const char* path) {
    char buffer[4096];
    size_t length;
    size_t index;
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    length = strlen(path);
    if (length >= sizeof(buffer)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(buffer, path, length + 1);
    for (index = 1; index < length; index++) {
        if (buffer[index] == '/' || buffer[index] == '\\') {
            char saved = buffer[index];
            if (index == 2 && buffer[1] == ':') {
                continue;
            }
            buffer[index] = '\0';
            if (buffer[0] != '\0' && kain_native_fs_create_one_dir(buffer) != 0) {
                buffer[index] = saved;
                return -1;
            }
            buffer[index] = saved;
        }
    }
    return 0;
}

static int64_t kain_native_fs_write_mode(const char* path, const char* content, const char* mode) {
    FILE* file = 0;
    size_t length;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return kain_native_fs_fail("write_text", path);
    }
    if (kain_native_fs_create_parent_dirs(path) != 0) {
        return kain_native_fs_fail("create_parent_dirs", path);
    }
#ifdef _WIN32
    if (fopen_s(&file, path, mode) != 0) {
        file = 0;
    }
#else
    file = fopen(path, mode);
#endif
    if (file == 0) {
        return kain_native_fs_fail("write_text", path);
    }
    if (content == 0) {
        content = "";
    }
    length = strlen(content);
    if (length > 0 && fwrite(content, 1, length, file) != length) {
        fclose(file);
        return kain_native_fs_fail("write_text", path);
    }
    fclose(file);
    return kain_native_fs_ok();
}

const char* kain_native_fs_read_text(const char* path) {
    FILE* file = 0;
    long size;
    char* buffer;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        kain_native_fs_fail("read_text", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) {
        file = 0;
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        kain_native_fs_fail("read_text", path);
        return string_new("");
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        kain_native_fs_fail("read_text", path);
        return string_new("");
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        kain_native_fs_fail("read_text", path);
        return string_new("");
    }
    rewind(file);
    buffer = kain_native_fs_string_with_len(0, (size_t)size);
    if (buffer == 0) {
        fclose(file);
        errno = ENOMEM;
        kain_native_fs_fail("read_text", path);
        return string_new("");
    }
    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        kain_native_fs_fail("read_text", path);
        return string_new("");
    }
    fclose(file);
    kain_native_fs_ok();
    return buffer;
}

int64_t kain_native_fs_write_text(const char* path, const char* content) {
    return kain_native_fs_write_mode(path, content, "wb");
}

int64_t kain_native_fs_append_text(const char* path, const char* content) {
    return kain_native_fs_write_mode(path, content, "ab");
}

int kain_native_fs_exists(const char* path) {
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

int kain_native_fs_is_file(const char* path) {
#ifdef _WIN32
    DWORD attrs;
    if (path == 0 || path[0] == '\0') return 0;
    attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat info;
    if (path == 0 || stat(path, &info) != 0) return 0;
    return S_ISREG(info.st_mode);
#endif
}

int kain_native_fs_is_dir(const char* path) {
#ifdef _WIN32
    DWORD attrs;
    if (path == 0 || path[0] == '\0') return 0;
    attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    if (path == 0 || stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
#endif
}

int64_t kain_native_fs_create_dir_all(const char* path) {
    char buffer[4096];
    size_t length;
    size_t index;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return kain_native_fs_fail("create_dir_all", path);
    }
    length = strlen(path);
    if (length >= sizeof(buffer)) {
        errno = ENAMETOOLONG;
        return kain_native_fs_fail("create_dir_all", path);
    }
    memcpy(buffer, path, length + 1);
    for (index = 1; index <= length; index++) {
        if (buffer[index] == '/' || buffer[index] == '\\' || buffer[index] == '\0') {
            char saved = buffer[index];
            if (index == 2 && buffer[1] == ':') {
                continue;
            }
            buffer[index] = '\0';
            if (buffer[0] != '\0' && !kain_native_fs_path_is_absolute(buffer)) {
                if (kain_native_fs_create_one_dir(buffer) != 0) {
                    buffer[index] = saved;
                    return kain_native_fs_fail("create_dir_all", buffer);
                }
            } else if (strlen(buffer) > 3) {
                if (kain_native_fs_create_one_dir(buffer) != 0) {
                    buffer[index] = saved;
                    return kain_native_fs_fail("create_dir_all", buffer);
                }
            }
            buffer[index] = saved;
        }
    }
    return kain_native_fs_ok();
}

int64_t kain_native_fs_copy_file(const char* src, const char* dest) {
    FILE* input = 0;
    FILE* output = 0;
    char buffer[65536];
    size_t read_count;
    if (src == 0 || dest == 0) {
        errno = EINVAL;
        return kain_native_fs_fail("copy_file", src ? src : dest);
    }
#ifdef _WIN32
    if (fopen_s(&input, src, "rb") != 0) input = 0;
#else
    input = fopen(src, "rb");
#endif
    if (input == 0) {
        return kain_native_fs_fail("copy_file", src);
    }
    if (kain_native_fs_create_parent_dirs(dest) != 0) {
        fclose(input);
        return kain_native_fs_fail("copy_file", dest);
    }
#ifdef _WIN32
    if (fopen_s(&output, dest, "wb") != 0) output = 0;
#else
    output = fopen(dest, "wb");
#endif
    if (output == 0) {
        fclose(input);
        return kain_native_fs_fail("copy_file", dest);
    }
    while ((read_count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, read_count, output) != read_count) {
            fclose(input);
            fclose(output);
            return kain_native_fs_fail("copy_file", dest);
        }
    }
    fclose(input);
    fclose(output);
    return kain_native_fs_ok();
}

int64_t kain_native_fs_move_path(const char* src, const char* dest) {
    if (src == 0 || dest == 0) {
        errno = EINVAL;
        return kain_native_fs_fail("move_path", src ? src : dest);
    }
    if (kain_native_fs_create_parent_dirs(dest) != 0) {
        return kain_native_fs_fail("move_path", dest);
    }
    if (rename(src, dest) != 0) {
        return kain_native_fs_fail("move_path", src);
    }
    return kain_native_fs_ok();
}

int64_t kain_native_fs_remove_file(const char* path) {
    if (path == 0 || remove(path) != 0) {
        return kain_native_fs_fail("remove_file", path);
    }
    return kain_native_fs_ok();
}

#ifdef _WIN32
static int64_t kain_native_fs_remove_dir_all_inner(const char* path) {
    char pattern[4096];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (snprintf(pattern, sizeof(pattern), "%s\\*", path) < 0) {
        errno = EINVAL;
        return -1;
    }
    handle = FindFirstFileA(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            char child[4096];
            if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                continue;
            }
            if (snprintf(child, sizeof(child), "%s\\%s", path, data.cFileName) < 0) {
                FindClose(handle);
                errno = EINVAL;
                return -1;
            }
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (kain_native_fs_remove_dir_all_inner(child) != 0) {
                    FindClose(handle);
                    return -1;
                }
            } else if (DeleteFileA(child) == 0) {
                FindClose(handle);
                errno = EACCES;
                return -1;
            }
        } while (FindNextFileA(handle, &data) != 0);
        FindClose(handle);
    }
    if (RemoveDirectoryA(path) == 0) {
        errno = EACCES;
        return -1;
    }
    return 0;
}
#else
static int64_t kain_native_fs_remove_dir_all_inner(const char* path) {
    DIR* dir = opendir(path);
    struct dirent* entry;
    if (dir == 0) {
        return -1;
    }
    while ((entry = readdir(dir)) != 0) {
        char child[4096];
        struct stat info;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0) {
            closedir(dir);
            errno = EINVAL;
            return -1;
        }
        if (stat(child, &info) != 0) {
            closedir(dir);
            return -1;
        }
        if (S_ISDIR(info.st_mode)) {
            if (kain_native_fs_remove_dir_all_inner(child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (unlink(child) != 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
    return rmdir(path);
}
#endif

int64_t kain_native_fs_remove_dir_all(const char* path) {
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return kain_native_fs_fail("remove_dir_all", path);
    }
    if (kain_native_fs_remove_dir_all_inner(path) != 0) {
        return kain_native_fs_fail("remove_dir_all", path);
    }
    return kain_native_fs_ok();
}

static void kain_native_fs_temp_base(char* buffer, size_t buffer_size) {
    const char* temp = getenv("TMPDIR");
    if (temp == 0 || temp[0] == '\0') temp = getenv("TEMP");
    if (temp == 0 || temp[0] == '\0') temp = getenv("TMP");
    if (temp == 0 || temp[0] == '\0') temp = ".";
    kain_native_fs_copy_message(buffer, buffer_size, temp);
}

static void kain_native_fs_temp_path(char* buffer, size_t buffer_size, const char* prefix, int attempt) {
    char base[1024];
    if (prefix == 0 || prefix[0] == '\0') {
        prefix = "kain";
    }
    kain_native_fs_temp_base(base, sizeof(base));
#ifdef _WIN32
    _snprintf_s(buffer, buffer_size, _TRUNCATE, "%s\\%s-%lu-%lld-%d", base, prefix, (unsigned long)GetCurrentProcessId(), (long long)time(NULL), attempt);
#else
    snprintf(buffer, buffer_size, "%s/%s-%lu-%lld-%d", base, prefix, (unsigned long)getpid(), (long long)time(NULL), attempt);
#endif
}

const char* kain_native_fs_temp_file(const char* prefix) {
    int attempt;
    for (attempt = 0; attempt < 128; attempt++) {
        char path[4096];
        FILE* file = 0;
        kain_native_fs_temp_path(path, sizeof(path), prefix, attempt);
#ifdef _WIN32
        if (fopen_s(&file, path, "wx") == 0 && file != 0) {
#else
        file = fopen(path, "wx");
        if (file != 0) {
#endif
            fclose(file);
            kain_native_fs_ok();
            return string_new(path);
        }
    }
    errno = EEXIST;
    kain_native_fs_fail("temp_file", prefix);
    return string_new("");
}

const char* kain_native_fs_temp_dir(const char* prefix) {
    int attempt;
    for (attempt = 0; attempt < 128; attempt++) {
        char path[4096];
        kain_native_fs_temp_path(path, sizeof(path), prefix, attempt);
        if (kain_native_fs_create_one_dir(path) == 0 && kain_native_fs_is_dir(path)) {
            kain_native_fs_ok();
            return string_new(path);
        }
    }
    errno = EEXIST;
    kain_native_fs_fail("temp_dir", prefix);
    return string_new("");
}

int64_t kain_native_fs_atomic_write_text(const char* path, const char* content) {
    char temp_path[4096];
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return kain_native_fs_fail("atomic_write_text", path);
    }
#ifdef _WIN32
    _snprintf_s(temp_path, sizeof(temp_path), _TRUNCATE, "%s.%lld.tmp", path, (long long)time(NULL));
#else
    snprintf(temp_path, sizeof(temp_path), "%s.%lld.tmp", path, (long long)time(NULL));
#endif
    if (kain_native_fs_write_text(temp_path, content) != 0) {
        return g_kain_native_fs_last_status;
    }
#ifdef _WIN32
    DeleteFileA(path);
#endif
    if (rename(temp_path, path) != 0) {
        remove(temp_path);
        return kain_native_fs_fail("atomic_write_text", path);
    }
    return kain_native_fs_ok();
}

typedef struct KainNativeSha256 {
    uint32_t state[8];
    uint64_t bit_len;
    unsigned char data[64];
    size_t data_len;
} KainNativeSha256;

static const uint32_t KAIN_NATIVE_SHA256_K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint32_t kain_native_sha256_rotr(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

static void kain_native_sha256_transform(KainNativeSha256* ctx, const unsigned char block[64]) {
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    size_t index;

    for (index = 0; index < 16; index++) {
        size_t base = index * 4;
        words[index] =
            ((uint32_t)block[base] << 24) |
            ((uint32_t)block[base + 1] << 16) |
            ((uint32_t)block[base + 2] << 8) |
            ((uint32_t)block[base + 3]);
    }
    for (index = 16; index < 64; index++) {
        uint32_t s0 =
            kain_native_sha256_rotr(words[index - 15], 7) ^
            kain_native_sha256_rotr(words[index - 15], 18) ^
            (words[index - 15] >> 3);
        uint32_t s1 =
            kain_native_sha256_rotr(words[index - 2], 17) ^
            kain_native_sha256_rotr(words[index - 2], 19) ^
            (words[index - 2] >> 10);
        words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (index = 0; index < 64; index++) {
        uint32_t s1 = kain_native_sha256_rotr(e, 6) ^
            kain_native_sha256_rotr(e, 11) ^
            kain_native_sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + KAIN_NATIVE_SHA256_K[index] + words[index];
        uint32_t s0 = kain_native_sha256_rotr(a, 2) ^
            kain_native_sha256_rotr(a, 13) ^
            kain_native_sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void kain_native_sha256_init(KainNativeSha256* ctx) {
    ctx->data_len = 0;
    ctx->bit_len = 0;
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void kain_native_sha256_update(KainNativeSha256* ctx, const unsigned char* data, size_t length) {
    size_t index;
    for (index = 0; index < length; index++) {
        ctx->data[ctx->data_len] = data[index];
        ctx->data_len++;
        if (ctx->data_len == 64) {
            kain_native_sha256_transform(ctx, ctx->data);
            ctx->bit_len += 512;
            ctx->data_len = 0;
        }
    }
}

static void kain_native_sha256_final(KainNativeSha256* ctx, unsigned char digest[32]) {
    size_t index = ctx->data_len;
    size_t state_index;

    ctx->data[index++] = 0x80;
    if (index > 56) {
        while (index < 64) {
            ctx->data[index++] = 0;
        }
        kain_native_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    } else {
        while (index < 56) {
            ctx->data[index++] = 0;
        }
    }

    ctx->bit_len += (uint64_t)ctx->data_len * 8U;
    ctx->data[56] = (unsigned char)(ctx->bit_len >> 56);
    ctx->data[57] = (unsigned char)(ctx->bit_len >> 48);
    ctx->data[58] = (unsigned char)(ctx->bit_len >> 40);
    ctx->data[59] = (unsigned char)(ctx->bit_len >> 32);
    ctx->data[60] = (unsigned char)(ctx->bit_len >> 24);
    ctx->data[61] = (unsigned char)(ctx->bit_len >> 16);
    ctx->data[62] = (unsigned char)(ctx->bit_len >> 8);
    ctx->data[63] = (unsigned char)(ctx->bit_len);
    kain_native_sha256_transform(ctx, ctx->data);

    for (state_index = 0; state_index < 8; state_index++) {
        digest[state_index * 4] = (unsigned char)(ctx->state[state_index] >> 24);
        digest[state_index * 4 + 1] = (unsigned char)(ctx->state[state_index] >> 16);
        digest[state_index * 4 + 2] = (unsigned char)(ctx->state[state_index] >> 8);
        digest[state_index * 4 + 3] = (unsigned char)(ctx->state[state_index]);
    }
}

const char* kain_native_fs_hash_file(const char* path) {
    FILE* file = 0;
    KainNativeSha256 sha;
    unsigned char digest[32];
    unsigned char buffer[65536];
    size_t read_count;
    char output[65];
    static const char hex[] = "0123456789abcdef";
    size_t index;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        kain_native_fs_fail("hash_file", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) file = 0;
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        kain_native_fs_fail("hash_file", path);
        return string_new("");
    }
    kain_native_sha256_init(&sha);
    while ((read_count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        kain_native_sha256_update(&sha, buffer, read_count);
    }
    if (ferror(file) != 0) {
        fclose(file);
        kain_native_fs_fail("hash_file", path);
        return string_new("");
    }
    fclose(file);
    kain_native_sha256_final(&sha, digest);
    for (index = 0; index < 32; index++) {
        output[index * 2] = hex[digest[index] >> 4];
        output[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    output[64] = '\0';
    kain_native_fs_ok();
    return string_new(output);
}

const char* kain_native_fs_path_join(const char* base, const char* child) {
    char joined[4096];
    size_t base_len;
    if (child == 0) child = "";
    if (base == 0 || base[0] == '\0' || kain_native_fs_path_is_absolute(child)) {
        return string_new((char*)child);
    }
    base_len = strlen(base);
    if (base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\')) {
#ifdef _WIN32
        _snprintf_s(joined, sizeof(joined), _TRUNCATE, "%s%s", base, child);
#else
        snprintf(joined, sizeof(joined), "%s%s", base, child);
#endif
    } else {
#ifdef _WIN32
        _snprintf_s(joined, sizeof(joined), _TRUNCATE, "%s\\%s", base, child);
#else
        snprintf(joined, sizeof(joined), "%s/%s", base, child);
#endif
    }
    return string_new(joined);
}

int64_t kain_native_fs_last_status(void) {
    return g_kain_native_fs_last_status;
}

const char* kain_native_fs_last_error_kind(void) {
    return string_new(g_kain_native_fs_last_error_kind);
}

const char* kain_native_fs_last_error_message(void) {
    return string_new(g_kain_native_fs_last_error_message);
}
