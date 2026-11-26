// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#include <sys/uio.h>

#include "foo.h"
#include "io.h"

extern inline void IO_init(IO *io, int fd);
extern inline void IO_fini(IO *io);
extern inline IO *IO_create(int fd);
extern inline IO *IO_attach(IO *io);
extern inline void IO_detach(IO *io);

ssize_t IO_read(IO *io, void *buf, size_t cap) {
    size_t nbytes = 0;
    uint8_t *dst;       /* uint8_t нужно для арифметики указателя */
    size_t dst_cap;
    size_t size, ncopy;
    struct iovec iov[2];
    ssize_t br;

    if (!cap)
        eret2_set(-1, EINVAL, "Размер буфера записи не может быть 0.");
    dst = buf;
    dst_cap = cap;
    size = io->len - io->pos;
    if (!size) 
        io->len = io->pos = 0;
    else {
        ncopy = size;
        if (ncopy > dst_cap)
            ncopy = dst_cap;
        memcpy(dst, io->buf + io->pos, ncopy);
        nbytes += ncopy;
        io->pos += ncopy;
        if (io->pos == io->len)
            io->pos = io->len = 0;
        if (nbytes == cap)
            return nbytes;
        dst += ncopy;
        dst_cap -= ncopy;
    }
    assert(io->pos == 0 && io->len == 0);
    iov[0].iov_base = dst;
    iov[0].iov_len = dst_cap;
    iov[1].iov_base = io->buf;
    iov[1].iov_len = sizeof io->buf;
    br = readv(io->fd, iov, 2);
    if (br < 0) {
        if (nbytes)
            return nbytes;
        if (errno == EAGAIN || errno == EINTR)
            elog2(errno, "readv()");
        return -1;
    }
    if (!br)
        return nbytes;
    if ((size_t)br <= dst_cap)
        nbytes += br;
    else {
        nbytes += dst_cap;
        io->len = br - dst_cap;
    }
    return nbytes;
}

ssize_t IO_fill(IO *io) {
    assert(io->pos == io->len);
    io->pos = 0;
    io->len = 0;

    ssize_t br = read(io->fd, io->buf, sizeof(io->buf));
    if (br < 0) {
        if (errno != EAGAIN && errno != EINTR)
            elog2(errno, "read()");
    } else
        io->len = (size_t)br;
    return br;
}

extern inline int IO_getc(IO *io);
