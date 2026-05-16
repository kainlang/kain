#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/kain_native_net_system.h"
#include "../../include/kain_runtime_actor.h"
#include "../../include/kain_runtime_base.h"

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
#include <strings.h>
#endif

void* kain_alloc_rc(size_t size, long long type_tag);

typedef struct KainNativeNetHeader {
    int in_use;
    char key[KAIN_NATIVE_NET_MAX_KEY];
    char value[KAIN_NATIVE_NET_MAX_TEXT];
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
    char method[KAIN_NATIVE_NET_MAX_KEY];
    char url[KAIN_NATIVE_NET_MAX_URL];
    char path[KAIN_NATIVE_NET_MAX_URL];
    char query[KAIN_NATIVE_NET_MAX_URL];
    KainNativeNetHeader headers[KAIN_NATIVE_NET_MAX_HEADERS];
    int64_t header_count;
    KainNativeNetHeader response_headers[KAIN_NATIVE_NET_MAX_HEADERS];
    int64_t response_header_count;
    unsigned char* body;
    size_t body_length;
    int64_t timeout_ms;
    int64_t actor_id;
    char actor_message_kind[KAIN_NATIVE_NET_MAX_KEY];
    char actor_payload[KAIN_NATIVE_NET_MAX_TEXT];
    char protocol[KAIN_NATIVE_NET_MAX_KEY];
} KainNativeHttpRequest;

typedef struct KainNativeHttpResponse {
    int in_use;
    int64_t id;
    int64_t status_code;
    char protocol[KAIN_NATIVE_NET_MAX_KEY];
    KainNativeNetHeader headers[KAIN_NATIVE_NET_MAX_HEADERS];
    int64_t header_count;
    unsigned char* body;
    size_t body_length;
} KainNativeHttpResponse;

typedef struct KainNativeHttpRoute {
    int in_use;
    char method[KAIN_NATIVE_NET_MAX_KEY];
    char path[KAIN_NATIVE_NET_MAX_URL];
    int64_t actor_id;
    char message_kind[KAIN_NATIVE_NET_MAX_KEY];
} KainNativeHttpRoute;

typedef struct KainNativeHttpServer {
    int in_use;
    int listening;
    int64_t id;
    SOCKET socket_handle;
    int64_t local_port;
    char host[KAIN_NATIVE_NET_MAX_KEY];
    KainNativeHttpRoute routes[KAIN_NATIVE_NET_MAX_ROUTES];
    int64_t route_count;
    uint64_t pending_request_slots;
} KainNativeHttpServer;

typedef struct KainNativeParsedUrl {
    char scheme[16];
    char host[256];
    char path[KAIN_NATIVE_NET_MAX_URL];
    int64_t port;
    int secure;
} KainNativeParsedUrl;

static KainNativeTcpConnection g_connections[KAIN_NATIVE_NET_MAX_CONNECTIONS];
static KainNativeTcpListener g_listeners[KAIN_NATIVE_NET_MAX_LISTENERS];
static KainNativeHttpRequest g_requests[KAIN_NATIVE_NET_MAX_HTTP_REQUESTS];
static KainNativeHttpResponse g_responses[KAIN_NATIVE_NET_MAX_HTTP_RESPONSES];
static KainNativeHttpServer g_servers[KAIN_NATIVE_NET_MAX_HTTP_SERVERS];
static uint64_t g_connection_occupancy_bits = 0u;
static uint64_t g_listener_occupancy_bits = 0u;
static uint64_t g_request_occupancy_bits = 0u;
static uint64_t g_response_occupancy_bits = 0u;
static uint64_t g_server_occupancy_bits = 0u;
#define KAIN_NATIVE_NET_CONNECTION_VALID_MASK UINT64_MAX
#define KAIN_NATIVE_NET_LISTENER_VALID_MASK UINT64_C(0x000000000000ffff)
#define KAIN_NATIVE_NET_REQUEST_VALID_MASK UINT64_MAX
#define KAIN_NATIVE_NET_RESPONSE_VALID_MASK UINT64_MAX
#define KAIN_NATIVE_NET_SERVER_VALID_MASK UINT64_C(0x000000000000ffff)
#define KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY 128u
#define KAIN_NATIVE_NET_CONNECTION_INDEX_MASK (KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY - 1u)
#define KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY 32u
#define KAIN_NATIVE_NET_LISTENER_INDEX_MASK (KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY - 1u)
#define KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY 128u
#define KAIN_NATIVE_NET_REQUEST_INDEX_MASK (KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY - 1u)
#define KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY 128u
#define KAIN_NATIVE_NET_RESPONSE_INDEX_MASK (KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY - 1u)
#define KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY 32u
#define KAIN_NATIVE_NET_SERVER_INDEX_MASK (KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY - 1u)
#if (KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY & KAIN_NATIVE_NET_CONNECTION_INDEX_MASK) != 0
#error "KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY & KAIN_NATIVE_NET_LISTENER_INDEX_MASK) != 0
#error "KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY & KAIN_NATIVE_NET_REQUEST_INDEX_MASK) != 0
#error "KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY & KAIN_NATIVE_NET_RESPONSE_INDEX_MASK) != 0
#error "KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY & KAIN_NATIVE_NET_SERVER_INDEX_MASK) != 0
#error "KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY must be a power of two for masked probing."
#endif
static uint32_t g_connection_index[KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY];
static uint32_t g_listener_index[KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY];
static uint32_t g_request_index[KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY];
static uint32_t g_response_index[KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY];
static uint32_t g_server_index[KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY];
static int64_t g_next_connection_id = 1;
static int64_t g_next_listener_id = 1;
static int64_t g_next_request_id = 1;
static int64_t g_next_response_id = 1;
static int64_t g_next_server_id = 1;
static int64_t g_last_status = KAIN_NATIVE_NET_OK;
static char g_last_error_kind[KAIN_NATIVE_NET_MAX_KEY] = "ok";
static char g_last_error_message[KAIN_NATIVE_NET_MAX_TEXT] = "";
static const char g_empty_string[] = "";
static const char g_http_protocol_http11[] = "http/1.1";
static const char g_http_protocol_http2[] = "http/2";

static int kain_native_net_size_add_overflow(size_t left, size_t right, size_t* out) {
    if (out == 0) {
        return 1;
    }
    if (right > (SIZE_MAX - left)) {
        return 1;
    }
    *out = left + right;
    return 0;
}

