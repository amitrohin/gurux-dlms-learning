// vi: ts=4:sts=4:sw=4:et:nu:syn=off:noai:nosi

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

#include "foo.h"
#include "mbuf.h"
#include "hdlc.h"


#define MAPENTRY(s)     [s] = #s
#define CASENTRY(s)     case s: return #s;

static char const *hdlc_type_strmap[] = {
    MAPENTRY(HDLC_I),
    MAPENTRY(HDLC_S),
    MAPENTRY(HDLC_U),
};
static inline char const *HDLC_strtype(enum hdlc_type type) {
    assert(type >= 0);
    assert(type < sizeof hdlc_type_strmap/sizeof hdlc_type_strmap[0]);
    return hdlc_type_strmap[type];
}

static char const *HDLC_s_strcmd(enum hdlc_s_cmd cmd) {
    switch (cmd) {
        CASENTRY(HDLC_S_RR)
        CASENTRY(HDLC_S_RNR)
        CASENTRY(HDLC_S_REJ)
        CASENTRY(HDLC_S_SREJ)
    }
    elog("Unknown HDLC S-frame command: %d", cmd);
    return NULL;
}
static char const *HDLC_u_strcmd(enum hdlc_u_cmd cmd) {
    switch (cmd) {
        CASENTRY(HDLC_U_UI)
        CASENTRY(HDLC_U_SNRM)
        CASENTRY(HDLC_U_DISC)
        CASENTRY(HDLC_U_UA)
        CASENTRY(HDLC_U_FRMR)
        CASENTRY(HDLC_U_DM)
        CASENTRY(HDLC_U_SIM)
        // CASENTRY(HDLC_U_RIM)
        // CASENTRY(HDLC_U_SARM)
        CASENTRY(HDLC_U_UP)
        CASENTRY(HDLC_U_SABM)
        // CASENTRY(HDLC_U_RD)
        CASENTRY(HDLC_U_SARME)
        CASENTRY(HDLC_U_SABME)
        CASENTRY(HDLC_U_RSET)
        CASENTRY(HDLC_U_XID)
        CASENTRY(HDLC_U_SNRME)
        CASENTRY(HDLC_U_TEST)
        CASENTRY(HDLC_U_UIH)
    }
    elog("Unknown HDLC U-frame command: %d", cmd);
    return NULL;
}


extern inline void HDLC_init(HDLC *hdlc);
extern inline void HDLC_fini(HDLC *hdlc);
extern inline HDLC *HDLC_attach(HDLC *hdlc);
extern inline void HDLC_detach(HDLC *hdlc);

HDLC *HDLC_create() {
    HDLC *hdlc;

    hdlc = malloc(sizeof(HDLC));
    if (!hdlc)
        elog2(errno, "malloc(%zu)", sizeof(HDLC));
    else
        HDLC_init(hdlc);
    return hdlc;
}

static char const *hdlc_ctx_state_strmap[] = {
    MAPENTRY(HDLC_Initial),
    MAPENTRY(HDLC_FrameOpenFlag),
    MAPENTRY(HDLC_FrameFormat_1),
    MAPENTRY(HDLC_FrameFormat),
    MAPENTRY(HDLC_DA_1),
    MAPENTRY(HDLC_DA_2),
    MAPENTRY(HDLC_DA_3),
    MAPENTRY(HDLC_DA),
    MAPENTRY(HDLC_SA_1),
    MAPENTRY(HDLC_SA_2),
    MAPENTRY(HDLC_SA_3),
    MAPENTRY(HDLC_SA),
    MAPENTRY(HDLC_Control),

    MAPENTRY(HDLC_Complete),
};
char const *HDLC_CTX_strstate(enum hdlc_ctx_state state) {
    assert(state >= 0);
    assert(state < sizeof hdlc_ctx_state_strmap /
                        sizeof hdlc_ctx_state_strmap[0]);
    return hdlc_ctx_state_strmap[state];
}

