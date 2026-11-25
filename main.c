// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include "foo.h"
#include "hdlc.h"

static void usage(int code, const char *progname) __attribute__((noreturn));
static int getnum(int *num, const char *s, int base);

static char *progname;
static struct line {
    FILE *fp;
#define LINE_TERMIOS_VALID  1
    int flags;
    int speed;
    char dev[24];
    struct termios tios;
} line = {
    .speed = 9600,
    .dev = "/dev/cuau0",
};

int main(int argc, char *argv[]) {
    progname = ({
        char *p = strrchr(argv[0], '/');
        p ? p + 1 : argv[0];
    });

    static struct option opts[] = {
        {"line", required_argument, NULL, 'l'},
        {"speed", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };
    for (int c;;) {
        c = getopt_long(argc, argv, "l:s:", opts, NULL);
        if (c == -1) {
            break;
        } else if (c == 'l') {
            char const *fmt = "%s";
            if (*optarg != '/') 
                fmt = "/dev/%s";
            if (snprintf(line.dev, sizeof line.dev, fmt, optarg) >= sizeof line.dev)
                egoto(E0, "%s: line too long.", optarg);
        } else if (c == 's') {
            if (getnum(&line.speed, optarg, 10))
                goto E0;
        } else
            usage(1, progname);
    }
    argc -= optind;
    argv += optind;

#define XXX 1

#ifndef XXX
    printf("%s, %d\n", line.dev, line.speed);

    line.fp = fopen(line.dev, "w+");
    if (!line.fp)
        egoto2(E0, errno, "fopen(): %s", line.dev);
    if (tcgetattr(fileno(line.fp), &line.tios) < 0)
        egoto2(E0, errno, "tcgetattr(): %s", line.dev);
    line.flags |= LINE_TERMIOS_VALID;
    do {
        struct termios raw = line.tios;
        if (cfsetspeed(&raw, line.speed) < 0)
            egoto2(E0, errno, "cfsetspeed(): %s: speed = %d", line.dev, line.speed);
        cfmakeraw(&raw);
        printf("MIN = %d, TIME = %d\n", raw.c_cc[VMIN], raw.c_cc[VTIME]);
        if (tcsetattr(fileno(line.fp), TCSAFLUSH, &raw) < 0)
            egoto2(E0, errno, "tcsetattr(): %s", line.dev);
    } while (0);

    fprintf(line.fp, "");
 

    


    if (tcsetattr(fileno(line.fp), TCSAFLUSH, &line.tios) < 0)
        elog2(errno, "tcsetattr(): %s", line.dev);
    fclose(line.fp);
#endif

    /*

    typedef struict hdlc_sess {
        int16_t DA_upper;
        int16_t DA_lower;
        int16_t SA_upper;
        int16_t SA_lower;
        uint8_t ns;
        uint8_t nr;
    } HDLC_SESS;

    HDLC_SESS hdlc_sess[1];
    HDLC hdlc[1];
    int kq;
    struct kevent kev[2];

    kq = kqueue();
    if (kq < 0)
        egoto2(E0, errno, "kqueue()");
    EV_SET(&kev[0], fileno(fp), EVFILT_READ,  EV_ADD|EV_DISABLE, 0, 0, NULL);
    EV_SET(&kev[1], fileno(fp), EVFILT_WRITE, EV_ADD|EV_DISABLE, 0, 0, NULL);
    rc = kevent(kq, kev, sizeof kev/sizeof kev[0], kev, sizeof kev/sizeof kev[0], NULL);
    if (rc < 0)
        egoto2(E0, errno, "kevent()");
    for (int i = 0; i < rc; i++)
        if (kev[i].flags & EV_ERROR)
            egoto2_set(E0, (int)kev[i].data, "Не получилось создать событие %s.", 
                kev[i].filter == EVFILT_READ ? "чтения из потока" : "записи в поток");

    HDLC_SESS_init2(hdlc_sess, 1, 8574, 16, -1);
    HDLC_init(hdlc);

    hdlc->DA_upper = hdlc_sess->DA_upper;
    hdlc->DA_lower = hdlc_sess->DA_lower;
    hdlc->SA_upper = hdlc_sess->SA_upper;
    hdlc->SA_lower = hdlc_sess->SA_lower;
    hdlc->fragment_bit = 0;
    hdlc->control.type = HDLC_U;
    hdlc->control.u.cmd = HDLC_U_SNRM;
    hdlc->control.u.pf = 1;

    if (DHLC_build_frame(hdlc, payload, payload_len))
        egoto(E0, "DHLC_build_frame() failed.");

    HDLC_dump_frame(hdlc);



    buf = hdlc->frame.buf;
    len = hdlc->frame.len;
    while (1) {
        if (len) {
            size_t n = fwrite(buf, 1, len, fp);
            buf += n;
            len -= n;
        }
        if (!len)
            if (!fflush(fp))
                break;
        assert(ferror(fp));
        if (errno == EPIPE)
            break;
        if (errno != EAGAIN)
            egoto2(E0, errno, "fflush()")
        clearerr(fp);
        EV_SET(kev, fileno(fp), EVFILT_WRITE, EV_ENABLE|EV_DISPATCH, 0, 0, NULL);
        rc = kevent(kq, &kev[0], 1, &kev[0], 1, NULL);
        if (rc < 0)
            egoto2(E0, errno, "kevent()");
        assert(rc);
        if (kev[0].flags & EV_ERROR)
            egoto2_set(E0, (int)kev[0].data, "Не получилось активировать событие записи в поток.");
    }


    HDLC_fini(hdlc);
    HDLC_SESS_fini(hdlc_sess);
    */

    static HDLC_CTX hdlc_ctx[1];
    static HDLC *hdlc;

    HDLC_CTX_init(hdlc_ctx);
    while (1) {
        int rc = HDLC_CTX_update(hdlc_ctx, stdin);
        if (rc < 0)
            elog("HDLC_CTX_update() failed.");
        if (!rc)
            break;
        hdlc = HDLC_attach(hdlc_ctx->hdlc);
        assert(hdlc);

        HDLC_CTX_reset(hdlc_ctx);  /* HDLC_Complete -> HDLC_Initial */

        /* HDLC_dump(hdlc); */
        HDLC_detach(hdlc);

#if 0
        HDLC_SC_INDEX hdlc_sc_index = {
            hdlc->DA_upper, hdlc->DA_lower, hdlc->SA_upper, hdlc->SA_lower,
        };

        sess = HDLC_SC_search(hdlc_sc, &hdlc_sc_index);
        if (!sess)
            sess = HDLC_SESS_create(hdlc_sc_index);

        rc = HDLC_SESS_update(sess, hdlc);
        if (rc < 0)
            elog("HDLC_SESS_update() failed.");


        HDLC_SESS_get_message()
#endif
    }
    HDLC_CTX_fini(hdlc_ctx);
    return 0;

E0: 
#ifndef XXX
    if (line.flags & LINE_TERMIOS_VALID)
        if (tcsetattr(fileno(line.fp), TCSAFLUSH, &line.tios) < 0)
            elog2(errno, "tcsetattr(): %s", line.dev);
    if (line.fp)
        fclose(line.fp);
#endif

    HDLC_CTX_fini(hdlc_ctx);
    return -1;
}

static void __attribute__((noreturn)) usage(int code, const char *progname) {
    printf("%s [-h|--help] [-l|--line line_port] [-s|--speed line_speed]\n",
        progname);
    exit(code);
}

static int getnum(int *num, const char *s, int base) {
    long n;
    char *eptr;

    errno = 0;
    n = strtol(s, &eptr, base);
    if ((!n && errno == EINVAL) || ((n == LONG_MIN || n == LONG_MAX) && errno == ERANGE))
        eret2(-1, errno, "strtol(): %s", s);
    while (isspace(*eptr))
        eptr++;
    if (*eptr)
        eret(-1, "strtol(): %s: trailing garbage.", s);
    if (n < INT_MIN || n > INT_MAX)
        eret2(-1, ERANGE, "strtol(): %s", s);
    *num = n;
    return 0;
}