static int kain_native_net_parse_content_length_header(const char* text, size_t* out_length) {
    char* end = 0;
    unsigned long long parsed = 0;

    if (out_length == 0 || text == 0) {
        return 0;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

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

/*
 * Proofs:
 * - runtime/native/src/core/z3/proofs-experimental/net-handle-index-probe-bounds.smt2
 * - runtime/native/src/core/z3/proofs-experimental/actor-table-debruijn-hash-distinct.smt2
 *
 * The solver owns two trust boundaries here: masked probe indices must remain
 * inside each sidecar handle table, and the de Bruijn low-bit decode is shared
 * with the already-proved actor occupancy path.
 */
static uint64_t kain_native_net_mix_id(int64_t id) {
    uint64_t x = (uint64_t)id;
    x ^= x >> 30u;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27u;
    x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31u;
    return x;
}

static uint64_t kain_native_net_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int kain_native_net_low_bit_index_u64(uint64_t one_hot) {
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

static uint32_t kain_native_net_index_start_slot(int64_t id, uint32_t mask) {
    return (uint32_t)(kain_native_net_mix_id(id) & mask);
}

static int kain_native_net_index_insert(
    uint32_t* index_table,
    uint32_t index_capacity,
    uint32_t index_mask,
    int64_t id,
    uint32_t slot
) {
    uint32_t start_index = kain_native_net_index_start_slot(id, index_mask);
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

static int kain_native_net_find_free_slot_u64(uint64_t occupancy_bits, uint64_t valid_mask, uint32_t* out_slot) {
    uint64_t free_mask = (~occupancy_bits) & valid_mask;
    if (out_slot == 0 || free_mask == 0u) {
        return 0;
    }
    *out_slot = (uint32_t)kain_native_net_low_bit_index_u64(
        kain_native_net_isolate_low_bit_u64(free_mask)
    );
    return 1;
}

static void kain_native_net_rebuild_connection_index(void) {
    uint32_t slot;
    memset(g_connection_index, 0, sizeof(g_connection_index));
    for (slot = 0u; slot < KAIN_NATIVE_NET_MAX_CONNECTIONS; ++slot) {
        if (g_connections[slot].in_use) {
            (void)kain_native_net_index_insert(
                g_connection_index,
                KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY,
                KAIN_NATIVE_NET_CONNECTION_INDEX_MASK,
                g_connections[slot].id,
                slot
            );
        }
    }
}

static void kain_native_net_rebuild_listener_index(void) {
    uint32_t slot;
    memset(g_listener_index, 0, sizeof(g_listener_index));
    for (slot = 0u; slot < KAIN_NATIVE_NET_MAX_LISTENERS; ++slot) {
        if (g_listeners[slot].in_use) {
            (void)kain_native_net_index_insert(
                g_listener_index,
                KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY,
                KAIN_NATIVE_NET_LISTENER_INDEX_MASK,
                g_listeners[slot].id,
                slot
            );
        }
    }
}

static void kain_native_net_rebuild_request_index(void) {
    uint32_t slot;
    memset(g_request_index, 0, sizeof(g_request_index));
    for (slot = 0u; slot < KAIN_NATIVE_NET_MAX_HTTP_REQUESTS; ++slot) {
        if (g_requests[slot].in_use) {
            (void)kain_native_net_index_insert(
                g_request_index,
                KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY,
                KAIN_NATIVE_NET_REQUEST_INDEX_MASK,
                g_requests[slot].id,
                slot
            );
        }
    }
}

static void kain_native_net_rebuild_response_index(void) {
    uint32_t slot;
    memset(g_response_index, 0, sizeof(g_response_index));
    for (slot = 0u; slot < KAIN_NATIVE_NET_MAX_HTTP_RESPONSES; ++slot) {
        if (g_responses[slot].in_use) {
            (void)kain_native_net_index_insert(
                g_response_index,
                KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY,
                KAIN_NATIVE_NET_RESPONSE_INDEX_MASK,
                g_responses[slot].id,
                slot
            );
        }
    }
}

static void kain_native_net_rebuild_server_index(void) {
    uint32_t slot;
    memset(g_server_index, 0, sizeof(g_server_index));
    for (slot = 0u; slot < KAIN_NATIVE_NET_MAX_HTTP_SERVERS; ++slot) {
        if (g_servers[slot].in_use) {
            (void)kain_native_net_index_insert(
                g_server_index,
                KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY,
                KAIN_NATIVE_NET_SERVER_INDEX_MASK,
                g_servers[slot].id,
                slot
            );
        }
    }
}

#ifdef _WIN32
static void kain_native_net_init_winsock(void) {
    static int initialized = 0;
    if (!initialized) {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        initialized = 1;
    }
}
#endif

static void kain_native_net_copy(char* destination, size_t capacity, const char* source) {
    if (destination == 0 || capacity == 0u) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, capacity, "%s", source);
}

static int kain_native_net_text_equal_ci(const char* left, const char* right) {
    if (left == 0 || right == 0) {
        return 0;
    }
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return strcasecmp(left, right) == 0;
#endif
}

static const char* kain_native_net_normalize_protocol_name(const char* protocol_name) {
    if (protocol_name == 0 || protocol_name[0] == '\0') {
        return g_http_protocol_http11;
    }
    if (kain_native_net_text_equal_ci(protocol_name, "http/1.1") ||
        kain_native_net_text_equal_ci(protocol_name, "http1") ||
        kain_native_net_text_equal_ci(protocol_name, "http11")) {
        return g_http_protocol_http11;
    }
    if (kain_native_net_text_equal_ci(protocol_name, "http/2") ||
        kain_native_net_text_equal_ci(protocol_name, "http2") ||
        kain_native_net_text_equal_ci(protocol_name, "h2")) {
        return g_http_protocol_http2;
    }
    return 0;
}

static const char* kain_native_net_http_version_token_from_protocol(const char* protocol_name) {
    const char* normalized = kain_native_net_normalize_protocol_name(protocol_name);
    if (normalized == 0) {
        return 0;
    }
    if (normalized == g_http_protocol_http2) {
        return "HTTP/2";
    }
    return "HTTP/1.1";
}

static const char* kain_native_net_protocol_from_http_version_token(const char* version_token) {
    if (version_token == 0 || version_token[0] == '\0') {
        return g_http_protocol_http11;
    }
    if (kain_native_net_text_equal_ci(version_token, "HTTP/2") ||
        kain_native_net_text_equal_ci(version_token, "HTTP/2.0")) {
        return g_http_protocol_http2;
    }
    return g_http_protocol_http11;
}

static const char* kain_native_net_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static const char* kain_native_net_string_from_bytes(const unsigned char* bytes, size_t length) {
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
    return output;
}

static int64_t kain_native_net_ok(void) {
    g_last_status = KAIN_NATIVE_NET_OK;
    kain_native_net_copy(g_last_error_kind, sizeof(g_last_error_kind), "ok");
    g_last_error_message[0] = '\0';
    return KAIN_NATIVE_NET_OK;
}

static int64_t kain_native_net_fail(int64_t status, const char* kind, const char* message) {
    g_last_status = status;
    kain_native_net_copy(g_last_error_kind, sizeof(g_last_error_kind), kind ? kind : "error");
    kain_native_net_copy(g_last_error_message, sizeof(g_last_error_message), message ? message : "");
    return status;
}

static void kain_native_net_socket_close(SOCKET socket_handle) {
    if (socket_handle == INVALID_SOCKET) {
        return;
    }
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

static int kain_native_net_wait_readable(SOCKET socket_handle, int64_t timeout_ms) {
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

static int kain_native_net_send_all(SOCKET socket_handle, const unsigned char* bytes, size_t length) {
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

static int kain_native_net_hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int kain_native_net_decode_hex(const char* hex, unsigned char** out_bytes, size_t* out_length) {
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
        int high = kain_native_net_hex_value(hex[index]);
        int low = kain_native_net_hex_value(hex[index + 1u]);
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

static const char* kain_native_net_encode_hex(const unsigned char* bytes, size_t length) {
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

static int kain_native_net_append_bytes(unsigned char** buffer, size_t* length, size_t* capacity, const unsigned char* bytes, size_t byte_count) {
    unsigned char* resized;
    size_t needed;
    size_t next_capacity;
    if (byte_count == 0u) {
        return 1;
    }
    if (buffer == 0 || length == 0 || capacity == 0 || bytes == 0) {
        return 0;
    }
    if (kain_native_net_size_add_overflow(*length, byte_count, &needed) ||
        kain_native_net_size_add_overflow(needed, 1u, &needed)) {
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

static KainNativeTcpConnection* kain_native_net_connection(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = kain_native_net_index_start_slot(id, KAIN_NATIVE_NET_CONNECTION_INDEX_MASK);
    for (probe = 0u; probe < KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_NATIVE_NET_CONNECTION_INDEX_MASK;
        uint32_t encoded_slot = g_connection_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_NATIVE_NET_MAX_CONNECTIONS &&
            g_connections[slot].in_use &&
            g_connections[slot].id == id) {
            return &g_connections[slot];
        }
    }
    return 0;
}

static KainNativeTcpConnection* kain_native_net_alloc_connection(SOCKET socket_handle) {
    uint32_t slot;
    uint64_t bit;
    if (!kain_native_net_find_free_slot_u64(
            g_connection_occupancy_bits,
            KAIN_NATIVE_NET_CONNECTION_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_connections[slot], 0, sizeof(g_connections[slot]));
    g_connections[slot].in_use = 1;
    g_connections[slot].id = g_next_connection_id++;
    g_connections[slot].socket_handle = socket_handle;
    bit = UINT64_C(1) << slot;
    g_connection_occupancy_bits |= bit;
    if (!kain_native_net_index_insert(
            g_connection_index,
            KAIN_NATIVE_NET_CONNECTION_INDEX_CAPACITY,
            KAIN_NATIVE_NET_CONNECTION_INDEX_MASK,
            g_connections[slot].id,
            slot)) {
        g_connection_occupancy_bits &= ~bit;
        memset(&g_connections[slot], 0, sizeof(g_connections[slot]));
        return 0;
    }
    return &g_connections[slot];
}

static KainNativeTcpListener* kain_native_net_listener(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = kain_native_net_index_start_slot(id, KAIN_NATIVE_NET_LISTENER_INDEX_MASK);
    for (probe = 0u; probe < KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_NATIVE_NET_LISTENER_INDEX_MASK;
        uint32_t encoded_slot = g_listener_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_NATIVE_NET_MAX_LISTENERS &&
            g_listeners[slot].in_use &&
            g_listeners[slot].id == id) {
            return &g_listeners[slot];
        }
    }
    return 0;
}

static KainNativeTcpListener* kain_native_net_alloc_listener(SOCKET socket_handle, int64_t local_port) {
    uint32_t slot;
    uint64_t bit;
    if (!kain_native_net_find_free_slot_u64(
            g_listener_occupancy_bits,
            KAIN_NATIVE_NET_LISTENER_VALID_MASK,
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
    if (!kain_native_net_index_insert(
            g_listener_index,
            KAIN_NATIVE_NET_LISTENER_INDEX_CAPACITY,
            KAIN_NATIVE_NET_LISTENER_INDEX_MASK,
            g_listeners[slot].id,
            slot)) {
        g_listener_occupancy_bits &= ~bit;
        memset(&g_listeners[slot], 0, sizeof(g_listeners[slot]));
        return 0;
    }
    return &g_listeners[slot];
}

static KainNativeHttpRequest* kain_native_net_request(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = kain_native_net_index_start_slot(id, KAIN_NATIVE_NET_REQUEST_INDEX_MASK);
    for (probe = 0u; probe < KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_NATIVE_NET_REQUEST_INDEX_MASK;
        uint32_t encoded_slot = g_request_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_NATIVE_NET_MAX_HTTP_REQUESTS &&
            g_requests[slot].in_use &&
            g_requests[slot].id == id) {
            return &g_requests[slot];
        }
    }
    return 0;
}

static KainNativeHttpRequest* kain_native_net_alloc_request(int incoming) {
    uint32_t slot;
    uint64_t bit;
    if (!kain_native_net_find_free_slot_u64(
            g_request_occupancy_bits,
            KAIN_NATIVE_NET_REQUEST_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_requests[slot], 0, sizeof(g_requests[slot]));
    g_requests[slot].in_use = 1;
    g_requests[slot].incoming = incoming;
    g_requests[slot].id = g_next_request_id++;
    g_requests[slot].socket_handle = INVALID_SOCKET;
    g_requests[slot].timeout_ms = 30000;
    kain_native_net_copy(g_requests[slot].protocol, sizeof(g_requests[slot].protocol), g_http_protocol_http11);
    bit = UINT64_C(1) << slot;
    g_request_occupancy_bits |= bit;
    if (!kain_native_net_index_insert(
            g_request_index,
            KAIN_NATIVE_NET_REQUEST_INDEX_CAPACITY,
            KAIN_NATIVE_NET_REQUEST_INDEX_MASK,
            g_requests[slot].id,
            slot)) {
        g_request_occupancy_bits &= ~bit;
        memset(&g_requests[slot], 0, sizeof(g_requests[slot]));
        return 0;
    }
    return &g_requests[slot];
}

static KainNativeHttpResponse* kain_native_net_response(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = kain_native_net_index_start_slot(id, KAIN_NATIVE_NET_RESPONSE_INDEX_MASK);
    for (probe = 0u; probe < KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_NATIVE_NET_RESPONSE_INDEX_MASK;
        uint32_t encoded_slot = g_response_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_NATIVE_NET_MAX_HTTP_RESPONSES &&
            g_responses[slot].in_use &&
            g_responses[slot].id == id) {
            return &g_responses[slot];
        }
    }
    return 0;
}

static KainNativeHttpResponse* kain_native_net_alloc_response(void) {
    uint32_t slot;
    uint64_t bit;
    if (!kain_native_net_find_free_slot_u64(
            g_response_occupancy_bits,
            KAIN_NATIVE_NET_RESPONSE_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_responses[slot], 0, sizeof(g_responses[slot]));
    g_responses[slot].in_use = 1;
    g_responses[slot].id = g_next_response_id++;
    kain_native_net_copy(g_responses[slot].protocol, sizeof(g_responses[slot].protocol), g_http_protocol_http11);
    bit = UINT64_C(1) << slot;
    g_response_occupancy_bits |= bit;
    if (!kain_native_net_index_insert(
            g_response_index,
            KAIN_NATIVE_NET_RESPONSE_INDEX_CAPACITY,
            KAIN_NATIVE_NET_RESPONSE_INDEX_MASK,
            g_responses[slot].id,
            slot)) {
        g_response_occupancy_bits &= ~bit;
        memset(&g_responses[slot], 0, sizeof(g_responses[slot]));
        return 0;
    }
    return &g_responses[slot];
}

static KainNativeHttpServer* kain_native_net_server(int64_t id) {
    uint32_t start_index;
    uint32_t probe;
    if (id <= 0) {
        return 0;
    }
    start_index = kain_native_net_index_start_slot(id, KAIN_NATIVE_NET_SERVER_INDEX_MASK);
    for (probe = 0u; probe < KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_NATIVE_NET_SERVER_INDEX_MASK;
        uint32_t encoded_slot = g_server_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_NATIVE_NET_MAX_HTTP_SERVERS &&
            g_servers[slot].in_use &&
            g_servers[slot].id == id) {
            return &g_servers[slot];
        }
    }
    return 0;
}

static KainNativeHttpServer* kain_native_net_alloc_server(void) {
    uint32_t slot;
    uint64_t bit;
    if (!kain_native_net_find_free_slot_u64(
            g_server_occupancy_bits,
            KAIN_NATIVE_NET_SERVER_VALID_MASK,
            &slot)) {
        return 0;
    }
    memset(&g_servers[slot], 0, sizeof(g_servers[slot]));
    g_servers[slot].in_use = 1;
    g_servers[slot].id = g_next_server_id++;
    g_servers[slot].socket_handle = INVALID_SOCKET;
    bit = UINT64_C(1) << slot;
    g_server_occupancy_bits |= bit;
    if (!kain_native_net_index_insert(
            g_server_index,
            KAIN_NATIVE_NET_SERVER_INDEX_CAPACITY,
            KAIN_NATIVE_NET_SERVER_INDEX_MASK,
            g_servers[slot].id,
            slot)) {
        g_server_occupancy_bits &= ~bit;
        memset(&g_servers[slot], 0, sizeof(g_servers[slot]));
        return 0;
    }
    return &g_servers[slot];
}

static void kain_native_net_clear_request_from_server_queue(KainNativeHttpRequest* request) {
    if (request != 0 && request->server_id > 0) {
        KainNativeHttpServer* server = kain_native_net_server(request->server_id);
        if (server != 0) {
            uint32_t slot = (uint32_t)(request - g_requests);
            server->pending_request_slots &= ~(UINT64_C(1) << slot);
        }
    }
}

static void kain_native_net_set_header(KainNativeNetHeader* headers, int64_t* header_count, const char* key, const char* value) {
    int64_t index;
    if (headers == 0 || header_count == 0 || key == 0 || key[0] == '\0') {
        return;
    }
    for (index = 0; index < *header_count; ++index) {
        if (headers[index].in_use && kain_native_net_text_equal_ci(headers[index].key, key)) {
            kain_native_net_copy(headers[index].value, sizeof(headers[index].value), value);
            return;
        }
    }
    if (*header_count >= KAIN_NATIVE_NET_MAX_HEADERS) {
        return;
    }
    headers[*header_count].in_use = 1;
    kain_native_net_copy(headers[*header_count].key, sizeof(headers[*header_count].key), key);
    kain_native_net_copy(headers[*header_count].value, sizeof(headers[*header_count].value), value);
    *header_count += 1;
}

static const char* kain_native_net_find_header(const KainNativeNetHeader* headers, int64_t header_count, const char* key) {
    int64_t index;
    if (headers == 0 || key == 0) {
        return g_empty_string;
    }
    for (index = 0; index < header_count; ++index) {
        if (headers[index].in_use && kain_native_net_text_equal_ci(headers[index].key, key)) {
            return headers[index].value;
        }
    }
    return g_empty_string;
}

static int kain_native_net_parse_url(const char* url, KainNativeParsedUrl* parsed) {
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
    parsed->secure = kain_native_net_text_equal_ci(parsed->scheme, "https");
    if (!parsed->secure && !kain_native_net_text_equal_ci(parsed->scheme, "http")) {
        return 0;
    }
    parsed->port = parsed->secure ? 443 : 80;
    path_start = strchr(host_start, '/');
    port_start = strchr(host_start, ':');
    if (path_start == 0) {
        path_start = url + strlen(url);
        kain_native_net_copy(parsed->path, sizeof(parsed->path), "/");
    } else {
        kain_native_net_copy(parsed->path, sizeof(parsed->path), path_start);
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

static SOCKET kain_native_net_connect_socket(const char* host, int64_t port) {
    SOCKET socket_handle = INVALID_SOCKET;
    struct addrinfo hints;
    struct addrinfo* result = 0;
    struct addrinfo* current;
    char port_text[32];
#ifdef _WIN32
    kain_native_net_init_winsock();
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
        kain_native_net_socket_close(socket_handle);
        socket_handle = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return INVALID_SOCKET;
}

static int64_t kain_native_net_bind_listener(const char* host, int64_t port) {
    SOCKET socket_handle = INVALID_SOCKET;
    struct addrinfo hints;
    struct addrinfo* result = 0;
    struct addrinfo* current;
    char port_text[32];
    int yes = 1;
#ifdef _WIN32
    kain_native_net_init_winsock();
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
        kain_native_net_socket_close(socket_handle);
        socket_handle = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return INVALID_SOCKET;
}

static int64_t kain_native_net_socket_local_port(SOCKET socket_handle) {
    struct sockaddr_in address;
    socklen_t address_length = (socklen_t)sizeof(address);
    memset(&address, 0, sizeof(address));
    if (getsockname(socket_handle, (struct sockaddr*)&address, &address_length) != 0) {
        return 0;
    }
    return (int64_t)ntohs(address.sin_port);
}

static unsigned long long kain_native_net_hash_message_name(const char* message_name) {
    unsigned long long hash = 1469598103934665603ULL;
    const unsigned char* cursor = (const unsigned char*)(message_name ? message_name : "");
    while (*cursor) {
        hash ^= (unsigned long long)(*cursor++);
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

static void kain_native_net_dispatch_route(KainNativeHttpServer* server, KainNativeHttpRequest* request) {
    int64_t index;
    if (server == 0 || request == 0) {
        return;
    }
    for (index = 0; index < server->route_count; ++index) {
        KainNativeHttpRoute* route = &server->routes[index];
        if (!route->in_use) {
            continue;
        }
        if (kain_native_net_text_equal_ci(route->method, request->method) && strcmp(route->path, request->path) == 0) {
            KainActorMessage message;
            request->actor_id = route->actor_id;
            kain_native_net_copy(request->actor_message_kind, sizeof(request->actor_message_kind), route->message_kind);
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
            message.type_tag = kain_native_net_hash_message_name(route->message_kind);
            message.data = request->actor_payload;
            message.data_size = strlen(request->actor_payload) + 1u;
            message.sender_id = KAIN_ACTOR_ID_INVALID;
            (void)kain_actor_send((KainActorId)route->actor_id, &message, 0);
            return;
        }
    }
}

static int kain_native_net_parse_header_line(KainNativeHttpRequest* request, char* line) {
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
    kain_native_net_set_header(request->headers, &request->header_count, line, value);
    return 1;
}

static int kain_native_net_parse_http_request(KainNativeHttpRequest* request, unsigned char* bytes, size_t length) {
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
    kain_native_net_copy(request->method, sizeof(request->method), line);
    kain_native_net_copy(
        request->protocol,
        sizeof(request->protocol),
        kain_native_net_protocol_from_http_version_token(version)
    );
    {
        char* query = strchr(target, '?');
        if (query != 0) {
            *query++ = '\0';
            kain_native_net_copy(request->query, sizeof(request->query), query);
        }
        kain_native_net_copy(request->path, sizeof(request->path), target);
    }
    line = next_line + 2;
    while (line < headers_end) {
        next_line = strstr(line, "\r\n");
        if (next_line == 0 || next_line > headers_end) {
            break;
        }
        *next_line = '\0';
        if (line[0] != '\0') {
            kain_native_net_parse_header_line(request, line);
        }
        line = next_line + 2;
    }
    content_length_text = kain_native_net_find_header(request->headers, request->header_count, "Content-Length");
    if (content_length_text && content_length_text[0]) {
        if (!kain_native_net_parse_content_length_header(content_length_text, &body_length)) {
            free(text);
            return 0;
        }
    }
    if (body_length > 0u) {
        if (kain_native_net_size_add_overflow(header_length, body_length, &required_length) ||
            required_length > length ||
            kain_native_net_size_add_overflow(body_length, 1u, &required_length)) {
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

static int64_t kain_native_net_store_http_response(
    int64_t status_code,
    const char* protocol_name,
    const KainNativeNetHeader* headers,
    int64_t header_count,
    const unsigned char* body,
    size_t body_length
) {
    KainNativeHttpResponse* response = kain_native_net_alloc_response();
    const char* normalized_protocol = kain_native_net_normalize_protocol_name(protocol_name);
    int64_t index;
    if (response == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP response capacity exceeded");
    }
    response->status_code = status_code;
    if (normalized_protocol == 0) {
        normalized_protocol = g_http_protocol_http11;
    }
    kain_native_net_copy(
        response->protocol,
        sizeof(response->protocol),
        normalized_protocol
    );
    for (index = 0; index < header_count && index < KAIN_NATIVE_NET_MAX_HEADERS; ++index) {
        response->headers[index] = headers[index];
        response->header_count++;
    }
    if (body_length > 0u) {
        size_t allocation_size = 0u;
        if (kain_native_net_size_add_overflow(body_length, 1u, &allocation_size)) {
            return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "HTTP response body length overflowed");
        }
        response->body = (unsigned char*)malloc(allocation_size);
        if (response->body == 0) {
            return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "could not allocate HTTP response body");
        }
        memcpy(response->body, body, body_length);
        response->body[body_length] = '\0';
        response->body_length = body_length;
    }
    kain_native_net_ok();
    return response->id;
}

static int64_t kain_native_net_send_raw_http_client(KainNativeHttpRequest* request, const KainNativeParsedUrl* url) {
    SOCKET socket_handle;
    unsigned char* response_bytes = 0;
    size_t response_length = 0u;
    size_t response_capacity = 0u;
    char request_head[KAIN_NATIVE_NET_MAX_TEXT];
    const char* request_protocol = kain_native_net_normalize_protocol_name(request->protocol);
    int header_index;
    int64_t result_id;
    if (request_protocol == 0) {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    if (request_protocol == g_http_protocol_http2) {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_PROTOCOL_UNSUPPORTED,
            "unsupported-protocol",
            "HTTP/2 client requests require the HTTPS WinHTTP lane in v1"
        );
    }
    socket_handle = kain_native_net_connect_socket(url->host, url->port);
    if (socket_handle == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "connect", "HTTP client TCP connect failed");
    }
    snprintf(
        request_head,
        sizeof(request_head),
        "%s %s %s\r\nHost: %s\r\nConnection: close\r\nContent-Length: %llu\r\n",
        request->method[0] ? request->method : "GET",
        url->path[0] ? url->path : "/",
        kain_native_net_http_version_token_from_protocol(request_protocol),
        url->host,
        (unsigned long long)request->body_length
    );
    if (!kain_native_net_send_all(socket_handle, (const unsigned char*)request_head, strlen(request_head))) {
        kain_native_net_socket_close(socket_handle);
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP request write failed");
    }
    for (header_index = 0; header_index < request->header_count; ++header_index) {
        char header_line[KAIN_NATIVE_NET_MAX_TEXT];
        snprintf(header_line, sizeof(header_line), "%s: %s\r\n", request->headers[header_index].key, request->headers[header_index].value);
        if (!kain_native_net_send_all(socket_handle, (const unsigned char*)header_line, strlen(header_line))) {
            kain_native_net_socket_close(socket_handle);
            return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP header write failed");
        }
    }
    if (!kain_native_net_send_all(socket_handle, (const unsigned char*)"\r\n", 2u)) {
        kain_native_net_socket_close(socket_handle);
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP header terminator write failed");
    }
    if (request->body_length > 0u && !kain_native_net_send_all(socket_handle, request->body, request->body_length)) {
        kain_native_net_socket_close(socket_handle);
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP body write failed");
    }
    while (kain_native_net_wait_readable(socket_handle, 5000)) {
        unsigned char buffer[4096];
        int read_count = recv(socket_handle, (char*)buffer, sizeof(buffer), 0);
        if (read_count <= 0) {
            break;
        }
        if (!kain_native_net_append_bytes(&response_bytes, &response_length, &response_capacity, buffer, (size_t)read_count)) {
            free(response_bytes);
            kain_native_net_socket_close(socket_handle);
            return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "HTTP response capture allocation failed");
        }
    }
    kain_native_net_socket_close(socket_handle);
    if (response_length == 0u) {
        free(response_bytes);
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "read", "HTTP response was empty");
    }
    {
        char* header_end = strstr((char*)response_bytes, "\r\n\r\n");
        int64_t status = 0;
        const char* response_protocol = g_http_protocol_http11;
        KainNativeNetHeader headers[KAIN_NATIVE_NET_MAX_HEADERS];
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
                    kain_native_net_protocol_from_http_version_token((char*)response_bytes);
                line += 2;
                while (*line) {
                    next_line = strstr(line, "\r\n");
                    if (next_line == 0) {
                        break;
                    }
                    *next_line = '\0';
                    {
                        char* colon = strchr(line, ':');
                        if (colon != 0 && header_count < KAIN_NATIVE_NET_MAX_HEADERS) {
                            *colon = '\0';
                            ++colon;
                            while (*colon == ' ' || *colon == '\t') ++colon;
                            kain_native_net_set_header(headers, &header_count, line, colon);
                        }
                    }
                    line = next_line + 2;
                }
            }
        }
        result_id = kain_native_net_store_http_response(
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
static wchar_t* kain_native_net_wide_from_utf8(const char* text) {
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

static int64_t kain_native_net_send_winhttp_client(KainNativeHttpRequest* request, const KainNativeParsedUrl* url) {
    HINTERNET session = 0;
    HINTERNET connection = 0;
    HINTERNET win_request = 0;
    wchar_t* host_wide = 0;
    wchar_t* path_wide = 0;
    wchar_t* method_wide = 0;
    unsigned char* response_body = 0;
    size_t body_length = 0u;
    size_t body_capacity = 0u;
    const char* request_protocol = kain_native_net_normalize_protocol_name(request->protocol);
    const char* response_protocol = g_http_protocol_http11;
    DWORD status_code = 0u;
    DWORD status_size = sizeof(status_code);
    int64_t response_id;
    if (request_protocol == 0) {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    host_wide = kain_native_net_wide_from_utf8(url->host);
    path_wide = kain_native_net_wide_from_utf8(url->path[0] ? url->path : "/");
    method_wide = kain_native_net_wide_from_utf8(request->method[0] ? request->method : "GET");
    if (host_wide == 0 || path_wide == 0 || method_wide == 0) {
        free(host_wide);
        free(path_wide);
        free(method_wide);
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "could not allocate WinHTTP strings");
    }
    session = WinHttpOpen(L"KainNet/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == 0) {
        response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "winhttp", "WinHttpOpen failed");
        goto cleanup;
    }
    connection = WinHttpConnect(session, host_wide, (INTERNET_PORT)url->port, 0);
    if (connection == 0) {
        response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "winhttp", "WinHttpConnect failed");
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
        response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "winhttp", "WinHttpOpenRequest failed");
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
            response_id = kain_native_net_fail(
                KAIN_NATIVE_NET_PROTOCOL_UNSUPPORTED,
                "unsupported-protocol",
                "WinHTTP could not enable HTTP/2 for this request"
            );
            goto cleanup;
        }
#else
        response_id = kain_native_net_fail(
            KAIN_NATIVE_NET_PROTOCOL_UNSUPPORTED,
            "unsupported-protocol",
            "HTTP/2 requires a WinHTTP SDK with protocol option support"
        );
        goto cleanup;
#endif
    }
    {
        int64_t index;
        for (index = 0; index < request->header_count; ++index) {
            char line[KAIN_NATIVE_NET_MAX_TEXT];
            wchar_t* wide_line;
            snprintf(line, sizeof(line), "%s: %s", request->headers[index].key, request->headers[index].value);
            wide_line = kain_native_net_wide_from_utf8(line);
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
        response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "winhttp", "WinHTTP send/receive failed");
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
            response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "WinHTTP body allocation failed");
            goto cleanup;
        }
        if (!WinHttpReadData(win_request, chunk, available, &read_count)) {
            free(chunk);
            response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "winhttp", "WinHTTP body read failed");
            goto cleanup;
        }
        if (!kain_native_net_append_bytes(&response_body, &body_length, &body_capacity, chunk, read_count)) {
            free(chunk);
            response_id = kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "HTTP response allocation failed");
            goto cleanup;
        }
        free(chunk);
    }
    response_id = kain_native_net_store_http_response(
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

int64_t kain_native_net_reset(void) {
    size_t index;
    for (index = 0u; index < KAIN_NATIVE_NET_MAX_CONNECTIONS; ++index) {
        if (g_connections[index].in_use) {
            kain_native_net_socket_close(g_connections[index].socket_handle);
        }
    }
    for (index = 0u; index < KAIN_NATIVE_NET_MAX_LISTENERS; ++index) {
        if (g_listeners[index].in_use) {
            kain_native_net_socket_close(g_listeners[index].socket_handle);
        }
    }
    for (index = 0u; index < KAIN_NATIVE_NET_MAX_HTTP_REQUESTS; ++index) {
        if (g_requests[index].in_use) {
            free(g_requests[index].body);
            if (g_requests[index].socket_handle != INVALID_SOCKET) {
                kain_native_net_socket_close(g_requests[index].socket_handle);
            }
        }
    }
    for (index = 0u; index < KAIN_NATIVE_NET_MAX_HTTP_RESPONSES; ++index) {
        if (g_responses[index].in_use) {
            free(g_responses[index].body);
        }
    }
    for (index = 0u; index < KAIN_NATIVE_NET_MAX_HTTP_SERVERS; ++index) {
        if (g_servers[index].in_use && g_servers[index].socket_handle != INVALID_SOCKET) {
            kain_native_net_socket_close(g_servers[index].socket_handle);
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
    return kain_native_net_ok();
}

int64_t kain_native_net_platform_available(void) {
    return 1;
}

const char* kain_native_net_platform_name(void) {
#ifdef _WIN32
    kain_native_net_ok();
    return kain_native_net_string("windows");
#elif defined(__APPLE__)
    kain_native_net_ok();
    return kain_native_net_string("macos");
#elif defined(__linux__)
    kain_native_net_ok();
    return kain_native_net_string("linux");
#else
    kain_native_net_ok();
    return kain_native_net_string("unknown");
#endif
}

int64_t kain_native_net_capability_state(const char* capability_key) {
    if (capability_key == 0 || capability_key[0] == '\0') {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_INVALID_ARGUMENT,
            "invalid-capability",
            "network capability key is required"
        );
    }
    if (kain_native_net_text_equal_ci(capability_key, "net") ||
        kain_native_net_text_equal_ci(capability_key, "tcp") ||
        kain_native_net_text_equal_ci(capability_key, "http1.client") ||
        kain_native_net_text_equal_ci(capability_key, "http.client") ||
        kain_native_net_text_equal_ci(capability_key, "http1.server") ||
        kain_native_net_text_equal_ci(capability_key, "http.server")) {
        kain_native_net_ok();
        return KAIN_NATIVE_NET_CAPABILITY_AVAILABLE;
    }
    if (kain_native_net_text_equal_ci(capability_key, "tls.client") ||
        kain_native_net_text_equal_ci(capability_key, "https.client")) {
#ifdef _WIN32
        kain_native_net_ok();
        return KAIN_NATIVE_NET_CAPABILITY_AVAILABLE;
#else
        kain_native_net_ok();
        return KAIN_NATIVE_NET_CAPABILITY_UNAVAILABLE;
#endif
    }
    if (kain_native_net_text_equal_ci(capability_key, "http2.client")) {
#ifdef _WIN32
        kain_native_net_ok();
        return KAIN_NATIVE_NET_CAPABILITY_DEGRADED;
#else
        kain_native_net_ok();
        return KAIN_NATIVE_NET_CAPABILITY_UNAVAILABLE;
#endif
    }
    if (kain_native_net_text_equal_ci(capability_key, "tls.server") ||
        kain_native_net_text_equal_ci(capability_key, "https.server") ||
        kain_native_net_text_equal_ci(capability_key, "http2.server")) {
        kain_native_net_ok();
        return KAIN_NATIVE_NET_CAPABILITY_UNAVAILABLE;
    }
    return kain_native_net_fail(
        KAIN_NATIVE_NET_INVALID_ARGUMENT,
        "invalid-capability",
        "network capability key is unknown"
    );
}

int64_t kain_native_tcp_connect(const char* host, int64_t port, int64_t timeout_ms) {
    SOCKET socket_handle;
    KainNativeTcpConnection* connection;
    (void)timeout_ms;
    if (host == 0 || host[0] == '\0' || port <= 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_ARGUMENT, "invalid-argument", "TCP connect requires host and port");
    }
    socket_handle = kain_native_net_connect_socket(host, port);
    if (socket_handle == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "connect", "TCP connect failed");
    }
    connection = kain_native_net_alloc_connection(socket_handle);
    if (connection == 0) {
        kain_native_net_socket_close(socket_handle);
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "TCP connection capacity exceeded");
    }
    kain_native_net_ok();
    return connection->id;
}

int64_t kain_native_tcp_listen(const char* host, int64_t port) {
    SOCKET socket_handle = (SOCKET)kain_native_net_bind_listener(host, port);
    KainNativeTcpListener* listener;
    if (socket_handle == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "listen", "TCP listen failed");
    }
    listener = kain_native_net_alloc_listener(socket_handle, kain_native_net_socket_local_port(socket_handle));
    if (listener == 0) {
        kain_native_net_socket_close(socket_handle);
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "TCP listener capacity exceeded");
    }
    kain_native_net_ok();
    return listener->id;
}

int64_t kain_native_tcp_listener_local_port(int64_t listener_id) {
    KainNativeTcpListener* listener = kain_native_net_listener(listener_id);
    if (listener == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-listener", "TCP listener not found");
    }
    kain_native_net_ok();
    return listener->local_port;
}

int64_t kain_native_tcp_accept(int64_t listener_id, int64_t timeout_ms) {
    KainNativeTcpListener* listener = kain_native_net_listener(listener_id);
    SOCKET accepted;
    KainNativeTcpConnection* connection;
    if (listener == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-listener", "TCP listener not found");
    }
    if (!kain_native_net_wait_readable(listener->socket_handle, timeout_ms)) {
        kain_native_net_ok();
        return 0;
    }
    accepted = accept(listener->socket_handle, 0, 0);
    if (accepted == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "accept", "TCP accept failed");
    }
    connection = kain_native_net_alloc_connection(accepted);
    if (connection == 0) {
        kain_native_net_socket_close(accepted);
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "TCP connection capacity exceeded");
    }
    kain_native_net_ok();
    return connection->id;
}

const char* kain_native_tcp_read_text(int64_t connection_id) {
    KainNativeTcpConnection* connection = kain_native_net_connection(connection_id);
    char buffer[4096];
    int read_count;
    if (connection == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
        return kain_native_net_string("");
    }
    if (!kain_native_net_wait_readable(connection->socket_handle, 5000)) {
        kain_native_net_ok();
        return kain_native_net_string("");
    }
    read_count = recv(connection->socket_handle, buffer, sizeof(buffer) - 1, 0);
    if (read_count <= 0) {
        kain_native_net_ok();
        return kain_native_net_string("");
    }
    buffer[read_count] = '\0';
    kain_native_net_ok();
    return kain_native_net_string(buffer);
}

const char* kain_native_tcp_read_hex(int64_t connection_id) {
    const char* text = kain_native_tcp_read_text(connection_id);
    return kain_native_net_encode_hex((const unsigned char*)text, strlen(text));
}

int64_t kain_native_tcp_write_text(int64_t connection_id, const char* payload) {
    KainNativeTcpConnection* connection = kain_native_net_connection(connection_id);
    const char* text = payload ? payload : "";
    if (connection == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
    }
    if (!kain_native_net_send_all(connection->socket_handle, (const unsigned char*)text, strlen(text))) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "TCP write failed");
    }
    return kain_native_net_ok();
}

