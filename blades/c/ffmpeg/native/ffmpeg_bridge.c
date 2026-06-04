#include "ffmpeg_bridge.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    KAIN_FFMPEG_MAX_MEDIA = 16,
    KAIN_FFMPEG_MAX_DECODERS = 16,
    KAIN_FFMPEG_ERROR_CAP = 512,
    KAIN_FFMPEG_CODEC_NAME_CAP = 128
};

typedef struct KainFfmpegMedia {
    int used;
    AVFormatContext* format;
    char path[512];
} KainFfmpegMedia;

typedef struct KainFfmpegDecoder {
    int used;
    int media_handle;
    int stream_index;
    AVCodecContext* codec;
    AVPacket* packet;
    AVFrame* frame;
    struct SwsContext* sws;
    unsigned char* rgba;
    int rgba_bytes;
    int width;
    int height;
    int frame_index;
    int frame_pts_ms;
    int eof_sent;
} KainFfmpegDecoder;

static KainFfmpegMedia G_MEDIA[KAIN_FFMPEG_MAX_MEDIA];
static KainFfmpegDecoder G_DECODERS[KAIN_FFMPEG_MAX_DECODERS];
static char G_ERROR[KAIN_FFMPEG_ERROR_CAP] = "ok";
static int G_STATUS = 0;
static char G_CODEC_NAME[KAIN_FFMPEG_CODEC_NAME_CAP] = "unknown";

static int set_status(int status, const char* message) {
    G_STATUS = status;
    if (message && message[0]) {
        snprintf(G_ERROR, sizeof(G_ERROR), "%s", message);
    } else {
        snprintf(G_ERROR, sizeof(G_ERROR), "ok");
    }
    return status;
}

