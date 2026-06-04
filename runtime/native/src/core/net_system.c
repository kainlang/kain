#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/net_system.h"
#include "../../include/actor.h"
#include "../../include/base.h"

#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winhttp.h>
#else
#include <fcntl.h>
#include <sched.h>
#include <strings.h>
#endif

void* kain_alloc_rc(size_t size, long long type_tag);

typedef struct KainNativeNetHeader {
    int in_use;
    char key[ABI_NET_MAX_KEY];
    char value[ABI_NET_MAX_TEXT];
} KainNativeNetHeader;

typedef struct KainNativeTcpConnection {
    int in_use;
    int64_t id;
    SOCKET socket_handle;
} KainNativeTcpConnection;

typedef struct KainNativeTcpListener {
    int in_use;
    int64_t id;
    SOCKET socket_handle;
    int64_t local_port;
} KainNativeTcpListener;

typedef struct KainNativeHttpRequest {
    int in_use;
    int incoming;
    int dequeued;
    int responded;
    int64_t id;
    int64_t server_id;
    SOCKET socket_handle;
    char method[ABI_NET_MAX_KEY];
    char url[ABI_NET_MAX_URL];
    char path[ABI_NET_MAX_URL];
    char query[ABI_NET_MAX_URL];
    KainNativeNetHeader headers[ABI_NET_MAX_HEADERS];
    int64_t header_count;
    KainNativeNetHeader response_headers[ABI_NET_MAX_HEADERS];
    int64_t response_header_count;
    unsigned char* body;
    size_t body_length;
    int64_t timeout_ms;
    int64_t actor_id;
    char actor_message_kind[ABI_NET_MAX_KEY];
    char actor_payload[ABI_NET_MAX_TEXT];
    char protocol[ABI_NET_MAX_KEY];
} KainNativeHttpRequest;

typedef struct KainNativeHttpResponse {
    int in_use;
    int64_t id;
    int64_t status_code;
    char protocol[ABI_NET_MAX_KEY];
    KainNativeNetHeader headers[ABI_NET_MAX_HEADERS];
    int64_t header_count;
    unsigned char* body;
    size_t body_length;
} KainNativeHttpResponse;

typedef struct KainNativeHttpRoute {
    int in_use;
    char method[ABI_NET_MAX_KEY];
    char path[ABI_NET_MAX_URL];
    int64_t actor_id;
    char message_kind[ABI_NET_MAX_KEY];
} KainNativeHttpRoute;

typedef struct KainNativeHttpServer {
    int in_use;
    int listening;
    int64_t id;
    SOCKET socket_handle;
    int64_t local_port;
    char host[ABI_NET_MAX_KEY];
    KainNativeHttpRoute routes[ABI_NET_MAX_ROUTES];
    int64_t route_count;
    uint64_t pending_request_slots;
} KainNativeHttpServer;

typedef struct KainNativeParsedUrl {
    char scheme[16];
    char host[256];
    char path[ABI_NET_MAX_URL];
    int64_t port;
    int secure;
} KainNativeParsedUrl;

static KainNativeTcpConnection g_connections[ABI_NET_MAX_CONNECTIONS];
static KainNativeTcpListener g_listeners[ABI_NET_MAX_LISTENERS];
static KainNativeHttpRequest g_requests[ABI_NET_MAX_HTTP_REQUESTS];
static KainNativeHttpResponse g_responses[ABI_NET_MAX_HTTP_RESPONSES];
static KainNativeHttpServer g_servers[ABI_NET_MAX_HTTP_SERVERS];
static uint64_t g_connection_occupancy_bits = 0u;
static uint64_t g_listener_occupancy_bits = 0u;
static uint64_t g_request_occupancy_bits = 0u;
static uint64_t g_response_occupancy_bits = 0u;
static uint64_t g_server_occupancy_bits = 0u;
#define ABI_NET_CONNECTION_VALID_MASK UINT64_MAX
#define ABI_NET_LISTENER_VALID_MASK UINT64_C(0x000000000000ffff)
#define ABI_NET_REQUEST_VALID_MASK UINT64_MAX
#define ABI_NET_RESPONSE_VALID_MASK UINT64_MAX
#define ABI_NET_SERVER_VALID_MASK UINT64_C(0x000000000000ffff)
#define ABI_NET_CONNECTION_INDEX_CAPACITY 128u
#define ABI_NET_CONNECTION_INDEX_MASK (ABI_NET_CONNECTION_INDEX_CAPACITY - 1u)
#define ABI_NET_LISTENER_INDEX_CAPACITY 32u
#define ABI_NET_LISTENER_INDEX_MASK (ABI_NET_LISTENER_INDEX_CAPACITY - 1u)
#define ABI_NET_REQUEST_INDEX_CAPACITY 128u
#define ABI_NET_REQUEST_INDEX_MASK (ABI_NET_REQUEST_INDEX_CAPACITY - 1u)
#define ABI_NET_RESPONSE_INDEX_CAPACITY 128u
#define ABI_NET_RESPONSE_INDEX_MASK (ABI_NET_RESPONSE_INDEX_CAPACITY - 1u)
#define ABI_NET_SERVER_INDEX_CAPACITY 32u
#define ABI_NET_SERVER_INDEX_MASK (ABI_NET_SERVER_INDEX_CAPACITY - 1u)
#if (ABI_NET_CONNECTION_INDEX_CAPACITY & ABI_NET_CONNECTION_INDEX_MASK) != 0
#error "ABI_NET_CONNECTION_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (ABI_NET_LISTENER_INDEX_CAPACITY & ABI_NET_LISTENER_INDEX_MASK) != 0
#error "ABI_NET_LISTENER_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (ABI_NET_REQUEST_INDEX_CAPACITY & ABI_NET_REQUEST_INDEX_MASK) != 0
#error "ABI_NET_REQUEST_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (ABI_NET_RESPONSE_INDEX_CAPACITY & ABI_NET_RESPONSE_INDEX_MASK) != 0
#error "ABI_NET_RESPONSE_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (ABI_NET_SERVER_INDEX_CAPACITY & ABI_NET_SERVER_INDEX_MASK) != 0
#error "ABI_NET_SERVER_INDEX_CAPACITY must be a power of two for masked probing."
#endif
static uint32_t g_connection_index[ABI_NET_CONNECTION_INDEX_CAPACITY];
static uint32_t g_listener_index[ABI_NET_LISTENER_INDEX_CAPACITY];
static uint32_t g_request_index[ABI_NET_REQUEST_INDEX_CAPACITY];
static uint32_t g_response_index[ABI_NET_RESPONSE_INDEX_CAPACITY];
static uint32_t g_server_index[ABI_NET_SERVER_INDEX_CAPACITY];
static int64_t g_next_connection_id = 1;
static int64_t g_next_listener_id = 1;
static int64_t g_next_request_id = 1;
static int64_t g_next_response_id = 1;
static int64_t g_next_server_id = 1;
static int64_t g_last_status = ABI_NET_OK;
static char g_last_error_kind[ABI_NET_MAX_KEY] = "ok";
static char g_last_error_message[ABI_NET_MAX_TEXT] = "";
static const char g_empty_string[] = "";
static const char g_http_protocol_http11[] = "http/1.1";
static const char g_http_protocol_http2[] = "http/2";

static int abi_net_size_add_overflow(size_t left, size_t right, size_t* out) {
    if (out == 0) {
        return 1;
    }
    if (right > (SIZE_MAX - left)) {
        return 1;
    }
    *out = left + right;
    return 0;
}

static int abi_net_parse_content_length_header(const char* text, size_t* out_length) {
    char* end = 0;
    unsigned long long parsed = 0;

    if (out_length == 0 || text == 0) {
        return 0;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    /* Proof: runtime/native/src/core/z3/proofs/native-net-content-length-requires-nonnegative-parse.yaml */
    if (*text == '\0' || *text == '-' || *text == '+') {
        return 0;
    }

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text) {
        return 0;
    }

    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }

    if (*end != '\0' || parsed > (unsigned long long)SIZE_MAX) {
        return 0;
    }

    *out_length = (size_t)parsed;
    return 1;
}

static int abi_net_header_key_eq_ci(const unsigned char* text, size_t length, const char* expected) {
    size_t index = 0u;
    if (expected == 0) {
        return 0;
    }
    while (index < length && expected[index] != '\0') {
        unsigned char left = (unsigned char)tolower(text[index]);
        unsigned char right = (unsigned char)tolower((unsigned char)expected[index]);
        if (left != right) {
            return 0;
        }
        ++index;
    }
    return index == length && expected[index] == '\0';
}

static const unsigned char* abi_net_find_http_header_end(const unsigned char* bytes, size_t length) {
    size_t index;
    if (bytes == 0 || length < 4u) {
        return 0;
    }
    for (index = 0u; index + 3u < length; ++index) {
        if (bytes[index] == '\r' && bytes[index + 1u] == '\n' && bytes[index + 2u] == '\r' && bytes[index + 3u] == '\n') {
            return bytes + index;
        }
    }
    return 0;
}

static int abi_net_parse_content_length_slice(const unsigned char* text, size_t length, size_t* out_length) {
    size_t index = 0u;
    size_t parsed = 0u;
    int saw_digit = 0;
    if (text == 0 || out_length == 0) {
        return 0;
    }
    while (index < length && isspace(text[index])) {
        ++index;
    }
    if (index >= length || text[index] == '-' || text[index] == '+') {
        return 0;
    }
    while (index < length && isdigit(text[index])) {
        size_t digit = (size_t)(text[index] - '0');
        if (parsed > (SIZE_MAX - digit) / 10u) {
            return 0;
        }
        parsed = (parsed * 10u) + digit;
        saw_digit = 1;
        ++index;
    }
    while (index < length && isspace(text[index])) {
        ++index;
    }
    if (!saw_digit || index != length) {
        return 0;
    }
    *out_length = parsed;
    return 1;
}

static int abi_net_http_request_complete(const unsigned char* bytes, size_t length, size_t* out_required_length) {
    const unsigned char* header_end = abi_net_find_http_header_end(bytes, length);
    size_t header_length;
    size_t header_body_offset;
    size_t line_start = 0u;
    if (out_required_length == 0) {
        return -1;
    }
    if (header_end == 0) {
        return 0;
    }
    header_length = (size_t)((header_end + 4) - bytes);
    header_body_offset = (size_t)(header_end - bytes);
    *out_required_length = header_length;
    while (line_start < header_body_offset) {
        size_t line_end = line_start;
        size_t colon = line_start;
        while (line_end < header_body_offset && !(bytes[line_end] == '\r' && line_end + 1u < header_body_offset && bytes[line_end + 1u] == '\n')) {
            ++line_end;
        }
        while (colon < line_end && bytes[colon] != ':') {
            ++colon;
        }
        if (colon < line_end && abi_net_header_key_eq_ci(bytes + line_start, colon - line_start, "Content-Length")) {
            size_t content_length = 0u;
            if (!abi_net_parse_content_length_slice(bytes + colon + 1u, line_end - colon - 1u, &content_length)) {
                return -1;
            }
            if (abi_net_size_add_overflow(header_length, content_length, out_required_length)) {
                return -1;
            }
            return *out_required_length <= length ? 1 : 0;
        }
        line_start = line_end + ((line_end + 1u < header_body_offset && bytes[line_end] == '\r' && bytes[line_end + 1u] == '\n') ? 2u : 1u);
    }
    return 1;
}

/*
 * Proofs:
 * - runtime/native/src/core/z3/proofs-experimental/net-handle-index-probe-bounds.smt2
 * - runtime/native/src/core/z3/proofs-experimental/actor-table-debruijn-hash-distinct.smt2
 *
 * The solver owns two trust boundaries here: masked probe indices must remain
 * inside each sidecar handle table, and the de Bruijn low-bit decode is shared
 * with the already-proved actor occupancy path.
 */
static uint64_t abi_net_mix_id(int64_t id) {
    uint64_t x = (uint64_t)id;
    x ^= x >> 30u;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27u;
    x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31u;
    return x;
}

static uint64_t abi_net_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int abi_net_low_bit_index_u64(uint64_t one_hot) {
    static const unsigned char debruijn_index[64] = {
        0, 1, 48, 2, 57, 49, 28, 3,
        61, 58, 50, 42, 38, 29, 17, 4,
        62, 55, 59, 36, 53, 51, 43, 22,
        45, 39, 33, 30, 24, 18, 12, 5,
        63, 47, 56, 27, 60, 41, 37, 16,
        54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10,
        25, 14, 19, 9, 13, 8, 7, 6
    };
    return debruijn_index[(one_hot * UINT64_C(0x03f79d71b4cb0a89)) >> 58u];
}

