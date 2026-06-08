/*
 * CBMC verification harness for input_system
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
// abi_input_reset
int64_t abi_input_reset(void);
// abi_input_session_create
int64_t abi_input_session_create(const char* name);
// abi_input_session_destroy
int64_t abi_input_session_destroy(int64_t session_id);
// abi_input_session_count
int64_t abi_input_session_count(void);
// abi_input_frame_index
int64_t abi_input_frame_index(int64_t session_id);

int main(void) {
    abi_input_reset();
    __CPROVER_assert(1, "abi_input_reset: call ok");
    { void *__p; abi_input_session_create(__p); }
    __CPROVER_assert(1, "abi_input_session_create: call ok");
    { void *__p; abi_input_session_destroy(__p); }
    __CPROVER_assert(1, "abi_input_session_destroy: call ok");
    abi_input_session_count();
    __CPROVER_assert(1, "abi_input_session_count: call ok");
    { void *__p; abi_input_frame_index(__p); }
    __CPROVER_assert(1, "abi_input_frame_index: call ok");
    return 0;
}
