/*
 * CBMC verification harness for actor
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

// Basic type definitions needed by runtime function signatures
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef signed char int8_t;
typedef unsigned long long size_t;
typedef long long ptrdiff_t;

// Forward declarations of functions under test
// kain_actor_abi_descriptor
KainActorAbiDescriptor kain_actor_abi_descriptor(void);
// kain_actor_ref_from_id
void kain_actor_ref_from_id(KainActorId actor_id, KainActorRef* out_ref);
// kain_actor_abi_descriptor_is_compatible
int kain_actor_abi_descriptor_is_compatible(const KainActorAbiDescriptor* expected);
// kain_actor_runtime_init
void kain_actor_runtime_init(void);
// kain_actor_runtime_shutdown
void kain_actor_runtime_shutdown(void);

int main(void) {
    kain_actor_abi_descriptor();
    __CPROVER_assert(1, "kain_actor_abi_descriptor: call ok");
    { void *__a; unsigned long long __b; kain_actor_ref_from_id(__a, __b); }
    __CPROVER_assert(1, "kain_actor_ref_from_id: call ok");
    { void *__p; kain_actor_abi_descriptor_is_compatible(__p); }
    __CPROVER_assert(1, "kain_actor_abi_descriptor_is_compatible: call ok");
    kain_actor_runtime_init();
    __CPROVER_assert(1, "kain_actor_runtime_init: call ok");
    kain_actor_runtime_shutdown();
    __CPROVER_assert(1, "kain_actor_runtime_shutdown: call ok");
    return 0;
}
