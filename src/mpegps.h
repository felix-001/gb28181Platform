#ifndef MPEGPS_H
#define MPEGPS_H

#include "public.h"

typedef struct _ps_decoder ps_decoder_t;
typedef struct {
    uint8_t *h264_buf;
    size_t h264_len;
    uint32_t timestamp;
    uint8_t es_type;
} ps_pkt_t;

extern ps_decoder_t *new_ps_decoder();
extern int ps_decode(ps_decoder_t *decoder, uint8_t *ps_buf, int ps_len, ps_pkt_t *ps_pkt);

#endif