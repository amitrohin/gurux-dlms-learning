// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#include "foo.h"
#include "mbuf.h"

extern inline void mbuf_init(struct mbuf *mb);
extern inline void mbuf_fini(struct mbuf *mb);
extern inline struct mbuf * mbuf_attach(struct mbuf *mb);
extern inline void mbuf_detach(struct mbuf *mb);

struct mbuf *mbuf_create(size_t size) {
    struct mbuf *mb;

    mb = malloc(sizeof(struct mbuf));
    if (mb) {
        mbuf_init(mb);
        if (mbuf_resize(mb, size)) {
            elog("mbuf_resize()");
            mbuf_detach(mb);
            mb = NULL;
        }
    } else
        elog2(errno, "malloc(%zu)", sizeof(struct mbuf));
    return mb;
}

int mbuf_resize(struct mbuf *mb, size_t size) {
    if (!size) {
        mb->cap = mb->len = 0;
        free(mb->buf);
        mb->buf = NULL;
    } else {
        void *p;
        size_t cap;

        cap = (size & (size - 1)) ? zu_nextpow2(size) : size;
        assert(cap != 0);
        if (cap != mb->cap) {
            p = realloc(mb->buf, cap);
            if (!p)
                eret2(-1, errno, "malloc(): %zu bytes\n", cap);
            mb->cap = cap;
            if (mb->len > cap)
                mb->len = cap;
            mb->buf = p;
        }
    }
    return 0;
}

extern inline int mbuf_reserve(struct mbuf *mb, size_t size);
extern inline void mbuf_reset(struct mbuf *mb);
extern inline size_t mbuf_len(struct mbuf *mb);

int mbuf_addc(struct mbuf *mb, int c) {
    if (mb->len == mb->cap) {
        if (mbuf_resize(mb, mb->len + 1))
            return -1;
    }
    mb->buf[mb->len++] = c;
    return 0;
}

int mbuf_add(struct mbuf *mb, void const *buf, size_t n) {
    size_t len = mb->len + n;
    if (len > mb->cap) {
        if (mbuf_resize(mb, len))
            return -1;
    }
    memcpy(mb->buf + mb->len, buf, n);
    mb->len = len;
    return 0;
}