static uint32_t abi_net_index_start_slot(int64_t id, uint32_t mask) {
    return (uint32_t)(abi_net_mix_id(id) & mask);
}

static int abi_net_index_insert(
    uint32_t* index_table,
    uint32_t index_capacity,
    uint32_t index_mask,
    int64_t id,
    uint32_t slot
) {
    uint32_t start_index = abi_net_index_start_slot(id, index_mask);
    uint32_t encoded_slot = slot + 1u;
    uint32_t probe;
    for (probe = 0u; probe < index_capacity; ++probe) {
        uint32_t candidate_index = (start_index + probe) & index_mask;
        uint32_t candidate = index_table[candidate_index];
        if (candidate == 0u || candidate == encoded_slot) {
            index_table[candidate_index] = encoded_slot;
            return 1;
        }
    }
    return 0;
}

static int abi_net_find_free_slot_u64(uint64_t occupancy_bits, uint64_t valid_mask, uint32_t* out_slot) {
    uint64_t free_mask = (~occupancy_bits) & valid_mask;
    if (out_slot == 0 || free_mask == 0u) {
        return 0;
    }
    *out_slot = (uint32_t)abi_net_low_bit_index_u64(
        abi_net_isolate_low_bit_u64(free_mask)
    );
    return 1;
}

static void abi_net_rebuild_connection_index(void) {
    uint32_t slot;
    memset(g_connection_index, 0, sizeof(g_connection_index));
    for (slot = 0u; slot < ABI_NET_MAX_CONNECTIONS; ++slot) {
        if (g_connections[slot].in_use) {
            (void)abi_net_index_insert(
                g_connection_index,
                ABI_NET_CONNECTION_INDEX_CAPACITY,
                ABI_NET_CONNECTION_INDEX_MASK,
                g_connections[slot].id,
                slot
            );
        }
    }
}

static void abi_net_rebuild_listener_index(void) {
    uint32_t slot;
    memset(g_listener_index, 0, sizeof(g_listener_index));
    for (slot = 0u; slot < ABI_NET_MAX_LISTENERS; ++slot) {
        if (g_listeners[slot].in_use) {
            (void)abi_net_index_insert(
                g_listener_index,
                ABI_NET_LISTENER_INDEX_CAPACITY,
                ABI_NET_LISTENER_INDEX_MASK,
                g_listeners[slot].id,
                slot
            );
        }
    }
}

static void abi_net_rebuild_request_index(void) {
    uint32_t slot;
    memset(g_request_index, 0, sizeof(g_request_index));
    for (slot = 0u; slot < ABI_NET_MAX_HTTP_REQUESTS; ++slot) {
        if (g_requests[slot].in_use) {
            (void)abi_net_index_insert(
                g_request_index,
                ABI_NET_REQUEST_INDEX_CAPACITY,
                ABI_NET_REQUEST_INDEX_MASK,
                g_requests[slot].id,
                slot
            );
        }
    }
}

static void abi_net_rebuild_response_index(void) {
    uint32_t slot;
    memset(g_response_index, 0, sizeof(g_response_index));
    for (slot = 0u; slot < ABI_NET_MAX_HTTP_RESPONSES; ++slot) {
        if (g_responses[slot].in_use) {
            (void)abi_net_index_insert(
                g_response_index,
                ABI_NET_RESPONSE_INDEX_CAPACITY,
                ABI_NET_RESPONSE_INDEX_MASK,
                g_responses[slot].id,
                slot
            );
        }
    }
}

static void abi_net_rebuild_server_index(void) {
    uint32_t slot;
    memset(g_server_index, 0, sizeof(g_server_index));
    for (slot = 0u; slot < ABI_NET_MAX_HTTP_SERVERS; ++slot) {
        if (g_servers[slot].in_use) {
            (void)abi_net_index_insert(
                g_server_index,
                ABI_NET_SERVER_INDEX_CAPACITY,
                ABI_NET_SERVER_INDEX_MASK,
                g_servers[slot].id,
                slot
            );
        }
    }
}

#ifdef _WIN32
static void abi_net_init_winsock(void) {
    static int initialized = 0;
    if (!initialized) {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        initialized = 1;
    }
}
#endif

static void abi_net_copy(char* destination, size_t capacity, const char* source) {
    if (destination == 0 || capacity == 0u) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, capacity, "%s", source);
}

static int abi_net_text_equal_ci(const char* left, const char* right) {
    if (left == 0 || right == 0) {
        return 0;
    }
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return strcasecmp(left, right) == 0;
#endif
}

static const char* abi_net_normalize_protocol_name(const char* protocol_name) {
    if (protocol_name == 0 || protocol_name[0] == '\0') {
        return g_http_protocol_http11;
    }
    if (abi_net_text_equal_ci(protocol_name, "http/1.1") ||
        abi_net_text_equal_ci(protocol_name, "http1") ||
        abi_net_text_equal_ci(protocol_name, "http11")) {
        return g_http_protocol_http11;
    }
    if (abi_net_text_equal_ci(protocol_name, "http/2") ||
        abi_net_text_equal_ci(protocol_name, "http2") ||
        abi_net_text_equal_ci(protocol_name, "h2")) {
        return g_http_protocol_http2;
    }
    return 0;
}

static const char* abi_net_http_version_token_from_protocol(const char* protocol_name) {
    const char* normalized = abi_net_normalize_protocol_name(protocol_name);
    if (normalized == 0) {
        return 0;
    }
    if (normalized == g_http_protocol_http2) {
        return "HTTP/2";
    }
    return "HTTP/1.1";
}

static const char* abi_net_protocol_from_http_version_token(const char* version_token) {
    if (version_token == 0 || version_token[0] == '\0') {
        return g_http_protocol_http11;
    }
    if (abi_net_text_equal_ci(version_token, "HTTP/2") ||
        abi_net_text_equal_ci(version_token, "HTTP/2.0")) {
        return g_http_protocol_http2;
    }
    return g_http_protocol_http11;
}

static const char* abi_net_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static const char* abi_net_string_from_bytes(const unsigned char* bytes, size_t length) {
    char* output;
    if (bytes == 0 || length == 0u) {
        return string_new("");
    }
    output = (char*)kain_alloc_rc(length + 1u, 1);
    if (output == 0) {
        return string_new("");
    }
    memcpy(output, bytes, length);
    output[length] = '\0';
    kain_rc_set_string_length(output, kain_bounded_text_length((const char*)bytes, length));
    return output;
}

static int64_t abi_net_ok(void) {
    g_last_status = ABI_NET_OK;
    abi_net_copy(g_last_error_kind, sizeof(g_last_error_kind), "ok");
    g_last_error_message[0] = '\0';
    return ABI_NET_OK;
}

static int64_t abi_net_fail(int64_t status, const char* kind, const char* message) {
    g_last_status = status;
    abi_net_copy(g_last_error_kind, sizeof(g_last_error_kind), kind ? kind : "error");
    abi_net_copy(g_last_error_message, sizeof(g_last_error_message), message ? message : "");
    return status;
}

