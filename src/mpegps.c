#include "public.h"
#include "mpegps.h"

struct _ps_decoder {
};

ps_decoder_t *new_ps_decoder()
{
    ps_decoder_t * decoder = (ps_decoder_t *)malloc(sizeof(ps_decoder_t));
    
    if (!decoder) {
        LOGE("malloc error");
        return NULL;
    }

    return decoder;
}

int ps_decode(ps_decoder_t *decoder, uint8_t *ps_buf, int ps_len, ps_pkt_t *ps_pkt)
{
    if (!decoder || !ps_buf || !ps_pkt)
        return -1;

    return 0;
}