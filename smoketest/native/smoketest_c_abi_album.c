#include "smoketest_c_abi_album.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear.h"

#include "utarray.h"
#include "uthash.h"
#include "utlist.h"
#include "utringbuffer.h"
#include "utstack.h"
#include "utstring.h"

enum {
    SMOKETEST_C_ABI_MODULUS = 1000000007,
    SMOKETEST_C_ABI_RING_CAPACITY = 8,
    SMOKETEST_C_ABI_MAX_ROUNDS = 12,
    SMOKETEST_C_ABI_SIGNATURE_CAPACITY = 512
};

typedef struct smoke_hash_row {
    int key;
    int value;
    UT_hash_handle hh;
} smoke_hash_row;

typedef struct smoke_list_row {
    int value;
    struct smoke_list_row *prev;
    struct smoke_list_row *next;
} smoke_list_row;

typedef struct smoke_stack_row {
    int value;
    struct smoke_stack_row *next;
} smoke_stack_row;

typedef struct smoke_album_metrics {
    int score;
    int command_count;
    int ring_tail;
    bool hot;
    char signature[SMOKETEST_C_ABI_SIGNATURE_CAPACITY];
} smoke_album_metrics;

/*
 * This repo snapshot carries a declaration-only copy of nuklear.h, so the
 * smoketest provides a tiny headless compatibility shim for the subset of the
 * public Nuklear ABI we exercise here.
 */
nk_bool nk_init_default(struct nk_context *ctx, const struct nk_user_font *font) {
    (void)font;
    memset(ctx, 0, sizeof(*ctx));
    return 1;
}

struct nk_rect nk_rect(float x, float y, float w, float h) {
    struct nk_rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    return rect;
}

nk_bool nk_begin(struct nk_context *ctx, const char *title, struct nk_rect bounds, nk_flags flags) {
    (void)ctx;
    (void)title;
    (void)bounds;
    (void)flags;
    return 1;
}

void nk_layout_row_dynamic(struct nk_context *ctx, float height, int cols) {
    (void)ctx;
    (void)height;
    (void)cols;
}

void nk_label(struct nk_context *ctx, const char *text, nk_flags align) {
    (void)ctx;
    (void)text;
    (void)align;
}

nk_bool nk_button_label(struct nk_context *ctx, const char *title) {
    (void)ctx;
    return title != NULL && title[0] != '\0';
}

nk_bool nk_property_int(
    struct nk_context *ctx,
    const char *name,
    int min,
    int *val,
    int max,
    int step,
    float inc_per_pixel
) {
    (void)ctx;
    (void)name;
    (void)step;
    (void)inc_per_pixel;
    if (val == NULL) {
        return 0;
    }
    if (*val < min) {
        *val = min;
    }
    if (*val > max) {
        *val = max;
    }
    return 1;
}

void nk_end(struct nk_context *ctx) {
    (void)ctx;
}

void nk_free(struct nk_context *ctx) {
    (void)ctx;
}

static int smoke_c_abi_clamp_rounds(int rounds) {
    if (rounds < 3) {
        return 3;
    }
    if (rounds > SMOKETEST_C_ABI_MAX_ROUNDS) {
        return SMOKETEST_C_ABI_MAX_ROUNDS;
    }
    return rounds;
}

static int smoke_c_abi_mix(int seed, int round) {
    long long value = (long long)(seed + 17) * (long long)(round + 3) * 29LL;
    value ^= (long long)(seed << ((round % 5) + 1));
    value += (long long)(round * 97) + 131LL;
    value %= SMOKETEST_C_ABI_MODULUS;
    if (value < 0) {
        value += SMOKETEST_C_ABI_MODULUS;
    }
    return (int)value;
}