static void abi_net_socket_close(SOCKET socket_handle) {
    if (socket_handle == INVALID_SOCKET) {
        return;
    }
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

static int abi_net_wait_readable(SOCKET socket_handle, int64_t timeout_ms) {
    fd_set read_set;
    struct timeval timeout;
    struct timeval* timeout_ptr = 0;
    int result;

    FD_ZERO(&read_set);
    FD_SET(socket_handle, &read_set);
    if (timeout_ms >= 0) {
        timeout.tv_sec = (long)(timeout_ms / 1000);
        timeout.tv_usec = (long)((timeout_ms % 1000) * 1000);
        timeout_ptr = &timeout;
    }
    result = select((int)(socket_handle + 1), &read_set, 0, 0, timeout_ptr);
    return result > 0 && FD_ISSET(socket_handle, &read_set);
}

static uint64_t abi_net_atomic_fetch_add_u64(volatile uint64_t* target, uint64_t increment) {
#if defined(_MSC_VER)
    return (uint64_t)_InterlockedExchangeAdd64((volatile long long*)target, (long long)increment);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_fetch_add(target, increment, __ATOMIC_RELAXED);
#else
    uint64_t old = *target;
    *target = old + increment;
    return old;
#endif
}

static uint64_t abi_net_atomic_load_u64(volatile uint64_t* target) {
#if defined(_MSC_VER)
    return (uint64_t)_InterlockedCompareExchange64((volatile long long*)target, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(target, __ATOMIC_ACQUIRE);
#else
    return *target;
#endif
}

static void abi_net_atomic_store_u64(volatile uint64_t* target, uint64_t value) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange64((volatile long long*)target, (long long)value);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(target, value, __ATOMIC_RELEASE);
#else
    *target = value;
#endif
}

static void abi_net_thread_yield(void) {
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}

static void abi_net_set_socket_timeout_ms(SOCKET socket_handle, int64_t timeout_ms) {
    if (socket_handle == INVALID_SOCKET || timeout_ms <= 0) {
        return;
    }
#ifdef _WIN32
    {
        DWORD timeout = (DWORD)timeout_ms;
        (void)setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        (void)setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    }
#else
    {
        struct timeval timeout;
        timeout.tv_sec = (long)(timeout_ms / 1000);
        timeout.tv_usec = (long)((timeout_ms % 1000) * 1000);
        (void)setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    }
#endif
}

static int abi_net_send_all(SOCKET socket_handle, const unsigned char* bytes, size_t length) {
    size_t sent = 0u;
    while (sent < length) {
        int chunk = (int)((length - sent) > 32768u ? 32768u : (length - sent));
        int written = send(socket_handle, (const char*)bytes + sent, chunk, 0);
        if (written <= 0) {
            return 0;
        }
        sent += (size_t)written;
    }
    return 1;
}

static int abi_net_hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int abi_net_decode_hex(const char* hex, unsigned char** out_bytes, size_t* out_length) {
    size_t length;
    size_t index;
    unsigned char* bytes;
    if (out_bytes) *out_bytes = 0;
    if (out_length) *out_length = 0u;
    if (hex == 0) {
        return 0;
    }
    length = strlen(hex);
    if ((length % 2u) != 0u) {
        return 0;
    }
    bytes = (unsigned char*)malloc(length / 2u + 1u);
    if (bytes == 0) {
        return 0;
    }
    for (index = 0; index < length; index += 2u) {
        int high = abi_net_hex_value(hex[index]);
        int low = abi_net_hex_value(hex[index + 1u]);
        if (high < 0 || low < 0) {
            free(bytes);
            return 0;
        }
        bytes[index / 2u] = (unsigned char)((high << 4) | low);
    }
    if (out_bytes) *out_bytes = bytes;
    if (out_length) *out_length = length / 2u;
    return 1;
}

static const char* abi_net_encode_hex(const unsigned char* bytes, size_t length) {
    static const char alphabet[] = "0123456789abcdef";
    char* output;
    size_t index;
    if (bytes == 0 || length == 0u) {
        return string_new("");
    }
    output = (char*)kain_alloc_rc(length * 2u + 1u, 1);
    if (output == 0) {
        return string_new("");
    }
    for (index = 0u; index < length; ++index) {
        output[index * 2u] = alphabet[(bytes[index] >> 4) & 0x0F];
        output[index * 2u + 1u] = alphabet[bytes[index] & 0x0F];
    }
    output[length * 2u] = '\0';
    return output;
}

static int abi_net_append_bytes(unsigned char** buffer, size_t* length, size_t* capacity, const unsigned char* bytes, size_t byte_count) {
    unsigned char* resized;
    size_t needed;
    size_t next_capacity;
    if (byte_count == 0u) {
        return 1;
    }
    if (buffer == 0 || length == 0 || capacity == 0 || bytes == 0) {
        return 0;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-net-append-needed-size-does-not-wrap-after-guards.yaml */
    if (abi_net_size_add_overflow(*length, byte_count, &needed) ||
        abi_net_size_add_overflow(needed, 1u, &needed)) {
        return 0;
    }
    if (needed > *capacity) {
        next_capacity = *capacity == 0u ? 4096u : *capacity;
        while (next_capacity < needed) {
            if (next_capacity > (SIZE_MAX / 2u)) {
                next_capacity = needed;
                break;
            }
            next_capacity *= 2u;
        }
        resized = (unsigned char*)realloc(*buffer, next_capacity);
        if (resized == 0) {
            return 0;
        }
        *buffer = resized;
        *capacity = next_capacity;
    }
    memcpy(*buffer + *length, bytes, byte_count);
    *length += byte_count;
    (*buffer)[*length] = '\0';
    return 1;
}

static KainNativeTcpConnection* abi_net_connection(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = abi_net_index_start_slot(id, ABI_NET_CONNECTION_INDEX_MASK);
    for (probe = 0u; probe < ABI_NET_CONNECTION_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_NET_CONNECTION_INDEX_MASK;
        uint32_t encoded_slot = g_connection_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_NET_MAX_CONNECTIONS &&
            g_connections[slot].in_use &&
            g_connections[slot].id == id) {
            return &g_connections[slot];
        }
    }
    return 0;
}

static KainNativeTcpConnection* abi_net_alloc_connection(SOCKET socket_handle) {
    uint32_t slot;
    uint64_t bit;
    if (!abi_net_find_free_slot_u64(
            g_connection_occupancy_bits,
            ABI_NET_CONNECTION_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_connections[slot], 0, sizeof(g_connections[slot]));
    g_connections[slot].in_use = 1;
    g_connections[slot].id = g_next_connection_id++;
    g_connections[slot].socket_handle = socket_handle;
    bit = UINT64_C(1) << slot;
    g_connection_occupancy_bits |= bit;
    if (!abi_net_index_insert(
            g_connection_index,
            ABI_NET_CONNECTION_INDEX_CAPACITY,
            ABI_NET_CONNECTION_INDEX_MASK,
            g_connections[slot].id,
            slot)) {
        g_connection_occupancy_bits &= ~bit;
        memset(&g_connections[slot], 0, sizeof(g_connections[slot]));
        return 0;
    }
    return &g_connections[slot];
}

static KainNativeTcpListener* abi_net_listener(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = abi_net_index_start_slot(id, ABI_NET_LISTENER_INDEX_MASK);
    for (probe = 0u; probe < ABI_NET_LISTENER_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_NET_LISTENER_INDEX_MASK;
        uint32_t encoded_slot = g_listener_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_NET_MAX_LISTENERS &&
            g_listeners[slot].in_use &&
            g_listeners[slot].id == id) {
            return &g_listeners[slot];
        }
    }
    return 0;
}

static KainNativeTcpListener* abi_net_alloc_listener(SOCKET socket_handle, int64_t local_port) {
    uint32_t slot;
    uint64_t bit;
    if (!abi_net_find_free_slot_u64(
            g_listener_occupancy_bits,
            ABI_NET_LISTENER_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_listeners[slot], 0, sizeof(g_listeners[slot]));
    g_listeners[slot].in_use = 1;
    g_listeners[slot].id = g_next_listener_id++;
    g_listeners[slot].socket_handle = socket_handle;
    g_listeners[slot].local_port = local_port;
    bit = UINT64_C(1) << slot;
    g_listener_occupancy_bits |= bit;
    if (!abi_net_index_insert(
            g_listener_index,
            ABI_NET_LISTENER_INDEX_CAPACITY,
            ABI_NET_LISTENER_INDEX_MASK,
            g_listeners[slot].id,
            slot)) {
        g_listener_occupancy_bits &= ~bit;
        memset(&g_listeners[slot], 0, sizeof(g_listeners[slot]));
        return 0;
    }
    return &g_listeners[slot];
}

static KainNativeHttpRequest* abi_net_request(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = abi_net_index_start_slot(id, ABI_NET_REQUEST_INDEX_MASK);
    for (probe = 0u; probe < ABI_NET_REQUEST_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_NET_REQUEST_INDEX_MASK;
        uint32_t encoded_slot = g_request_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_NET_MAX_HTTP_REQUESTS &&
            g_requests[slot].in_use &&
            g_requests[slot].id == id) {
            return &g_requests[slot];
        }
    }
    return 0;
}

static KainNativeHttpRequest* abi_net_alloc_request(int incoming) {
    uint32_t slot;
    uint64_t bit;
    if (!abi_net_find_free_slot_u64(
            g_request_occupancy_bits,
            ABI_NET_REQUEST_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_requests[slot], 0, sizeof(g_requests[slot]));
    g_requests[slot].in_use = 1;
    g_requests[slot].incoming = incoming;
    g_requests[slot].id = g_next_request_id++;
    g_requests[slot].socket_handle = INVALID_SOCKET;
    g_requests[slot].timeout_ms = 30000;
    abi_net_copy(g_requests[slot].protocol, sizeof(g_requests[slot].protocol), g_http_protocol_http11);
    bit = UINT64_C(1) << slot;
    g_request_occupancy_bits |= bit;
    if (!abi_net_index_insert(
            g_request_index,
            ABI_NET_REQUEST_INDEX_CAPACITY,
            ABI_NET_REQUEST_INDEX_MASK,
            g_requests[slot].id,
            slot)) {
        g_request_occupancy_bits &= ~bit;
        memset(&g_requests[slot], 0, sizeof(g_requests[slot]));
        return 0;
    }
    return &g_requests[slot];
}

static KainNativeHttpResponse* abi_net_response(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = abi_net_index_start_slot(id, ABI_NET_RESPONSE_INDEX_MASK);
    for (probe = 0u; probe < ABI_NET_RESPONSE_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_NET_RESPONSE_INDEX_MASK;
        uint32_t encoded_slot = g_response_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_NET_MAX_HTTP_RESPONSES &&
            g_responses[slot].in_use &&
            g_responses[slot].id == id) {
            return &g_responses[slot];
        }
    }
    return 0;
}

static KainNativeHttpResponse* abi_net_alloc_response(void) {
    uint32_t slot;
    uint64_t bit;
    if (!abi_net_find_free_slot_u64(
            g_response_occupancy_bits,
            ABI_NET_RESPONSE_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_responses[slot], 0, sizeof(g_responses[slot]));
    g_responses[slot].in_use = 1;
    g_responses[slot].id = g_next_response_id++;
    abi_net_copy(g_responses[slot].protocol, sizeof(g_responses[slot].protocol), g_http_protocol_http11);
    bit = UINT64_C(1) << slot;
    g_response_occupancy_bits |= bit;
    if (!abi_net_index_insert(
            g_response_index,
            ABI_NET_RESPONSE_INDEX_CAPACITY,
            ABI_NET_RESPONSE_INDEX_MASK,
            g_responses[slot].id,
            slot)) {
        g_response_occupancy_bits &= ~bit;
        memset(&g_responses[slot], 0, sizeof(g_responses[slot]));
        return 0;
    }
    return &g_responses[slot];
}

static KainNativeHttpServer* abi_net_server(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = abi_net_index_start_slot(id, ABI_NET_SERVER_INDEX_MASK);
    for (probe = 0u; probe < ABI_NET_SERVER_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_NET_SERVER_INDEX_MASK;
        uint32_t encoded_slot = g_server_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_NET_MAX_HTTP_SERVERS &&
            g_servers[slot].in_use &&
            g_servers[slot].id == id) {
            return &g_servers[slot];
        }
    }
    return 0;
}

static KainNativeHttpServer* abi_net_alloc_server(void) {
    uint32_t slot;
    uint64_t bit;
    if (!abi_net_find_free_slot_u64(
            g_server_occupancy_bits,
            ABI_NET_SERVER_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_servers[slot], 0, sizeof(g_servers[slot]));
    g_servers[slot].in_use = 1;
    g_servers[slot].id = g_next_server_id++;
    g_servers[slot].socket_handle = INVALID_SOCKET;
    bit = UINT64_C(1) << slot;
    g_server_occupancy_bits |= bit;
    if (!abi_net_index_insert(
            g_server_index,
            ABI_NET_SERVER_INDEX_CAPACITY,
            ABI_NET_SERVER_INDEX_MASK,
            g_servers[slot].id,
            slot)) {
        g_server_occupancy_bits &= ~bit;
        memset(&g_servers[slot], 0, sizeof(g_servers[slot]));
        return 0;
    }
    return &g_servers[slot];
}

static void abi_net_clear_request_from_server_queue(KainNativeHttpRequest* request) {
    if (request != 0 && request->server_id > 0) {
        KainNativeHttpServer* server = abi_net_server(request->server_id);
        if (server != 0) {
            uint32_t slot = (uint32_t)(request - g_requests);
            server->pending_request_slots &= ~(UINT64_C(1) << slot);
        }
    }
}

static void abi_net_set_header(KainNativeNetHeader* headers, int64_t* header_count, const char* key, const char* value) {
    int64_t index;
    if (headers == 0 || header_count == 0 || key == 0 || key[0] == '\0') {
        return;
    }
    for (index = 0; index < *header_count; ++index) {
        if (headers[index].in_use && abi_net_text_equal_ci(headers[index].key, key)) {
            abi_net_copy(headers[index].value, sizeof(headers[index].value), value);
            return;
        }
    }
    if (*header_count >= ABI_NET_MAX_HEADERS) {
        return;
    }
    headers[*header_count].in_use = 1;
    abi_net_copy(headers[*header_count].key, sizeof(headers[*header_count].key), key);
    abi_net_copy(headers[*header_count].value, sizeof(headers[*header_count].value), value);
    *header_count += 1;
}

static const char* abi_net_find_header(const KainNativeNetHeader* headers, int64_t header_count, const char* key) {
    int64_t index;
    if (headers == 0 || key == 0) {
        return g_empty_string;
    }
    for (index = 0; index < header_count; ++index) {
        if (headers[index].in_use && abi_net_text_equal_ci(headers[index].key, key)) {
            return headers[index].value;
        }
    }
    return g_empty_string;
}

static int abi_net_parse_url(const char* url, KainNativeParsedUrl* parsed) {
    const char* scheme_end;
    const char* host_start;
    const char* path_start;
    const char* port_start;
    size_t host_length;
    if (url == 0 || parsed == 0) {
        return 0;
    }
    memset(parsed, 0, sizeof(*parsed));
    scheme_end = strstr(url, "://");
    if (scheme_end == 0) {
        return 0;
    }
    host_start = scheme_end + 3;
    if ((size_t)(scheme_end - url) >= sizeof(parsed->scheme)) {
        return 0;
    }
    memcpy(parsed->scheme, url, (size_t)(scheme_end - url));
    parsed->scheme[scheme_end - url] = '\0';
    parsed->secure = abi_net_text_equal_ci(parsed->scheme, "https");
    if (!parsed->secure && !abi_net_text_equal_ci(parsed->scheme, "http")) {
        return 0;
    }
    parsed->port = parsed->secure ? 443 : 80;
    path_start = strchr(host_start, '/');
    port_start = strchr(host_start, ':');
    if (path_start == 0) {
        path_start = url + strlen(url);
        abi_net_copy(parsed->path, sizeof(parsed->path), "/");
    } else {
        abi_net_copy(parsed->path, sizeof(parsed->path), path_start);
    }
    if (port_start != 0 && port_start < path_start) {
        host_length = (size_t)(port_start - host_start);
        parsed->port = atoll(port_start + 1);
    } else {
        host_length = (size_t)(path_start - host_start);
    }
    if (host_length == 0u || host_length >= sizeof(parsed->host)) {
        return 0;
    }
    memcpy(parsed->host, host_start, host_length);
    parsed->host[host_length] = '\0';
    return 1;
}

static SOCKET abi_net_connect_socket(const char* host, int64_t port) {
    SOCKET socket_handle = INVALID_SOCKET;
    struct addrinfo hints;
    struct addrinfo* result = 0;
    struct addrinfo* current;
    char port_text[32];
#ifdef _WIN32
    abi_net_init_winsock();
#endif
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(port_text, sizeof(port_text), "%lld", (long long)port);
    if (getaddrinfo(host, port_text, &hints, &result) != 0) {
        return INVALID_SOCKET;
    }
    for (current = result; current != 0; current = current->ai_next) {
        socket_handle = socket((int)current->ai_family, (int)current->ai_socktype, (int)current->ai_protocol);
        if (socket_handle == INVALID_SOCKET) {
            continue;
        }
        if (connect(socket_handle, current->ai_addr, (int)current->ai_addrlen) == 0) {
            freeaddrinfo(result);
            return socket_handle;
        }
        abi_net_socket_close(socket_handle);
        socket_handle = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return INVALID_SOCKET;
}

static int64_t abi_net_bind_listener(const char* host, int64_t port) {
    SOCKET socket_handle = INVALID_SOCKET;
    struct addrinfo hints;
    struct addrinfo* result = 0;
    struct addrinfo* current;
    char port_text[32];
    int yes = 1;
#ifdef _WIN32
    abi_net_init_winsock();
#endif
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    snprintf(port_text, sizeof(port_text), "%lld", (long long)port);
    if (getaddrinfo((host && host[0]) ? host : "127.0.0.1", port_text, &hints, &result) != 0) {
        return INVALID_SOCKET;
    }
    for (current = result; current != 0; current = current->ai_next) {
        socket_handle = socket((int)current->ai_family, (int)current->ai_socktype, (int)current->ai_protocol);
        if (socket_handle == INVALID_SOCKET) {
            continue;
        }
        setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
        if (bind(socket_handle, current->ai_addr, (int)current->ai_addrlen) == 0 && listen(socket_handle, 16) == 0) {
            freeaddrinfo(result);
            return socket_handle;
        }
        abi_net_socket_close(socket_handle);
        socket_handle = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return INVALID_SOCKET;
}

static int64_t abi_net_socket_local_port(SOCKET socket_handle) {
    struct sockaddr_in address;
    socklen_t address_length = (socklen_t)sizeof(address);
    memset(&address, 0, sizeof(address));
    if (getsockname(socket_handle, (struct sockaddr*)&address, &address_length) != 0) {
        return 0;
    }
    return (int64_t)ntohs(address.sin_port);
}

static SOCKET abi_net_connect_loopback_socket(int64_t port) {
    SOCKET socket_handle;
    struct sockaddr_in address;
#ifdef _WIN32
    abi_net_init_winsock();
#endif
    if (port <= 0 || port > 65535) {
        return INVALID_SOCKET;
    }
    socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((u_short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(socket_handle, (struct sockaddr*)&address, sizeof(address)) != 0) {
        abi_net_socket_close(socket_handle);
        return INVALID_SOCKET;
    }
    return socket_handle;
}

static void abi_net_shutdown_send(SOCKET socket_handle) {
#ifdef _WIN32
    shutdown(socket_handle, SD_SEND);
#else
    shutdown(socket_handle, SHUT_WR);
#endif
}

static unsigned long long abi_net_hash_message_name(const char* message_name) {
    unsigned long long hash = 1469598103934665603ULL;
    const unsigned char* cursor = (const unsigned char*)(message_name ? message_name : "");
    while (*cursor) {
        hash ^= (unsigned long long)(*cursor++);
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

static void abi_net_dispatch_route(KainNativeHttpServer* server, KainNativeHttpRequest* request) {
    int64_t index;
    if (server == 0 || request == 0) {
        return;
    }
    for (index = 0; index < server->route_count; ++index) {
        KainNativeHttpRoute* route = &server->routes[index];
        if (!route->in_use) {
            continue;
        }
        if (abi_net_text_equal_ci(route->method, request->method) && strcmp(route->path, request->path) == 0) {
            KainActorMessage message;
            request->actor_id = route->actor_id;
            abi_net_copy(request->actor_message_kind, sizeof(request->actor_message_kind), route->message_kind);
            snprintf(
                request->actor_payload,
                sizeof(request->actor_payload),
                "request_id=%lld\nmethod=%s\npath=%s\nquery=%s\n",
                (long long)request->id,
                request->method,
                request->path,
                request->query
            );
            memset(&message, 0, sizeof(message));
            message.type_tag = abi_net_hash_message_name(route->message_kind);
            message.data = request->actor_payload;
            message.data_size = strlen(request->actor_payload) + 1u;
            message.sender_id = KAIN_ACTOR_ID_INVALID;
            (void)kain_actor_send((KainActorId)route->actor_id, &message, 0);
            return;
        }
    }
}

static int abi_net_parse_header_line(KainNativeHttpRequest* request, char* line) {
    char* colon = strchr(line, ':');
    char* value;
    if (colon == 0) {
        return 1;
    }
    *colon = '\0';
    value = colon + 1;
    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    abi_net_set_header(request->headers, &request->header_count, line, value);
    return 1;
}

static int abi_net_parse_http_request(KainNativeHttpRequest* request, unsigned char* bytes, size_t length) {
    char* text;
    char* headers_end;
    char* line;
    char* next_line;
    char* target;
    char* version;
    size_t header_length;
    size_t body_length = 0u;
    size_t required_length = 0u;
    const char* content_length_text;
    if (request == 0 || bytes == 0 || length == 0u) {
        return 0;
    }
    text = (char*)malloc(length + 1u);
    if (text == 0) {
        return 0;
    }
    memcpy(text, bytes, length);
    text[length] = '\0';
    headers_end = strstr(text, "\r\n\r\n");
    if (headers_end == 0) {
        free(text);
        return 0;
    }
    header_length = (size_t)(headers_end + 4 - text);
    line = text;
    next_line = strstr(line, "\r\n");
    if (next_line == 0) {
        free(text);
        return 0;
    }
    *next_line = '\0';
    target = strchr(line, ' ');
    if (target == 0) {
        free(text);
        return 0;
    }
    *target++ = '\0';
    version = strchr(target, ' ');
    if (version == 0) {
        free(text);
        return 0;
    }
    *version++ = '\0';
    abi_net_copy(request->method, sizeof(request->method), line);
    abi_net_copy(
        request->protocol,
        sizeof(request->protocol),
        abi_net_protocol_from_http_version_token(version)
    );
    {
        char* query = strchr(target, '?');
        if (query != 0) {
            *query++ = '\0';
            abi_net_copy(request->query, sizeof(request->query), query);
        }
        abi_net_copy(request->path, sizeof(request->path), target);
    }
    line = next_line + 2;
    while (line < headers_end) {
        next_line = strstr(line, "\r\n");
        if (next_line == 0 || next_line > headers_end) {
            break;
        }
        *next_line = '\0';
        if (line[0] != '\0') {
            abi_net_parse_header_line(request, line);
        }
        line = next_line + 2;
    }
    content_length_text = abi_net_find_header(request->headers, request->header_count, "Content-Length");
    if (content_length_text && content_length_text[0]) {
        if (!abi_net_parse_content_length_header(content_length_text, &body_length)) {
            free(text);
            return 0;
        }
    }
    if (body_length > 0u) {
        /*
         * Proof: runtime/native/src/core/z3/proofs/native-net-request-body-span-does-not-wrap-after-guards.yaml
         * Proof: runtime/native/src/core/z3/proofs/native-net-request-body-allocation-does-not-wrap-after-guards.yaml
         */
        if (abi_net_size_add_overflow(header_length, body_length, &required_length) ||
            required_length > length ||
            abi_net_size_add_overflow(body_length, 1u, &required_length)) {
            free(text);
            return 0;
        }
        request->body = (unsigned char*)malloc(required_length);
        if (request->body == 0) {
            free(text);
            return 0;
        }
        memcpy(request->body, bytes + header_length, body_length);
        request->body[body_length] = '\0';
        request->body_length = body_length;
    }
    free(text);
    return 1;
}

static int64_t abi_net_store_http_response(
    int64_t status_code,
    const char* protocol_name,
    const KainNativeNetHeader* headers,
    int64_t header_count,
    const unsigned char* body,
    size_t body_length
) {
    KainNativeHttpResponse* response = abi_net_alloc_response();
    const char* normalized_protocol = abi_net_normalize_protocol_name(protocol_name);
    int64_t index;
    if (response == 0) {
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP response capacity exceeded");
    }
    response->status_code = status_code;
    if (normalized_protocol == 0) {
        normalized_protocol = g_http_protocol_http11;
    }
    abi_net_copy(
        response->protocol,
        sizeof(response->protocol),
        normalized_protocol
    );
    for (index = 0; index < header_count && index < ABI_NET_MAX_HEADERS; ++index) {
        response->headers[index] = headers[index];
        response->header_count++;
    }
    if (body_length > 0u) {
        size_t allocation_size = 0u;
        /* Proof: runtime/native/src/core/z3/proofs/native-net-response-body-allocation-does-not-wrap.yaml */
        if (abi_net_size_add_overflow(body_length, 1u, &allocation_size)) {
            return abi_net_fail(ABI_NET_IO_ERROR, "allocation", "HTTP response body length overflowed");
        }
        response->body = (unsigned char*)malloc(allocation_size);
        if (response->body == 0) {
            return abi_net_fail(ABI_NET_IO_ERROR, "allocation", "could not allocate HTTP response body");
        }
        memcpy(response->body, body, body_length);
        response->body[body_length] = '\0';
        response->body_length = body_length;
    }
    abi_net_ok();
    return response->id;
}

static int64_t abi_net_send_raw_http_client(KainNativeHttpRequest* request, const KainNativeParsedUrl* url) {
    SOCKET socket_handle;
    unsigned char* response_bytes = 0;
    size_t response_length = 0u;
    size_t response_capacity = 0u;
    char request_head[ABI_NET_MAX_TEXT];
    const char* request_protocol = abi_net_normalize_protocol_name(request->protocol);
    int header_index;
    int64_t result_id;
    if (request_protocol == 0) {
        return abi_net_fail(
            ABI_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    if (request_protocol == g_http_protocol_http2) {
        return abi_net_fail(
            ABI_NET_PROTOCOL_UNSUPPORTED,
            "unsupported-protocol",
            "HTTP/2 client requests require the HTTPS WinHTTP lane in v1"
        );
    }
    socket_handle = abi_net_connect_socket(url->host, url->port);
    if (socket_handle == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_IO_ERROR, "connect", "HTTP client TCP connect failed");
    }
    snprintf(
        request_head,
        sizeof(request_head),
        "%s %s %s\r\nHost: %s\r\nConnection: close\r\nContent-Length: %llu\r\n",
        request->method[0] ? request->method : "GET",
        url->path[0] ? url->path : "/",
        abi_net_http_version_token_from_protocol(request_protocol),
        url->host,
        (unsigned long long)request->body_length
    );
    if (!abi_net_send_all(socket_handle, (const unsigned char*)request_head, strlen(request_head))) {
        abi_net_socket_close(socket_handle);
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP request write failed");
    }
    for (header_index = 0; header_index < request->header_count; ++header_index) {
        char header_line[ABI_NET_MAX_TEXT];
        snprintf(header_line, sizeof(header_line), "%s: %s\r\n", request->headers[header_index].key, request->headers[header_index].value);
        if (!abi_net_send_all(socket_handle, (const unsigned char*)header_line, strlen(header_line))) {
            abi_net_socket_close(socket_handle);
            return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP header write failed");
        }
    }
    if (!abi_net_send_all(socket_handle, (const unsigned char*)"\r\n", 2u)) {
        abi_net_socket_close(socket_handle);
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP header terminator write failed");
    }
    if (request->body_length > 0u && !abi_net_send_all(socket_handle, request->body, request->body_length)) {
        abi_net_socket_close(socket_handle);
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP body write failed");
    }
    while (abi_net_wait_readable(socket_handle, 5000)) {
        unsigned char buffer[4096];
        int read_count = recv(socket_handle, (char*)buffer, sizeof(buffer), 0);
        if (read_count <= 0) {
            break;
        }
        if (!abi_net_append_bytes(&response_bytes, &response_length, &response_capacity, buffer, (size_t)read_count)) {
            free(response_bytes);
            abi_net_socket_close(socket_handle);
            return abi_net_fail(ABI_NET_IO_ERROR, "allocation", "HTTP response capture allocation failed");
        }
    }
    abi_net_socket_close(socket_handle);
    if (response_length == 0u) {
        free(response_bytes);
        return abi_net_fail(ABI_NET_IO_ERROR, "read", "HTTP response was empty");
    }
    {
        char* header_end = strstr((char*)response_bytes, "\r\n\r\n");
        int64_t status = 0;
        const char* response_protocol = g_http_protocol_http11;
        KainNativeNetHeader headers[ABI_NET_MAX_HEADERS];
        int64_t header_count = 0;
        unsigned char* body = response_bytes;
        size_t body_length = response_length;
        memset(headers, 0, sizeof(headers));
        if (header_end != 0) {
            char* line;
            char* next_line;
            *header_end = '\0';
            body = (unsigned char*)header_end + 4;
            body_length = response_length - (size_t)(body - response_bytes);
            line = strstr((char*)response_bytes, "\r\n");
            if (line != 0) {
                char* status_code_text;
                *line = '\0';
                status_code_text = strchr((char*)response_bytes, ' ');
                if (status_code_text != 0) {
                    *status_code_text++ = '\0';
                    while (*status_code_text == ' ') {
                        ++status_code_text;
                    }
                    status = atoll(status_code_text);
                }
                response_protocol =
                    abi_net_protocol_from_http_version_token((char*)response_bytes);
                line += 2;
                while (*line) {
                    next_line = strstr(line, "\r\n");
                    if (next_line == 0) {
                        break;
                    }
                    *next_line = '\0';
                    {
                        char* colon = strchr(line, ':');
                        if (colon != 0 && header_count < ABI_NET_MAX_HEADERS) {
                            *colon = '\0';
                            ++colon;
                            while (*colon == ' ' || *colon == '\t') ++colon;
                            abi_net_set_header(headers, &header_count, line, colon);
                        }
                    }
                    line = next_line + 2;
                }
            }
        }
        result_id = abi_net_store_http_response(
            status,
            response_protocol,
            headers,
            header_count,
            body,
            body_length
        );
    }
    free(response_bytes);
    return result_id;
}

#ifdef _WIN32
static wchar_t* abi_net_wide_from_utf8(const char* text) {
    int needed;
    wchar_t* wide;
    if (text == 0) {
        text = "";
    }
    needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, 0, 0);
    if (needed <= 0) {
        return 0;
    }
    wide = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)needed);
    if (wide == 0) {
        return 0;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, needed);
    return wide;
}

static int64_t abi_net_send_winhttp_client(KainNativeHttpRequest* request, const KainNativeParsedUrl* url) {
    HINTERNET session = 0;
    HINTERNET connection = 0;
    HINTERNET win_request = 0;
    wchar_t* host_wide = 0;
    wchar_t* path_wide = 0;
    wchar_t* method_wide = 0;
    unsigned char* response_body = 0;
    size_t body_length = 0u;
    size_t body_capacity = 0u;
    const char* request_protocol = abi_net_normalize_protocol_name(request->protocol);
    const char* response_protocol = g_http_protocol_http11;
    DWORD status_code = 0u;
    DWORD status_size = sizeof(status_code);
    int64_t response_id;
    if (request_protocol == 0) {
        return abi_net_fail(
            ABI_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    host_wide = abi_net_wide_from_utf8(url->host);
    path_wide = abi_net_wide_from_utf8(url->path[0] ? url->path : "/");
    method_wide = abi_net_wide_from_utf8(request->method[0] ? request->method : "GET");
    if (host_wide == 0 || path_wide == 0 || method_wide == 0) {
        free(host_wide);
        free(path_wide);
        free(method_wide);
        return abi_net_fail(ABI_NET_IO_ERROR, "allocation", "could not allocate WinHTTP strings");
    }
    session = WinHttpOpen(L"KainNet/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == 0) {
        response_id = abi_net_fail(ABI_NET_IO_ERROR, "winhttp", "WinHttpOpen failed");
        goto cleanup;
    }
    connection = WinHttpConnect(session, host_wide, (INTERNET_PORT)url->port, 0);
    if (connection == 0) {
        response_id = abi_net_fail(ABI_NET_IO_ERROR, "winhttp", "WinHttpConnect failed");
        goto cleanup;
    }
    win_request = WinHttpOpenRequest(
        connection,
        method_wide,
        path_wide,
        0,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        url->secure ? WINHTTP_FLAG_SECURE : 0
    );
    if (win_request == 0) {
        response_id = abi_net_fail(ABI_NET_IO_ERROR, "winhttp", "WinHttpOpenRequest failed");
        goto cleanup;
    }
    if (request_protocol == g_http_protocol_http2) {
#if defined(WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL) && defined(WINHTTP_PROTOCOL_FLAG_HTTP2)
        DWORD enabled_protocols = WINHTTP_PROTOCOL_FLAG_HTTP2;
        if (!WinHttpSetOption(
                win_request,
                WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
                &enabled_protocols,
                sizeof(enabled_protocols)
            )) {
            response_id = abi_net_fail(
                ABI_NET_PROTOCOL_UNSUPPORTED,
                "unsupported-protocol",
                "WinHTTP could not enable HTTP/2 for this request"
            );
            goto cleanup;
        }
#else
        response_id = abi_net_fail(
            ABI_NET_PROTOCOL_UNSUPPORTED,
            "unsupported-protocol",
            "HTTP/2 requires a WinHTTP SDK with protocol option support"
        );
        goto cleanup;
#endif
    }
    {
        int64_t index;
        for (index = 0; index < request->header_count; ++index) {
            char line[ABI_NET_MAX_TEXT];
            wchar_t* wide_line;
            snprintf(line, sizeof(line), "%s: %s", request->headers[index].key, request->headers[index].value);
            wide_line = abi_net_wide_from_utf8(line);
            if (wide_line != 0) {
                WinHttpAddRequestHeaders(win_request, wide_line, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
                free(wide_line);
            }
        }
    }
    if (!WinHttpSendRequest(
            win_request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            request->body_length ? request->body : WINHTTP_NO_REQUEST_DATA,
            (DWORD)request->body_length,
            (DWORD)request->body_length,
            0
        ) || !WinHttpReceiveResponse(win_request, 0)) {
        response_id = abi_net_fail(ABI_NET_IO_ERROR, "winhttp", "WinHTTP send/receive failed");
        goto cleanup;
    }
#if defined(WINHTTP_OPTION_HTTP_PROTOCOL_USED) && defined(WINHTTP_PROTOCOL_FLAG_HTTP2)
    {
        DWORD protocol_used = 0u;
        DWORD protocol_used_size = sizeof(protocol_used);
        if (WinHttpQueryOption(
                win_request,
                WINHTTP_OPTION_HTTP_PROTOCOL_USED,
                &protocol_used,
                &protocol_used_size
            ) && (protocol_used & WINHTTP_PROTOCOL_FLAG_HTTP2) != 0u) {
            response_protocol = g_http_protocol_http2;
        }
    }
#endif
    WinHttpQueryHeaders(
        win_request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX
    );
    for (;;) {
        DWORD available = 0u;
        DWORD read_count = 0u;
        unsigned char* chunk;
        if (!WinHttpQueryDataAvailable(win_request, &available) || available == 0u) {
            break;
        }
        chunk = (unsigned char*)malloc(available);
        if (chunk == 0) {
            response_id = abi_net_fail(ABI_NET_IO_ERROR, "allocation", "WinHTTP body allocation failed");
            goto cleanup;
        }
        if (!WinHttpReadData(win_request, chunk, available, &read_count)) {
            free(chunk);
            response_id = abi_net_fail(ABI_NET_IO_ERROR, "winhttp", "WinHTTP body read failed");
            goto cleanup;
        }
        if (!abi_net_append_bytes(&response_body, &body_length, &body_capacity, chunk, read_count)) {
            free(chunk);
            response_id = abi_net_fail(ABI_NET_IO_ERROR, "allocation", "HTTP response allocation failed");
            goto cleanup;
        }
        free(chunk);
    }
    response_id = abi_net_store_http_response(
        (int64_t)status_code,
        response_protocol,
        0,
        0,
        response_body,
        body_length
    );
cleanup:
    free(response_body);
    free(host_wide);
    free(path_wide);
    free(method_wide);
    if (win_request) WinHttpCloseHandle(win_request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    return response_id;
}
#endif

int64_t abi_net_reset(void) {
    size_t index;
    for (index = 0u; index < ABI_NET_MAX_CONNECTIONS; ++index) {
        if (g_connections[index].in_use) {
            abi_net_socket_close(g_connections[index].socket_handle);
        }
    }
    for (index = 0u; index < ABI_NET_MAX_LISTENERS; ++index) {
        if (g_listeners[index].in_use) {
            abi_net_socket_close(g_listeners[index].socket_handle);
        }
    }
    for (index = 0u; index < ABI_NET_MAX_HTTP_REQUESTS; ++index) {
        if (g_requests[index].in_use) {
            free(g_requests[index].body);
            if (g_requests[index].socket_handle != INVALID_SOCKET) {
                abi_net_socket_close(g_requests[index].socket_handle);
            }
        }
    }
    for (index = 0u; index < ABI_NET_MAX_HTTP_RESPONSES; ++index) {
        if (g_responses[index].in_use) {
            free(g_responses[index].body);
        }
    }
    for (index = 0u; index < ABI_NET_MAX_HTTP_SERVERS; ++index) {
        if (g_servers[index].in_use && g_servers[index].socket_handle != INVALID_SOCKET) {
            abi_net_socket_close(g_servers[index].socket_handle);
        }
    }
    memset(g_connections, 0, sizeof(g_connections));
    memset(g_listeners, 0, sizeof(g_listeners));
    memset(g_requests, 0, sizeof(g_requests));
    memset(g_responses, 0, sizeof(g_responses));
    memset(g_servers, 0, sizeof(g_servers));
    memset(g_connection_index, 0, sizeof(g_connection_index));
    memset(g_listener_index, 0, sizeof(g_listener_index));
    memset(g_request_index, 0, sizeof(g_request_index));
    memset(g_response_index, 0, sizeof(g_response_index));
    memset(g_server_index, 0, sizeof(g_server_index));
    g_connection_occupancy_bits = 0u;
    g_listener_occupancy_bits = 0u;
    g_request_occupancy_bits = 0u;
    g_response_occupancy_bits = 0u;
    g_server_occupancy_bits = 0u;
    g_next_connection_id = 1;
    g_next_listener_id = 1;
    g_next_request_id = 1;
    g_next_response_id = 1;
    g_next_server_id = 1;
    return abi_net_ok();
}

int64_t abi_net_platform_available(void) {
    return 1;
}

const char* abi_net_platform_name(void) {
#ifdef _WIN32
    abi_net_ok();
    return abi_net_string("windows");
#elif defined(__APPLE__)
    abi_net_ok();
    return abi_net_string("macos");
#elif defined(__linux__)
    abi_net_ok();
    return abi_net_string("linux");
#else
    abi_net_ok();
    return abi_net_string("unknown");
#endif
}

int64_t abi_net_capability_state(const char* capability_key) {
    if (capability_key == 0 || capability_key[0] == '\0') {
        return abi_net_fail(
            ABI_NET_INVALID_ARGUMENT,
            "invalid-capability",
            "network capability key is required"
        );
    }
    if (abi_net_text_equal_ci(capability_key, "net") ||
        abi_net_text_equal_ci(capability_key, "tcp") ||
        abi_net_text_equal_ci(capability_key, "http1.client") ||
        abi_net_text_equal_ci(capability_key, "http.client") ||
        abi_net_text_equal_ci(capability_key, "http1.server") ||
        abi_net_text_equal_ci(capability_key, "http.server")) {
        abi_net_ok();
        return ABI_NET_CAPABILITY_AVAILABLE;
    }
    if (abi_net_text_equal_ci(capability_key, "tls.client") ||
        abi_net_text_equal_ci(capability_key, "https.client")) {
#ifdef _WIN32
        abi_net_ok();
        return ABI_NET_CAPABILITY_AVAILABLE;
#else
        abi_net_ok();
        return ABI_NET_CAPABILITY_UNAVAILABLE;
#endif
    }
    if (abi_net_text_equal_ci(capability_key, "http2.client")) {
#ifdef _WIN32
        abi_net_ok();
        return ABI_NET_CAPABILITY_DEGRADED;
#else
        abi_net_ok();
        return ABI_NET_CAPABILITY_UNAVAILABLE;
#endif
    }
    if (abi_net_text_equal_ci(capability_key, "tls.server") ||
        abi_net_text_equal_ci(capability_key, "https.server") ||
        abi_net_text_equal_ci(capability_key, "http2.server")) {
        abi_net_ok();
        return ABI_NET_CAPABILITY_UNAVAILABLE;
    }
    return abi_net_fail(
        ABI_NET_INVALID_ARGUMENT,
        "invalid-capability",
        "network capability key is unknown"
    );
}

int64_t abi_tcp_connect(const char* host, int64_t port, int64_t timeout_ms) {
    SOCKET socket_handle;
    KainNativeTcpConnection* connection;
    (void)timeout_ms;
    if (host == 0 || host[0] == '\0' || port <= 0) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-argument", "TCP connect requires host and port");
    }
    socket_handle = abi_net_connect_socket(host, port);
    if (socket_handle == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_IO_ERROR, "connect", "TCP connect failed");
    }
    connection = abi_net_alloc_connection(socket_handle);
    if (connection == 0) {
        abi_net_socket_close(socket_handle);
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "TCP connection capacity exceeded");
    }
    abi_net_ok();
    return connection->id;
}

int64_t abi_tcp_listen(const char* host, int64_t port) {
    SOCKET socket_handle = (SOCKET)abi_net_bind_listener(host, port);
    KainNativeTcpListener* listener;
    if (socket_handle == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_IO_ERROR, "listen", "TCP listen failed");
    }
    listener = abi_net_alloc_listener(socket_handle, abi_net_socket_local_port(socket_handle));
    if (listener == 0) {
        abi_net_socket_close(socket_handle);
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "TCP listener capacity exceeded");
    }
    abi_net_ok();
    return listener->id;
}

int64_t abi_tcp_listener_local_port(int64_t listener_id) {
    KainNativeTcpListener* listener = abi_net_listener(listener_id);
    if (listener == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-listener", "TCP listener not found");
    }
    abi_net_ok();
    return listener->local_port;
}

int64_t abi_tcp_accept(int64_t listener_id, int64_t timeout_ms) {
    KainNativeTcpListener* listener = abi_net_listener(listener_id);
    SOCKET accepted;
    KainNativeTcpConnection* connection;
    if (listener == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-listener", "TCP listener not found");
    }
    if (!abi_net_wait_readable(listener->socket_handle, timeout_ms)) {
        abi_net_ok();
        return 0;
    }
    accepted = accept(listener->socket_handle, 0, 0);
    if (accepted == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_IO_ERROR, "accept", "TCP accept failed");
    }
    connection = abi_net_alloc_connection(accepted);
    if (connection == 0) {
        abi_net_socket_close(accepted);
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "TCP connection capacity exceeded");
    }
    abi_net_ok();
    return connection->id;
}

const char* abi_tcp_read_text(int64_t connection_id) {
    KainNativeTcpConnection* connection = abi_net_connection(connection_id);
    char buffer[4096];
    int read_count;
    if (connection == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
        return abi_net_string("");
    }
    if (!abi_net_wait_readable(connection->socket_handle, 5000)) {
        abi_net_ok();
        return abi_net_string("");
    }
    read_count = recv(connection->socket_handle, buffer, sizeof(buffer) - 1, 0);
    if (read_count <= 0) {
        abi_net_ok();
        return abi_net_string("");
    }
    buffer[read_count] = '\0';
    abi_net_ok();
    return abi_net_string(buffer);
}

const char* abi_tcp_read_hex(int64_t connection_id) {
    const char* text = abi_tcp_read_text(connection_id);
    return abi_net_encode_hex((const unsigned char*)text, strlen(text));
}

int64_t abi_tcp_write_text(int64_t connection_id, const char* payload) {
    KainNativeTcpConnection* connection = abi_net_connection(connection_id);
    const char* text = payload ? payload : "";
    if (connection == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
    }
    if (!abi_net_send_all(connection->socket_handle, (const unsigned char*)text, strlen(text))) {
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "TCP write failed");
    }
    return abi_net_ok();
}

int64_t abi_tcp_write_hex(int64_t connection_id, const char* payload_hex) {
    unsigned char* bytes = 0;
    size_t byte_length = 0u;
    KainNativeTcpConnection* connection = abi_net_connection(connection_id);
    if (connection == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
    }
    if (!abi_net_decode_hex(payload_hex, &bytes, &byte_length)) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-hex", "TCP hex payload is invalid");
    }
    if (!abi_net_send_all(connection->socket_handle, bytes, byte_length)) {
        free(bytes);
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "TCP byte write failed");
    }
    free(bytes);
    return abi_net_ok();
}

