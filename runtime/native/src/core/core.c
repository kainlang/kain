#include "../../include/attrition.h"
#include "../../include/base.h"
#include "../../include/diagnostics.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <shellapi.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static RcHeader* get_header(void* ptr) {
    return ((RcHeader*)ptr) - 1;
}

static int kain_rc_header_is_alive(const RcHeader* header) {
    return header != NULL && header->magic == KAIN_RC_MAGIC_ALIVE;
}

static int kain_rc_header_is_freed(const RcHeader* header) {
    return header != NULL && header->magic == KAIN_RC_MAGIC_FREED;
}

static int kain_rc_is_immediate_handle(const void* ptr) {
    return ptr != NULL && ((((uintptr_t)ptr) & 7u) != 0u);
}

typedef enum KainRcTrackedState {
    KAIN_RC_TRACK_NONE = 0,
    KAIN_RC_TRACK_LIVE = 1,
    KAIN_RC_TRACK_FREED = 2
} KainRcTrackedState;

typedef struct {
    const void* payload;
    RcHeader* header;
    long long type_tag;
    size_t payload_size;
    size_t string_length;
    void (*destructor)(void*);
    uint64_t free_epoch;
    unsigned char state;
} KainRcRegistryEntry;

typedef struct {
#ifdef _WIN32
    CRITICAL_SECTION lock;
    INIT_ONCE init_once;
#else
    pthread_mutex_t lock;
    pthread_once_t init_once;
#endif
    KainRcRegistryEntry* entries;
    size_t capacity;
    size_t live_count;
    size_t occupied_count;
    uint64_t next_free_epoch;
} KainRcRegistryState;

typedef struct {
    KainRcTrackedState state;
    RcHeader* header;
    long long type_tag;
    size_t payload_size;
    size_t string_length;
    void (*destructor)(void*);
} KainRcTrackedPointer;

#define KAIN_RC_REGISTRY_INITIAL_CAPACITY 1024u
#define KAIN_RC_REGISTRY_RECENT_FREED_MAX 16384u

static KainRcRegistryState g_kain_rc_registry = {
#ifdef _WIN32
    .init_once = INIT_ONCE_STATIC_INIT
#else
    .init_once = PTHREAD_ONCE_INIT
#endif
};

static size_t kain_rc_registry_hash_payload(const void* payload) {
    uint64_t bits = (uint64_t)(((uintptr_t)payload) >> 3u);
    bits ^= bits >> 33u;
    bits *= UINT64_C(0xff51afd7ed558ccd);
    bits ^= bits >> 33u;
    bits *= UINT64_C(0xc4ceb9fe1a85ec53);
    bits ^= bits >> 33u;
    return (size_t)bits;
}

/*
 * Proof:
 * - runtime/native/src/core/z3/proofs/native-memory-rc-registry-half-load-preserves-empty-slot.yaml
 *
 * The registry rebuilds before occupied slots can reach half the table, so
 * open-addressed probes always hit an empty sentinel before wrapping forever.
 */
static size_t kain_rc_registry_capacity_for_occupied(size_t occupied_slots) {
    size_t capacity = KAIN_RC_REGISTRY_INITIAL_CAPACITY;
    while (occupied_slots >= (capacity / 2u)) {
        if (capacity > (SIZE_MAX / 2u)) {
            return 0u;
        }
        capacity <<= 1u;
    }
    return capacity;
}

#ifdef _WIN32
static BOOL CALLBACK kain_rc_registry_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context) {
    (void)init_once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_kain_rc_registry.lock);
    return TRUE;
}
#else
static void kain_rc_registry_init_once(void) {
    pthread_mutex_init(&g_kain_rc_registry.lock, NULL);
}
#endif

static void kain_rc_registry_ensure_initialized(void) {
#ifdef _WIN32
    InitOnceExecuteOnce(&g_kain_rc_registry.init_once, kain_rc_registry_init_once, NULL, NULL);
#else
    pthread_once(&g_kain_rc_registry.init_once, kain_rc_registry_init_once);
#endif
}

static void kain_rc_registry_lock(void) {
    kain_rc_registry_ensure_initialized();
#ifdef _WIN32
    EnterCriticalSection(&g_kain_rc_registry.lock);
#else
    pthread_mutex_lock(&g_kain_rc_registry.lock);
#endif
}

static void kain_rc_registry_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_kain_rc_registry.lock);
#else
    pthread_mutex_unlock(&g_kain_rc_registry.lock);
#endif
}

static int kain_rc_registry_keep_freed_entry_locked(const KainRcRegistryEntry* entry) {
    uint64_t cutoff_epoch;
    if (entry == NULL || entry->state != KAIN_RC_TRACK_FREED) {
        return 0;
    }
    if (g_kain_rc_registry.next_free_epoch <= KAIN_RC_REGISTRY_RECENT_FREED_MAX) {
        return 1;
    }
    cutoff_epoch = g_kain_rc_registry.next_free_epoch - KAIN_RC_REGISTRY_RECENT_FREED_MAX;
    return entry->free_epoch >= cutoff_epoch;
}

static KainRcRegistryEntry* kain_rc_registry_find_entry_locked(const void* payload) {
    KainRcRegistryEntry* entry;
    size_t index;
    size_t probe;
    size_t mask;
    if (payload == NULL || g_kain_rc_registry.entries == NULL || g_kain_rc_registry.capacity == 0u) {
        return NULL;
    }
    mask = g_kain_rc_registry.capacity - 1u;
    index = kain_rc_registry_hash_payload(payload) & mask;
    for (probe = 0u; probe < g_kain_rc_registry.capacity; ++probe) {
        entry = &g_kain_rc_registry.entries[index];
        if (entry->state == KAIN_RC_TRACK_NONE) {
            return NULL;
        }
        if (entry->payload == payload) {
            return entry;
        }
        index = (index + 1u) & mask;
    }
    return NULL;
}

static KainRcRegistryEntry* kain_rc_registry_find_insert_slot_locked(const void* payload) {
    KainRcRegistryEntry* entry;
    size_t index;
    size_t probe;
    size_t mask;
    if (payload == NULL || g_kain_rc_registry.entries == NULL || g_kain_rc_registry.capacity == 0u) {
        return NULL;
    }
    mask = g_kain_rc_registry.capacity - 1u;
    index = kain_rc_registry_hash_payload(payload) & mask;
    for (probe = 0u; probe < g_kain_rc_registry.capacity; ++probe) {
        entry = &g_kain_rc_registry.entries[index];
        if (entry->state == KAIN_RC_TRACK_NONE || entry->payload == payload) {
            return entry;
        }
        index = (index + 1u) & mask;
    }
    return NULL;
}

static void kain_rc_registry_insert_rehashed_entry(
    KainRcRegistryEntry* entries,
    size_t capacity,
    const KainRcRegistryEntry* source
) {
    KainRcRegistryEntry* entry;
    size_t index;
    size_t mask = capacity - 1u;
    index = kain_rc_registry_hash_payload(source->payload) & mask;
    while (1) {
        entry = &entries[index];
        if (entry->state == KAIN_RC_TRACK_NONE) {
            *entry = *source;
            return;
        }
        index = (index + 1u) & mask;
    }
}

static int kain_rc_registry_rebuild_locked(size_t incoming_slots) {
    KainRcRegistryEntry* old_entries = g_kain_rc_registry.entries;
    size_t old_capacity = g_kain_rc_registry.capacity;
    KainRcRegistryEntry* new_entries;
    size_t retained_freed = 0u;
    size_t target_capacity;
    size_t target_occupied;
    size_t index;

    if (old_entries != NULL) {
        for (index = 0u; index < old_capacity; ++index) {
            if (kain_rc_registry_keep_freed_entry_locked(&old_entries[index])) {
                retained_freed += 1u;
            }
        }
    }

    if (SIZE_MAX - incoming_slots < g_kain_rc_registry.live_count ||
        SIZE_MAX - retained_freed < g_kain_rc_registry.live_count + incoming_slots) {
        return 0;
    }
    target_occupied = g_kain_rc_registry.live_count + retained_freed + incoming_slots;
    target_capacity = kain_rc_registry_capacity_for_occupied(target_occupied);
    if (target_capacity == 0u) {
        return 0;
    }

    new_entries = (KainRcRegistryEntry*)calloc(target_capacity, sizeof(KainRcRegistryEntry));
    if (new_entries == NULL) {
        return 0;
    }

    if (old_entries != NULL) {
        for (index = 0u; index < old_capacity; ++index) {
            KainRcRegistryEntry* source = &old_entries[index];
            if (source->state == KAIN_RC_TRACK_LIVE || kain_rc_registry_keep_freed_entry_locked(source)) {
                kain_rc_registry_insert_rehashed_entry(new_entries, target_capacity, source);
            }
        }
        free(old_entries);
    }

    g_kain_rc_registry.entries = new_entries;
    g_kain_rc_registry.capacity = target_capacity;
    g_kain_rc_registry.occupied_count = g_kain_rc_registry.live_count + retained_freed;
    return 1;
}

static int kain_rc_registry_register_live(const void* payload, RcHeader* header) {
    KainRcRegistryEntry* entry;
    kain_rc_registry_lock();
    entry = kain_rc_registry_find_entry_locked(payload);
    if (entry == NULL &&
        (g_kain_rc_registry.entries == NULL ||
         g_kain_rc_registry.capacity == 0u ||
         (g_kain_rc_registry.occupied_count + 1u) >= (g_kain_rc_registry.capacity / 2u))) {
        if (!kain_rc_registry_rebuild_locked(1u)) {
            kain_rc_registry_unlock();
            return 0;
        }
        entry = NULL;
    }
    if (entry == NULL) {
        entry = kain_rc_registry_find_insert_slot_locked(payload);
    }
    if (entry == NULL) {
        kain_rc_registry_unlock();
        return 0;
    }
    if (entry->state == KAIN_RC_TRACK_NONE) {
        g_kain_rc_registry.occupied_count += 1u;
    } else if (entry->state != KAIN_RC_TRACK_LIVE) {
        entry->free_epoch = 0u;
    }
    if (entry->state != KAIN_RC_TRACK_LIVE) {
        g_kain_rc_registry.live_count += 1u;
    }
    entry->payload = payload;
    entry->header = header;
    entry->type_tag = header->type_tag;
    entry->payload_size = header->payload_size;
    entry->string_length = header->string_length;
    entry->destructor = header->destructor;
    entry->free_epoch = 0u;
    entry->state = KAIN_RC_TRACK_LIVE;
    kain_rc_registry_unlock();
    return 1;
}

static void kain_rc_registry_mark_freed(const void* payload, RcHeader* header, int keep_header_live) {
    KainRcRegistryEntry* entry;
    kain_rc_registry_lock();
    entry = kain_rc_registry_find_entry_locked(payload);
    if (entry == NULL &&
        (g_kain_rc_registry.entries == NULL ||
         g_kain_rc_registry.capacity == 0u ||
         (g_kain_rc_registry.occupied_count + 1u) >= (g_kain_rc_registry.capacity / 2u))) {
        if (!kain_rc_registry_rebuild_locked(1u)) {
            kain_rc_registry_unlock();
            return;
        }
        entry = NULL;
    }
    if (entry == NULL) {
        entry = kain_rc_registry_find_insert_slot_locked(payload);
    }
    if (entry != NULL) {
        if (entry->state == KAIN_RC_TRACK_NONE) {
            g_kain_rc_registry.occupied_count += 1u;
        }
        if (entry->state == KAIN_RC_TRACK_LIVE && g_kain_rc_registry.live_count > 0u) {
            g_kain_rc_registry.live_count -= 1u;
        }
        entry->payload = payload;
        entry->header = keep_header_live ? header : NULL;
        entry->type_tag = header->type_tag;
        entry->payload_size = header->payload_size;
        entry->string_length = header->string_length;
        entry->destructor = header->destructor;
        entry->free_epoch = ++g_kain_rc_registry.next_free_epoch;
        entry->state = KAIN_RC_TRACK_FREED;
    }
    kain_rc_registry_unlock();
}

static void kain_rc_registry_drop_freed_header(const void* payload) {
    KainRcRegistryEntry* entry;
    kain_rc_registry_lock();
    entry = kain_rc_registry_find_entry_locked(payload);
    if (entry != NULL && entry->state == KAIN_RC_TRACK_FREED) {
        entry->header = NULL;
    }
    kain_rc_registry_unlock();
}

