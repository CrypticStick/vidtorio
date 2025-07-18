/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * Based on the demuxing, decoding, filtering, encoding and muxing API usage example
 * from https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcode.c
 *
 * Generates a media preview of the Factorio blueprint for some input file.
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <string.h>
#include "media_processor.h"
#include "display_renderer.h" // For generating preview frames with icons from paletted frames

// Private function that identifies and processes media.
string _process_media(bool identify_only);

// Initialize global objects
Media_Format_Config media_config = {0};
Media_Type_Flags media_loaded_type = NONE_TYPE;

// Initialize local objects
static const int IMAGE_AV_CODEC_ID = AV_CODEC_ID_PNG;
static const int IMAGE_AV_PIX_FMT = AV_PIX_FMT_RGBA;
static const string IMAGE_OUTPUT_FILE_SUFFIX = str("_vidtorio.png");
static const int VIDEO_AV_CODEC_ID = AV_CODEC_ID_VP9;
static const int VIDEO_AV_PIX_FMT = AV_PIX_FMT_YUVA420P;
static const string VIDEO_OUTPUT_FILE_SUFFIX = str("_vidtorio.webm");
static const int AUDIO_AV_CODEC_ID = AV_CODEC_ID_OPUS;
static const string AUDIO_OUTPUT_FILE_SUFFIX = str("_vidtorio.ogg");

// TODO: Dynamically build filter according to media_config
/* Video filter that scales output and applies a color palette */
static const char *video_filter_descr = "[in] fps=10,scale=1024:1024:flags=lanczos:force_original_aspect_ratio=decrease [intermediate]; movie=palette.png [pal]; [intermediate][pal] paletteuse=dither=floyd_steinberg [out]";

/* Passthrough (dummy) filter for audio */
static const char *audio_filter_descr = "anull"; 

static const char src_filename_cstr[MAX_FILE_LEN + 1];

static const string DST_FILE_DIR = str("out/");
static const char dst_filepath_cstr[lengthof("out/") + MAX_FILE_LEN + 1];
static string_buffer dst_filepath = {{(char *)dst_filepath_cstr, 0}, lengthof("out/") + MAX_FILE_LEN + 1};

static ProcessFrameCallback process_frame_cbk = NULL;

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;

    AVPacket *enc_pkt;
    AVFrame *filtered_frame;
} FilteringContext;
static FilteringContext *filter_ctx;

typedef struct StreamContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;
    AVFrame *dec_frame;
    struct SwsContext *sws_ctx_rgba_to_yuva;
} StreamContext;
static StreamContext *stream_ctx;