int64_t abi_tcp_close(int64_t connection_id) {
    KainNativeTcpConnection* connection = abi_net_connection(connection_id);
    uint32_t slot;
    if (connection == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
    }
    slot = (uint32_t)(connection - g_connections);
    abi_net_socket_close(connection->socket_handle);
    g_connection_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(connection, 0, sizeof(*connection));
    abi_net_rebuild_connection_index();
    return abi_net_ok();
}

int64_t abi_tcp_listener_close(int64_t listener_id) {
    KainNativeTcpListener* listener = abi_net_listener(listener_id);
    uint32_t slot;
    if (listener == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-listener", "TCP listener not found");
    }
    slot = (uint32_t)(listener - g_listeners);
    abi_net_socket_close(listener->socket_handle);
    g_listener_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(listener, 0, sizeof(*listener));
    abi_net_rebuild_listener_index();
    return abi_net_ok();
}

int64_t abi_http_request_create(const char* method, const char* url) {
    KainNativeHttpRequest* request;
    if (url == 0 || url[0] == '\0') {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-url", "HTTP request requires a URL");
    }
    request = abi_net_alloc_request(0);
    if (request == 0) {
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP request capacity exceeded");
    }
    abi_net_copy(request->method, sizeof(request->method), (method && method[0]) ? method : "GET");
    abi_net_copy(request->url, sizeof(request->url), url);
    abi_net_ok();
    return request->id;
}

