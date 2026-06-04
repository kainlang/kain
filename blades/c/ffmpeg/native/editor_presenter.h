#ifndef KAIN_EDITOR_PRESENTER_H
#define KAIN_EDITOR_PRESENTER_H

#if defined(_WIN32)
#define KAIN_EDITOR_PRESENTER_EXPORT __declspec(dllexport)
#else
#define KAIN_EDITOR_PRESENTER_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_open(const char* title, int width, int height);
KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_pump(int presenter_handle);
KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_should_close(int presenter_handle);
KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_present_rgba_words(
    int presenter_handle,
    long long words_address,
    int width,
    int height,
    int word_count,
    int playhead_ms,
    long long frame_checksum,
    int clip_count
);
KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_close(int presenter_handle);
KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_frame_count(int presenter_handle);
KAIN_EDITOR_PRESENTER_EXPORT long long editor_presenter_frame_hash(int presenter_handle);
KAIN_EDITOR_PRESENTER_EXPORT int editor_presenter_last_status(void);
KAIN_EDITOR_PRESENTER_EXPORT const char* editor_presenter_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