static int set_av_error(const char* stage, int rc) {
    char text[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(rc, text, sizeof(text));
    snprintf(G_ERROR, sizeof(G_ERROR), "%s: %s (%d)", stage ? stage : "ffmpeg", text, rc);
    G_STATUS = rc;
    return rc;
}

static KainFfmpegMedia* media_from_handle(int handle) {
    int index = handle - 1;
    if (index < 0 || index >= KAIN_FFMPEG_MAX_MEDIA || !G_MEDIA[index].used) {
        set_status(-1001, "invalid media handle");
        return NULL;
    }
    return &G_MEDIA[index];
}

static KainFfmpegDecoder* decoder_from_handle(int handle) {
    int index = handle - 1;
    if (index < 0 || index >= KAIN_FFMPEG_MAX_DECODERS || !G_DECODERS[index].used) {
        set_status(-1002, "invalid decoder handle");
        return NULL;
    }
    return &G_DECODERS[index];
}

static AVStream* media_stream(KainFfmpegMedia* media, int stream_index) {
    if (!media || !media->format || stream_index < 0 || stream_index >= (int)media->format->nb_streams) {
        set_status(-1003, "invalid stream index");
        return NULL;
    }
    return media->format->streams[stream_index];
}

static int media_slot(void) {
    for (int index = 0; index < KAIN_FFMPEG_MAX_MEDIA; ++index) {
        if (!G_MEDIA[index].used) {
            return index;
        }
    }
    return -1;
}

static int decoder_slot(void) {
    for (int index = 0; index < KAIN_FFMPEG_MAX_DECODERS; ++index) {
        if (!G_DECODERS[index].used) {
            return index;
        }
    }
    return -1;
}

static int ensure_rgba(KainFfmpegDecoder* decoder) {
    int byte_count = decoder->width * decoder->height * 4;
    if (byte_count <= 0) {
        return set_status(-1004, "invalid decoder dimensions");
    }
    if (decoder->rgba && decoder->rgba_bytes >= byte_count) {
        return 0;
    }
    if (decoder->rgba) {
        av_free(decoder->rgba);
        decoder->rgba = NULL;
        decoder->rgba_bytes = 0;
    }
    decoder->rgba = (unsigned char*)av_malloc((size_t)byte_count);
    if (!decoder->rgba) {
        return set_status(-1005, "failed to allocate rgba frame buffer");
    }
    decoder->rgba_bytes = byte_count;
    return 0;
}

static int convert_frame(KainFfmpegDecoder* decoder, AVStream* stream) {
    int status = ensure_rgba(decoder);
    if (status != 0) {
        return status;
    }
    decoder->sws = sws_getCachedContext(
        decoder->sws,
        decoder->codec->width,
        decoder->codec->height,
        decoder->codec->pix_fmt,
        decoder->width,
        decoder->height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );
    if (!decoder->sws) {
        return set_status(-1006, "failed to create swscale context");
    }

    uint8_t* dst_slices[4] = { decoder->rgba, NULL, NULL, NULL };
    int dst_strides[4] = { decoder->width * 4, 0, 0, 0 };
    sws_scale(
        decoder->sws,
        (const uint8_t* const*)decoder->frame->data,
        decoder->frame->linesize,
        0,
        decoder->codec->height,
        dst_slices,
        dst_strides
    );

    int64_t pts = decoder->frame->best_effort_timestamp;
    if (pts != AV_NOPTS_VALUE && stream) {
        AVRational ms_time_base = { 1, 1000 };
        decoder->frame_pts_ms = (int)av_rescale_q(pts, stream->time_base, ms_time_base);
    } else {
        decoder->frame_pts_ms = decoder->frame_index * 33;
    }
    decoder->frame_index += 1;
    return 1;
}

int ffmpeg_bridge_avutil_version(void) {
    return (int)avutil_version();
}

int ffmpeg_bridge_avcodec_version(void) {
    return (int)avcodec_version();
}

int ffmpeg_bridge_avformat_version(void) {
    return (int)avformat_version();
}

int ffmpeg_bridge_swscale_version(void) {
    return (int)swscale_version();
}

const char* ffmpeg_bridge_configuration(void) {
    return avformat_configuration();
}

int ffmpeg_bridge_open_media(const char* path) {
    if (!path || !path[0]) {
        return set_status(-1007, "media path is empty");
    }
    int slot = media_slot();
    if (slot < 0) {
        return set_status(-1008, "media handle table is full");
    }

    AVFormatContext* format = NULL;
    int rc = avformat_open_input(&format, path, NULL, NULL);
    if (rc < 0) {
        return set_av_error("avformat_open_input", rc);
    }
    rc = avformat_find_stream_info(format, NULL);
    if (rc < 0) {
        avformat_close_input(&format);
        return set_av_error("avformat_find_stream_info", rc);
    }

    KainFfmpegMedia* media = &G_MEDIA[slot];
    memset(media, 0, sizeof(*media));
    media->used = 1;
    media->format = format;
    snprintf(media->path, sizeof(media->path), "%s", path);
    set_status(0, "ok");
    return slot + 1;
}

int ffmpeg_bridge_close_media(int media_handle) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    if (!media) {
        return -1;
    }
    if (media->format) {
        avformat_close_input(&media->format);
    }
    memset(media, 0, sizeof(*media));
    return set_status(0, "ok");
}

int ffmpeg_bridge_best_video_stream(int media_handle) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    if (!media) {
        return -1;
    }
    int best = av_find_best_stream(media->format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (best < 0) {
        return set_av_error("av_find_best_stream", best);
    }
    set_status(0, "ok");
    return best;
}

int ffmpeg_bridge_stream_count(int media_handle) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    return media ? (int)media->format->nb_streams : 0;
}

int ffmpeg_bridge_duration_ms(int media_handle) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    if (!media || media->format->duration <= 0) {
        return 0;
    }
    return (int)(media->format->duration / (AV_TIME_BASE / 1000));
}

int ffmpeg_bridge_video_width(int media_handle, int stream_index) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    AVStream* stream = media_stream(media, stream_index);
    return stream && stream->codecpar ? stream->codecpar->width : 0;
}

int ffmpeg_bridge_video_height(int media_handle, int stream_index) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    AVStream* stream = media_stream(media, stream_index);
    return stream && stream->codecpar ? stream->codecpar->height : 0;
}

int ffmpeg_bridge_video_fps_num(int media_handle, int stream_index) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    AVStream* stream = media_stream(media, stream_index);
    if (!stream) {
        return 0;
    }
    AVRational rate = stream->avg_frame_rate.num > 0 ? stream->avg_frame_rate : stream->r_frame_rate;
    return rate.num;
}

