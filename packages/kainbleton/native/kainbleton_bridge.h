#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int kb_signature(int frames, int tracks, int clips, int salt);
int kb_meter_color(int track, int frame, int seed);
const char *kb_label(void);

#ifdef __cplusplus
}
#endif
