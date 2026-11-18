// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#ifndef __MBUF_H__
#define __MBUF_H__

#include <stdlib.h>

struct mbuf {
    size_t cap;
    size_t len;
    char *buf;
};
#define MBUF_INITIALIZER    {.cap = 0, .len = 0, .buf = NULL}

inline void mbuf_init(struct mbuf *mb) {
    mb->cap = mb->len = 0;
    mb->buf = NULL;
}

inline void mbuf_fini(struct mbuf *mb) {
    if (mb->buf)
        free(mb->buf);
}

int mbuf_resize(struct mbuf *mb, size_t size);

inline int mbuf_reserve(struct mbuf *mb, size_t size) {
    return size > mb->cap ? mbuf_resize(mb, size) : 0;
}

inline void mbuf_reset(struct mbuf *mb) {
    mb->len = 0;
}

int mbuf_addc(struct mbuf *mb, int c);
int mbuf_add(struct mbuf *mb, void const *buf, size_t n);

#endif