static KainRcTrackedPointer kain_rc_registry_lookup(const void* payload) {
    KainRcTrackedPointer tracked = {0};
    KainRcRegistryEntry* entry;
    if (payload == NULL || kain_rc_is_immediate_handle(payload)) {
        return tracked;
    }
    kain_rc_registry_lock();
    entry = kain_rc_registry_find_entry_locked(payload);
    if (entry != NULL) {
        tracked.state = (KainRcTrackedState)entry->state;
        tracked.header = entry->header;
        tracked.type_tag = entry->type_tag;
        tracked.payload_size = entry->payload_size;
        tracked.string_length = entry->string_length;
        tracked.destructor = entry->destructor;
    }
    kain_rc_registry_unlock();
    return tracked;
}

int kain_rc_is_tracked_pointer(const void* ptr) {
    return kain_rc_registry_lookup(ptr).state != KAIN_RC_TRACK_NONE;
}

static size_t kain_string_len_rc(const char* value) {
    if (!value) {
        return 0u;
    }
    return get_header((void*)value)->string_length;
}

static const char* kain_find_substring_bytes(
    const char* haystack,
    size_t haystack_len,
    const char* needle,
    size_t needle_len,
    size_t start
) {
    const char* cursor;
    size_t remaining;
    unsigned char first;
    if (!haystack || !needle || start > haystack_len) {
        return NULL;
    }
    if (needle_len == 0u) {
        return haystack + start;
    }
    remaining = haystack_len - start;
    if (needle_len > remaining) {
        return NULL;
    }
    cursor = haystack + start;
    first = (unsigned char)needle[0];
    if (needle_len == 1u) {
        return (const char*)memchr(cursor, (int)first, remaining);
    }
    while (remaining >= needle_len) {
        const char* found = (const char*)memchr(cursor, (int)first, remaining - needle_len + 1u);
        if (!found) {
            return NULL;
        }
        if (memcmp(found + 1, needle + 1, needle_len - 1u) == 0) {
            return found;
        }
        cursor = found + 1;
        remaining = haystack_len - (size_t)(cursor - haystack);
    }
    return NULL;
}

void* kain_alloc_rc(size_t size, long long type_tag);

/* Global diagnostic for last error (thread-unsafe, but matches current runtime model) */
static KainDiagnostic g_last_diagnostic;
static int g_last_diagnostic_valid = 0;

static void emit_diagnostic(
    KainDiagSubsystem subsystem,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail
) {
    kain_diagnostic_create(&g_last_diagnostic, subsystem, severity, code, message, detail, NULL);
    g_last_diagnostic_valid = 1;

    /* Print fatal and error diagnostics immediately */
    if (severity >= KAIN_DIAG_SEVERITY_ERROR) {
        kain_diagnostic_print(&g_last_diagnostic);
    }
}

static char* kain_string_new_with_len(const char* src, size_t len) {
    char* buf = (char*)kain_alloc_rc(len + 1, 1);
    if (!buf) return NULL;
    if (src && len > 0) {
        memcpy(buf, src, len);
    }
    buf[len] = '\0';
    kain_rc_set_string_length(buf, len);
    return buf;
}

static int kain_char_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

void array_free_elems(void* ptr);
void map_free_elems(void* ptr);

double kain_clampd(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

long long kain_floor_i64(double value) {
    return (long long)floor(value);
}

long long kain_ceil_i64(double value) {
    return (long long)ceil(value);
}

long long kain_round_i64(double value) {
    return (long long)round(value);
}

long long kain_ord(char* src) {
    size_t len;
    if (!src) {
        return -1;
    }
    len = kain_string_len_rc(src);
    if (len == 0u) {
        return -1;
    }
    return (long long)((unsigned char)src[0]);
}

char* kain_chr(long long code) {
    unsigned char byte;
    if (code < 0 || code > 255) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid chr code",
            "chr expects a byte-sized integer in the range 0..255"
        );
        return string_new("");
    }
    byte = (unsigned char)code;
    return kain_string_new_with_len((const char*)&byte, 1u);
}

long long kain_parse_i64_string(char* src) {
    size_t len;
    char* scratch;
    char* end = NULL;
    long long value;
    if (!src) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_int input",
            "to_int expects a non-null string"
        );
        return 0;
    }
    len = kain_string_len_rc(src);
    if (len == 0u) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_int input",
            "to_int cannot parse an empty string"
        );
        return 0;
    }
    scratch = (char*)malloc(len + 1u);
    if (!scratch) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Memory allocation failed",
            "Failed to allocate a parse scratch buffer for to_int"
        );
        return 0;
    }
    memcpy(scratch, src, len);
    scratch[len] = '\0';
    if (kain_char_is_space(scratch[0]) || kain_char_is_space(scratch[len - 1u])) {
        free(scratch);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_int input",
            "to_int rejects surrounding whitespace"
        );
        return 0;
    }
    errno = 0;
    value = strtoll(scratch, &end, 10);
    if (errno == ERANGE || end == scratch || (size_t)(end - scratch) != len) {
        free(scratch);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_int input",
            "to_int could not parse the provided string as a base-10 integer"
        );
        return 0;
    }
    free(scratch);
    return value;
}

double kain_parse_f64_string(char* src) {
    size_t len;
    char* scratch;
    char* end = NULL;
    double value;
    if (!src) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_float input",
            "to_float expects a non-null string"
        );
        return 0.0;
    }
    len = kain_string_len_rc(src);
    if (len == 0u) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_float input",
            "to_float cannot parse an empty string"
        );
        return 0.0;
    }
    scratch = (char*)malloc(len + 1u);
    if (!scratch) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Memory allocation failed",
            "Failed to allocate a parse scratch buffer for to_float"
        );
        return 0.0;
    }
    memcpy(scratch, src, len);
    scratch[len] = '\0';
    if (kain_char_is_space(scratch[0]) || kain_char_is_space(scratch[len - 1u])) {
        free(scratch);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_float input",
            "to_float rejects surrounding whitespace"
        );
        return 0.0;
    }
    errno = 0;
    value = strtod(scratch, &end);
    if (errno == ERANGE || end == scratch || (size_t)(end - scratch) != len) {
        free(scratch);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Invalid to_float input",
            "to_float could not parse the provided string as a base-10 float"
        );
        return 0.0;
    }
    free(scratch);
    return value;
}

void* kain_alloc_rc(size_t size, long long type_tag) {
    size_t total_size;
    RcHeader* header;
    void* payload;
    char detail[192];
    if (size > SIZE_MAX - sizeof(RcHeader)) {
        snprintf(
            detail,
            sizeof(detail),
            "RC allocation size overflow: payload=%zu header=%zu type_tag=%lld",
            size,
            sizeof(RcHeader),
            type_tag
        );
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Memory allocation failed",
            detail
        );
        return NULL;
    }
    total_size = sizeof(RcHeader) + size;
    header = (RcHeader*)kain_attrition_heap_alloc(total_size);
    if (!header) {
        snprintf(
            detail,
            sizeof(detail),
            "Failed to allocate RC block: payload=%zu total=%zu type_tag=%lld",
            size,
            total_size,
            type_tag
        );
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Memory allocation failed",
            detail
        );
        return NULL;
    }
    header->magic = KAIN_RC_MAGIC_ALIVE;
    header->ref_count = 1;
    header->weak_count = 0;
    header->type_tag = type_tag;
    header->payload_size = size;
    header->string_length = (type_tag == 1 && size > 0u) ? (size - 1u) : 0u;
    header->destructor = NULL;
    payload = (void*)(header + 1);
    if (!kain_rc_registry_register_live(payload, header)) {
        if (!kain_attrition_heap_release(header, total_size)) {
            free(header);
        }
        snprintf(
            detail,
            sizeof(detail),
            "Failed to register RC provenance: payload=%zu total=%zu type_tag=%lld",
            size,
            total_size,
            type_tag
        );
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Memory allocation failed",
            detail
        );
        return NULL;
    }
    kain_attrition_note_rc_alloc(total_size);
    return payload;
}

void* kain_alloc(size_t size) {
    return kain_alloc_rc(size, 0);
}

void* KAIN_alloc(long long size) {
    return kain_alloc((size_t)size);
}

void rc_retain(void* ptr) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    char detail[256];
    if (!ptr || kain_rc_is_immediate_handle(ptr)) return;
    tracked = kain_rc_registry_lookup(ptr);
    if (tracked.state == KAIN_RC_TRACK_NONE) {
        return;
    }
    if (tracked.state == KAIN_RC_TRACK_FREED) {
        kain_attrition_note_rc_underflow();
        snprintf(
            detail,
            sizeof(detail),
            "payload=%p type_tag=0x%llx payload_size=%zu string_length=%zu destructor=%p",
            ptr,
            (unsigned long long)tracked.type_tag,
            tracked.payload_size,
            tracked.string_length,
            (void*)tracked.destructor
        );
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "RC retain after free",
            detail
        );
        return;
    }
    header = tracked.header;
    if (!kain_rc_header_is_alive(header)) {
        return;
    }
    if (header->ref_count == LLONG_MAX) {
        kain_attrition_note_rc_overflow();
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "RC retain overflow",
            "Retaining this object would overflow the signed ref_count."
        );
        return;
    }
    header->ref_count++;
    kain_attrition_note_rc_retain();
}

void rc_weak_retain(void* ptr) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    if (!ptr || kain_rc_is_immediate_handle(ptr)) return;
    tracked = kain_rc_registry_lookup(ptr);
    if (tracked.state != KAIN_RC_TRACK_LIVE || tracked.header == NULL) {
        return;
    }
    header = tracked.header;
    if (!kain_rc_header_is_alive(header)) {
        return;
    }
    header->weak_count++;
}

void rc_release(void* ptr) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    size_t total_size;
    char detail[256];
    if (!ptr || kain_rc_is_immediate_handle(ptr)) return;
    tracked = kain_rc_registry_lookup(ptr);
    if (tracked.state == KAIN_RC_TRACK_NONE) {
        return;
    }
    if (tracked.state == KAIN_RC_TRACK_FREED) {
        kain_attrition_note_rc_underflow();
        snprintf(
            detail,
            sizeof(detail),
            "payload=%p type_tag=0x%llx payload_size=%zu string_length=%zu destructor=%p",
            ptr,
            (unsigned long long)tracked.type_tag,
            tracked.payload_size,
            tracked.string_length,
            (void*)tracked.destructor
        );
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "RC release after free",
            detail
        );
        return;
    }
    header = tracked.header;
    if (!kain_rc_header_is_alive(header)) {
        return;
    }
    if (header->ref_count <= 0) {
        kain_attrition_note_rc_underflow();
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "RC release underflow",
            "Releasing this object would underflow the signed ref_count."
        );
        return;
    }
    header->ref_count--;
    kain_attrition_note_rc_release();

    if (header->ref_count == 0) {
        if (header->type_tag == 2) {
            array_free_elems(ptr);
        } else if (header->type_tag == 3) {
            map_free_elems(ptr);
        }

        if (header->destructor) {
            header->destructor(ptr);
        }

        header->magic = KAIN_RC_MAGIC_FREED;
        if (header->weak_count == 0) {
            total_size = sizeof(RcHeader) + header->payload_size;
            kain_rc_registry_mark_freed(ptr, header, 0);
            kain_attrition_note_rc_free(total_size);
            if (!kain_attrition_heap_release(header, total_size)) {
                free(header);
            }
        } else {
            kain_rc_registry_mark_freed(ptr, header, 1);
        }
    }
}

void rc_weak_release(void* ptr) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    if (!ptr || kain_rc_is_immediate_handle(ptr)) return;
    tracked = kain_rc_registry_lookup(ptr);
    if (tracked.state == KAIN_RC_TRACK_NONE || tracked.header == NULL) {
        return;
    }
    header = tracked.header;
    if (!kain_rc_header_is_alive(header) && !kain_rc_header_is_freed(header)) {
        return;
    }
    if (header->weak_count <= 0) {
        return;
    }
    header->weak_count--;

    if (header->weak_count == 0 && header->ref_count == 0) {
        kain_rc_registry_drop_freed_header(ptr);
        free(header);
    }
}

void* weak_upgrade(void* ptr) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    if (!ptr) return NULL;
    if (kain_rc_is_immediate_handle(ptr)) return ptr;
    tracked = kain_rc_registry_lookup(ptr);
    if (tracked.state != KAIN_RC_TRACK_LIVE || tracked.header == NULL) {
        return NULL;
    }
    header = tracked.header;
    if (!kain_rc_header_is_alive(header)) {
        return NULL;
    }
    if (header->ref_count > 0) {
        if (header->ref_count == LLONG_MAX) {
            kain_attrition_note_rc_overflow();
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_MEMORY,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
                "Weak upgrade overflow",
                "Upgrading this weak pointer would overflow the signed ref_count."
            );
            return NULL;
        }
        header->ref_count++;
        kain_attrition_note_rc_retain();
        return ptr;
    }
    return NULL;
}

