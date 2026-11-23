// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#ifndef __MBUF_H__
#define __MBUF_H__

#include <stdlib.h>
#include <stdint.h>

struct mbuf {
    int nref;
    size_t cap;
    size_t len;
    uint8_t *buf;
};
#define MBUF_INITIALIZER    {.nref = 1, .cap = 0, .len = 0, .buf = NULL}

inline void     mbuf_init(struct mbuf *mb) {
    mb->nref = 1;
    mb->cap = mb->len = 0;
    mb->buf = NULL;
}

inline void     mbuf_fini(struct mbuf *mb) {
    assert(mb->nref == 1);
    if (mb->buf)
        free(mb->buf);
}

inline struct mbuf *mbuf_attach(struct mbuf *mb) {
    assert(mb->nref > 0);
    mb->nref++;
    return mb;
}

inline void     mbuf_detach(struct mbuf *mb) {
    assert(mb->nref > 0);
    if (mb->nref == 1) {
        mbuf_fini(mb);
        free(mb);
    } else
        mb->nref--;
}

struct mbuf *   mbuf_create(size_t size);

int mbuf_resize(struct mbuf *mb, size_t size);

inline int      mbuf_reserve(struct mbuf *mb, size_t size) {
    return size > mb->cap ? mbuf_resize(mb, size) : 0;
}

inline size_t   mbuf_len(struct mbuf *mb) {
    return mb->len;
}

inline void     mbuf_reset(struct mbuf *mb) {
    mb->len = 0;
}

int             mbuf_addc(struct mbuf *mb, int c);
int             mbuf_add(struct mbuf *mb, void const *buf, size_t n);

#endif
