#include "smoketest_interop_bridge.h"

int smoketest_bridge_bias(int value) {
    return (value * 3) + 17;
}

int smoketest_bridge_mix(int seed, int salt) {
    int lane = (seed * 31) + (salt * 17) + 13;
    int folded = lane ^ (seed << 2) ^ (salt << 1);
    if (folded < 0) {
        folded = -folded;
    }
    return folded % 1000000007;
}

int smoketest_bridge_fold(int seed, int salt, int turns) {
    int acc = seed + 97;
    int step = 0;
    while (step < turns) {
        acc = ((acc * 17) + salt + (step * 7) + 19) % 1000003;
        step += 1;
    }
    return acc;
}
