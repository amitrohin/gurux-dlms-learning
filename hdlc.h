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


typedef struct hdlc_addr {
    int16_t upper;  /* 7 или 14 бит */
    int16_t lower;  /* 7 или 14 бит */
} hdlc_addr_t;

static inline int hdlc_addr_eq(hdlc_addr_t a, hdlc_addr_t b) {
    return (a.upper == b.upper) && (a.lower == b.lower);
}


enum hdlc_frag_role {
    HDLC_FRAG_NONE, /* обычный hdlc, не входит в цепочку фрагментов */
    HDLC_FRAG_HEAD, /* цепочка фрагментов, первый узел */
    HDLC_FRAG_PART, /* цепочка фрагментов, не головной узел */
};
typedef struct hdlc {
    int nref;
    struct mbuf frame;
    int16_t length;         /* 11 бит */
    hdlc_addr_t DA, SA;
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

#define HDLC_INITIALIZER(hdlc)  { \
        .nref = 1, \
        .mbuf_frame = MBUF_INITIALIZER(hdlc.mbuf_frame), \
        .length = 0, \
        .DA = { .upper = -1, .lower = -1 }, \
        .SA = { .upper = -1, .lower = -1 }, \
        .fragment_bit = 0, \
        .control = { .type = -1 }, \
        .payload_offset = -1, \
        .payload_len = -1, \
        .llc_dsap = -1, \
        .llc_ssap = -1, \
        .llc_control = -1, \
        .ent = {0}, \
        .frag_role = HDLC_FRAG_NONE, \
        .frag_head = STAILQ_HEAD_INITIALIZER(hdlc.frag_head) \
    }

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

inline void HDLC_AUTOPTR_cleanup(HDLC **hdlc) {
    if (*hdlc)
        HDLC_detach(*hdlc);
}
#define HDLC_AUTOPTR(ptr) \
    __attribute__((cleanup(HDLC_AUTOPTR_cleanup))) HDLC *ptr = NULL

inline void HDLC_AUTOADDR_cleanup(HDLC (*hdlc)[1]) {
    HDLC_fini(*hdlc);
}
#define HDLC_AUTOADDR(addr) \
    __attribute__((cleanup(HDLC_AUTOADDR_cleanup))) \
    HDLC addr[1] = {HDLC_INITIALIZER(addr[0])}


