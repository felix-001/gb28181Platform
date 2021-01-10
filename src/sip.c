#include "public.h"
#include "osip2/osip_mt.h"
#include "eXosip2/eXosip.h"

static struct eXosip_t *ctx;

static void * sip_evtloop_thread(void *arg)
{
    return NULL;
}

int sip_init(conf_t *conf)
{
    ctx = eXosip_malloc();
    if (!ctx) {
        LOGE("new uas context error");
        goto err;
    }
	if (eXosip_init(ctx)) {
        LOGE("exosip init error");
        goto err;
	}
    if (eXosip_listen_addr(ctx, IPPROTO_UDP, NULL, atoi(conf->sip_port), AF_INET, 0)) {
        LOGE("listen error");
        goto err;
    }
    eXosip_set_user_agent(ctx, conf->ua);
    if (eXosip_add_authentication_info(ctx, conf->srv_gbid, conf->srv_gbid, conf->passwd, NULL, NULL)){
        LOGE("add authentication info error");
        goto err;
    }
    pthread_t tid;
    pthread_create(&tid, NULL, sip_evtloop_thread, NULL);
    return 0;
err:
    return -1;
}