void kain_set_destructor(void* ptr, void (*dtor)(void*)) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    if (!ptr || kain_rc_is_immediate_handle(ptr)) return;
    tracked = kain_rc_registry_lookup(ptr);
    if (tracked.state != KAIN_RC_TRACK_LIVE || tracked.header == NULL) {
        return;
    }
    header = tracked.header;
    if (!kain_rc_header_is_alive(header)) {
        return;
    }
    header->destructor = dtor;
}

void KAIN_set_destructor(void* ptr, void (*dtor)(void*)) {
    kain_set_destructor(ptr, dtor);
}

#ifdef _WIN32
static DWORD WINAPI thread_wrapper(LPVOID lp_param) {
    ThreadArgs* args = (ThreadArgs*)lp_param;
    args->func(args->arg);
    rc_release(args->arg);
    free(args);
    return 0;
}
#else
static void* thread_wrapper(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    args->func(args->arg);
    rc_release(args->arg);
    free(args);
    return NULL;
}
#endif

void kain_spawn(void (*func)(void*), void* arg) {
    ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    if (!args) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_ACTOR,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
            "Thread spawn failed",
            "Failed to allocate thread arguments structure"
        );
        return;
    }
    args->func = func;
    args->arg = arg;
    rc_retain(arg);

#ifdef _WIN32
    {
        HANDLE thread_handle = CreateThread(NULL, 0, thread_wrapper, args, 0, NULL);
        if (!thread_handle) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_ACTOR,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
                "Thread spawn failed",
                "CreateThread returned NULL"
            );
            rc_release(arg);
            free(args);
        }
    }
#else
    {
        pthread_t thread;
        int result = pthread_create(&thread, NULL, thread_wrapper, args);
        if (result != 0) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_ACTOR,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
                "Thread spawn failed",
                "pthread_create returned non-zero"
            );
            rc_release(arg);
            free(args);
            return;
        }
        pthread_detach(thread);
    }
#endif
}

void KAIN_spawn(void* func, void* arg) {
    kain_spawn((void (*)(void*))func, arg);
}

// DEPRECATED: This function was used as a fallback wrapper for actor spawning.
// As of the actor bootstrap ABI implementation (Requirement 5.1), LLVM codegen
// now emits actor-specific entrypoints (e.g., ActorName_run) and spawns them
// directly. This function remains for backward compatibility but should not be
// used in new code.
void default_actor_run(void* arg) {
    (void)arg;
    printf("Actor running (default wrapper)\n");
}

void kain_sleep(double seconds) {
    if (seconds <= 0.0) {
        return;
    }
    kain_attrition_sleep_for_millis((unsigned long long)(seconds * 1000.0));
}

void KAIN_sleep(double seconds) {
    kain_sleep(seconds);
}

char* string_new(char* src) {
    char* buf;
    size_t len;
    if (!src) return NULL;
    len = strlen(src);
    buf = (char*)kain_alloc_rc(len + 1, 1);
    if (!buf) return NULL;
    memcpy(buf, src, len + 1);
    return buf;
}

void print_str(char* str, long long len) {
    (void)len;
    if (str) {
        printf("%s\n", str);
    } else {
        printf("(null)\n");
    }
}

void print_i64(long long n) {
    printf("%lld\n", n);
}

void print_f64(double n) {
    printf("%f\n", n);
}

void print_bool(int n) {
    printf("%s\n", n ? "true" : "false");
}

static size_t kain_u64_decimal_digit_count(unsigned long long value) {
    if (value < 10ULL) return 1u;
    if (value < 100ULL) return 2u;
    if (value < 1000ULL) return 3u;
    if (value < 10000ULL) return 4u;
    if (value < 100000ULL) return 5u;
    if (value < 1000000ULL) return 6u;
    if (value < 10000000ULL) return 7u;
    if (value < 100000000ULL) return 8u;
    if (value < 1000000000ULL) return 9u;
    if (value < 10000000000ULL) return 10u;
    if (value < 100000000000ULL) return 11u;
    if (value < 1000000000000ULL) return 12u;
    if (value < 10000000000000ULL) return 13u;
    if (value < 100000000000000ULL) return 14u;
    if (value < 1000000000000000ULL) return 15u;
    if (value < 10000000000000000ULL) return 16u;
    if (value < 100000000000000000ULL) return 17u;
    if (value < 1000000000000000000ULL) return 18u;
    if (value < 10000000000000000000ULL) return 19u;
    return 20u;
}

static int kain_size_add_checked(size_t left, size_t right, size_t* out) {
    if (left > SIZE_MAX - right) {
        return 0;
    }
    *out = left + right;
    return 1;
}

static char* kain_string_concat_parts(const char* const* parts, size_t count) {
    size_t lengths[10];
    size_t total_length = 0u;
    size_t alloc_size = 0u;
    size_t index;
    char* out;
    char* cursor;
    for (index = 0u; index < count; ++index) {
        size_t part_length = parts[index] ? kain_string_len_rc(parts[index]) : 0u;
        lengths[index] = part_length;
        if (!kain_size_add_checked(total_length, part_length, &total_length)) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_MEMORY,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "String concat overflow",
                "Concatenated string length overflowed size_t"
            );
            return NULL;
        }
    }
    if (!kain_size_add_checked(total_length, 1u, &alloc_size)) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "String concat overflow",
            "Concatenated string terminator overflowed size_t"
        );
        return NULL;
    }
    out = (char*)kain_alloc_rc(alloc_size, 1);
    if (!out) return NULL;
    cursor = out;
    for (index = 0u; index < count; ++index) {
        if (lengths[index] > 0u) {
            memcpy(cursor, parts[index], lengths[index]);
            cursor += lengths[index];
        }
    }
    *cursor = '\0';
    return out;
}

char* to_string(long long n) {
    unsigned long long magnitude = n < 0 ? (0ULL - (unsigned long long)n) : (unsigned long long)n;
    size_t digit_count = kain_u64_decimal_digit_count(magnitude);
    size_t text_length = digit_count + (n < 0 ? 1u : 0u);
    char* buf = (char*)kain_alloc_rc(text_length + 1u, 1);
    char* cursor;
    if (!buf) return NULL;
    cursor = buf + text_length;
    *cursor = '\0';
    do {
        unsigned long long next = magnitude / 10ULL;
        unsigned long long digit = magnitude - (next * 10ULL);
        *--cursor = (char)('0' + (char)digit);
        magnitude = next;
    } while (magnitude != 0ULL);
    if (n < 0) {
        *--cursor = '-';
    }
    return buf;
}

char* str_concat(char* s1, char* s2) {
    if (!s1 && !s2) return NULL;
    {
        const char* parts[2] = {s1, s2};
        return kain_string_concat_parts(parts, 2u);
    }
}

char* str_concat3(char* s1, char* s2, char* s3) {
    const char* parts[3] = {s1, s2, s3};
    return kain_string_concat_parts(parts, 3u);
}

char* str_concat4(char* s1, char* s2, char* s3, char* s4) {
    const char* parts[4] = {s1, s2, s3, s4};
    return kain_string_concat_parts(parts, 4u);
}

char* str_concat5(char* s1, char* s2, char* s3, char* s4, char* s5) {
    const char* parts[5] = {s1, s2, s3, s4, s5};
    return kain_string_concat_parts(parts, 5u);
}

char* str_concat6(char* s1, char* s2, char* s3, char* s4, char* s5, char* s6) {
    const char* parts[6] = {s1, s2, s3, s4, s5, s6};
    return kain_string_concat_parts(parts, 6u);
}

char* str_concat7(char* s1, char* s2, char* s3, char* s4, char* s5, char* s6, char* s7) {
    const char* parts[7] = {s1, s2, s3, s4, s5, s6, s7};
    return kain_string_concat_parts(parts, 7u);
}

char* str_concat8(char* s1, char* s2, char* s3, char* s4, char* s5, char* s6, char* s7, char* s8) {
    const char* parts[8] = {s1, s2, s3, s4, s5, s6, s7, s8};
    return kain_string_concat_parts(parts, 8u);
}

char* str_concat9(char* s1, char* s2, char* s3, char* s4, char* s5, char* s6, char* s7, char* s8, char* s9) {
    const char* parts[9] = {s1, s2, s3, s4, s5, s6, s7, s8, s9};
    return kain_string_concat_parts(parts, 9u);
}

char* str_concat10(char* s1, char* s2, char* s3, char* s4, char* s5, char* s6, char* s7, char* s8, char* s9, char* s10) {
    const char* parts[10] = {s1, s2, s3, s4, s5, s6, s7, s8, s9, s10};
    return kain_string_concat_parts(parts, 10u);
}

long long clock_wrapper() {
    return kain_attrition_clock_ticks();
}

void array_free_elems(void* ptr) {
    KainArray* arr = (KainArray*)ptr;
    long long i;
    if (!arr) return;
    for (i = 0; i < arr->len; i++) {
        KainRcTrackedPointer tracked = kain_rc_registry_lookup((void*)(intptr_t)arr->data[i]);
        if (tracked.state == KAIN_RC_TRACK_LIVE && tracked.header != NULL) {
            rc_release((void*)(intptr_t)arr->data[i]);
        }
    }
    free(arr->data);
}

KainArray* array_new(long long cap) {
    KainArray* arr = (KainArray*)kain_alloc_rc(sizeof(KainArray), 2);
    if (!arr) return NULL;
    arr->len = 0;
    arr->cap = cap < 4 ? 4 : cap;
    arr->data = (long long*)malloc((size_t)arr->cap * sizeof(long long));
    if (!arr->data) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Array allocation failed",
            "Failed to allocate array data buffer"
        );
        /* Note: arr itself will be cleaned up by RC when released */
        return NULL;
    }
    return arr;
}

void array_push(KainArray* arr, long long val) {
    if (arr->len >= arr->cap) {
        arr->cap *= 2;
        arr->data = (long long*)realloc(arr->data, (size_t)arr->cap * sizeof(long long));
    }
    arr->data[arr->len++] = val;
}

void push(void* arr, long long val) {
    array_push((KainArray*)arr, val);
}

long long array_get(KainArray* arr, long long index) {
    if (index < 0 || index >= arr->len) {
        char detail[128];
        snprintf(detail, sizeof(detail), "Index %lld out of bounds for array of length %lld", index, arr->len);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_FATAL,
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "Array index out of bounds",
            detail
        );
        exit(1);
    }
    return arr->data[index];
}

void array_set(KainArray* arr, long long index, long long val) {
    long long previous;
    KainRcTrackedPointer tracked;
    if (index < 0 || index >= arr->len) {
        char detail[128];
        snprintf(detail, sizeof(detail), "Index %lld out of bounds for array of length %lld", index, arr->len);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_FATAL,
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "Array index out of bounds",
            detail
        );
        exit(1);
    }
    previous = arr->data[index];
    if (previous != val) {
        tracked = kain_rc_registry_lookup((void*)(intptr_t)previous);
        if (tracked.state == KAIN_RC_TRACK_LIVE && tracked.header != NULL) {
            rc_release((void*)(intptr_t)previous);
        }
    }
    arr->data[index] = val;
}

long long array_len(void* value) {
    KainRcTrackedPointer tracked;
    if (!value) return 0;
    tracked = kain_rc_registry_lookup(value);
    if (tracked.state != KAIN_RC_TRACK_LIVE || tracked.header == NULL) {
        return 0;
    }
    if (tracked.header->type_tag == 2) {
        return ((KainArray*)value)->len;
    }
    if (tracked.header->type_tag == 1) {
        return (long long)tracked.header->string_length;
    }
    if (tracked.header->type_tag == 3) {
        return ((KainMap*)value)->count;
    }
    return 0;
}

long long pop(void* arr_ptr) {
    KainArray* arr = (KainArray*)arr_ptr;
    if (!arr || arr->len <= 0) return 0;
    arr->len -= 1;
    return arr->data[arr->len];
}

static KainArray* kain_cli_empty_array(void) {
    KainArray* arr = array_new(1);
    if (!arr) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "CLI array allocation failed",
            "Failed to allocate command-line array"
        );
    }
    return arr;
}

#if !defined(_WIN32) && !defined(__APPLE__)
static KainArray* kain_cli_split_nul_delimited_args(const char* data, size_t length) {
    KainArray* arr = kain_cli_empty_array();
    size_t start = 0u;
    size_t index;
    if (!arr) {
        return NULL;
    }
    if (!data || length == 0u) {
        return arr;
    }
    for (index = 0u; index < length; ++index) {
        if (data[index] == '\0') {
            if (index > start) {
                array_push(arr, (long long)(intptr_t)kain_string_new_with_len(data + start, index - start));
            }
            start = index + 1u;
        }
    }
    if (start < length) {
        array_push(arr, (long long)(intptr_t)kain_string_new_with_len(data + start, length - start));
    }
    return arr;
}
#endif

