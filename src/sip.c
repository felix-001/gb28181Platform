#include "public.h"
#include "conf.h"
#include "osip2/osip_mt.h"
#include "eXosip2/eXosip.h"
#include "xml.h"
#include "sip.h"
#include "sdp.h"
#include "rtp.h"

typedef int (*evt_handler_t)(eXosip_event_t *evtp); 

struct _sip_ctx_t {
    struct eXosip_t *eXo_ctx;
    conf_t *conf;
    char *usr_gbid;
    char *usr_host;
    char *usr_port;
    pthread_t tid;
    int run;
    int callid;
    rtp_ctx_t *rtp_ctx;
};

typedef enum {
    SIP_OUT,
    SIP_IN,
} sip_dierction_t;

typedef enum {
    SIP_CMD_TYPE_KEEPALIVE,
    SIP_CMD_TYPE_CATALOG,
    SIP_CMD_TYPE_ALARM,
} sip_cmdtype_t;

static void dump_sip_message(sip_ctx_t *ctx, osip_message_t *msg, sip_dierction_t direction)
{
    char *s;
    conf_t *conf = ctx->conf;

    if (!strcmp(conf->dbg, "on")) {
        osip_message_to_str(msg, &s, NULL);
        if (direction == SIP_OUT) {
            LOGI("[S==>C]:\n%s", s);
        } else {
            LOGI("[C==>S]:\n%s", s);
        }
    }
}

int send_ack_200(sip_ctx_t *ctx, eXosip_event_t *evtp)
{
    int ret = 0 ;
    struct eXosip_t *eXo_ctx = ctx->eXo_ctx;
    osip_message_t * msg = NULL;

    ret = eXosip_message_build_answer(eXo_ctx, evtp->tid, 200, &msg);
    if (ret || !msg) {
        LOGE("build answer for register error ret:%d", ret);
        return -1;
    }
    eXosip_lock(eXo_ctx);
    eXosip_message_send_answer(eXo_ctx, evtp->tid, 200, msg);
    //dump_sip_message(msg, SIP_OUT);
    eXosip_unlock(eXo_ctx);
    return 0;
}

static int send_catalog_req(sip_ctx_t *ctx)
{
    char body[1024] = {0};
    char from[1024] = {0};
    char to[1024] = {0};
    osip_message_t *msg;
    struct eXosip_t *eXo_ctx = ctx->eXo_ctx;
    conf_t *conf = ctx->conf;

    LOGI("send catalog req to client");
    sprintf(from, "sip:%s@%s:%d", conf->srv_gbid, conf->srv_ip, atoi(conf->srv_sip_port));
    sprintf(to, "sip:%s@%s:%d", ctx->usr_gbid, ctx->usr_host, atoi(ctx->usr_port));

    sprintf(body, "<?xml version=\"1.0\"?>\r\n"
                  "<Query>\r\n"
                  "<CmdType>Catalog</CmdType>\r\n"
                  "<SN>1</SN>\r\n"
                  "<DeviceID>%s</DeviceID>\r\n"
                  "</Query>\r\n", ctx->usr_gbid);

    eXosip_message_build_request(eXo_ctx, &msg, "MESSAGE", to, from, NULL);
    osip_message_set_body(msg, body, strlen(body));
    osip_message_set_content_type(msg, "Application/MANSCDP+xml");
    eXosip_message_send_request(eXo_ctx, msg);	
    //dump_sip_message(ctx, msg, SIP_OUT);
    return 0;
}

int register_handler(sip_ctx_t *ctx, eXosip_event_t *evtp)
{
    osip_contact_t *contact;

    int ret = osip_message_get_contact(evtp->request, 0, &contact);
    if (ret < 0) {
        LOGE("get contact from message error: %d", ret);
        return ret;
    }
    osip_uri_t *uri;
    uri = osip_contact_get_url(contact);
    if (!uri || !uri->username || !uri->host || !uri->port) {
        LOGE("get url from contact error");
        return -1;
    }
    ctx->usr_gbid = strdup(uri->username);
    ctx->usr_host = strdup(uri->host);
    ctx->usr_port = strdup(uri->port);
    LOGI("got register from : %s %s:%s", ctx->usr_gbid, ctx->usr_host, ctx->usr_port);
    ret = send_ack_200(ctx, evtp);
    if (ret < 0)
        return ret;

    osip_header_t *expires = NULL;
    osip_message_get_expires(evtp->request, 0, &expires);
    LOGI("expires:%s", expires->hvalue);
    if (expires && atoi(expires->hvalue) > 0) {
        return send_catalog_req(ctx);
    }

    return 0;
}