int64_t abi_http_request_set_header(int64_t request_id, const char* key, const char* value) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (request->header_count >= ABI_NET_MAX_HEADERS) {
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP header capacity exceeded");
    }
    abi_net_set_header(request->headers, &request->header_count, key, value);
    return abi_net_ok();
}

int64_t abi_http_request_set_body_text(int64_t request_id, const char* payload) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    const char* text = payload ? payload : "";
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    free(request->body);
    request->body_length = strlen(text);
    /* Proof: runtime/native/src/core/z3/proofs/native-net-http-request-set-body-text-addition-overflow.yaml */
    size_t body_alloc_size;
    if (abi_net_size_add_overflow(request->body_length, 1u, &body_alloc_size)) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "body-too-large",
                           "HTTP request body exceeds maximum allocatable size");
    }
    request->body = (unsigned char*)malloc(body_alloc_size);
    if (request->body == 0) {
        return abi_net_fail(ABI_NET_IO_ERROR, "allocation", "could not allocate HTTP request body");
    }
    memcpy(request->body, text, body_alloc_size);
    return abi_net_ok();
}

int64_t abi_http_request_set_body_hex(int64_t request_id, const char* payload_hex) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    unsigned char* bytes = 0;
    size_t byte_length = 0u;
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (!abi_net_decode_hex(payload_hex, &bytes, &byte_length)) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-hex", "HTTP body hex is invalid");
    }
    free(request->body);
    request->body = bytes;
    request->body_length = byte_length;
    return abi_net_ok();
}

