#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "jit.common.h"
#include "jit.gl.h"

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstdio>

/*
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
*/

// Audio Ring Buffer (Lock-Free for Single Producer/Consumer)
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t size) : size_(size), read_(0), write_(0) {
        buffer_.resize(size);
    }

    void push(const float* data, size_t count) {
        size_t w = write_.load(std::memory_order_relaxed);
        size_t r = read_.load(std::memory_order_acquire);

        size_t free_space = (size_ - 1) - ((w - r + size_) % size_);
        if (count > free_space) count = free_space; // Drop if full

        for (size_t i = 0; i < count; ++i) {
            buffer_[w] = data[i];
            w = (w + 1) % size_;
        }
        write_.store(w, std::memory_order_release);
    }

    bool pop(float* data, size_t count) {
        size_t w = write_.load(std::memory_order_acquire);
        size_t r = read_.load(std::memory_order_relaxed);

        size_t available = (w - r + size_) % size_;
        if (available < count) return false;

        for (size_t i = 0; i < count; ++i) {
            data[i] = buffer_[r];
            r = (r + 1) % size_;
        }
        read_.store(r, std::memory_order_release);
        return true;
    }

    size_t available() const {
        size_t w = write_.load(std::memory_order_acquire);
        size_t r = read_.load(std::memory_order_relaxed);
        return (w - r + size_) % size_;
    }

private:
    std::vector<float> buffer_;
    size_t size_;
    std::atomic<size_t> read_, write_;
};

struct VideoFrame {
    std::vector<uint8_t> data; // ARGB or RGB
    int width;
    int height;
    int stride;
    double timestamp;
};

// FFmpeg Streamer Class
class Streamer {
public:
    Streamer() : // fmt_ctx(nullptr), v_ctx(nullptr), a_ctx(nullptr),
                 // v_stream(nullptr), a_stream(nullptr), sws_ctx(nullptr), swr_ctx(nullptr),
                 streaming(false), width(0), height(0), fps(0), v_pts(0), a_pts(0),
                 sws_width(0), sws_height(0),
                 a_ring(131072) {}

    ~Streamer() { stop(); }

    bool start(const std::string& url, int width, int height, int fps, int bitrate, int sample_rate) {
        if (streaming) return false;

        // Store params
        this->url = url;
        this->width = width;
        this->height = height;
        this->fps = fps;
        this->bitrate = bitrate;
        this->sample_rate = sample_rate;

        streaming = true;
        // worker = std::thread(&Streamer::loop, this);
        return true;
    }

    void stop() {
        if (streaming) {
            streaming = false;
            if (worker.joinable()) worker.join();
        }
        cleanup_ffmpeg();
    }

    void push_video(VideoFrame&& frame) {
        if (!streaming) return;
        std::lock_guard<std::mutex> lock(v_mutex);
        if (v_queue.size() < 30) v_queue.push(std::move(frame));
    }

    void push_audio_samples(const float* samples, size_t count) {
        if (!streaming) return;
        a_ring.push(samples, count);
    }

private:
    /*
    AVFormatContext* fmt_ctx;
    AVCodecContext *v_ctx, *a_ctx;
    AVStream *v_stream, *a_stream;
    SwsContext* sws_ctx;
    SwrContext* swr_ctx;
    */

    std::thread worker;
    std::atomic<bool> streaming;

    std::mutex v_mutex;
    std::queue<VideoFrame> v_queue;

    AudioRingBuffer a_ring;

    // Start Params
    std::string url;
    int width, height, fps, bitrate, sample_rate;

    int64_t v_pts;
    int64_t a_pts;
    int sws_width, sws_height;

    void cleanup_ffmpeg() {
        /*
        if (fmt_ctx) {
            // Only write trailer if we actually started (v_stream exists)
            if (v_stream) av_write_trailer(fmt_ctx);
            if (fmt_ctx->pb) avio_closep(&fmt_ctx->pb);
            avformat_free_context(fmt_ctx);
            fmt_ctx = nullptr;
        }
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
        }
        if (v_ctx) { avcodec_free_context(&v_ctx); v_ctx = nullptr; }
        if (a_ctx) { avcodec_free_context(&a_ctx); a_ctx = nullptr; }

        v_stream = nullptr;
        a_stream = nullptr;
        */
        sws_width = 0;
        sws_height = 0;
    }

