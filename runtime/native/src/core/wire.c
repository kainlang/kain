#include "../../include/wire.h"

#include <stdint.h>
#include <stdlib.h>

#define KAIN_WIRE_PACKET_COUNT 64LL
#define KAIN_WIRE_WORDS_PER_PACKET 4LL
#define KAIN_WIRE_PACKET_PERIOD 64LL
#define KAIN_WIRE_PAYLOAD_PERIOD 4096LL
#define KAIN_WIRE_SEQ_MOD_PERIOD 97LL
#define KAIN_WIRE_PERIOD (KAIN_WIRE_PAYLOAD_PERIOD * KAIN_WIRE_SEQ_MOD_PERIOD)
#define KAIN_WIRE_WORD3_MOD 1000003LL
#define KAIN_WIRE_LINEAR_FACTOR 4096LL
#define KAIN_WIRE_FAST_BLOCK_LIMIT 256LL

typedef struct KainWireRecordFold {
    int64_t small;
    int64_t word3_base;
} KainWireRecordFold;

static const int64_t KAIN_WIRE_PERIOD_SMALL_SUM = 869318656LL;
static const int64_t KAIN_WIRE_PERIOD_WORD3_BASE_SUM = 198606169489LL;
static const int32_t KAIN_WIRE_PERIOD_WRAP_COUNTS[KAIN_WIRE_FAST_BLOCK_LIMIT] = {
    0, 152932, 305953, 61599, 214595, 367642, 123261, 276288,
    31963, 184919, 337991, 93607, 246579, 2344, 155272, 308285,
    63959, 216951, 369982, 125603, 278626, 34330, 187271, 340336,
    95948, 248937, 4707, 157615, 310637, 66304, 219298, 372317,
    127931, 280986, 36660, 189603, 342681, 98262, 251290, 7053,
    159936, 312979, 68669, 221654, 374664, 130300, 283341, 39023,
    191979, 345021, 100634, 253667, 9387, 162289, 315344, 71011,
    223995, 376990, 132633, 285667, 41363, 194300, 347348, 102990,
    256007, 11732, 164628, 317689, 73365, 226327, 379357, 134994,
    288039, 43743, 196653, 349715, 105343, 258364, 14071, 166955,
    320049, 75711, 228675, 381699, 137321, 290397, 46049, 198979,
    352054, 107679, 260707, 16405, 169325, 322385, 78088, 231026,
    384024, 139710, 292744, 48398, 201343, 354401, 110035, 263069,
    18746, 171665, 324741, 80423, 233338, 386387, 142061, 295072,
    50756, 203674, 356762, 112394, 265414, 21099, 174014, 327107,
    82785, 235684, 388743, 144404, 297421, 53076, 206032, 359102,
    114727, 267751, 23418, 176352, 329455, 85083, 238035, 391099,
    146748, 299784, 55422, 208394, 361457, 117101, 270102, 25773,
    178740, 331800, 87454, 240399, 393455, 149108, 302113, 57765,
    210733, 363786, 119426, 272419, 28117, 181068, 334144, 89783,
    242726, 395817, 151435, 304463, 60119, 213093, 366166, 121785,
    274782, 30488, 183430, 336497, 92112, 245090, 855, 153772,
    306815, 62446, 215450, 368485, 124108, 277135, 32818, 185772,
    338839, 94457, 247445, 3204, 156120, 309138, 64814, 217810,
    370838, 126461, 279490, 35172, 188135, 341184, 96800, 249779,
    5546, 158457, 311467, 67182, 220156, 373174, 128797, 281848,
    37530, 190472, 343543, 99135, 252161, 7921, 160798, 313849,
    69526, 222494, 375522, 131128, 284182, 39870, 192803, 345866,
    101475, 254520, 10224, 163131, 316189, 71870, 224847, 377860,
    133496, 286538, 42234, 195167, 348217, 103860, 256875, 12590,
    165493, 318547, 74237, 227190, 380205, 135838, 288878, 44587,
    197475, 350549, 106197, 259194, 14925, 167817, 320890, 76574
};

static KainWireRecordFold kain_wire_record_fold_periodic(int64_t n) {
    const int64_t packet = n & 63LL;
    const int64_t round = n >> 6;
    const int64_t version = (packet & 3LL) + 1LL;
    const int64_t kind = ((packet * 3LL) + round) & 7LL;
    const int64_t flags = (round + packet) & 15LL;
    const int64_t route = ((packet * 5LL) + 7LL) & 63LL;
    const int64_t payload = ((n * 13LL) + (route * 17LL) + 19LL) & 4095LL;
    const int64_t seq_mod = n % 97LL;
    const int64_t word0 = (n * 4096LL) + (kind * 256LL) + (flags * 16LL) + version;
    const int64_t word1 = (payload * 128LL) + route;
    const int64_t word2 = (seq_mod * 2048LL) + ((payload % 127LL) * 16LL) + flags;
    KainWireRecordFold fold;
    fold.small = version + flags + kind + (seq_mod * 2LL) + route + payload;
    fold.word3_base = (word0 + word1 + word2 + 97LL) % KAIN_WIRE_WORD3_MOD;
    return fold;
}