int64_t kain_native_tcp_write_hex(int64_t connection_id, const char* payload_hex) {
    unsigned char* bytes = 0;
    size_t byte_length = 0u;
    KainNativeTcpConnection* connection = kain_native_net_connection(connection_id);
    if (connection == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
    }
    if (!kain_native_net_decode_hex(payload_hex, &bytes, &byte_length)) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_ARGUMENT, "invalid-hex", "TCP hex payload is invalid");
    }
    if (!kain_native_net_send_all(connection->socket_handle, bytes, byte_length)) {
        free(bytes);
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "TCP byte write failed");
    }
    free(bytes);
    return kain_native_net_ok();
}

int64_t kain_native_tcp_close(int64_t connection_id) {
    KainNativeTcpConnection* connection = kain_native_net_connection(connection_id);
    uint32_t slot;
    if (connection == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-connection", "TCP connection not found");
    }
    slot = (uint32_t)(connection - g_connections);
    kain_native_net_socket_close(connection->socket_handle);
    g_connection_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(connection, 0, sizeof(*connection));
    kain_native_net_rebuild_connection_index();
    return kain_native_net_ok();
}

int64_t kain_native_tcp_listener_close(int64_t listener_id) {
    KainNativeTcpListener* listener = kain_native_net_listener(listener_id);
    uint32_t slot;
    if (listener == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-listener", "TCP listener not found");
    }
    slot = (uint32_t)(listener - g_listeners);
    kain_native_net_socket_close(listener->socket_handle);
    g_listener_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(listener, 0, sizeof(*listener));
    kain_native_net_rebuild_listener_index();
    return kain_native_net_ok();
}

