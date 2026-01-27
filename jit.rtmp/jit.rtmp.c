#include "jit.common.h"
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>

typedef struct _jit_rtmp 
{
    t_object        ob;
    
    // Attributes
    t_symbol        *url;
    long            width;
    long            height;
    double          framerate;
    long            bitrate;
    long            running; 

    // FFmpeg context
    AVFormatContext *fmt_ctx;
    AVCodecContext  *codec_ctx;
    AVStream        *stream;
    AVFrame         *frame;
    struct SwsContext *sws_ctx;
    
    int             src_width;
    int             src_height;
    
    int64_t         next_pts;
    int64_t         start_time;
    
} t_jit_rtmp;

void *_jit_rtmp_class;

t_jit_err jit_rtmp_init(void); 
t_jit_rtmp *jit_rtmp_new(void);
void jit_rtmp_free(t_jit_rtmp *x);
t_jit_err jit_rtmp_matrix_calc(t_jit_rtmp *x, void *inputs, void *outputs);
void jit_rtmp_calculate_ndim(t_jit_rtmp *x, long dimcount, long *dim, long planecount, t_jit_matrix_info *in_minfo, char *bip, t_jit_matrix_info *out_minfo, char *bop);

// FFmpeg helpers
void jit_rtmp_init_ffmpeg(t_jit_rtmp *x);
void jit_rtmp_cleanup_ffmpeg(t_jit_rtmp *x);
int jit_rtmp_encode_frame(t_jit_rtmp *x, char *data, long stride, int width, int height);

t_jit_err jit_rtmp_init(void) 
{
    long attrflags = 0;
    t_jit_object *attr;
    t_jit_object *mop;
    
    _jit_rtmp_class = jit_class_new("jit_rtmp", (method)jit_rtmp_new, (method)jit_rtmp_free,
        sizeof(t_jit_rtmp), 0);

    // Add matrix operator
    mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop, 1, 0); // 1 input, 0 outputs (sink)
    jit_class_addadornment(_jit_rtmp_class, mop);
    
    // Add method
    jit_class_addmethod(_jit_rtmp_class, (method)jit_rtmp_matrix_calc, "matrix_calc", A_CANT, 0);

    // Attributes
    attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    
    // URL
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "url", _jit_sym_symbol, attrflags,
        (method)0, (method)0, calcoffset(t_jit_rtmp, url));
    jit_class_addattr(_jit_rtmp_class, attr);
    
    // Width
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "width", _jit_sym_long, attrflags,
        (method)0, (method)0, calcoffset(t_jit_rtmp, width));
    jit_class_addattr(_jit_rtmp_class, attr);
    
    // Height
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "height", _jit_sym_long, attrflags,
        (method)0, (method)0, calcoffset(t_jit_rtmp, height));
    jit_class_addattr(_jit_rtmp_class, attr);
    
    // Framerate
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "framerate", _jit_sym_float64, attrflags,
        (method)0, (method)0, calcoffset(t_jit_rtmp, framerate));
    jit_class_addattr(_jit_rtmp_class, attr);

    // Bitrate
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "bitrate", _jit_sym_long, attrflags,
        (method)0, (method)0, calcoffset(t_jit_rtmp, bitrate));
    jit_class_addattr(_jit_rtmp_class, attr);

    // Running
    attr = (t_jit_object *)jit_object_new(_jit_sym_jit_attr_offset, "running", _jit_sym_long, attrflags,
        (method)0, (method)0, calcoffset(t_jit_rtmp, running));
    jit_class_addattr(_jit_rtmp_class, attr);

    jit_class_register(_jit_rtmp_class);

    return JIT_ERR_NONE;
}

t_jit_rtmp *jit_rtmp_new(void) 
{
    t_jit_rtmp *x;
    
    if (x = (t_jit_rtmp *)jit_object_alloc(_jit_rtmp_class)) {
        x->url = gensym("rtmp://localhost/live/stream");
        x->width = 1280;
        x->height = 720;
        x->framerate = 30.0;
        x->bitrate = 2500000;
        x->running = 0;
        
        x->fmt_ctx = NULL;
        x->codec_ctx = NULL;
        x->stream = NULL;
        x->frame = NULL;
        x->sws_ctx = NULL;
        x->src_width = 0;
        x->src_height = 0;
        x->next_pts = 0;
        x->start_time = 0;
    }
    return x;
}

