#ifndef RTP_H
#define RTP_H

#include "public.h"

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