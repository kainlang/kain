#include "../include/tiny_math.h"

struct TinyOpaque {
    int seed;
};

int tiny_add(int left, int right) {
    return left + right;
}

double tiny_gain(double value) {
    return value * 1.5;
}

int tiny_apply_callback(int value, tiny_math_callback callback) {
    if (callback == 0) {
        return value;
    }
    return callback(value);
}

TinyOpaque* tiny_context(void) {
    static TinyOpaque context = { 17 };
    return &context;
}

TinyPair tiny_make_pair(int left, int right) {
    TinyPair pair;
    pair.left = left;
    pair.right = right;
    return pair;
}
