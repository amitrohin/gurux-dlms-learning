// vi: ts=4:sts=4:sw=4:et:nu:noai:nosi:syn=off

#include <openssl/err.h>
#include "foo.h"

extern inline unsigned int ui_nextpow2(unsigned int x);
extern inline unsigned long ul_nextpow2(unsigned long x);
extern inline unsigned long long ull_nextpow2(unsigned long long x);
extern inline size_t zu_nextpow2(size_t x);

static __thread char *pos;
static __thread size_t cap, len;
static __thread char buf[0x1000];

static void log_bprefix_default(char const *file, int line, char const *func);
__thread log_bprefix_t log_bprefix = log_bprefix_default;

void log_breset() {
    pos = buf;
    cap = sizeof buf - 1;
    len = 0;
    buf[0] = '\0';
}

void log_bvprintf(char const *fmt, va_list ap) {
    size_t n = vsnprintf(pos, cap, fmt, ap);
    if (n > cap)
        n = cap;
    pos += n;
    cap -= n;
    len += n;
    *pos = '\0';
}

void log_bprintf(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_bvprintf(fmt, ap);
    va_end(ap);
}

void log_bprefix_default(char const *file, int line, char const *func) {
    log_bprintf("%s:%d: %s()", file, line, func);
}

void log_flush() {
    fflush(stdout);
    fprintf(stderr, "%s\n", buf);
    log_breset();
}

void vlog_ex(
    char const *file, int line, char const *func,
    char const *fmt, va_list ap)
{
    log_breset();
    log_bprefix(file, line, func);
    log_bprintf(": ");
    log_bvprintf(fmt, ap);
    log_flush();
}

void log_ex(
    char const *file, int line, char const *func,
    char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_ex(file, line, func, fmt, ap);
    va_end(ap);
}

void ossl_velog_ex(
    char const *file, int line, char const *func,
    char const *fmt, va_list ap)
{
    unsigned long e_;
    char const *libname_, *reason_, *file_, *data_, *func_;
    int line_, flags_, lib_id_, reason_id_, fatal_;

    e_ = ERR_get_error_all(&file_, &line_, &func_, &data_, &flags_);
    if (e_) {
        libname_ = ERR_lib_error_string(e_);
        reason_ = ERR_reason_error_string(e_);
        lib_id_ = ERR_GET_LIB(e_);
        reason_id_ = ERR_GET_REASON(e_);
        fatal_ = ERR_FATAL_ERROR(e_);

        log_breset();
        log_bprefix(file, line, func);
        log_bprintf(": ");
        log_bvprintf(fmt, ap);
        log_bprintf(
            ": %s:%d: %s(): err=0x%lx (lib=0x%x, reason=0x%x, fatal=%d),"
            " libname=\"%s\", reason=\"%s\"",
            file_, line_, func_, e_, lib_id_, reason_id_,
            fatal_, libname_, reason_
        );
        if (flags_ & ERR_TXT_STRING)
            log_bprintf(", data=\"%s\".", data_);
        log_flush();
    }
}

void ossl_elog_ex(
    char const *file, int line, char const *func,
    char const *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ossl_velog_ex(file, line, func, fmt, ap);
    va_end(ap);
}


static ssize_t fmread(void *cookie, char *buf, size_t size) {
    struct fmemctl *ctl = cookie;
    ssize_t n = ctl->len - ctl->pos;
    if (n > size)
        n = size;
    memcpy(buf, ctl->bufp + ctl->pos, n);
    ctl->pos += n;
    return n;
}

static ssize_t fmwrite(void *cookie, char const *buf, size_t size) {
    struct fmemctl *ctl = cookie;
    ssize_t n = ctl->cap - ctl->pos - 1;
    if (!n) {
        n = -1;
        errno = ENOSPC;
    } else {
        if (n > size)
            n = size;
        memcpy(ctl->bufp + ctl->pos, buf, n);
        ctl->pos += n;
        if (ctl->len < ctl->pos) {
            ctl->len = ctl->pos;
            ctl->bufp[ctl->len] = '\0';
        }
    }
    return n;
}

static int fmseek(void *cookie, off64_t *offset, int whence) {
    struct fmemctl *ctl = cookie;
    off64_t pos;
    switch (whence) {
        case SEEK_SET: pos = *offset; break;
        case SEEK_CUR: pos = ctl->pos + *offset; break;
        case SEEK_END: pos = ctl->len + *offset; break;
        default:
            errno = EINVAL;
            return -1;
    }
    if (pos < 0 || pos >= (off64_t)ctl->cap) {
        errno = EINVAL;
        return -1;
    }
    ssize_t gap = (ssize_t)pos - (ssize_t)ctl->len;
    if (gap > 0) {
        memset(ctl->bufp + ctl->len, 0, gap + 1);
        ctl->len = pos;
    }
    ctl->pos = *offset = pos;
    return 0;
}

FILE *fmemopen_ex(
    char * restrict buf, size_t size, char const * restrict mode,
    struct fmemctl *ctl)
{   
    FILE *fp = NULL;

    if (!buf || !size || !ctl)
        errno = EINVAL;
    else {
        ctl->bufp = buf;
        ctl->cap = size;
        ctl->len = ctl->pos = 0;
        cookie_io_functions_t fmemopen_ex_io_funcs = {
            .read = fmread, .write = fmwrite, .seek = fmseek, .close = NULL
        };
        fp = fopencookie(ctl, mode, fmemopen_ex_io_funcs);
        if (fp)
            setbuf(fp, NULL);
    }
    return fp;
}

extern inline void fmemopen_ex_reset(struct fmemctl *);


FILE *dup_fp(FILE *fp, char const *mode) {
    int fd, fd2 = -1;
    FILE *fp2 = NULL;

    if (!fp)
        egoto2_set(E0, EINVAL, "fp is NULL");
    fd = fileno(fp);
    if (fd == -1)
        egoto2(E0, errno, "fileno()");

    fd2 = dup(fileno(fp));
    if (fd2 == -1)
        egoto2(E0, errno, "dup()");
    fp2 = fdopen(fd2, mode);
    if (!fp2)
        egoto2(E0, errno, "fdopen()");
    return fp2;

E0: if (fd2 != -1) {
        int saved_errno = errno;
        close(fd2);
        errno = saved_errno;
    }
    return NULL;
}

