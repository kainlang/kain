#include "net_system.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

static int expect_int(const char* label, long long actual, long long expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %lld, got %lld\n", label, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_positive(const char* label, long long actual) {
    if (actual <= 0) {
        fprintf(stderr, "%s: expected positive handle, got %lld\n", label, actual);
        return 1;
    }
    return 0;
}

static int expect_text_contains(const char* label, const char* actual, const char* expected_fragment) {
    if (actual == 0 || strstr(actual, expected_fragment) == 0) {
        fprintf(stderr, "%s: expected '%s' in '%s'\n", label, expected_fragment, actual ? actual : "");
        return 1;
    }
    return 0;
}

static int expect_non_empty_text(const char* label, const char* actual) {
    if (actual == 0 || actual[0] == '\0') {
        fprintf(stderr, "%s: expected non-empty text\n", label);
        return 1;
    }
    return 0;
}

typedef struct HttpServerThreadArgs {
    int64_t port;
    char response_text[4096];
} HttpServerThreadArgs;

static void sleep_millis(int milliseconds) {
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    usleep((useconds_t)milliseconds * 1000u);
#endif
}

static int run_tcp_loopback(void) {
    int64_t listener = abi_tcp_listen("127.0.0.1", 0);
    int64_t port;
    int64_t client;
    int64_t server;
    const char* server_text;
    const char* client_text;
    if (expect_positive("tcp listener", listener)) return 10;
    port = abi_tcp_listener_local_port(listener);
    if (expect_positive("tcp listener port", port)) return 11;
    client = abi_tcp_connect("127.0.0.1", port, 5000);
    if (expect_positive("tcp client", client)) return 12;
    server = abi_tcp_accept(listener, 5000);
    if (expect_positive("tcp accept", server)) return 13;
    if (expect_int("tcp client write", abi_tcp_write_text(client, "tcp-proof"), 0)) return 14;
    server_text = abi_tcp_read_text(server);
    if (expect_text_contains("tcp server read", server_text, "tcp-proof")) return 15;
    if (expect_int("tcp server write", abi_tcp_write_text(server, "tcp-echo"), 0)) return 16;
    client_text = abi_tcp_read_text(client);
    if (expect_text_contains("tcp client read", client_text, "tcp-echo")) return 17;
    abi_tcp_close(client);
    abi_tcp_close(server);
    abi_tcp_listener_close(listener);
    return 0;
}