int64_t kain_native_http_request_create(const char* method, const char* url) {
    KainNativeHttpRequest* request;
    if (url == 0 || url[0] == '\0') {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_ARGUMENT, "invalid-url", "HTTP request requires a URL");
    }
    request = kain_native_net_alloc_request(0);
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP request capacity exceeded");
    }
    kain_native_net_copy(request->method, sizeof(request->method), (method && method[0]) ? method : "GET");
    kain_native_net_copy(request->url, sizeof(request->url), url);
    kain_native_net_ok();
    return request->id;
}

int64_t kain_native_http_request_set_header(int64_t request_id, const char* key, const char* value) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (request->header_count >= KAIN_NATIVE_NET_MAX_HEADERS) {
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP header capacity exceeded");
    }
    kain_native_net_set_header(request->headers, &request->header_count, key, value);
    return kain_native_net_ok();
}

int64_t kain_native_http_request_set_body_text(int64_t request_id, const char* payload) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    const char* text = payload ? payload : "";
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    free(request->body);
    request->body_length = strlen(text);
    request->body = (unsigned char*)malloc(request->body_length + 1u);
    if (request->body == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "could not allocate HTTP request body");
    }
    memcpy(request->body, text, request->body_length + 1u);
    return kain_native_net_ok();
}