static int get_cmdtype(eXosip_event_t *evtp, sip_cmdtype_t *cmdtype)
{
    char *body_str = NULL;
    osip_body_t *body;
    char *value = NULL;
    int ret = 0;
    size_t len = 0;

    ret = osip_message_get_body(evtp->request, 0, &body);
    if (ret < 0) {
        LOGE("get body error %d", ret);
        return -1;
    }
    osip_body_to_str(body, &body_str, &len);
    body_str = (char *)realloc(body_str, len+1);
    body_str[len] = '\0';
    ret = xml_get_item(body_str, "Notify/CmdType", &value);
    if (ret < 0) {
        ret = xml_get_item(body_str, "Response/CmdType", &value);
        if (ret < 0) {
            LOGE("get xml item error");
            return ret;
        }
    }
    free(body_str);
    if (!strcmp(value, "Keepalive")) {
        LOGI("got req cmd: %s", value);
        *cmdtype = SIP_CMD_TYPE_KEEPALIVE;
    } else if (!strcmp(value, "Alarm")) {
        *cmdtype = SIP_CMD_TYPE_ALARM;
    } else if (!strcmp(value, "Catalog")) {
        LOGI("got req cmd: %s", value);
        *cmdtype = SIP_CMD_TYPE_CATALOG;
    }
    return 0;
}

static int send_invite_req(sip_ctx_t *ctx)
{
    conf_t *conf = ctx->conf;
    struct eXosip_t *eXo_ctx = ctx->eXo_ctx;
    char from[1024] = {0};
    char to[1024] = {0};
    char expires[1024] = { 0 };
    osip_message_t *msg = NULL;

    LOGI("send invite req to client");
    sprintf(from, "sip:%s@%s:%d", conf->srv_gbid, conf->srv_ip, atoi(conf->srv_sip_port));
    sprintf(to, "sip:%s@%s:%d", ctx->usr_gbid, ctx->usr_host, atoi(ctx->usr_port));
    eXosip_call_build_initial_invite(eXo_ctx, &msg, to, from, NULL, NULL);
    char *sdp = NULL;
    int ret = gen_sdp(ctx->conf, &sdp);
    if (ret < 0)
        return ret;
    osip_message_set_body(msg, sdp, strlen(sdp));
	osip_message_set_content_type(msg, "application/sdp");
	snprintf(expires, sizeof(expires)-1, "%s;refresher=uac", conf->timeout);
	osip_message_set_header(msg, "Session-Expires", expires);
	osip_message_set_supported(msg, "timer");
	ctx->callid = eXosip_call_send_initial_invite(eXo_ctx, msg);
    //dump_sip_message(ctx, msg, SIP_OUT);
    return 0;
}


int message_handler(sip_ctx_t *ctx, eXosip_event_t *evtp)
{
    sip_cmdtype_t cmdtype = -1;
    conf_t *conf = ctx->conf;

    get_cmdtype(evtp, &cmdtype);
    if (cmdtype != SIP_CMD_TYPE_ALARM && cmdtype != SIP_CMD_TYPE_KEEPALIVE && cmdtype != SIP_CMD_TYPE_CATALOG) {
        dump_sip_message(ctx, evtp->request, SIP_IN);
    }
    send_ack_200(ctx, evtp);
    if (!strcmp(conf->auto_invite, "on") && cmdtype == SIP_CMD_TYPE_CATALOG) {
        send_invite_req(ctx);
    }

    return 0;
}

static int call_send_ack(sip_ctx_t *ctx, eXosip_event_t *evtp)
{
    osip_message_t* ack = NULL;
    LOGI("send invite ack to client");
    eXosip_call_build_ack(ctx->eXo_ctx, evtp->did, &ack);  
    eXosip_call_send_ack(ctx->eXo_ctx, evtp->did, ack);
    //dump_sip_message(ctx, ack, SIP_OUT);
    return 0;
}

