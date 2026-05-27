#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

#include "runtime_tiers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* label;
    const char* file;
    uint32_t line;
    uint32_t depth;
    uint64_t token;
    uint64_t start_ns;
    uint8_t active;
} KainProfileScope;

void kain_profile_scope_begin(
    KainProfileScope* scope,
    const char* label,
    const char* file,
    uint32_t line
);

void kain_profile_scope_end(KainProfileScope* scope);
void kain_profile_reset(void);
uint64_t kain_profile_zone_count(void);
uint64_t kain_profile_total_ns(void);
uint64_t kain_profile_last_duration_ns(void);
const char* kain_profile_last_label(void);

#if KAIN_RUNTIME_PROFILE_TIER == KAIN_RUNTIME_TIER_NOOP
#define KAIN_PROFILE_SCOPE(label) for (int _kain_profile_once = 1; _kain_profile_once; _kain_profile_once = 0)
#else
#define KAIN_PROFILE_SCOPE(label) \
    for (KainProfileScope _kain_profile_scope, *_kain_profile_once = \
             (kain_profile_scope_begin(&_kain_profile_scope, (label), __FILE__, (uint32_t)__LINE__), &_kain_profile_scope); \
         _kain_profile_once != 0; \
         kain_profile_scope_end(&_kain_profile_scope), _kain_profile_once = 0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROFILE_H */