int64_t kain_native_http_request_set_body_hex(int64_t request_id, const char* payload_hex) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    unsigned char* bytes = 0;
    size_t byte_length = 0u;
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (!kain_native_net_decode_hex(payload_hex, &bytes, &byte_length)) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_ARGUMENT, "invalid-hex", "HTTP body hex is invalid");
    }
    free(request->body);
    request->body = bytes;
    request->body_length = byte_length;
    return kain_native_net_ok();
}

int64_t kain_native_http_request_set_timeout(int64_t request_id, int64_t timeout_ms) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    request->timeout_ms = timeout_ms <= 0 ? 30000 : timeout_ms;
    return kain_native_net_ok();
}

int64_t kain_native_http_request_set_protocol(int64_t request_id, const char* protocol_name) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    const char* normalized_protocol;
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    normalized_protocol = kain_native_net_normalize_protocol_name(protocol_name);
    if (normalized_protocol == 0) {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    kain_native_net_copy(request->protocol, sizeof(request->protocol), normalized_protocol);
    return kain_native_net_ok();
}

const char* kain_native_http_request_protocol(int64_t request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(request->protocol);
}

int64_t kain_native_http_client_send(int64_t request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    KainNativeParsedUrl parsed;
    const char* request_protocol;
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (!kain_native_net_parse_url(request->url, &parsed)) {
        return kain_native_net_fail(KAIN_NATIVE_NET_PARSE_ERROR, "invalid-url", "HTTP request URL could not be parsed");
    }
    request_protocol = kain_native_net_normalize_protocol_name(request->protocol);
    if (request_protocol == 0) {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_INVALID_ARGUMENT,
            "invalid-protocol",
            "HTTP request protocol name is invalid"
        );
    }
    if (!parsed.secure && request_protocol == g_http_protocol_http2) {
        return kain_native_net_fail(
            KAIN_NATIVE_NET_PROTOCOL_UNSUPPORTED,
            "unsupported-protocol",
            "HTTP/2 client requests currently require an HTTPS URL"
        );
    }