#ifdef _WIN32
static char* kain_cli_string_from_wide(const wchar_t* wide) {
    char* utf8 = NULL;
    int utf8_bytes;
    char* result;
    if (!wide || !wide[0]) {
        return string_new("");
    }
    utf8_bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (utf8_bytes <= 0) {
        return string_new("");
    }
    utf8 = (char*)malloc((size_t)utf8_bytes);
    if (!utf8) {
        return string_new("");
    }
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8_bytes, NULL, NULL) <= 0) {
        free(utf8);
        return string_new("");
    }
    result = string_new(utf8);
    free(utf8);
    return result;
}
#endif

char* file_read(char* path) {
    FILE* f;
    long size;
    char* buf;

    if (!path) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "File read failed",
            "Path is NULL"
        );
        return NULL;
    }

#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0) f = NULL;
#else
    f = fopen(path, "rb");
#endif
    if (!f) {
        char detail[256];
        snprintf(detail, sizeof(detail), "Failed to open file: %s", path);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "File read failed",
            detail
        );
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = (char*)kain_alloc_rc((size_t)size + 1, 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, (size_t)size, f);
    buf[size] = 0;
    kain_rc_set_string_length(buf, kain_bounded_text_length(buf, (size_t)size));

    fclose(f);
    return buf;
}

char* read_file(char* path) {
    return file_read(path);
}

void file_write(char* path, char* content) {
    FILE* f = NULL;

    if (!path) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "File write failed",
            "Path is NULL"
        );
        return;
    }

#ifdef _WIN32
    if (fopen_s(&f, path, "wb") != 0) f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f) {
        char detail[256];
        snprintf(detail, sizeof(detail), "Failed to open file for writing: %s", path);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "File write failed",
            detail
        );
        return;
    }
    if (content) {
        fprintf(f, "%s", content);
    }
    fclose(f);
}

void write_file(char* path, char* content) {
    file_write(path, content);
}

char* read_line(void) {
    char buffer[8192];
    size_t len;
    if (!fgets(buffer, (int)sizeof(buffer), stdin)) {
        return string_new("");
    }
    len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
        len--;
    }
    return kain_string_new_with_len(buffer, len);
}

void stdout_write(char* text) {
    if (!text) return;
    fputs(text, stdout);
    fflush(stdout);
}

void stderr_write(char* text) {
    if (!text) return;
    fputs(text, stderr);
    fflush(stderr);
}

char* stdin_read_exact(long long length) {
    size_t remaining;
    size_t offset = 0;
    char* buffer;
    if (length <= 0) {
        return string_new("");
    }
    remaining = (size_t)length;
    buffer = (char*)kain_alloc_rc(remaining + 1, 1);
    if (!buffer) {
        return NULL;
    }
    while (remaining > 0) {
        size_t read_count = fread(buffer + offset, 1, remaining, stdin);
        if (read_count == 0) {
            break;
        }
        offset += read_count;
        remaining -= read_count;
    }
    buffer[offset] = '\0';
    kain_rc_set_string_length(buffer, kain_bounded_text_length(buffer, offset));
    return buffer;
}

int file_exists(char* path) {
    if (!path || !path[0]) return 0;
#ifdef _WIN32
    {
        DWORD attrs = GetFileAttributesA(path);
        return attrs != INVALID_FILE_ATTRIBUTES;
    }
#else
    return access(path, F_OK) == 0;
#endif
}

int fs_exists(char* path) {
    return file_exists(path);
}

