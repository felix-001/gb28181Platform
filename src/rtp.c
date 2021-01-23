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

static void dbg_dump_buf(uint8_t *buf, size_t size)
{
    for (int i=0; i<size; i++) {
        printf("0x%x ", buf[i]);
    }
    printf("\n");
}

#define RTP_MAX_LEN (1414)
#define PS_MAX_LEN (1024*1024*4)
// https://datatracker.ietf.org/doc/rfc4571/?include_text=1
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    ---------------------------------------------------------------
//   |             LENGTH            |  RTP or RTCP packet ...       |
//    ---------------------------------------------------------------
#define RFC_4571_LEN (2)

int read_rtp(int fd, uint8_t *rtp_buf)
{
    short rtp_len = 0;
    int len = 0;
    int read_len = 0;

    len = read(fd, (uint8_t *)&rtp_len, RFC_4571_LEN);
    if (len < 0) {
        LOGE("read error, %s", strerror(errno));
        return -1;
    }
    if (len == 0) {
        LOGI("EOF");
        goto exit;
    }
    rtp_len = ntohs(rtp_len);
    while (rtp_len > 0) {
        len = read(fd, rtp_buf + read_len, rtp_len);
        if (len < 0) {
            LOGE("read error, %s", strerror(errno));
            return -1;
        }
        if (len == 0) {
            LOGI("EOF");
            goto exit;
        } 
        read_len += len;
        rtp_len -= read_len;
    }

    //dbg_dump_buf(rtp_buf, 32);
exit:
    return read_len;
}

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
    LOGI("rtp srv listen on %s:%s", conf->srv_ip, conf->rtp_port);
    connfd = accept(listenfd, (struct sockaddr *)&client_addr, (socklen_t*)&c);
    LOGI("got connection from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    while(ctx->run) {
        len = read_rtp(connfd, rtp_buf);
        if (len <= 0) {
            LOGI("len:%d", len);
            goto exit;
        }
        if (len <= sizeof(RTP_header_t)) {
            LOGE("got rtp pkt len:%d", len);
            continue;
        }
        RTP_header_t *hdr = (RTP_header_t *)rtp_buf;
        if (ntohl(hdr->ssrc) != ctx->ssrc) {
            LOGE("check ssrc error %u:%u", ntohl(hdr->ssrc), ntohl(ctx->ssrc));
            continue;
        }
        if (ctx->cur_timestamp == (uint32_t)-1) {
            memcpy(ps_buf+ps_len, rtp_buf+sizeof(RTP_header_t), len - sizeof(RTP_header_t));
            ps_len += len - sizeof(RTP_header_t);
            ctx->cur_timestamp = hdr->timestamp;
        } else if (hdr->timestamp == ctx->cur_timestamp){
            if (ps_len + len - sizeof(RTP_header_t) > PS_MAX_LEN) {
                LOGE("ps buf overflow, %lu", ps_len + len - sizeof(RTP_header_t));
                ps_len = 0;
                continue;
            }
            //LOGI("len:%d rtp header len:%d",len, sizeof(RTP_header_t));
            memcpy(ps_buf+ps_len, rtp_buf+sizeof(RTP_header_t), len - sizeof(RTP_header_t));
            ps_len += len - sizeof(RTP_header_t);
        } else {
            ps_pkt_t ps_pkt;
            if (!ps_len) {
                ps_len = len - sizeof(RTP_header_t);
            }
            //LOGI("got ps len:%d", ps_len);
            int ret = ps_decode(ctx->ps_decoder, ps_buf, ps_len, &ps_pkt);
            if (ret < 0) {
                //dbg_dump_buf(ps_buf, 32);
                return NULL;
            }
            ps_len = 0;
            memcpy(ps_buf+ps_len, rtp_buf+sizeof(RTP_header_t), len - sizeof(RTP_header_t));
            ps_len += len - sizeof(RTP_header_t);
            ctx->cur_timestamp = hdr->timestamp;
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
    ctx->cur_timestamp = (uint32_t)-1;
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