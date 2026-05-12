#ifndef KAIN_NATIVE_NET_SYSTEM_H
#define KAIN_NATIVE_NET_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_NATIVE_NET_OK 0
#define KAIN_NATIVE_NET_INVALID_ARGUMENT -1
#define KAIN_NATIVE_NET_INVALID_HANDLE -2
#define KAIN_NATIVE_NET_CAPACITY_EXCEEDED -3
#define KAIN_NATIVE_NET_UNSUPPORTED_PLATFORM -4
#define KAIN_NATIVE_NET_IO_ERROR -5
#define KAIN_NATIVE_NET_PARSE_ERROR -6

#define KAIN_NATIVE_NET_MAX_KEY 64
#define KAIN_NATIVE_NET_MAX_TEXT 4096
#define KAIN_NATIVE_NET_MAX_URL 2048
#define KAIN_NATIVE_NET_MAX_HEADERS 32
#define KAIN_NATIVE_NET_MAX_CONNECTIONS 64
#define KAIN_NATIVE_NET_MAX_LISTENERS 16
#define KAIN_NATIVE_NET_MAX_HTTP_REQUESTS 64
#define KAIN_NATIVE_NET_MAX_HTTP_RESPONSES 64
#define KAIN_NATIVE_NET_MAX_HTTP_SERVERS 16
#define KAIN_NATIVE_NET_MAX_ROUTES 32

typedef struct KainNativeNetFunctionTable {
    int64_t (*reset)(void);
    int64_t (*platform_available)(void);
    int64_t (*tcp_connect)(const char* host, int64_t port, int64_t timeout_ms);
    int64_t (*tcp_listen)(const char* host, int64_t port);
    int64_t (*tcp_accept)(int64_t listener_id, int64_t timeout_ms);
    int64_t (*http_request_create)(const char* method, const char* url);
    int64_t (*http_client_send)(int64_t request_id);
    int64_t (*http_server_create)(const char* host, int64_t port);
    int64_t (*http_server_listen)(int64_t server_id);
    int64_t (*http_server_pump)(int64_t server_id, int64_t timeout_ms);
    int64_t (*last_status)(void);
} KainNativeNetFunctionTable;

extern const KainNativeNetFunctionTable g_kain_native_net_function_table;

int64_t kain_native_net_reset(void);
int64_t kain_native_net_platform_available(void);

int64_t kain_native_tcp_connect(const char* host, int64_t port, int64_t timeout_ms);
int64_t kain_native_tcp_listen(const char* host, int64_t port);
int64_t kain_native_tcp_listener_local_port(int64_t listener_id);
int64_t kain_native_tcp_accept(int64_t listener_id, int64_t timeout_ms);
const char* kain_native_tcp_read_text(int64_t connection_id);
const char* kain_native_tcp_read_hex(int64_t connection_id);
int64_t kain_native_tcp_write_text(int64_t connection_id, const char* payload);
int64_t kain_native_tcp_write_hex(int64_t connection_id, const char* payload_hex);
int64_t kain_native_tcp_close(int64_t connection_id);
int64_t kain_native_tcp_listener_close(int64_t listener_id);

int64_t kain_native_http_request_create(const char* method, const char* url);
int64_t kain_native_http_request_set_header(int64_t request_id, const char* key, const char* value);
int64_t kain_native_http_request_set_body_text(int64_t request_id, const char* payload);
int64_t kain_native_http_request_set_body_hex(int64_t request_id, const char* payload_hex);
int64_t kain_native_http_request_set_timeout(int64_t request_id, int64_t timeout_ms);
int64_t kain_native_http_client_send(int64_t request_id);
int64_t kain_native_http_response_status(int64_t response_id);
const char* kain_native_http_response_header(int64_t response_id, const char* key);
const char* kain_native_http_response_body_text(int64_t response_id);
const char* kain_native_http_response_body_hex(int64_t response_id);
int64_t kain_native_http_request_destroy(int64_t request_id);
int64_t kain_native_http_response_destroy(int64_t response_id);

int64_t kain_native_http_server_create(const char* host, int64_t port);
int64_t kain_native_http_server_listen(int64_t server_id);
int64_t kain_native_http_server_local_port(int64_t server_id);
int64_t kain_native_http_server_route_actor(
    int64_t server_id,
    const char* method,
    const char* path,
    int64_t actor_id,
    const char* message_kind
);
int64_t kain_native_http_server_pump(int64_t server_id, int64_t timeout_ms);
int64_t kain_native_http_server_next_request(int64_t server_id);
const char* kain_native_http_request_method(int64_t incoming_request_id);
const char* kain_native_http_request_path(int64_t incoming_request_id);
const char* kain_native_http_request_query(int64_t incoming_request_id);
const char* kain_native_http_request_header(int64_t incoming_request_id, const char* key);
const char* kain_native_http_request_body_text(int64_t incoming_request_id);
const char* kain_native_http_request_body_hex(int64_t incoming_request_id);
int64_t kain_native_http_respond_text(int64_t incoming_request_id, int64_t status_code, const char* payload);
int64_t kain_native_http_respond_hex(int64_t incoming_request_id, int64_t status_code, const char* payload_hex);
int64_t kain_native_http_response_set_header_for_request(
    int64_t incoming_request_id,
    const char* key,
    const char* value
);
int64_t kain_native_http_server_close(int64_t server_id);
const char* kain_native_http_local_url(int64_t port, const char* path);

int64_t kain_native_net_last_status(void);
const char* kain_native_net_last_error_kind(void);
const char* kain_native_net_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif
