#ifndef RUNTIME_BASE_H
#define RUNTIME_BASE_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <ws2tcpip.h>
#include <gl/GL.h>
#else
#include <errno.h>
#include <limits.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <strings.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef unsigned int GLuint;
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef _WIN32
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif

static inline int kain_fopen_s(FILE** file, const char* path, const char* mode) {
    if (!file) {
        return EINVAL;
    }
    *file = fopen(path, mode);
    if (*file) {
        return 0;
    }
    return errno ? errno : 1;
}

static inline int kain_dupenv_s(char** buffer, size_t* length, const char* name) {
    const char* value;
    size_t value_length;

    if (buffer) {
        *buffer = NULL;
    }
    if (length) {
        *length = 0u;
    }
    if (!buffer || !name || !name[0]) {
        return EINVAL;
    }

    value = getenv(name);
    if (!value || !value[0]) {
        return 0;
    }

    value_length = strlen(value);
    *buffer = (char*)malloc(value_length + 1u);
    if (!*buffer) {
        return ENOMEM;
    }

    memcpy(*buffer, value, value_length + 1u);
    if (length) {
        *length = value_length + 1u;
    }
    return 0;
}

static inline int kain_putenv_s(const char* name, const char* value) {
    if (!name || !name[0]) {
        return EINVAL;
    }
    if (setenv(name, value ? value : "", 1) == 0) {
        return 0;
    }
    return errno ? errno : 1;
}

static inline void kain_sleep_millis(unsigned int milliseconds) {
    struct timespec delay;

    delay.tv_sec = (time_t)(milliseconds / 1000u);
    delay.tv_nsec = (long)((milliseconds % 1000u) * 1000000u);
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

static inline int kain_strncpy_s(
    char* destination,
    size_t destination_capacity,
    const char* source,
    size_t count
) {
    size_t source_length;
    size_t copy_length;

    if (!destination || destination_capacity == 0u) {
        return EINVAL;
    }
    if (!source) {
        destination[0] = '\0';
        return EINVAL;
    }

    source_length = strlen(source);
    copy_length = source_length;
    if (count != _TRUNCATE && count < copy_length) {
        copy_length = count;
    }
    if (copy_length >= destination_capacity) {
        copy_length = destination_capacity - 1u;
    }

    memcpy(destination, source, copy_length);
    destination[copy_length] = '\0';
    return 0;
}

static inline int kain_strncat_s(
    char* destination,
    size_t destination_capacity,
    const char* source,
    size_t count
) {
    size_t destination_length;

    if (!destination || destination_capacity == 0u) {
        return EINVAL;
    }

    destination_length = strlen(destination);
    if (destination_length >= destination_capacity) {
        return ERANGE;
    }

    return kain_strncpy_s(
        destination + destination_length,
        destination_capacity - destination_length,
        source,
        count
    );
}

#define fopen_s(file, path, mode) kain_fopen_s((file), (path), (mode))
#define _dupenv_s(buffer, length, name) kain_dupenv_s((buffer), (length), (name))
#define _putenv_s(name, value) kain_putenv_s((name), (value))
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _strtoui64 strtoull
#ifndef Sleep
#define Sleep(milliseconds) kain_sleep_millis((unsigned int)(milliseconds))
#endif
#define strncpy_s(destination, capacity, source, count) \
    kain_strncpy_s((destination), (capacity), (source), (count))
#define strncat_s(destination, capacity, source, count) \
    kain_strncat_s((destination), (capacity), (source), (count))
#endif

typedef struct {
    long long ref_count;
    long long weak_count;
    long long type_tag;
    size_t payload_size;
    size_t string_length;
    void (*destructor)(void*);
} RcHeader;

static inline size_t kain_bounded_text_length(const char* text, size_t max_length) {
    const void* terminator;
    if (!text) {
        return 0u;
    }
    terminator = memchr(text, '\0', max_length);
    if (!terminator) {
        return max_length;
    }
    return (size_t)((const char*)terminator - text);
}

static inline void kain_rc_set_string_length(void* ptr, size_t length) {
    if (ptr) {
        RcHeader* header = ((RcHeader*)ptr) - 1;
        header->string_length = length;
    }
}

static inline size_t kain_rc_string_length(const void* ptr) {
    if (!ptr) {
        return 0u;
    }
    return (((const RcHeader*)ptr) - 1)->string_length;
}

typedef struct {
    void (*func)(void*);
    void* arg;
} ThreadArgs;

typedef struct {
    long long* data;
    long long len;
    long long cap;
} KainArray;

typedef struct {
    char* key;
    uint64_t hash;
    uint64_t key_prefix;
    size_t key_length;
    long long value;
    int occupied;
} MapEntry;

enum {
    KAIN_MAP_ENTRY_EMPTY = 0,
    KAIN_MAP_ENTRY_OWNED_KEY = 1,
    KAIN_MAP_ENTRY_STATIC_KEY = 2,
    KAIN_MAP_TINY_MAX_COUNT = 24,
    KAIN_MAP_TINY_DISPATCH_SIZE = 64,
    KAIN_MAP_TINY_EMPTY_INDEX = 255,
};

typedef struct {
    MapEntry* entries;
    long long capacity;
    long long count;
    uint64_t mask;
    uint64_t tiny_magic;
    uint8_t tiny_ready;
    uint8_t tiny_dispatch[KAIN_MAP_TINY_DISPATCH_SIZE];
} KainMap;

typedef struct MessageNode {
    long long type_tag;
    void* data;
    size_t data_size;
    unsigned long long sender_id;
    struct MessageNode* next;
} MessageNode;

typedef struct {
    MessageNode* head;
    MessageNode* tail;
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
} MessageQueue;

double kain_clampd(double value, double min_value, double max_value);
long long kain_floor_i64(double value);
long long kain_ceil_i64(double value);
long long kain_round_i64(double value);
char* string_new(char* src);
void map_set_static(KainMap* map, char* key, long long value);
void map_set_static_prehashed(
    KainMap* map,
    char* key,
    uint64_t key_length,
    uint64_t key_hash,
    uint64_t key_prefix,
    long long value
);
long long map_get_prehashed(KainMap* map, char* key, uint64_t key_length, uint64_t key_hash, uint64_t key_prefix);
long long find_substring_from_known_lengths(
    char* s,
    long long s_len,
    char* needle,
    long long needle_len,
    long long start
);
int deep_eq(void* a, void* b);
void rc_retain(void* ptr);
void rc_weak_retain(void* ptr);
void rc_release(void* ptr);
void rc_weak_release(void* ptr);

#endif