int fs_is_file(char* path) {
    if (!path || !path[0]) {
        return 0;
    }
#ifdef _WIN32
    {
        DWORD attrs = GetFileAttributesA(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
#else
    {
        struct stat info;
        if (stat(path, &info) != 0) {
            return 0;
        }
        return S_ISREG(info.st_mode);
    }
#endif
}

int fs_is_dir(char* path) {
    if (!path || !path[0]) {
        return 0;
    }
#ifdef _WIN32
    {
        DWORD attrs = GetFileAttributesA(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
#else
    {
        struct stat info;
        if (stat(path, &info) != 0) {
            return 0;
        }
        return S_ISDIR(info.st_mode);
    }
#endif
}

char* env(char* name) {
    if (!name || !name[0]) {
        return string_new("");
    }
#ifdef _WIN32
    {
        char* value = NULL;
        size_t length = 0;
        if (_dupenv_s(&value, &length, name) != 0 || !value) {
            free(value);
            return string_new("");
        }
        if (!value[0]) {
            free(value);
            return string_new("");
        }
        {
            char* result = string_new(value);
            free(value);
            return result;
        }
    }
#else
    {
        const char* value = getenv(name);
        if (!value || !value[0]) {
            return string_new("");
        }
        return string_new((char*)value);
    }
#endif
}

char* cwd(void) {
#ifdef _WIN32
    DWORD needed = GetCurrentDirectoryW(0, NULL);
    wchar_t* buffer = NULL;
    char* result;
    if (needed == 0) {
        return string_new("");
    }
    buffer = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
    if (!buffer) {
        return string_new("");
    }
    if (GetCurrentDirectoryW(needed, buffer) == 0) {
        free(buffer);
        return string_new("");
    }
    result = kain_cli_string_from_wide(buffer);
    free(buffer);
    return result;
#else
    size_t capacity = 256u;
    char* buffer = NULL;
    while (capacity <= (size_t)(1024u * 1024u)) {
        buffer = (char*)malloc(capacity);
        if (!buffer) {
            return string_new("");
        }
        if (getcwd(buffer, capacity) != NULL) {
            char* result = string_new(buffer);
            free(buffer);
            return result;
        }
        free(buffer);
        if (errno != ERANGE) {
            return string_new("");
        }
        capacity <<= 1u;
    }
    return string_new("");
#endif
}

int64_t args(void) {
#ifdef _WIN32
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    KainArray* arr = kain_cli_empty_array();
    int index;
    if (!arr) {
        if (argv) {
            LocalFree(argv);
        }
        return 0;
    }
    if (!argv || argc <= 0) {
        if (argv) {
            LocalFree(argv);
        }
        return (int64_t)(intptr_t)arr;
    }
    for (index = 0; index < argc; ++index) {
        array_push(arr, (long long)(intptr_t)kain_cli_string_from_wide(argv[index]));
    }
    LocalFree(argv);
    return (int64_t)(intptr_t)arr;
#elif defined(__APPLE__)
    char*** argv_ref = _NSGetArgv();
    int argc = *_NSGetArgc();
    KainArray* arr = kain_cli_empty_array();
    int index;
    if (!arr) {
        return 0;
    }
    if (!argv_ref || !*argv_ref || argc <= 0) {
        return (int64_t)(intptr_t)arr;
    }
    for (index = 0; index < argc; ++index) {
        const char* value = (*argv_ref)[index];
        array_push(arr, (long long)(intptr_t)string_new((char*)(value ? value : "")));
    }
    return (int64_t)(intptr_t)arr;
#else
    FILE* file = fopen("/proc/self/cmdline", "rb");
    unsigned char chunk[4096];
    char* buffer = NULL;
    size_t length = 0u;
    size_t capacity = 0u;
    KainArray* arr;
    if (!file) {
        return (int64_t)(intptr_t)kain_cli_empty_array();
    }
    while (!feof(file)) {
        size_t read_count = fread(chunk, 1u, sizeof(chunk), file);
        size_t needed = 0u;
        char* grown;
        if (read_count == 0u) {
            break;
        }
        if (!kain_size_add_checked(length, read_count, &needed)) {
            free(buffer);
            fclose(file);
            return (int64_t)(intptr_t)kain_cli_empty_array();
        }
        if (needed > capacity) {
            size_t next_capacity = capacity == 0u ? 4096u : capacity;
            while (next_capacity < needed) {
                if (!kain_size_add_checked(next_capacity, next_capacity, &next_capacity)) {
                    free(buffer);
                    fclose(file);
                    return (int64_t)(intptr_t)kain_cli_empty_array();
                }
            }
            grown = (char*)realloc(buffer, next_capacity);
            if (!grown) {
                free(buffer);
                fclose(file);
                return (int64_t)(intptr_t)kain_cli_empty_array();
            }
            buffer = grown;
            capacity = next_capacity;
        }
        memcpy(buffer + length, chunk, read_count);
        length += read_count;
    }
    fclose(file);
    arr = kain_cli_split_nul_delimited_args(buffer, length);
    free(buffer);
    return (int64_t)(intptr_t)arr;
#endif
}

static int kain_path_is_separator(char ch) {
    return ch == '/' || ch == '\\';
}

static int kain_path_is_ascii_drive_letter(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int kain_path_is_absolute(const char* path) {
    if (!path || !path[0]) {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    return strlen(path) > 2u && path[1] == ':';
}

static size_t kain_path_root_span(const char* path) {
    size_t length = 0u;
    if (!path || !path[0]) {
        return 0u;
    }
    length = strlen(path);
#ifdef _WIN32
    if (kain_path_is_ascii_drive_letter(path[0]) && path[1] == ':') {
        return kain_path_is_separator(path[2]) ? 3u : 2u;
    }
    if (kain_path_is_separator(path[0]) && kain_path_is_separator(path[1])) {
        size_t index = 2u;
        if (length > index + 1u &&
            (path[index] == '?' || path[index] == '.') &&
            kain_path_is_separator(path[index + 1u])) {
            index += 2u;
            if (length > index + 3u &&
                (path[index] == 'U' || path[index] == 'u') &&
                (path[index + 1u] == 'N' || path[index + 1u] == 'n') &&
                (path[index + 2u] == 'C' || path[index + 2u] == 'c') &&
                kain_path_is_separator(path[index + 3u])) {
                index += 4u;
            } else if (length > index + 2u &&
                       kain_path_is_ascii_drive_letter(path[index]) &&
                       path[index + 1u] == ':' &&
                       kain_path_is_separator(path[index + 2u])) {
                return index + 3u;
            } else {
                return length;
            }
        }
        {
            int segment = 0;
            while (index < length && kain_path_is_separator(path[index])) {
                index += 1u;
            }
            for (; index < length; ++segment) {
                while (index < length && !kain_path_is_separator(path[index])) {
                    index += 1u;
                }
                if (segment == 1) {
                    while (index < length && kain_path_is_separator(path[index])) {
                        index += 1u;
                    }
                    return index;
                }
                while (index < length && kain_path_is_separator(path[index])) {
                    index += 1u;
                }
            }
            return index;
        }
    }
#endif
    return kain_path_is_separator(path[0]) ? 1u : 0u;
}

static size_t kain_path_trimmed_end(const char* path, size_t root_span) {
    size_t end;
    if (!path) {
        return 0u;
    }
    end = strlen(path);
    while (end > root_span && kain_path_is_separator(path[end - 1u])) {
        end -= 1u;
    }
    return end;
}

static int kain_fs_create_one_dir(const char* path) {
    if (!path || !path[0]) {
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

static int kain_fs_create_parent_dirs(const char* path) {
    char buffer[4096];
    size_t length;
    size_t index;
    size_t root_span;
    if (!path || !path[0]) {
        return 0;
    }
    length = strlen(path);
    if (length >= sizeof(buffer)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(buffer, path, length + 1u);
    root_span = kain_path_root_span(buffer);
    for (index = root_span; index < length; ++index) {
        if (kain_path_is_separator(buffer[index])) {
            char saved = buffer[index];
            if (index <= root_span || kain_path_is_separator(buffer[index - 1u])) {
                continue;
            }
            buffer[index] = '\0';
            if (buffer[0] != '\0' && kain_fs_create_one_dir(buffer) != 0) {
                buffer[index] = saved;
                return -1;
            }
            buffer[index] = saved;
        }
    }
    return 0;
}

static int kain_fs_open_write_retry_parent_dirs(const char* path, const char* mode, FILE** out_file) {
    if (!out_file) {
        errno = EINVAL;
        return -1;
    }
    *out_file = NULL;
#ifdef _WIN32
    if (fopen_s(out_file, path, mode) == 0 && *out_file != NULL) {
        return 0;
    }
#else
    *out_file = fopen(path, mode);
    if (*out_file != NULL) {
        return 0;
    }
#endif
    if (kain_fs_create_parent_dirs(path) != 0) {
        return -1;
    }
#ifdef _WIN32
    if (fopen_s(out_file, path, mode) == 0 && *out_file != NULL) {
        return 0;
    }
#else
    *out_file = fopen(path, mode);
    if (*out_file != NULL) {
        return 0;
    }
#endif
    return -1;
}

static KainArray* kain_fs_read_dir_entries(const char* path) {
    KainArray* arr = kain_cli_empty_array();
    if (!arr || !path || !path[0]) {
        return arr;
    }
#ifdef _WIN32
    {
        char pattern[4096];
        WIN32_FIND_DATAA data;
        HANDLE handle;
        if (_snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*", path) < 0) {
            return arr;
        }
        handle = FindFirstFileA(pattern, &data);
        if (handle == INVALID_HANDLE_VALUE) {
            return arr;
        }
        do {
            char child[4096];
            if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                continue;
            }
            if (_snprintf_s(child, sizeof(child), _TRUNCATE, "%s\\%s", path, data.cFileName) < 0) {
                continue;
            }
            array_push(arr, (long long)(intptr_t)string_new(child));
        } while (FindNextFileA(handle, &data) != 0);
        FindClose(handle);
    }
#else
    {
        DIR* dir = opendir(path);
        struct dirent* entry;
        if (!dir) {
            return arr;
        }
        while ((entry = readdir(dir)) != NULL) {
            char child[4096];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0) {
                continue;
            }
            array_push(arr, (long long)(intptr_t)string_new(child));
        }
        closedir(dir);
    }
#endif
    return arr;
}

KainArray* read_dir(char* path) {
    return kain_fs_read_dir_entries(path);
}

KainArray* fs_read_dir_paths(char* path) {
    return read_dir(path);
}

void create_dir_all(char* path) {
    char buffer[4096];
    size_t length;
    size_t index;
    size_t root_span;
    if (!path || !path[0]) {
        return;
    }
    length = strlen(path);
    if (length >= sizeof(buffer)) {
        return;
    }
    memcpy(buffer, path, length + 1u);
    root_span = kain_path_root_span(buffer);
    for (index = root_span; index <= length; ++index) {
        if (kain_path_is_separator(buffer[index]) || buffer[index] == '\0') {
            char saved = buffer[index];
            if (index <= root_span || kain_path_is_separator(buffer[index - 1u])) {
                continue;
            }
            buffer[index] = '\0';
            if (buffer[0] != '\0' && kain_fs_create_one_dir(buffer) != 0) {
                buffer[index] = saved;
                return;
            }
            buffer[index] = saved;
        }
    }
}

void fs_create_dir_all(char* path) {
    create_dir_all(path);
}

void copy_file(char* src, char* dest) {
    FILE* input = NULL;
    FILE* output = NULL;
    char buffer[65536];
    size_t read_count;
    if (!src || !dest) {
        return;
    }
#ifdef _WIN32
    if (fopen_s(&input, src, "rb") != 0) {
        input = NULL;
    }
#else
    input = fopen(src, "rb");
#endif
    if (!input) {
        return;
    }
    if (kain_fs_open_write_retry_parent_dirs(dest, "wb", &output) != 0 || !output) {
        fclose(input);
        return;
    }
    while ((read_count = fread(buffer, 1u, sizeof(buffer), input)) > 0u) {
        if (fwrite(buffer, 1u, read_count, output) != read_count) {
            fclose(input);
            fclose(output);
            return;
        }
    }
    fclose(input);
    fclose(output);
}

void fs_copy_file(char* src, char* dest) {
    copy_file(src, dest);
}

void remove_file(char* path) {
    if (!path || !path[0]) {
        return;
    }
#ifdef _WIN32
    (void)DeleteFileA(path);
#else
    (void)remove(path);
#endif
}

void fs_remove_file(char* path) {
    remove_file(path);
}

char* path_join(char* base, char* child) {
    char joined[4096];
    size_t base_len;
    if (!child) {
        child = "";
    }
    if (!base || !base[0] || kain_path_is_absolute(child)) {
        return string_new(child);
    }
    base_len = strlen(base);
    if (base_len > 0u && kain_path_is_separator(base[base_len - 1u])) {
#ifdef _WIN32
        if (_snprintf_s(joined, sizeof(joined), _TRUNCATE, "%s%s", base, child) < 0) {
            return string_new("");
        }
#else
        if (snprintf(joined, sizeof(joined), "%s%s", base, child) < 0) {
            return string_new("");
        }
#endif
    } else {
#ifdef _WIN32
        if (_snprintf_s(joined, sizeof(joined), _TRUNCATE, "%s\\%s", base, child) < 0) {
            return string_new("");
        }
#else
        if (snprintf(joined, sizeof(joined), "%s/%s", base, child) < 0) {
            return string_new("");
        }
#endif
    }
    return string_new(joined);
}

char* fs_path_join(char* base, char* child) {
    return path_join(base, child);
}

char* path_parent(char* path) {
    size_t root_span;
    size_t end;
    size_t separator;
    if (!path || !path[0]) {
        return string_new("");
    }
    root_span = kain_path_root_span(path);
    end = kain_path_trimmed_end(path, root_span);
    if (end <= root_span) {
        return string_new("");
    }
    separator = end;
    while (separator > root_span && !kain_path_is_separator(path[separator - 1u])) {
        separator -= 1u;
    }
    if (separator <= root_span) {
        if (root_span > 0u) {
            return kain_string_new_with_len(path, root_span);
        }
        return string_new("");
    }
    while (separator > root_span && kain_path_is_separator(path[separator - 1u])) {
        separator -= 1u;
    }
    return kain_string_new_with_len(path, separator);
}

char* fs_path_parent(char* path) {
    return path_parent(path);
}

char* path_file_name(char* path) {
    size_t root_span;
    size_t end;
    size_t start;
    if (!path || !path[0]) {
        return string_new("");
    }
    root_span = kain_path_root_span(path);
    end = kain_path_trimmed_end(path, root_span);
    if (end <= root_span) {
        return string_new("");
    }
    start = end;
    while (start > root_span && !kain_path_is_separator(path[start - 1u])) {
        start -= 1u;
    }
    return kain_string_new_with_len(path + start, end - start);
}

char* fs_path_file_name(char* path) {
    return path_file_name(path);
}

char* path_extension(char* path) {
    size_t root_span;
    size_t end;
    size_t start;
    size_t index;
    if (!path || !path[0]) {
        return string_new("");
    }
    root_span = kain_path_root_span(path);
    end = kain_path_trimmed_end(path, root_span);
    if (end <= root_span) {
        return string_new("");
    }
    start = end;
    while (start > root_span && !kain_path_is_separator(path[start - 1u])) {
        start -= 1u;
    }
    for (index = end; index > start; --index) {
        if (path[index - 1u] == '.') {
            if ((index - 1u) == start) {
                return string_new("");
            }
            return kain_string_new_with_len(path + index, end - index);
        }
    }
    return string_new("");
}

char* fs_path_extension(char* path) {
    return path_extension(path);
}

char* path_stem(char* path) {
    size_t root_span;
    size_t end;
    size_t start;
    size_t index;
    if (!path || !path[0]) {
        return string_new("");
    }
    root_span = kain_path_root_span(path);
    end = kain_path_trimmed_end(path, root_span);
    if (end <= root_span) {
        return string_new("");
    }
    start = end;
    while (start > root_span && !kain_path_is_separator(path[start - 1u])) {
        start -= 1u;
    }
    for (index = end; index > start; --index) {
        if (path[index - 1u] == '.') {
            if ((index - 1u) == start) {
                return kain_string_new_with_len(path + start, end - start);
            }
            return kain_string_new_with_len(path + start, (index - 1u) - start);
        }
    }
    return kain_string_new_with_len(path + start, end - start);
}

char* fs_path_stem(char* path) {
    return path_stem(path);
}

int path_is_file(char* path) {
    return fs_is_file(path);
}

int path_is_dir(char* path) {
    return fs_is_dir(path);
}

char* trim(char* s) {
    const char* start;
    const char* end;
    if (!s) {
        return string_new("");
    }
    start = s;
    while (*start && kain_char_is_space(*start)) {
        start++;
    }
    end = start + strlen(start);
    while (end > start && kain_char_is_space(*(end - 1))) {
        end--;
    }
    return kain_string_new_with_len(start, (size_t)(end - start));
}

char* to_upper(char* s) {
    size_t i;
    size_t len;
    char* out;
    if (!s) {
        return string_new("");
    }
    len = kain_string_len_rc(s);
    out = (char*)kain_alloc_rc(len + 1, 1);
    if (!out) return NULL;
    for (i = 0; i < len; ++i) {
        out[i] = (char)toupper((unsigned char)s[i]);
    }
    out[len] = '\0';
    return out;
}

char* to_lower(char* s) {
    size_t i;
    size_t len;
    char* out;
    if (!s) {
        return string_new("");
    }
    len = kain_string_len_rc(s);
    out = (char*)kain_alloc_rc(len + 1, 1);
    if (!out) return NULL;
    for (i = 0; i < len; ++i) {
        out[i] = (char)tolower((unsigned char)s[i]);
    }
    out[len] = '\0';
    return out;
}

int contains(char* s, char* sub) {
    size_t s_len;
    size_t sub_len;
    if (!s || !sub) return 0;
    s_len = kain_string_len_rc(s);
    sub_len = kain_string_len_rc(sub);
    return kain_find_substring_bytes(s, s_len, sub, sub_len, 0u) != NULL;
}

int starts_with(char* s, char* prefix) {
    size_t s_len;
    size_t prefix_len;
    if (!s || !prefix) return 0;
    s_len = kain_string_len_rc(s);
    prefix_len = kain_string_len_rc(prefix);
    return prefix_len <= s_len && memcmp(s, prefix, prefix_len) == 0;
}

int ends_with(char* s, char* suffix) {
    size_t s_len;
    size_t suffix_len;
    if (!s || !suffix) return 0;
    s_len = kain_string_len_rc(s);
    suffix_len = kain_string_len_rc(suffix);
    if (suffix_len > s_len) return 0;
    return memcmp(s + (s_len - suffix_len), suffix, suffix_len) == 0;
}

char* char_at(char* s, long long index) {
    size_t s_len;
    char ch;
    if (!s || index < 0) {
        return string_new("");
    }
    s_len = kain_string_len_rc(s);
    if ((size_t)index >= s_len) {
        return string_new("");
    }
    ch = s[index];
    return kain_string_new_with_len(&ch, 1);
}

char* substring(char* s, long long start, long long end) {
    size_t s_len;
    size_t slice_start;
    size_t slice_end;
    if (!s) {
        return string_new("");
    }
    s_len = kain_string_len_rc(s);
    if (start < 0) start = 0;
    if (end < 0 || (size_t)end > s_len) end = (long long)s_len;
    if ((size_t)start > s_len) start = (long long)s_len;
    if (end < start) end = start;
    slice_start = (size_t)start;
    slice_end = (size_t)end;
    return kain_string_new_with_len(s + slice_start, slice_end - slice_start);
}

long long find_substring_from(char* s, char* needle, long long start) {
    const char* match;
    size_t s_len;
    size_t needle_len;
    size_t offset;
    if (!s || !needle) return -1;
    if (start < 0) start = 0;
    s_len = kain_string_len_rc(s);
    if ((size_t)start > s_len) return -1;
    needle_len = kain_string_len_rc(needle);
    if (needle_len == 0u) return start;
    match = kain_find_substring_bytes(s, s_len, needle, needle_len, (size_t)start);
    if (!match) return -1;
    offset = (size_t)(match - s);
    if (offset > (size_t)LLONG_MAX) return -1;
    return (long long)offset;
}

long long find_substring_from_known_lengths(
    char* s,
    long long s_len,
    char* needle,
    long long needle_len,
    long long start
) {
    const char* match;
    size_t haystack_len;
    size_t needle_len_size;
    size_t offset;
    if (!s || !needle || s_len < 0 || needle_len < 0) return -1;
    if (start < 0) start = 0;
    haystack_len = (size_t)s_len;
    if ((size_t)start > haystack_len) return -1;
    needle_len_size = (size_t)needle_len;
    if (needle_len_size == 0u) return start;
    match = kain_find_substring_bytes(s, haystack_len, needle, needle_len_size, (size_t)start);
    if (!match) return -1;
    offset = (size_t)(match - s);
    if (offset > (size_t)LLONG_MAX) return -1;
    return (long long)offset;
}

long long byte_at(char* s, long long index) {
    size_t s_len;
    if (!s || index < 0) return -1;
    s_len = kain_string_len_rc(s);
    if ((size_t)index >= s_len) return -1;
    return (long long)((unsigned char)s[index]);
}

char* replace(char* s, char* from, char* to) {
    const char* cursor;
    const char* match;
    size_t source_len;
    size_t from_len;
    size_t to_len;
    size_t final_len = 0;
    size_t replacements = 0;
    char* out;
    char* write_cursor;
    if (!s) return string_new("");
    if (!from || !from[0]) return string_new(s);
    if (!to) to = "";

    source_len = strlen(s);
    from_len = strlen(from);
    to_len = strlen(to);
    cursor = s;
    while ((match = strstr(cursor, from)) != NULL) {
        final_len += (size_t)(match - cursor);
        final_len += to_len;
        cursor = match + from_len;
        replacements++;
    }
    final_len += strlen(cursor);
    if (replacements == 0) {
        return string_new(s);
    }
    out = (char*)kain_alloc_rc(final_len + 1, 1);
    if (!out) return NULL;
    write_cursor = out;
    cursor = s;
    while ((match = strstr(cursor, from)) != NULL) {
        size_t prefix_len = (size_t)(match - cursor);
        if (prefix_len > 0) {
            memcpy(write_cursor, cursor, prefix_len);
            write_cursor += prefix_len;
        }
        if (to_len > 0) {
            memcpy(write_cursor, to, to_len);
            write_cursor += to_len;
        }
        cursor = match + from_len;
    }
    if (*cursor) {
        size_t tail_len = strlen(cursor);
        memcpy(write_cursor, cursor, tail_len);
        write_cursor += tail_len;
    }
    *write_cursor = '\0';
    (void)source_len;
    return out;
}

long long len(void* value) {
    KainRcTrackedPointer tracked;
    RcHeader* header;
    if (!value) return 0;
    tracked = kain_rc_registry_lookup(value);
    if (tracked.state != KAIN_RC_TRACK_LIVE || tracked.header == NULL) {
        return 0;
    }
    header = tracked.header;
    if (header->type_tag == 1) {
        return (long long)header->string_length;
    }
    if (header->type_tag == 2) {
        return ((KainArray*)value)->len;
    }
    if (header->type_tag == 3) {
        return ((KainMap*)value)->count;
    }
    return 0;
}

typedef struct {
    uint64_t any_match;
    uint64_t any_empty;
    uint64_t selected_match_index;
    uint64_t selected_empty_index;
    uint64_t selected_value;
} KainMapProbeWindow;

typedef struct {
    uint64_t has_match;
    uint64_t has_empty;
    uint64_t match_index;
    uint64_t empty_index;
} KainMapSlotSearch;

static uint64_t kain_mix_u64(uint64_t value);

static uint64_t kain_rotate_left_u64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64u - shift));
}

static uint64_t kain_map_nonzero_bit(uint64_t value) {
    return (value | (0u - value)) >> 63u;
}

static uint64_t kain_map_zero_bit(uint64_t value) {
    return kain_map_nonzero_bit(value) ^ 1u;
}

static uint64_t kain_map_tiny_fingerprint(uint64_t key_hash, uint64_t key_prefix, uint64_t key_length) {
    return key_hash ^
        kain_rotate_left_u64(key_prefix, 17u) ^
        (key_length * 0x9e3779b97f4a7c15ULL);
}

static uint64_t kain_map_tiny_slot(uint64_t fingerprint, uint64_t magic) {
    return (fingerprint * magic) >> 58u;
}

static void kain_map_disable_tiny_dispatch(KainMap* map) {
    if (!map) {
        return;
    }
    map->tiny_magic = 0u;
    map->tiny_ready = 0u;
    memset(map->tiny_dispatch, 0xFF, sizeof(map->tiny_dispatch));
}

static void kain_map_rebuild_tiny_dispatch(KainMap* map) {
    uint64_t fingerprints[KAIN_MAP_TINY_MAX_COUNT];
    uint8_t entry_indices[KAIN_MAP_TINY_MAX_COUNT];
    uint8_t dispatch[KAIN_MAP_TINY_DISPATCH_SIZE];
    uint64_t occupied_count = 0u;
    uint64_t entry_index;
    uint64_t base_seed;
    uint64_t attempt;

    kain_map_disable_tiny_dispatch(map);
    if (!map || map->count <= 0) {
        return;
    }
    if ((uint64_t)map->count > KAIN_MAP_TINY_MAX_COUNT) {
        return;
    }
    if ((uint64_t)map->capacity > (uint64_t)KAIN_MAP_TINY_EMPTY_INDEX) {
        return;
    }

    for (entry_index = 0u; entry_index < (uint64_t)map->capacity; ++entry_index) {
        MapEntry* entry = &map->entries[entry_index];
        if (entry->occupied) {
            if (occupied_count >= KAIN_MAP_TINY_MAX_COUNT) {
                return;
            }
            fingerprints[occupied_count] = kain_map_tiny_fingerprint(
                entry->hash,
                entry->key_prefix,
                (uint64_t)entry->key_length
            );
            entry_indices[occupied_count] = (uint8_t)entry_index;
            occupied_count += 1u;
        }
    }

    if (occupied_count != (uint64_t)map->count) {
        return;
    }

    base_seed = kain_mix_u64(
        fingerprints[0] ^
        ((uint64_t)occupied_count * 0x94d049bb133111ebULL) ^
        map->mask
    ) | 1u;

    for (attempt = 0u; attempt < 4096u; ++attempt) {
        uint64_t magic = (base_seed + (attempt * 0x9e3779b97f4a7c15ULL)) | 1u;
        uint64_t probe_index;
        uint64_t collision = 0u;
        memset(dispatch, 0xFF, sizeof(dispatch));
        for (probe_index = 0u; probe_index < occupied_count; ++probe_index) {
            uint64_t slot = kain_map_tiny_slot(fingerprints[probe_index], magic);
            if (dispatch[slot] != KAIN_MAP_TINY_EMPTY_INDEX) {
                collision = 1u;
                break;
            }
            dispatch[slot] = entry_indices[probe_index];
        }
        if (!collision) {
            map->tiny_magic = magic;
            map->tiny_ready = 1u;
            memcpy(map->tiny_dispatch, dispatch, sizeof(dispatch));
            return;
        }
    }
}

static uint64_t kain_map_select_u64(uint64_t left, uint64_t right, uint64_t bit) {
    uint64_t mask = 0u - bit;
    return (left & ~mask) | (right & mask);
}

static uint64_t kain_mix_u64(uint64_t value) {
    value ^= value >> 33u;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33u;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33u;
    return value;
}

static uint64_t kain_hash_bytes(const unsigned char* bytes, size_t length) {
    const uint64_t seed = 0x9e3779b97f4a7c15ULL;
    const uint64_t step = 0x94d049bb133111ebULL;
    uint64_t hash = seed ^ ((uint64_t)length * step);

    for (; length >= sizeof(uint64_t); length -= sizeof(uint64_t)) {
        uint64_t chunk;
        memcpy(&chunk, bytes, sizeof(chunk));
        hash ^= kain_mix_u64(chunk + seed);
        hash = kain_rotate_left_u64(hash, 27u) * step + seed;
        bytes += sizeof(uint64_t);
    }

    if (length > 0u) {
        uint64_t tail = 0u;
        memcpy(&tail, bytes, length);
        hash ^= kain_mix_u64(tail ^ ((uint64_t)length << 56u));
        hash = kain_rotate_left_u64(hash, 27u) * step + seed;
    }

    return kain_mix_u64(hash);
}

static uint64_t kain_map_magic_prefix_state(
    uint64_t word0,
    uint64_t word1,
    uint64_t word2,
    uint64_t word3,
    uint64_t length
) {
    const uint64_t magic = 0x64170d358aa115a1ULL;
    const uint64_t lane1 = 0x9e3779b97f4a7c15ULL;
    const uint64_t lane2 = 0xbf58476d1ce4e5b9ULL;
    const uint64_t lane3 = 0x94d049bb133111ebULL;
    const uint64_t lane4 = 0xd6e8feb86659fd93ULL;
    uint64_t folded0 = (word0 ^ length) * magic;
    uint64_t folded1 = (word1 ^ kain_rotate_left_u64(magic, 13u)) * lane1;
    uint64_t folded2 = (word2 ^ kain_rotate_left_u64(magic, 27u)) * lane2;
    uint64_t folded3 = (word3 ^ (magic ^ lane3)) * lane4;
    uint64_t state = folded0 ^ folded1 ^ folded2 ^ folded3;
    return ((state ^ (state >> 33u)) * 0xff51afd7ed558ccdULL) ^ (state >> 29u);
}

static void kain_map_key_metadata(
    const char* key,
    size_t* out_length,
    uint64_t* out_hash,
    uint64_t* out_prefix
) {
    size_t key_length = strlen(key);
    size_t prefix_length = key_length < 32u ? key_length : 32u;
    uint64_t prefix_words[4] = {0u, 0u, 0u, 0u};
    uint64_t key_prefix;

    if (prefix_length > 0u) {
        memcpy(prefix_words, key, prefix_length);
    }
    key_prefix = kain_map_magic_prefix_state(
        prefix_words[0],
        prefix_words[1],
        prefix_words[2],
        prefix_words[3],
        (uint64_t)key_length
    );

    if (out_length) {
        *out_length = key_length;
    }
    if (out_hash) {
        *out_hash = kain_hash_bytes((const unsigned char*)key, key_length);
    }
    if (out_prefix) {
        *out_prefix = key_prefix;
    }
}

static uint64_t kain_map_growth_threshold(uint64_t capacity) {
    /* Proof: runtime/native/src/core/z3/proofs/native-map-growth-threshold-stays-below-capacity.yaml */
    return capacity - (capacity >> 2u);
}

static uint64_t kain_map_entry_match_bit(
    const MapEntry* entry,
    const char* key,
    size_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix
) {
    uint64_t occupied = kain_map_nonzero_bit((uint64_t)(unsigned int)entry->occupied);
    uint64_t hash_match = kain_map_zero_bit(entry->hash ^ key_hash);
    uint64_t prefix_match = kain_map_zero_bit(entry->key_prefix ^ key_prefix);
    uint64_t length_match = kain_map_zero_bit((uint64_t)entry->key_length ^ (uint64_t)key_length);
    uint64_t metadata_match = occupied & hash_match & prefix_match & length_match;
    uint64_t exact_match = metadata_match
        ? (uint64_t)(entry->key == key || memcmp(entry->key, key, key_length) == 0)
        : 0u;
    return metadata_match & exact_match;
}

static int kain_map_entry_matches_prehashed(
    const MapEntry* entry,
    const char* key,
    size_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix
) {
    return entry->occupied &&
        entry->hash == key_hash &&
        entry->key_prefix == key_prefix &&
        entry->key_length == key_length &&
        (entry->key == key || memcmp(entry->key, key, key_length) == 0);
}

static int kain_map_tiny_get_prehashed(
    KainMap* map,
    char* key,
    size_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix,
    long long* out_value
) {
    uint64_t slot;
    uint8_t entry_index;
    MapEntry* entry;

    if (!map || !map->tiny_ready) {
        return -1;
    }

    slot = kain_map_tiny_slot(
        kain_map_tiny_fingerprint(key_hash, key_prefix, (uint64_t)key_length),
        map->tiny_magic
    );
    entry_index = map->tiny_dispatch[slot];
    if (entry_index == KAIN_MAP_TINY_EMPTY_INDEX) {
        return 0;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-map-tiny-dispatch-metadata-guard.yaml */
    if ((uint64_t)entry_index >= (uint64_t)map->capacity) {
        return -1;
    }

    entry = &map->entries[entry_index];
    if (!entry->occupied) {
        return -1;
    }
    if (!kain_map_entry_matches_prehashed(entry, key, key_length, key_hash, key_prefix)) {
        return 0;
    }
    *out_value = entry->value;
    return 1;
}

static KainMapProbeWindow kain_map_probe_window(
    KainMap* map,
    const char* key,
    size_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix,
    uint64_t base_index
) {
    uint64_t index0 = (base_index + 0u) & map->mask;
    uint64_t index1 = (base_index + 1u) & map->mask;
    uint64_t index2 = (base_index + 2u) & map->mask;
    uint64_t index3 = (base_index + 3u) & map->mask;
    uint64_t index4 = (base_index + 4u) & map->mask;
    uint64_t index5 = (base_index + 5u) & map->mask;
    uint64_t index6 = (base_index + 6u) & map->mask;
    uint64_t index7 = (base_index + 7u) & map->mask;
    MapEntry* entry0 = &map->entries[index0];
    MapEntry* entry1 = &map->entries[index1];
    MapEntry* entry2 = &map->entries[index2];
    MapEntry* entry3 = &map->entries[index3];
    MapEntry* entry4 = &map->entries[index4];
    MapEntry* entry5 = &map->entries[index5];
    MapEntry* entry6 = &map->entries[index6];
    MapEntry* entry7 = &map->entries[index7];
    uint64_t occupied0 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry0->occupied);
    uint64_t occupied1 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry1->occupied);
    uint64_t occupied2 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry2->occupied);
    uint64_t occupied3 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry3->occupied);
    uint64_t occupied4 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry4->occupied);
    uint64_t occupied5 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry5->occupied);
    uint64_t occupied6 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry6->occupied);
    uint64_t occupied7 = kain_map_nonzero_bit((uint64_t)(unsigned int)entry7->occupied);
    uint64_t empty0 = occupied0 ^ 1u;
    uint64_t empty1 = occupied1 ^ 1u;
    uint64_t empty2 = occupied2 ^ 1u;
    uint64_t empty3 = occupied3 ^ 1u;
    uint64_t empty4 = occupied4 ^ 1u;
    uint64_t empty5 = occupied5 ^ 1u;
    uint64_t empty6 = occupied6 ^ 1u;
    uint64_t empty7 = occupied7 ^ 1u;
    uint64_t match0 = kain_map_entry_match_bit(entry0, key, key_length, key_hash, key_prefix);
    uint64_t match1 = kain_map_entry_match_bit(entry1, key, key_length, key_hash, key_prefix);
    uint64_t match2 = kain_map_entry_match_bit(entry2, key, key_length, key_hash, key_prefix);
    uint64_t match3 = kain_map_entry_match_bit(entry3, key, key_length, key_hash, key_prefix);
    uint64_t match4 = kain_map_entry_match_bit(entry4, key, key_length, key_hash, key_prefix);
    uint64_t match5 = kain_map_entry_match_bit(entry5, key, key_length, key_hash, key_prefix);
    uint64_t match6 = kain_map_entry_match_bit(entry6, key, key_length, key_hash, key_prefix);
    uint64_t match7 = kain_map_entry_match_bit(entry7, key, key_length, key_hash, key_prefix);
    uint64_t seen_match0 = match0;
    uint64_t take_match0 = match0;
    uint64_t take_match1 = match1 & kain_map_zero_bit(seen_match0);
    uint64_t seen_match1 = seen_match0 | match1;
    uint64_t take_match2 = match2 & kain_map_zero_bit(seen_match1);
    uint64_t seen_match2 = seen_match1 | match2;
    uint64_t take_match3 = match3 & kain_map_zero_bit(seen_match2);
    uint64_t seen_match3 = seen_match2 | match3;
    uint64_t take_match4 = match4 & kain_map_zero_bit(seen_match3);
    uint64_t seen_match4 = seen_match3 | match4;
    uint64_t take_match5 = match5 & kain_map_zero_bit(seen_match4);
    uint64_t seen_match5 = seen_match4 | match5;
    uint64_t take_match6 = match6 & kain_map_zero_bit(seen_match5);
    uint64_t seen_match6 = seen_match5 | match6;
    uint64_t take_match7 = match7 & kain_map_zero_bit(seen_match6);
    uint64_t seen_match7 = seen_match6 | match7;
    uint64_t seen_empty0 = empty0;
    uint64_t take_empty0 = empty0;
    uint64_t take_empty1 = empty1 & kain_map_zero_bit(seen_empty0);
    uint64_t seen_empty1 = seen_empty0 | empty1;
    uint64_t take_empty2 = empty2 & kain_map_zero_bit(seen_empty1);
    uint64_t seen_empty2 = seen_empty1 | empty2;
    uint64_t take_empty3 = empty3 & kain_map_zero_bit(seen_empty2);
    uint64_t seen_empty3 = seen_empty2 | empty3;
    uint64_t take_empty4 = empty4 & kain_map_zero_bit(seen_empty3);
    uint64_t seen_empty4 = seen_empty3 | empty4;
    uint64_t take_empty5 = empty5 & kain_map_zero_bit(seen_empty4);
    uint64_t seen_empty5 = seen_empty4 | empty5;
    uint64_t take_empty6 = empty6 & kain_map_zero_bit(seen_empty5);
    uint64_t seen_empty6 = seen_empty5 | empty6;
    uint64_t take_empty7 = empty7 & kain_map_zero_bit(seen_empty6);
    uint64_t seen_empty7 = seen_empty6 | empty7;
    KainMapProbeWindow window;
    window.any_match = seen_match7;
    window.any_empty = seen_empty7;
    window.selected_match_index =
        (index0 * take_match0) |
        (index1 * take_match1) |
        (index2 * take_match2) |
        (index3 * take_match3) |
        (index4 * take_match4) |
        (index5 * take_match5) |
        (index6 * take_match6) |
        (index7 * take_match7);
    window.selected_empty_index =
        (index0 * take_empty0) |
        (index1 * take_empty1) |
        (index2 * take_empty2) |
        (index3 * take_empty3) |
        (index4 * take_empty4) |
        (index5 * take_empty5) |
        (index6 * take_empty6) |
        (index7 * take_empty7);
    window.selected_value =
        ((uint64_t)entry0->value & (0u - take_match0)) |
        ((uint64_t)entry1->value & (0u - take_match1)) |
        ((uint64_t)entry2->value & (0u - take_match2)) |
        ((uint64_t)entry3->value & (0u - take_match3)) |
        ((uint64_t)entry4->value & (0u - take_match4)) |
        ((uint64_t)entry5->value & (0u - take_match5)) |
        ((uint64_t)entry6->value & (0u - take_match6)) |
        ((uint64_t)entry7->value & (0u - take_match7));
    return window;
}

