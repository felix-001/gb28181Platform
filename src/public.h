#ifndef PUBLIC_H
#define PUBLIC_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include "log.h"

typedef struct {
    char *srv_gbid;
    char *ua;
    char *rtp_port;
    char *srv_sip_port;
    char *passwd;
    char *expiry;
    char *timeout;
    char *srv_ip;
    char *dbg;
    char *dump_video_file;
    char *auto_invite;
} conf_t;
#define arrsz(arr) (sizeof(arr)/sizeof(arr[0]))

#endif