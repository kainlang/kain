#ifndef KAIN_FFMPEG_BRIDGE_H
#define KAIN_FFMPEG_BRIDGE_H

#if defined(_WIN32)
#define KAIN_FFMPEG_BRIDGE_EXPORT __declspec(dllexport)
#else
#define KAIN_FFMPEG_BRIDGE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_avutil_version(void);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_avcodec_version(void);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_avformat_version(void);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_swscale_version(void);
KAIN_FFMPEG_BRIDGE_EXPORT const char* ffmpeg_bridge_configuration(void);

KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_open_media(const char* path);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_close_media(int media_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_best_video_stream(int media_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_stream_count(int media_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_duration_ms(int media_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_video_width(int media_handle, int stream_index);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_video_height(int media_handle, int stream_index);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_video_fps_num(int media_handle, int stream_index);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_video_fps_den(int media_handle, int stream_index);
KAIN_FFMPEG_BRIDGE_EXPORT const char* ffmpeg_bridge_video_codec_name(int media_handle, int stream_index);

KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_create(int media_handle, int stream_index);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_destroy(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_seek_ms(int decoder_handle, int timestamp_ms);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_decode_next(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_width(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_height(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_frame_index(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_frame_pts_ms(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_decoder_frame_word_count(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT long long ffmpeg_bridge_decoder_frame_checksum(int decoder_handle);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_copy_rgba_words(int decoder_handle, long long dst_words_address, int word_capacity);

KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_live_media_count(void);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_live_decoder_count(void);
KAIN_FFMPEG_BRIDGE_EXPORT int ffmpeg_bridge_last_status(void);
KAIN_FFMPEG_BRIDGE_EXPORT const char* ffmpeg_bridge_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
