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
    usleep((useconds_t)(seconds * 1000000.0));
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

static unsigned long hash_str(char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (unsigned long)c;
    }
    return hash;
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
    unsigned long h;
    long long idx;
    if (!key) return;

    if (map->count >= map->capacity * 0.75) {
        long long old_cap = map->capacity;
        MapEntry* old_entries = map->entries;
        long long i;

        map->capacity *= 2;
        map->entries = (MapEntry*)calloc((size_t)map->capacity, sizeof(MapEntry));
        map->count = 0;

        for (i = 0; i < old_cap; ++i) {
            if (old_entries[i].occupied) {
                map_set(map, old_entries[i].key, old_entries[i].value);
            }
        }
        free(old_entries);
    }

    h = hash_str(key);
    idx = (long long)(h % (unsigned long)map->capacity);

    while (map->entries[idx].occupied) {
        if (strcmp(map->entries[idx].key, key) == 0) {
            map->entries[idx].value = value;
            return;
        }
        idx = (idx + 1) % map->capacity;
    }

    rc_retain(key);
    map->entries[idx].key = key;
    map->entries[idx].value = value;
    map->entries[idx].occupied = 1;
    map->count++;
}

long long map_get(KainMap* map, char* key) {
    unsigned long h;
    long long idx;
    long long i;
    if (!key) return 0;

    h = hash_str(key);
    idx = (long long)(h % (unsigned long)map->capacity);

    for (i = 0; i < map->capacity; ++i) {
        if (!map->entries[idx].occupied) return 0;
        if (strcmp(map->entries[idx].key, key) == 0) {
            return map->entries[idx].value;
        }
        idx = (idx + 1) % map->capacity;
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
