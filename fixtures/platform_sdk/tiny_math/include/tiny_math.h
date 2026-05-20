#ifndef TINY_MATH_H
#define TINY_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TinyPair {
    int left;
    int right;
} TinyPair;

typedef struct TinyOpaque TinyOpaque;
typedef int (*tiny_math_callback)(int value);

int tiny_add(int left, int right);
double tiny_gain(double value);
int tiny_apply_callback(int value, tiny_math_callback callback);
TinyOpaque* tiny_context(void);
TinyPair tiny_make_pair(int left, int right);

#ifdef __cplusplus
}
#endif

#endif /* TINY_MATH_H */
