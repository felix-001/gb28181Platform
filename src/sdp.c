#include "public.h"
#include "conf.h"
#include "osipparser2/sdp_message.h" 
#include "osipparser2/osip_port.h" 

static int sdp_add_gb_f_y(char **sdp_str)
{
    char *f = "f=\r\n";
    char *y = "y=0100000001\r\n";

    *sdp_str = realloc(*sdp_str, strlen(*sdp_str)+strlen(f)+strlen(y));
    if (!*sdp_str) {
        LOGE("realloc error");
        return -1;
    }
    strcat(*sdp_str, y);
    strcat(*sdp_str, f);
    return 0;
}

int gen_sdp(conf_t *conf, char **sdp_str)
{
    sdp_message_t *sdp = NULL;
    
    int ret = sdp_message_init(&sdp);
    if (ret < 0) {
        LOGE("sdp message init error");
        return ret;
    }
    sdp_message_v_version_set(sdp, osip_strdup("0"));
    sdp_message_o_origin_set(sdp, osip_strdup(conf->srv_gbid), osip_strdup("0"), osip_strdup("0"), osip_strdup("IN"), osip_strdup("IP4"), osip_strdup(conf->srv_ip));
    sdp_message_s_name_set(sdp, osip_strdup("Play"));
    sdp_message_c_connection_add(sdp, -1, osip_strdup("IN"), osip_strdup("IP4"), osip_strdup(conf->srv_ip), NULL, NULL);
    sdp_message_t_time_descr_add(sdp, osip_strdup("0"), osip_strdup("0"));
    sdp_message_m_media_add(sdp, osip_strdup("video"), osip_strdup(conf->rtp_port), NULL, osip_strdup("TCP/RTP/AVP"));
    sdp_message_m_payload_add(sdp, 0, osip_strdup("96"));
    sdp_message_a_attribute_add(sdp, 0, osip_strdup("rtpmap"), osip_strdup("96 PS/90000"));
    sdp_message_a_attribute_add(sdp, -1, osip_strdup("setup"), osip_strdup("passive"));
    sdp_message_a_attribute_add(sdp, -1, osip_strdup("recvonly"), NULL);
    sdp_message_a_attribute_add(sdp, -1, osip_strdup("connecton"), osip_strdup("new"));
    sdp_message_to_str(sdp, sdp_str);
    if (!*sdp_str) {
        LOGE("gen sdp error");
        return -1;
    }
    sdp_message_free(sdp);
    ret = sdp_add_gb_f_y(sdp_str);
    if (ret < 0)
        return ret;
    return 0;
}