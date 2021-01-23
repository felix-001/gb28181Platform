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
#define MAX_VIDEO_LEN (4*1024*1024)
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
    decoder->video_stream.es = (uint8_t *)malloc(MAX_VIDEO_LEN);
    if (!decoder->video_stream.es) {
        LOGE("malloc error");
        return NULL;
    }

    return decoder;
}

#define uint16_len(buf) ntohs(*(uint16_t *)buf)

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
    LOGI("pack header");
    uint8_t stuffing_len = decoder->ps_buf[9] & 0x07;
    *pack_len = PACK_HEADER_LEN + stuffing_len;
    return 0;
}

#define SYSTEM_HEADER_LEN (2)
static int system_header_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    LOGI("system header");
    uint16_t len = uint16_len(decoder->ps_buf);
    *pack_len = SYSTEM_HEADER_LEN + len;
    decoder->iskey = 1;
    return 0;
}

#define PSM_HEADER_LEN (2)
#define PSM_CRC_LEN (4)
#define STREAM_TYPE_H264 (0x1b)
#define STREAM_TYPE_G711 (0x90)
static int program_system_map_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    LOGI("program system map");
    uint8_t *ps_buf = decoder->ps_buf;
    uint16_t psm_len = uint16_len(ps_buf);

    //dbg_dump_buf(ps_buf, 160);
    ps_buf += 2;
    *pack_len = PSM_HEADER_LEN + psm_len;
    LOGI("psm_len:%d", psm_len);
    ps_buf += 2;// skip indicator, version
    uint16_t psm_info_len = uint16_len(ps_buf);
    LOGI("psm_info_len:%d", psm_info_len);
    ps_buf += psm_info_len + 2;
    uint16_t es_map_len = uint16_len(ps_buf);
    ps_buf += 2;
    LOGI("es_map_len:%d", es_map_len);

    while(es_map_len >= 4) {
        uint8_t stream_type = *ps_buf++;
        
        if (stream_type == STREAM_TYPE_H264) {
            decoder->video_stream.es_id = *ps_buf++;
        } else if (stream_type == STREAM_TYPE_G711) {
            decoder->audio_stream.es_id = *ps_buf++;
        } else if (stream_type == 0) {
            LOGE("check stream type error");
            return -1;
        } else {
            LOGE("unknow stream_type: 0x%x", stream_type);
            ps_buf++;
        }
        LOGI("stream_type:0x%x", stream_type);
        LOGI("es_id:0x%x", decoder->video_stream.es_id);
        uint16_t es_info_len = uint16_len(ps_buf);
        LOGI("es_info_len:%d", es_info_len);
        ps_buf += es_info_len + 2;
        es_map_len -= 4 + es_info_len;
    }
    ps_buf += PSM_CRC_LEN;
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
    //dbg_dump_buf(ps_buf, 64);
    uint16_t pes_pkt_len = uint16_len(ps_buf);
    //LOGI("pes_pkt_len:%d", pes_pkt_len);
    ps_buf += 2;
    uint8_t pts_dts_flags = ((*ps_buf++) & 0xF0) >> 6;
    ps_buf++;// skip escr es_rate ... flags
    uint8_t hdr_data_len = *ps_buf++;
    ps_buf += hdr_data_len;
    //LOGI("hdr_data_len:%d", hdr_data_len);
    // hdr_data_len already contain timestamp len(5bytes)
    // so here needn't to ps_buf += TIMESTAMP_LEN;
    if (pts_dts_flags) {
        if (pts)
            *pts = parse_timestamp(ps_buf);
    }
    uint32_t payload_len = pes_pkt_len - (ps_buf - decoder->ps_buf - 2);// 2 - pes_pkt_len 
    LOGI("payload_len:%d", payload_len);
    //dbg_dump_buf(ps_buf, 64);
    if (stream) {
        memcpy(stream->es+stream->es_len, ps_buf, payload_len);
        stream->es_len += payload_len;
    }
    ps_buf += payload_len;
    *pack_len = ps_buf - decoder->ps_buf;
    return 0;
}

static int pes_video_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    LOGI("video");
    return pes_decode(decoder, &decoder->video_pts, &decoder->video_stream, pack_len);
}

static int pes_audio_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    LOGI("audio");
    return pes_decode(decoder, &decoder->audio_pts, &decoder->audio_stream, pack_len);
}

static int pes_private_stream_handler(ps_decoder_t *decoder, uint32_t *pack_len)
{
    LOGI("private pes");
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

static int check_header(uint32_t header)
{
    if (header != PACK_HEADER
     && header != SYSTEM_HEADER
     && header != PROGRAM_STREM_MAP
     && header != PES_VIDEO_STREAM
     && header != PES_AUDIO_STREAM
     && header != PES_PRIVATE_STREAM) {
         return -1;
     }
     return 0;
}

#define HEADER_LEN (4)
int ps_decode(ps_decoder_t *decoder, uint8_t *ps_buf, int ps_len, ps_pkt_t *ps_pkt)
{
    if (!decoder || !ps_buf || !ps_pkt || !ps_len)
        return -1;

    decoder->ps_buf = ps_buf;
    while(decoder->ps_buf < ps_buf + ps_len) {
        uint32_t header = ntohl(*(uint32_t *)decoder->ps_buf);
        //LOGI("header:0x%x", header);
        if (check_header(header) < 0) {
            LOGE("check header error, 0x%x offset: %ld", header, decoder->ps_buf - ps_buf);
            return -1;
        }
        decoder->ps_buf += HEADER_LEN;
        int idx = hash(header);
        if (handlers[idx]) {
            uint32_t pack_len = 0;
            //LOGI("pos:%ld", decoder->ps_buf - ps_buf);
            handlers[idx](decoder, &pack_len);
            //LOGI("pack_len:%d", pack_len);
            decoder->ps_buf += pack_len;
        } else {
            LOGE("check header error: 0x%x", header);
            return -1;
        }
    }

    if (!strcmp(decoder->conf->dump_video_file, "on") 
        && decoder->video_stream.es
        && decoder->vfp) {
        //dbg_dump_buf(decoder->video_stream.es, 64);
        if (fwrite(decoder->video_stream.es, decoder->video_stream.es_len, 1, decoder->vfp) != 1) {
            LOGE("write video to file error");
        }
    }
    decoder->video_stream.es_len = 0;

    return 0;
}