// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#ifndef __IO_H__
#define __IO_H__

#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

typedef struct io {
    int nref;
    int fd;
    size_t len;
    size_t pos;
    uint8_t buf[512];
} IO;

inline void IO_init(IO *io, int fd) {
    io->nref = 1;
    io->fd = fd;
    io->len = io->pos = 0;
}

inline void IO_fini(IO *io) {
    assert(io->nref == 1);
    if (io->fd != -1)
        close(io->fd);
}

inline IO *IO_create(int fd) {
    IO *io = malloc(sizeof(*io));
    if (io)
        IO_init(io, fd);
    else
        elog2(errno, "malloc()");
    return io;
}

inline IO *IO_attach(IO *io) {
    assert(io->nref > 0);
    io->nref++;
    return io;
}

inline void IO_detach(IO *io) {
    assert(io->nref > 0);
    if (io->nref == 1) {
        IO_fini(io);
        free(io);
    } else
        io->nref--;

}

ssize_t IO_read(IO *io, void *buf, size_t cap);
ssize_t IO_fill(IO *io);

inline int IO_getc(IO *io) {
    if (io->pos < io->len)
        return io->buf[io->pos++];
    ssize_t rc = IO_fill(io);
    if (rc <= 0) {
        if (rc == 0)
            errno = 0;
        return -1;
    }
    return io->buf[io->pos++];
}

#endif
