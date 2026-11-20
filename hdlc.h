// vi: ts=4:sts=4:sw=4:et:nu:syn=off:noai:nosi
#ifndef __SPODES_HDLC_H__
#define __SPODES_HDLC_H__

#include "foo.h"
#include "mbuf.h"

typedef struct hdlc {
    int nref;
    int fragment_bit;
    int length;
    int DA_upper;
    int DA_lower;
    int SA_upper;
    int SA_lower;
} HDLC;

#define HDLC_INITIALIZER \
    { \
        .nref = 1, \
        .fragment_but = 0, \
        .length = 0, \
        .DA_upper = -1, \
        .DA_lower = -1, \
        .SA_upper = -1, \
        .SA_lower = -1, \
    }

inline
void        HDLC_init(HDLC *hdlc)
{
    hdlc->nref = 1;
    hdlc->fragment_bit = 0;
    hdlc->length = 0;
    hdlc->DA_upper = -1;
    hdlc->DA_lower = -1;
    hdlc->SA_upper = -1;
    hdlc->SA_lower = -1;
}

inline
void        HDLC_fini(HDLC *hdlc)
{
    assert(hdlc->nref == 1);
}

inline
HDLC *      HDLC_attach(HDLC *hdlc)
{
    hdlc->nref++;
    return hdlc;
}

inline
void        HDLC_detach(HDLC *hdlc)
{
    assert(hdlc->nref > 0);
    if (hdlc->nref == 1) {
        HDLC_fini(hdlc);
        free(hdlc);
    } else
        hdlc->nref--;
}

HDLC *      HDLC_create();


enum hdlc_ctx_state {
    HDLC_Initial,           /* состояние перед чтением данных */
    HDLC_FrameOpenFlag,     /* прочитан маркер начала кадра 0x7e */
    HDLC_FrameFormat_1,     /* разобран первый байт */
    HDLC_FrameFormat,       /* прочитан второй байт (все поле) */
    HDLC_DA_1,
    HDLC_DA_2,
    HDLC_DA_3,
    HDLC_DA,
    HDLC_SA_1,
    HDLC_SA_2,
    HDLC_SA_3,
    HDLC_SA,
    HDLC_Complete,          /* пакет полностью прочитан */
};
char const *hdlc_ctx_strstate(enum hdlc_ctx_state state);

typedef struct hdlc_ctx {
    int nref;
    enum hdlc_ctx_state state;
    struct mbuf *pkt;
    HDLC *hdlc;
} HDLC_CTX;

int         HDLC_CTX_init(HDLC_CTX *ctx, struct mbuf *mb, HDLC *hdlc);
void        HDLC_CTX_fini(struct hdlc_ctx *ctx);

inline
HDLC_CTX *  HDLC_CTX_attach(HDLC_CTX *ctx)
{
    assert(ctx->nref > 0);
    ctx->nref++;
    return ctx;
}

inline
void        HDLC_CTX_detach(HDLC_CTX *ctx)
{
    assert(ctx->nref > 0);
    if (ctx->nref == 1) {
        HDLC_CTX_fini(ctx);
        free(ctx);
    } else 
        ctx->nref--;
}

HDLC_CTX *  HDLC_CTX_create(struct mbuf *pkt, HDLC *hdlc);
int         HDLC_CTX_update(struct hdlc_ctx *ctx, FILE *fp);

#endif
