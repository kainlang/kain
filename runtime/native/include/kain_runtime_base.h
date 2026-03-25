#ifndef KAIN_RUNTIME_BASE_H
#define KAIN_RUNTIME_BASE_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <ws2tcpip.h>
#include <gl/GL.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    long long ref_count;
    long long weak_count;
    long long type_tag;
    void (*destructor)(void*);
} RcHeader;

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
    long long value;
    int occupied;
} MapEntry;

typedef struct {
    MapEntry* entries;
    long long capacity;
    long long count;
} KainMap;

typedef struct MessageNode {
    long long type_tag;
    void* data;
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
int deep_eq(void* a, void* b);

#endif
