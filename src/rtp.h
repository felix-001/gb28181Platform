#ifndef RTP_H
#define RTP_H

#include "public.h"

typedef struct {
    uint8_t version;
    uint8_t padding;
    uint8_t extension;
    uint8_t csrc_count;
    uint8_t marker;
    uint8_t payload_type;
    uint16_t seq_num;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_pkt_t;

typedef struct
{
    uint16_t cc:4;
    uint16_t extbit:1;
    uint16_t padbit:1;
    uint16_t version:2;
    uint16_t paytype:7;
    uint16_t markbit:1;
    uint16_t seq_number;
    uint32_t timestamp;
    uint32_t ssrc;
} RTP_header_t;

typedef struct _rtp_ctx rtp_ctx_t;

extern rtp_ctx_t *new_rtp_context(conf_t *conf, uint32_t ssrc);
extern int rtp_srv_run(rtp_ctx_t *ctx);

#endif