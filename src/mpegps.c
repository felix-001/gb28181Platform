#include "public.h"
#include "mpegps.h"
#include "conf.h"

typedef struct {
    uint8_t es_id;
    uint8_t *es;
    uint32_t es_len;
} stream_info_t;

struct _ps_decoder {
    uint8_t *ps_buf;
    uint8_t iskey;
    stream_info_t video_stream;
    stream_info_t audio_stream;
    int64_t video_pts;
    int64_t audio_pts;
    conf_t *conf;
    FILE *vfp;
};

#define VIDEO_FILE "./gb281_video.h264"
ps_decoder_t *new_ps_decoder(conf_t *conf)
{
    ps_decoder_t * decoder = (ps_decoder_t *)malloc(sizeof(ps_decoder_t));
    
    if (!decoder) {
        LOGE("malloc error");
        return NULL;
    }
    memset(decoder, 0, sizeof(*decoder));
    decoder->conf = conf;
    if (!strcmp(conf->dump_video_file, "on")) {
        decoder->vfp = fopen(VIDEO_FILE, "w");
        if (!decoder->vfp) {
            LOGE("open file %s error", VIDEO_FILE);
        }
    }

    return decoder;
}

#define uint16_len(buf) htons(*(uint16_t *)buf)

#define PACK_HEADER         (0x000001BA)
#define SYSTEM_HEADER       (0x000001BB)
#define PROGRAM_STREM_MAP   (0x000001BC)
#define PES_VIDEO_STREAM    (0x000001E0)
#define PES_AUDIO_STREAM    (0x000001C0)
#define PES_PRIVATE_STREAM  (0x000001BD)

typedef int (*decode_handler_t)(ps_decoder_t *decoder, uint32_t *pack_len);

#define PACK_HEADER_LEN (10)
static int pack_header_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    uint8_t stuffing_len = decoder->ps_buf[9] & 0x07;
    *pack_len = PACK_HEADER + stuffing_len;
    return 0;
}

#define SYSTEM_HEADER_LEN (2)
static int system_header_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    uint16_t len = uint16_len(decoder->ps_buf);
    *pack_len = SYSTEM_HEADER_LEN + len;
    decoder->iskey = 1;
    return 0;
}

#define PSM_HEADER_LEN (2)
#define STREAM_TYPE_VIDEO (0xE0)
#define STREAM_TYPE_AUDIO (0xC0)
static int program_system_map_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    uint8_t *ps_buf = decoder->ps_buf;
    uint16_t psm_len = uint16_len(ps_buf);

    *pack_len = PSM_HEADER_LEN + psm_len;
    ps_buf += 2;// skip indicator, version
    uint16_t psm_info_len = uint16_len(ps_buf);
    ps_buf += psm_info_len + 2;
    uint16_t es_map_len = uint16_len(ps_buf);
    ps_buf += 2;

    while(es_map_len >= 4) {
        uint8_t stream_type = *ps_buf++;
        
        if (stream_type == STREAM_TYPE_VIDEO) {
            decoder->video_stream.es_id = *ps_buf++;
        } else if (stream_type == STREAM_TYPE_AUDIO) {
            decoder->audio_stream.es_id = *ps_buf++;
        } else {
            LOGE("unknow stream_type: 0x%x", stream_type);
            ps_buf++;
        }
        uint16_t es_info_len = uint16_len(ps_buf);
        ps_buf += es_info_len + 2;
        es_map_len -= 4 + es_info_len;
    }
    *pack_len = ps_buf - decoder->ps_buf;
    return 0;
}

static int64_t parse_timestamp(const uint8_t* p)
{
	unsigned long b;
	unsigned long val, val2, val3;

	b = *p++;
	val = (b & 0x0e);

	b = (*(p++)) << 8;
	b += *(p++);
	val2 = (b & 0xfffe) >> 1;
	
	b = (*(p++)) << 8;
	b += *(p++);
	val3 = (b & 0xfffe) >> 1;

	val = (val << 29) | (val2 << 15) | val3;
	return val;
}

#define PES_FLAGS_LEN (2)
#define PES_HEADER_DATA_LEN (1)
#define TIMESTAMP_LEN (5)
static int  pes_decode(ps_decoder_t *decoder, int64_t *pts, stream_info_t *stream, uint32_t *pack_len)
{
    uint8_t *ps_buf = decoder->ps_buf;
    uint16_t pes_pkt_len = uint16_len(ps_buf);
    ps_buf += 2;
    uint8_t pts_dts_flags = ((*ps_buf++) & 0xF0) >> 6;
    ps_buf += 2;// skip escr es_rate ... flags
    uint8_t hdr_data_len = *ps_buf++;
    ps_buf += hdr_data_len;
    if (pts && pts_dts_flags) {
        *pts = parse_timestamp(ps_buf);
    }
    ps_buf += TIMESTAMP_LEN;
    uint32_t payload_len = pes_pkt_len - (ps_buf - decoder->ps_buf); 
    if (stream) {
        stream->es = ps_buf;
        stream->es_len = payload_len;
    }
    *pack_len = ps_buf - decoder->ps_buf;
    return 0;
}

static int pes_video_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return pes_decode(decoder, &decoder->video_pts, &decoder->video_stream, pack_len);
}

static int pes_audio_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return pes_decode(decoder, &decoder->audio_pts, &decoder->audio_stream, pack_len);
}

static int pes_private_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    return pes_decode(decoder, NULL, NULL, pack_len);
}

decode_handler_t handlers[] = 
{
    pes_private_stream_handler,
    pack_header_handler,
    NULL,
    program_system_map_handler,
    NULL,
    pes_video_stream_handler,
    system_header_handler,
    pes_audio_stream_handler,
};

#define FIBONACCI_64BIT (11400714819323198485llu)

static size_t hash(size_t hash)
{
    return (hash *FIBONACCI_64BIT) >> 61;
}

#define HEADER_LEN (4)
int ps_decode(ps_decoder_t *decoder, uint8_t *ps_buf, int ps_len, ps_pkt_t *ps_pkt)
{
    if (!decoder || !ps_buf || !ps_pkt)
        return -1;

    decoder->ps_buf = ps_buf;
    while(decoder->ps_buf < ps_buf + ps_len) {
        uint32_t header = ntohl(*(uint32_t *)ps_buf);
        decoder->ps_buf += HEADER_LEN;
        int idx = hash(header);
        if (handlers[idx]) {
            uint32_t pack_len = 0;
            handlers[idx](decoder, &pack_len);
            decoder->ps_buf += pack_len;
        }
    }

    if (!strcmp(decoder->conf->dump_video_file, "on") 
        && decoder->video_stream.es
        && decoder->vfp) {
        if (fwrite(decoder->video_stream.es, decoder->video_stream.es_len, 1, decoder->vfp) != 1) {
            LOGE("write video to file error");
        }
    }

    return 0;
}