static int open_input_file(const char *filename) {
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    stream_ctx = av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    stream_ctx->sws_ctx_rgba_to_yuva = NULL;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                                       "for stream #%u\n",
                   i);
            return ret;
        }

        /* Inform the decoder about the timebase for the packet timestamps.
         * This is highly recommended, but not mandatory. */
        codec_ctx->pkt_timebase = stream->time_base;

        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
        stream_ctx[i].dec_ctx = codec_ctx;

        stream_ctx[i].dec_frame = av_frame_alloc();
        if (!stream_ctx[i].dec_frame)
            return AVERROR(ENOMEM);
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename, Media_Type_Flags target_type) {
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    const AVCodec *encoder;
    int enc_codec_id;
    int ret;
    unsigned int i;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Choose codec based on the target output format */
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (target_type & IMAGE_TYPE)
                    enc_codec_id = IMAGE_AV_CODEC_ID;
                else if (target_type & VIDEO_TYPE)
                    enc_codec_id = VIDEO_AV_CODEC_ID;
                else
                    enc_codec_id = AV_CODEC_ID_NONE;
            } else {
                if (target_type & AUDIO_TYPE)
                    enc_codec_id = AUDIO_AV_CODEC_ID;
                else
                    enc_codec_id = AV_CODEC_ID_NONE;
            }

            encoder = avcodec_find_encoder(enc_codec_id);
            if (!encoder) {
                av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) {
                av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }

            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                /* Configure encoder for the final upscaled dimensions */
                Icon_Rect frame_rect = Get_Icon_Frame_Rect(media_config.image_config.icon_resolution, dec_ctx->width, dec_ctx->height, media_config.image_config.tile_spacing);
                enc_ctx->height = frame_rect.h;
                enc_ctx->width = frame_rect.w;
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                enc_ctx->pix_fmt = (target_type & IMAGE_TYPE) ? IMAGE_AV_PIX_FMT : VIDEO_AV_PIX_FMT;

                /* Set timebase - using the inverse of the framerate is common */
                /* Note: The paletteuse filter might change frame rate, but we'll base it on the decoder's guess */
                if (dec_ctx->framerate.num > 0 && dec_ctx->framerate.den > 0) {
                    enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
                } else {
                    // Fallback if framerate is unknown
                    enc_ctx->time_base = stream_ctx[i].dec_ctx->pkt_timebase; // Use decoder packet timebase as fallback
                }
                // Ensure timebase is valid
                if (enc_ctx->time_base.num <= 0 || enc_ctx->time_base.den <= 0) {
                    av_log(NULL, AV_LOG_WARNING, "Invalid timebase determined for encoder, using 1/25 as default\n");
                    enc_ctx->time_base = (AVRational){1, 25};
                }
            } else {
                const enum AVSampleFormat *sample_fmts = NULL;

                enc_ctx->sample_rate = dec_ctx->sample_rate;
                ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
                if (ret < 0)
                    return ret;

                ret = avcodec_get_supported_config(dec_ctx, NULL,
                                                   AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                                   (const void **)&sample_fmts, NULL);

                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = (ret >= 0 && sample_fmts) ? sample_fmts[0] : dec_ctx->sample_fmt;

                enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
            }

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open %s encoder for stream #%u\n", encoder->name, i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            /* if this stream must be remuxed */
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }
    }
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0){
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx, const char *filter_spec) {
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
        if (!buffersink_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            ret = AVERROR(ENOMEM);
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }

        ret = avfilter_init_dict(buffersink_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot initialize buffer sink\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        char buf[64];
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&dec_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
        av_channel_layout_describe(&dec_ctx->ch_layout, buf, sizeof(buf));
        snprintf(args, sizeof(args),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den, dec_ctx->sample_rate,
                av_get_sample_fmt_name(dec_ctx->sample_fmt),
                buf);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
        if (!buffersink_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            ret = AVERROR(ENOMEM);
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

        av_channel_layout_describe(&enc_ctx->ch_layout, buf, sizeof(buf));
        ret = av_opt_set(buffersink_ctx, "ch_layouts",
                        buf, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }

        if (enc_ctx->frame_size > 0)
            av_buffersink_set_frame_size(buffersink_ctx, enc_ctx->frame_size);

        ret = avfilter_init_dict(buffersink_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot initialize audio buffer sink\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filter2(FilteringContext* fctx, AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx, const char *filter_spec) {
    char filters[512];
    int ret = 0;
    AVFilterInOut *inputs  = NULL;
    AVFilterInOut *outputs = NULL;
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        snprintf(filters, sizeof(filters),
                    "buffer=video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d [in]; %s; [out] buffersink",
                    dec_ctx->width, dec_ctx->height,
                    dec_ctx->pix_fmt,
                    dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
                    dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den,
                    filter_spec);
    } else {
        snprintf(filters, sizeof(filters), "null");
    }

    if((ret = avfilter_graph_parse2(filter_graph, filters, &inputs, &outputs)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    fctx->buffersrc_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffer_0");
    // Note: this may change if a new filter is added to filter_spec
    fctx->buffersink_ctx = avfilter_graph_get_filter(filter_graph, "Parsed_buffersink_5");
    fctx->filter_graph = filter_graph;

    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(void) {
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph = NULL;
        if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO || ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;

        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = video_filter_descr;
        else
            filter_spec = audio_filter_descr;
        ret = init_filter2(&filter_ctx[i], stream_ctx[i].dec_ctx,
                          stream_ctx[i].enc_ctx, filter_spec);
        if (ret)
            return ret;

        filter_ctx[i].enc_pkt = av_packet_alloc();
        if (!filter_ctx[i].enc_pkt)
            return AVERROR(ENOMEM);

        filter_ctx[i].filtered_frame = av_frame_alloc();
        if (!filter_ctx[i].filtered_frame)
            return AVERROR(ENOMEM);
    }
    return 0;
}

static int encode_write_frame(unsigned int stream_index, int flush) {
    StreamContext *stream = &stream_ctx[stream_index];
    FilteringContext *filter = &filter_ctx[stream_index];
    AVFrame *frame_to_process = flush ? NULL : filter->filtered_frame;
    AVFrame *processed_rgba_frame = NULL;
    AVFrame *processed_yuva_frame = NULL;
    AVFrame *frame_to_encode = NULL; // Pointer for our newly generated frame
    AVPacket *enc_pkt = filter->enc_pkt;
    int ret;

    if (stream->enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        // Process frame before encoding (get new RGBA frame)
        if (!flush && frame_to_process && frame_to_process->format == AV_PIX_FMT_PAL8)
        {
            av_log(NULL, AV_LOG_INFO, "Processing paletted frame before encoding\n");
            processed_rgba_frame = Render_Icon_Frame(frame_to_process, media_config.image_config.icon_resolution, media_config.image_config.tile_spacing);

            if (process_frame_cbk != NULL) {
                // Perform additional user-defined processing on the frame (e.g. building a blueprint string)
                process_frame_cbk(frame_to_process);
            }

            if (!processed_rgba_frame || processed_rgba_frame->format != AV_PIX_FMT_RGBA) {
                av_log(NULL, AV_LOG_ERROR, "Failed to process frame, skipping encoding for this frame.\n");
                av_frame_free(&processed_rgba_frame);
                return 0; // Skip this frame
            }

            // RGBA to YUVA Conversion
            enum AVPixelFormat target_pix_fmt = stream->enc_ctx->pix_fmt;
            if (target_pix_fmt == AV_PIX_FMT_YUVA420P) {
                // Initialize SwsContext (cached for efficiency)
                stream->sws_ctx_rgba_to_yuva = sws_getCachedContext(stream->sws_ctx_rgba_to_yuva,
                                                    processed_rgba_frame->width, processed_rgba_frame->height, (enum AVPixelFormat)processed_rgba_frame->format,
                                                    stream->enc_ctx->width, stream->enc_ctx->height, target_pix_fmt,
                                                    SWS_BILINEAR, NULL, NULL, NULL);
                if (!stream->sws_ctx_rgba_to_yuva) {
                    av_log(NULL, AV_LOG_ERROR, "Cannot initialize the RGBA->YUVA conversion context\n");
                    av_frame_free(&processed_rgba_frame);
                    return AVERROR(EINVAL);
                }

                // Allocate the destination YUVA frame
                processed_yuva_frame = av_frame_alloc();
                if (!processed_yuva_frame) {
                    av_log(NULL, AV_LOG_ERROR, "Cannot allocate YUVA frame\n");
                    av_frame_free(&processed_rgba_frame);
                    return AVERROR(ENOMEM);
                }
                    
                processed_yuva_frame->width = stream->enc_ctx->width;
                processed_yuva_frame->height = stream->enc_ctx->height;
                processed_yuva_frame->format = target_pix_fmt;
                processed_yuva_frame->pts = processed_rgba_frame->pts;
                processed_yuva_frame->time_base = processed_rgba_frame->time_base;
                processed_yuva_frame->sample_aspect_ratio = processed_rgba_frame->sample_aspect_ratio;

                ret = av_frame_get_buffer(processed_yuva_frame, 0);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Cannot allocate YUVA frame buffer\n");
                    av_frame_free(&processed_yuva_frame);
                    av_frame_free(&processed_rgba_frame);
                    return AVERROR(ENOMEM);
                }

                // Perform the conversion
                ret = sws_scale(stream->sws_ctx_rgba_to_yuva, (const uint8_t * const *)processed_rgba_frame->data,
                            processed_rgba_frame->linesize, 0, processed_rgba_frame->height,
                            processed_yuva_frame->data, processed_yuva_frame->linesize);

                av_frame_free(&processed_rgba_frame); // Free the intermediate RGBA frame now
                processed_rgba_frame = NULL;

                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error during RGBA->YUVA conversion\n");
                    av_frame_free(&processed_yuva_frame);
                    return ret;
                }

                frame_to_encode = processed_yuva_frame;
                av_log(NULL, AV_LOG_INFO, "Converted frame to YUVA format.\n");
            } else {
                frame_to_encode = processed_rgba_frame;
            }
        }
    } else if (stream->enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        // TODO: Add audio processing
        frame_to_encode = frame_to_process;
    }

    if (frame_to_encode) {
        av_log(NULL, AV_LOG_INFO, "Encoding frame (pts %"PRId64")\n", frame_to_encode->pts);
    } else if (flush) {
        av_log(NULL, AV_LOG_INFO, "Flushing encoder for stream %u\n", stream_index);
    }

    /* encode filtered frame */
    av_packet_unref(enc_pkt);

    // Rescale timestamp if we have a frame to encode
    if (frame_to_encode && frame_to_encode->pts != AV_NOPTS_VALUE) {
        // Rescale pts from the filter's timebase to the encoder's timebase
         AVRational filter_time_base = av_buffersink_get_time_base(filter->buffersink_ctx);
         if (filter_time_base.num > 0 && filter_time_base.den > 0) {
             frame_to_encode->pts = av_rescale_q(frame_to_encode->pts,
                                                 filter_time_base, // Input timebase (from filter)
                                                 stream->enc_ctx->time_base); // Output timebase (encoder)
         } else {
             av_log(NULL, AV_LOG_WARNING, "Invalid filter timebase, unable to rescale PTS accurately.\n");
             // As a fallback, assume it matches the decoder packet timebase
             frame_to_encode->pts = av_rescale_q(frame_to_encode->pts, stream->dec_ctx->pkt_timebase, stream->enc_ctx->time_base);
         }
    }

    ret = avcodec_send_frame(stream->enc_ctx, frame_to_encode);

    // If we created a new frame, free it now since avcodec_send_frame has taken ownership/copied it
    if (processed_rgba_frame || processed_yuva_frame) {
        av_frame_free(&frame_to_encode);
        frame_to_encode = NULL;
    }

    if (ret < 0) {
        // EAGAIN might happen if flushing, EOF if flushed.
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_ERROR, "Error sending frame to encoder: %s\n", errbuf);
        }
        // If flushing, AVERROR_EOF is expected, not an error.
        // If not flushing, EAGAIN means we should try again later.
        return (ret == AVERROR_EOF && flush) ? 0 : ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // EAGAIN means need more input/output; EOF means stream finished
            // In either case, break the loop for receiving packets for *this* input frame.
            // Return 0 to indicate success for this encode_write_frame call.
            return 0;
        } else if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_ERROR, "Error receiving packet from encoder: %s\n", errbuf);
            return ret; // Return the error
        }

        /* prepare packet for muxing */
        enc_pkt->stream_index = stream_index;
        av_packet_rescale_ts(enc_pkt,
                             stream->enc_ctx->time_base,
                             ofmt_ctx->streams[stream_index]->time_base);

        av_log(NULL, AV_LOG_DEBUG, "Muxing frame (stream %u, pts %"PRId64", dts %"PRId64")\n",
            stream_index, enc_pkt->pts, enc_pkt->dts);

        /* mux encoded frame */
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            av_log(NULL, AV_LOG_ERROR, "Error during writing frame: %s\n", errbuf);
            return ret;
       }
    }

    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index) {
    FilteringContext *filter = &filter_ctx[stream_index];
    int ret;

    av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter->buffersrc_ctx,
                                       frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1) {
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter->buffersink_ctx,
                                      filter->filtered_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            break;
        }

        filter->filtered_frame->time_base = av_buffersink_get_time_base(filter->buffersink_ctx);
        filter->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(stream_index, 0);
        av_frame_unref(filter->filtered_frame);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index) {
    if (!(stream_ctx[stream_index].enc_ctx->codec->capabilities &
          AV_CODEC_CAP_DELAY))
        return 0;

    av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
    return encode_write_frame(stream_index, 1);
}

