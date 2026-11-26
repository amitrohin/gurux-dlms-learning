// vi: ts=4:sts=4:sw=4:et:nu:syn=off:noai:nosi

#ifndef __SPODES_HDLC_H__
#define __SPODES_HDLC_H__

#include <stdint.h>
#include <sys/queue.h>

#include "foo.h"
#include "mbuf.h"
#include "crc.h"
#include "io.h"

enum hdlc_type { HDLC_I, HDLC_S, HDLC_U };

enum hdlc_s_cmd {
    HDLC_S_RR   = 0x01, /* Receive Ready (подтверждение кадров до nr - 1) */
    HDLC_S_RNR  = 0x05, /* Receive Not Ready (буфер занят) */

    /* Ниже команды, которые не используются в СПОДЭС */
    HDLC_S_REJ  = 0x09, /* Reject, запрос повторной передачи с кадра nr */
    HDLC_S_SREJ = 0x0d, /* Selective Reject, выборочный повтор кадра nr */
};

/* [!] Кодируются битами байта Control без учета флага P/F (маска 0xEF) */
enum hdlc_u_cmd {
    HDLC_U_UI   = 0x03, /* Unnumbered Information - бессессионный кадр данных */
    HDLC_U_SNRM = 0x83, /* Set Normal Response Mode - запрос открытия сеанса связи */
    HDLC_U_DISC = 0x43, /* Disconnect - запрос разрыва сеанса связи */
    HDLC_U_UA   = 0x63, /* Unnumbered Acknowledgment - подтверждение SNRM или DISC */
    HDLC_U_FRMR = 0x87, /* Frame Reject - отклонение некорректного кадра (ошибка протокола) */
    HDLC_U_DM   = 0x0F, /* Disconnected Mode - станция находится в отключенном состоянии */

    /* Команды и ответы стандарта ISO/IEC 13239, не используемые в СПОДЭС */
    HDLC_U_SIM  = 0x07, /* Set Initialization Mode - команда перевода в режим инициализации */
    HDLC_U_RIM  = HDLC_U_SIM,
                        /* Request Initialization Mode - ответ-запрос инициализации */
    HDLC_U_SARM = HDLC_U_DM,
                        /* Set Asynchronous Response Mode - асинхронный режим ответа */
    HDLC_U_UP   = 0x23, /* Unnumbered Poll - команда ненумерованного опроса */
    HDLC_U_SABM = 0x2F, /* Set Asynchronous Balanced Mode - асинхронный сбалансированный режим */
    HDLC_U_RD   = HDLC_U_DISC,
                        /* Request Disconnect - ответ-запрос на отключение */
    HDLC_U_SARME= 0x4F, /* Set Asynchronous Response Mode Extended — расширенный ARM (7-бит) */
    HDLC_U_SABME= 0x6F, /* Set Asynchronous Balanced Mode Extended — расширенный ABM (7-бит) */
    HDLC_U_RSET = 0x8F, /* Reset - команда сброса счетчика принимаемых кадров N(R) */
    HDLC_U_XID  = 0xAF, /* Exchange Identification - обмен идентификацией и параметрами станции */
    HDLC_U_SNRME= 0xCF, /* Set Normal Response Mode Extended - расширенный NRM (7-бит счетчики) */
    HDLC_U_TEST = 0xE3, /* Test - команда/ответ тестирования канала связи (эхо-проверка) */
    HDLC_U_UIH  = 0xEF, /* UI with Header Check - ненумерованная информация с CRC только заголовка */
};

/* Поля байта Control для информационного кадра (I-Frame) */
struct hdlc_control_i {
    enum hdlc_type type;
    uint8_t ns;             /* N(S): номер отправляемого кадра (0..7) */
    uint8_t nr;             /* N(R): номер следующего ожидаемого кадра (0..7) */
    uint8_t pf;             /* P/F: бит опроса/финала (0 или 1) */
};

/* Поля байта Control для супервизорного кадра (S-Frame) */
struct hdlc_control_s {
    enum hdlc_type type;
    enum hdlc_s_cmd cmd;    /* Поля байта Control для супервизорного кадра (S-Frame) */
    uint8_t nr;             /* N(R): номер следующего ожидаемого кадра (0..7) */
    uint8_t pf;             /* P/F: бит опроса/финала (0 или 1) */
};

/* Поля байта Control для ненумерованного кадра (U-Frame) */
struct hdlc_control_u {
    enum hdlc_type type;
    enum hdlc_u_cmd cmd;    /* Команда/ответ U-кадра (SNRM, UA, DISC, DM, FRMR, UI и др.) */
    uint8_t pf;             /* P/F: бит опроса/финала (0 или 1) */
};

union hdlc_control {
    enum hdlc_type type;
    struct hdlc_control_i i;
    struct hdlc_control_s s;
    struct hdlc_control_u u;
};


