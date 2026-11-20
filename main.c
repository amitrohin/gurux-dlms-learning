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
            if (snprintf(line.dev, sizeof line.dev, fmt, optarg) >= sizeof line)
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

    static HDLC_CTX hdlc_ctx;

    if (HDLC_CTX_init(&hdlc_ctx, NULL, NULL))
        egoto(E0, "HDLC_CTX_init()");
    HDLC_CTX_update(&hdlc_ctx, stdin);

    HDLC_CTX_fini(&hdlc_ctx);
    return 0;

E0: 
#ifndef XXX
    if (line.flags & LINE_TERMIOS_VALID)
        if (tcsetattr(fileno(line.fp), TCSAFLUSH, &line.tios) < 0)
            elog2(errno, "tcsetattr(): %s", line.dev);
    if (line.fp)
        fclose(line.fp);
#endif

    HDLC_CTX_fini(&hdlc_ctx);
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

