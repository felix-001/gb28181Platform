#include "public.h"
#include "conf.h"
#include "rtp.h"

#define RTP_HEADER_LEN (12)

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

void *rtp_recv_srv(void *arg)
{
    conf_t *conf = (conf_t *)arg;
    return NULL;
}

int rtp_srv_start(conf_t *conf)
{
    pthread_t tid;
    pthread_create(&tid, NULL, rtp_recv_srv, conf);
    return 0;
}