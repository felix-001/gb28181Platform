#include <SDL.h>
#include <SDL_thread.h>
#include <math.h>
#include "public.h"
#include "queue.h"

#define FRAME_QUEUE_SIZE 5
#define REFRESH_RATE 0.01

typedef struct disp_t {
    AVCodecContext *codecCtx;
    struct SwsContext *swsCtx;
    queue_t *frame_queue;
    AVFrame *frame;
    double max_frame_duration;
    double default_duration;
} disp_t;

static int sdl_init(disp_t *disp)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        LOGE("Could not initialize SDL - %s", SDL_GetError());
        return -1;
    }
#ifndef __DARWIN__
    disp->screen = SDL_SetVideoMode(640, 480, 0, 0);
#else
    disp->screen = SDL_SetVideoMode(640, 480, 24, 0);
#endif
    if (!disp->screen) {
        LOGE("SDL: could not set video mode - exiting");
        return -1;
    }

    return 0;
}

static int decoder_init(disp_t *disp)
{
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE("Unsupported codec!");
        return -1;
    }
    codecCtx = avcodec_alloc_context3(codec);
    if (avcodec_copy_context(codecCtx, pFormatCtx->streams[stream_index]->codec) != 0) {
        LOGE("Couldn't copy codec context");
        return -1;
    }
    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        LOGE("Unsupported codec!");
        return -1;
    }
    disp->swsCtx = sws_getContext(codecCtx->width, codecCtx->height,
                                  codecCtx->pix_fmt, codecCtx->width,
                                  codecCtx->height, PIX_FMT_YUV420P,
                                  SWS_BILINEAR, NULL, NULL, NULL);
    disp->codecCtx = codecCtx;

    return 0;
}

static void *frame_blk_alloc(void *opaque, size_t *size)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOGE("mem error");
        return NULL;
    }
    *size = sizeof(AVFrame);
    return (void *)frame;
}

disp_t* new_disp()
{
    disp_t * disp = malloc(sizeof(disp_t));
    if (!disp) {
        LOG("malloc error");
        return NULL;
    }
    memset(disp, 0, sizeof(*disp));
    disp->frame = av_frame_alloc();
    if (!disp->frame) {
        LOGE("mem error");
        return -1;
    }
    disp->frame_queue = new_queue(FRAME_QUEUE_SIZE, frame_blk_alloc, disp);
    if (!disp->frame_queue)
        return -1;
    sdl_init(disp);
    decoder_init(disp);
    return disp;
}

static int video_display(disp_t *disp)
{
    SDL_Overlay *overlay = NULL;
    SDL_Rect rect;

    if (!overlay)
        return -1;
    compute_disp_coordinate(disp, &rect); 
    SDL_DisplayYUVOverlay(overlay, &rect);
    return 0;
}

static void *evt_handle_thread(void *arg)
{
    SDL_Event event;

    for (;;) {
        SDL_WaitEvent(&event);
        switch (event.type)
        {
        case FF_QUIT_EVENT:
        case SDL_QUIT:
            SDL_Quit();
            return 0;
            break;
        default:
            break;
        }
    }
    return NULL;
}

inline double pts2sec(int64_t pts, AVRational time_base)
{
    double sec = (pts == AV_NOPTS_VALUE) ? NAN : pts * av_q2d(time_base);
    return sec;
}

static double get_duration(disp_t *disp, AVFrame *frame, AVFrame *last_frame)
{
    double pts = pts2sec(frame->pts, disp->codecCtx->framerate);
    double last_pts = pts2sec(last_frame->pts, disp->codecCtx->framerate);
    double duration = pts - last_pts;
    if (isnan(duration) || duration <= 0 || duration > disp->max_frame_duration)
        return disp->default_duration;
    return duration;
}

int video_refresh(disp_t *disp, double *remaining_time)
{
    if (!queue_size(disp->frame_queue))
        return -1;
    AVFrame *frame = queue_peek(disp->queue);
    AVFrame *last_frame = queue_peek_last(disp->queue);
    double duration = get_duration(disp, frame, last_frame);
}

static void *disp_thread(void *arg)
{
    disp_t *disp = (disp_t *)arg;
    double remaining_time = 0.0;
    AVRational frame_rate = disp->codecCtx->framerate;

    disp->default_duration = frame_rate.num && frame_rate.den ? av_q2d(frame_rate) : 0;
    for (;;) {
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        video_refresh(disp, &remaining_time);

    }
    return NULL;
}

int start_disp(disp_t *disp)
{
    pthread_t tid;
    pthread_create(&tid, NULL, evt_handle_thread, disp);
    pthread_create(&tid, NULL, disp_thread, disp);
    return 0;
}


int decode(disp_t *disp, uint8_t *video, uin32_t len, int64_t pts)
{
    int done = 0;
    AVPacket pkt;
    
    av_init_packet(&pkt);
    pkt.pts = pts;
    pkt.dts = pts;
    int ret = av_packet_from_data(&pkt, video, len);
    if (ret < 0) {
        LOGE("av pkt from data error");
        return -1;
    }
    ret = avcodec_decode_video2(disp->codecCtx, disp->frame, &done, &pkt);
    if (ret < 0) {
        LOGE("decode video error");
        return -1;
    }
    if (done) {
        int ret = queue_push(disp->frame_queue, frame);
        if (ret < 0)
            return -1;
    }

    return 0;
}