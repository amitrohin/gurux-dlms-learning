#ifndef __FOO_H__
#define __FOO_H__
// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

/* stringify without expanding x */
#define FOO_STRING(x)                   #x  
/* expand x, then stringify */
#define FOO_XSTRING(x)                  FOO_STRING(x)

#define FOO_CONCAT(x, y)                x##y
#define FOO_XCONCAT(x, y)               FOO_CONCAT(x, y)
#define AUTONAME                        FOO_XCONCAT(autoname_, __COUNTER__)

/* Round x up to the nearest multiple of y */
#define roundup_(x, y, y_)              ({ __auto_type y_ = (y); (((x) + (y_ - 1)) / y_) * y_; })
#define roundup(x, y)                   roundup_(x, y, AUTONAME)

/* Round x up to a multiple of y, where y is a power of two */
#define roundup2_(x, y, y_)             ({ __auto_type y_ = (y) - 1; ((x) + y_) & ~y_; })
#define roundup2(x, y)                  roundup2_(x, y, AUTONAME)

void log_breset();
void log_bvprintf(char const *fmt, va_list ap);
void log_bprintf(char const *fmt, ...);
void log_flush();

typedef void (*log_bprefix_t)(char const *file, int line, char const *func);
extern __thread log_bprefix_t log_bprefix;

void vlog_ex(char const *file, int line, char const *func, char const *fmt, va_list ap);
void log_ex(char const *file, int line, char const *func, char const *fmt, ...);
#define log(fmt, ...)                   log_ex(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define elog(fmt, ...)                  log_ex(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define elog2(e, fmt, ...)              elog(fmt ": %s (%d).", ##__VA_ARGS__, strerror(e), e)
#define egoto(L, fmt, ...)              do { elog(fmt, ##__VA_ARGS__); goto L; } while (0) 
#define egoto2(L, e, fmt, ...)          do { elog2(e, fmt, ##__VA_ARGS__); goto L; } while (0) 
#define egoto2_set(L, e, fmt, ...)      do { errno = (e); egoto2(L, errno, fmt, ##__VA_ARGS__); } while (0)
#define eret(code, fmt, ...)            do { elog(fmt, ##__VA_ARGS__); return (code); } while (0) 
#define eret2(code, e, fmt, ...)        do { elog2(e, fmt, ##__VA_ARGS__); return (code); } while (0) 
#define eret2_set(code, e, fmt, ...)    do { errno = (e); eret2(code, errno, fmt, ##__VA_ARGS__); } while (0) 

#define panic(fmt, ...)                 do { elog(fmt, ##__VA_ARGS__); abort(); } while (0)

void ossl_velog_ex(char const *file, int line, char const *func, char const *fmt, va_list ap);
void ossl_elog_ex(char const *file, int line, char const *func, char const *fmt, ...);
#define ossl_elog(fmt, ...)             ossl_elog_ex(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ossl_egoto(L, fmt, ...)         do { ossl_elog(fmt, ##__VA_ARGS__); goto L; } while (0)
#define ossl_eret(code, fmt, ...)       do { ossl_elog(fmt, ##__VA_ARGS__); return (code); } while (0) 

inline unsigned int ui_nextpow2(unsigned int x) {
    unsigned int n = 2;
    if (x > 1)
        n = 1U << (sizeof(unsigned int) * CHAR_BIT - __builtin_clz(x - 1));
    return n;
}
inline unsigned long ul_nextpow2(unsigned long x) {
    unsigned long n = 2;
    if (x > 1)
        n = 1UL << (sizeof(unsigned long) * CHAR_BIT - __builtin_clzl(x - 1));
    return n;
}
inline unsigned long long ull_nextpow2(unsigned long long x) {
    unsigned long long n = 2;
    if (x > 1)
        n = 1ULL << (sizeof(unsigned long long) * CHAR_BIT - __builtin_clzll(x - 1));
    return n;
}
inline size_t zu_nextpow2(size_t x) {
    size_t n;
    if (sizeof(size_t) == sizeof(unsigned int))
        n = ui_nextpow2(x);
    else if (sizeof(size_t) == sizeof(unsigned long))
        n = ul_nextpow2(x);
    else if (sizeof(size_t) == sizeof(unsigned long long))
        n = ull_nextpow2(x);
    else {
        panic("size_t должен быть одним из типов: unsigned int, "
            "unsigned long или unsigned long long");
    }
    return n;
}

/* FILE *fmemopen_ex(buf, size, mode, ctl);
 *
 * A variant of fmemopen() that supports resetting the internal buffer state
 * to the initial position, allowing reuse of the same FILE object without
 * freeing and re-creating it.
 */
struct fmemctl {
    char *bufp;
    size_t cap;
    size_t len;
    size_t pos;
};
FILE *fmemopen_ex(
    char *restrict buf,
    size_t size,
    char const *restrict mode,
    struct fmemctl *ctl
); 
inline void fmemopen_ex_reset(struct fmemctl *ctl) {
    ctl->bufp[0] = '\0';
    ctl->len = ctl->pos = 0;
}

#endif