Media_Type_Flags load_media(char *filename) {
    int ret;
    AVPacket *packet = NULL;
    bool has_audio = false, has_video = false;
    int frame_count = 0;
    unsigned int stream_index;
    unsigned int i;

    media_loaded_type = NONE_TYPE;

    /* Save input filename */
    strncpy((char *)src_filename_cstr, filename, MAX_FILE_LEN); 

    /* load input file and setup decoder */
    if ((ret = open_input_file(src_filename_cstr)) < 0)
        goto end;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        switch(stream_ctx[i].dec_ctx->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            has_video = true;
            break;
        case AVMEDIA_TYPE_AUDIO:
            has_audio = true;
            break;
        default:
            break;
        }
    }

    if (!(packet = av_packet_alloc()))
        goto end;

    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;
        stream_index = packet->stream_index;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
            stream_index);

        StreamContext *stream = &stream_ctx[stream_index];
        if (stream->dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
            stream->dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {

            av_log(NULL, AV_LOG_DEBUG, "Going to decode the frame\n");

            ret = avcodec_send_packet(stream->dec_ctx, packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                    break;
                else if (ret < 0)
                    goto end;
                if (stream->dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                    if (++frame_count > 1) {
                        goto end;
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    /* flush decoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        StreamContext *stream = &stream_ctx[i];

        if (stream->dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO &&
            stream->dec_ctx->codec_type != AVMEDIA_TYPE_AUDIO)
            continue;

        av_log(NULL, AV_LOG_INFO, "Flushing stream %u decoder\n", i);

        /* flush decoder */
        ret = avcodec_send_packet(stream->dec_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing decoding failed\n");
            goto end;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
            if (ret == AVERROR_EOF)
                break;
            else if (ret < 0)
                goto end;

            if (stream->dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (++frame_count > 1) {
                    goto end;
                }
            }
        }
    }

    end:
    av_packet_free(&packet);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        avcodec_free_context(&stream_ctx[i].dec_ctx);
        av_frame_free(&stream_ctx[i].dec_frame);
    }
    av_free(stream_ctx);
    avformat_close_input(&ifmt_ctx);

    media_loaded_type = (has_video && frame_count == 1 ? IMAGE_TYPE : NONE_TYPE) |
                        (has_video && frame_count != 1 ? VIDEO_TYPE : NONE_TYPE) |
                        (has_audio ? AUDIO_TYPE : NONE_TYPE);

    if (ret < 0 && ret != AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
        return NONE_TYPE;
    }

    return media_loaded_type;
}

string process_media(ProcessFrameCallback process_frame) {
    int ret;
    AVPacket *packet = NULL;
    unsigned int stream_index;
    unsigned int i;

    if (media_loaded_type == NONE_TYPE) {
        return string_cast(NULL);
    }

    process_frame_cbk = process_frame;

    if ((ret = open_input_file(src_filename_cstr)) < 0)
        goto end;

    if (media_loaded_type == NONE_TYPE) {
        fprintf(stderr, "Format for file %s is unknown (did you call load_media first?)\n", src_filename_cstr);
        goto end;
    }
    /* Prepare output file based on detected format */
    string dst_filename_suffix =
        media_loaded_type & IMAGE_TYPE ? IMAGE_OUTPUT_FILE_SUFFIX : media_loaded_type & VIDEO_TYPE ? VIDEO_OUTPUT_FILE_SUFFIX
                                                                                                        : AUDIO_OUTPUT_FILE_SUFFIX;
    string_buffer_clear(&dst_filepath);
    string_buffer_add_str(&dst_filepath, DST_FILE_DIR);
    string_buffer_add_strn(&dst_filepath, string_cast((char *)src_filename_cstr), string_buffer_remaining(&dst_filepath) - dst_filename_suffix.len - 1);
    string_buffer_add_str(&dst_filepath, dst_filename_suffix);
    string_buffer_terminate(&dst_filepath);

    if ((ret = open_output_file(dst_filepath_cstr, media_loaded_type)) < 0)
        goto end;
    if ((ret = init_filters()) < 0)
        goto end;
    
    if (!(packet = av_packet_alloc()))
        goto end;

    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;
        stream_index = packet->stream_index;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
               stream_index);

        if (filter_ctx[stream_index].filter_graph) {
            StreamContext *stream = &stream_ctx[stream_index];

            av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");

            ret = avcodec_send_packet(stream->dec_ctx, packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                    break;
                else if (ret < 0)
                    goto end;

                
                stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
                ret = filter_encode_write_frame(stream->dec_frame, stream_index);
                if (ret < 0)
                    goto end;
            }
        } else {
            /* remux this frame without reencoding */
            av_packet_rescale_ts(packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 ofmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(ofmt_ctx, packet);
            if (ret < 0)
                goto end;
        }
        av_packet_unref(packet);
    }

    /* flush decoders, filters and encoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        StreamContext *stream;

        if (!filter_ctx[i].filter_graph)
            continue;

        stream = &stream_ctx[i];

        av_log(NULL, AV_LOG_INFO, "Flushing stream %u decoder\n", i);

        /* flush decoder */
        ret = avcodec_send_packet(stream->dec_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing decoding failed\n");
            goto end;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
            if (ret == AVERROR_EOF)
                break;
            else if (ret < 0)
                goto end;

            stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
            ret = filter_encode_write_frame(stream->dec_frame, i);
            if (ret < 0)
                goto end;
        }

        /* flush filter */
        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        /* flush encoder */
        ret = flush_encoder(i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }

    av_write_trailer(ofmt_ctx);
end:
    av_packet_free(&packet);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        avcodec_free_context(&stream_ctx[i].dec_ctx);
        sws_freeContext(stream_ctx[i].sws_ctx_rgba_to_yuva);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
            avcodec_free_context(&stream_ctx[i].enc_ctx);
        if (filter_ctx && filter_ctx[i].filter_graph) {
            avfilter_graph_free(&filter_ctx[i].filter_graph);
            av_packet_free(&filter_ctx[i].enc_pkt);
            av_frame_free(&filter_ctx[i].filtered_frame);
        }
        av_frame_free(&stream_ctx[i].dec_frame);
    }
    av_free(filter_ctx);
    av_free(stream_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
        return (string){0};
    }

    return dst_filepath.str;
}