int HDLC_CTX_init(HDLC_CTX *ctx, struct mbuf *pkt, HDLC *hdlc) {
    ctx->nref = 1;
    ctx->state = HDLC_Initial;
    ctx->pkt = NULL;
    ctx->hdlc = NULL;
    if (!pkt) {
        ctx->pkt = mbuf_create(128);
        if (!ctx->pkt)
            egoto(E0, "mbuf_create() failed.");
        if (mbuf_reserve(ctx->pkt, 128))
            egoto(E0, "mbuf_reserve() failed.");
    } else
        ctx->pkt = mbuf_attach(pkt);
    if (!hdlc) {
        ctx->hdlc = HDLC_create();
        if (!ctx->hdlc)
            egoto(E0, "HDLC_create() failed.");
    } else
        ctx->hdlc = HDLC_attach(hdlc);
    return 0;

E0: if (ctx->hdlc)
        HDLC_detach(ctx->hdlc);
    if (ctx->pkt)
        mbuf_detach(ctx->pkt);
    return -1;
}

void HDLC_CTX_fini(struct hdlc_ctx *ctx) {
    assert(ctx->nref == 1);
    if (ctx->hdlc)
        HDLC_detach(ctx->hdlc);
    if (ctx->pkt)
        mbuf_detach(ctx->pkt);
}

extern inline HDLC_CTX *HDLC_CTX_attach(HDLC_CTX *ctx);
extern inline void HDLC_CTX_detach(HDLC_CTX *ctx);

HDLC_CTX *HDLC_CTX_create(struct mbuf *pkt, HDLC *hdlc) {
    HDLC_CTX *ctx;

    ctx = malloc(sizeof(HDLC_CTX));
    if (!ctx)
        egoto2(E0, errno, "malloc(%zu)", sizeof(HDLC_CTX));
    if (HDLC_CTX_init(ctx, pkt, hdlc))
        egoto(E0, "HDLC_CTX_init() failed.");
    return ctx;

E0: if (ctx)
        HDLC_CTX_detach(ctx);
    return NULL;
}