static int smoke_c_abi_nuklear_command_count(const char *signature, int ring_tail) {
    struct nk_context ctx;
    int command_count = 0;
    int slider_value = ring_tail % 17;

    if (!nk_init_default(&ctx, NULL)) {
        return -1;
    }

    if (nk_begin(&ctx, "smoketest.c_abi_album", nk_rect(0.0f, 0.0f, 420.0f, 240.0f),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(&ctx, 24.0f, 1);
        nk_label(&ctx, "headless nuklear probe", NK_TEXT_LEFT);
        command_count += 1;
        nk_layout_row_dynamic(&ctx, 20.0f, 1);
        nk_label(&ctx, signature, NK_TEXT_LEFT);
        command_count += 1;
        nk_layout_row_dynamic(&ctx, 28.0f, 2);
        command_count += nk_button_label(&ctx, "ignite");
        command_count += nk_button_label(&ctx, "certify");
        nk_layout_row_dynamic(&ctx, 22.0f, 1);
        command_count += nk_property_int(&ctx, "tail", 0, &slider_value, 64, 1, 1.0f);
    }
    nk_end(&ctx);

    nk_free(&ctx);
    return command_count;
}

static smoke_album_metrics smoke_c_abi_collect_metrics(int seed, int rounds) {
    smoke_album_metrics metrics;
    UT_array *values = NULL;
    UT_ringbuffer ring;
    UT_string *signature = NULL;
    smoke_hash_row *hash_rows = NULL;
    smoke_hash_row *hash_row = NULL;
    smoke_hash_row *hash_tmp = NULL;
    smoke_list_row *list_rows = NULL;
    smoke_list_row *list_row = NULL;
    smoke_list_row *list_tmp = NULL;
    smoke_stack_row *stack_rows = NULL;
    smoke_stack_row *stack_row = NULL;
    smoke_stack_row *popped = NULL;
    int array_sum = 0;
    int hash_sum = 0;
    int list_sum = 0;
    int stack_sum = 0;
    int ring_sum = 0;
    int hash_count = 0;
    int list_count = 0;
    int stack_count = 0;
    int round = 0;

    memset(&metrics, 0, sizeof(metrics));
    memset(metrics.signature, 0, sizeof(metrics.signature));
    rounds = smoke_c_abi_clamp_rounds(rounds);

    utarray_new(values, &ut_int_icd);
    utringbuffer_init(&ring, SMOKETEST_C_ABI_RING_CAPACITY, &ut_int_icd);
    utstring_new(signature);

    for (round = 0; round < rounds; ++round) {
        int value = smoke_c_abi_mix(seed, round);
        int weighted = (value + ((round + 1) * 31)) % SMOKETEST_C_ABI_MODULUS;

        utarray_push_back(values, &value);
        array_sum = (array_sum + value) % SMOKETEST_C_ABI_MODULUS;
        utringbuffer_push_back(&ring, &weighted);

        hash_row = (smoke_hash_row*)malloc(sizeof(*hash_row));
        list_row = (smoke_list_row*)malloc(sizeof(*list_row));
        stack_row = (smoke_stack_row*)malloc(sizeof(*stack_row));

        if (!hash_row || !list_row || !stack_row) {
            free(hash_row);
            free(list_row);
            free(stack_row);
            break;
        }

        hash_row->key = round + 1;
        hash_row->value = weighted;
        HASH_ADD_INT(hash_rows, key, hash_row);

        list_row->value = weighted + round + 5;
        list_row->prev = NULL;
        list_row->next = NULL;
        DL_APPEND(list_rows, list_row);

        stack_row->value = value ^ ((round + 7) * 9);
        stack_row->next = NULL;
        STACK_PUSH(stack_rows, stack_row);
    }

    HASH_ITER(hh, hash_rows, hash_row, hash_tmp) {
        hash_sum = (hash_sum + hash_row->value) % SMOKETEST_C_ABI_MODULUS;
        hash_count += 1;
    }

    DL_FOREACH(list_rows, list_row) {
        list_sum = (list_sum + list_row->value) % SMOKETEST_C_ABI_MODULUS;
        list_count += 1;
    }

    while (!STACK_EMPTY(stack_rows)) {
        STACK_POP(stack_rows, popped);
        stack_sum = (stack_sum + popped->value) % SMOKETEST_C_ABI_MODULUS;
        stack_count += 1;
        free(popped);
    }

    {
        int *ring_value = (int*)utringbuffer_front(&ring);
        while (ring_value != NULL) {
            metrics.ring_tail = *ring_value;
            ring_sum = (ring_sum + *ring_value) % SMOKETEST_C_ABI_MODULUS;
            ring_value = (int*)utringbuffer_next(&ring, ring_value);
        }
    }

    utstring_printf(
        signature,
        "seed=%d|rounds=%d|arr=%u|hash=%d|list=%d|stack=%d|ring=%u|tail=%d",
        seed,
        rounds,
        (unsigned)utarray_len(values),
        hash_count,
        list_count,
        stack_count,
        (unsigned)utringbuffer_len(&ring),
        metrics.ring_tail
    );

    metrics.command_count = smoke_c_abi_nuklear_command_count(utstring_body(signature), metrics.ring_tail);
    metrics.hot =
        metrics.command_count >= 4 &&
        hash_count == rounds &&
        list_count == rounds &&
        stack_count == rounds &&
        !utringbuffer_empty(&ring);

    snprintf(
        metrics.signature,
        sizeof(metrics.signature),
        "%s|nk=%d|hot=%d",
        utstring_body(signature),
        metrics.command_count,
        metrics.hot ? 1 : 0
    );

    metrics.score =
        (array_sum +
         hash_sum +
         list_sum +
         stack_sum +
         ring_sum +
         metrics.ring_tail +
         metrics.command_count +
         (int)strlen(metrics.signature)) % SMOKETEST_C_ABI_MODULUS;
    if (metrics.score < 0) {
        metrics.score += SMOKETEST_C_ABI_MODULUS;
    }

    HASH_ITER(hh, hash_rows, hash_row, hash_tmp) {
        HASH_DEL(hash_rows, hash_row);
        free(hash_row);
    }

    DL_FOREACH_SAFE(list_rows, list_row, list_tmp) {
        DL_DELETE(list_rows, list_row);
        free(list_row);
    }

    if (signature != NULL) {
        utstring_free(signature);
    }
    if (values != NULL) {
        utarray_free(values);
    }
    utringbuffer_done(&ring);

    return metrics;
}

int smoketest_c_abi_album_score(int seed, int rounds) {
    return smoke_c_abi_collect_metrics(seed, rounds).score;
}

int smoketest_c_abi_album_command_count(int seed, int rounds) {
    return smoke_c_abi_collect_metrics(seed, rounds).command_count;
}

int smoketest_c_abi_album_ring_tail(int seed, int rounds) {
    return smoke_c_abi_collect_metrics(seed, rounds).ring_tail;
}

int smoketest_c_abi_album_signature_span(int seed, int rounds) {
    smoke_album_metrics metrics = smoke_c_abi_collect_metrics(seed, rounds);
    return (int)strlen(metrics.signature);
}

const char* smoketest_c_abi_album_signature(int seed, int rounds) {
    enum { SMOKETEST_C_ABI_SIGNATURE_SLOTS = 4 };
    static char signature_slots[SMOKETEST_C_ABI_SIGNATURE_SLOTS][SMOKETEST_C_ABI_SIGNATURE_CAPACITY];
    static int signature_slot = 0;
    smoke_album_metrics metrics = smoke_c_abi_collect_metrics(seed, rounds);
    char *signature = signature_slots[signature_slot];
    signature_slot = (signature_slot + 1) % SMOKETEST_C_ABI_SIGNATURE_SLOTS;
    snprintf(signature, SMOKETEST_C_ABI_SIGNATURE_CAPACITY, "%s", metrics.signature);
    return signature;
}

_Bool smoketest_c_abi_album_hot(int seed, int rounds) {
    return smoke_c_abi_collect_metrics(seed, rounds).hot ? 1 : 0;
}
