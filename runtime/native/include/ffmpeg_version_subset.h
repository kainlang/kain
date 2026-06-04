#ifndef KAIN_FFMPEG_VERSION_SUBSET_H
#define KAIN_FFMPEG_VERSION_SUBSET_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Scalar-safe FFmpeg system-header subset for Kain angle-bracket imports.
 * The real public FFmpeg headers remain available to bridge C sources; this
 * header keeps authored `include <libav*/...> as ...` calls extractor-safe.
 */
unsigned int avutil_version(void);
unsigned int avcodec_version(void);
unsigned int avformat_version(void);
unsigned int swscale_version(void);

#ifdef __cplusplus
}
#endif

#endif