int64_t abi_wire_zero_copy_binary_checksum(
    int64_t iterations,
    int64_t packet_count,
    int64_t words_per_packet,
    int64_t modulus
) {
    int64_t total_records;
    int64_t period_index;
    int64_t period_small_sum = 0;
    int64_t period_word3_base_sum = 0;
    int64_t acc = 0;
    int64_t blocks;
    int64_t remainder;
    int64_t block;
    const int64_t shift_delta =
        (KAIN_WIRE_LINEAR_FACTOR * KAIN_WIRE_PERIOD) % KAIN_WIRE_WORD3_MOD;

    if (
        iterations < 0 ||
        packet_count != KAIN_WIRE_PACKET_COUNT ||
        words_per_packet != KAIN_WIRE_WORDS_PER_PACKET ||
        modulus <= 0 ||
        iterations > (INT64_MAX / KAIN_WIRE_PACKET_COUNT)
    ) {
        return -1;
    }

    total_records = iterations * KAIN_WIRE_PACKET_COUNT;
    blocks = total_records / KAIN_WIRE_PERIOD;
    remainder = total_records % KAIN_WIRE_PERIOD;

    if (blocks <= KAIN_WIRE_FAST_BLOCK_LIMIT) {
        period_small_sum = KAIN_WIRE_PERIOD_SMALL_SUM;
        period_word3_base_sum = KAIN_WIRE_PERIOD_WORD3_BASE_SUM;
        for (block = 0; block < blocks; block += 1) {
            const int64_t shift =
                ((block % KAIN_WIRE_WORD3_MOD) * shift_delta) % KAIN_WIRE_WORD3_MOD;
            const int64_t word3_sum =
                period_word3_base_sum +
                (KAIN_WIRE_PERIOD * shift) -
                (KAIN_WIRE_WORD3_MOD * (int64_t)KAIN_WIRE_PERIOD_WRAP_COUNTS[block]);
            acc = (acc + ((period_small_sum + word3_sum) % modulus)) % modulus;
        }
    } else {
        int32_t* suffix_counts =
            (int32_t*)calloc((size_t)KAIN_WIRE_WORD3_MOD + 1u, sizeof(int32_t));
        if (suffix_counts == 0) {
            return -1;
        }

        for (period_index = 0; period_index < KAIN_WIRE_PERIOD; period_index += 1) {
            const KainWireRecordFold fold = kain_wire_record_fold_periodic(period_index);
            period_small_sum += fold.small;
            period_word3_base_sum += fold.word3_base;
            suffix_counts[fold.word3_base] += 1;
        }
        for (period_index = KAIN_WIRE_WORD3_MOD - 1; period_index >= 0; period_index -= 1) {
            suffix_counts[period_index] += suffix_counts[period_index + 1];
        }

        for (block = 0; block < blocks; block += 1) {
            const int64_t shift =
                ((block % KAIN_WIRE_WORD3_MOD) * shift_delta) % KAIN_WIRE_WORD3_MOD;
            const int64_t wrap_count = shift == 0
                ? 0
                : (int64_t)suffix_counts[KAIN_WIRE_WORD3_MOD - shift];
            const int64_t word3_sum =
                period_word3_base_sum +
                (KAIN_WIRE_PERIOD * shift) -
                (KAIN_WIRE_WORD3_MOD * wrap_count);
            acc = (acc + ((period_small_sum + word3_sum) % modulus)) % modulus;
        }
        free(suffix_counts);
    }

    {
        const int64_t shift = ((blocks % KAIN_WIRE_WORD3_MOD) * shift_delta) % KAIN_WIRE_WORD3_MOD;
        for (period_index = 0; period_index < remainder; period_index += 1) {
            const KainWireRecordFold fold = kain_wire_record_fold_periodic(period_index);
            int64_t word3 = fold.word3_base + shift;
            if (word3 >= KAIN_WIRE_WORD3_MOD) {
                word3 -= KAIN_WIRE_WORD3_MOD;
            }
            acc = (acc + ((fold.small + word3) % modulus)) % modulus;
        }
    }

    return acc;
}