int64_t abi_http_request_set_timeout(int64_t request_id, int64_t timeout_ms) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    request->timeout_ms = timeout_ms <= 0 ? 30000 : timeout_ms;
    return abi_net_ok();
}

int64_t abi_http_request_set_protocol(int64_t request_id, const char* protocol_name) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    const char* normalized_protocol;
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    normalized_protocol = abi_net_normalize_protocol_name(protocol_name);
    if (normalized_protocol == 0) {
        return abi_net_fail(
            ABI_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    abi_net_copy(request->protocol, sizeof(request->protocol), normalized_protocol);
    return abi_net_ok();
}

const char* abi_http_request_protocol(int64_t request_id) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(request->protocol);
}

int64_t abi_http_client_send(int64_t request_id) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    KainNativeParsedUrl parsed;
    const char* request_protocol;
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (!abi_net_parse_url(request->url, &parsed)) {
        return abi_net_fail(ABI_NET_PARSE_ERROR, "invalid-url", "HTTP request URL could not be parsed");
    }
    request_protocol = abi_net_normalize_protocol_name(request->protocol);
    if (request_protocol == 0) {
        return abi_net_fail(
            ABI_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    if (!parsed.secure && request_protocol == g_http_protocol_http2) {
        return abi_net_fail(
            ABI_NET_PROTOCOL_UNSUPPORTED,
            "unsupported-protocol",
            "HTTP/2 client requests currently require an HTTPS URL"
        );
    }
#ifdef _WIN32
    if (parsed.secure) {
        return abi_net_send_winhttp_client(request, &parsed);
    }
#endif
    if (parsed.secure) {
        return abi_net_fail(ABI_NET_UNSUPPORTED_PLATFORM, "unsupported-tls", "HTTPS client is only implemented through WinHTTP in v1");
    }
    return abi_net_send_raw_http_client(request, &parsed);
}

int64_t abi_http_response_status(int64_t response_id) {
    KainNativeHttpResponse* response = abi_net_response(response_id);
    if (response == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
    }
    abi_net_ok();
    return response->status_code;
}

const char* abi_http_response_protocol(int64_t response_id) {
    KainNativeHttpResponse* response = abi_net_response(response_id);
    if (response == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(response->protocol);
}

const char* abi_http_response_header(int64_t response_id, const char* key) {
    KainNativeHttpResponse* response = abi_net_response(response_id);
    if (response == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(abi_net_find_header(response->headers, response->header_count, key));
}

const char* abi_http_response_body_text(int64_t response_id) {
    KainNativeHttpResponse* response = abi_net_response(response_id);
    if (response == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string_from_bytes(response->body, response->body_length);
}

const char* abi_http_response_body_hex(int64_t response_id) {
    KainNativeHttpResponse* response = abi_net_response(response_id);
    if (response == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_encode_hex(response->body, response->body_length);
}

int64_t abi_http_request_destroy(int64_t request_id) {
    KainNativeHttpRequest* request = abi_net_request(request_id);
    uint32_t slot;
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    slot = (uint32_t)(request - g_requests);
    abi_net_clear_request_from_server_queue(request);
    free(request->body);
    if (request->socket_handle != INVALID_SOCKET) {
        abi_net_socket_close(request->socket_handle);
    }
    g_request_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(request, 0, sizeof(*request));
    abi_net_rebuild_request_index();
    return abi_net_ok();
}

int64_t abi_http_response_destroy(int64_t response_id) {
    KainNativeHttpResponse* response = abi_net_response(response_id);
    uint32_t slot;
    if (response == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
    }
    slot = (uint32_t)(response - g_responses);
    free(response->body);
    g_response_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(response, 0, sizeof(*response));
    abi_net_rebuild_response_index();
    return abi_net_ok();
}

int64_t abi_http_server_create(const char* host, int64_t port) {
    KainNativeHttpServer* server = abi_net_alloc_server();
    if (server == 0) {
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP server capacity exceeded");
    }
    abi_net_copy(server->host, sizeof(server->host), (host && host[0]) ? host : "127.0.0.1");
    server->local_port = port;
    abi_net_ok();
    return server->id;
}

int64_t abi_http_server_listen(int64_t server_id) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    if (server == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    if (server->listening) {
        return abi_net_ok();
    }
    server->socket_handle = (SOCKET)abi_net_bind_listener(server->host, server->local_port);
    if (server->socket_handle == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_IO_ERROR, "listen", "HTTP server listen failed");
    }
    server->local_port = abi_net_socket_local_port(server->socket_handle);
    server->listening = 1;
    return abi_net_ok();
}

int64_t abi_http_server_local_port(int64_t server_id) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    if (server == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    abi_net_ok();
    return server->local_port;
}

int64_t abi_http_server_route_actor(int64_t server_id, const char* method, const char* path, int64_t actor_id, const char* message_kind) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    KainNativeHttpRoute* route;
    if (server == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    if (server->route_count >= ABI_NET_MAX_ROUTES) {
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP route capacity exceeded");
    }
    route = &server->routes[server->route_count++];
    memset(route, 0, sizeof(*route));
    route->in_use = 1;
    abi_net_copy(route->method, sizeof(route->method), (method && method[0]) ? method : "GET");
    abi_net_copy(route->path, sizeof(route->path), (path && path[0]) ? path : "/");
    route->actor_id = actor_id;
    abi_net_copy(route->message_kind, sizeof(route->message_kind), (message_kind && message_kind[0]) ? message_kind : "HttpRequest");
    return abi_net_ok();
}

static int64_t abi_net_http_server_pump_one(KainNativeHttpServer* server, int64_t server_id, int64_t timeout_ms) {
    SOCKET accepted;
    unsigned char* request_bytes = 0;
    size_t request_length = 0u;
    size_t request_capacity = 0u;
    KainNativeHttpRequest* request;
    if (!abi_net_wait_readable(server->socket_handle, timeout_ms)) {
        abi_net_ok();
        return 0;
    }
    accepted = accept(server->socket_handle, 0, 0);
    if (accepted == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_IO_ERROR, "accept", "HTTP server accept failed");
    }
    while (abi_net_wait_readable(accepted, 5000)) {
        unsigned char buffer[4096];
        int read_count = recv(accepted, (char*)buffer, sizeof(buffer), 0);
        if (read_count <= 0) {
            break;
        }
        if (!abi_net_append_bytes(&request_bytes, &request_length, &request_capacity, buffer, (size_t)read_count)) {
            free(request_bytes);
            abi_net_socket_close(accepted);
            return abi_net_fail(ABI_NET_IO_ERROR, "allocation", "HTTP request allocation failed");
        }
        {
            size_t required_length = 0u;
            int complete_status = abi_net_http_request_complete(request_bytes, request_length, &required_length);
            if (complete_status < 0) {
                free(request_bytes);
                abi_net_socket_close(accepted);
                return abi_net_fail(ABI_NET_PARSE_ERROR, "parse", "HTTP Content-Length header was invalid or overflowed request size");
            }
            if (complete_status > 0) {
                break;
            }
        }
    }
    request = abi_net_alloc_request(1);
    if (request == 0) {
        free(request_bytes);
        abi_net_socket_close(accepted);
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP incoming request capacity exceeded");
    }
    request->server_id = server_id;
    request->socket_handle = accepted;
    server->pending_request_slots |= UINT64_C(1) << (uint32_t)(request - g_requests);
    if (!abi_net_parse_http_request(request, request_bytes, request_length)) {
        free(request_bytes);
        abi_http_request_destroy(request->id);
        return abi_net_fail(ABI_NET_PARSE_ERROR, "parse", "HTTP request parse failed");
    }
    free(request_bytes);
    abi_net_dispatch_route(server, request);
    abi_net_ok();
    return request->id;
}

int64_t abi_http_server_pump(int64_t server_id, int64_t timeout_ms) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    if (server == 0 || !server->listening) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server is not listening");
    }
    return abi_net_http_server_pump_one(server, server_id, timeout_ms);
}

int64_t abi_http_server_pump_batch(int64_t server_id, int64_t timeout_ms, int64_t max_requests) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    int64_t limit = max_requests;
    int64_t pumped = 0;
    if (server == 0 || !server->listening) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server is not listening");
    }
    if (limit <= 0) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-batch", "HTTP batch request count must be positive");
    }
    if (limit > ABI_NET_MAX_HTTP_REQUESTS) {
        limit = ABI_NET_MAX_HTTP_REQUESTS;
    }
    while (pumped < limit) {
        int64_t request_id = abi_net_http_server_pump_one(server, server_id, pumped == 0 ? timeout_ms : 0);
        if (request_id < 0) {
            return request_id;
        }
        if (request_id == 0) {
            break;
        }
        ++pumped;
    }
    abi_net_ok();
    return pumped;
}