void jit_rtmp_free(t_jit_rtmp *x) 
{
    jit_rtmp_cleanup_ffmpeg(x);
}

void jit_rtmp_cleanup_ffmpeg(t_jit_rtmp *x)
{
    if (x->fmt_ctx) {
        if (x->fmt_ctx->pb) { // Close I/O context if open
             av_write_trailer(x->fmt_ctx); // Try to finish stream cleanly
             avio_closep(&x->fmt_ctx->pb);
        }
        avformat_free_context(x->fmt_ctx);
        x->fmt_ctx = NULL;
    }
    if (x->codec_ctx) {
        avcodec_free_context(&x->codec_ctx);
        x->codec_ctx = NULL;
    }
    if (x->frame) {
        av_frame_free(&x->frame);
        x->frame = NULL;
    }
    if (x->sws_ctx) {
        sws_freeContext(x->sws_ctx);
        x->sws_ctx = NULL;
    }
    x->stream = NULL;
    x->next_pts = 0;
}

void jit_rtmp_init_ffmpeg(t_jit_rtmp *x)
{
    const AVCodec *codec;
    int ret;
    AVDictionary *opt = NULL;

    jit_rtmp_cleanup_ffmpeg(x); // Ensure clean state

    if (!x->url || x->url == _jit_sym_nothing) return;

    // Allocate output context
    avformat_alloc_output_context2(&x->fmt_ctx, NULL, "flv", x->url->s_name);
    if (!x->fmt_ctx) {
        jit_object_error((t_object *)x, "Could not create output context for FLV/RTMP");
        return;
    }

    // Find H.264 encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        jit_object_error((t_object *)x, "Codec libx264 not found");
        return;
    }

    // Create stream
    x->stream = avformat_new_stream(x->fmt_ctx, NULL);
    if (!x->stream) {
        jit_object_error((t_object *)x, "Could not allocate stream");
        return;
    }
    x->stream->id = x->fmt_ctx->nb_streams - 1;

    // Allocate codec context
    x->codec_ctx = avcodec_alloc_context3(codec);
    if (!x->codec_ctx) {
        jit_object_error((t_object *)x, "Could not allocate encoding context");
        return;
    }

    // Setup codec parameters
    x->codec_ctx->codec_id = AV_CODEC_ID_H264;
    x->codec_ctx->bit_rate = x->bitrate;
    x->codec_ctx->width = x->width;
    x->codec_ctx->height = x->height;
    
    AVRational fps = av_d2q(x->framerate, 100000);
    x->stream->time_base = (AVRational){fps.den, fps.num};
    
    x->codec_ctx->time_base = x->stream->time_base;
    x->codec_ctx->gop_size = (int)(x->framerate * 2); // Keyframe every 2 seconds
    x->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // Tuning for low latency streaming
    if (x->codec_ctx->codec_id == AV_CODEC_ID_H264) {
        av_opt_set(x->codec_ctx->priv_data, "preset", "veryfast", 0);
        av_opt_set(x->codec_ctx->priv_data, "tune", "zerolatency", 0);
    }

    if (x->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        x->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Open codec
    ret = avcodec_open2(x->codec_ctx, codec, NULL);
    if (ret < 0) {
        jit_object_error((t_object *)x, "Could not open video codec");
        return;
    }

    // Allocate frame
    x->frame = av_frame_alloc();
    x->frame->format = x->codec_ctx->pix_fmt;
    x->frame->width = x->codec_ctx->width;
    x->frame->height = x->codec_ctx->height;
    ret = av_frame_get_buffer(x->frame, 32);
    if (ret < 0) {
        jit_object_error((t_object *)x, "Could not allocate frame data");
        return;
    }

    // Copy parameters to stream
    ret = avcodec_parameters_from_context(x->stream->codecpar, x->codec_ctx);
    if (ret < 0) {
         jit_object_error((t_object *)x, "Could not copy stream parameters");
         return;
    }

    // Open output URL
    if (!(x->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&x->fmt_ctx->pb, x->url->s_name, AVIO_FLAG_WRITE);
        if (ret < 0) {
            jit_object_error((t_object *)x, "Could not open '%s'", x->url->s_name);
            return;
        }
    }

    // Write header
    ret = avformat_write_header(x->fmt_ctx, NULL);
    if (ret < 0) {
        jit_object_error((t_object *)x, "Error occurred when opening output stream");
        return;
    }

    x->next_pts = 0;
    // x->start_time = av_gettime(); // Ideally sync with wall clock or keep relative
}


