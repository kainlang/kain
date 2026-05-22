#ifndef SMOKETEST_INTEROP_BRIDGE_H
#define SMOKETEST_INTEROP_BRIDGE_H

int smoketest_bridge_bias(int value);
int smoketest_bridge_mix(int seed, int salt);
int smoketest_bridge_fold(int seed, int salt, int turns);

#endif