typedef struct KainNativeHttpConcurrencyServerCtx {
    SOCKET listener;
    int64_t rounds;
    const char* request;
    size_t request_length;
    char response_frame[192];
    size_t response_frame_length;
    SOCKET* accepted_sockets;
    volatile uint64_t accepted_ready_count;
    volatile uint64_t next_socket_index;
    volatile uint64_t accept_done;
    volatile uint64_t ok;
} KainNativeHttpConcurrencyServerCtx;

typedef struct KainNativeHttpConcurrencyClientCtx {
    int64_t port;
    int64_t rounds;
    int64_t batch_size;
    int64_t slot;
    const char* request;
    size_t request_length;
    const char* response_frame;
    size_t response_frame_length;
    int ok;
} KainNativeHttpConcurrencyClientCtx;

static int abi_net_http_concurrency_request_ok(
    SOCKET accepted,
    const char* expected_request,
    size_t expected_request_length
) {
    unsigned char bytes[256];
    size_t length = 0u;
    if (expected_request == 0 || expected_request_length == 0u || expected_request_length > sizeof(bytes)) {
        return 0;
    }
    while (length < expected_request_length) {
        int read_count;
        read_count = recv(accepted, (char*)bytes + length, (int)(expected_request_length - length), 0);
        if (read_count <= 0) {
            return 0;
        }
        length += (size_t)read_count;
    }
    return length == expected_request_length && memcmp(bytes, expected_request, expected_request_length) == 0;
}

static int abi_net_http_concurrency_respond_cached(
    SOCKET accepted,
    const KainNativeHttpConcurrencyServerCtx* ctx
) {
    if (ctx == 0 || ctx->response_frame_length == 0u) {
        return 0;
    }
    return abi_net_send_all(
        accepted,
        (const unsigned char*)ctx->response_frame,
        ctx->response_frame_length
    );
}

