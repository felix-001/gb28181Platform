#include "public.h"
#include "osip2/osip_mt.h"
#include "eXosip2/eXosip.h"

static struct eXosip_t *ctx;

static void * sip_evtloop_thread(void *arg)
{
    return NULL;
}

int sip_init(char *passwd, char *gb_id, char *user_agent, int port)
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
    if (eXosip_listen_addr(ctx, IPPROTO_UDP, NULL, port, AF_INET, 0)) {
        LOGE("listen error");
        goto err;
    }
    eXosip_set_user_agent(ctx, user_agent);
    if (eXosip_add_authentication_info(ctx, gb_id, gb_id, passwd, NULL, NULL)){
        LOGE("add authentication info error");
        goto err;
    }
    pthread_t tid;
    pthread_create(&tid, NULL, sip_evtloop_thread, NULL);
    return 0;
err:
    return -1;
}