int HDLC_CTX_update(struct hdlc_ctx *ctx, FILE *fp) {
    while (1) {
        enum hdlc_ctx_state state = ctx->state;
        int c = getc_unlocked(fp);
        log("0x%02x", c);
        if (c < 0) {
            if (ferror(fp)) {
                if (errno == EAGAIN || errno == EINTR) {
                    clearerr(fp);
                    goto E0;
                }
                egoto2(E0, errno, "getc_unlocked()");
            } else if (feof(fp)) {
                if (ctx->state == HDLC_Initial)
                    break;
                assert(ctx->state != HDLC_Complete);
                egoto2_set(E0, EBADMSG, "Unexpected end of file: not enough "
                    "data to complete the HDLC frame.");
            }
        }
        c &= 0xff;
        if (mbuf_addc(ctx->pkt, c) < 0)
            egoto2(E0, errno, "mbuf_addc()");

        if (ctx->state == HDLC_Initial) {
            if (c != 0x07e)
                egoto2_set(E0, EINVAL, "Ожидается начало hdlc-кадра - 0x7e, "
                    "но прочитано - 0x%02x.", c);
            ctx->state = HDLC_FrameOpenFlag;
        } else if (ctx->state == HDLC_FrameOpenFlag) {
            if ((c & 0xf0) != 0xa0)
                egoto2_set(E0, EINVAL, "Ожидается hdlc frame format 3: "
                    "(frame[1] & 0xf0) == 0xa0, но 0x%02x != 0xa0", c & 0xf0);
            ctx->hdlc->fragment_bit = ((c >> 3) & 1);
            ctx->hdlc->length = ((c & 0x07) << 8);
            ctx->state = HDLC_FrameFormat_1;
            log("  [hdlc] frame type 3");
            log("  [hdlc] S (fragment_bit): %d", ctx->hdlc->fragment_bit);
        } else if (ctx->state == HDLC_FrameFormat_1) {
            ctx->hdlc->length |= c;
            ctx->state = HDLC_FrameFormat;
            log("  [hdlc] frame length: %d", ctx->hdlc->length);
        } else if (ctx->state == HDLC_FrameFormat) {
            ctx->hdlc->DA_upper = (c >> 1);
            ctx->hdlc->DA_lower = -1;
            if (c & 1) {
                /* однобайтовый формат DA */
                ctx->state = HDLC_DA;
                log("  [hdlc] Upper DA: %d", ctx->hdlc->DA_upper);
                log("  [hdlc] Lower DA: none");
            } else {
                /* многобайтовый DA */
                ctx->state = HDLC_DA_1;
            }
        } else if (ctx->state == HDLC_DA_1) {
            if (c & 1) {
                /* двубайтовый формат upper+lower */
                ctx->hdlc->DA_lower = (c >> 1);
                ctx->state = HDLC_DA;
                log("  [hdlc] Upper DA: %d", ctx->hdlc->DA_upper);
                log("  [hdlc] Lower DA: %d", ctx->hdlc->DA_lower);
            } else {
                ctx->hdlc->DA_upper = ((ctx->hdlc->DA_upper << 7) | (c >> 1));
                ctx->state = HDLC_DA_2;
                log("  [hdlc] Upper DA: %d", ctx->hdlc->DA_upper);
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
            log("  [hdlc] Lower DA: %d", ctx->hdlc->DA_lower);
        } else if (ctx->state == HDLC_DA) {
            ctx->hdlc->SA_upper = (c >> 1);
            ctx->hdlc->SA_lower = -1;
            if (c & 1) {
                /* однобайтовый формат SA */
                ctx->state = HDLC_SA;
                log("  [hdlc] Upper SA: %d", ctx->hdlc->SA_upper);
                log("  [hdlc] Lower SA: none");
            } else {
                /* многобайтовый SA */
                ctx->state = HDLC_SA_1;
            }
        } else if (ctx->state == HDLC_SA_1) {
            if (c & 1) {
                /* двубайтовый формат upper+lower */
                ctx->hdlc->SA_lower = (c >> 1);
                ctx->state = HDLC_SA;
                log("  [hdlc] Upper SA: %d", ctx->hdlc->SA_upper);
                log("  [hdlc] Lower SA: %d", ctx->hdlc->SA_lower);
            } else {
                ctx->hdlc->SA_upper = ((ctx->hdlc->SA_upper << 7) | (c >> 1));
                ctx->state = HDLC_SA_2;
                log("  [hdlc] Upper SA: %d", ctx->hdlc->SA_upper);
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
            log("  [hdlc] Lower SA: %d", ctx->hdlc->SA_lower);
        } else if (ctx->state == HDLC_SA) {
            int pf = (c >> 4) & 1U;
            switch (c & 3U) {
                case 1:
                    ctx->hdlc->control.s.type = HDLC_S;
                    ctx->hdlc->control.s.cmd = c & 0x0fU;
                    ctx->hdlc->control.s.nr = (c >> 5) & 0x07U;
                    ctx->hdlc->control.s.pf = pf;
                    log("  [hdlc] control -> {type = %s, cmd = %s, N(R) = %"
                        PRIi16 ", P/F = %" PRIu8 "}"
                        , HDLC_strtype(ctx->hdlc->control.s.type)
                        , HDLC_s_strcmd(ctx->hdlc->control.s.cmd)
                        , ctx->hdlc->control.s.nr
                        , ctx->hdlc->control.s.pf
                    );
                    break;
                case 3:
                    ctx->hdlc->control.u.type = HDLC_U;
                    ctx->hdlc->control.u.cmd = c & 0xefU;
                    ctx->hdlc->control.u.pf = pf;
                    break;
                default:
                    ctx->hdlc->control.i.type = HDLC_I;
                    ctx->hdlc->control.i.ns = (c >> 1) & 0x07U;
                    ctx->hdlc->control.i.nr = (c >> 5) & 0x07U;
                    ctx->hdlc->control.i.pf = pf;
            }
            ctx->state = HDLC_Control;
        } else {
            /* ... */
        }
        log("  >>> %s -> %s", HDLC_CTX_strstate(state), HDLC_CTX_strstate(ctx->state));
    }

    /* [9.3]
     *
     * В поле "Адрес источника" не должны использоваться адреса No-station
     * и Broadcasting. Кадры и такими адресами считаются неправильными;
     *
     * Только кадры, передаваемые первичной станцией, могут содержать в поле
     * "Адрес назначения" адреса No-station и Broadcasting;
     *
     * В информационных кадрах не должно быть широковещательных адресов;
     *
     * В кадрах с адресом назначения No-station и Broadcasting бит P/F
     * должен быть сброшен в О, то есть данный кадр должен быть командой;
     *
     * Адрес Calling используется для организации инициативных сообщений о
     * событиях от вторичной станции к первичной.
     */
    /* ... */

    assert(ctx->state == HDLC_Initial || ctx->state == HDLC_Complete);
    return ctx->hdlc->length;

E0: return -1;
}