static int run_net_capability_surface(void) {
    const char* platform_name = abi_net_platform_name();
    if (expect_non_empty_text("platform name", platform_name)) return 18;
    if (expect_int("tcp capability", abi_net_capability_state("tcp"), ABI_NET_CAPABILITY_AVAILABLE)) return 19;
    if (expect_int("http client capability", abi_net_capability_state("http.client"), ABI_NET_CAPABILITY_AVAILABLE)) return 20;
    if (expect_int("http server capability", abi_net_capability_state("http.server"), ABI_NET_CAPABILITY_AVAILABLE)) return 21;
#ifdef _WIN32
    if (expect_text_contains("platform name windows", platform_name, "windows")) return 22;
    if (expect_int("tls client capability", abi_net_capability_state("tls.client"), ABI_NET_CAPABILITY_AVAILABLE)) return 23;
    if (expect_int("http2 client capability", abi_net_capability_state("http2.client"), ABI_NET_CAPABILITY_DEGRADED)) return 24;
#else
    if (expect_int("tls client capability", abi_net_capability_state("tls.client"), ABI_NET_CAPABILITY_UNAVAILABLE)) return 23;
    if (expect_int("http2 client capability", abi_net_capability_state("http2.client"), ABI_NET_CAPABILITY_UNAVAILABLE)) return 24;
#endif
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI http_server_client_thread(void* data)
#else
static void* http_server_client_thread(void* data)
#endif
{
    HttpServerThreadArgs* args = (HttpServerThreadArgs*)data;
    int64_t client;
    const char* response;
    sleep_millis(100);
    client = abi_tcp_connect("127.0.0.1", args->port, 5000);
    if (client > 0) {
        abi_tcp_write_text(
            client,
            "POST /actor?proof=1 HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 11\r\n\r\nhello-actor"
        );
        response = abi_tcp_read_text(client);
        snprintf(args->response_text, sizeof(args->response_text), "%s", response ? response : "");
        if (strstr(args->response_text, "actor-ok") == 0) {
            response = abi_tcp_read_text(client);
            snprintf(
                args->response_text + strlen(args->response_text),
                sizeof(args->response_text) - strlen(args->response_text),
                "%s",
                response ? response : ""
            );
        }
        abi_tcp_close(client);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

static int run_http_server_roundtrip(void) {
    HttpServerThreadArgs thread_args;
    int64_t server = abi_http_server_create("127.0.0.1", 0);
    int64_t port;
    int64_t incoming;
    int64_t next;
    const char* method;
    const char* path;
    const char* body;
    const char* protocol;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
    memset(&thread_args, 0, sizeof(thread_args));
    if (expect_positive("http server", server)) return 20;
    if (expect_int("http listen", abi_http_server_listen(server), 0)) return 21;
    port = abi_http_server_local_port(server);
    if (expect_positive("http port", port)) return 22;
    thread_args.port = port;
    if (expect_int("http route actor", abi_http_server_route_actor(server, "POST", "/actor", 0, "HttpRequest"), 0)) return 23;
#ifdef _WIN32
    thread_handle = CreateThread(0, 0, http_server_client_thread, &thread_args, 0, 0);
    if (thread_handle == 0) return 24;
#else
    if (pthread_create(&thread_handle, 0, http_server_client_thread, &thread_args) != 0) return 24;
#endif
    incoming = abi_http_server_pump(server, 5000);
    if (expect_positive("http incoming", incoming)) return 25;
    next = abi_http_server_next_request(server);
    if (expect_int("http next request", next, incoming)) return 26;
    if (expect_int("http pending request count", abi_http_server_pending_request_count(server), 0)) return 27;
    method = abi_http_request_method(incoming);
    path = abi_http_request_path(incoming);
    body = abi_http_request_body_text(incoming);
    protocol = abi_http_request_protocol(incoming);
    if (expect_text_contains("http method", method, "POST")) return 28;
    if (expect_text_contains("http path", path, "/actor")) return 29;
    if (expect_text_contains("http body", body, "hello-actor")) return 30;
    if (expect_text_contains("http protocol", protocol, "http/1.1")) return 31;
    if (expect_int("http respond", abi_http_respond_text(incoming, 201, "actor-ok"), 0)) return 32;
#ifdef _WIN32
    WaitForSingleObject(thread_handle, 5000);
    CloseHandle(thread_handle);
#else
    pthread_join(thread_handle, 0);
#endif
    if (expect_text_contains("http client response", thread_args.response_text, "actor-ok")) return 33;
    abi_http_server_close(server);
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI http_invalid_content_length_thread(void* data)
#else
static void* http_invalid_content_length_thread(void* data)
#endif
{
    HttpServerThreadArgs* args = (HttpServerThreadArgs*)data;
    int64_t client;
    sleep_millis(100);
    client = abi_tcp_connect("127.0.0.1", args->port, 5000);
    if (client > 0) {
        abi_tcp_write_text(
            client,
            "POST /broken HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: -1\r\n\r\nboom"
        );
        abi_tcp_close(client);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

static int run_http_invalid_content_length_rejected(void) {
    HttpServerThreadArgs thread_args;
    int64_t server = abi_http_server_create("127.0.0.1", 0);
    int64_t port;
    int64_t incoming;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
    memset(&thread_args, 0, sizeof(thread_args));
    if (expect_positive("invalid content-length server", server)) return 32;
    if (expect_int("invalid content-length listen", abi_http_server_listen(server), 0)) return 33;
    port = abi_http_server_local_port(server);
    if (expect_positive("invalid content-length port", port)) return 34;
    thread_args.port = port;
#ifdef _WIN32
    thread_handle = CreateThread(0, 0, http_invalid_content_length_thread, &thread_args, 0, 0);
    if (thread_handle == 0) return 35;
#else
    if (pthread_create(&thread_handle, 0, http_invalid_content_length_thread, &thread_args) != 0) return 35;
#endif
    incoming = abi_http_server_pump(server, 5000);
#ifdef _WIN32
    WaitForSingleObject(thread_handle, 5000);
    CloseHandle(thread_handle);
#else
    pthread_join(thread_handle, 0);
#endif
    if (expect_int("invalid content-length rejected", incoming, ABI_NET_PARSE_ERROR)) return 36;
    if (expect_text_contains("invalid content-length kind", abi_net_last_error_kind(), "parse")) return 37;
    if (expect_text_contains("invalid content-length message", abi_net_last_error_message(), "Content-Length")) return 38;
    abi_http_server_close(server);
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI http_client_test_server_thread(void* data)
#else
static void* http_client_test_server_thread(void* data)
#endif
{
    int64_t listener = *(int64_t*)data;
    int64_t server = abi_tcp_accept(listener, 5000);
    const char* request_text;
    if (server > 0) {
        request_text = abi_tcp_read_text(server);
        if (request_text != 0 && strstr(request_text, "GET /client") != 0) {
            abi_tcp_write_text(
                server,
                "HTTP/1.1 200 OK\r\nContent-Length: 15\r\nConnection: close\r\n\r\nclient-proof-ok"
            );
        }
        abi_tcp_close(server);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

static int run_http_client_roundtrip(void) {
    int64_t listener = abi_tcp_listen("127.0.0.1", 0);
    int64_t port;
    int64_t request;
    int64_t response;
    char url[256];
    const char* response_text;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
    if (expect_positive("client server listener", listener)) return 40;
    port = abi_tcp_listener_local_port(listener);
    snprintf(url, sizeof(url), "http://127.0.0.1:%lld/client", (long long)port);
#ifdef _WIN32
    thread_handle = CreateThread(0, 0, http_client_test_server_thread, &listener, 0, 0);
    if (thread_handle == 0) return 41;
#else
    if (pthread_create(&thread_handle, 0, http_client_test_server_thread, &listener) != 0) return 41;
#endif
    request = abi_http_request_create("GET", url);
    if (expect_positive("http client request", request)) return 42;
    if (expect_int("http client request protocol", abi_http_request_set_protocol(request, "http/1.1"), 0)) return 43;
    response = abi_http_client_send(request);
    if (response <= 0) {
        fprintf(
            stderr,
            "http client response diagnostics: %s - %s\n",
            abi_net_last_error_kind(),
            abi_net_last_error_message()
        );
        if (expect_positive("http client response", response)) return 44;
    }
    if (expect_int("http client status", abi_http_response_status(response), 200)) return 45;
    if (expect_text_contains("http client protocol", abi_http_response_protocol(response), "http/1.1")) return 46;
    response_text = abi_http_response_body_text(response);
    if (expect_text_contains("http client body", response_text, "client-proof-ok")) return 47;
#ifdef _WIN32
    WaitForSingleObject(thread_handle, 5000);
    CloseHandle(thread_handle);
#else
    pthread_join(thread_handle, 0);
#endif
    abi_tcp_listener_close(listener);
    return 0;
}

static int run_http_server_request_slot_reuse(void) {
    int64_t server = abi_http_server_create("127.0.0.1", 0);
    int64_t port;
    int round;
    if (expect_positive("reuse server", server)) return 48;
    if (expect_int("reuse listen", abi_http_server_listen(server), 0)) return 49;
    port = abi_http_server_local_port(server);
    if (expect_positive("reuse port", port)) return 50;
    for (round = 0; round < 96; ++round) {
        int64_t client = abi_tcp_connect("127.0.0.1", port, 5000);
        int64_t incoming;
        int64_t next;
        const char* response_text;
        if (expect_positive("reuse client", client)) return 51;
        if (expect_int(
                "reuse write",
                abi_tcp_write_text(client, "GET /reuse HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"),
                0
            )) return 52;
        incoming = abi_http_server_pump(server, 5000);
        if (incoming <= 0) {
            fprintf(
                stderr,
                "reuse pump diagnostics round=%d: %s - %s\n",
                round,
                abi_net_last_error_kind(),
                abi_net_last_error_message()
            );
            return 53;
        }
        next = abi_http_server_next_request(server);
        if (expect_int("reuse next", next, incoming)) return 54;
        if (expect_int("reuse respond", abi_http_respond_text(incoming, 200, "slot-ok"), 0)) return 55;
        response_text = abi_tcp_read_text(client);
        if (expect_text_contains("reuse response text", response_text, "slot-ok")) return 56;
        if (expect_int("reuse pending request count", abi_http_server_pending_request_count(server), 0)) return 57;
        abi_tcp_close(client);
    }
    abi_http_server_close(server);
    return 0;
}

int main(void) {
    int result;
    if (expect_int("reset", abi_net_reset(), 0)) return 1;
    if (expect_int("platform available", abi_net_platform_available(), 1)) return 2;
    result = run_tcp_loopback();
    if (result != 0) return result;
    result = run_net_capability_surface();
    if (result != 0) return result;
    result = run_http_server_roundtrip();
    if (result != 0) return result;
    result = run_http_invalid_content_length_rejected();
    if (result != 0) return result;
    result = run_http_client_roundtrip();
    if (result != 0) return result;
    result = run_http_server_request_slot_reuse();
    if (result != 0) return result;
    if (expect_int("final reset", abi_net_reset(), 0)) return 60;
    return 0;
}
