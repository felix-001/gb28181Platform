#include "public.h"
#include "conf.h"
#include "rtp.h"
#include "mpegps.h"

struct _rtp_ctx {
    uint8_t run;
    conf_t *conf;
    pthread_t tid;
    uint32_t cur_timestamp;
    uint32_t ssrc;
    ps_decoder_t *ps_decoder;
};

int rtp_dec(uint8_t *data, rtp_pkt_t *rtp)
{
    uint8_t byte = *data++;

    rtp->version = (byte >> 6) & 0x03;
    rtp->padding = (byte >> 5) & 0x01;
    rtp->extension = (byte >> 4) & 0x01;
    rtp->csrc_count = byte & 0x0f;
    byte = *data++;
    rtp->marker = (byte >> 7) & 0x01;
    rtp->payload_type = byte & 0x7f;
    rtp->seq_num = ntohs(*(short *)data);
    data += 2;
    rtp->timestamp = ntohl(*(int *)data);
    data +=4;
    rtp->ssrc = ntohl(*(int *)data);
    data += 4;
    return 0;
}

#define RTP_MAX_LEN (4096)
#define PS_MAX_LEN (1024*1024*4)
static void *rtp_recv_thread(void *arg)
{
    int listenfd = 0, connfd = 0, ps_len = 0;
    int len = 0, c = sizeof(struct sockaddr_in);
    rtp_ctx_t *ctx = (rtp_ctx_t *)arg;
    conf_t *conf = ctx->conf;
    struct sockaddr_in servaddr, client_addr;
    uint8_t *rtp_buf = (uint8_t *)malloc(RTP_MAX_LEN);
    uint8_t *ps_buf = (uint8_t *)malloc(PS_MAX_LEN);

    if (!rtp_buf)
        return NULL;
    if (!ps_buf)
        goto exit;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&servaddr, '0', sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(conf->srv_ip);
    servaddr.sin_port = htons(atoi(conf->rtp_port));
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(listenfd, 10);
    LOGI("listen on %s:%s", conf->srv_ip, conf->rtp_port);
    connfd = accept(listenfd, (struct sockaddr *)&client_addr, (socklen_t*)&c);
    LOGI("got connection from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    while(ctx->run) {
        len = read(connfd, rtp_buf, sizeof(rtp_buf));
        if (len < 0) {
            LOGE("read error, %s", strerror(errno));
            goto exit;
        }
        if (len < sizeof(RTP_header_t)) {
            LOGE("got rtp pkt len:%d", len);
            continue;
        }
        RTP_header_t *hdr = (RTP_header_t *)rtp_buf;
        if (hdr->ssrc != ctx->ssrc) {
            LOGE("check ssrc error 0x%x:0x%x", hdr->ssrc, ctx->ssrc);
            continue;
        }
        if (!ctx->cur_timestamp) {
            ctx->cur_timestamp = hdr->timestamp;
        } else if (hdr->timestamp == ctx->cur_timestamp){
            if (ps_len + len - sizeof(RTP_header_t) > PS_MAX_LEN) {
                LOGE("ps buf overflow, %lu", ps_len + len - sizeof(RTP_header_t));
                ps_len = 0;
                continue;
            }
            memcpy(ps_buf+ps_len, rtp_buf+sizeof(RTP_header_t), len - sizeof(RTP_header_t));
            ps_len += len - sizeof(RTP_header_t);
        } else {
            ps_pkt_t ps_pkt;
            ps_decode(ctx->ps_decoder, ps_buf, ps_len, &ps_pkt);
        }
    }

exit:
    if (ps_buf)
        free(ps_buf);
    return NULL;
}

rtp_ctx_t *new_rtp_context(conf_t *conf, uint32_t ssrc)
{
    rtp_ctx_t *ctx = (rtp_ctx_t *)malloc(sizeof(rtp_ctx_t));

    if (!ctx) {
        LOGE("new rtp context error");
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->conf = conf;
    ctx->run = 1;
    ctx->ssrc = ssrc;
    ctx->ps_decoder = new_ps_decoder(conf);
    if (!ctx->ps_decoder)
        return NULL;

    return ctx;
}

int rtp_srv_run(rtp_ctx_t *ctx)
{
    if (!ctx)
        return -1;
    
    pthread_create(&ctx->tid, NULL, rtp_recv_thread, (void*)ctx);
    return 0;
}