    bool internal_connect() {
        /*
        avformat_alloc_output_context2(&fmt_ctx, nullptr, "flv", url.c_str());
        if (!fmt_ctx) return false;

        // Add Video Stream
        if (!add_video_stream(width, height, fps, bitrate)) return false;

        // Add Audio Stream
        if (!add_audio_stream(sample_rate)) return false;

        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&fmt_ctx->pb, url.c_str(), AVIO_FLAG_WRITE) < 0) return false;
        }

        if (avformat_write_header(fmt_ctx, nullptr) < 0) return false;
        */
        v_pts = 0;
        a_pts = 0;
        return true;
    }

    /*
    bool add_video_stream(int w, int h, int fps, int bitrate) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) return false;

        v_stream = avformat_new_stream(fmt_ctx, nullptr);
        if (!v_stream) return false;

        v_ctx = avcodec_alloc_context3(codec);
        v_ctx->width = w;
        v_ctx->height = h;
        v_ctx->time_base = {1, fps};
        v_stream->time_base = v_ctx->time_base;
        v_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        v_ctx->bit_rate = bitrate;
        v_ctx->gop_size = fps * 2;
        v_ctx->max_b_frames = 0;

        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            v_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "preset", "veryfast", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);

        int ret = avcodec_open2(v_ctx, codec, &opts);
        av_dict_free(&opts); // Prevent leak

        if (ret < 0) return false;

        avcodec_parameters_from_context(v_stream->codecpar, v_ctx);
        return true;
    }

    bool add_audio_stream(int sample_rate) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!codec) return false;

        a_stream = avformat_new_stream(fmt_ctx, nullptr);

        a_ctx = avcodec_alloc_context3(codec);
        a_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        a_ctx->bit_rate = 128000;
        a_ctx->sample_rate = sample_rate;

        // Handle API differences for channel layout
#if LIBAVCODEC_VERSION_MAJOR >= 60
        AVChannelLayout stereo_layout;
        av_channel_layout_default(&stereo_layout, 2);
        av_channel_layout_copy(&a_ctx->ch_layout, &stereo_layout);
#else
        a_ctx->channels = 2;
        a_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
#endif

        a_stream->time_base = {1, sample_rate};

        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            a_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(a_ctx, codec, nullptr) < 0) return false;
        avcodec_parameters_from_context(a_stream->codecpar, a_ctx);
        return true;
    }
    */

