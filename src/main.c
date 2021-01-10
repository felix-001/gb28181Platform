#include "public.h"
#include "sip.h"
#include "conf.h"

#define CONF_FILE "/usr/local/etc/gbSrv.conf"


static int conf_handler(void* user, const char* section, const char* name, const char* value)
{
    #define same(key) !strcmp(name, key)
    #define conf_get(key) if (!conf->key) conf->key = same(#key) ? strdup(value) : NULL;

    conf_t *conf = (conf_t *)user;

    conf_get(usr_gbid);
    conf_get(srv_gbid);
    conf_get(rtp_port);
    conf_get(sip_port);
    conf_get(passwd);
    conf_get(expiry);
    conf_get(timeout);
    conf_get(ua);
    return 0;
}

int main(int argc, char *argv[])
{
    conf_t conf;

    memset(&conf, 0, sizeof(conf));
    int ret = ini_parse(CONF_FILE, conf_handler, &conf);
    if (ret < 0) {
        LOGE("load conf %s error %d %s", CONF_FILE, ret, strerror(errno));
        exit(0);
    }
    LOGI("srv_gbid : %s", conf.srv_gbid);
    LOGI("usr_gbid : %s", conf.usr_gbid);
    LOGI("ua : %s", conf.ua);
    LOGI("rtp_port : %s", conf.rtp_port);
    LOGI("sip_port : %s", conf.sip_port);
    LOGI("passwd : %s", conf.passwd);
    LOGI("expiry : %s", conf.expiry);
    LOGI("timeout : %s", conf.timeout);
    ret = sip_init(&conf);
    if (ret < 0) {
        exit(0);
    }
    for(;;) {
        sleep(3);
    }
    return 0;
}