#ifdef _WIN32
    if (parsed.secure) {
        return kain_native_net_send_winhttp_client(request, &parsed);
    }
#endif
    if (parsed.secure) {
        return kain_native_net_fail(KAIN_NATIVE_NET_UNSUPPORTED_PLATFORM, "unsupported-tls", "HTTPS client is only implemented through WinHTTP in v1");
    }
    return kain_native_net_send_raw_http_client(request, &parsed);
}

int64_t kain_native_http_response_status(int64_t response_id) {
    KainNativeHttpResponse* response = kain_native_net_response(response_id);
    if (response == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
    }
    kain_native_net_ok();
    return response->status_code;
}

const char* kain_native_http_response_protocol(int64_t response_id) {
    KainNativeHttpResponse* response = kain_native_net_response(response_id);
    if (response == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(response->protocol);
}

const char* kain_native_http_response_header(int64_t response_id, const char* key) {
    KainNativeHttpResponse* response = kain_native_net_response(response_id);
    if (response == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(kain_native_net_find_header(response->headers, response->header_count, key));
}

const char* kain_native_http_response_body_text(int64_t response_id) {
    KainNativeHttpResponse* response = kain_native_net_response(response_id);
    if (response == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string_from_bytes(response->body, response->body_length);
}

const char* kain_native_http_response_body_hex(int64_t response_id) {
    KainNativeHttpResponse* response = kain_native_net_response(response_id);
    if (response == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_encode_hex(response->body, response->body_length);
}

int64_t kain_native_http_request_destroy(int64_t request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(request_id);
    uint32_t slot;
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    slot = (uint32_t)(request - g_requests);
    kain_native_net_clear_request_from_server_queue(request);
    free(request->body);
    if (request->socket_handle != INVALID_SOCKET) {
        kain_native_net_socket_close(request->socket_handle);
    }
    g_request_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(request, 0, sizeof(*request));
    kain_native_net_rebuild_request_index();
    return kain_native_net_ok();
}

int64_t kain_native_http_response_destroy(int64_t response_id) {
    KainNativeHttpResponse* response = kain_native_net_response(response_id);
    uint32_t slot;
    if (response == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-response", "HTTP response not found");
    }
    slot = (uint32_t)(response - g_responses);
    free(response->body);
    g_response_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(response, 0, sizeof(*response));
    kain_native_net_rebuild_response_index();
    return kain_native_net_ok();
}

int64_t kain_native_http_server_create(const char* host, int64_t port) {
    KainNativeHttpServer* server = kain_native_net_alloc_server();
    if (server == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP server capacity exceeded");
    }
    kain_native_net_copy(server->host, sizeof(server->host), (host && host[0]) ? host : "127.0.0.1");
    server->local_port = port;
    kain_native_net_ok();
    return server->id;
}

int64_t kain_native_http_server_listen(int64_t server_id) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    if (server == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    if (server->listening) {
        return kain_native_net_ok();
    }
    server->socket_handle = (SOCKET)kain_native_net_bind_listener(server->host, server->local_port);
    if (server->socket_handle == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "listen", "HTTP server listen failed");
    }
    server->local_port = kain_native_net_socket_local_port(server->socket_handle);
    server->listening = 1;
    return kain_native_net_ok();
}

int64_t kain_native_http_server_local_port(int64_t server_id) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    if (server == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    kain_native_net_ok();
    return server->local_port;
}

int64_t kain_native_http_server_route_actor(int64_t server_id, const char* method, const char* path, int64_t actor_id, const char* message_kind) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    KainNativeHttpRoute* route;
    if (server == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    if (server->route_count >= KAIN_NATIVE_NET_MAX_ROUTES) {
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP route capacity exceeded");
    }
    route = &server->routes[server->route_count++];
    memset(route, 0, sizeof(*route));
    route->in_use = 1;
    kain_native_net_copy(route->method, sizeof(route->method), (method && method[0]) ? method : "GET");
    kain_native_net_copy(route->path, sizeof(route->path), (path && path[0]) ? path : "/");
    route->actor_id = actor_id;
    kain_native_net_copy(route->message_kind, sizeof(route->message_kind), (message_kind && message_kind[0]) ? message_kind : "HttpRequest");
    return kain_native_net_ok();
}

int64_t kain_native_http_server_pump(int64_t server_id, int64_t timeout_ms) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    SOCKET accepted;
    unsigned char* request_bytes = 0;
    size_t request_length = 0u;
    size_t request_capacity = 0u;
    KainNativeHttpRequest* request;
    if (server == 0 || !server->listening) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-server", "HTTP server is not listening");
    }
    if (!kain_native_net_wait_readable(server->socket_handle, timeout_ms)) {
        kain_native_net_ok();
        return 0;
    }
    accepted = accept(server->socket_handle, 0, 0);
    if (accepted == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "accept", "HTTP server accept failed");
    }
    while (kain_native_net_wait_readable(accepted, 5000)) {
        unsigned char buffer[4096];
        int read_count = recv(accepted, (char*)buffer, sizeof(buffer), 0);
        if (read_count <= 0) {
            break;
        }
        if (!kain_native_net_append_bytes(&request_bytes, &request_length, &request_capacity, buffer, (size_t)read_count)) {
            free(request_bytes);
            kain_native_net_socket_close(accepted);
            return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "allocation", "HTTP request allocation failed");
        }
        if (strstr((char*)request_bytes, "\r\n\r\n") != 0) {
            const char* content_length_text = 0;
            size_t header_length = 0u;
            size_t content_length = 0u;
            size_t required_length = 0u;
            char* headers_copy = (char*)malloc(request_length + 1u);
            char* header_end;
            if (headers_copy != 0) {
                memcpy(headers_copy, request_bytes, request_length + 1u);
                header_end = strstr(headers_copy, "\r\n\r\n");
                if (header_end != 0) {
                    *header_end = '\0';
                    header_length = (size_t)((header_end + 4) - headers_copy);
                    char* content_length = strstr(headers_copy, "Content-Length:");
                    if (content_length != 0 && content_length < header_end) {
                        content_length_text = content_length + strlen("Content-Length:");
                    }
                }
                if (content_length_text == 0) {
                    free(headers_copy);
                    break;
                }
                if (!kain_native_net_parse_content_length_header(content_length_text, &content_length)) {
                    free(headers_copy);
                    free(request_bytes);
                    kain_native_net_socket_close(accepted);
                    return kain_native_net_fail(KAIN_NATIVE_NET_PARSE_ERROR, "parse", "HTTP Content-Length header was invalid");
                }
                if (kain_native_net_size_add_overflow(header_length, content_length, &required_length)) {
                    free(headers_copy);
                    free(request_bytes);
                    kain_native_net_socket_close(accepted);
                    return kain_native_net_fail(KAIN_NATIVE_NET_PARSE_ERROR, "parse", "HTTP Content-Length header overflowed request size");
                }
                if (required_length <= request_length) {
                    free(headers_copy);
                    break;
                }
                free(headers_copy);
            } else {
                break;
            }
        }
    }
    request = kain_native_net_alloc_request(1);
    if (request == 0) {
        free(request_bytes);
        kain_native_net_socket_close(accepted);
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP incoming request capacity exceeded");
    }
    request->server_id = server_id;
    request->socket_handle = accepted;
    server->pending_request_slots |= UINT64_C(1) << (uint32_t)(request - g_requests);
    if (!kain_native_net_parse_http_request(request, request_bytes, request_length)) {
        free(request_bytes);
        kain_native_http_request_destroy(request->id);
        return kain_native_net_fail(KAIN_NATIVE_NET_PARSE_ERROR, "parse", "HTTP request parse failed");
    }
    free(request_bytes);
    kain_native_net_dispatch_route(server, request);
    kain_native_net_ok();
    return request->id;
}