int ffmpeg_bridge_video_fps_den(int media_handle, int stream_index) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    AVStream* stream = media_stream(media, stream_index);
    if (!stream) {
        return 1;
    }
    AVRational rate = stream->avg_frame_rate.num > 0 ? stream->avg_frame_rate : stream->r_frame_rate;
    return rate.den == 0 ? 1 : rate.den;
}

const char* ffmpeg_bridge_video_codec_name(int media_handle, int stream_index) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    AVStream* stream = media_stream(media, stream_index);
    if (!stream || !stream->codecpar) {
        snprintf(G_CODEC_NAME, sizeof(G_CODEC_NAME), "unknown");
        return G_CODEC_NAME;
    }
    snprintf(G_CODEC_NAME, sizeof(G_CODEC_NAME), "%s", avcodec_get_name(stream->codecpar->codec_id));
    return G_CODEC_NAME;
}

int ffmpeg_bridge_decoder_create(int media_handle, int stream_index) {
    KainFfmpegMedia* media = media_from_handle(media_handle);
    AVStream* stream = media_stream(media, stream_index);
    if (!media || !stream || !stream->codecpar) {
        return -1;
    }

    int slot = decoder_slot();
    if (slot < 0) {
        return set_status(-1009, "decoder handle table is full");
    }

    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        return set_status(-1010, "no FFmpeg decoder for stream codec");
    }
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        return set_status(-1011, "failed to allocate codec context");
    }
    int rc = avcodec_parameters_to_context(ctx, stream->codecpar);
    if (rc < 0) {
        avcodec_free_context(&ctx);
        return set_av_error("avcodec_parameters_to_context", rc);
    }
    rc = avcodec_open2(ctx, codec, NULL);
    if (rc < 0) {
        avcodec_free_context(&ctx);
        return set_av_error("avcodec_open2", rc);
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!packet || !frame) {
        if (packet) {
            av_packet_free(&packet);
        }
        if (frame) {
            av_frame_free(&frame);
        }
        avcodec_free_context(&ctx);
        return set_status(-1012, "failed to allocate packet/frame");
    }

    KainFfmpegDecoder* decoder = &G_DECODERS[slot];
    memset(decoder, 0, sizeof(*decoder));
    decoder->used = 1;
    decoder->media_handle = media_handle;
    decoder->stream_index = stream_index;
    decoder->codec = ctx;
    decoder->packet = packet;
    decoder->frame = frame;
    decoder->width = ctx->width;
    decoder->height = ctx->height;
    set_status(0, "ok");
    return slot + 1;
}

int ffmpeg_bridge_decoder_destroy(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    if (!decoder) {
        return -1;
    }
    if (decoder->sws) {
        sws_freeContext(decoder->sws);
    }
    if (decoder->rgba) {
        av_free(decoder->rgba);
    }
    if (decoder->frame) {
        av_frame_free(&decoder->frame);
    }
    if (decoder->packet) {
        av_packet_free(&decoder->packet);
    }
    if (decoder->codec) {
        avcodec_free_context(&decoder->codec);
    }
    memset(decoder, 0, sizeof(*decoder));
    return set_status(0, "ok");
}

int ffmpeg_bridge_decoder_seek_ms(int decoder_handle, int timestamp_ms) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    if (!decoder) {
        return -1;
    }
    KainFfmpegMedia* media = media_from_handle(decoder->media_handle);
    AVStream* stream = media_stream(media, decoder->stream_index);
    if (!stream) {
        return -1;
    }
    AVRational ms_time_base = { 1, 1000 };
    int64_t target = av_rescale_q((int64_t)timestamp_ms, ms_time_base, stream->time_base);
    int rc = av_seek_frame(media->format, decoder->stream_index, target, AVSEEK_FLAG_BACKWARD);
    if (rc < 0) {
        return set_av_error("av_seek_frame", rc);
    }
    avcodec_flush_buffers(decoder->codec);
    decoder->frame_index = 0;
    decoder->frame_pts_ms = timestamp_ms;
    decoder->eof_sent = 0;
    return set_status(0, "ok");
}

