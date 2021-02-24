#include <SDL.h>
#include <SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include <math.h>
#include "public.h"
#include "queue.h"

#define FRAME_QUEUE_SIZE 5
#define REFRESH_RATE 0.01
#define AV_SYNC_THRESHOLD_MAX 0.1

typedef struct disp_t {
    AVCodecContext *codecCtx;
    struct SwsContext *swsCtx;
    queue_t *frame_queue;
    AVFrame *frame;
    double max_frame_duration;
    double default_duration;
    double frame_timer;
    SDL_Overlay *overlay;
    SDL_Surface *surface;
    int last_width;
    int last_height;
} disp_t;

static int sdl_init(disp_t *disp)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        LOGE("Could not initialize SDL - %s", SDL_GetError());
        return -1;
    }
#ifndef __DARWIN__
    disp->surface = SDL_SetVideoMode(640, 480, 0, 0);
#else
    disp->surface = SDL_SetVideoMode(640, 480, 24, 0);
#endif
    if (!disp->surface) {
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
    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        LOGE("Unsupported codec!");
        return -1;
    }
    disp->swsCtx = sws_getContext(codecCtx->width, codecCtx->height,
                                  codecCtx->pix_fmt, codecCtx->width,
                                  codecCtx->height, AV_PIX_FMT_YUV420P,
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
        LOGE("malloc error");
        return NULL;
    }
    memset(disp, 0, sizeof(*disp));
    disp->frame_timer = (double)av_gettime() / 1000000.0;
    disp->frame = av_frame_alloc();
    if (!disp->frame) {
        LOGE("mem error");
        return NULL;
    }
    disp->frame_queue = new_queue(FRAME_QUEUE_SIZE, frame_blk_alloc, disp);
    if (!disp->frame_queue)
        return NULL;
    sdl_init(disp);
    decoder_init(disp);
    return disp;
}

// Convert the image into YUV format that SDL uses
static int frame_to_yv12(disp_t *disp, AVFrame *frame)
{
    uint8_t *pixels[4];
    int pitch[4];
    SDL_Overlay *overlay = disp->overlay;

    pixels[0] = overlay->pixels[0];
    pixels[1] = overlay->pixels[2];
    pixels[2] = overlay->pixels[1];
    pitch[0] = overlay->pitches[0];
    pitch[1] = overlay->pitches[2];
    pitch[2] = overlay->pitches[1];
    sws_scale(disp->swsCtx, 
              (uint8_t const * const *)frame->data,
	          frame->linesize,
              0, 
              disp->codecCtx->height,
	          pixels, 
              pitch);
   return 0; 
}

static int compute_disp_rect(disp_t *disp, SDL_Rect *rect)
{
    int w, h, x, y;
    float aspect_ratio;
    AVCodecContext *codecCtx = disp->codecCtx;
    SDL_Surface *surface = disp->surface;

    if(!codecCtx->sample_aspect_ratio.num) {
        aspect_ratio = 0;
    } else {
        // FIXME:codecCtx->width and codecCtx->height maybe nened to use AVFrame's
        aspect_ratio = av_q2d(codecCtx->sample_aspect_ratio) * codecCtx->width / codecCtx->height;
    }

    if(aspect_ratio <= 0.0) {
        aspect_ratio = (float)codecCtx->width / (float)codecCtx->height;
    }

    h = surface->h;
    w = ((int)rint(h * aspect_ratio)) & -3;
    if(w > surface->w) {
        w = surface->w;
        h = ((int)rint(w / aspect_ratio)) & -3;
    }
    x = (surface->w - w) / 2;
    y = (surface->h - h) / 2;
    
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;

    return 0;
}

static int create_overlay(disp_t *disp, AVFrame *frame)
{
    if (disp->overlay &&
        frame->width == disp->last_width &&
        frame->height == disp->last_height) {
        return 0;
    }
    if (disp->overlay) {
        SDL_FreeYUVOverlay(disp->overlay);
    }
    disp->overlay = SDL_CreateYUVOverlay(frame->width,
                                         frame->height,
                                         SDL_YV12_OVERLAY,
                                         disp->surface);
    disp->last_height = frame->height;
    disp->last_width = frame->width;
    return 0;
}

static int video_display(disp_t *disp)
{
    AVFrame *frame = queue_pop(disp->frame_queue);
    create_overlay(disp, frame); 
    frame_to_yv12(disp, frame);
    SDL_Rect rect;
    compute_disp_rect(disp, &rect);
    SDL_DisplayYUVOverlay(disp->overlay, &rect);
    return 0;
}

static void *evt_handle_thread(void *arg)
{
    SDL_Event event;

    for (;;) {
        SDL_WaitEvent(&event);
        switch (event.type)
        {
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

double pts2sec(int64_t pts, AVRational time_base)
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
    AVFrame *frame = queue_peek(disp->frame_queue);
    AVFrame *last_frame = queue_peek_last(disp->frame_queue);
    double delay = get_duration(disp, frame, last_frame);
    double cur_time = av_gettime_relative()/1000000.0;
    // 还没到这一帧需要显示的时间
    if (cur_time < disp->frame_timer + delay) {
        *remaining_time = FFMIN(disp->frame_timer + delay - cur_time, *remaining_time);
        return 0;
    }
    disp->frame_timer += delay;
    // 这一帧来的太晚了
    if (delay > 0 && cur_time - disp->frame_timer > AV_SYNC_THRESHOLD_MAX)
        disp->frame_timer = cur_time;

     if (queue_size(disp->frame_queue) > 1) {
         AVFrame *next_frame = queue_peek_next(disp->frame_queue);
         delay = get_duration(disp, frame, next_frame);
         if (cur_time > disp->frame_timer + delay) {
             // 丢弃太晚的帧
             queue_pop(disp->frame_queue);
             return 0;
         }
     }
     video_display(disp);
     return 0;
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


int decode(disp_t *disp, uint8_t *video, uint32_t len, int64_t pts)
{
    AVPacket pkt;
    
    av_init_packet(&pkt);
    pkt.pts = pts;
    pkt.dts = pts;
    int ret = av_packet_from_data(&pkt, video, len);
    if (ret < 0) {
        LOGE("av pkt from data error");
        return -1;
    }
    ret = avcodec_send_packet(disp->codecCtx, &pkt);
    if (ret < 0) {
        LOGE("Error sending a packet for decoding");
        return -1;
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(disp->codecCtx, disp->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            LOGE("Error during decoding\n");
            return -1;
        }
    }
    if (queue_push(disp->frame_queue, disp->frame) < 0) 
        return -1;

    return 0;
}