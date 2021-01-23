#include "public.h"
#include "sip.h"
#include "conf.h"
#include "cli.h"

#define CONF_FILE "/usr/local/etc/gbSrv.conf"

static int conf_handler(void* user, const char* section, const char* name, const char* value)
{
    #define same(key) !strcmp(name, key)
    #define conf_get(key) if (!conf->key) conf->key = same(#key) ? strdup(value) : NULL;

    conf_t *conf = (conf_t *)user;

    conf_get(srv_gbid);
    conf_get(rtp_port);
    conf_get(srv_sip_port);
    conf_get(passwd);
    conf_get(expiry);
    conf_get(timeout);
    conf_get(ua);
    conf_get(dbg);
    conf_get(auto_invite);
    conf_get(ssrc);
    conf_get(dump_video_file);
    conf_get(enable_cli);
    return 0;
}

const char* get_ip(void)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char *host = NULL;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return NULL;
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        family = ifa->ifa_addr->sa_family;
        if (!strcmp(ifa->ifa_name, "lo"))
            continue;
        if (!strcmp(ifa->ifa_name, "lo0"))
            continue;
        if (family == AF_INET) {
            if ((host = (char*)malloc(NI_MAXHOST)) == NULL)
                return NULL;
            s = getnameinfo(ifa->ifa_addr,
                    (family == AF_INET) ? sizeof(struct sockaddr_in) :
                    sizeof(struct sockaddr_in6),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                return NULL;
            }
            freeifaddrs(ifaddr);
            return host;
        }
    }
    return NULL;
}

void dump_conf(conf_t *conf)
{
    LOGI("srv_gbid\t: %s", conf->srv_gbid);
    LOGI("ua\t\t: %s", conf->ua);
    LOGI("rtp_port\t: %s", conf->rtp_port);
    LOGI("srv_sip_port\t: %s", conf->srv_sip_port);
    LOGI("passwd\t: %s", conf->passwd);
    LOGI("expiry\t: %s", conf->expiry);
    LOGI("timeout\t: %s", conf->timeout);
    LOGI("srv_ip\t: %s", conf->srv_ip);
    LOGI("dbg\t\t: %s", conf->dbg);
    LOGI("auto_invite\t: %s", conf->auto_invite);
    LOGI("ssrc\t\t: %s", conf->ssrc);
    LOGI("dump_video\t: %s", conf->dump_video_file);
    LOGI("enable_cli\t: %s", conf->enable_cli);
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
    const char *ip = get_ip();
    if (!ip) {
        LOGE("get ip error");
        exit(0);
    }
    conf.srv_ip = strdup(ip);
    dump_conf(&conf);
    sip_ctx_t *ctx = new_sip_context(&conf);
    if (!ctx) exit(0);
    sip_run(ctx);
    if (!strcmp(conf.enable_cli, "on")) {
        start_cli(ctx);
    }
    for(;;) {
        sleep(3);
    }
    return 0;
}
