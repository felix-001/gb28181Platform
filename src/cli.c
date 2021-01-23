#include "public.h"
#include "cli.h"
#include "sip.h"

static int cmd_proc(char *buf, sip_ctx_t *ctx)
{
    if (!strncmp(buf, "invite", strlen("invite"))) {
        start_stream(ctx);
    } else if (!strncmp(buf, "exit", strlen("exit"))) {
        exit(0);
    } else if (!strncmp(buf, "help", strlen("help"))) {
        printf("support commands: invite exit\n");
    }

    return 0;
}

static void *cli_thread(void *arg)
{
    sip_ctx_t *ctx = (sip_ctx_t *)arg;
    LOGI("enter cli thread");
    for (;;) {
        char buf[1024] = {0};
        fgets(buf, sizeof(buf), stdin);
        if (buf[0] == '\n') {
            printf("enter command: ");
        } else {
            cmd_proc(buf, ctx);
        }
    }
    return NULL;
}

int start_cli(sip_ctx_t *ctx)
{
    pthread_t tid = 0;
    pthread_create(&tid, NULL, cli_thread, ctx);
    return 0;
}