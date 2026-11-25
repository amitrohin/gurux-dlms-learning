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

static char const *HDLC_S_strcmd(enum hdlc_s_cmd cmd) {
    switch (cmd) {
        CASENTRY(HDLC_S_RR)
        CASENTRY(HDLC_S_RNR)
        CASENTRY(HDLC_S_REJ)
        CASENTRY(HDLC_S_SREJ)
    }
    elog("Unknown HDLC S-frame command: %d", cmd);
    return NULL;
}
static char const *HDLC_U_strcmd(enum hdlc_u_cmd cmd) {
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


void HDLC_init(HDLC *hdlc) {
    hdlc->nref = 1;
    mbuf_init(&hdlc->frame);
    hdlc->length = 0;
    hdlc->DA_upper = -1;
    hdlc->DA_lower = -1;
    hdlc->SA_upper = -1;
    hdlc->SA_lower = -1;
    hdlc->fragment_bit = 0;
    hdlc->control.type = -1;
    hdlc->payload_offset = -1;
    hdlc->payload_len = -1;
    STAILQ_NEXT(hdlc, ent) = NULL;
    if (mbuf_resize(&hdlc->frame, 128))
        elog("WARNING: Ошибка резервирования 128 байтов под hdlc фрейм.");
}


extern inline void HDLC_fini(HDLC *hdlc);
extern inline HDLC *HDLC_attach(HDLC *hdlc);
extern inline void HDLC_detach(HDLC *hdlc);
extern inline int HDLC_shared(HDLC *hdlc);


HDLC *HDLC_create() {
    HDLC *hdlc;

    hdlc = malloc(sizeof(HDLC));
    if (hdlc)
        HDLC_init(hdlc);
    else
        elog2(errno, "malloc(%zu)", sizeof(HDLC));
    return hdlc;
}

void HDLC_reset(HDLC *hdlc) {
    assert(hdlc->nref > 0);
    if (hdlc->nref != 1)
        panic("Очистить буфер можно только если на него никто кроме нас не ссылается.");
    assert(hdlc->frame.nref == 1);
    mbuf_reset(&hdlc->frame);
    hdlc->length = 0;
    hdlc->DA_upper = -1;
    hdlc->DA_lower = -1;
    hdlc->SA_upper = -1;
    hdlc->SA_lower = -1;
    hdlc->fragment_bit = 0;
    hdlc->control.type = -1;
    hdlc->payload_offset = -1;
    hdlc->payload_len = -1;
    STAILQ_NEXT(hdlc, ent) = NULL;
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
    MAPENTRY(HDLC_HCS_1),
    MAPENTRY(HDLC_HCS),
    MAPENTRY(HDLC_Payload_),
    MAPENTRY(HDLC_Payload),
    MAPENTRY(HDLC_FCS_1),
    MAPENTRY(HDLC_FCS),
    MAPENTRY(HDLC_Complete),
};
char const *HDLC_CTX_strstate(enum hdlc_ctx_state state) {
    assert(state >= 0);
    assert(state < sizeof hdlc_ctx_state_strmap /
                        sizeof hdlc_ctx_state_strmap[0]);
    return hdlc_ctx_state_strmap[state];
}

void HDLC_CTX_init(HDLC_CTX *ctx) {
    ctx->nref = 1;
    ctx->state = HDLC_Initial;
    ctx->hdlc = NULL;
}

void HDLC_CTX_fini(HDLC_CTX *ctx) {
    assert(ctx->nref == 1);
    if (ctx->hdlc)
        HDLC_detach(ctx->hdlc);
}

extern inline HDLC_CTX *HDLC_CTX_attach(HDLC_CTX *ctx);
extern inline void HDLC_CTX_detach(HDLC_CTX *ctx);
extern inline int HDLC_CTX_shared(HDLC_CTX *ctx);

HDLC_CTX *HDLC_CTX_create() {
    HDLC_CTX *ctx;

    ctx = malloc(sizeof(HDLC_CTX));
    if (!ctx) {
        elog2(errno, "malloc(%zu)", sizeof(HDLC_CTX));
        HDLC_CTX_init(ctx);
    }
    return ctx;
}

void HDLC_CTX_reset(HDLC_CTX *ctx) {
    assert(ctx->nref > 0);
    if (ctx->nref != 1)
        panic("Очистить HDLC_CTX можно только если на него никто кроме нас не ссылается.");
    ctx->state = HDLC_Initial;
    if (ctx->hdlc) {
        if (HDLC_shared(ctx->hdlc)) {
            HDLC_detach(ctx->hdlc);
            ctx->hdlc = NULL;
        } else
            HDLC_reset(ctx->hdlc);
    }
}

int HDLC_CTX_update(HDLC_CTX *ctx, FILE *fp) {
    HDLC *hdlc;

    hdlc = ctx->hdlc;

    if (ctx->state == HDLC_Complete) {
        if (hdlc) {
            if (HDLC_shared(hdlc)) {
                ctx->hdlc = NULL;
                HDLC_detach(hdlc);
                hdlc = NULL;
            } else
                HDLC_reset(hdlc);
        }
        ctx->state = HDLC_Initial;
    }

    if (ctx->state == HDLC_Initial) {
        if (!hdlc) {
            hdlc = HDLC_create();
            if (!hdlc)
                egoto(E0, "HDLC_create() failed.");
            ctx->hdlc = hdlc;
        }
    }

    while (1) {
        /* ctx->state - это состояние чтения пакета, на который указывает hdlc */
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
                if (state == HDLC_Initial)
                    break;
                assert(state != HDLC_Complete);
                egoto2_set(E0, EBADMSG, "Unexpected end of file: not enough "
                    "data to complete the HDLC frame.");
            }
        }
        c &= 0xff;

        if (!mbuf_len(&hdlc->frame)) {
            if (mbuf_resize(&hdlc->frame, 128))
                elog("WARNING: Не получилось зарезервировать 128 байтов под фрейм hdlc.");
        }
        if (mbuf_addc(&hdlc->frame, c) < 0)
            egoto2(E0, errno, "mbuf_addc()");

        if (state == HDLC_Initial) {
            if (c != 0x7e)
                egoto2_set(E0, EINVAL, "Ожидается начало hdlc-кадра - 0x7e, "
                    "но прочитано - 0x%02x.", c);
            ctx->state = HDLC_FrameOpenFlag;

        } else if (state == HDLC_FrameOpenFlag) {
            if ((c & 0xf0) != 0xa0)
                egoto2_set(E0, EINVAL, "Ожидается hdlc frame format 3: "
                    "(frame[1] & 0xf0) == 0xa0, но 0x%02x != 0xa0", c & 0xf0);

            hdlc->fragment_bit = ((c >> 3) & 1);
            hdlc->length = ((c & 0x07) << 8);
            ctx->state = HDLC_FrameFormat_1;
            log("  [hdlc] Frame Type 3");
            log("  [hdlc] S (fragment_bit): %d", hdlc->fragment_bit);

        } else if (state == HDLC_FrameFormat_1) {
            hdlc->length |= c;
            ctx->state = HDLC_FrameFormat;
            log("  [hdlc] frame length: %d", hdlc->length);

        } else if (state == HDLC_FrameFormat) {
            hdlc->DA_upper = (c >> 1);
            hdlc->DA_lower = -1;
            if (c & 1) {
                /* однобайтовый формат DA */
                ctx->state = HDLC_DA;
                log("  [hdlc] Upper DA: %d", hdlc->DA_upper);
                log("  [hdlc] Lower DA: <не используется>");
            } else {
                /* многобайтовый DA */
                ctx->state = HDLC_DA_1;
            }

        } else if (state == HDLC_DA_1) {
            if (c & 1) {
                /* двубайтовый формат upper+lower */
                hdlc->DA_lower = (c >> 1);
                ctx->state = HDLC_DA;
                log("  [hdlc] Upper DA: %d", hdlc->DA_upper);
                log("  [hdlc] Lower DA: %d", hdlc->DA_lower);
            } else {
                hdlc->DA_upper = ((hdlc->DA_upper << 7) | (c >> 1));
                ctx->state = HDLC_DA_2;
                log("  [hdlc] Upper DA: %d", hdlc->DA_upper);
            }

        } else if (state == HDLC_DA_2) {
            if (c & 1)
                egoto2_set(E0, EINVAL, "hdlc адрес не может заканчиваться "
                    "на третьем байте (т.е. младший бит в третьем байте не "
                    "может быть 1).");
            hdlc->DA_lower = (c >> 1);
            ctx->state = HDLC_DA_3;

        } else if (state == HDLC_DA_3) {
            if ((c & 1) == 0)
                egoto2_set(E0, EINVAL, "hdlc адрес не может быть длиннее "
                    "четырех байтов (т.е. младший бит в четвертом байте не "
                    "должен быть 0).");
            hdlc->DA_lower = ((hdlc->DA_lower << 7) | (c >> 1));
            ctx->state = HDLC_DA;
            log("  [hdlc] Lower DA: %d", hdlc->DA_lower);

        } else if (state == HDLC_DA) {
            hdlc->SA_upper = (c >> 1);
            hdlc->SA_lower = -1;
            if (c & 1) {
                /* однобайтовый формат SA */
                ctx->state = HDLC_SA;
                log("  [hdlc] Upper SA: %d", hdlc->SA_upper);
                log("  [hdlc] Lower SA: <не используется>");
            } else {
                /* многобайтовый SA */
                ctx->state = HDLC_SA_1;
            }

        } else if (state == HDLC_SA_1) {
            if (c & 1) {
                /* двубайтовый формат upper+lower */
                hdlc->SA_lower = (c >> 1);
                ctx->state = HDLC_SA;
                log("  [hdlc] Upper SA: %d", hdlc->SA_upper);
                log("  [hdlc] Lower SA: %d", hdlc->SA_lower);
            } else {
                hdlc->SA_upper = ((hdlc->SA_upper << 7) | (c >> 1));
                ctx->state = HDLC_SA_2;
                log("  [hdlc] Upper SA: %d", hdlc->SA_upper);
            }

        } else if (state == HDLC_SA_2) {
            if (c & 1)
                egoto2_set(E0, EINVAL, "hdlc адрес не может заканчиваться "
                    "на третьем байте (т.е. младший бит в третьем байте не "
                    "может быть 1).");
            hdlc->SA_lower = (c >> 1);
            ctx->state = HDLC_SA_3;

        } else if (state == HDLC_SA_3) {
            if ((c & 1) == 0)
                egoto2_set(E0, EINVAL, "hdlc адрес не может быть длиннее "
                    "четырех байтов (т.е. младший бит в четвертом байте не "
                    "должен быть 0).");
            hdlc->SA_lower = ((hdlc->SA_lower << 7) | (c >> 1));
            ctx->state = HDLC_SA;
            log("  [hdlc] Lower SA: %d", hdlc->SA_lower);

        } else if (state == HDLC_SA) {
            int pf = (c >> 4) & 1U;
            switch (c & 3U) {
                case 1:
                    hdlc->control.s.type = HDLC_S;
                    hdlc->control.s.cmd = c & 0x0fU;
                    hdlc->control.s.nr = (c >> 5) & 0x07U;
                    hdlc->control.s.pf = pf;
                    log("  [hdlc] control -> {type = %s, cmd = %s, N(R) = %"
                        PRIi16 ", P/F = %" PRIu8 "}"
                        , HDLC_strtype(hdlc->control.s.type)
                        , HDLC_S_strcmd(hdlc->control.s.cmd)
                        , hdlc->control.s.nr
                        , hdlc->control.s.pf
                    );
                    break;
                case 3:
                    hdlc->control.u.type = HDLC_U;
                    hdlc->control.u.cmd = c & 0xefU;
                    hdlc->control.u.pf = pf;
                    log("  [hdlc] control -> {type = %s, cmd = %s, P/F = %" PRIu8 "}"
                        , HDLC_strtype(hdlc->control.u.type)
                        , HDLC_U_strcmd(hdlc->control.u.cmd)
                        , hdlc->control.u.pf
                    );
                    break;
                default:
                    hdlc->control.i.type = HDLC_I;
                    hdlc->control.i.ns = (c >> 1) & 0x07U;
                    hdlc->control.i.nr = (c >> 5) & 0x07U;
                    hdlc->control.i.pf = pf;
                    log("  [hdlc] control -> {type = %s, N(R) = %"
                        PRIi16 ", N(S) = %" PRIi16 ", P/F = %" PRIu8 "}"
                        , HDLC_strtype(hdlc->control.i.type)
                        , hdlc->control.i.nr
                        , hdlc->control.i.ns
                        , hdlc->control.i.pf
                    );
            }
            ctx->state = HDLC_Control;

        } else if (state == HDLC_Control) {
            ctx->state = HDLC_HCS_1;

        } else if (state == HDLC_HCS_1) {
            hdlc->payload_offset = mbuf_len(&hdlc->frame);
            uint16_t cksum_orig =
                (uint8_t)hdlc->frame.buf[hdlc->frame.len - 2] + 
                (uint8_t)hdlc->frame.buf[hdlc->frame.len - 1] * 256U;
            uint16_t cksum_calc = crc16((uint8_t *)(hdlc->frame.buf + 1),
                hdlc->payload_offset - 2 /* hcs */ - 1 /* open flag 0x7e */);
            if (cksum_orig != cksum_calc)
                egoto2_set(E0, EBADMSG, "Bad check sum (hdlc hcs)");
            log("  [hdlc] hcs/fcs: cksum is ok");
            
            /* [!] Длина в пакете не считает первый и последний 0x7e.
             * [!] Пакет может не содержать payload-а. В этом случае поле hcs не используется.
             */
            hdlc->payload_len = hdlc->length - (hdlc->payload_offset - 1);
            log("  [hdlc] payload_offset = %hd, payload_len = %hd", hdlc->payload_offset, hdlc->payload_len);

            if (!hdlc->payload_len)
                ;
            else if (hdlc->payload_len > 2)
                hdlc->payload_len -= 2;
            else
                egoto2_set(E0, EBADMSG, "Длина полезной нагрузки не может "
                    "быть меньше 2 (какая-то ошибка кодирования) и не должна "
                    "быть равной 2 (пустое информационное поле, где есть обе "
                    "конрольные суммы подряд - hcs и fcs).");
            ctx->state = hdlc->payload_len ? HDLC_HCS : HDLC_FCS;

        } else if (state == HDLC_HCS) {
            ctx->state = hdlc->payload_len == 1 ? HDLC_Payload : HDLC_Payload_;

        } else if (state == HDLC_Payload_) {
            assert(hdlc->payload_len);
            assert(mbuf_len(&hdlc->frame) <= 1 + hdlc->length - 2);
            if (mbuf_len(&hdlc->frame) == 1 + hdlc->length - 2)
                ctx->state = HDLC_Payload;

        } else if (state == HDLC_Payload) {
            ctx->state = HDLC_FCS_1;

        } else if (state == HDLC_FCS_1) {
            uint16_t cksum_orig =
                (uint8_t)hdlc->frame.buf[hdlc->frame.len - 2] + 
                (uint8_t)hdlc->frame.buf[hdlc->frame.len - 1] * 256U;
            uint16_t cksum_calc = crc16(hdlc->frame.buf + 1, hdlc->length - 2);
            if (cksum_orig != cksum_calc)
                egoto2_set(E0, EBADMSG, "Ошибка контрольной суммы кадра FCS: "
                    "ожидалось 0x%04x, получено 0x%04x.", cksum_calc, cksum_orig);
            log("  [hdlc] fcs: cksum is ok.");
            ctx->state = HDLC_FCS;

        } else if (state == HDLC_FCS) {
           if (c != 0x7e)
                egoto2_set(E0, EBADMSG, "Ожидается закрывающий флаг 0x7e, "
                    "но прочитано 0x%02x.", c);
            log("  [hdlc] Frame complete (len=%d, payload=%u)",
                hdlc->length, hdlc->payload_len);
            ctx->state = HDLC_Complete;
            break;

        } else
            panic("Unhandled state: %s", HDLC_CTX_strstate(state));
            
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

    int rc;

    assert(!ferror(fp));
    if (ctx->state == HDLC_Initial) {
        assert(feof(fp));
        rc = 0;
    } else if (ctx->state == HDLC_Complete)
        rc = 1;
#if 0
    else
        panic("Unexpected state after parser loop.");
#endif
    return rc;

E0: return -1;
}
