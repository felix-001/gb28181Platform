#include "public.h"
#include "osip2/osip_mt.h"
#include "eXosip2/eXosip.h"

typedef int (*evt_handler_t)(eXosip_event_t *evtp); 

typedef struct {
    struct eXosip_t *eXo_ctx;
    int run;
} gb_ctx_t;

static gb_ctx_t *ctx;

int send_ack_200(eXosip_event_t *evtp)
{
    int ret = 0 ;
    struct eXosip_t *eXo_ctx = ctx->eXo_ctx;
    osip_message_t * msg = NULL;

    ret = eXosip_message_build_answer (eXo_ctx, evtp->tid, 200, &msg);
    if (ret || !msg) {
        LOGE("build answer for register error ret:%d", ret);
        return -1;
    }
    eXosip_lock(eXo_ctx);
    eXosip_message_send_answer(eXo_ctx, evtp->tid, 200, msg);
    eXosip_unlock(eXo_ctx);
    return 0;
}

int register_handler(eXosip_event_t *evtp)
{
    return send_ack_200(evtp);
}

int message_handler(eXosip_event_t *evtp)
{
    return send_ack_200(evtp);
}

static int evt_handler(eXosip_event_t *evtp)
{
    switch (evtp->type) {
    case EXOSIP_MESSAGE_NEW:
        if (MSG_IS_REGISTER(evtp->request)) {
            register_handler(evtp);
        } else if (MSG_IS_MESSAGE(evtp->request)) {
            message_handler(evtp);
        } else {
            LOGI("evt type: %d", evtp->type);
        }
        break;
    default:
        break;
    }

    return 0;
}

static void * sip_evtloop_thread(void *arg)
{
    #define TIMEOUT 20
    #define SLEEP 100000

    struct eXosip_t *eXo_ctx = ctx->eXo_ctx;

    while(ctx->run) {
        osip_message_t *msg = NULL;
		eXosip_event_t *evtp = eXosip_event_wait(eXo_ctx, 0, TIMEOUT);

        if (!evtp){
			eXosip_automatic_action(eXo_ctx);
			osip_usleep(SLEEP);
			continue;
		}
        eXosip_automatic_action(eXo_ctx);
        evt_handler(evtp);       
        eXosip_event_free(evtp);
    }
    return NULL;
}

int sip_init(conf_t *conf)
{
    ctx = (gb_ctx_t *)malloc(sizeof(gb_ctx_t));
    struct eXosip_t *eXo_ctx = eXosip_malloc();
    if (!eXo_ctx) {
        LOGE("new uas context error");
        goto err;
    }
	if (eXosip_init(eXo_ctx)) {
        LOGE("exosip init error");
        goto err;
	}
    if (eXosip_listen_addr(eXo_ctx, IPPROTO_UDP, NULL, atoi(conf->sip_port), AF_INET, 0)) {
        LOGE("listen error");
        goto err;
    }
    eXosip_set_user_agent(eXo_ctx, conf->ua);
    if (eXosip_add_authentication_info(eXo_ctx, conf->srv_gbid, conf->srv_gbid, conf->passwd, NULL, NULL)){
        LOGE("add authentication info error");
        goto err;
    }
    ctx->eXo_ctx = eXo_ctx;
    pthread_t tid;
    pthread_create(&tid, NULL, sip_evtloop_thread, NULL);
    return 0;
err:
    return -1;
}