int ffmpeg_bridge_decoder_decode_next(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    if (!decoder) {
        return -1;
    }
    KainFfmpegMedia* media = media_from_handle(decoder->media_handle);
    AVStream* stream = media_stream(media, decoder->stream_index);
    if (!media || !stream) {
        return -1;
    }

    for (;;) {
        int rc = avcodec_receive_frame(decoder->codec, decoder->frame);
        if (rc == 0) {
            return convert_frame(decoder, stream);
        }
        if (rc == AVERROR_EOF) {
            return set_status(0, "decoder eof");
        }
        if (rc != AVERROR(EAGAIN)) {
            return set_av_error("avcodec_receive_frame", rc);
        }

        if (decoder->eof_sent) {
            return set_status(0, "decoder drained");
        }

        rc = av_read_frame(media->format, decoder->packet);
        if (rc == AVERROR_EOF) {
            decoder->eof_sent = 1;
            rc = avcodec_send_packet(decoder->codec, NULL);
            if (rc < 0) {
                return set_av_error("avcodec_send_packet.flush", rc);
            }
            continue;
        }
        if (rc < 0) {
            return set_av_error("av_read_frame", rc);
        }
        if (decoder->packet->stream_index == decoder->stream_index) {
            rc = avcodec_send_packet(decoder->codec, decoder->packet);
            av_packet_unref(decoder->packet);
            if (rc < 0) {
                return set_av_error("avcodec_send_packet", rc);
            }
        } else {
            av_packet_unref(decoder->packet);
        }
    }
}

int ffmpeg_bridge_decoder_width(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    return decoder ? decoder->width : 0;
}

int ffmpeg_bridge_decoder_height(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    return decoder ? decoder->height : 0;
}

int ffmpeg_bridge_decoder_frame_index(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    return decoder ? decoder->frame_index : 0;
}

int ffmpeg_bridge_decoder_frame_pts_ms(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    return decoder ? decoder->frame_pts_ms : 0;
}

int ffmpeg_bridge_decoder_frame_word_count(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    if (!decoder || decoder->rgba_bytes <= 0) {
        return 0;
    }
    return (decoder->rgba_bytes + 3) / 4;
}

long long ffmpeg_bridge_decoder_frame_checksum(int decoder_handle) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    if (!decoder || !decoder->rgba || decoder->rgba_bytes <= 0) {
        return 0;
    }
    uint64_t hash = 1469598103934665603ull;
    for (int i = 0; i < decoder->rgba_bytes; ++i) {
        hash ^= (uint64_t)decoder->rgba[i];
        hash *= 1099511628211ull;
    }
    return (long long)(hash & 0x7fffffffffffffffLL);
}

int ffmpeg_bridge_copy_rgba_words(int decoder_handle, long long dst_words_address, int word_capacity) {
    KainFfmpegDecoder* decoder = decoder_from_handle(decoder_handle);
    if (!decoder || !decoder->rgba || decoder->rgba_bytes <= 0) {
        return set_status(-1013, "no decoded rgba frame is available");
    }
    if (dst_words_address == 0 || word_capacity <= 0) {
        return set_status(-1014, "destination frame buffer is invalid");
    }
    int needed = ffmpeg_bridge_decoder_frame_word_count(decoder_handle);
    if (word_capacity < needed) {
        return set_status(-1015, "destination frame buffer is too small");
    }
    uint64_t* words = (uint64_t*)(uintptr_t)dst_words_address;
    for (int word = 0; word < needed; ++word) {
        uint64_t packed = 0;
        int base = word * 4;
        for (int lane = 0; lane < 4; ++lane) {
            int byte_index = base + lane;
            uint64_t value = byte_index < decoder->rgba_bytes ? (uint64_t)decoder->rgba[byte_index] : 0;
            packed |= value << (lane * 8);
        }
        words[word] = packed;
    }
    return needed;
}

int ffmpeg_bridge_live_media_count(void) {
    int count = 0;
    for (int index = 0; index < KAIN_FFMPEG_MAX_MEDIA; ++index) {
        if (G_MEDIA[index].used) {
            count += 1;
        }
    }
    return count;
}

int ffmpeg_bridge_live_decoder_count(void) {
    int count = 0;
    for (int index = 0; index < KAIN_FFMPEG_MAX_DECODERS; ++index) {
        if (G_DECODERS[index].used) {
            count += 1;
        }
    }
    return count;
}

int ffmpeg_bridge_last_status(void) {
    return G_STATUS;
}

const char* ffmpeg_bridge_last_error(void) {
    return G_ERROR;
}
