#include "../../include/kain_runtime_base.h"
#include "../../include/kain_runtime_diagnostics.h"
#include <ctype.h>

static RcHeader* get_header(void* ptr) {
    return ((RcHeader*)ptr) - 1;
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

void* kain_alloc_rc(size_t size, long long type_tag) {
    RcHeader* header = malloc(sizeof(RcHeader) + size);
    if (!header) {
        emit_diagnostic(
            KAIN_DIAG_SUBSYSTEM_MEMORY,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Memory allocation failed",
            "Failed to allocate memory with reference counting header"
        );
        return NULL;
    }
    header->ref_count = 1;
    header->weak_count = 0;
    header->type_tag = type_tag;
    header->destructor = NULL;
    return (void*)(header + 1);
}

void* kain_alloc(size_t size) {
    return kain_alloc_rc(size, 0);
}

void* KAIN_alloc(long long size) {
    return kain_alloc((size_t)size);
}

void rc_retain(void* ptr) {
    if (!ptr) return;
    get_header(ptr)->ref_count++;
}

void rc_weak_retain(void* ptr) {
    if (!ptr) return;
    get_header(ptr)->weak_count++;
}

void rc_release(void* ptr) {
    RcHeader* header;
    if (!ptr) return;
    header = get_header(ptr);
    header->ref_count--;

    if (header->ref_count == 0) {
        if (header->type_tag == 2) {
            array_free_elems(ptr);
        } else if (header->type_tag == 3) {
            map_free_elems(ptr);
        }

        if (header->destructor) {
            header->destructor(ptr);
        }

        if (header->weak_count == 0) {
            free(header);
        }
    }
}

void rc_weak_release(void* ptr) {
    RcHeader* header;
    if (!ptr) return;
    header = get_header(ptr);
    header->weak_count--;

    if (header->weak_count == 0 && header->ref_count == 0) {
        free(header);
    }
}

void* weak_upgrade(void* ptr) {
    RcHeader* header;
    if (!ptr) return NULL;
    header = get_header(ptr);
    if (header->ref_count > 0) {
        header->ref_count++;
        return ptr;
    }
    return NULL;
}

void kain_set_destructor(void* ptr, void (*dtor)(void*)) {
    if (!ptr) return;
    get_header(ptr)->destructor = dtor;
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
#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000.0));
#else
    struct timespec delay;

    if (seconds <= 0.0) {
        return;
    }

    delay.tv_sec = (time_t)seconds;
    delay.tv_nsec = (long)((seconds - (double)delay.tv_sec) * 1000000000.0);
    if (delay.tv_nsec < 0) {
        delay.tv_nsec = 0;
    } else if (delay.tv_nsec >= 1000000000L) {
        delay.tv_sec += 1;
        delay.tv_nsec -= 1000000000L;
    }

    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
#endif
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

char* to_string(long long n) {
    char* buf = (char*)kain_alloc_rc(32, 1);
    if (!buf) return NULL;
    sprintf(buf, "%lld", n);
    return buf;
}

char* str_concat(char* s1, char* s2) {
    size_t l1 = s1 ? strlen(s1) : 0;
    size_t l2 = s2 ? strlen(s2) : 0;
    char* res;
    if (!s1 && !s2) return NULL;
    res = (char*)kain_alloc_rc(l1 + l2 + 1, 1);
    if (!res) return NULL;
    if (s1) {
        memcpy(res, s1, l1);
    } else {
        l1 = 0;
    }
    if (s2) {
        memcpy(res + l1, s2, l2);
    }
    res[l1 + l2] = 0;
    return res;
}

long long clock_wrapper() {
    return (long long)clock();
}

void array_free_elems(void* ptr) {
    KainArray* arr = (KainArray*)ptr;
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
    arr->data[index] = val;
}

long long array_len(KainArray* arr) {
    return arr->len;
}

long long pop(void* arr_ptr) {
    KainArray* arr = (KainArray*)arr_ptr;
    if (!arr || arr->len <= 0) return 0;
    arr->len -= 1;
    return arr->data[arr->len];
}

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
    len = strlen(s);
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
    len = strlen(s);
    out = (char*)kain_alloc_rc(len + 1, 1);
    if (!out) return NULL;
    for (i = 0; i < len; ++i) {
        out[i] = (char)tolower((unsigned char)s[i]);
    }
    out[len] = '\0';
    return out;
}

int contains(char* s, char* sub) {
    if (!s || !sub) return 0;
    return strstr(s, sub) != NULL;
}

int starts_with(char* s, char* prefix) {
    size_t prefix_len;
    if (!s || !prefix) return 0;
    prefix_len = strlen(prefix);
    return strncmp(s, prefix, prefix_len) == 0;
}

int ends_with(char* s, char* suffix) {
    size_t s_len;
    size_t suffix_len;
    if (!s || !suffix) return 0;
    s_len = strlen(s);
    suffix_len = strlen(suffix);
    if (suffix_len > s_len) return 0;
    return strncmp(s + (s_len - suffix_len), suffix, suffix_len) == 0;
}

char* char_at(char* s, long long index) {
    size_t s_len;
    char ch;
    if (!s || index < 0) {
        return string_new("");
    }
    s_len = strlen(s);
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
    s_len = strlen(s);
    if (start < 0) start = 0;
    if (end < 0 || (size_t)end > s_len) end = (long long)s_len;
    if ((size_t)start > s_len) start = (long long)s_len;
    if (end < start) end = start;
    slice_start = (size_t)start;
    slice_end = (size_t)end;
    return kain_string_new_with_len(s + slice_start, slice_end - slice_start);
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
    RcHeader* header;
    if (!value) return 0;
    header = get_header(value);
    if (header->type_tag == 1) {
        return (long long)strlen((char*)value);
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

static uint64_t kain_rotate_left_u64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64u - shift));
}

