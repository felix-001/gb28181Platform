#include <SDL.h>
#include <SDL_thread.h>
#include "public.h"
#include "queue.h"

typedef struct disp_t {
    SDL_Surface *screen;
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;
    AVCodecContext *codecCtx;
    struct SwsContext *swsCtx;
    double frame_timer;
    frame_last_delay double;
    queue_t *q;
} disp_t;

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque)
{
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = opaque;
    SDL_PushEvent(&event);
    return 0; /* 0 means stop timer */
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
    disp->q = new_queue();

    for (;;) {

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