enum hdlc_frag_role {
    HDLC_FRAG_NONE, /* обычный hdlc, не входит в цепочку фрагментов */
    HDLC_FRAG_HEAD, /* цепочка фрагментов, первый узел */
    HDLC_FRAG_PART, /* цепочка фрагментов, не головной узел */
};
typedef struct hdlc {
    int nref;
    struct mbuf frame;
    int16_t length;         /* 11 бит */
    int16_t DA_upper;       /* 7 или 14 бит */
    int16_t DA_lower;       /* 7 или 14 бит */
    int16_t SA_upper;       /* 7 или 14 бит */
    int16_t SA_lower;       /* 7 или 14 бит */
    uint8_t fragment_bit;   /* 1 bit */
    union hdlc_control control;
    int16_t payload_offset;
    int16_t payload_len;
    int16_t llc_dsap;       /* 0xe6 */
    int16_t llc_ssap;       /* 0xe6, 0xe7 */
    int16_t llc_control;    /* 0 */
    STAILQ_ENTRY(hdlc) ent;
    enum hdlc_frag_role frag_role;
    union {
        STAILQ_HEAD(, hdlc) frag_head;
        STAILQ_ENTRY(hdlc) frag_ent;
    };
} HDLC;

void        HDLC_init(HDLC *hdlc);

inline
void HDLC_detach(HDLC *hdlc);

inline
void        HDLC_fini(HDLC *hdlc) {
    assert(hdlc);
    assert(hdlc->nref == 1);
    mbuf_fini(&hdlc->frame);
    switch (hdlc->frag_role) {
        case HDLC_FRAG_NONE:
            break;
        case HDLC_FRAG_HEAD: {
            HDLC *p = STAILQ_FIRST(&hdlc->frag_head);
            while (p) {
                HDLC *q = STAILQ_NEXT(p, frag_ent);
                HDLC_detach(p);
                p = q;
            }
            break;
        }
        case HDLC_FRAG_PART:
            break;
    }
}

inline
HDLC *      HDLC_attach(HDLC *hdlc) {
    assert(hdlc);
    assert(hdlc->nref > 0);
    hdlc->nref++;
    return hdlc;
}

inline
void        HDLC_detach(HDLC *hdlc) {
    assert(hdlc);
    assert(hdlc->nref > 0);
    if (hdlc->nref == 1) {
        HDLC_fini(hdlc);
        free(hdlc);
    } else
        hdlc->nref--;
}

inline
int         HDLC_shared(HDLC *hdlc) {
    assert(hdlc);
    assert(hdlc->nref > 0);
    return (hdlc->nref != 1);
}

HDLC *      HDLC_create();
void        HDLC_reset(HDLC *hdlc);

enum hdlc_ctx_state {
    HDLC_Initial,           /* состояние перед чтением данных */
    HDLC_FrameOpenFlag,     /* прочитан маркер начала кадра 0x7e */
    HDLC_FrameFormat_1,     /* разобран первый байт */
    HDLC_FrameFormat,       /* прочитан второй байт (все поле) */
    HDLC_DA_1,              /* прочитан 1-й байт адреса получателя (младший бит == 0) */
    HDLC_DA_2,              /* прочитан 2-й байт адреса получателя (младший бит == 0) */
    HDLC_DA_3,              /* прочитан 3-й байт адреса получателя (младший бит == 0) */
    HDLC_DA,                /* адрес получателя прочитан полностью (младший бит == 1, 1..4 байта) */
    HDLC_SA_1,              /* прочитан 1-й байт адреса отправителя (младший бит == 0) */
    HDLC_SA_2,              /* прочитан 2-й байт адреса отправителя (младший бит == 0) */
    HDLC_SA_3,              /* прочитан 3-й байт адреса отправителя (младший бит == 0) */
    HDLC_SA,                /* адрес отправителя прочитан полностью (младший бит == 1, 1..4 байта) */
    HDLC_Control,           /* прочитан байт Control */
    HDLC_HCS_1,             /* прочитан 1-й байт HCS (младший) */
    HDLC_HCS,               /* прочитан и проверен HCS (2 байта) */
    HDLC_Payload_,          /* чтение информационного поля (LLC + APDU / параметры) */
    HDLC_Payload,           /* чтение информационного поля (LLC + APDU / параметры) */
    HDLC_FCS_1,             /* прочитан 1-й байт FCS (младший) */
    HDLC_FCS,               /* прочитан и проверен FCS (2 байта) */
    HDLC_Complete,          /* пакет полностью прочитан (включая закрывающий 0x7e) */
};
char const *hdlc_ctx_strstate(enum hdlc_ctx_state state);

typedef struct hdlc_ctx {
    int nref;
    enum hdlc_ctx_state state;
    HDLC *hdlc;
} HDLC_CTX;

void        HDLC_CTX_init(HDLC_CTX *ctx);
void        HDLC_CTX_fini(HDLC_CTX *ctx);

inline
HDLC_CTX *  HDLC_CTX_attach(HDLC_CTX *ctx) {
    assert(ctx->nref > 0);
    ctx->nref++;
    return ctx;
}

inline
void        HDLC_CTX_detach(HDLC_CTX *ctx) {
    assert(ctx->nref > 0);
    if (ctx->nref == 1) {
        HDLC_CTX_fini(ctx);
        free(ctx);
    } else 
        ctx->nref--;
}

inline
int         HDLC_CTX_shared(HDLC_CTX *ctx) {
    assert(ctx->nref > 0);
    return (ctx->nref != 1);
}

HDLC_CTX *  HDLC_CTX_create();
int         HDLC_CTX_update(HDLC_CTX *ctx, IO *io);
void        HDLC_CTX_reset(HDLC_CTX *ctx);

#endif