static KainMapSlotSearch kain_map_find_slot(
    KainMap* map,
    const char* key,
    size_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix
) {
    uint64_t start_index = key_hash & map->mask;
    uint64_t probe_offset;
    KainMapSlotSearch search;
    search.has_match = 0u;
    search.has_empty = 0u;
    search.match_index = 0u;
    search.empty_index = 0u;

    for (probe_offset = 0u; probe_offset < (uint64_t)map->capacity; probe_offset += 8u) {
        uint64_t base_index = (start_index + probe_offset) & map->mask;
        KainMapProbeWindow window = kain_map_probe_window(
            map,
            key,
            key_length,
            key_hash,
            key_prefix,
            base_index
        );
        uint64_t take_match = kain_map_zero_bit(search.has_match) & window.any_match;
        uint64_t take_empty = kain_map_zero_bit(search.has_empty) & window.any_empty;
        search.match_index = kain_map_select_u64(search.match_index, window.selected_match_index, take_match);
        search.empty_index = kain_map_select_u64(search.empty_index, window.selected_empty_index, take_empty);
        search.has_match |= window.any_match;
        search.has_empty |= window.any_empty;
    }

    return search;
}

static int kain_map_insert_prehashed(
    KainMap* map,
    char* key,
    size_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix,
    long long value,
    int key_state,
    int rebuild_tiny_dispatch
) {
    KainMapSlotSearch slot = kain_map_find_slot(map, key, key_length, key_hash, key_prefix);

    if (slot.has_match) {
        MapEntry* entry = &map->entries[slot.match_index];
        /* Proof: runtime/native/src/core/z3/proofs/native-map-static-key-state-guard.yaml */
        if (entry->occupied == KAIN_MAP_ENTRY_OWNED_KEY && key_state == KAIN_MAP_ENTRY_STATIC_KEY) {
            rc_release(entry->key);
            entry->key = key;
            entry->occupied = KAIN_MAP_ENTRY_STATIC_KEY;
        }
        entry->value = value;
        return 1;
    }
    if (!slot.has_empty) {
        return 0;
    }
    if (key_state == KAIN_MAP_ENTRY_OWNED_KEY) {
        rc_retain(key);
    }
    map->entries[slot.empty_index].key = key;
    map->entries[slot.empty_index].hash = key_hash;
    map->entries[slot.empty_index].key_prefix = key_prefix;
    map->entries[slot.empty_index].key_length = key_length;
    map->entries[slot.empty_index].value = value;
    map->entries[slot.empty_index].occupied = key_state;
    map->count += 1;
    if (rebuild_tiny_dispatch) {
        kain_map_rebuild_tiny_dispatch(map);
    }
    return 1;
}

