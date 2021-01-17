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

#endif