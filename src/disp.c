#include <SDL.h>
#include <SDL_thread.h>
#include "public.h"
#include "mem_pool.h"

typedef struct disp_t {
    SDL_Surface *screen;
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;
    AVCodecContext *codecCtx;
    struct SwsContext *swsCtx;
    double frame_timer;
    frame_last_delay double;
    AVFrame *frame;
    double video_clock;
    mem_pool_t *mem_pool;
} disp_t;

static uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque)
{
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = opaque;
    SDL_PushEvent(&event);
    return 0;
}

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
    if (!screen) {
        LOGE("SDL: could not set video mode - exiting");
        return -1;
    }
    disp->screen_mutex = SDL_CreateMutex();
    is->pictq_mutex = SDL_CreateMutex();
    is->pictq_cond = SDL_CreateCond();
    SDL_AddTimer(40, sdl_refresh_timer_cb, disp);

    return 0;
}

disp_t* new_disp()
{
    disp_t * disp = malloc(sizeof(disp_t));
    if (!disp) {
        LOG("malloc error");
        return NULL;
    }
    memset(disp, 0, sizeof(*disp));

    return disp;
}

static int video_display(disp_t *disp)
{
    return 0;
}

static int schedule_next(disp_t *disp)
{
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
            is->quit = 1;
            SDL_Quit();
            return 0;
            break;
        case FF_REFRESH_EVENT:
            video_refresh_timer(event.user.data1);
            break;
        default:
            break;
        }
    }
    return NULL;
}

static int decoder_init(disp_t *disp)
{
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;
    codec = avcodec_find_decoder(AV_CODEC_ID_H264;
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
  disp->frame = av_frame_alloc();
  if (!disp->frame) {
      LOGE("alloc frame error");
      return -1;
  }
  disp->swsCtx = sws_getContext(codecCtx->width, codecCtx->height,
				                codecCtx->pix_fmt, codecCtx->width,
				                codecCtx->height, PIX_FMT_YUV420P,
				                SWS_BILINEAR, NULL, NULL, NULL);
  disp->frame_timer = (double)av_gettime() / 1000000.0;
  disp->frame_last_delay = 40e-3;
  disp->codecCtx = codecCtx;
}

static void *disp_thread(void *arg)
{
    disp_t *disp = (disp_t *)arg;

    sdl_init(disp);
    decoder_init(disp);

    for (;;) {

    }
    return NULL;
}

int start_disp(disp_t *disp)
{
    pthread_t tid;
    SDL_AddTimer(40, sdl_refresh_timer_cb, disp);
    pthread_create(&tid, NULL, evt_handle_thread, disp);
    pthread_create(&tid, NULL, disp_thread, disp);
    return 0;
}

static double synchronize_video(disp_t *disp, AVFrame *frame, double pts) {

  double frame_delay;

  if(pts != 0) {
      disp->video_clock = pts;
  } else {
      pts = is->video_clock;
  }
  frame_delay = av_q2d(disp->codecCtx->time_base);
  frame_delay += frame->repeat_pict * (frame_delay * 0.5);
  disp->video_clock += frame_delay;
  return pts;
}

static void *blk_alloc(size_t blk_size, void *opaque)
{
    disp_t *disp = (disp_t *)opaque;
    AVCodecContext *codecCtx = disp->codecCtx;

    if (!codecCtx)
        return -1;
    return SDL_CreateYUVOverlay(codecCtx->width,
				                icodecCtx->height,
				                SDL_YV12_OVERLAY,
				                disp->screen);
}

static void *blk_realloc(void *mem, size_t blk_size, void *opaque)
{
    disp_t *disp = (disp_t *)opaque;
    AVCodecContext *codecCtx = disp->codecCtx;

    if (!codecCtx)
        return -1;
    if (!mem)
        return -1;
    SDL_FreeYUVOverlay((SDL_Overlay *)mem);
    return SDL_CreateYUVOverlay(codecCtx->width,
				                icodecCtx->height,
				                SDL_YV12_OVERLAY,
				                disp->screen);
}

static int push_blk(void *src, void *mem, size_t blk_size, void *opaque)
{
    if (!mem || !opaque)
        return -1;
    AVFrame *frame = (AVFrame *)src;
    disp_t *disp = (disp_t *)opaque;
    AVPicture pict;
    SDL_Overlay *overlay = (SDL_Overlay *)mem;

    pict.data[0] = overlay->pixels[0];
    pict.data[1] = overlay->pixels[2];
    pict.data[2] = overlay->pixels[1];
    pict.linesize[0] = overlay->pitches[0];
    pict.linesize[1] = overlay->pitches[2];
    pict.linesize[2] = overlay->pitches[1];
    int ret = sws_scale(disp->swsCtx,
                        (uint8_t const *const *)frame->data,
                        frame->linesize,
                        0,
                        disp->codecCtx->height,
                        pict.data,
                        pict.linesize);
    if (ret < 0) {
        LOGE("sws scale error");
        return -1;
    }
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
        pts = synchronize_video(disp, frame, pts);
        if (!disp->mem_pool) {
            AVCodecContext *codecCtx = disp->codecCtx;
            disp->mem_pool = new_mem_pool(5, 0, blk_alloc, blk_realloc, push_blk, disp);
            if (!disp->mem_pool)
                return -1;
            if (mem_pool_push_blk(disp->mem_pool, 
                                  frame, 
                                  codecCtx->height*codecCtx->width) < 0)
                return -1;
        }
    }

    return 0;
}