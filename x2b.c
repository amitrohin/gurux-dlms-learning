// vi: ts=4:sts=4:sw=4:et:nosi:noai:nu:syn=off

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>

int hexdigit(int c) {
    return isdigit(c) ? c - '0' : toupper(c) - 'A' + 10;
}

int main() {
    int num;
    enum {
        sInitial,
        s1,
        s0x,
        s0x1,
    } st = sInitial;
    while (1) {
        int c = getc(stdin);
        if (c < 0) {
            if (ferror(stdin))
                err(1, "getc(stdin)");
            if (st != sInitial)
                errx(1, "Last hex-byte incomplete");
            break;
        }
        if (st == sInitial) {
            if (isspace(c) || c == '_' || c == '-')
                continue;
            if (isxdigit(c)) {
                num = hexdigit(c);
                st = s1;
                continue;
            }
        } else if (st == s1) {
            if (isxdigit(c)) {
                unsigned char uc = (num << 4) + hexdigit(c);
                if (!fwrite(&uc, 1, 1, stdout))
                    err(1, "fwrite()");
                st = sInitial;
                continue;
            }
            if ((c == 'x' || c == 'X') && !num) {
                st = s0x;
                continue;
            }
        } else if (st == s0x) {
            if (isxdigit(c)) {
                num = hexdigit(c);
                st = s0x1;
                continue;
            }
        } else if (st == s0x1) {
            if (isxdigit(c)) {
                unsigned char uc = (num << 4) + hexdigit(c);
                if (!fwrite(&uc, 1, 1, stdout))
                    err(1, "fwrite()");
                st = sInitial;
                continue;
            }
        }
        errx(1, "Syntax error: illegal character 0x%02x ('%c')",
             c, isprint((unsigned char)c) ? c : '?');
    }
    if (fflush(stdout) == EOF)
        err(1, "fflush(stdout)");
    return 0;
}
