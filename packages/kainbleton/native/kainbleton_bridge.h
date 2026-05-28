#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int kainbleton_signature(int frames, int tracks, int clips, int salt);
int kainbleton_meter_color(int track, int frame, int seed);

#ifdef __cplusplus
}
#endif