static uint64_t kain_map_nonzero_bit(uint64_t value) {
    return (value | (0u - value)) >> 63u;
}

static uint64_t kain_map_zero_bit(uint64_t value) {
    return kain_map_nonzero_bit(value) ^ 1u;
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
    uint64_t exact_match = metadata_match ? (uint64_t)(memcmp(entry->key, key, key_length) == 0) : 0u;
    return metadata_match & exact_match;
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
    int retain_key
) {
    KainMapSlotSearch slot = kain_map_find_slot(map, key, key_length, key_hash, key_prefix);

    if (slot.has_match) {
        map->entries[slot.match_index].value = value;
        return 1;
    }
    if (!slot.has_empty) {
        return 0;
    }
    if (retain_key) {
        rc_retain(key);
    }
    map->entries[slot.empty_index].key = key;
    map->entries[slot.empty_index].hash = key_hash;
    map->entries[slot.empty_index].key_prefix = key_prefix;
    map->entries[slot.empty_index].key_length = key_length;
    map->entries[slot.empty_index].value = value;
    map->entries[slot.empty_index].occupied = 1;
    map->count += 1;
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
    return 1;
}

void map_free_elems(void* ptr) {
    KainMap* map = (KainMap*)ptr;
    long long i;
    for (i = 0; i < map->capacity; ++i) {
        if (map->entries[i].occupied) {
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

    if (!kain_map_insert_prehashed(map, key, key_length, key_hash, key_prefix, value, 1)) {
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
        (void)kain_map_insert_prehashed(map, key, key_length, key_hash, key_prefix, value, 1);
    }
}

long long map_get(KainMap* map, char* key) {
    size_t key_length;
    uint64_t key_hash;
    uint64_t key_prefix;
    uint64_t start_index;
    uint64_t probe_offset;
    uint64_t found = 0u;
    uint64_t selected_value = 0u;

    if (!map || !key) return 0;

    kain_map_key_metadata(key, &key_length, &key_hash, &key_prefix);
    start_index = key_hash & map->mask;

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
        uint64_t take_value = kain_map_zero_bit(found) & window.any_match;
        selected_value = kain_map_select_u64(selected_value, window.selected_value, take_value);
        found |= window.any_match;
    }
    return (long long)(selected_value * found);
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
    RcHeader* ha;
    RcHeader* hb;
    if (a == b) return 1;
    if (!a || !b) return 0;

    ha = get_header(a);
    hb = get_header(b);
    if (ha->type_tag != hb->type_tag) return 0;

    if (ha->type_tag == 1) {
        return strcmp((char*)a, (char*)b) == 0;
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
int kain_runtime_get_last_diagnostic(KainDiagnostic* out) {
    if (!out || !g_last_diagnostic_valid) {
        return 0;
    }
    memcpy(out, &g_last_diagnostic, sizeof(KainDiagnostic));
    return 1;
}

void kain_runtime_clear_last_diagnostic(void) {
    g_last_diagnostic_valid = 0;
}