int64_t kain_native_http_server_pending_request_count(int64_t server_id) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    uint64_t pending_mask;
    int64_t pending_count = 0;
    if (server == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    pending_mask = server->pending_request_slots & g_request_occupancy_bits;
    while (pending_mask != 0u) {
        ++pending_count;
        pending_mask &= pending_mask - 1u;
    }
    kain_native_net_ok();
    return pending_count;
}

int64_t kain_native_http_server_next_request(int64_t server_id) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    uint64_t pending_mask;
    if (server == 0) {
        kain_native_net_ok();
        return 0;
    }
    pending_mask = server->pending_request_slots & g_request_occupancy_bits;
    while (pending_mask != 0u) {
        uint64_t low_bit = kain_native_net_isolate_low_bit_u64(pending_mask);
        uint32_t slot = (uint32_t)kain_native_net_low_bit_index_u64(low_bit);
        KainNativeHttpRequest* request = &g_requests[slot];
        server->pending_request_slots &= ~low_bit;
        if (request->in_use && request->incoming && request->server_id == server_id && !request->dequeued && !request->responded) {
            request->dequeued = 1;
            kain_native_net_ok();
            return request->id;
        }
        pending_mask &= ~low_bit;
    }
    kain_native_net_ok();
    return 0;
}

const char* kain_native_http_request_method(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(request->method);
}