t_jit_err jit_rtmp_matrix_calc(t_jit_rtmp *x, void *inputs, void *outputs)
{
    t_jit_err err = JIT_ERR_NONE;
    long in_savelock;
    t_jit_matrix_info in_minfo;
    char *in_bp;
    long i, dimcount, planecount, dim[JIT_MATRIX_MAX_DIMCOUNT];
    void *in_matrix;
    
    in_matrix = jit_object_method(inputs, _jit_sym_getindex, 0);

    if (x && in_matrix) {
        in_savelock = (long) jit_object_method(in_matrix, _jit_sym_lock, 1);
        
        jit_object_method(in_matrix, _jit_sym_getinfo, &in_minfo);
        jit_object_method(in_matrix, _jit_sym_getdata, &in_bp);
        
        if (!in_bp) {
            err = JIT_ERR_INVALID_INPUT;
            goto out;
        }
        
        // If running is 0, ensure cleanup and return
        if (!x->running) {
            if (x->fmt_ctx) jit_rtmp_cleanup_ffmpeg(x);
            goto out;
        }

        // If running but not initialized, or dimensions changed, re-init
        // Note: Changing dimensions mid-stream on RTMP is not standard, usually requires restart
        if (!x->fmt_ctx) {
            // Update width/height from matrix if they are default? 
            // Better to respect attributes, but let's check if we should adapt
            // For now, we rely on attributes.
            jit_rtmp_init_ffmpeg(x);
            if (!x->fmt_ctx) goto out; // Failed to init
        }

        // Validate input
        if (in_minfo.type != _jit_sym_char) {
            err = JIT_ERR_MISMATCH_TYPE;
            goto out;
        }
        if (in_minfo.planecount != 4) {
            err = JIT_ERR_MISMATCH_PLANE;
            goto out;
        }

        // Process frame
        jit_rtmp_encode_frame(x, in_bp, in_minfo.dimstride[1], in_minfo.dim[0], in_minfo.dim[1]);
        
out:
        jit_object_method(in_matrix, _jit_sym_lock, in_savelock);
    } else {
        return JIT_ERR_INVALID_PTR;
    }
    
    return err;
}

int jit_rtmp_encode_frame(t_jit_rtmp *x, char *data, long stride, int width, int height)
{
    int ret;
    AVPacket *pkt;
    
    // Make sure frame is writable
    ret = av_frame_make_writable(x->frame);
    if (ret < 0) return ret;

    // Check if input dimensions changed, update sws_ctx
    if (!x->sws_ctx || x->src_width != width || x->src_height != height) {
        if (x->sws_ctx) sws_freeContext(x->sws_ctx);
        x->src_width = width;
        x->src_height = height;
        
        // Assume Jitter stores as ARGB or BGRA depending on platform?
        // Usually it's A R G B in memory for char planes.
        // Let's assume AV_PIX_FMT_ARGB for input.
        x->sws_ctx = sws_getContext(width, height, AV_PIX_FMT_ARGB,
                                    x->codec_ctx->width, x->codec_ctx->height, x->codec_ctx->pix_fmt,
                                    SWS_BICUBIC, NULL, NULL, NULL);
        if (!x->sws_ctx) return -1;
    }
    
    // Jitter data pointer usually points to the start of the first row.
    // Stride is in bytes.
    const uint8_t *inData[4] = { (const uint8_t*)data, NULL, NULL, NULL };
    int inLinesize[4] = { (int)stride, 0, 0, 0 };
    
    sws_scale(x->sws_ctx, inData, inLinesize, 0, height, x->frame->data, x->frame->linesize);
    
    x->frame->pts = x->next_pts++;

    // Send frame
    ret = avcodec_send_frame(x->codec_ctx, x->frame);
    if (ret < 0) return ret;
    
    pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(x->codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            av_packet_free(&pkt);
            return ret;
        }

        // Rescale packet timestamp
        av_packet_rescale_ts(pkt, x->codec_ctx->time_base, x->stream->time_base);
        pkt->stream_index = x->stream->index;

        // Write packet
        ret = av_interleaved_write_frame(x->fmt_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    
    return 0;
}
