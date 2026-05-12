#include "kain_native_net_system.h"

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
    int64_t listener = kain_native_tcp_listen("127.0.0.1", 0);
    int64_t port;
    int64_t client;
    int64_t server;
    const char* server_text;
    const char* client_text;
    if (expect_positive("tcp listener", listener)) return 10;
    port = kain_native_tcp_listener_local_port(listener);
    if (expect_positive("tcp listener port", port)) return 11;
    client = kain_native_tcp_connect("127.0.0.1", port, 5000);
    if (expect_positive("tcp client", client)) return 12;
    server = kain_native_tcp_accept(listener, 5000);
    if (expect_positive("tcp accept", server)) return 13;
    if (expect_int("tcp client write", kain_native_tcp_write_text(client, "tcp-proof"), 0)) return 14;
    server_text = kain_native_tcp_read_text(server);
    if (expect_text_contains("tcp server read", server_text, "tcp-proof")) return 15;
    if (expect_int("tcp server write", kain_native_tcp_write_text(server, "tcp-echo"), 0)) return 16;
    client_text = kain_native_tcp_read_text(client);
    if (expect_text_contains("tcp client read", client_text, "tcp-echo")) return 17;
    kain_native_tcp_close(client);
    kain_native_tcp_close(server);
    kain_native_tcp_listener_close(listener);
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
    client = kain_native_tcp_connect("127.0.0.1", args->port, 5000);
    if (client > 0) {
        kain_native_tcp_write_text(
            client,
            "POST /actor?proof=1 HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 11\r\n\r\nhello-actor"
        );
        response = kain_native_tcp_read_text(client);
        snprintf(args->response_text, sizeof(args->response_text), "%s", response ? response : "");
        if (strstr(args->response_text, "actor-ok") == 0) {
            response = kain_native_tcp_read_text(client);
            snprintf(
                args->response_text + strlen(args->response_text),
                sizeof(args->response_text) - strlen(args->response_text),
                "%s",
                response ? response : ""
            );
        }
        kain_native_tcp_close(client);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

static int run_http_server_roundtrip(void) {
    HttpServerThreadArgs thread_args;
    int64_t server = kain_native_http_server_create("127.0.0.1", 0);
    int64_t port;
    int64_t incoming;
    int64_t next;
    const char* method;
    const char* path;
    const char* body;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
    memset(&thread_args, 0, sizeof(thread_args));
    if (expect_positive("http server", server)) return 20;
    if (expect_int("http listen", kain_native_http_server_listen(server), 0)) return 21;
    port = kain_native_http_server_local_port(server);
    if (expect_positive("http port", port)) return 22;
    thread_args.port = port;
    if (expect_int("http route actor", kain_native_http_server_route_actor(server, "POST", "/actor", 0, "HttpRequest"), 0)) return 23;
#ifdef _WIN32
    thread_handle = CreateThread(0, 0, http_server_client_thread, &thread_args, 0, 0);
    if (thread_handle == 0) return 24;
#else
    if (pthread_create(&thread_handle, 0, http_server_client_thread, &thread_args) != 0) return 24;
#endif
    incoming = kain_native_http_server_pump(server, 5000);
    if (expect_positive("http incoming", incoming)) return 25;
    next = kain_native_http_server_next_request(server);
    if (expect_int("http next request", next, incoming)) return 26;
    method = kain_native_http_request_method(incoming);
    path = kain_native_http_request_path(incoming);
    body = kain_native_http_request_body_text(incoming);
    if (expect_text_contains("http method", method, "POST")) return 27;
    if (expect_text_contains("http path", path, "/actor")) return 28;
    if (expect_text_contains("http body", body, "hello-actor")) return 29;
    if (expect_int("http respond", kain_native_http_respond_text(incoming, 201, "actor-ok"), 0)) return 30;
#ifdef _WIN32
    WaitForSingleObject(thread_handle, 5000);
    CloseHandle(thread_handle);
#else
    pthread_join(thread_handle, 0);
#endif
    if (expect_text_contains("http client response", thread_args.response_text, "actor-ok")) return 31;
    kain_native_http_server_close(server);
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
    client = kain_native_tcp_connect("127.0.0.1", args->port, 5000);
    if (client > 0) {
        kain_native_tcp_write_text(
            client,
            "POST /broken HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: -1\r\n\r\nboom"
        );
        kain_native_tcp_close(client);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

static int run_http_invalid_content_length_rejected(void) {
    HttpServerThreadArgs thread_args;
    int64_t server = kain_native_http_server_create("127.0.0.1", 0);
    int64_t port;
    int64_t incoming;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
    memset(&thread_args, 0, sizeof(thread_args));
    if (expect_positive("invalid content-length server", server)) return 32;
    if (expect_int("invalid content-length listen", kain_native_http_server_listen(server), 0)) return 33;
    port = kain_native_http_server_local_port(server);
    if (expect_positive("invalid content-length port", port)) return 34;
    thread_args.port = port;
#ifdef _WIN32
    thread_handle = CreateThread(0, 0, http_invalid_content_length_thread, &thread_args, 0, 0);
    if (thread_handle == 0) return 35;
#else
    if (pthread_create(&thread_handle, 0, http_invalid_content_length_thread, &thread_args) != 0) return 35;
#endif
    incoming = kain_native_http_server_pump(server, 5000);
#ifdef _WIN32
    WaitForSingleObject(thread_handle, 5000);
    CloseHandle(thread_handle);
#else
    pthread_join(thread_handle, 0);
#endif
    if (expect_int("invalid content-length rejected", incoming, KAIN_NATIVE_NET_PARSE_ERROR)) return 36;
    if (expect_text_contains("invalid content-length kind", kain_native_net_last_error_kind(), "parse")) return 37;
    if (expect_text_contains("invalid content-length message", kain_native_net_last_error_message(), "Content-Length")) return 38;
    kain_native_http_server_close(server);
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI http_client_test_server_thread(void* data)
#else
static void* http_client_test_server_thread(void* data)
#endif
{
    int64_t listener = *(int64_t*)data;
    int64_t server = kain_native_tcp_accept(listener, 5000);
    const char* request_text;
    if (server > 0) {
        request_text = kain_native_tcp_read_text(server);
        if (request_text != 0 && strstr(request_text, "GET /client") != 0) {
            kain_native_tcp_write_text(
                server,
                "HTTP/1.1 200 OK\r\nContent-Length: 15\r\nConnection: close\r\n\r\nclient-proof-ok"
            );
        }
        kain_native_tcp_close(server);
    }
#ifdef _WIN32
    return 0;
#else
    return 0;
#endif
}

static int run_http_client_roundtrip(void) {
    int64_t listener = kain_native_tcp_listen("127.0.0.1", 0);
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
    port = kain_native_tcp_listener_local_port(listener);
    snprintf(url, sizeof(url), "http://127.0.0.1:%lld/client", (long long)port);
#ifdef _WIN32
    thread_handle = CreateThread(0, 0, http_client_test_server_thread, &listener, 0, 0);
    if (thread_handle == 0) return 41;
#else
    if (pthread_create(&thread_handle, 0, http_client_test_server_thread, &listener) != 0) return 41;
#endif
    request = kain_native_http_request_create("GET", url);
    if (expect_positive("http client request", request)) return 42;
    response = kain_native_http_client_send(request);
    if (response <= 0) {
        fprintf(
            stderr,
            "http client response diagnostics: %s - %s\n",
            kain_native_net_last_error_kind(),
            kain_native_net_last_error_message()
        );
        if (expect_positive("http client response", response)) return 43;
    }
    if (expect_int("http client status", kain_native_http_response_status(response), 200)) return 44;
    response_text = kain_native_http_response_body_text(response);
    if (expect_text_contains("http client body", response_text, "client-proof-ok")) return 45;
#ifdef _WIN32
    WaitForSingleObject(thread_handle, 5000);
    CloseHandle(thread_handle);
#else
    pthread_join(thread_handle, 0);
#endif
    kain_native_tcp_listener_close(listener);
    return 0;
}

int main(void) {
    int result;
    if (expect_int("reset", kain_native_net_reset(), 0)) return 1;
    if (expect_int("platform available", kain_native_net_platform_available(), 1)) return 2;
    result = run_tcp_loopback();
    if (result != 0) return result;
    result = run_http_server_roundtrip();
    if (result != 0) return result;
    result = run_http_invalid_content_length_rejected();
    if (result != 0) return result;
    result = run_http_client_roundtrip();
    if (result != 0) return result;
    if (expect_int("final reset", kain_native_net_reset(), 0)) return 60;
    return 0;
}
