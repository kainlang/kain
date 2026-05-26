#ifndef PYTHON_LAB_BRIDGE_H
#define PYTHON_LAB_BRIDGE_H

int python_lab_bridge_bias(int value);
int python_lab_bridge_mix(int seed, int salt);
int python_lab_bridge_fold4(int a, int b, int c, int d);
int python_lab_bridge_window_route(int width, int height, int frames, int seed);

#endif
