#include "public.h"
#include "mpegps.h"

struct _ps_decoder {
    uint8_t *ps_buf;
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

#define PACK_HEADER         (0x000001BA)
#define SYSTEM_HEADER       (0x000001BB)
#define PROGRAM_STREM_MAP   (0x000001BC)
#define PES_VIDEO_STREAM    (0x000001E0)
#define PES_AUDIO_STREAM    (0x000001C0)
#define PES_PRIVATE_STREAM  (0x000001BD)

typedef struct {
    uint32_t pack_type;
    int (*handler)(ps_decoder_t *decoder, uint32_t *pack_len);
} decode_handler_t;

static int pack_header_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return 0;
}

static int system_header_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return 0;
}

static int program_system_map_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return 0;
}

static int pes_video_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return 0;
}

static int pes_audio_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return 0;
}

static int pes_private_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return 0;
}

decode_handler_t handlers[] = 
{
    {PACK_HEADER, pack_header_handler},
    {SYSTEM_HEADER, system_header_handler},
    {PROGRAM_STREM_MAP, program_system_map_handler},
    {PES_VIDEO_STREAM, pes_video_stream_handler},
    {PES_AUDIO_STREAM, pes_audio_stream_handler},
    {PES_PRIVATE_STREAM, pes_private_stream_handler},
};

int ps_decode(ps_decoder_t *decoder, uint8_t *ps_buf, int ps_len, ps_pkt_t *ps_pkt)
{
    if (!decoder || !ps_buf || !ps_pkt)
        return -1;

    uint32_t header = ntohl(*(uint32_t *)ps_buf);

    decoder->ps_buf = ps_buf;
    while(decoder->ps_buf < ps_buf + ps_len) {
        for(int i=0; i<arrsz(handlers); i++) {
            if (handlers[i].pack_type == header && handlers[i].handler) {
                uint32_t pack_len = 0;
                handlers[i].handler(decoder, &pack_len);
                decoder->ps_buf += pack_len;
            }
        }
    }

    return 0;
}