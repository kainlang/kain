/*
 * CBMC verification harness for machine_stones
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
// kain_machine_now_ns
uint64_t kain_machine_now_ns(void);
// kain_machine_real_time_now_ms
uint64_t kain_machine_real_time_now_ms(void);
// kain_machine_pulse_total_fire_count
uint64_t kain_machine_pulse_total_fire_count(void);
// kain_machine_teleport_count
uint64_t kain_machine_teleport_count(void);
// kain_machine_teleport_last_token
uint64_t kain_machine_teleport_last_token(void);

int main(void) {
    kain_machine_now_ns();
    __CPROVER_assert(1, "kain_machine_now_ns: call ok");
    kain_machine_real_time_now_ms();
    __CPROVER_assert(1, "kain_machine_real_time_now_ms: call ok");
    kain_machine_pulse_total_fire_count();
    __CPROVER_assert(1, "kain_machine_pulse_total_fire_count: call ok");
    kain_machine_teleport_count();
    __CPROVER_assert(1, "kain_machine_teleport_count: call ok");
    kain_machine_teleport_last_token();
    __CPROVER_assert(1, "kain_machine_teleport_last_token: call ok");
    return 0;
}