const char* kain_native_http_request_path(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(request->path);
}

const char* kain_native_http_request_query(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(request->query);
}

const char* kain_native_http_request_header(int64_t incoming_request_id, const char* key) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string(kain_native_net_find_header(request->headers, request->header_count, key));
}

const char* kain_native_http_request_body_text(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_string_from_bytes(request->body, request->body_length);
}

const char* kain_native_http_request_body_hex(int64_t incoming_request_id) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
        return kain_native_net_string("");
    }
    kain_native_net_ok();
    return kain_native_net_encode_hex(request->body, request->body_length);
}

int64_t kain_native_http_response_set_header_for_request(int64_t incoming_request_id, const char* key, const char* value) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    if (request == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request not found");
    }
    if (request->response_header_count >= KAIN_NATIVE_NET_MAX_HEADERS) {
        return kain_native_net_fail(KAIN_NATIVE_NET_CAPACITY_EXCEEDED, "capacity", "HTTP response header capacity exceeded");
    }
    kain_native_net_set_header(request->response_headers, &request->response_header_count, key, value);
    return kain_native_net_ok();
}

static int64_t kain_native_net_respond_bytes(KainNativeHttpRequest* request, int64_t status_code, const unsigned char* payload, size_t payload_length) {
    char head[KAIN_NATIVE_NET_MAX_TEXT];
    int64_t header_index;
    if (request == 0 || request->socket_handle == INVALID_SOCKET) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-request", "HTTP request socket is not available");
    }
    snprintf(
        head,
        sizeof(head),
        "HTTP/1.1 %lld OK\r\nContent-Length: %llu\r\nConnection: close\r\n",
        (long long)status_code,
        (unsigned long long)payload_length
    );
    if (!kain_native_net_send_all(request->socket_handle, (const unsigned char*)head, strlen(head))) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP response head write failed");
    }
    for (header_index = 0; header_index < request->response_header_count; ++header_index) {
        char header_line[KAIN_NATIVE_NET_MAX_TEXT];
        snprintf(
            header_line,
            sizeof(header_line),
            "%s: %s\r\n",
            request->response_headers[header_index].key,
            request->response_headers[header_index].value
        );
        if (!kain_native_net_send_all(request->socket_handle, (const unsigned char*)header_line, strlen(header_line))) {
            return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP response header write failed");
        }
    }
    if (!kain_native_net_send_all(request->socket_handle, (const unsigned char*)"\r\n", 2u)) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP response terminator write failed");
    }
    if (payload_length > 0u && !kain_native_net_send_all(request->socket_handle, payload, payload_length)) {
        return kain_native_net_fail(KAIN_NATIVE_NET_IO_ERROR, "write", "HTTP response body write failed");
    }
    request->responded = 1;
    kain_native_net_socket_close(request->socket_handle);
    request->socket_handle = INVALID_SOCKET;
    return kain_native_http_request_destroy(request->id);
}

int64_t kain_native_http_respond_text(int64_t incoming_request_id, int64_t status_code, const char* payload) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    const char* text = payload ? payload : "";
    return kain_native_net_respond_bytes(request, status_code, (const unsigned char*)text, strlen(text));
}

int64_t kain_native_http_respond_hex(int64_t incoming_request_id, int64_t status_code, const char* payload_hex) {
    KainNativeHttpRequest* request = kain_native_net_request(incoming_request_id);
    unsigned char* bytes = 0;
    size_t byte_length = 0u;
    int64_t result;
    if (!kain_native_net_decode_hex(payload_hex, &bytes, &byte_length)) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_ARGUMENT, "invalid-hex", "HTTP response hex is invalid");
    }
    result = kain_native_net_respond_bytes(request, status_code, bytes, byte_length);
    free(bytes);
    return result;
}

int64_t kain_native_http_server_close(int64_t server_id) {
    KainNativeHttpServer* server = kain_native_net_server(server_id);
    uint32_t slot;
    if (server == 0) {
        return kain_native_net_fail(KAIN_NATIVE_NET_INVALID_HANDLE, "invalid-server", "HTTP server not found");
    }
    slot = (uint32_t)(server - g_servers);
    if (server->socket_handle != INVALID_SOCKET) {
        kain_native_net_socket_close(server->socket_handle);
    }
    server->pending_request_slots = 0u;
    g_server_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(server, 0, sizeof(*server));
    kain_native_net_rebuild_server_index();
    return kain_native_net_ok();
}

const char* kain_native_http_local_url(int64_t port, const char* path) {
    char url[KAIN_NATIVE_NET_MAX_URL];
    snprintf(url, sizeof(url), "http://127.0.0.1:%lld%s%s", (long long)port, (path && path[0] == '/') ? "" : "/", path ? path : "");
    return kain_native_net_string(url);
}

int64_t kain_native_net_last_status(void) {
    return g_last_status;
}

const char* kain_native_net_last_error_kind(void) {
    return kain_native_net_string(g_last_error_kind);
}

const char* kain_native_net_last_error_message(void) {
    return kain_native_net_string(g_last_error_message);
}

const KainNativeNetFunctionTable g_kain_native_net_function_table = {
    kain_native_net_reset,
    kain_native_net_platform_available,
    kain_native_net_platform_name,
    kain_native_net_capability_state,
    kain_native_tcp_connect,
    kain_native_tcp_listen,
    kain_native_tcp_accept,
    kain_native_http_request_create,
    kain_native_http_request_set_protocol,
    kain_native_http_client_send,
    kain_native_http_response_protocol,
    kain_native_http_server_create,
    kain_native_http_server_listen,
    kain_native_http_server_pump,
    kain_native_http_server_pending_request_count,
    kain_native_net_last_status
};