static int kain_map_resize(KainMap* map, uint64_t new_capacity) {
    MapEntry* old_entries = map->entries;
    long long old_capacity = map->capacity;
    MapEntry* new_entries;
    long long i;

    new_entries = (MapEntry*)calloc((size_t)new_capacity, sizeof(MapEntry));
    if (!new_entries) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Map resize failed",
            "Failed to allocate resized map entries buffer"
        );
        return 0;
    }

    map->entries = new_entries;
    map->capacity = (long long)new_capacity;
    map->count = 0;
    map->mask = new_capacity - 1u;

    for (i = 0; i < old_capacity; ++i) {
        MapEntry* old_entry = &old_entries[i];
        if (old_entry->occupied) {
            if (!kain_map_insert_prehashed(
                map,
                old_entry->key,
                old_entry->key_length,
                old_entry->hash,
                old_entry->key_prefix,
                old_entry->value,
                old_entry->occupied,
                0
            )) {
                emit_diagnostic(
                    KAIN_DIAG_SUBSYSTEM_MEMORY,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                    "Map resize failed",
                    "Resized map could not place a moved entry"
                );
                free(old_entries);
                return 0;
            }
        }
    }

    free(old_entries);
    kain_map_rebuild_tiny_dispatch(map);
    return 1;
}

void map_free_elems(void* ptr) {
    KainMap* map = (KainMap*)ptr;
    long long i;
    for (i = 0; i < map->capacity; ++i) {
        if (map->entries[i].occupied == KAIN_MAP_ENTRY_OWNED_KEY) {
            rc_release(map->entries[i].key);
        }
    }
    free(map->entries);
}

KainMap* map_new() {
    KainMap* map = (KainMap*)kain_alloc_rc(sizeof(KainMap), 3);
    if (!map) return NULL;
    map->capacity = 16;
    map->count = 0;
    map->mask = 15u;
    map->entries = (MapEntry*)calloc(16, sizeof(MapEntry));
    kain_map_disable_tiny_dispatch(map);
    if (!map->entries) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Map allocation failed",
            "Failed to allocate map entries buffer"
        );
        /* Note: map itself will be cleaned up by RC when released */
        return NULL;
    }
    return map;
}

void map_set(KainMap* map, char* key, long long value) {
    size_t key_length;
    uint64_t key_hash;
    uint64_t key_prefix;

    if (!map || !key) return;

    kain_map_key_metadata(key, &key_length, &key_hash, &key_prefix);

    /* Proof: runtime/native/src/core/z3/proofs/native-map-entry-allocation-does-not-wrap-after-capacity-guard.yaml */
    if ((uint64_t)map->count >= kain_map_growth_threshold((uint64_t)map->capacity)) {
        uint64_t current_capacity = (uint64_t)map->capacity;
        uint64_t new_capacity = current_capacity << 1u;

        if (new_capacity <= current_capacity || new_capacity > (SIZE_MAX / sizeof(MapEntry))) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_MEMORY,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Map resize failed",
                "Map capacity overflowed while doubling"
            );
            return;
        }
        if (!kain_map_resize(map, new_capacity)) {
            return;
        }
    }

    if (!kain_map_insert_prehashed(
        map,
        key,
        key_length,
        key_hash,
        key_prefix,
        value,
        KAIN_MAP_ENTRY_OWNED_KEY,
        1
    )) {
        uint64_t current_capacity = (uint64_t)map->capacity;
        uint64_t new_capacity = current_capacity << 1u;
        if (new_capacity <= current_capacity || new_capacity > (SIZE_MAX / sizeof(MapEntry))) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_MEMORY,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Map insert failed",
                "Map capacity overflowed while recovering from a saturated probe"
            );
            return;
        }
        if (!kain_map_resize(map, new_capacity)) {
            return;
        }
        (void)kain_map_insert_prehashed(
            map,
            key,
            key_length,
            key_hash,
            key_prefix,
            value,
            KAIN_MAP_ENTRY_OWNED_KEY,
            1
        );
    }
}

void map_set_static(KainMap* map, char* key, long long value) {
    size_t key_length;
    uint64_t key_hash;
    uint64_t key_prefix;

    if (!map || !key) return;

    kain_map_key_metadata(key, &key_length, &key_hash, &key_prefix);
    map_set_static_prehashed(map, key, (uint64_t)key_length, key_hash, key_prefix, value);
}