static int evt_handler(sip_ctx_t *ctx, eXosip_event_t *evtp)
{
    switch (evtp->type) {
    case EXOSIP_MESSAGE_NEW:
        if (MSG_IS_REGISTER(evtp->request)) {
            register_handler(ctx, evtp);
        } else if (MSG_IS_MESSAGE(evtp->request)) {
            message_handler(ctx, evtp);
        } else {
            LOGI("evt type: %d", evtp->type);
        }
        break;
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        LOGI("%s", evtp->textinfo);
        if (evtp->ack)
            dump_sip_message(ctx, evtp->ack, SIP_IN);
        else if (evtp->response)
            dump_sip_message(ctx, evtp->response, SIP_IN);
        else if (evtp->request)
            dump_sip_message(ctx, evtp->request, SIP_OUT);
        break;
    case EXOSIP_MESSAGE_ANSWERED:
        LOGI("%s", evtp->textinfo);
        //dump_sip_message(ctx, evtp->response, SIP_IN);
        break;
    case EXOSIP_CALL_ANSWERED:
        LOGI("invite ok");
        LOGI("%s", evtp->textinfo);
        //dump_sip_message(ctx, evtp->response, SIP_IN);
        call_send_ack(ctx, evtp);
        //rtp_srv_run(ctx->rtp_ctx);
        break;
    case EXOSIP_CALL_MESSAGE_ANSWERED:
        LOGI("EXOSIP_CALL_MESSAGE_ANSWERED");
        break;
    case EXOSIP_CALL_CLOSED:
        LOGI("%s", evtp->textinfo);
        //dump_sip_message(ctx, evtp->request, SIP_IN);
        break;
    case EXOSIP_CALL_RELEASED:
        LOGI("%s", evtp->textinfo);
        break;
    case EXOSIP_CALL_MESSAGE_NEW:
        LOGI("%s", evtp->textinfo);
       // dump_sip_message(ctx, evtp->request, SIP_IN);
        break;
    case EXOSIP_CALL_PROCEEDING:
        LOGI("%s", evtp->textinfo);
        //dump_sip_message(ctx, evtp->response, SIP_IN);
        break;
    default:
        LOGI("recv evt: %d", evtp->type);
        dump_sip_message(ctx, evtp->request, SIP_IN);
        break;
    }

    return 0;
}

static void * sip_evtloop_thread(void *arg)
{
    #define TIMEOUT 20
    #define SLEEP 100000

    sip_ctx_t *ctx = (sip_ctx_t *)arg;
    struct eXosip_t *eXo_ctx = ctx->eXo_ctx;
    LOGI("enter event loop");

    while(ctx->run) {
		eXosip_event_t *evtp = eXosip_event_wait(eXo_ctx, 0, TIMEOUT);

        if (!evtp){
			eXosip_automatic_action(eXo_ctx);
			osip_usleep(SLEEP);
			continue;
		}
        eXosip_automatic_action(eXo_ctx);
        evt_handler(ctx, evtp);       
        eXosip_event_free(evtp);
    }
    return NULL;
}

sip_ctx_t * new_sip_context(conf_t *conf)
{
    sip_ctx_t *ctx = (sip_ctx_t *)malloc(sizeof(sip_ctx_t));
    ctx->conf = conf;
    struct eXosip_t *eXo_ctx = eXosip_malloc();
    if (!eXo_ctx) {
        LOGE("new uas context error");
        goto err;
    }
	if (eXosip_init(eXo_ctx)) {
        LOGE("exosip init error");
        goto err;
	}
    if (eXosip_listen_addr(eXo_ctx, IPPROTO_UDP, NULL, atoi(conf->srv_sip_port), AF_INET, 0)) {
        LOGE("listen error");
        goto err;
    }
    eXosip_set_user_agent(eXo_ctx, conf->ua);
    if (eXosip_add_authentication_info(eXo_ctx, conf->srv_gbid, conf->srv_gbid, conf->passwd, NULL, NULL)){
        LOGE("add authentication info error");
        goto err;
    }
    ctx->rtp_ctx = new_rtp_context(conf, atoi(conf->ssrc));
    if (!ctx->rtp_ctx)
        goto err;
    rtp_srv_run(ctx->rtp_ctx);
    ctx->eXo_ctx = eXo_ctx;
    ctx->run = 1;
    return ctx;
err:
    return NULL;
}

int sip_run(sip_ctx_t *ctx)
{
    if (!ctx)
        return -1;

    pthread_create(&ctx->tid, NULL, sip_evtloop_thread, (void*)ctx);
    return 0;
}