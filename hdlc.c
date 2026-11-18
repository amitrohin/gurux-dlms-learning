// vi: ts=4:sts=4:sw=4:et:nu:syn=off:noai:nosi

#include "foo.h"
#include "mbuf.h"

typedef struct hdlc {
    int fragment_bit;
    int length;
    int DA_upper;
    int DA_lower;
    int SA_upper;
    int SA_lower;
} HDLC;
void HDLC_init(HDLC *);
void HDLC_fini(HDLC *);
HDLC *HDLC_create();
void HDLC_destroy(HDLC *);

typedef struct hdlc_ctx {
    enum {
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
    } state;
    struct mbuf pkt;
    HDLC hdlc;
} HDLC_CTX;

int hdlc_ctx_init(struct hdlc_ctx *ctx) {
    ctx->state = HDLC_Initial;
    mbuf_init(&ctx->pkt);
    if (mbuf_reserve(&ctx->pkt, 256))
        eret(-1, "mbuf_reserve()");
    ctx->hdlc = NULL;
    return 0;
}
void hdlc_ctx_fini(struct hdlc_ctx *ctx) {
    mbuf_fini(&ctx->mbuf);
    hdlc_destroy(ctx->hdlc);
}

int HDLC_CTX_Update(struct hdlc_ctx *ctx, FILE *fp) {
    while (1) {
        int c = fgetc_unlocked(fp);
        if (c < 0) {
            if (ferror(fp)) {
                if (errno == EAGAIN || errno == EINTR) {
                    clearerr(fp);
                    goto E1;
                }
                egoto2(E1, errno, "fgetc_unlocked()");
            } else if (feof(fp)) {
                if (ctx->status == HDLC_Initial)
                    break;
                assert(ctx->status != HDLC_Complete);
                egoto2_set(E1, ENODATA, "Unexpected end of file: not enough "
                    "data to complete the HDLC frame.");
            }
        }
        c &= 0xff;
        if (mbuf_addc(&ctx->mbuf, c) < 0)
            egoto2(E0, errno, "mbuf_addc()");
        if (!ctx->hdlc) {
            ctx->hdlc = hdlc_create(256);
            if (!ctx->hdlc)
                egoto2(E0, errno, "hdlc_create()");
        }

        if (ctx->state == HDLC_Initial) {
            if (c != 0x07e)
                egoto_set(E0, EINVAL, "Ожидается начало hdlc-кадра - 0x7e, "
                    "но прочитано - 0x%02x.", c);
            ctx->state = HDLC_FrameOpenFlag;
        } else if (ctx->state == HDLC_FrameOpenFlag) {
            if ((c & 0xf0) != 0xa0)
                egoto_set(E0, EINVAL, "Ожидается hdlc frame format 3: "
                    "(frame[1] & 0xf0) == 0xa0, но 0x%02x != 0xa0", c & 0xf0);
            ctx->hdlc->fragment_bit = ((c >> 3) & 1);
            ctx->hdlc->length = ((c & 0x07) << 8);
            ctx->state = HDLC_FrameFormat_1;
        } else if (ctx->state == HDLC_FrameFormat_1) {
            ctx->hdlc->length |= c;
            ctx->state = HDLC_FrameFormat;
        } else if (ctx->state == HDLC_FrameFormat) {
            ctx->hdlc->DA_upper = (c >> 1);
            ctx->hdlc->DA_lower = -1;
            if (c & 1) {
                /* однобайтовый формат DA */
                ctx->state = HDLC_DA;
            } else {
                /* многобайтовый DA */
                ctx->state = HDLC_DA_1;
            }
        } else if (ctx->state == HDLC_DA_1) {
            if (c & 1) {
                /* двубайтовый формат upper+lower */
                ctx->hdlc->DA_lower = (c >> 1);
                ctx->state = HDLC_DA;
            } else {
                ctx->hdlc->DA_upper = ((ctx->hdlc->DA_upper << 7) | (c >> 1));
                ctx->state = HDLC_DA_2;
            }
        } else if (ctx->state == HDLC_DA_2) {
            if (c & 1)
                egoto2_set(E0, EINVAL, "hdlc адрес не может заканчиваться "
                    "на третьем байте (т.е. младший бит в третьем байте не "
                    "может быть 1).");
            ctx->hdlc->DA_lower = (c >> 1);
            ctx->state = HDLC_DA_3;
        } else if (ctx->state == HDLC_DA_3) {
            if ((c & 1) == 0)
                egoto2_set(E0, EINVAL, "hdlc адрес не может быть длиннее "
                    "четырех байтов (т.е. младший бит в четвертом байте не "
                    "должен быть 0).");
            ctx->hdlc->DA_lower = ((ctx->hdlc->DA_lower << 7) | (c >> 1));
            ctx->state = HDLC_DA;
        } else if (ctx->state == HDLC_DA) {
            ctx->hdlc->SA_upper = (c >> 1);
            ctx->hdlc->SA_lower = -1;
            if (c & 1) {
                /* однобайтовый формат SA */
                ctx->state = HDLC_SA;
            } else {
                /* многобайтовый SA */
                ctx->state = HDLC_SA_1;
            }
        } else if (ctx->state == HDLC_SA_1) {
            if (c & 1) {
                /* двубайтовый формат upper+lower */
                ctx->hdlc->SA_lower = (c >> 1);
                ctx->state = HDLC_SA;
            } else {
                ctx->hdlc->SA_upper = ((ctx->hdlc->SA_upper << 7) | (c >> 1));
                ctx->state = HDLC_SA_2;
            }
        } else if (ctx->state == HDLC_SA_2) {
            if (c & 1)
                egoto2_set(E0, EINVAL, "hdlc адрес не может заканчиваться "
                    "на третьем байте (т.е. младший бит в третьем байте не "
                    "может быть 1).");
            ctx->hdlc->SA_lower = (c >> 1);
            ctx->state = HDLC_SA_3;
        } else if (ctx->state == HDLC_SA_3) {
            if ((c & 1) == 0)
                egoto2_set(E0, EINVAL, "hdlc адрес не может быть длиннее "
                    "четырех байтов (т.е. младший бит в четвертом байте не "
                    "должен быть 0).");
            ctx->hdlc->SA_lower = ((ctx->hdlc->SA_lower << 7) | (c >> 1));
            ctx->state = HDLC_SA;
        }

    }
    assert(hdlc_ctx->state == HDLC_CTX_STATE_sInitial ||
        hdlc_ctx->state == HDLC_CTX_sComplete);
    return hdlc_ctx->hdlc ? hdlc_ctx->hdlc->length : 0;

E0: return -1;
}
