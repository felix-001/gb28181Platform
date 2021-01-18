#ifndef SIP_H
#define SIP_H

typedef struct _sip_ctx_t sip_ctx_t;

extern sip_ctx_t * new_sip_context(conf_t *conf);
extern int sip_run(sip_ctx_t *ctx);

#endif