enum hdlc_framer_state {
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
char const *hdlc_framer_strstate(enum hdlc_framer_state state);

typedef struct hdlc_framer {
    int nref;
    enum hdlc_framer_state state;
    HDLC *hdlc;
} HDLC_FRAMER;

void        HDLC_FRAMER_init(HDLC_FRAMER *framer);
void        HDLC_FRAMER_fini(HDLC_FRAMER *framer);

inline
HDLC_FRAMER *HDLC_FRAMER_attach(HDLC_FRAMER *framer) {
    assert(framer->nref > 0);
    framer->nref++;
    return framer;
}

inline
void HDLC_FRAMER_detach(HDLC_FRAMER *framer) {
    assert(framer->nref > 0);
    if (framer->nref == 1) {
        HDLC_FRAMER_fini(framer);
        free(framer);
    } else 
        framer->nref--;
}

inline HDLC *HDLC_FRAMER_get_hdlc(HDLC_FRAMER *framer) {
    HDLC *hdlc = framer->hdlc;
    if (hdlc)
        HDLC_attach(hdlc);
    return hdlc;
}

inline
int HDLC_FRAMER_shared(HDLC_FRAMER *ctx) {
    assert(ctx->nref > 0);
    return (ctx->nref != 1);
}

HDLC_FRAMER *HDLC_FRAMER_create();
int HDLC_FRAMER_update(HDLC_FRAMER *framer, IO *io);
void HDLC_FRAMER_reset(HDLC_FRAMER *framer);


/* V(S) — Send State Variable (переменная состояния передачи):
 *     Счетчик (0..7), который хранит номер следующего I-кадра, который мы собираемся
 *     отправить. При формировании исходящего кадра значение V(S) записывается в поле
 *     N(S) заголовка, после чего V(S) увеличивается на 1 по модулю 8.
 * 
 * V(R) — Receive State Variable (переменная состояния приема):
 *     Счетчик (0..7), который хранит номер N(S) следующего входящего I-кадра, который
 *     мы ожидаем получить от собеседника.  Когда приходит корректный кадр с N(S) ==
 *     V(R), мы увеличиваем V(R) на 1 по модулю 8 и подставляем это новое V(R) в поле
 *     N(R) своих ответных кадров (подтверждая тем самым прием всех кадров вплоть до
 *     V(R) - 1).
 */ 
typedef struct hdlc_sess {
    LIST_ENTRY(hdlc_sess) mux_ent;
    hdlc_addr_t client, server;
    int nref;
    int8_t vs, vr;
    HDLC *hdlc;
} HDLC_SESS;

inline void HDLC_SESS_init(HDLC_SESS *sess) {
    sess->client.upper = -1;
    sess->client.lower = -1;
    sess->server.upper = -1;
    sess->server.lower = -1;
    sess->nref = -1;
    sess->vs = -1;
    sess->vr = -1;
    sess->hdlc = NULL;
}

inline void HDLC_SESS_fini(HDLC_SESS *sess) {
    assert(sess);
    assert(sess->nref == 1);
    if (sess->hdlc)
        HDLC_detach(sess->hdlc);
}

inline HDLC_SESS *HDLC_SESS_create() {
    HDLC_SESS *sess;

    sess = malloc(sizeof *sess);
    if (sess)
        HDLC_SESS_init(sess);
    else
        elog2(errno, "malloc()");
    return sess;
}

inline HDLC_SESS *HDLC_SESS_attach(HDLC_SESS *sess) {
    assert(sess);
    assert(sess->nref > 0);
    sess->nref++;
    return sess;
}

inline void HDLC_SESS_detach(HDLC_SESS *sess) {
    assert(sess);
    assert(sess->nref > 0);
    if (sess->nref == 1) {
        HDLC_SESS_fini(sess);
        free(sess);
    } else
        sess->nref--;
}

inline void HDLC_SESS_AUTOPTR_cleanup(HDLC_SESS **sess) {
    if (*sess)
        HDLC_SESS_detach(*sess);
}
#define HDLC_SESS_AUTOPTR(ptr) \
    __attribute__((cleanup(HDLC_SESS_AUTOPTR_cleanup))) HDLC_SESS *ptr = NULL

inline void HDLC_SESS_AUTOADDR_cleanup(HDLC_SESS (*sess)[1]) {
    HDLC_SESS_fini(*sess);
}
#define HDLC_SESS_AUTOADDR(addr) \
    __attribute__((cleanup(HDLC_SESS_AUTOADDR_cleanup))) \
    HDLC_SESS addr[1] = {HDLC_INITIALIZER(addr[0])}



#define HDLC_MUX_HASH_BITS  7
#define HDLC_MUX_HASH_SIZE  (1 << HDLC_MUX_HASH_BITS)

inline size_t hdlc_mux_hash_index(hdlc_addr_t a, hdlc_addr_t b) {
    uint32_t ka = ((uint32_t)(uint16_t)a.upper << 16) | (uint16_t)a.lower;
    uint32_t kb = ((uint32_t)(uint16_t)b.upper << 16) | (uint16_t)b.lower;
    if (ka > kb) {
        uint32_t tmp = ka;
        ka = kb;
        kb = tmp;
    }
    return (size_t)(((((uint64_t)ka << 32) | kb) * 11400714819323198485ULL) >> (64 - HDLC_MUX_HASH_BITS));
}

typedef struct hdlc_mux {
    LIST_HEAD(, hdlc_sess) buckets[HDLC_MUX_HASH_SIZE];
} HDLC_MUX;

inline void HDLC_MUX_init(HDLC_MUX *mux) {
    for (size_t i = 0; i < HDLC_MUX_HASH_SIZE; i++)
        LIST_INIT(mux->buckets + i);
}

inline void HDLC_MUX_fini(HDLC_MUX *mux) {
    for (size_t i = 0; i < HDLC_MUX_HASH_SIZE; i++) {
        HDLC_SESS *p = LIST_FIRST(mux->buckets + i);
        while (p) {
            HDLC_SESS *q = LIST_NEXT(p, mux_ent);
            HDLC_SESS_detach(p);
            p = q;
        }
        LIST_INIT(mux->buckets + i);
    }
}

inline HDLC_SESS *HDLC_MUX_get_sess(HDLC_MUX *mux, hdlc_addr_t a, hdlc_addr_t b) {
    size_t index = hdlc_mux_hash_index(a, b);
    HDLC_SESS *p = NULL;
    LIST_FOREACH(p, mux->buckets + index, mux_ent) {
        if ((hdlc_addr_eq(p->client, a) || hdlc_addr_eq(p->client, b)) &&
            (hdlc_addr_eq(p->server, a) || hdlc_addr_eq(p->server, b)))
            break;
    }
    return p ? HDLC_SESS_attach(p) : p;
}

inline void HDLC_MUX_set_sess(HDLC_MUX *mux, HDLC_SESS *sess) {
    HDLC_SESS_attach(sess);
    size_t index = hdlc_mux_hash_index(sess->client, sess->server);
    LIST_INSERT_HEAD(mux->buckets + index, sess, mux_ent);
}

#if 0
typedef struct hdlc_sess {
    int8_t ns, nr;
} HDLC_SESS;


HDLC_FRAMER framer[1];
HDLC_SESS sess[1];

HDLC_FRAMER_init(framer)
HDLC_SESS_init(sess);

sess = HDLC_SESS_se(dau, dal, sau, sal);
HDLC_SESS_();


#endif

#endif