void map_set_static_prehashed(
    KainMap* map,
    char* key,
    uint64_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix,
    long long value
) {
    if (!map || !key) return;
    if (key_length > (uint64_t)SIZE_MAX) return;

    if ((uint64_t)map->count >= kain_map_growth_threshold((uint64_t)map->capacity)) {
        uint64_t current_capacity = (uint64_t)map->capacity;
        uint64_t new_capacity = current_capacity << 1u;

        if (new_capacity <= current_capacity || new_capacity > (SIZE_MAX / sizeof(MapEntry))) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_MEMORY,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Map resize failed",
                "Map capacity overflowed while doubling"
            );
            return;
        }
        if (!kain_map_resize(map, new_capacity)) {
            return;
        }
    }

    if (!kain_map_insert_prehashed(
        map,
        key,
        (size_t)key_length,
        key_hash,
        key_prefix,
        value,
        KAIN_MAP_ENTRY_STATIC_KEY,
        1
    )) {
        uint64_t current_capacity = (uint64_t)map->capacity;
        uint64_t new_capacity = current_capacity << 1u;
        if (new_capacity <= current_capacity || new_capacity > (SIZE_MAX / sizeof(MapEntry))) {
            emit_diagnostic(
                KAIN_DIAG_SUBSYSTEM_MEMORY,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Map insert failed",
                "Map capacity overflowed while recovering from a saturated probe"
            );
            return;
        }
        if (!kain_map_resize(map, new_capacity)) {
            return;
        }
        (void)kain_map_insert_prehashed(
            map,
            key,
            (size_t)key_length,
            key_hash,
            key_prefix,
            value,
            KAIN_MAP_ENTRY_STATIC_KEY,
            1
        );
    }
}

long long map_get(KainMap* map, char* key) {
    size_t key_length;
    uint64_t key_hash;
    uint64_t key_prefix;
    
    if (!map || !key) return 0;

    kain_map_key_metadata(key, &key_length, &key_hash, &key_prefix);
    return map_get_prehashed(map, key, (uint64_t)key_length, key_hash, key_prefix);
}

long long map_get_prehashed(
    KainMap* map,
    char* key,
    uint64_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix
) {
    long long tiny_value;
    int tiny_status;
    uint64_t start_index;
    uint64_t probe_offset;
    size_t exact_key_length;

    if (!map || !key) return 0;
    if (key_length > (uint64_t)SIZE_MAX) return 0;
    exact_key_length = (size_t)key_length;

    tiny_status = kain_map_tiny_get_prehashed(
        map,
        key,
        exact_key_length,
        key_hash,
        key_prefix,
        &tiny_value
    );
    if (tiny_status > 0) {
        return tiny_value;
    }
    if (tiny_status == 0) {
        return 0;
    }

    start_index = key_hash & map->mask;
    for (probe_offset = 0u; probe_offset < (uint64_t)map->capacity; ++probe_offset) {
        MapEntry* entry = &map->entries[(start_index + probe_offset) & map->mask];
        /* Proof: runtime/native/src/core/z3/proofs/native-map-linear-probe-empty-slot-precludes-later-match.yaml */
        if (!entry->occupied) {
            return 0;
        }
        if (kain_map_entry_matches_prehashed(entry, key, exact_key_length, key_hash, key_prefix)) {
            return entry->value;
        }
    }
    return 0;
}

void* mq_new() {
    MessageQueue* mq = (MessageQueue*)malloc(sizeof(MessageQueue));
    if (!mq) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_ACTOR,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Message queue allocation failed",
            "Failed to allocate message queue structure"
        );
        return NULL;
    }
    mq->head = NULL;
    mq->tail = NULL;
#ifdef _WIN32
    InitializeCriticalSection(&mq->lock);
#else
    pthread_mutex_init(&mq->lock, NULL);
#endif
    return mq;
}

void mq_push(void* mq_ptr, long long type_tag, void* data) {
    MessageQueue* mq = (MessageQueue*)mq_ptr;
    MessageNode* node = (MessageNode*)malloc(sizeof(MessageNode));
    if (!node) return;
    node->type_tag = type_tag;
    node->data = data;
    node->data_size = 0;
    node->sender_id = 0ULL;
    node->next = NULL;

#ifdef _WIN32
    EnterCriticalSection(&mq->lock);
#else
    pthread_mutex_lock(&mq->lock);
#endif

    if (mq->tail) {
        mq->tail->next = node;
        mq->tail = node;
    } else {
        mq->head = node;
        mq->tail = node;
    }

#ifdef _WIN32
    LeaveCriticalSection(&mq->lock);
#else
    pthread_mutex_unlock(&mq->lock);
#endif
}

int mq_pop(void* mq_ptr, long long* out_tag, void** out_data) {
    MessageQueue* mq = (MessageQueue*)mq_ptr;
    MessageNode* node;

#ifdef _WIN32
    EnterCriticalSection(&mq->lock);
#else
    pthread_mutex_lock(&mq->lock);
#endif

    if (!mq->head) {
#ifdef _WIN32
        LeaveCriticalSection(&mq->lock);
#else
        pthread_mutex_unlock(&mq->lock);
#endif
        return 0;
    }

    node = mq->head;
    mq->head = node->next;
    if (!mq->head) {
        mq->tail = NULL;
    }

#ifdef _WIN32
    LeaveCriticalSection(&mq->lock);
#else
    pthread_mutex_unlock(&mq->lock);
#endif

    *out_tag = node->type_tag;
    *out_data = node->data;
    free(node);
    return 1;
}

#ifdef _WIN32
static void init_winsock(void) {
    static int initialized = 0;
    if (!initialized) {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
        initialized = 1;
    }
}
#endif

static void kain_socket_close_native(SOCKET sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

long long socket_connect(char* host, long long port) {
    SOCKET sock = INVALID_SOCKET;
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    struct addrinfo* current;
    char port_text[32];

#ifdef _WIN32
    init_winsock();
#endif

    if (!host || !host[0]) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "Socket connect failed",
            "Host is NULL or empty"
        );
        return -1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(port_text, sizeof(port_text), "%lld", port);

    if (getaddrinfo(host, port_text, &hints, &result) != 0) {
        char detail[256];
        snprintf(detail, sizeof(detail), "getaddrinfo failed for host: %s, port: %lld", host, port);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "Socket connect failed",
            detail
        );
        return -1;
    }

    for (current = result; current; current = current->ai_next) {
        sock = socket((int)current->ai_family, (int)current->ai_socktype, (int)current->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        if (connect(sock, current->ai_addr, (int)current->ai_addrlen) == 0) {
            freeaddrinfo(result);
            return (long long)sock;
        }
        kain_socket_close_native(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    {
        char detail[256];
        snprintf(detail, sizeof(detail), "Failed to connect to host: %s, port: %lld", host, port);
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
            "Socket connect failed",
            detail
        );
    }

    return -1;
}

void socket_send(long long sock, char* data) {
    if (sock < 0 || !data) return;
    send((SOCKET)sock, data, (int)strlen(data), 0);
}

char* socket_recv(long long sock) {
    char buf[4096];
    int len;
    if (sock < 0) return NULL;
    len = recv((SOCKET)sock, buf, sizeof(buf) - 1, 0);
    if (len > 0) {
        buf[len] = 0;
        return string_new(buf);
    }
    return string_new("");
}

int deep_eq(void* a, void* b) {
    KainRcTrackedPointer tracked_a;
    KainRcTrackedPointer tracked_b;
    RcHeader* ha;
    RcHeader* hb;
    if (a == b) return 1;
    if (!a || !b) return 0;

    tracked_a = kain_rc_registry_lookup(a);
    tracked_b = kain_rc_registry_lookup(b);
    if (tracked_a.state != KAIN_RC_TRACK_LIVE ||
        tracked_b.state != KAIN_RC_TRACK_LIVE ||
        tracked_a.header == NULL ||
        tracked_b.header == NULL) {
        return 0;
    }
    ha = tracked_a.header;
    hb = tracked_b.header;
    if (ha->type_tag != hb->type_tag) return 0;

    if (ha->type_tag == 1) {
        size_t len_a = ha->string_length;
        size_t len_b = hb->string_length;
        return len_a == len_b && (len_a == 0u || memcmp((char*)a, (char*)b, len_a) == 0);
    }

    if (ha->type_tag == 2) {
        KainArray* arr_a = (KainArray*)a;
        KainArray* arr_b = (KainArray*)b;
        long long i;
        if (arr_a->len != arr_b->len) return 0;
        for (i = 0; i < arr_a->len; ++i) {
            if (arr_a->data[i] != arr_b->data[i]) return 0;
        }
        return 1;
    }

    return 0;
}

/* Public API to retrieve last diagnostic */
int runtime_get_last_diagnostic(KainDiagnostic* out) {
    if (!out || !g_last_diagnostic_valid) {
        return 0;
    }
    memcpy(out, &g_last_diagnostic, sizeof(KainDiagnostic));
    return 1;
}

void runtime_clear_last_diagnostic(void) {
    g_last_diagnostic_valid = 0;
}

/* Standard library thread wrappers */
typedef struct {
    void (*func)(void*);
    void* arg;
    int64_t* thread_id_out;
    int64_t* done_flag;
} ThreadSpawnCtx;

#ifdef _WIN32
static DWORD WINAPI win32_thread_entry(LPVOID param) {
    ThreadSpawnCtx* ctx = (ThreadSpawnCtx*)param;
    if (ctx->thread_id_out) {
        *ctx->thread_id_out = (int64_t)GetCurrentThreadId();
    }
    ctx->func(ctx->arg);
    if (ctx->done_flag) {
        *ctx->done_flag = 1;
    }
    free(ctx);
    return 0;
}
#else
static void* posix_thread_entry(void* param) {
    ThreadSpawnCtx* ctx = (ThreadSpawnCtx*)param;
    if (ctx->thread_id_out) {
        *ctx->thread_id_out = (int64_t)(uintptr_t)pthread_self();
    }
    ctx->func(ctx->arg);
    if (ctx->done_flag) {
        *ctx->done_flag = 1;
    }
    free(ctx);
    return NULL;
}
#endif

void* abi_thread_spawn(void (*func)(void*), void* arg, int64_t* thread_id_out, int64_t* done_flag) {
    ThreadSpawnCtx* ctx = (ThreadSpawnCtx*)malloc(sizeof(ThreadSpawnCtx));
    if (!ctx) return NULL;
    ctx->func = func;
    ctx->arg = arg;
    ctx->thread_id_out = thread_id_out;
    ctx->done_flag = done_flag;
    
#ifdef _WIN32
    HANDLE handle = CreateThread(NULL, 0, win32_thread_entry, ctx, 0, NULL);
    return (void*)handle;
#else
    pthread_t thread;
    int res = pthread_create(&thread, NULL, posix_thread_entry, ctx);
    if (res != 0) {
        free(ctx);
        return NULL;
    }
    pthread_t* th_ptr = (pthread_t*)malloc(sizeof(pthread_t));
    if (!th_ptr) {
        pthread_detach(thread);
        return NULL;
    }
    *th_ptr = thread;
    return (void*)th_ptr;
#endif
}

int64_t abi_thread_join(void* thread_handle) {
    if (!thread_handle) return -1;
#ifdef _WIN32
    HANDLE handle = (HANDLE)thread_handle;
    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
    return 0;
#else
    pthread_t* th_ptr = (pthread_t*)thread_handle;
    pthread_join(*th_ptr, NULL);
    free(th_ptr);
    return 0;
#endif
}

int64_t abi_thread_set_name(const char* name) {
    if (!name) return -1;
#ifdef _WIN32
    return 0;
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), name);
    return 0;
#elif defined(__APPLE__)
    pthread_setname_np(name);
    return 0;
#else
    return 0;
#endif
}

void* abi_fs_open(const char* path, const char* mode) {
    if (!path || !mode) return NULL;
    FILE* file = NULL;
#ifdef _WIN32
    fopen_s(&file, path, mode);
#else
    file = fopen(path, mode);
#endif
    return (void*)file;
}

int64_t abi_fs_close(void* handle) {
    if (!handle) return -1;
    return (int64_t)fclose((FILE*)handle);
}

int64_t abi_fs_read(void* handle, void* buffer, int64_t byte_count) {
    if (!handle || !buffer || byte_count <= 0) return 0;
    return (int64_t)fread(buffer, 1, (size_t)byte_count, (FILE*)handle);
}

int64_t abi_fs_write(void* handle, const void* buffer, int64_t byte_count) {
    if (!handle || !buffer || byte_count <= 0) return 0;
    return (int64_t)fwrite(buffer, 1, (size_t)byte_count, (FILE*)handle);
}

int64_t abi_fs_seek(void* handle, int64_t offset, int64_t origin) {
    if (!handle) return -1;
    return (int64_t)fseek((FILE*)handle, (long)offset, (int)origin);
}

int64_t abi_fs_tell(void* handle) {
    if (!handle) return -1;
    return (int64_t)ftell((FILE*)handle);
}

int64_t abi_fs_flush(void* handle) {
    if (!handle) return -1;
    return (int64_t)fflush((FILE*)handle);
}