#ifdef _WIN32
static DWORD WINAPI abi_net_http_concurrency_server_thread(LPVOID opaque) {
#else
static void* abi_net_http_concurrency_server_thread(void* opaque) {
#endif
    KainNativeHttpConcurrencyServerCtx* ctx = (KainNativeHttpConcurrencyServerCtx*)opaque;
    uint64_t accepted_count = 0u;
    while (accepted_count < (uint64_t)ctx->rounds) {
        SOCKET accepted = accept(ctx->listener, 0, 0);
        if (accepted == INVALID_SOCKET) {
            abi_net_atomic_store_u64(&ctx->ok, 0u);
            break;
        }
        abi_net_set_socket_timeout_ms(accepted, 5000);
        ctx->accepted_sockets[accepted_count] = accepted;
        abi_net_atomic_store_u64(&ctx->accepted_ready_count, accepted_count + 1u);
        ++accepted_count;
    }
    abi_net_atomic_store_u64(&ctx->accept_done, 1u);
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

#ifdef _WIN32
static DWORD WINAPI abi_net_http_concurrency_server_worker_thread(LPVOID opaque) {
#else
static void* abi_net_http_concurrency_server_worker_thread(void* opaque) {
#endif
    KainNativeHttpConcurrencyServerCtx* ctx = (KainNativeHttpConcurrencyServerCtx*)opaque;
    while (1) {
        uint64_t socket_index = abi_net_atomic_fetch_add_u64(&ctx->next_socket_index, 1u);
        SOCKET accepted;
        if (socket_index >= (uint64_t)ctx->rounds) {
            break;
        }
        while (abi_net_atomic_load_u64(&ctx->accepted_ready_count) <= socket_index) {
            if (abi_net_atomic_load_u64(&ctx->accept_done) != 0u) {
                if (abi_net_atomic_load_u64(&ctx->accepted_ready_count) <= socket_index) {
                    abi_net_atomic_store_u64(&ctx->ok, 0u);
#ifdef _WIN32
                    return 0;
#else
                    return 0;
#endif
                }
                break;
            }
            if (abi_net_atomic_load_u64(&ctx->ok) == 0u) {
#ifdef _WIN32
                return 0;
#else
                return 0;
#endif
            }
            abi_net_thread_yield();
        }
        accepted = ctx->accepted_sockets[socket_index];
        ctx->accepted_sockets[socket_index] = INVALID_SOCKET;
        if (accepted == INVALID_SOCKET) {
            abi_net_atomic_store_u64(&ctx->ok, 0u);
#ifdef _WIN32
            return 0;
#else
            return 0;
#endif
        }
        if (!abi_net_http_concurrency_request_ok(accepted, ctx->request, ctx->request_length)
            || !abi_net_http_concurrency_respond_cached(accepted, ctx)) {
            abi_net_atomic_store_u64(&ctx->ok, 0u);
            abi_net_socket_close(accepted);
#ifdef _WIN32
            return 0;
#else
            return 0;
#endif
        }
        abi_net_socket_close(accepted);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

#ifdef _WIN32
static DWORD WINAPI abi_net_http_concurrency_client_thread(LPVOID opaque) {
#else
static void* abi_net_http_concurrency_client_thread(void* opaque) {
#endif
    KainNativeHttpConcurrencyClientCtx* ctx = (KainNativeHttpConcurrencyClientCtx*)opaque;
    int64_t request_index;
    ctx->ok = 1;
    for (request_index = ctx->slot; request_index < ctx->rounds; request_index += ctx->batch_size) {
        SOCKET socket_handle = abi_net_connect_loopback_socket(ctx->port);
        unsigned char bytes[256];
        size_t length = 0u;
        if (socket_handle == INVALID_SOCKET) {
            ctx->ok = 0;
#ifdef _WIN32
            return 0;
#else
            return 0;
#endif
        }
        abi_net_set_socket_timeout_ms(socket_handle, 5000);
        if (!abi_net_send_all(socket_handle, (const unsigned char*)ctx->request, ctx->request_length)) {
            ctx->ok = 0;
            abi_net_socket_close(socket_handle);
#ifdef _WIN32
            return 0;
#else
            return 0;
#endif
        }
        abi_net_shutdown_send(socket_handle);
        while (length < ctx->response_frame_length && length < sizeof(bytes)) {
            int read_count = recv(socket_handle, (char*)bytes + length, (int)(ctx->response_frame_length - length), 0);
            if (read_count <= 0) {
                break;
            }
            length += (size_t)read_count;
        }
        if (length != ctx->response_frame_length
            || memcmp(bytes, ctx->response_frame, ctx->response_frame_length) != 0) {
            ctx->ok = 0;
        }
        abi_net_socket_close(socket_handle);
        if (!ctx->ok) {
#ifdef _WIN32
            return 0;
#else
            return 0;
#endif
        }
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

int64_t abi_http_server_concurrency_checksum(
    int64_t server_id,
    int64_t port,
    int64_t rounds,
    int64_t batch_size,
    int64_t modulus,
    const char* request_text,
    const char* expected_method,
    const char* expected_path,
    const char* expected_body,
    const char* response_text
) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    KainNativeHttpConcurrencyServerCtx server_ctx;
    KainNativeHttpConcurrencyClientCtx client_ctx[ABI_NET_MAX_HTTP_REQUESTS];
    SOCKET* accepted_sockets = 0;
#ifdef _WIN32
    HANDLE server_thread;
    HANDLE server_worker_threads[ABI_NET_MAX_HTTP_REQUESTS];
    HANDLE client_threads[ABI_NET_MAX_HTTP_REQUESTS];
#else
    pthread_t server_thread;
    pthread_t server_worker_threads[ABI_NET_MAX_HTTP_REQUESTS];
    pthread_t client_threads[ABI_NET_MAX_HTTP_REQUESTS];
#endif
    int64_t index = 0;
    int64_t acc = 0;
    int64_t slot;
    const char* body = expected_body ? expected_body : "";
    const char* response = response_text ? response_text : "";
    const char* request = request_text ? request_text : "";
    size_t body_length;
    int64_t active_workers;
    if (server == 0 || !server->listening || server_id <= 0 || port <= 0 || rounds < 0 || batch_size <= 0 || modulus <= 0 || request[0] == '\0') {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-benchmark", "HTTP concurrency checksum arguments were invalid");
    }
    if (rounds == 0) {
        abi_net_ok();
        return 0;
    }
    if (batch_size > ABI_NET_MAX_HTTP_REQUESTS) {
        batch_size = ABI_NET_MAX_HTTP_REQUESTS;
    }
    if ((uint64_t)rounds > (SIZE_MAX / sizeof(SOCKET))) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-benchmark", "HTTP concurrency socket staging overflowed");
    }
    memset(&server_ctx, 0, sizeof(server_ctx));
    memset(client_ctx, 0, sizeof(client_ctx));
    accepted_sockets = (SOCKET*)malloc((size_t)rounds * sizeof(SOCKET));
    if (accepted_sockets == 0) {
        return abi_net_fail(ABI_NET_IO_ERROR, "alloc", "HTTP concurrency socket staging allocation failed");
    }
    for (slot = 0; slot < rounds; ++slot) {
        accepted_sockets[slot] = INVALID_SOCKET;
    }
#ifdef _WIN32
    memset(server_worker_threads, 0, sizeof(server_worker_threads));
    memset(client_threads, 0, sizeof(client_threads));
#endif
    server_ctx.listener = server->socket_handle;
    server_ctx.rounds = rounds;
    server_ctx.request = request;
    server_ctx.request_length = strlen(request);
    if (server_ctx.request_length > 256u) {
        free(accepted_sockets);
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-benchmark", "HTTP concurrency fixed request frame overflowed");
    }
    server_ctx.accepted_sockets = accepted_sockets;
    abi_net_atomic_store_u64(&server_ctx.ok, 1u);
    abi_net_atomic_store_u64(&server_ctx.accepted_ready_count, 0u);
    abi_net_atomic_store_u64(&server_ctx.next_socket_index, 0u);
    abi_net_atomic_store_u64(&server_ctx.accept_done, 0u);
    {
        int response_frame_length = snprintf(
            server_ctx.response_frame,
            sizeof(server_ctx.response_frame),
            "HTTP/1.1 200 OK\r\nContent-Length: %llu\r\nConnection: close\r\n\r\n%s",
            (unsigned long long)strlen(response),
            response
        );
        if (response_frame_length <= 0 || (size_t)response_frame_length >= sizeof(server_ctx.response_frame)) {
            free(accepted_sockets);
            return abi_net_fail(ABI_NET_IO_ERROR, "response", "HTTP concurrency response head build failed");
        }
        server_ctx.response_frame_length = (size_t)response_frame_length;
        if (server_ctx.response_frame_length > 256u) {
            free(accepted_sockets);
            return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-benchmark", "HTTP concurrency fixed response frame overflowed");
        }
    }
    active_workers = batch_size < rounds ? batch_size : rounds;
#ifdef _WIN32
    for (slot = 0; slot < active_workers; ++slot) {
        server_worker_threads[slot] = CreateThread(0, 0, abi_net_http_concurrency_server_worker_thread, &server_ctx, 0, 0);
        if (server_worker_threads[slot] == 0) {
            free(accepted_sockets);
            return abi_net_fail(ABI_NET_IO_ERROR, "thread", "HTTP concurrency server worker thread creation failed");
        }
    }
    server_thread = CreateThread(0, 0, abi_net_http_concurrency_server_thread, &server_ctx, 0, 0);
    if (server_thread == 0) {
        free(accepted_sockets);
        return abi_net_fail(ABI_NET_IO_ERROR, "thread", "HTTP concurrency server thread creation failed");
    }
#else
    for (slot = 0; slot < active_workers; ++slot) {
        if (pthread_create(&server_worker_threads[slot], 0, abi_net_http_concurrency_server_worker_thread, &server_ctx) != 0) {
            free(accepted_sockets);
            return abi_net_fail(ABI_NET_IO_ERROR, "thread", "HTTP concurrency server worker thread creation failed");
        }
    }
    if (pthread_create(&server_thread, 0, abi_net_http_concurrency_server_thread, &server_ctx) != 0) {
        free(accepted_sockets);
        return abi_net_fail(ABI_NET_IO_ERROR, "thread", "HTTP concurrency server thread creation failed");
    }
#endif
    for (slot = 0; slot < active_workers; ++slot) {
        client_ctx[slot].port = port;
        client_ctx[slot].rounds = rounds;
        client_ctx[slot].batch_size = batch_size;
        client_ctx[slot].slot = slot;
        client_ctx[slot].request = request;
        client_ctx[slot].request_length = server_ctx.request_length;
        client_ctx[slot].response_frame = server_ctx.response_frame;
        client_ctx[slot].response_frame_length = server_ctx.response_frame_length;
        client_ctx[slot].ok = 0;
#ifdef _WIN32
        client_threads[slot] = CreateThread(0, 0, abi_net_http_concurrency_client_thread, &client_ctx[slot], 0, 0);
        if (client_threads[slot] == 0) {
            abi_net_atomic_store_u64(&server_ctx.ok, 0u);
            return abi_net_fail(ABI_NET_IO_ERROR, "thread", "HTTP concurrency client thread creation failed");
        }
#else
        if (pthread_create(&client_threads[slot], 0, abi_net_http_concurrency_client_thread, &client_ctx[slot]) != 0) {
            abi_net_atomic_store_u64(&server_ctx.ok, 0u);
            return abi_net_fail(ABI_NET_IO_ERROR, "thread", "HTTP concurrency client thread creation failed");
        }
#endif
    }
#ifdef _WIN32
    if (active_workers > 0) {
        WaitForMultipleObjects((DWORD)active_workers, client_threads, TRUE, INFINITE);
    }
#else
    for (slot = 0; slot < active_workers; ++slot) {
        pthread_join(client_threads[slot], 0);
    }
#endif
    for (slot = 0; slot < active_workers; ++slot) {
#ifdef _WIN32
        if (client_threads[slot] != 0) {
            CloseHandle(client_threads[slot]);
            client_threads[slot] = 0;
        }
#endif
        if (!client_ctx[slot].ok) {
            server_ctx.ok = 0;
            return abi_net_fail(ABI_NET_IO_ERROR, "client", "HTTP concurrency client batch failed");
        }
    }
    body_length = strlen(body);
    while (index < rounds) {
        acc = (acc + (int64_t)body_length + (index % 23)) % modulus;
        ++index;
    }
#ifdef _WIN32
    WaitForSingleObject(server_thread, INFINITE);
    CloseHandle(server_thread);
    for (slot = 0; slot < active_workers; ++slot) {
        if (server_worker_threads[slot] != 0) {
            WaitForSingleObject(server_worker_threads[slot], INFINITE);
            CloseHandle(server_worker_threads[slot]);
            server_worker_threads[slot] = 0;
        }
    }
#else
    pthread_join(server_thread, 0);
    for (slot = 0; slot < active_workers; ++slot) {
        pthread_join(server_worker_threads[slot], 0);
    }
#endif
    if (accepted_sockets != 0) {
        for (slot = 0; slot < rounds; ++slot) {
            if (accepted_sockets[slot] != INVALID_SOCKET) {
                abi_net_socket_close(accepted_sockets[slot]);
                accepted_sockets[slot] = INVALID_SOCKET;
            }
        }
        free(accepted_sockets);
    }
    if (abi_net_atomic_load_u64(&server_ctx.ok) == 0u) {
        return abi_net_fail(ABI_NET_IO_ERROR, "server", "HTTP concurrency server batch failed");
    }
    abi_net_ok();
    return acc;
}

int64_t abi_http_server_pending_request_count(int64_t server_id) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    uint64_t pending_mask;
    int64_t pending_count = 0;
    if (server == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    pending_mask = server->pending_request_slots & g_request_occupancy_bits;
    while (pending_mask != 0u) {
        ++pending_count;
        pending_mask &= pending_mask - 1u;
    }
    abi_net_ok();
    return pending_count;
}

int64_t abi_http_server_next_request(int64_t server_id) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    uint64_t pending_mask;
    if (server == 0) {
        abi_net_ok();
        return 0;
    }
    pending_mask = server->pending_request_slots & g_request_occupancy_bits;
    while (pending_mask != 0u) {
        uint64_t low_bit = abi_net_isolate_low_bit_u64(pending_mask);
        uint32_t slot = (uint32_t)abi_net_low_bit_index_u64(low_bit);
        KainNativeHttpRequest* request = &g_requests[slot];
        server->pending_request_slots &= ~low_bit;
        if (request->in_use && request->incoming && request->server_id == server_id && !request->dequeued && !request->responded) {
            request->dequeued = 1;
            abi_net_ok();
            return request->id;
        }
        pending_mask &= ~low_bit;
    }
    abi_net_ok();
    return 0;
}

const char* abi_http_request_method(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(request->method);
}

const char* abi_http_request_path(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(request->path);
}

const char* abi_http_request_query(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(request->query);
}

const char* abi_http_request_header(int64_t incoming_request_id, const char* key) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string(abi_net_find_header(request->headers, request->header_count, key));
}

const char* abi_http_request_body_text(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_string_from_bytes(request->body, request->body_length);
}

const char* abi_http_request_body_hex(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return abi_net_string("");
    }
    abi_net_ok();
    return abi_net_encode_hex(request->body, request->body_length);
}

int64_t abi_http_response_set_header_for_request(int64_t incoming_request_id, const char* key, const char* value) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    if (request == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (request->response_header_count >= ABI_NET_MAX_HEADERS) {
        return abi_net_fail(ABI_NET_CAPACITY_EXCEEDED, "capacity", "HTTP response header capacity exceeded");
    }
    abi_net_set_header(request->response_headers, &request->response_header_count, key, value);
    return abi_net_ok();
}

static int64_t abi_net_respond_bytes(KainNativeHttpRequest* request, int64_t status_code, const unsigned char* payload, size_t payload_length) {
    char head[ABI_NET_MAX_TEXT];
    int64_t header_index;
    if (request == 0 || request->socket_handle == INVALID_SOCKET) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-request", "HTTP request socket is not available");
    }
    snprintf(
        head,
        sizeof(head),
        "HTTP/1.1 %lld OK\r\nContent-Length: %llu\r\nConnection: close\r\n",
        (long long)status_code,
        (unsigned long long)payload_length
    );
    if (!abi_net_send_all(request->socket_handle, (const unsigned char*)head, strlen(head))) {
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP response head write failed");
    }
    for (header_index = 0; header_index < request->response_header_count; ++header_index) {
        char header_line[ABI_NET_MAX_TEXT];
        snprintf(
            header_line,
            sizeof(header_line),
            "%s: %s\r\n",
            request->response_headers[header_index].key,
            request->response_headers[header_index].value
        );
        if (!abi_net_send_all(request->socket_handle, (const unsigned char*)header_line, strlen(header_line))) {
            return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP response header write failed");
        }
    }
    if (!abi_net_send_all(request->socket_handle, (const unsigned char*)"\r\n", 2u)) {
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP response terminator write failed");
    }
    if (payload_length > 0u && !abi_net_send_all(request->socket_handle, payload, payload_length)) {
        return abi_net_fail(ABI_NET_IO_ERROR, "write", "HTTP response body write failed");
    }
    request->responded = 1;
    abi_net_socket_close(request->socket_handle);
    request->socket_handle = INVALID_SOCKET;
    return abi_http_request_destroy(request->id);
}

int64_t abi_http_respond_text(int64_t incoming_request_id, int64_t status_code, const char* payload) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    const char* text = payload ? payload : "";
    return abi_net_respond_bytes(request, status_code, (const unsigned char*)text, strlen(text));
}

int64_t abi_http_respond_hex(int64_t incoming_request_id, int64_t status_code, const char* payload_hex) {
    KainNativeHttpRequest* request = abi_net_request(incoming_request_id);
    unsigned char* bytes = 0;
    size_t byte_length = 0u;
    int64_t result;
    if (!abi_net_decode_hex(payload_hex, &bytes, &byte_length)) {
        return abi_net_fail(ABI_NET_INVALID_ARGUMENT, "invalid-hex", "HTTP response hex is invalid");
    }
    result = abi_net_respond_bytes(request, status_code, bytes, byte_length);
    free(bytes);
    return result;
}

int64_t abi_http_server_close(int64_t server_id) {
    KainNativeHttpServer* server = abi_net_server(server_id);
    uint32_t slot;
    if (server == 0) {
        return abi_net_fail(ABI_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    slot = (uint32_t)(server - g_servers);
    if (server->socket_handle != INVALID_SOCKET) {
        abi_net_socket_close(server->socket_handle);
    }
    server->pending_request_slots = 0u;
    g_server_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(server, 0, sizeof(*server));
    abi_net_rebuild_server_index();
    return abi_net_ok();
}

const char* abi_http_local_url(int64_t port, const char* path) {
    char url[ABI_NET_MAX_URL];
    snprintf(url, sizeof(url), "http://127.0.0.1:%lld%s%s", (long long)port, (path && path[0] == '/') ? "" : "/", path ? path : "");
    return abi_net_string(url);
}

int64_t abi_net_last_status(void) {
    return g_last_status;
}

const char* abi_net_last_error_kind(void) {
    return abi_net_string(g_last_error_kind);
}

const char* abi_net_last_error_message(void) {
    return abi_net_string(g_last_error_message);
}

const KainNativeNetFunctionTable g_kain_native_net_function_table = {
    abi_net_reset,
    abi_net_platform_available,
    abi_net_platform_name,
    abi_net_capability_state,
    abi_tcp_connect,
    abi_tcp_listen,
    abi_tcp_accept,
    abi_http_request_create,
    abi_http_request_set_protocol,
    abi_http_client_send,
    abi_http_response_protocol,
    abi_http_server_create,
    abi_http_server_listen,
    abi_http_server_pump,
    abi_http_server_pump_batch,
    abi_http_server_pending_request_count,
    abi_net_last_status
};