    void loop() {
        if (!internal_connect()) {
            std::cerr << "jit.stream: Failed to connect to RTMP server." << std::endl;
            streaming = false;
            cleanup_ffmpeg();
            return;
        }

        std::vector<float> audio_scratch(2048);

        while (streaming) {
            // Process Video
            {
                std::lock_guard<std::mutex> lock(v_mutex);
                while (!v_queue.empty()) {
                    VideoFrame frame = std::move(v_queue.front());
                    v_queue.pop();
                    process_video(frame);
                }
            }

            // Process Audio
            int frame_size = 1024;
            int channels = 2;
            int samples_needed = frame_size * channels;

            if (audio_scratch.size() < samples_needed) audio_scratch.resize(samples_needed);

            while (a_ring.available() >= samples_needed) {
                if (a_ring.pop(audio_scratch.data(), samples_needed)) {
                    process_audio(audio_scratch.data(), frame_size);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void process_video(const VideoFrame& vf) {
        /*
        if (!v_ctx) return;

        AVFrame* frame = av_frame_alloc();
        frame->format = v_ctx->pix_fmt;
        frame->width = v_ctx->width;
        frame->height = v_ctx->height;
        av_frame_get_buffer(frame, 32);

        // Check scaler context
        if (!sws_ctx || sws_width != vf.width || sws_height != vf.height) {
            if (sws_ctx) sws_freeContext(sws_ctx);
            sws_ctx = sws_getContext(vf.width, vf.height, AV_PIX_FMT_ARGB,
                                     v_ctx->width, v_ctx->height, v_ctx->pix_fmt,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
            sws_width = vf.width;
            sws_height = vf.height;
        }

        if (sws_ctx) {
            const uint8_t* srcSlice[] = { vf.data.data() };
            int srcStride[] = { vf.stride };
            sws_scale(sws_ctx, srcSlice, srcStride, 0, vf.height, frame->data, frame->linesize);

            frame->pts = v_pts++;

            int ret = avcodec_send_frame(v_ctx, frame);
            av_frame_free(&frame);

            if (ret >= 0) {
                AVPacket* pkt = av_packet_alloc();
                while (avcodec_receive_packet(v_ctx, pkt) >= 0) {
                    av_packet_rescale_ts(pkt, v_ctx->time_base, v_stream->time_base);
                    pkt->stream_index = v_stream->index;
                    av_interleaved_write_frame(fmt_ctx, pkt);
                    av_packet_unref(pkt);
                }
                av_packet_free(&pkt);
            }
        } else {
             av_frame_free(&frame);
        }
        */
    }

    void process_audio(const float* samples, int nb_samples) {
        /*
        if (!a_ctx) return;

        AVFrame* frame = av_frame_alloc();
        frame->nb_samples = nb_samples;
        frame->format = a_ctx->sample_fmt;
#if LIBAVCODEC_VERSION_MAJOR >= 60
        AVChannelLayout stereo_layout;
        av_channel_layout_default(&stereo_layout, 2);
        av_channel_layout_copy(&frame->ch_layout, &stereo_layout);
#else
        frame->channels = 2;
        frame->channel_layout = AV_CH_LAYOUT_STEREO;
#endif
        frame->sample_rate = a_ctx->sample_rate;

        av_frame_get_buffer(frame, 0);
        if (av_frame_make_writable(frame) < 0) {
             av_frame_free(&frame);
             return;
        }

        float* dst_l = (float*)frame->data[0];
        float* dst_r = (float*)frame->data[1];

        for (int i = 0; i < nb_samples; ++i) {
            dst_l[i] = samples[2 * i];
            dst_r[i] = samples[2 * i + 1];
        }

        frame->pts = a_pts;
        a_pts += nb_samples;

        int ret = avcodec_send_frame(a_ctx, frame);
        av_frame_free(&frame);

        if (ret >= 0) {
            AVPacket* pkt = av_packet_alloc();
            while (avcodec_receive_packet(a_ctx, pkt) >= 0) {
                av_packet_rescale_ts(pkt, a_ctx->time_base, a_stream->time_base);
                pkt->stream_index = a_stream->index;
                av_interleaved_write_frame(fmt_ctx, pkt);
                av_packet_unref(pkt);
            }
            av_packet_free(&pkt);
        }
        */
    }
};


// Max Object
struct t_jit_stream {
    t_pxobject ob;
    Streamer* streamer;

    t_symbol* url;
    long enable;
    long width;
    long height;
    long fps;
    long bitrate;
    double samplerate;
};

static t_class* jit_stream_class = nullptr;

void* jit_stream_new(t_symbol* s, long argc, t_atom* argv);
void jit_stream_free(t_jit_stream* x);
void jit_stream_dsp64(t_jit_stream* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void jit_stream_perform64(t_jit_stream* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);
void jit_stream_matrix(t_jit_stream* x, t_symbol* s, long argc, t_atom* argv);
void jit_stream_texture(t_jit_stream* x, t_symbol* s, long argc, t_atom* argv);
void jit_stream_assist(t_jit_stream* x, void* b, long m, long a, char* s);

// Ensure visibility of the entry point
extern "C" __attribute__((visibility("default"))) void ext_main(void* r) {
    common_symbols_init(); // Initialize standard symbols

    t_class* c = class_new("jit.stream", (method)jit_stream_new, (method)jit_stream_free, sizeof(t_jit_stream), NULL, A_GIMME, 0);

    class_addmethod(c, (method)jit_stream_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)jit_stream_matrix, "jit_matrix", A_GIMME, 0);
    class_addmethod(c, (method)jit_stream_texture, "jit_gl_texture", A_GIMME, 0);
    class_addmethod(c, (method)jit_stream_assist, "assist", A_CANT, 0);

    CLASS_ATTR_SYM(c, "url", 0, t_jit_stream, url);
    CLASS_ATTR_LONG(c, "enable", 0, t_jit_stream, enable);
    CLASS_ATTR_LONG(c, "width", 0, t_jit_stream, width);
    CLASS_ATTR_LONG(c, "height", 0, t_jit_stream, height);
    CLASS_ATTR_LONG(c, "fps", 0, t_jit_stream, fps);
    CLASS_ATTR_LONG(c, "bitrate", 0, t_jit_stream, bitrate);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    jit_stream_class = c;
}

void* jit_stream_new(t_symbol* s, long argc, t_atom* argv) {
    t_jit_stream* x = (t_jit_stream*)object_alloc(jit_stream_class);

    dsp_setup((t_pxobject*)x, 2);

    x->streamer = new Streamer();
    x->url = gensym("rtmp://localhost/live/stream");
    x->enable = 0;
    x->width = 1280;
    x->height = 720;
    x->fps = 30;
    x->bitrate = 2500000;
    x->samplerate = 44100.0;

    attr_args_process(x, argc, argv);

    return x;
}

void jit_stream_free(t_jit_stream* x) {
    dsp_free((t_pxobject*)x);
    if (x->streamer) {
        delete x->streamer;
    }
}

void jit_stream_assist(t_jit_stream* x, void* b, long m, long a, char* s) {
    if (m == ASSIST_INLET) {
        if (a == 0) snprintf(s, 256, "jit_matrix/texture (Video)");
        else snprintf(s, 256, "Signal %ld (Audio)", a);
    }
}

void jit_stream_matrix(t_jit_stream* x, t_symbol* s, long argc, t_atom* argv) {
    // Check enable
    if (!x->enable) {
        if (x->streamer) x->streamer->stop();
        return;
    }

    t_symbol* name = atom_getsym(argv);
    void* matrix = jit_object_findregistered(name);

    if (!matrix) return;

    // Use gensym for safety
    t_symbol* s_lock = gensym("lock");
    t_symbol* s_getinfo = gensym("getinfo");
    t_symbol* s_getdata = gensym("getdata");
    t_symbol* s_char = gensym("char");

    // Lock matrix
    long lockstate = (long)jit_object_method(matrix, s_lock, 1);

    t_jit_matrix_info in_info;
    char* in_bp = NULL;

    jit_object_method(matrix, s_getinfo, &in_info);
    jit_object_method(matrix, s_getdata, &in_bp);

    if (!in_bp) {
        jit_object_method(matrix, s_lock, lockstate);
        return;
    }

    VideoFrame frame;
    frame.width = in_info.dim[0];
    frame.height = in_info.dim[1];

    // Correct stride handling (handle negative stride)
    frame.stride = std::abs(in_info.dimstride[1]);
    frame.timestamp = 0;

    // Check type (assume char) and planecount (4 for ARGB/RGBA)
    if (in_info.type != s_char || in_info.planecount != 4) {
        jit_object_method(matrix, s_lock, lockstate);
        return;
    }

    size_t size = frame.stride * frame.height;
    frame.data.resize(size);
    memcpy(frame.data.data(), in_bp, size);

    jit_object_method(matrix, s_lock, lockstate);

    x->streamer->start(x->url->s_name, x->width, x->height, x->fps, x->bitrate, (int)x->samplerate);
    x->streamer->push_video(std::move(frame));
}

void jit_stream_texture(t_jit_stream* x, t_symbol* s, long argc, t_atom* argv) {
     // Textures not fully supported in this mode
     object_error((t_object*)x, "jit.stream: Texture input is not directly supported. Please use [jit.gl.asyncread] to convert to matrix, or use [jit.matrix] between the source and jit.stream.");
}

void jit_stream_dsp64(t_jit_stream* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags) {
    x->samplerate = samplerate;
    object_method(dsp64, gensym("dsp_add64"), x, jit_stream_perform64, 0, NULL);
}

void jit_stream_perform64(t_jit_stream* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam) {
    if (!x->enable) return;

    double* in_l = ins[0];
    double* in_r = ins[1];

    // Interleave on the fly and push to ring buffer
    const int CHUNK_SIZE = 256;
    float temp[CHUNK_SIZE * 2];

    int processed = 0;
    while (processed < sampleframes) {
        int to_process = std::min((long)CHUNK_SIZE, sampleframes - processed);
        for (int i = 0; i < to_process; ++i) {
            temp[2*i] = (float)in_l[processed + i];
            temp[2*i+1] = (float)in_r[processed + i];
        }
        x->streamer->push_audio_samples(temp, to_process * 2);
        processed += to_process;
    }
}
