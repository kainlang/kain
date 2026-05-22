#ifndef FANOUT_H
#define FANOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*KainFanoutIndexFn)(void* ctx, int64_t index);

int __kain_fanout_i64(int64_t start, int64_t end, void* ctx, KainFanoutIndexFn fn);
void kain_fanout_runtime_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* FANOUT_H */
